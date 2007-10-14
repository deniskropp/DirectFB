/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

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

#include <voodoo/internal.h>
#include <voodoo/manager.h>


#define IN_BUF_MAX   (32 * 1024)
#define OUT_BUF_MAX  (17 * 1024)
#define MAX_MSG_SIZE (17 * 1024)


typedef struct {
     bool                        super;
     IAny                       *proxy;
     IAny                       *real;
     VoodooDispatch              dispatch;
} VoodooInstance;

struct __V_VoodooManager {
     int                         magic;

     int                         fd;

     bool                        quit;

     long long                   millis;

     VoodooClient               *client;     /* Either client ... */
     VoodooServer               *server;     /* ... or server is valid. */

     int                         msg_count;
     VoodooMessageSerial         msg_serial;

     DirectThread               *dispatcher;

     struct {
          pthread_mutex_t        lock;
          DirectHash            *local;
          DirectHash            *remote;
          VoodooInstanceID       last;
     } instances;

     struct {
          pthread_mutex_t        lock;
          pthread_cond_t         wait;
          VoodooResponseMessage *current;
     } response;

     struct {
          DirectThread          *thread;
          pthread_mutex_t        lock;
          pthread_cond_t         wait;
          u8                     buffer[IN_BUF_MAX + MAX_MSG_SIZE];
          int                    start;
          int                    end;
          int                    max;
     } input;

     struct {
          DirectThread          *thread;
          pthread_mutex_t        lock;
          pthread_cond_t         wait;
          u8                     buffer[OUT_BUF_MAX];
          int                    start;
          int                    end;
     } output;
};

/**************************************************************************************************/

static void *manager_dispatch_loop( DirectThread *thread, void *arg );
static void *manager_input_loop   ( DirectThread *thread, void *arg );
static void *manager_output_loop  ( DirectThread *thread, void *arg );

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

static const int one = 1;

/**************************************************************************************************/

#define DUMP_SOCKET_OPTION(o)                                                   \
     val = 0;                                                                   \
     len = 4;                                                                   \
                                                                                \
     if (getsockopt( fd, SOL_SOCKET, o, &val, &len ))                           \
          D_PERROR( "Voodoo/Manager: getsockopt() for " #o " failed!\n" );      \
     else                                                                       \
          D_DEBUG( "Voodoo/Manager: " #o " is %d\n", val );


DirectResult
voodoo_manager_create( int             fd,
                       VoodooClient   *client,
                       VoodooServer   *server,
                       VoodooManager **ret_manager )
{
     DirectResult     ret;
     VoodooManager   *manager;
     int              val;
     unsigned int     len;
     static const int tos = IPTOS_LOWDELAY;

     D_ASSERT( fd >= 0 );
     D_ASSERT( (client != NULL) ^ (server != NULL) );
     D_ASSERT( ret_manager != NULL );

     /* Allocate manager structure. */
     manager = D_CALLOC( 1, sizeof(VoodooManager) );
     if (!manager) {
          D_WARN( "out of memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     D_DEBUG( "Voodoo/Manager: Creating manager at %p.\n", manager );

     if (setsockopt( fd, SOL_IP, IP_TOS, &tos, sizeof(tos) ) < 0)
          D_PERROR( "Voodoo/Manager: Could not set IP_TOS!\n" );

     if (setsockopt( fd, SOL_TCP, TCP_NODELAY, &one, sizeof(one) ) < 0)
          D_PERROR( "Voodoo/Manager: Could not set TCP_NODELAY!\n" );

     DUMP_SOCKET_OPTION( SO_SNDLOWAT );
     DUMP_SOCKET_OPTION( SO_RCVLOWAT );
     DUMP_SOCKET_OPTION( SO_SNDBUF );
     DUMP_SOCKET_OPTION( SO_RCVBUF );

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

     /* Store file descriptor. */
     manager->fd = fd;

     /* Store client or server. */
     manager->client = client;
     manager->server = server;

     /* Initialize all locks. */
     direct_util_recursive_pthread_mutex_init( &manager->instances.lock );
     direct_util_recursive_pthread_mutex_init( &manager->response.lock );
     direct_util_recursive_pthread_mutex_init( &manager->input.lock );
     direct_util_recursive_pthread_mutex_init( &manager->output.lock );

     /* Initialize all wait conditions. */
     pthread_cond_init( &manager->response.wait, NULL );
     pthread_cond_init( &manager->input.wait, NULL );
     pthread_cond_init( &manager->output.wait, NULL );

     /* Set default buffer limit. */
     manager->input.max = IN_BUF_MAX;

     D_MAGIC_SET( manager, VoodooManager );

     /* Create all threads. */
     manager->dispatcher    = direct_thread_create( DTT_MESSAGING, manager_dispatch_loop,
                                                    manager, "Voodoo Dispatch" );

     manager->input.thread  = direct_thread_create( DTT_INPUT, manager_input_loop,
                                                    manager, "Voodoo Input" );

     manager->output.thread = direct_thread_create( DTT_OUTPUT, manager_output_loop,
                                                    manager, "Voodoo Output" );

     /* Return the new manager. */
     *ret_manager = manager;

     return DFB_OK;
}

DirectResult
voodoo_manager_quit( VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSUME( !manager->quit );

     if (manager->quit)
          return DFB_OK;

     D_DEBUG( "Voodoo/Manager: Quitting manager at %p!\n", manager );

     /* Have all threads quit upon this. */
     manager->quit = true;

     /* Acquire locks and wake up waiters. */
     pthread_mutex_lock( &manager->input.lock );
     pthread_cond_broadcast( &manager->input.wait );
     pthread_mutex_unlock( &manager->input.lock );

     pthread_mutex_lock( &manager->response.lock );
     pthread_cond_broadcast( &manager->response.wait );
     pthread_mutex_unlock( &manager->response.lock );

     pthread_mutex_lock( &manager->output.lock );
     pthread_cond_broadcast( &manager->output.wait );
     pthread_mutex_unlock( &manager->output.lock );

     return DFB_OK;
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
     direct_thread_join( manager->input.thread );
     direct_thread_destroy( manager->input.thread );

     direct_thread_join( manager->dispatcher );
     direct_thread_destroy( manager->dispatcher );

     direct_thread_join( manager->output.thread );
     direct_thread_destroy( manager->output.thread );

     /* Destroy conditions. */
     pthread_cond_destroy( &manager->input.wait );
     pthread_cond_destroy( &manager->response.wait );
     pthread_cond_destroy( &manager->output.wait );

     /* Destroy locks. */
     pthread_mutex_destroy( &manager->instances.lock );
     pthread_mutex_destroy( &manager->input.lock );
     pthread_mutex_destroy( &manager->response.lock );
     pthread_mutex_destroy( &manager->output.lock );

     /* Release all remaining interfaces. */
     direct_hash_iterate( manager->instances.local, instance_iterator, (void*) false );
     direct_hash_iterate( manager->instances.local, instance_iterator, (void*) true );
     direct_hash_destroy( manager->instances.local );

     direct_hash_destroy( manager->instances.remote );

     D_MAGIC_CLEAR( manager );

     /* Deallocate manager structure. */
     D_FREE( manager );

     return DFB_OK;
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

     D_DEBUG( "Voodoo/Manager: Sending SUPER message %zu for '%s' (%d bytes).\n", serial, name, size );

     /* Unlock the output buffer. */
     manager_unlock_output( manager, true );


     /* Wait for and lock the response buffer. */
     ret = manager_lock_response( manager, serial, &response );
     if (ret) {
          D_ERROR( "Voodoo/Manager: "
                   "Waiting for the response failed (%s)!\n", DirectResultString( ret ) );
          return ret;
     }

     D_DEBUG( "Voodoo/Manager: Got response %zu (%s) with instance %u for request %zu "
              "(%d bytes).\n", response->header.serial, DirectResultString( ret ),
              response->instance, response->request, response->header.size );

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

     return DFB_OK;
}

static inline int
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

static inline void
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


     D_DEBUG( "Voodoo/Manager: Sending REQUEST message %zu to %u::%u %s(%d bytes).\n",
              serial, instance, method, (flags & VREQ_RESPOND) ? "[RESPONDING] " : "", size );

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

          D_DEBUG( "Voodoo/Manager: Got response %zu (%s) with instance %u for request %zu "
                   "(%d bytes).\n", response->header.serial, DirectResultString( response->result ),
                   response->instance, response->request, response->header.size );

          *ret_response = response;
     }

     return DFB_OK;
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
              "Request %zu, result %d, instance %u...\n", request, result, instance );

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
              "Sending RESPONSE message %zu (%s) with instance %u for request %zu (%d bytes).\n",
              serial, DirectResultString( result ), instance, request, size );

     /* Unlock the output buffer. */
     manager_unlock_output( manager, true );

     return DFB_OK;
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
     D_ASSERT( dispatcher != NULL );
     D_ASSERT( real != NULL );
     D_ASSERT( dispatch != NULL );
     D_ASSERT( ret_instance != NULL );

     instance = D_CALLOC( 1, sizeof(VoodooInstance) );
     if (!instance) {
          D_WARN( "out of memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     instance->super    = super;
     instance->proxy    = dispatcher;
     instance->real     = real;
     instance->dispatch = dispatch;

     pthread_mutex_lock( &manager->instances.lock );

     instance_id = ++manager->instances.last;

     ret = direct_hash_insert( manager->instances.local, instance_id, instance );

     pthread_mutex_unlock( &manager->instances.lock );

     if (ret) {
          D_ERROR( "Voodoo/Manager: Adding a new instance to the dispatcher hash table failed!\n" );
          D_FREE( instance );
          return ret;
     }

     D_DEBUG( "Voodoo/Manager: "
              "Added instance %u, dispatcher %p, real %p.\n", instance_id, dispatcher, real );

     *ret_instance = instance_id;

     return DFB_OK;
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

     pthread_mutex_lock( &manager->instances.lock );

     instance = direct_hash_lookup( manager->instances.local, instance_id );

     pthread_mutex_unlock( &manager->instances.lock );

     if (!instance)
          return DFB_NOSUCHINSTANCE;

     if (ret_dispatcher)
          *ret_dispatcher = instance->proxy;

     if (ret_real)
          *ret_real = instance->real;

     return DFB_OK;
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
          return DFB_NOSYSTEMMEMORY;
     }

     instance->super = super;
     instance->proxy = requestor;

     pthread_mutex_lock( &manager->instances.lock );

     ret = direct_hash_insert( manager->instances.remote, instance_id, instance );

     pthread_mutex_unlock( &manager->instances.lock );

     if (ret) {
          D_ERROR( "Voodoo/Manager: Adding a new instance to the requestor hash table failed!\n" );
          D_FREE( instance );
          return ret;
     }

     D_DEBUG( "Voodoo/Manager: "
              "Added remote instance %u, requestor %p.\n", instance_id, requestor );

     return DFB_OK;
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

     pthread_mutex_lock( &manager->instances.lock );

     instance = direct_hash_lookup( manager->instances.remote, instance_id );

     pthread_mutex_unlock( &manager->instances.lock );

     if (!instance)
          return DFB_NOSUCHINSTANCE;

     *ret_requestor = instance->proxy;

     return DFB_OK;
}

/**************************************************************************************************/

static void
handle_disconnect( VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     if (0) {
          int       num;
          long long millis = direct_clock_get_millis();
          long long diff   = millis - manager->millis;

          num = manager->msg_count * 1000LL / diff;

          D_INFO( "Voodoo/Manager: Average number of messages: %d.%03d k/sec\n", num / 1000, num % 1000 );
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

     D_DEBUG( "Voodoo/Dispatch: Handling SUPER message %zu for '%s' (%d bytes).\n",
              super->header.serial, name, super->header.size );

     if (manager->server) {
          VoodooInstanceID instance;

          ret = voodoo_server_construct( manager->server, manager, name, &instance );
          if (ret) {
               voodoo_manager_respond( manager, super->header.serial,
                                       ret, VOODOO_INSTANCE_NONE,
                                       VMBT_NONE );
               return;
          }

          voodoo_manager_respond( manager, super->header.serial,
                                  DFB_OK, instance,
                                  VMBT_NONE );
     }
     else {
          D_WARN( "can't handle this as a client" );
          voodoo_manager_respond( manager, super->header.serial,
                                  DFB_UNSUPPORTED, VOODOO_INSTANCE_NONE,
                                  VMBT_NONE );
     }
}

typedef struct {
     VoodooManager        *manager;
     VoodooInstance       *instance;
     VoodooRequestMessage *request;
} DispatchAsyncContext;

static void *
dispatch_async_thread( void *arg )
{
     DirectResult          ret;
     DispatchAsyncContext *context  = arg;
     VoodooManager        *manager  = context->manager;
     VoodooInstance       *instance = context->instance;
     VoodooRequestMessage *request  = context->request;

     ret = instance->dispatch( instance->proxy, instance->real, manager, request );

     if (ret && (request->flags & VREQ_RESPOND))
          voodoo_manager_respond( manager, request->header.serial,
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

     D_DEBUG( "Voodoo/Dispatch: Handling REQUEST message %zu to %u::%u %s%s(%d bytes).\n",
              request->header.serial, request->instance, request->method,
              (request->flags & VREQ_RESPOND) ? "[RESPONDING] " : "",
              (request->flags & VREQ_ASYNC) ? "[ASYNC] " : "",
              request->header.size );

     pthread_mutex_lock( &manager->instances.lock );

     instance = direct_hash_lookup( manager->instances.local, request->instance );
     if (!instance) {
          pthread_mutex_unlock( &manager->instances.lock );

          D_ERROR( "Voodoo/Dispatch: "
                   "Requested instance %u doesn't exist (anymore)!\n", request->instance );

          if (request->flags & VREQ_RESPOND)
               voodoo_manager_respond( manager, request->header.serial,
                                       DFB_NOSUCHINSTANCE, VOODOO_INSTANCE_NONE,
                                       VMBT_NONE );

          return;
     }

     if (request->flags & VREQ_ASYNC) {
          pthread_t             thread;
          DispatchAsyncContext *context;

          context = D_MALLOC( sizeof(DispatchAsyncContext) + request->header.size );
          if (!context) {
               D_WARN( "out of memory" );
               pthread_mutex_unlock( &manager->instances.lock );
               return;
          }

          context->manager  = manager;
          context->instance = instance;
          context->request  = (VoodooRequestMessage*) (context + 1);

          direct_memcpy( context->request, request, request->header.size );

          pthread_create( &thread, NULL, dispatch_async_thread, context );
          pthread_detach( thread );
     }
     else {
          ret = instance->dispatch( instance->proxy, instance->real, manager, request );

          if (ret && (request->flags & VREQ_RESPOND))
               voodoo_manager_respond( manager, request->header.serial,
                                       ret, VOODOO_INSTANCE_NONE,
                                       VMBT_NONE );
     }

     pthread_mutex_unlock( &manager->instances.lock );
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

     D_DEBUG( "Voodoo/Dispatch: Handling RESPONSE message %zu (%s) with instance %u for request "
              "%zu (%d bytes).\n", response->header.serial, DirectResultString( response->result ),
              response->instance, response->request, response->header.size );

     pthread_mutex_lock( &manager->response.lock );

     D_ASSERT( manager->response.current == NULL );

     manager->response.current = response;

     pthread_cond_broadcast( &manager->response.wait );

     while (manager->response.current && !manager->quit)
          pthread_cond_wait( &manager->response.wait, &manager->response.lock );

     pthread_mutex_unlock( &manager->response.lock );
}

/**************************************************************************************************/

static void *
manager_dispatch_loop( DirectThread *thread, void *arg )
{
     VoodooManager *manager = arg;

     /* Lock the input buffer. */
     pthread_mutex_lock( &manager->input.lock );

     while (!manager->quit) {
          VoodooMessageHeader *header;
          int                  aligned;

          D_MAGIC_ASSERT( manager, VoodooManager );

          D_DEBUG( "Voodoo/Dispatch: START %d, END %d, MAX %d\n",
                   manager->input.start, manager->input.end, manager->input.max );

          /* Wait for at least four bytes which contain the length of the message. */
          while (manager->input.end - manager->input.start < 4) {
               D_DEBUG( "Voodoo/Dispatch: Waiting for messages...\n" );

               pthread_cond_wait( &manager->input.wait, &manager->input.lock );

               if (manager->quit)
                    break;
          }

          if (manager->quit)
               break;

          /* Get the message header. */
          header  = (VoodooMessageHeader *)(manager->input.buffer + manager->input.start);
          aligned = VOODOO_MSG_ALIGN( header->size );

          D_DEBUG( "Voodoo/Dispatch: Next message has %d (%d) bytes and is of type %d...\n",
                   header->size, aligned, header->type );

          D_ASSERT( header->size >= sizeof(VoodooMessageHeader) );
          D_ASSERT( header->size <= MAX_MSG_SIZE );

          /* Extend the buffer if the message doesn't fit into the default boundary. */
          if (aligned > manager->input.max - manager->input.start) {
               D_ASSERT( manager->input.max == IN_BUF_MAX );

               D_DEBUG( "Voodoo/Dispatch: ...fetching tail of message.\n" );

               manager->input.max = manager->input.start + aligned;

               pthread_cond_broadcast( &manager->input.wait );
          }

          /* Wait until the complete message is received. */
          while (aligned > manager->input.end - manager->input.start) {
               pthread_cond_wait( &manager->input.wait, &manager->input.lock );

               if (manager->quit)
                    break;
          }

          if (manager->quit)
               break;

          /* Unlock the input buffer. */
          pthread_mutex_unlock( &manager->input.lock );

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

          /* Lock the input buffer. */
          pthread_mutex_lock( &manager->input.lock );

          manager->input.start += aligned;

          if (manager->input.start == manager->input.end) {
               if (manager->input.start == manager->input.max)
                    pthread_cond_broadcast( &manager->input.wait );

               manager->input.start = manager->input.end = 0;

               manager->input.max = IN_BUF_MAX;
          }
     }

     /* Unlock the input buffer. */
     pthread_mutex_unlock( &manager->input.lock );

     return NULL;
}

static void *
manager_input_loop( DirectThread *thread, void *arg )
{
     int            len;
     struct pollfd  pf;
     VoodooManager *manager = arg;

     manager->millis = direct_clock_get_millis();

     while (!manager->quit) {
          D_MAGIC_ASSERT( manager, VoodooManager );

          pf.fd     = manager->fd;
          pf.events = POLLIN;

          switch (poll( &pf, 1, 100 )) {
               case -1:
                    if (errno != EINTR) {
                         D_PERROR( "Voodoo/Input: Could not poll() the socket!\n" );
                         usleep( 200000 );
                    }
                    /* fall through */

               case 0:
                    continue;
          }

          pthread_mutex_lock( &manager->input.lock );

          while (manager->input.end == manager->input.max) {
               pthread_cond_wait( &manager->input.wait, &manager->input.lock );

               if (manager->quit)
                    break;
          }

          if (!manager->quit) {
               len = recv( manager->fd, manager->input.buffer + manager->input.end,
                           manager->input.max - manager->input.end, MSG_DONTWAIT );
               if (len < 0) {
                    switch (errno) {
                         case EINTR:
                         case EAGAIN:
                              break;
                         default:
                              D_PERROR( "Voodoo/Input: Could not recv() data!\n" );
                              usleep( 200000 );
                    }
               }
               else if (len > 0) {
                    D_DEBUG( "Voodoo/Input: Received %d bytes...\n", len );

                    manager->input.end += len;

                    pthread_cond_broadcast( &manager->input.wait );
               }
               else
                    handle_disconnect( manager );
          }

          pthread_mutex_unlock( &manager->input.lock );
     }

     return NULL;
}

static void *
manager_output_loop( DirectThread *thread, void *arg )
{
     int            len;
     struct pollfd  pf;
     VoodooManager *manager = arg;

     while (!manager->quit) {
          D_MAGIC_ASSERT( manager, VoodooManager );

          pf.fd     = manager->fd;
          pf.events = POLLOUT;

          switch (poll( &pf, 1, 100 )) {
               case -1:
                    if (errno != EINTR) {
                         D_PERROR( "Voodoo/Output: Could not poll() the socket!\n" );
                         usleep( 200000 );
                    }
                    /* fall through */

               case 0:
                    continue;
          }

          pthread_mutex_lock( &manager->output.lock );

          while (manager->output.start == manager->output.end) {
               struct timeval  now;
               struct timespec timeout;

               D_ASSUME( manager->output.start == 0 );
               D_ASSUME( manager->output.end == 0 );

               gettimeofday( &now, NULL );

               timeout.tv_sec  = now.tv_sec;
               timeout.tv_nsec = (now.tv_usec + 50000) * 1000;

               timeout.tv_sec  += timeout.tv_nsec / 1000000000;
               timeout.tv_nsec %= 1000000000;

               pthread_cond_timedwait( &manager->output.wait, &manager->output.lock, &timeout );

               if (manager->quit)
                    break;
          }

          if (!manager->quit) {
               len = send( manager->fd, manager->output.buffer + manager->output.start,
                           manager->output.end - manager->output.start, MSG_DONTWAIT );
               if (len < 0) {
                    switch (errno) {
                         case EINTR:
                         case EAGAIN:
                              break;
                         default:
                              D_PERROR( "Voodoo/Output: Could not send() data!\n" );
                              usleep( 200000 );
                    }
               }
               else {
                    D_DEBUG( "Voodoo/Output: Sent %d/%d bytes...\n", len, manager->output.end - manager->output.start );

                    manager->output.start += len;

                    if (manager->output.start == manager->output.end) {
                         manager->output.start = manager->output.end = 0;

                         pthread_cond_broadcast( &manager->output.wait );
                    }
               }
          }

          pthread_mutex_unlock( &manager->output.lock );
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
          return DFB_LIMITEXCEEDED;
     }

     aligned = VOODOO_MSG_ALIGN( length );

     pthread_mutex_lock( &manager->output.lock );

     while (manager->output.end + aligned > OUT_BUF_MAX) {
          pthread_cond_broadcast( &manager->output.wait );

          pthread_cond_wait( &manager->output.wait, &manager->output.lock );

          if (manager->quit) {
               pthread_mutex_lock( &manager->output.lock );
               return DFB_DESTROYED;
          }
     }

     *ret_ptr = manager->output.buffer + manager->output.end;

     manager->output.end += aligned;

     return DFB_OK;
}

static DirectResult
manager_unlock_output( VoodooManager *manager,
                       bool           flush )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     if (flush)
          pthread_cond_broadcast( &manager->output.wait );

     pthread_mutex_unlock( &manager->output.lock );

     return DFB_OK;
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

     D_DEBUG( "Voodoo/Manager: Locking response to request %zu...\n", request );

     pthread_mutex_lock( &manager->response.lock );

     while (!manager->quit) {
          response = manager->response.current;
          if (response && response->request == request)
               break;

          if (response)
               D_DEBUG( "Voodoo/Manager: ...current response is for request %zu...\n", response->request );

          D_DEBUG( "Voodoo/Manager: ...(still) waiting for response to request %zu...\n", request );

          pthread_cond_wait( &manager->response.wait, &manager->response.lock );
     }

     if (manager->quit) {
          pthread_mutex_unlock( &manager->response.lock );
          return DFB_DESTROYED;
     }

     D_DEBUG( "Voodoo/Manager: ...locked response %zu to request %zu (%d bytes).\n",
              response->header.serial, request, response->header.size );

     *ret_response = response;

     return DFB_OK;
}

static DirectResult
manager_unlock_response( VoodooManager         *manager,
                         VoodooResponseMessage *response )
{
     D_MAGIC_ASSERT( manager, VoodooManager );
     D_ASSERT( response != NULL );
     D_ASSERT( response == manager->response.current );

     D_DEBUG( "Voodoo/Manager: Unlocking response %zu to request %zu (%d bytes)...\n",
              response->header.serial, response->request, response->header.size );

     manager->response.current = NULL;

     pthread_cond_broadcast( &manager->response.wait );

     pthread_mutex_unlock( &manager->response.lock );

     return DFB_OK;
}

