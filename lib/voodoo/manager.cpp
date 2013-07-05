/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <algorithm>

extern "C" {
#include <direct/debug.h>
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
#include <voodoo/dispatcher.h>
#include <voodoo/manager.h>
#include <voodoo/packet.h>
#include <voodoo/play_internal.h>


//namespace Voodoo {

D_DEBUG_DOMAIN( Voodoo_Dispatch, "Voodoo/Dispatch", "Voodoo Dispatch" );
D_DEBUG_DOMAIN( Voodoo_Manager,  "Voodoo/Manager",  "Voodoo Manager" );
D_DEBUG_DOMAIN( Voodoo_Time,     "Voodoo/Time",     "Voodoo Time" );

/**********************************************************************************************************************/

class TimeService : public VoodooInstance
{
     virtual DirectResult Dispatch( VoodooManager        *manager,
                                    VoodooRequestMessage *msg )
     {
          long long now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

          D_DEBUG_AT( Voodoo_Time, "VoodooInstance::%s( %p, manager %p, msg %p )\n", __func__, this, manager, msg );

          D_MAGIC_ASSERT( this, VoodooInstance );

          return voodoo_manager_respond( manager, true, msg->header.serial,
                                         DR_OK, VOODOO_INSTANCE_NONE,
                                         VMBT_UINT, (unsigned long long) now >> 32,
                                         VMBT_UINT, (unsigned long long) now,
                                         VMBT_NONE );
     }
};

static TimeService time_service;

/**********************************************************************************************************************/

VoodooManager::VoodooManager( VoodooConnection *connection,
                              VoodooContext    *context )
     :
     magic(0),
     is_quit(false),
     msg_count(0),
     msg_serial(0)
{
     DirectResult ret;

     D_ASSERT( connection != NULL );
     D_ASSERT( context != NULL );

     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     /* Store link and context */
     this->connection = connection;
     this->context    = context;


     instances.last   = 0;

     response.current = NULL;


     /* Initialize all locks. */
     direct_recursive_mutex_init( &instances.lock );
     direct_recursive_mutex_init( &response.lock );

     /* Initialize all wait conditions. */
     direct_waitqueue_init( &response.wait_get );
     direct_waitqueue_init( &response.wait_put );

     D_MAGIC_SET( this, VoodooManager );


     dispatcher = new VoodooDispatcher( this );


     register_local( &time_service, &local_time_service_id );


     connection->Start( this );


     ret = do_super( "TimeService", &remote_time_service_id );
     if (ret)
          D_DERROR( ret, "Voodoo/Manager: Failed to query for TimeService!\n" );
     else
          connection->SetupTime();
}

static void
instance_iterator( std::pair<VoodooInstanceID,VoodooInstance*> pair )
{
     D_DEBUG_AT( Voodoo_Manager, "%s( id %u, instance %p )\n", __func__, pair.first, pair.second );

     pair.second->Release();
}

VoodooManager::~VoodooManager()
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooManager );

     if (!is_quit)
          quit();

     connection->Stop();

     /* Destroy dispatcher */
     delete dispatcher;

     /* Remove connection */
     delete connection;

     /* Destroy conditions. */
     direct_waitqueue_deinit( &response.wait_get );
     direct_waitqueue_deinit( &response.wait_put );

     /* Destroy locks. */
     direct_mutex_deinit( &instances.lock );
     direct_mutex_deinit( &response.lock );

     /* Release all remaining interfaces. */
     std::for_each( instances.remote.begin(), instances.remote.end(), instance_iterator );
     std::for_each( instances.local.begin(), instances.local.end(), instance_iterator );

     D_MAGIC_CLEAR( this );
}

/**********************************************************************************************************************/

void
VoodooManager::DispatchPacket( VoodooPacket *packet )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p, packet %p )\n", __func__, this, packet );

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSUME( !is_quit );

     if (is_quit)
          return;

     dispatcher->PutPacket( packet );
}

bool
VoodooManager::DispatchReady()
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooManager );
//     D_ASSUME( !is_quit );

     if (is_quit)
          return false;

     return dispatcher->Ready();
}

/**********************************************************************************************************************/

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

     unregister_local( local_time_service_id );

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

     D_DEBUG_AT( Voodoo_Manager, "  -> Remote site disconnected from manager at %p!\n", this );

     quit();
}

void
VoodooManager::handle_super( VoodooSuperMessage *super )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     DirectResult  ret = DR_OK;
     const char   *name;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( super != NULL );
     D_ASSERT( super->header.size >= (int) sizeof(VoodooSuperMessage) );
     D_ASSERT( super->header.type == VMSG_SUPER );

     name = (const char *) (super + 1);

     D_DEBUG_AT( Voodoo_Dispatch, "  -> Handling SUPER message %llu for '%s' (%d bytes).\n",
                 (unsigned long long)super->header.serial, name, super->header.size );

     VoodooInstanceID instance_id;

     if (!strcmp( name, "TimeService" ))
          instance_id = local_time_service_id;
     else
          ret = context->HandleSuper( this, name, &instance_id );

     if (ret)
          do_respond( true, super->header.serial, ret );
     else
          do_respond( true, super->header.serial, DR_OK, instance_id );
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

     ret = instance->Dispatch( manager, request );

     if (ret && (request->flags & VREQ_RESPOND))
          manager->do_respond( true, request->header.serial, ret );

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
               do_respond( true, request->header.serial, DR_NOSUCHINSTANCE );

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
          ret = instance->Dispatch( this, request );

          if (ret && (request->flags & VREQ_RESPOND))
               do_respond( true, request->header.serial, ret );
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

void
VoodooManager::handle_discover( VoodooMessageHeader *header )
{
     DirectResult         ret;
     int                  size;
     VoodooPacket        *packet;
     VoodooMessageSerial  serial;
     VoodooMessageHeader *msg;
     VoodooPlayer        *player;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( header != NULL );
     D_ASSERT( header->size >= (int) sizeof(VoodooMessageHeader) );
     D_ASSERT( header->type == VMSG_DISCOVER );

     D_DEBUG( "Voodoo/Dispatch: Handling DISCOVER message %llu (%d bytes).\n",
              (unsigned long long)header->serial, header->size );

     /* Get player singleton. */
     ret = voodoo_player_create( NULL, &player );
     if (ret) {
          D_DERROR( ret, "Voodoo/Manager: Error creating player singleton!\n" );
          return;
     }

     /* Calculate the total message size. */
     size = sizeof(VoodooMessageHeader) + sizeof(VoodooPlayVersion) + sizeof(VoodooPlayInfo);

     /* Lock the output buffer for direct writing. */
     packet = connection->GetPacket( size );
     if (!packet)
          return;

     msg = (VoodooMessageHeader*) packet->data_raw();

     serial = msg_serial++;

     /* Fill message header. */
     msg->size   = size;
     msg->serial = serial;
     msg->type   = VMSG_SENDINFO;

     /* Fill message body. */
     direct_memcpy( (u8*) packet->data_raw() + sizeof(VoodooMessageHeader),
                    &player->version, sizeof(VoodooPlayVersion) );

     direct_memcpy( (u8*) packet->data_raw() + sizeof(VoodooMessageHeader) + sizeof(VoodooPlayVersion),
                    &player->info, sizeof(VoodooPlayInfo) );


     D_DEBUG( "Voodoo/Manager: Sending SENDINFO message %llu (%d bytes).\n", (unsigned long long)serial, size );

     /* Unlock the output buffer. */
     connection->PutPacket( packet, true );
}

long long
VoodooManager::connection_delay()
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p ) <- %lld\n", __func__, this, connection->GetDelay() );

     return connection->GetDelay();
}

long long
VoodooManager::clock_to_local( long long remote )
{
     long long local;

     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p, %lld )\n", __func__, this, remote );

     local = remote - connection->GetTimeDiff();

     D_DEBUG_AT( Voodoo_Manager, "  => %lld (diff %lld)\n", local, connection->GetTimeDiff() );

     return local;
}

long long
VoodooManager::clock_to_remote( long long local )
{
     long long remote;

     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p, %lld )\n", __func__, this, local );

     remote = local + connection->GetTimeDiff();

     D_DEBUG_AT( Voodoo_Manager, "  => %lld (diff %lld)\n", remote, connection->GetTimeDiff() );

     return remote;
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

     if (is_quit) {
          D_DEBUG_AT( Voodoo_Manager, "  -> QUIT!\n" );
          return DR_IO;
     }

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
                 "(%d bytes).\n", (unsigned long long)response->header.serial, DirectResultString( response->result ),
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

void
VoodooManager::write_blocks( void                     *dst,
                             const VoodooMessageBlock *blocks,
                             size_t                    num )
{
     size_t  i;
     u32    *d32 = (u32*) dst;

     for (i=0; i<num; i++) {
          /* Write block type and length. */
          d32[0] = blocks[i].type;
          d32[1] = blocks[i].len;

          /* Write block content. */
          if (blocks[i].ptr) {
               u32 *s32 = (u32*) blocks[i].ptr;

               switch (blocks[i].len) {
                    case 16:
                         d32[5] = s32[3];
                    case 12:
                         d32[4] = s32[2];
                    case 8:
                         d32[3] = s32[1];
                    case 4:
                         d32[2] = s32[0];
                         break;

                    default:
                         direct_memcpy( &d32[2], blocks[i].ptr, blocks[i].len );
               }
          }
          else if (blocks[i].len) {
               D_ASSERT( blocks[i].len == 4 );

               d32[2] = blocks[i].val;
          }

          /* Advance message data pointer. */
          d32 += 2 + (VOODOO_MSG_ALIGN(blocks[i].len) >> 2);
     }

     /* Write terminator. */
     d32[0] = VMBT_NONE;
}

DirectResult
VoodooManager::do_request( VoodooInstanceID         instance,
                           VoodooMethodID           method,
                           VoodooRequestFlags       flags,
                           VoodooResponseMessage  **ret_response,
                           VoodooMessageBlock      *blocks,
                           size_t                   num_blocks,
                           size_t                   data_size )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     DirectResult          ret;
     size_t                size;
     VoodooPacket         *packet;
     VoodooMessageSerial   serial;
     VoodooRequestMessage *msg;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( instance != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_response != NULL || !(flags & VREQ_RESPOND) );
     D_ASSUME( (flags & (VREQ_RESPOND | VREQ_QUEUE)) != (VREQ_RESPOND | VREQ_QUEUE) );

     D_DEBUG_AT( Voodoo_Manager, "  -> Instance %u, method %u, flags 0x%08x...\n", instance, method, flags );

     if (is_quit) {
          D_DEBUG_AT( Voodoo_Manager, "  -> QUIT!\n" );
          return DR_IO;
     }

     /* Calculate the total message size. */
     size = sizeof(VoodooRequestMessage) + data_size;

     D_DEBUG_AT( Voodoo_Manager, "  -> complete message size: "_ZU"\n", size );

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
     write_blocks( msg + 1, blocks, num_blocks );


     D_DEBUG_AT( Voodoo_Manager, "  -> Sending REQUEST message %llu to %u::%u %s("_ZU" bytes).\n",
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
VoodooManager::do_respond( bool                 flush,
                           VoodooMessageSerial  request,
                           DirectResult         result,
                           VoodooInstanceID     instance,
                           VoodooMessageBlock  *blocks,
                           size_t               num_blocks,
                           size_t               data_size )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     size_t                 size;
     VoodooPacket          *packet;
     VoodooMessageSerial    serial;
     VoodooResponseMessage *msg;

     D_MAGIC_ASSERT( this, VoodooManager );

     D_DEBUG_AT( Voodoo_Manager, "  -> Request %llu, result %d, instance %u...\n", (unsigned long long)request, result, instance );

     if (is_quit) {
          D_DEBUG_AT( Voodoo_Manager, "  -> QUIT!\n" );
          return DR_IO;
     }

     /* Calculate the total message size. */
     size = sizeof(VoodooResponseMessage) + data_size;

     D_DEBUG_AT( Voodoo_Manager, "  -> complete message size: "_ZU"\n", size );


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
     write_blocks( msg + 1, blocks, num_blocks );


     D_DEBUG_AT( Voodoo_Manager, "  -> Sending RESPONSE message %llu (%s) with instance %u for request %llu ("_ZU" bytes).\n",
                 (unsigned long long)serial, DirectResultString( result ), instance, (unsigned long long)request, size );

     /* Unlock the output buffer. */
     connection->PutPacket( packet, flush );

     return DR_OK;
}

DirectResult
VoodooManager::register_local( VoodooInstance   *instance,
                               VoodooInstanceID *ret_instance )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     VoodooInstanceID instance_id;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( instance != NULL );
     D_ASSERT( ret_instance != NULL );

     instance->AddRef();

     direct_mutex_lock( &instances.lock );

     instance_id = ++instances.last;

     instances.local[instance_id] = instance;

     direct_mutex_unlock( &instances.lock );

     D_DEBUG_AT( Voodoo_Manager, "  -> Added local instance %u (%p)\n", instance_id, instance );

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

     instance->Release();

     return DR_OK;
}

DirectResult
VoodooManager::lookup_local( VoodooInstanceID   instance_id,
                             VoodooInstance   **ret_instance )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     VoodooInstance *instance;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( instance_id != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_instance != NULL );

     direct_mutex_lock( &instances.lock );

     InstanceMap::iterator itr = instances.local.find( instance_id );

     direct_mutex_unlock( &instances.lock );

     if (itr == instances.local.end())
          return DR_NOSUCHINSTANCE;

     instance = (*itr).second;

     // FIXME: addref?

     *ret_instance = instance;

     return DR_OK;
}

DirectResult
VoodooManager::register_remote( VoodooInstance   *instance,
                                VoodooInstanceID  instance_id )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( instance != NULL );
     D_ASSERT( instance_id != VOODOO_INSTANCE_NONE );

     instance->AddRef();

     direct_mutex_lock( &instances.lock );

     instances.remote[instance_id] = instance;

     direct_mutex_unlock( &instances.lock );

     D_DEBUG_AT( Voodoo_Manager, "  -> Added remote instance %u (%p)\n", instance_id, instance );

     return DR_OK;
}

DirectResult
VoodooManager::unregister_remote( VoodooInstanceID instance_id )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     VoodooInstance *instance;

     D_MAGIC_ASSERT( this, VoodooManager );

     direct_mutex_lock( &instances.lock );

     InstanceMap::iterator itr = instances.remote.find( instance_id );

     if (itr == instances.remote.end()) {
          direct_mutex_unlock( &instances.lock );
          return DR_NOSUCHINSTANCE;
     }

     instance = (*itr).second;

     instances.remote.erase( itr );

     direct_mutex_unlock( &instances.lock );

     instance->Release();

     return DR_OK;
}

DirectResult
VoodooManager::lookup_remote( VoodooInstanceID   instance_id,
                              VoodooInstance   **ret_instance )
{
     D_DEBUG_AT( Voodoo_Manager, "VoodooManager::%s( %p )\n", __func__, this );

     VoodooInstance *instance;

     D_MAGIC_ASSERT( this, VoodooManager );
     D_ASSERT( instance_id != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_instance != NULL );

     direct_mutex_lock( &instances.lock );

     InstanceMap::iterator itr = instances.remote.find( instance_id );

     direct_mutex_unlock( &instances.lock );

     if (itr == instances.remote.end())
          return DR_NOSUCHINSTANCE;

     instance = (*itr).second;

     // FIXME: addref?

     *ret_instance = instance;

     return DR_OK;
}

//}

