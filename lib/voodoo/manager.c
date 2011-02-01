/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/conf.h>
#include <voodoo/internal.h>
#include <voodoo/manager.h>


#define IN_BUF_MAX   (640 * 1024)
#define OUT_BUF_MAX  (640 * 1024)
#define MAX_MSG_SIZE (17 * 1024)


D_DEBUG_DOMAIN( Voodoo_Dispatch, "Voodoo/Dispatch", "Voodoo Dispatch" );
D_DEBUG_DOMAIN( Voodoo_Input,    "Voodoo/Input",    "Voodoo Input" );
D_DEBUG_DOMAIN( Voodoo_Manager,  "Voodoo/Manager",  "Voodoo Manager" );
D_DEBUG_DOMAIN( Voodoo_Output,   "Voodoo/Output",   "Voodoo Output" );

/**********************************************************************************************************************/

typedef struct {
     bool                        super;
     IAny                       *proxy;
     IAny                       *real;
     VoodooDispatch              dispatch;
} VoodooInstance;

struct __V_VoodooManager {
     int                         magic;

     VoodooLink                 *link;

     bool                        quit;

     long long                   millis;

     VoodooClient               *client;     /* Either client ... */
     VoodooServer               *server;     /* ... or server is valid. */

     size_t                      msg_count;
     VoodooMessageSerial         msg_serial;

     DirectThread               *dispatcher;
     DirectThread               *io;

     struct {
          DirectMutex            lock;
          DirectHash            *local;
          DirectHash            *remote;
          VoodooInstanceID       last;
     } instances;

     struct {
          DirectMutex            lock;
          DirectWaitQueue        wait_get;
          DirectWaitQueue        wait_put;
          VoodooResponseMessage *current;
     } response;

     struct {
          DirectMutex            lock;
          DirectWaitQueue        wait;
          u8                    *buffer;
          size_t                 start;
          size_t                 last;
          size_t                 end;
          size_t                 max;
     } input;

     struct {
          DirectMutex            lock;
          DirectWaitQueue        wait;
          u8                    *buffer;
          size_t                 start;
          size_t                 end;
     } output;
};

/**************************************************************************************************/

static void *manager_dispatch_loop( DirectThread *thread, void *arg );
static void *manager_io_loop      ( DirectThread *thread, void *arg );

/**************************************************************************************************/

static DirectResult manager_lock_output    ( VoodooManager          *manager,
                                             int                     length,
                                             void                  **ret_ptr );

static DirectResult manager_unlock_output  ( VoodooManager          *manager,
                                             bool                    flush );

/**************************************************************************************************/

static DirectResult manager_lock_response  ( VoodooManager          *manager,
                                             VoodooMessageSerial     request,
                                             VoodooResponseMessage **ret_response );

static DirectResult manager_unlock_response( VoodooManager          *manager,
                                             VoodooResponseMessage  *response );

/**************************************************************************************************/

DirectResult
voodoo_manager_create( VoodooLink     *link,
                       VoodooClient   *client,
                       VoodooServer   *server,
                       VoodooManager **ret_manager )
{
     DirectResult   ret;
     VoodooManager *manager;

     D_ASSERT( link != NULL );
     D_ASSERT( link->Read != NULL );
     D_ASSERT( link->Write != NULL );
//     D_ASSERT( (client != NULL) ^ (server != NULL) );
     D_ASSERT( ret_manager != NULL );

     /* Allocate manager structure. */
     manager = D_CALLOC( 1, sizeof(VoodooManager) );
     if (!manager) {
          D_WARN( "out of memory" );
          return DR_NOLOCALMEMORY;
     }

     D_DEBUG( "Voodoo/Manager: Creating manager at %p.\n", manager );

     /* Create the hash table for dispatcher instances. */
     ret = direct_hash_create( 251, &manager->instances.local );
     if (ret) {
          D_FREE( manager );
          return ret;
     }

     /* Create the hash table for requestor instances. */
     ret = direct_hash_create( 251, &manager->instances.remote );
     if (ret) {
          direct_hash_destroy( manager->instances.local );
          D_FREE( manager );
          return ret;
     }

     /* Store link. */
     manager->link = link;

     /* Store client or server. */
     manager->client = client;
     manager->server = server;

     /* Initialize all locks. */
     direct_recursive_mutex_init( &manager->instances.lock );
     direct_recursive_mutex_init( &manager->response.lock );
     direct_recursive_mutex_init( &manager->input.lock );
     direct_recursive_mutex_init( &manager->output.lock );

     /* Initialize all wait conditions. */
     direct_waitqueue_init( &manager->response.wait_get );
     direct_waitqueue_init( &manager->response.wait_put );
     direct_waitqueue_init( &manager->input.wait );
     direct_waitqueue_init( &manager->output.wait );

     /* Set default buffer limit. */
     manager->input.max = IN_BUF_MAX;

     manager->input.buffer  = D_MALLOC( IN_BUF_MAX + MAX_MSG_SIZE );
     manager->output.buffer = D_MALLOC( OUT_BUF_MAX );

     D_MAGIC_SET( manager, VoodooManager );

     /* Create all threads. */
     manager->dispatcher    = direct_thread_create( DTT_MESSAGING, manager_dispatch_loop,
                                                    manager, "Voodoo Dispatch" );

     manager->io            = direct_thread_create( DTT_OUTPUT, manager_io_loop,
                                                    manager, "Voodoo IO" );

     /* Return the new manager. */
     *ret_manager = manager;

     return DR_OK;
}

DirectResult
voodoo_manager_quit( VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSUME( !manager->quit );

     if (manager->quit)
          return DR_OK;

     D_DEBUG( "Voodoo/Manager: Quitting manager at %p!\n", manager );

     manager->link->WakeUp( manager->link );

     /* Have all threads quit upon this. */
     manager->quit = true;

     /* Acquire locks and wake up waiters. */
     direct_mutex_lock( &manager->input.lock );
     direct_waitqueue_broadcast( &manager->input.wait );
     direct_mutex_unlock( &manager->input.lock );

     direct_mutex_lock( &manager->response.lock );
     direct_waitqueue_broadcast( &manager->response.wait_get );
     direct_waitqueue_broadcast( &manager->response.wait_put );
     direct_mutex_unlock( &manager->response.lock );

     direct_mutex_lock( &manager->output.lock );
     direct_waitqueue_broadcast( &manager->output.wait );
     direct_mutex_unlock( &manager->output.lock );

     return DR_OK;
}

static bool
instance_iterator( DirectHash    *hash,
                   unsigned long  key,
                   void          *value,
                   void          *ctx )
{
     bool            super    = (unsigned long) ctx;
     VoodooInstance *instance = value;

     D_ASSERT( instance != NULL );

     if (instance->super != super) {
          if (super)
               D_FREE( instance );

          return true;
     }


     D_DEBUG( "Voodoo/Manager: Releasing dispatcher interface %p %s(instance %lu)...\n",
              instance->proxy, instance->super ? "[super] " : "", key );

     D_ASSERT( instance->proxy != NULL );
     D_ASSERT( instance->proxy->Release != NULL );

     instance->proxy->Release( instance->proxy );


     D_DEBUG( "Voodoo/Manager: Releasing real interface %p %s(instance %lu)...\n",
              instance->real, instance->super ? "[super] " : "", key );

     D_ASSERT( instance->real != NULL );
     D_ASSERT( instance->real->Release != NULL );

     instance->real->Release( instance->real );

     if (super)
          D_FREE( instance );

     return true;
}

DirectResult
voodoo_manager_destroy( VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     D_DEBUG( "Voodoo/Manager: Destroying manager at %p!\n", manager );

     if (!manager->quit)
          voodoo_manager_quit( manager );

     /* Wait for manager threads exiting. */
     direct_thread_join( manager->dispatcher );
     direct_thread_destroy( manager->dispatcher );

     direct_thread_join( manager->io );
     direct_thread_destroy( manager->io );

     /* Destroy conditions. */
     direct_waitqueue_deinit( &manager->input.wait );
     direct_waitqueue_deinit( &manager->response.wait_get );
     direct_waitqueue_deinit( &manager->response.wait_put );
     direct_waitqueue_deinit( &manager->output.wait );

     /* Destroy locks. */
     direct_mutex_deinit( &manager->instances.lock );
     direct_mutex_deinit( &manager->input.lock );
     direct_mutex_deinit( &manager->response.lock );
     direct_mutex_deinit( &manager->output.lock );

     /* Release all remaining interfaces. */
     direct_hash_iterate( manager->instances.local, instance_iterator, (void*) false );
     direct_hash_iterate( manager->instances.local, instance_iterator, (void*) true );
     direct_hash_destroy( manager->instances.local );

     direct_hash_destroy( manager->instances.remote );

     D_MAGIC_CLEAR( manager );

     /* Deallocate manager structure. */
     D_FREE( manager->output.buffer );
     D_FREE( manager->input.buffer );
     D_FREE( manager );

     return DR_OK;
}

bool
voodoo_manager_is_closed( const VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->quit;
}

/**************************************************************************************************/

DirectResult
voodoo_manager_super( VoodooManager    *manager,
                      const char       *name,
                      VoodooInstanceID *ret_instance )
{
     DirectResult           ret;
     int                    len;
     int                    size;
     void                  *ptr;
     VoodooMessageSerial    serial;
     VoodooSuperMessage    *msg;
     VoodooResponseMessage *response;

     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( name != NULL );
     D_ASSERT( ret_instance != NULL );

     /* Calculate the total message size. */
     len  = strlen( name ) + 1;
     size = sizeof(VoodooSuperMessage) + len;


     /* Lock the output buffer for direct writing. */
     ret = manager_lock_output( manager, size, &ptr );
     if (ret)
          return ret;

     msg = ptr;

     serial = manager->msg_serial++;

     /* Fill message header. */
     msg->header.size   = size;
     msg->header.serial = serial;
     msg->header.type   = VMSG_SUPER;

     /* Append the name of the super interface to create. */
     direct_memcpy( msg + 1, name, len );

     D_DEBUG( "Voodoo/Manager: Sending SUPER message %llu for '%s' (%d bytes).\n", (unsigned long long)serial, name, size );

     /* Unlock the output buffer. */
     manager_unlock_output( manager, true );


     /* Wait for and lock the response buffer. */
     ret = manager_lock_response( manager, serial, &response );
     if (ret) {
          D_ERROR( "Voodoo/Manager: "
                   "Waiting for the response failed (%s)!\n", DirectResultString( ret ) );
          return ret;
     }

     D_DEBUG( "Voodoo/Manager: Got response %llu (%s) with instance %u for request %llu "
              "(%d bytes).\n", (unsigned long long)response->header.serial, DirectResultString( ret ),
              response->instance, (unsigned long long)response->request, response->header.size );

     ret = response->result;
     if (ret) {
          D_ERROR( "Voodoo/Manager: Could not create remote super interface '%s' (%s)!\n",
                   name, DirectResultString( ret ) );
          manager_unlock_response( manager, response );
          return ret;
     }

     D_INFO( "Voodoo/Manager: Created remote super interface '%s'.\n", name );

     /* Return the new instance ID. */
     *ret_instance = response->instance;

     /* Unlock the response buffer. */
     manager_unlock_response( manager, response );

     return DR_OK;
}

static __inline__ int
calc_blocks( VoodooMessageBlockType type, va_list args )
{
     int size = 4;  /* for the terminating VMBT_NONE */

     while (type != VMBT_NONE) {
          u32 arg;
          s32 in;
          void *ptr;
          int   len = 0;

          switch (type) {
               case VMBT_ID:
                    len = 4;
                    arg = va_arg( args, u32 );

                    D_DEBUG( "Voodoo/Message: + ID %u\n", arg );
                    break;

               case VMBT_INT:
                    len = 4;
                    in  = va_arg( args, s32 );

                    D_DEBUG( "Voodoo/Message: + INT %d\n", in );
                    break;

               case VMBT_UINT:
                    len = 4;
                    arg = va_arg( args, u32 );

                    D_DEBUG( "Voodoo/Message: + UINT %u\n", arg );
                    break;

               case VMBT_DATA:
                    len = va_arg( args, int );
                    ptr = va_arg( args, void * );

                    D_ASSERT( len > 0 );
                    D_ASSERT( ptr != NULL );

                    D_DEBUG( "Voodoo/Message: + DATA at %p with length %d\n", ptr, len );
                    break;

               case VMBT_ODATA:
                    len = va_arg( args, int );
                    ptr = va_arg( args, void * );

                    D_ASSERT( len > 0 );

                    D_DEBUG( "Voodoo/Message: + ODATA at %p with length %d\n", ptr, len );

                    if (!ptr)
                         len = 0;
                    break;

               case VMBT_STRING:
                    ptr = va_arg( args, char * );
                    len = strlen( ptr ) + 1;

                    D_ASSERT( ptr != NULL );

                    D_DEBUG( "Voodoo/Message: + STRING '%s' at %p with length %d\n", (char*) ptr, ptr, len );
                    break;

               default:
                    D_BREAK( "unknown message block type" );
          }

          size += 8 + VOODOO_MSG_ALIGN(len);

          type = va_arg( args, VoodooMessageBlockType );
     }

     return size;
}

static __inline__ void
write_blocks( void *dst, VoodooMessageBlockType type, va_list args )
{
     u32 *d32 = dst;

     while (type != VMBT_NONE) {
          u32   arg = 0;
          u32   len = 0;
          void *ptr = NULL;

          switch (type) {
               case VMBT_ID:
               case VMBT_INT:
               case VMBT_UINT:
                    len = 4;
                    arg = va_arg( args, u32 );
                    break;
               case VMBT_DATA:
                    len = va_arg( args, int );
                    ptr = va_arg( args, void * );
                    break;
               case VMBT_ODATA:
                    len = va_arg( args, int );
                    ptr = va_arg( args, void * );
                    if (!ptr)
                         len = 0;
                    break;
               case VMBT_STRING:
                    ptr = va_arg( args, char * );
                    len = strlen( ptr ) + 1;
                    break;
               default:
                    D_BREAK( "unknown message block type" );
          }

          /* Write block type and length. */
          d32[0] = type;
          d32[1] = len;

          /* Write block content. */
          if (ptr)
               direct_memcpy( &d32[2], ptr, len );
          else if (len) {
               D_ASSERT( len == 4 );

               d32[2] = arg;
          }

          /* Advance message data pointer. */
          d32 += 2 + (VOODOO_MSG_ALIGN(len) >> 2);

          /* Fetch next message block type. */
          type = va_arg( args, VoodooMessageBlockType );
     }

     /* Write terminator. */
     d32[0] = VMBT_NONE;
}

DirectResult
voodoo_manager_request( VoodooManager           *manager,
                        VoodooInstanceID         instance,
                        VoodooMethodID           method,
                        VoodooRequestFlags       flags,
                        VoodooResponseMessage  **ret_response,
                        VoodooMessageBlockType   block_type, ... )
{
     DirectResult          ret;
     int                   size;
     void                 *ptr;
     VoodooMessageSerial   serial;
     VoodooRequestMessage *msg;
     va_list               args;

     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( instance != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_response != NULL || !(flags & VREQ_RESPOND) );
     D_ASSUME( (flags & (VREQ_RESPOND | VREQ_QUEUE)) != (VREQ_RESPOND | VREQ_QUEUE) );

     D_DEBUG( "Voodoo/Request: "
              "Instance %u, method %u, flags 0x%08x...\n", instance, method, flags );

     /* Calculate the total message size. */
     va_start( args, block_type );
     size = sizeof(VoodooRequestMessage) + calc_blocks( block_type, args );
     va_end( args );

     D_DEBUG( "Voodoo/Request:  --> complete message size: %d\n", size );

     /* Lock the output buffer for direct writing. */
     ret = manager_lock_output( manager, size, &ptr );
     if (ret)
          return ret;

     msg = ptr;

     serial = manager->msg_serial++;

     /* Fill message header. */
     msg->header.size   = size;
     msg->header.serial = serial;
     msg->header.type   = VMSG_REQUEST;

     /* Fill message body. */
     msg->instance = instance;
     msg->method   = method;
     msg->flags    = flags;

     /* Append custom data. */
     va_start( args, block_type );
     write_blocks( msg + 1, block_type, args );
     va_end( args );


     D_DEBUG( "Voodoo/Manager: Sending REQUEST message %llu to %u::%u %s(%d bytes).\n",
              (unsigned long long)serial, instance, method, (flags & VREQ_RESPOND) ? "[RESPONDING] " : "", size );

     /* Unlock the output buffer. */
     manager_unlock_output( manager, !(flags & VREQ_QUEUE) );

     /* Wait for and lock the response buffer. */
     if (flags & VREQ_RESPOND) {
          VoodooResponseMessage *response;

          ret = manager_lock_response( manager, serial, &response );
          if (ret) {
               D_ERROR( "Voodoo/Manager: "
                        "Waiting for the response failed (%s)!\n", DirectResultString( ret ) );
               return ret;
          }

          D_DEBUG( "Voodoo/Manager: Got response %llu (%s) with instance %u for request %llu "
                   "(%d bytes).\n", (unsigned long long)response->header.serial, DirectResultString( response->result ),
                   response->instance, (unsigned long long)response->request, response->header.size );

          *ret_response = response;
     }

     return DR_OK;
}

DirectResult
voodoo_manager_next_response( VoodooManager          *manager,
                              VoodooResponseMessage  *response,
                              VoodooResponseMessage **ret_response )
{
     DirectResult        ret;
     VoodooMessageSerial serial;

     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( response != NULL );

     serial = response->request;

     /* Unlock the response buffer. */
     manager_unlock_response( manager, response );

     ret = manager_lock_response( manager, serial, &response );
     if (ret) {
          D_ERROR( "Voodoo/Manager: "
                   "Waiting for the response failed (%s)!\n", DirectResultString( ret ) );
          return ret;
     }

     D_DEBUG( "Voodoo/Manager: Got response %llu (%s) with instance %u for request %llu "
              "(%d bytes).\n", (unsigned long long)response->header.serial, DirectResultString( response->result ),
              response->instance, (unsigned long long)response->request, response->header.size );

     *ret_response = response;

     return DR_OK;
}

DirectResult
voodoo_manager_finish_request( VoodooManager         *manager,
                               VoodooResponseMessage *response )
{
     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( response != NULL );

     /* Unlock the response buffer. */
     return manager_unlock_response( manager, response );
}

DirectResult
voodoo_manager_respond( VoodooManager          *manager,
                        bool                    flush,
                        VoodooMessageSerial     request,
                        DirectResult            result,
                        VoodooInstanceID        instance,
                        VoodooMessageBlockType  block_type, ... )
{
     DirectResult           ret;
     int                    size;
     void                  *ptr;
     VoodooMessageSerial    serial;
     VoodooResponseMessage *msg;
     va_list                args;

     D_MAGIC_ASSERT( manager, VoodooManager );

     D_DEBUG( "Voodoo/Response: "
              "Request %llu, result %d, instance %u...\n", (unsigned long long)request, result, instance );

     /* Calculate the total message size. */
     va_start( args, block_type );
     size = sizeof(VoodooResponseMessage) + calc_blocks( block_type, args );
     va_end( args );

     D_DEBUG( "Voodoo/Response:  --> complete message size: %d\n", size );


     /* Lock the output buffer for direct writing. */
     ret = manager_lock_output( manager, size, &ptr );
     if (ret)
          return ret;

     msg = ptr;

     serial = manager->msg_serial++;

     /* Fill message header. */
     msg->header.size   = size;
     msg->header.serial = serial;
     msg->header.type   = VMSG_RESPONSE;

     /* Fill message body. */
     msg->request  = request;
     msg->result   = result;
     msg->instance = instance;

     /* Append custom data. */
     va_start( args, block_type );
     write_blocks( msg + 1, block_type, args );
     va_end( args );


     D_DEBUG( "Voodoo/Manager: "
              "Sending RESPONSE message %llu (%s) with instance %u for request %llu (%d bytes).\n",
              (unsigned long long)serial, DirectResultString( result ), instance, (unsigned long long)request, size );

     /* Unlock the output buffer. */
     manager_unlock_output( manager, flush );

     return DR_OK;
}

DirectResult
voodoo_manager_register_local( VoodooManager    *manager,
                               bool              super,
                               void             *dispatcher,
                               void             *real,
                               VoodooDispatch    dispatch,
                               VoodooInstanceID *ret_instance )
{
     DirectResult      ret;
     VoodooInstance   *instance;
     VoodooInstanceID  instance_id;

     D_MAGIC_ASSERT( manager, VoodooManager );
//     D_ASSERT( dispatcher != NULL );
//     D_ASSERT( real != NULL );
     D_ASSERT( dispatch != NULL );
     D_ASSERT( ret_instance != NULL );

     instance = D_CALLOC( 1, sizeof(VoodooInstance) );
     if (!instance) {
          D_WARN( "out of memory" );
          return DR_NOLOCALMEMORY;
     }

     instance->super    = super;
     instance->proxy    = dispatcher;
     instance->real     = real;
     instance->dispatch = dispatch;

     direct_mutex_lock( &manager->instances.lock );

     instance_id = ++manager->instances.last;

     ret = direct_hash_insert( manager->instances.local, instance_id, instance );

     direct_mutex_unlock( &manager->instances.lock );

     if (ret) {
          D_ERROR( "Voodoo/Manager: Adding a new instance to the dispatcher hash table failed!\n" );
          D_FREE( instance );
          return ret;
     }

     D_DEBUG( "Voodoo/Manager: "
              "Added instance %u, dispatcher %p, real %p.\n", instance_id, dispatcher, real );

     *ret_instance = instance_id;

     return DR_OK;
}

DirectResult
voodoo_manager_unregister_local( VoodooManager    *manager,
                                 VoodooInstanceID  instance_id )
{
     VoodooInstance *instance;

     D_MAGIC_ASSERT( manager, VoodooManager );

     direct_mutex_lock( &manager->instances.lock );

     instance = direct_hash_lookup( manager->instances.local, instance_id );
     if (!instance) {
          direct_mutex_unlock( &manager->instances.lock );
          return DR_NOSUCHINSTANCE;
     }

     direct_hash_remove( manager->instances.local, instance_id );

     direct_mutex_unlock( &manager->instances.lock );

     D_FREE( instance );

     return DR_OK;
}

DirectResult
voodoo_manager_lookup_local( VoodooManager     *manager,
                             VoodooInstanceID   instance_id,
                             void             **ret_dispatcher,
                             void             **ret_real )
{
     VoodooInstance *instance;

     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( instance_id != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_dispatcher != NULL || ret_real != NULL );

     direct_mutex_lock( &manager->instances.lock );

     instance = direct_hash_lookup( manager->instances.local, instance_id );

     direct_mutex_unlock( &manager->instances.lock );

     if (!instance)
          return DR_NOSUCHINSTANCE;

     if (ret_dispatcher)
          *ret_dispatcher = instance->proxy;

     if (ret_real)
          *ret_real = instance->real;

     return DR_OK;
}

DirectResult
voodoo_manager_register_remote( VoodooManager    *manager,
                                bool              super,
                                void             *requestor,
                                VoodooInstanceID  instance_id )
{
     DirectResult    ret;
     VoodooInstance *instance;

     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( requestor != NULL );
     D_ASSERT( instance_id != VOODOO_INSTANCE_NONE );

     instance = D_CALLOC( 1, sizeof(VoodooInstance) );
     if (!instance) {
          D_WARN( "out of memory" );
          return DR_NOLOCALMEMORY;
     }

     instance->super = super;
     instance->proxy = requestor;

     direct_mutex_lock( &manager->instances.lock );

     ret = direct_hash_insert( manager->instances.remote, instance_id, instance );

     direct_mutex_unlock( &manager->instances.lock );

     if (ret) {
          D_ERROR( "Voodoo/Manager: Adding a new instance to the requestor hash table failed!\n" );
          D_FREE( instance );
          return ret;
     }

     D_DEBUG( "Voodoo/Manager: "
              "Added remote instance %u, requestor %p.\n", instance_id, requestor );

     return DR_OK;
}


DirectResult
voodoo_manager_lookup_remote( VoodooManager     *manager,
                              VoodooInstanceID   instance_id,
                              void             **ret_requestor )
{
     VoodooInstance *instance;

     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( instance_id != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_requestor != NULL );

     direct_mutex_lock( &manager->instances.lock );

     instance = direct_hash_lookup( manager->instances.remote, instance_id );

     direct_mutex_unlock( &manager->instances.lock );

     if (!instance)
          return DR_NOSUCHINSTANCE;

     *ret_requestor = instance->proxy;

     return DR_OK;
}

DirectResult
voodoo_manager_check_allocation( VoodooManager *manager,
                                 unsigned int   amount )
{
#ifndef WIN32
     FILE *f;
     char  buf[2000];
     int   size;
     char *p;

     D_MAGIC_ASSERT( manager, VoodooManager );

     if (!voodoo_config->memory_max)
          return DR_OK;

     direct_snprintf( buf, sizeof(buf), "/proc/%d/status", direct_getpid() );

     f = fopen( buf, "r" );
     if (!f) {
          D_ERROR( "Could not open '%s'!\n", buf );
          return DR_FAILURE;
     }

     fread( buf, 1, sizeof(buf), f );

     fclose( f );


     p = strstr( buf, "VmRSS:" );
     if (!p) {
          D_ERROR( "Could not find memory information!\n" );
          return DR_FAILURE;
     }

     sscanf( p + 6, " %u", &size );

     D_INFO( "SIZE: %u kB (+%u kB)\n", size, amount / 1024 );

     if (size * 1024 + amount > voodoo_config->memory_max)
          return DR_LIMITEXCEEDED;
#endif
     return DR_OK;
}

/**************************************************************************************************/

static void
handle_disconnect( VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     if (0) {
          long long num;
          long long millis = direct_clock_get_millis();
          long long diff   = millis - manager->millis;

          num = manager->msg_count * 1000LL / diff;

          D_INFO( "Voodoo/Manager: Average number of messages: %lld.%03lld k/sec\n", num / 1000, num % 1000 );
     }

     D_DEBUG( "Voodoo/Manager: Remote site disconnected from manager at %p!\n", manager );

     voodoo_manager_quit( manager );
}

static void
handle_super( VoodooManager      *manager,
              VoodooSuperMessage *super )
{
     DirectResult  ret;
     const char   *name;

     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( super != NULL );
     D_ASSERT( super->header.size >= sizeof(VoodooSuperMessage) );
     D_ASSERT( super->header.type == VMSG_SUPER );

     name = (const char *) (super + 1);

     D_DEBUG( "Voodoo/Dispatch: Handling SUPER message %llu for '%s' (%d bytes).\n",
              (unsigned long long)super->header.serial, name, super->header.size );

     if (manager->server) {
          VoodooInstanceID instance;

          ret = voodoo_server_construct( manager->server, manager, name, &instance );
          if (ret) {
               voodoo_manager_respond( manager, true, super->header.serial,
                                       ret, VOODOO_INSTANCE_NONE,
                                       VMBT_NONE );
               return;
          }

          voodoo_manager_respond( manager, true, super->header.serial,
                                  DR_OK, instance,
                                  VMBT_NONE );
     }
     else {
          D_WARN( "can't handle this as a client" );
          voodoo_manager_respond( manager, true, super->header.serial,
                                  DR_UNSUPPORTED, VOODOO_INSTANCE_NONE,
                                  VMBT_NONE );
     }
}

typedef struct {
     VoodooManager        *manager;
     VoodooInstance       *instance;
     VoodooRequestMessage *request;
} DispatchAsyncContext;

static void *
dispatch_async_thread( DirectThread *thread,
                       void         *arg )
{
     DirectResult          ret;
     DispatchAsyncContext *context  = arg;
     VoodooManager        *manager  = context->manager;
     VoodooInstance       *instance = context->instance;
     VoodooRequestMessage *request  = context->request;

     ret = instance->dispatch( instance->proxy, instance->real, manager, request );

     if (ret && (request->flags & VREQ_RESPOND))
          voodoo_manager_respond( manager, true, request->header.serial,
                                  ret, VOODOO_INSTANCE_NONE,
                                  VMBT_NONE );

     D_FREE( context );

     return NULL;
}

static void
handle_request( VoodooManager        *manager,
                VoodooRequestMessage *request )
{
     DirectResult    ret;
     VoodooInstance *instance;

     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( request != NULL );
     D_ASSERT( request->header.size >= sizeof(VoodooRequestMessage) );
     D_ASSERT( request->header.type == VMSG_REQUEST );

     D_DEBUG( "Voodoo/Dispatch: Handling REQUEST message %llu to %u::%u %s%s(%d bytes).\n",
              (unsigned long long)request->header.serial, request->instance, request->method,
              (request->flags & VREQ_RESPOND) ? "[RESPONDING] " : "",
              (request->flags & VREQ_ASYNC) ? "[ASYNC] " : "",
              request->header.size );

     direct_mutex_lock( &manager->instances.lock );

     instance = direct_hash_lookup( manager->instances.local, request->instance );
     if (!instance) {
          direct_mutex_unlock( &manager->instances.lock );

          D_ERROR( "Voodoo/Dispatch: "
                   "Requested instance %u doesn't exist (anymore)!\n", request->instance );

          if (request->flags & VREQ_RESPOND)
               voodoo_manager_respond( manager, true, request->header.serial,
                                       DR_NOSUCHINSTANCE, VOODOO_INSTANCE_NONE,
                                       VMBT_NONE );

          return;
     }

     if (request->flags & VREQ_ASYNC) {
          DirectThread         *thread;
          DispatchAsyncContext *context;

          context = D_MALLOC( sizeof(DispatchAsyncContext) + request->header.size );
          if (!context) {
               D_WARN( "out of memory" );
               direct_mutex_unlock( &manager->instances.lock );
               return;
          }

          context->manager  = manager;
          context->instance = instance;
          context->request  = (VoodooRequestMessage*) (context + 1);

          direct_memcpy( context->request, request, request->header.size );

          thread = direct_thread_create( DTT_DEFAULT, dispatch_async_thread, context, "Voodoo Async" );
          direct_thread_detach( thread );
          // FIXME: free thread?
     }
     else {
          ret = instance->dispatch( instance->proxy, instance->real, manager, request );

          if (ret && (request->flags & VREQ_RESPOND))
               voodoo_manager_respond( manager, true, request->header.serial,
                                       ret, VOODOO_INSTANCE_NONE,
                                       VMBT_NONE );
     }

     direct_mutex_unlock( &manager->instances.lock );
}

static void
handle_response( VoodooManager         *manager,
                 VoodooResponseMessage *response )
{
     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( response != NULL );
     D_ASSERT( response->header.size >= sizeof(VoodooResponseMessage) );
     D_ASSERT( response->header.type == VMSG_RESPONSE );
     D_ASSERT( response->request < manager->msg_serial );

     D_DEBUG_AT( Voodoo_Dispatch, "Handling RESPONSE message %llu (%s) with instance %u for request "
                 "%llu (%d bytes).\n", (unsigned long long)response->header.serial, DirectResultString( response->result ),
                 response->instance, (unsigned long long)response->request, response->header.size );

     direct_mutex_lock( &manager->response.lock );

     D_ASSERT( manager->response.current == NULL );

     manager->response.current = response;

     direct_mutex_unlock( &manager->response.lock );


     direct_waitqueue_broadcast( &manager->response.wait_get );

     direct_mutex_lock( &manager->response.lock );

     while (manager->response.current && !manager->quit)
          direct_waitqueue_wait( &manager->response.wait_put, &manager->response.lock );

     direct_mutex_unlock( &manager->response.lock );
}

/**************************************************************************************************/

static void *
manager_dispatch_loop( DirectThread *thread, void *arg )
{
     VoodooManager *manager = arg;

     /* Lock the input buffer. */
     direct_mutex_lock( &manager->input.lock );

     while (!manager->quit) {
          VoodooMessageHeader *header;
          size_t               aligned;
          int                  start;

          D_MAGIC_ASSERT( manager, VoodooManager );

          D_DEBUG_AT( Voodoo_Dispatch, "START "_ZD", LAST "_ZD", END "_ZD", MAX "_ZD"\n",
                      manager->input.start, manager->input.last, manager->input.end, manager->input.max );

          /* Wait for at least four bytes which contain the length of the message. */
          while (manager->input.last - manager->input.start < 4) {
               D_DEBUG_AT( Voodoo_Dispatch, "Waiting for messages...\n" );

               direct_waitqueue_wait( &manager->input.wait, &manager->input.lock );

               if (manager->quit)
                    break;
          }

          if (manager->quit)
               break;

          start = manager->input.start;

          /* Unlock the input buffer. */
          direct_mutex_unlock( &manager->input.lock );


          while (start != manager->input.last) {
               /* Get the message header. */
               header  = (VoodooMessageHeader *)(manager->input.buffer + start);
               aligned = VOODOO_MSG_ALIGN( header->size );

               D_DEBUG_AT( Voodoo_Dispatch, "Next message has %d ("_ZU") bytes and is of type %d... (start "_ZU", last "_ZU")\n",
                           header->size, aligned, header->type, start, manager->input.last );

               D_ASSERT( header->size >= sizeof(VoodooMessageHeader) );
               D_ASSERT( header->size <= MAX_MSG_SIZE );

               D_ASSERT( aligned <= manager->input.end - start );

               switch (header->type) {
                    case VMSG_SUPER:
                         handle_super( manager, (VoodooSuperMessage*) header );
                         break;

                    case VMSG_REQUEST:
                         handle_request( manager, (VoodooRequestMessage*) header );
                         break;

                    case VMSG_RESPONSE:
                         handle_response( manager, (VoodooResponseMessage*) header );
                         break;

                    default:
                         D_BUG( "invalid message type %d", header->type );
                         break;
               }

               start += aligned;
          }


          /* Lock the input buffer. */
          direct_mutex_lock( &manager->input.lock );

          manager->input.start = start;

          if (start == manager->input.max) {
               //direct_waitqueue_broadcast( &manager->input.wait );
               manager->link->WakeUp( manager->link );
          }
     }

     /* Unlock the input buffer. */
     direct_mutex_unlock( &manager->input.lock );

     return NULL;
}

static void *
manager_io_loop( DirectThread *thread, void *arg )
{
     VoodooManager *manager = arg;

     manager->millis = direct_clock_get_millis();

     while (!manager->quit) {
          D_MAGIC_ASSERT( manager, VoodooManager );

          direct_mutex_lock( &manager->input.lock );

          if (manager->input.start == manager->input.max) {
               manager->input.start = 0;
               manager->input.end   = 0;
               manager->input.last  = 0;
               manager->input.max   = IN_BUF_MAX;
          }

          direct_mutex_unlock( &manager->input.lock );

          if (!manager->quit) {
               DirectResult  ret;
               VoodooChunk   chunks[2];
               VoodooChunk  *chunk_read  = NULL;
               VoodooChunk  *chunk_write = NULL;
               size_t        last = manager->input.last;

               direct_mutex_lock( &manager->output.lock );

               if (manager->input.end < manager->input.max) {
                    chunk_read = &chunks[0];

                    chunk_read->ptr    = manager->input.buffer + manager->input.end;
                    chunk_read->length = manager->input.max - manager->input.end;
                    chunk_read->done   = 0;
               }

               if (manager->output.end > manager->output.start) {
                    chunk_write = &chunks[1];

                    chunk_write->ptr    = manager->output.buffer + manager->output.start;
                    chunk_write->length = manager->output.end - manager->output.start;
                    chunk_write->done   = 0;

                    if (chunk_write->length > 65536) {
                         chunk_write->length = 65536;
                    }
               }

               if (!chunk_write)
                    direct_mutex_unlock( &manager->output.lock );


               ret = manager->link->SendReceive( manager->link,
                                                 chunk_write, chunk_write ? 1 : 0,
                                                 chunk_read,  chunk_read  ? 1 : 0 );
               switch (ret) {
                    case DR_OK:
                         if (chunk_write && chunk_write->done) {
                              D_DEBUG_AT( Voodoo_Output, "Sent "_ZD"/"_ZD" bytes...\n", chunk_write->done, chunk_write->length );

                              manager->output.start += (size_t) chunk_write->done;

                              //direct_mutex_lock( &manager->output.lock );

                              if (manager->output.start == manager->output.end) {
                                   manager->output.start = manager->output.end = 0;

                                   direct_waitqueue_broadcast( &manager->output.wait );
                              }

                              //direct_mutex_unlock( &manager->output.lock );
                         }

                         if (chunk_read && chunk_read->done) {
                              D_DEBUG_AT( Voodoo_Input, "Received "_ZD" bytes...\n", chunk_read->done );
     
                              manager->input.end += (size_t) chunk_read->done;
     
                              do {
                                   VoodooMessageHeader *header;
                                   size_t               aligned;
     
                                   /* Get the message header. */
                                   header  = (VoodooMessageHeader *)(manager->input.buffer + last);
                                   aligned = VOODOO_MSG_ALIGN( header->size );
     
                                   D_DEBUG_AT( Voodoo_Input, "Next message has %d ("_ZD") bytes and is of type %d...\n",
                                               header->size, aligned, header->type );
     
                                   D_ASSERT( header->size >= sizeof(VoodooMessageHeader) );
                                   D_ASSERT( header->size <= MAX_MSG_SIZE );
     
                                   if (aligned > manager->input.end - last) {
                                        D_DEBUG_AT( Voodoo_Input, "...fetching tail of message.\n" );
     
                                        /* Extend the buffer if the message doesn't fit into the default boundary. */
                                        if (aligned > manager->input.max - last) {
                                             D_ASSERT( manager->input.max == IN_BUF_MAX );
     
     
                                             manager->input.max = last + aligned;
                                        }
     
                                        break;
                                   }
     
                                   last += aligned;
                              } while (last < manager->input.end);
     
                              if (last != manager->input.last) {
                                   direct_mutex_lock( &manager->input.lock );
     
                                   manager->input.last = last;
     
                                   direct_waitqueue_broadcast( &manager->input.wait );
     
                                   direct_mutex_unlock( &manager->input.lock );
                              }
                         }
                         break;

                    case DR_TIMEOUT:
                         //D_WARN("timeout");
                         break;

                    case DR_INTERRUPTED:
                         //D_WARN("interrupted");
                         break;

                    default:
                         D_DERROR( ret, "Voodoo/Manager: Could not receive data!\n" );
                         handle_disconnect( manager );
                         break;
               }

               if (chunk_write)
                    direct_mutex_unlock( &manager->output.lock );
          }
     }

     return NULL;
}

/**************************************************************************************************/

static DirectResult
manager_lock_output( VoodooManager  *manager,
                     int             length,
                     void          **ret_ptr )
{
     int aligned;

     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( length >= sizeof(VoodooMessageHeader) );
     D_ASSUME( length <= MAX_MSG_SIZE );
     D_ASSERT( ret_ptr != NULL );

     if (length > MAX_MSG_SIZE) {
          D_WARN( "%d exceeds maximum message size of %d", length, MAX_MSG_SIZE );
          return DR_LIMITEXCEEDED;
     }

     aligned = VOODOO_MSG_ALIGN( length );

     direct_mutex_lock( &manager->output.lock );

     while (manager->output.end + aligned > OUT_BUF_MAX) {
          //direct_waitqueue_broadcast( &manager->output.wait );
          manager->link->WakeUp( manager->link );

          direct_waitqueue_wait( &manager->output.wait, &manager->output.lock );

          if (manager->quit) {
               direct_mutex_lock( &manager->output.lock );
               return DR_DESTROYED;
          }
     }

     *ret_ptr = manager->output.buffer + manager->output.end;

     D_DEBUG_AT( Voodoo_Output, "Output locked at "_ZD" (length %d)...\n", manager->output.end, aligned );

     manager->output.end += aligned;

     return DR_OK;
}

static DirectResult
manager_unlock_output( VoodooManager *manager,
                       bool           flush )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     direct_mutex_unlock( &manager->output.lock );

     if (flush) {
          //direct_waitqueue_broadcast( &manager->output.wait );
          manager->link->WakeUp( manager->link );
     }

     return DR_OK;
}

/**************************************************************************************************/

static DirectResult
manager_lock_response( VoodooManager          *manager,
                       VoodooMessageSerial     request,
                       VoodooResponseMessage **ret_response )
{
     VoodooResponseMessage *response = NULL;

     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( ret_response != NULL );

     D_DEBUG_AT( Voodoo_Manager, "Locking response to request %llu...\n", (unsigned long long)request );

     direct_mutex_lock( &manager->response.lock );

     while (!manager->quit) {
          response = manager->response.current;
          if (response && response->request == request)
               break;

          if (response)
               D_DEBUG_AT( Voodoo_Manager, "...current response is for request %llu...\n", (unsigned long long)response->request );

          D_DEBUG_AT( Voodoo_Manager, "...(still) waiting for response to request %llu...\n", (unsigned long long)request );

          direct_waitqueue_wait( &manager->response.wait_get, &manager->response.lock );
     }

     if (manager->quit) {
          D_ERROR( "Voodoo/Manager: Quit while waiting for response!\n" );
          direct_mutex_unlock( &manager->response.lock );
          return DR_DESTROYED;
     }

     D_DEBUG_AT( Voodoo_Manager, "...locked response %llu to request %llu (%d bytes).\n",
                 (unsigned long long)response->header.serial, (unsigned long long)request, response->header.size );

     *ret_response = response;

     return DR_OK;
}

static DirectResult
manager_unlock_response( VoodooManager         *manager,
                         VoodooResponseMessage *response )
{
     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( response != NULL );
     D_ASSERT( response == manager->response.current );

     D_DEBUG_AT( Voodoo_Manager, "Unlocking response %llu to request %llu (%d bytes)...\n",
                 (unsigned long long)response->header.serial, (unsigned long long)response->request, response->header.size );

     manager->response.current = NULL;

     direct_mutex_unlock( &manager->response.lock );

     direct_waitqueue_broadcast( &manager->response.wait_put );

     return DR_OK;
}

