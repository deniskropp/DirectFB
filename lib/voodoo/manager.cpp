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

extern "C" {
#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/fastlz.h>
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
#include <voodoo/link.h>
}

#include <voodoo/connection_packet.h>
#include <voodoo/connection_raw.h>
#include <voodoo/manager.h>
#include <voodoo/packet.h>


//namespace Voodoo {

D_DEBUG_DOMAIN( Voodoo_Dispatch, "Voodoo/Dispatch", "Voodoo Dispatch" );
D_DEBUG_DOMAIN( Voodoo_Manager,  "Voodoo/Manager",  "Voodoo Manager" );

/**********************************************************************************************************************/

VoodooManager::VoodooManager( VoodooLink     *link,
                              VoodooClient   *client,
                              VoodooServer   *server )
     :
     magic(0),
     is_quit(false),
     millis(0),
     msg_count(0),
     msg_serial(0)
{
     D_ASSERT( link != NULL );
     D_ASSERT( link->Read != NULL );
     D_ASSERT( link->Write != NULL );

     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     /* Store link. */
     this->link = link;

     /* Store client or server. */
     this->client = client;
     this->server = server;


     instances.last   = 0;

     response.current = NULL;


     /* Initialize all locks. */
     direct_recursive_mutex_init( &instances.lock );
     direct_recursive_mutex_init( &response.lock );

     /* Initialize all wait conditions. */
     direct_waitqueue_init( &response.wait_get );
     direct_waitqueue_init( &response.wait_put );

     millis = direct_clock_get_abs_millis();

     D_MAGIC_SET( this, VoodooManager );

     /* Add connection */
     if ((link->code & 0x8000ffff) == 0x80008676)
          connection = new VoodooConnectionPacket( this, link );
     else
          connection = new VoodooConnectionRaw( this, link );
}

VoodooManager::~VoodooManager()
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooManager );

     if (!is_quit)
          quit();

     /* Remove connection */
     delete connection;

     /* Destroy conditions. */
     direct_waitqueue_deinit( &response.wait_get );
     direct_waitqueue_deinit( &response.wait_put );

     /* Destroy locks. */
     direct_mutex_deinit( &instances.lock );
     direct_mutex_deinit( &response.lock );

     /* Release all remaining interfaces. */
     //std::for_each<>
     //direct_hash_iterate( instances.local, instance_iterator, (void*) false );
     //direct_hash_iterate( instances.local, instance_iterator, (void*) true );
     //direct_hash_destroy( instances.local );

     D_MAGIC_CLEAR( this );
}

/**************************************************************************************************/

void
VoodooManager::quit()
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSUME( !is_quit );

     if (is_quit)
          return;

     /* Have all threads quit upon this. */
     is_quit = true;

     /* Acquire locks and wake up waiters. */
     direct_mutex_lock( &response.lock );
     direct_waitqueue_broadcast( &response.wait_get );
     direct_waitqueue_broadcast( &response.wait_put );
     direct_mutex_unlock( &response.lock );
}

void
VoodooManager::handle_disconnect()
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooManager );

     if (0) {
          long long num;
          long long millis = direct_clock_get_millis();
          long long diff   = millis - this->millis;

          num = msg_count * 1000LL / diff;

          D_INFO( "Voodoo/Manager: Average number of messages: %lld.%03lld k/sec\n", num / 1000, num % 1000 );
     }

     D_DEBUG_AT( Voodoo_Manager, "  -> Remote site disconnected from manager at %p!\n", this );

     voodoo_manager_quit( this );
}

void
VoodooManager::handle_super( VoodooSuperMessage *super )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     DirectResult  ret;
     const char   *name;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( super != NULL );
     D_ASSERT( super->header.size >= (int) sizeof(VoodooSuperMessage) );
     D_ASSERT( super->header.type == VMSG_SUPER );

     name = (const char *) (super + 1);

     D_DEBUG_AT( Voodoo_Dispatch, "  -> Handling SUPER message %llu for '%s' (%d bytes).\n",
                 (unsigned long long)super->header.serial, name, super->header.size );

     if (server) {
          VoodooInstanceID instance;

          ret = voodoo_server_construct( server, this, name, &instance );
          if (ret) {
               voodoo_manager_respond( this, true, super->header.serial,
                                       ret, VOODOO_INSTANCE_NONE,
                                       VMBT_NONE );
               return;
          }

          voodoo_manager_respond( this, true, super->header.serial,
                                  DR_OK, instance,
                                  VMBT_NONE );
     }
     else {
          D_WARN( "can't handle this as a client" );
          voodoo_manager_respond( this, true, super->header.serial,
                                  DR_UNSUPPORTED, VOODOO_INSTANCE_NONE,
                                  VMBT_NONE );
     }
}

typedef struct {
     VoodooManager        *manager;
     VoodooInstance       *instance;
     VoodooRequestMessage *request;
} DispatchAsyncContext;

void *
VoodooManager::dispatch_async_thread( DirectThread *thread,
                                      void         *arg )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, arg );

     DirectResult          ret;
     DispatchAsyncContext *context  = (DispatchAsyncContext*) arg;
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

void
VoodooManager::handle_request( VoodooRequestMessage *request )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     DirectResult    ret;
     VoodooInstance *instance;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( request != NULL );
     D_ASSERT( request->header.size >= (int) sizeof(VoodooRequestMessage) );
     D_ASSERT( request->header.type == VMSG_REQUEST );

     D_DEBUG_AT( Voodoo_Dispatch, "  -> Handling REQUEST message %llu to %u::%u %s%s(%d bytes).\n",
                 (unsigned long long)request->header.serial, request->instance, request->method,
                 (request->flags & VREQ_RESPOND) ? "[RESPONDING] " : "",
                 (request->flags & VREQ_ASYNC) ? "[ASYNC] " : "",
                 request->header.size );

     direct_mutex_lock( &instances.lock );

     InstanceMap::iterator itr = instances.local.find( request->instance );

     if (itr == instances.local.end()) {
          direct_mutex_unlock( &instances.lock );

          D_ERROR( "Voodoo/Dispatch: "
                   "Requested instance %u doesn't exist (anymore)!\n", request->instance );

          if (request->flags & VREQ_RESPOND)
               voodoo_manager_respond( this, true, request->header.serial,
                                       DR_NOSUCHINSTANCE, VOODOO_INSTANCE_NONE,
                                       VMBT_NONE );

          return;
     }

     instance = (*itr).second;

     if (request->flags & VREQ_ASYNC) {
          DirectThread         *thread;
          DispatchAsyncContext *context;

          context = (DispatchAsyncContext*) D_MALLOC( sizeof(DispatchAsyncContext) + request->header.size );
          if (!context) {
               D_WARN( "out of memory" );
               direct_mutex_unlock( &instances.lock );
               return;
          }

          context->manager  = this;
          context->instance = instance;
          context->request  = (VoodooRequestMessage*) (context + 1);

          direct_memcpy( context->request, request, request->header.size );

          thread = direct_thread_create( DTT_DEFAULT, dispatch_async_thread, context, "Voodoo Async" );
          direct_thread_detach( thread );
          // FIXME: free thread?
     }
     else {
          ret = instance->dispatch( instance->proxy, instance->real, this, request );

          if (ret && (request->flags & VREQ_RESPOND))
               voodoo_manager_respond( this, true, request->header.serial,
                                       ret, VOODOO_INSTANCE_NONE,
                                       VMBT_NONE );
     }

     direct_mutex_unlock( &instances.lock );
}

void
VoodooManager::handle_response( VoodooResponseMessage *msg )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( msg != NULL );
     D_ASSERT( msg->header.size >= (int) sizeof(VoodooResponseMessage) );
     D_ASSERT( msg->header.type == VMSG_RESPONSE );
     D_ASSERT( msg->request < msg_serial );

     D_DEBUG_AT( Voodoo_Dispatch, "  -> Handling RESPONSE message %llu (%s) with instance %u for request "
                 "%llu (%d bytes).\n", (unsigned long long)msg->header.serial, DirectResultString( msg->result ),
                 msg->instance, (unsigned long long)msg->request, msg->header.size );

     direct_mutex_lock( &response.lock );

     D_ASSERT( response.current == NULL );

     response.current = msg;

     direct_mutex_unlock( &response.lock );


     direct_waitqueue_broadcast( &response.wait_get );

     direct_mutex_lock( &response.lock );

     while (response.current && !is_quit)
          direct_waitqueue_wait( &response.wait_put, &response.lock );

     direct_mutex_unlock( &response.lock );
}

/**************************************************************************************************/

DirectResult
VoodooManager::lock_response( VoodooMessageSerial     request,
                              VoodooResponseMessage **ret_response )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     VoodooResponseMessage *msg = NULL;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( ret_response != NULL );

     D_DEBUG_AT( Voodoo_Manager, "  -> Locking response to request %llu...\n", (unsigned long long)request );

     direct_mutex_lock( &response.lock );

     while (!is_quit) {
          msg = response.current;
          if (msg && msg->request == request)
               break;

          if (msg)
               D_DEBUG_AT( Voodoo_Manager, "  -> ...current response is for request %llu...\n", (unsigned long long)msg->request );

          D_DEBUG_AT( Voodoo_Manager, "  -> ...(still) waiting for response to request %llu...\n", (unsigned long long)request );

          direct_waitqueue_wait( &response.wait_get, &response.lock );
     }

     if (is_quit) {
          D_ERROR( "Voodoo/Manager: Quit while waiting for response!\n" );
          direct_mutex_unlock( &response.lock );
          return DR_DESTROYED;
     }

     D_DEBUG_AT( Voodoo_Manager, "  -> ...locked response %llu to request %llu (%d bytes).\n",
                 (unsigned long long)msg->header.serial, (unsigned long long)request, msg->header.size );

     *ret_response = msg;

     return DR_OK;
}

DirectResult
VoodooManager::unlock_response( VoodooResponseMessage *msg )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( msg != NULL );
     D_ASSERT( msg == response.current );

     D_DEBUG_AT( Voodoo_Manager, "  -> Unlocking response %llu to request %llu (%d bytes)...\n",
                 (unsigned long long)msg->header.serial, (unsigned long long)msg->request, msg->header.size );

     response.current = NULL;

     direct_mutex_unlock( &response.lock );

     direct_waitqueue_broadcast( &response.wait_put );

     return DR_OK;
}




DirectResult
VoodooManager::do_super( const char       *name,
                         VoodooInstanceID *ret_instance )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     DirectResult           ret;
     int                    len;
     int                    size;
     VoodooPacket          *packet;
     VoodooMessageSerial    serial;
     VoodooSuperMessage    *msg;
     VoodooResponseMessage *response;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( name != NULL );
     D_ASSERT( ret_instance != NULL );

     /* Calculate the total message size. */
     len  = strlen( name ) + 1;
     size = sizeof(VoodooSuperMessage) + len;


     /* Lock the output buffer for direct writing. */
     packet = connection->GetPacket( size );
     if (!packet)
          return DR_FAILURE;

     msg = (VoodooSuperMessage*) packet->data_raw();

     serial = msg_serial++;

     /* Fill message header. */
     msg->header.size   = size;
     msg->header.serial = serial;
     msg->header.type   = VMSG_SUPER;

     /* Append the name of the super interface to create. */
     direct_memcpy( msg + 1, name, len );

     D_DEBUG_AT( Voodoo_Manager, "  -> Sending SUPER message %llu for '%s' (%d bytes).\n", (unsigned long long)serial, name, size );

     /* Unlock the output buffer. */
     connection->PutPacket( packet, true );


     /* Wait for and lock the response buffer. */
     ret = lock_response( serial, &response );
     if (ret) {
          D_ERROR( "Voodoo/Manager: "
                   "Waiting for the response failed (%s)!\n", DirectResultString( ret ) );
          return ret;
     }

     D_DEBUG_AT( Voodoo_Manager, "  -> Got response %llu (%s) with instance %u for request %llu "
                 "(%d bytes).\n", (unsigned long long)response->header.serial, DirectResultString( ret ),
                 response->instance, (unsigned long long)response->request, response->header.size );

     ret = response->result;
     if (ret) {
          D_ERROR( "Voodoo/Manager: Could not create remote super interface '%s' (%s)!\n",
                   name, DirectResultString( ret ) );
          unlock_response( response );
          return ret;
     }

     D_INFO( "Voodoo/Manager: Created remote super interface '%s'.\n", name );

     /* Return the new instance ID. */
     *ret_instance = response->instance;

     /* Unlock the response buffer. */
     unlock_response( response );

     return DR_OK;
}

int
VoodooManager::calc_blocks( VoodooMessageBlockType type, va_list args )
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

//                    D_ASSERT( len > 0 );
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
                    len = strlen( (const char*) ptr ) + 1;

                    D_ASSERT( ptr != NULL );

                    D_DEBUG( "Voodoo/Message: + STRING '%s' at %p with length %d\n", (char*) ptr, ptr, len );
                    break;

               default:
                    D_BREAK( "unknown message block type" );
          }

          size += 8 + VOODOO_MSG_ALIGN(len);

          type = (VoodooMessageBlockType) va_arg( args, int );
     }

     return size;
}

void
VoodooManager::write_blocks( void *dst, VoodooMessageBlockType type, va_list args )
{
     u32 *d32 = (u32*) dst;

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
                    len = strlen( (const char*) ptr ) + 1;
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
          type = (VoodooMessageBlockType) va_arg( args, int );
     }

     /* Write terminator. */
     d32[0] = VMBT_NONE;
}

DirectResult
VoodooManager::do_request( VoodooInstanceID         instance,
                           VoodooMethodID           method,
                           VoodooRequestFlags       flags,
                           VoodooResponseMessage  **ret_response,
                           VoodooMessageBlockType   block_type,
                           va_list                  args )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     DirectResult          ret;
     int                   size;
     VoodooPacket         *packet;
     VoodooMessageSerial   serial;
     VoodooRequestMessage *msg;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( instance != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_response != NULL || !(flags & VREQ_RESPOND) );
     D_ASSUME( (flags & (VREQ_RESPOND | VREQ_QUEUE)) != (VREQ_RESPOND | VREQ_QUEUE) );

     D_DEBUG_AT( Voodoo_Manager, "  -> Instance %u, method %u, flags 0x%08x...\n", instance, method, flags );

     /* Calculate the total message size. */
#ifdef __GNUC__
     va_list args2;

     va_copy( args2, args );
     size = sizeof(VoodooRequestMessage) + calc_blocks( block_type, args2 );
     va_end( args2 );
#else
     size = sizeof(VoodooRequestMessage) + calc_blocks( block_type, args );
#endif

     D_DEBUG_AT( Voodoo_Manager, "  -> complete message size: %d\n", size );

     /* Lock the output buffer for direct writing. */
     packet = connection->GetPacket( size );
     if (!packet)
          return DR_FAILURE;

     msg = (VoodooRequestMessage*) packet->data_raw();

     serial = msg_serial++;

     /* Fill message header. */
     msg->header.size   = size;
     msg->header.serial = serial;
     msg->header.type   = VMSG_REQUEST;

     /* Fill message body. */
     msg->instance = instance;
     msg->method   = method;
     msg->flags    = flags;

     /* Append custom data. */
     write_blocks( msg + 1, block_type, args );


     D_DEBUG_AT( Voodoo_Manager, "  -> Sending REQUEST message %llu to %u::%u %s(%d bytes).\n",
                 (unsigned long long)serial, instance, method, (flags & VREQ_RESPOND) ? "[RESPONDING] " : "", size );

     /* Unlock the output buffer. */
     connection->PutPacket( packet, !(flags & VREQ_QUEUE) );

     /* Wait for and lock the response buffer. */
     if (flags & VREQ_RESPOND) {
          VoodooResponseMessage *response;

          ret = lock_response( serial, &response );
          if (ret) {
               D_ERROR( "Voodoo/Manager: "
                        "Waiting for the response failed (%s)!\n", DirectResultString( ret ) );
               return ret;
          }

          D_DEBUG_AT( Voodoo_Manager, "  -> Got response %llu (%s) with instance %u for request %llu "
                      "(%d bytes).\n", (unsigned long long)response->header.serial, DirectResultString( response->result ),
                      response->instance, (unsigned long long)response->request, response->header.size );

          *ret_response = response;
     }

     return DR_OK;
}

DirectResult
VoodooManager::next_response( VoodooResponseMessage  *response,
                              VoodooResponseMessage **ret_response )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     DirectResult        ret;
     VoodooMessageSerial serial;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( response != NULL );

     serial = response->request;

     /* Unlock the response buffer. */
     unlock_response( response );

     ret = lock_response( serial, &response );
     if (ret) {
          D_ERROR( "Voodoo/Manager: "
                   "Waiting for the response failed (%s)!\n", DirectResultString( ret ) );
          return ret;
     }

     D_DEBUG_AT( Voodoo_Manager, "  -> Got response %llu (%s) with instance %u for request %llu "
                 "(%d bytes).\n", (unsigned long long)response->header.serial, DirectResultString( response->result ),
                 response->instance, (unsigned long long)response->request, response->header.size );

     *ret_response = response;

     return DR_OK;
}

DirectResult
VoodooManager::finish_request( VoodooResponseMessage *response )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( response != NULL );

     /* Unlock the response buffer. */
     return unlock_response( response );
}

DirectResult
VoodooManager::do_respond( bool                   flush,
                           VoodooMessageSerial    request,
                           DirectResult           result,
                           VoodooInstanceID       instance,
                           VoodooMessageBlockType block_type,
                           va_list                args )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     int                    size;
     VoodooPacket          *packet;
     VoodooMessageSerial    serial;
     VoodooResponseMessage *msg;

     D_MAGIC_ASSERT( this, VoodooManager );

     D_DEBUG_AT( Voodoo_Manager, "  -> Request %llu, result %d, instance %u...\n", (unsigned long long)request, result, instance );

     /* Calculate the total message size. */
#ifdef __GNUC__
     va_list args2;

     va_copy( args2, args );
     size = sizeof(VoodooResponseMessage) + calc_blocks( block_type, args2 );
     va_end( args2 );
#else
     size = sizeof(VoodooResponseMessage) + calc_blocks( block_type, args );
#endif

     D_DEBUG_AT( Voodoo_Manager, "  -> complete message size: %d\n", size );


     /* Lock the output buffer for direct writing. */
     packet = connection->GetPacket( size );
     if (!packet)
          return DR_FAILURE;

     msg = (VoodooResponseMessage*) packet->data_raw();

     serial = msg_serial++;

     /* Fill message header. */
     msg->header.size   = size;
     msg->header.serial = serial;
     msg->header.type   = VMSG_RESPONSE;

     /* Fill message body. */
     msg->request  = request;
     msg->result   = result;
     msg->instance = instance;

     /* Append custom data. */
     write_blocks( msg + 1, block_type, args );


     D_DEBUG_AT( Voodoo_Manager, "  -> Sending RESPONSE message %llu (%s) with instance %u for request %llu (%d bytes).\n",
                 (unsigned long long)serial, DirectResultString( result ), instance, (unsigned long long)request, size );

     /* Unlock the output buffer. */
     connection->PutPacket( packet, flush );

     return DR_OK;
}



/*
static bool
instance_iterator( DirectHash    *hash,
                   unsigned long  key,
                   void          *value,
                   void          *ctx )
{
     bool            super    = (unsigned long) ctx;
     VoodooInstance *instance = (VoodooInstance*) value;

     D_ASSERT( instance != NULL );

     if (instance->super != super) {
          if (super)
               D_FREE( instance );

          return true;
     }


     D_DEBUG_AT( Voodoo_Manager, "  -> Releasing dispatcher interface %p %s(instance %lu)...\n",
                 instance->proxy, instance->super ? "[super] " : "", key );

     D_ASSERT( instance->proxy != NULL );
     D_ASSERT( instance->proxy->Release != NULL );

     instance->proxy->Release( instance->proxy );


     D_DEBUG_AT( Voodoo_Manager, "  -> Releasing real interface %p %s(instance %lu)...\n",
                 instance->real, instance->super ? "[super] " : "", key );

     D_ASSERT( instance->real != NULL );
     D_ASSERT( instance->real->Release != NULL );

     instance->real->Release( instance->real );

     if (super)
          D_FREE( instance );

     return true;
}
*/



DirectResult
VoodooManager::register_local( bool              super,
                               void             *dispatcher,
                               void             *real,
                               VoodooDispatch    dispatch,
                               VoodooInstanceID *ret_instance )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     VoodooInstance   *instance;
     VoodooInstanceID  instance_id;

     D_MAGIC_ASSERT( this, VoodooManager );
//     D_ASSERT( dispatcher != NULL );
//     D_ASSERT( real != NULL );
     D_ASSERT( dispatch != NULL );
     D_ASSERT( ret_instance != NULL );

     instance = (VoodooInstance*) D_CALLOC( 1, sizeof(VoodooInstance) );
     if (!instance) {
          D_WARN( "out of memory" );
          return DR_NOLOCALMEMORY;
     }

     instance->super    = super;
     instance->proxy    = (IAny*) dispatcher;
     instance->real     = (IAny*) real;
     instance->dispatch = dispatch;

     direct_mutex_lock( &instances.lock );

     instance_id = ++instances.last;

     instances.local[instance_id] = instance;

     direct_mutex_unlock( &instances.lock );

     D_DEBUG_AT( Voodoo_Manager, "  -> Added instance %u, dispatcher %p, real %p.\n", instance_id, dispatcher, real );

     *ret_instance = instance_id;

     return DR_OK;
}

DirectResult
VoodooManager::unregister_local( VoodooInstanceID instance_id )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     VoodooInstance *instance;

     D_MAGIC_ASSERT( this, VoodooManager );

     direct_mutex_lock( &instances.lock );

     InstanceMap::iterator itr = instances.local.find( instance_id );

     if (itr == instances.local.end()) {
          direct_mutex_unlock( &instances.lock );
          return DR_NOSUCHINSTANCE;
     }

     instance = (*itr).second;

     instances.local.erase( itr );

     direct_mutex_unlock( &instances.lock );

     D_FREE( instance );

     return DR_OK;
}

DirectResult
VoodooManager::lookup_local( VoodooInstanceID   instance_id,
                             void             **ret_dispatcher,
                             void             **ret_real )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     VoodooInstance *instance;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( instance_id != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_dispatcher != NULL || ret_real != NULL );

     direct_mutex_lock( &instances.lock );

     InstanceMap::iterator itr = instances.local.find( instance_id );

     direct_mutex_unlock( &instances.lock );

     if (itr == instances.local.end())
          return DR_NOSUCHINSTANCE;

     instance = (*itr).second;

     if (ret_dispatcher)
          *ret_dispatcher = instance->proxy;

     if (ret_real)
          *ret_real = instance->real;

     return DR_OK;
}

DirectResult
VoodooManager::register_remote( bool              super,
                                void             *requestor,
                                VoodooInstanceID  instance_id )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     VoodooInstance *instance;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( requestor != NULL );
     D_ASSERT( instance_id != VOODOO_INSTANCE_NONE );

     instance = (VoodooInstance*) D_CALLOC( 1, sizeof(VoodooInstance) );
     if (!instance) {
          D_WARN( "out of memory" );
          return DR_NOLOCALMEMORY;
     }

     instance->super = super;
     instance->proxy = (IAny*) requestor;

     direct_mutex_lock( &instances.lock );

     instances.remote[instance_id] = instance;

     direct_mutex_unlock( &instances.lock );

     D_DEBUG_AT( Voodoo_Manager, "  -> Added remote instance %u, requestor %p.\n", instance_id, requestor );

     return DR_OK;
}


DirectResult
VoodooManager::lookup_remote( VoodooInstanceID   instance_id,
                              void             **ret_requestor )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     VoodooInstance *instance;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( instance_id != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_requestor != NULL );

     direct_mutex_lock( &instances.lock );

     InstanceMap::iterator itr = instances.remote.find( instance_id );

     direct_mutex_unlock( &instances.lock );

     if (itr == instances.remote.end())
          return DR_NOSUCHINSTANCE;

     instance = (*itr).second;

     *ret_requestor = instance->proxy;

     return DR_OK;
}

//}








/**********************************************************************************************************************/
/**********************************************************************************************************************/
/**********************************************************************************************************************/


DirectResult
voodoo_manager_create( VoodooLink     *link,
                       VoodooClient   *client,
                       VoodooServer   *server,
                       VoodooManager **ret_manager )
{
     D_ASSERT( ret_manager != NULL );

     *ret_manager = new VoodooManager( link, client, server );

     return DR_OK;
}

DirectResult
voodoo_manager_quit( VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     manager->quit();

     return DR_OK;
}

DirectResult
voodoo_manager_destroy( VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     delete manager;

     return DR_OK;
}

bool
voodoo_manager_is_closed( const VoodooManager *manager )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->is_quit;
}

/**************************************************************************************************/

DirectResult
voodoo_manager_super( VoodooManager    *manager,
                      const char       *name,
                      VoodooInstanceID *ret_instance )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->do_super( name, ret_instance );
}

DirectResult
voodoo_manager_request( VoodooManager           *manager,
                        VoodooInstanceID         instance,
                        VoodooMethodID           method,
                        VoodooRequestFlags       flags,
                        VoodooResponseMessage  **ret_response,
                        VoodooMessageBlockType   block_type, ... )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     va_list ap;

     va_start( ap, block_type );

     DirectResult ret = manager->do_request( instance, method, flags, ret_response, block_type, ap );

     va_end( ap );

     return ret;
}

DirectResult
voodoo_manager_next_response( VoodooManager          *manager,
                              VoodooResponseMessage  *response,
                              VoodooResponseMessage **ret_response )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->next_response( response, ret_response );
}

DirectResult
voodoo_manager_finish_request( VoodooManager         *manager,
                               VoodooResponseMessage *response )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->finish_request( response );
}

DirectResult
voodoo_manager_respond( VoodooManager          *manager,
                        bool                    flush,
                        VoodooMessageSerial     request,
                        DirectResult            result,
                        VoodooInstanceID        instance,
                        VoodooMessageBlockType  block_type, ... )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     va_list ap;

     va_start( ap, block_type );

     DirectResult ret = manager->do_respond( flush, request, result, instance, block_type, ap );

     va_end( ap );

     return ret;
}

DirectResult
voodoo_manager_register_local( VoodooManager    *manager,
                               bool              super,
                               void             *dispatcher,
                               void             *real,
                               VoodooDispatch    dispatch,
                               VoodooInstanceID *ret_instance )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->register_local( super, dispatcher, real, dispatch, ret_instance );
}

DirectResult
voodoo_manager_unregister_local( VoodooManager    *manager,
                                 VoodooInstanceID  instance_id )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->unregister_local( instance_id );
}

DirectResult
voodoo_manager_lookup_local( VoodooManager     *manager,
                             VoodooInstanceID   instance_id,
                             void             **ret_dispatcher,
                             void             **ret_real )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->lookup_local( instance_id, ret_dispatcher, ret_real );
}

DirectResult
voodoo_manager_register_remote( VoodooManager    *manager,
                                bool              super,
                                void             *requestor,
                                VoodooInstanceID  instance_id )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->register_remote( super, requestor, instance_id );
}


DirectResult
voodoo_manager_lookup_remote( VoodooManager     *manager,
                              VoodooInstanceID   instance_id,
                              void             **ret_requestor )
{
     D_MAGIC_ASSERT( manager, VoodooManager );

     return manager->lookup_remote( instance_id, ret_requestor );
}

DirectResult
voodoo_manager_check_allocation( VoodooManager *manager,
                                 unsigned int   amount )
{
#ifndef WIN32
     FILE   *f;
     char    buf[2000];
     int     size;
     char   *p;
     size_t  bytes;

     D_MAGIC_ASSERT( manager, VoodooManager );

     if (!voodoo_config->memory_max)
          return DR_OK;

     direct_snprintf( buf, sizeof(buf), "/proc/%d/status", direct_getpid() );

     f = fopen( buf, "r" );
     if (!f) {
          D_ERROR( "Could not open '%s'!\n", buf );
          return DR_FAILURE;
     }

     bytes = fread( buf, 1, sizeof(buf)-1, f );

     fclose( f );

     if (bytes) {
          buf[bytes] = 0;

          p = strstr( buf, "VmRSS:" );
          if (!p) {
               D_ERROR( "Could not find memory information!\n" );
               return DR_FAILURE;
          }
     
          sscanf( p + 6, " %u", &size );
     
          D_INFO( "SIZE: %u kB (+%u kB)\n", size, amount / 1024 );
     
          if (size * 1024 + amount > voodoo_config->memory_max)
               return DR_LIMITEXCEEDED;
     }
#endif
     return DR_OK;
}

