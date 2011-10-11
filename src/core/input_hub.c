/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

#include <directfb_build.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/messages.h>

#include <core/input.h>
#include <core/input_hub.h>

#include <One/One.h>


D_DEBUG_DOMAIN( Core_InputHub, "Core/Input/Hub", "DirectFB Input Core Hub" );

/**********************************************************************************************************************/


#if DIRECTFB_BUILD_ONE


struct __CoreDFB__CoreInputHub {
     OneThread                    *thread;

     OneQID                        method_qid;
     OneQID                        notify_qid;

     DirectMutex                   lock;
};

struct __CoreDFB__CoreInputHubClient {
     OneThread                    *thread;

     OneQID                        listen_qid;
     OneQID                        remote_qid;

     CoreInputHubClientCallbacks   callbacks;
     void                         *ctx;
};

/**********************************************************************************************************************/

typedef struct {
     OneQID     listen_qid;
} InputHubAttachRequest;

typedef enum {
     IHNT_DEVICE_ADD,
     IHNT_DEVICE_REMOVE,
     IHNT_EVENT_DISPATCH,
} InputHubNotificationType;

typedef struct {
     InputHubNotificationType  type;

     union {
          struct {
               DFBInputDeviceID          device_id;
               DFBInputDeviceDescription description;
          } device_add;

          struct {
               DFBInputDeviceID          device_id;
          } device_remove;

          struct {
               DFBInputDeviceID          device_id;
               DFBInputEvent             event;
          } event_dispatch;
     } data;
} InputHubNotification;

/**********************************************************************************************************************/

static DFBEnumerationResult
CoreInputHub_EnumDevice_Callback( CoreInputDevice *device,
                                  void            *ctx )
{
     InputHubNotification   notification;
     InputHubAttachRequest *request = ctx;

     memset( &notification, 0, sizeof(notification) );

     notification.type = IHNT_DEVICE_ADD;

     notification.data.device_add.device_id = dfb_input_device_id( device );

     dfb_input_device_description( device, &notification.data.device_add.description );

     OneQueue_Dispatch( request->listen_qid, &notification, sizeof(notification) );

     return DFENUM_OK;
}

/**********************************************************************************************************************/

static void
CoreInputHub_Dispatch( void                  *context,
                       const OnePacketHeader *header,
                       void                  *data,
                       OneThread             *thread )
{
     InputHubAttachRequest *request = data;
     CoreInputHub          *hub     = context;

     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &hub->lock );

     // FIXME: replace by own list of devices recorded in CoreInputHub_AddDevice/RemoveDevice calls
     dfb_input_enumerate_devices( CoreInputHub_EnumDevice_Callback, request, DICAPS_ALL );

     OneQueue_Attach( hub->notify_qid, request->listen_qid );

     direct_mutex_unlock( &hub->lock );
}

/**********************************************************************************************************************/

DFBResult
CoreInputHub_Create( u32            queue_id,
                     CoreInputHub **ret_hub )
{
     DFBResult     ret;
     CoreInputHub *hub;

     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     D_ASSERT( ret_hub != NULL );

     One_Initialize();

     hub = D_CALLOC( 1, sizeof(CoreInputHub) );
     if (!hub)
          return D_OOM();

     direct_mutex_init( &hub->lock );

     ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, queue_id, &hub->method_qid );
     if (ret)
          goto error;

     ret = OneQueue_New( ONE_QUEUE_VIRTUAL, ONE_QID_NONE, &hub->notify_qid );
     if (ret)
          goto error;

     ret = OneThread_Create( "Input Hub", &hub->thread );
     if (ret)
          goto error;

     ret = OneThread_AddQueue( hub->thread, hub->method_qid, CoreInputHub_Dispatch, hub );
     if (ret)
          goto error;

     D_INFO( "Core/Input/Hub: Running at QID %u\n", hub->method_qid );

     *ret_hub = hub;

     return DFB_OK;


error:
     if (hub->thread)
          OneThread_Destroy( hub->thread );

     if (hub->notify_qid)
          OneQueue_Destroy( hub->notify_qid );

     if (hub->method_qid)
          OneQueue_Destroy( hub->method_qid );

     direct_mutex_deinit( &hub->lock );

     D_FREE( hub );

     return ret;
}

DFBResult
CoreInputHub_Destroy( CoreInputHub *hub )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     D_ASSERT( hub != NULL );

     OneThread_Destroy( hub->thread );

     OneQueue_Destroy( hub->notify_qid );

     OneQueue_Destroy( hub->method_qid );

     direct_mutex_deinit( &hub->lock );

     D_FREE( hub );

     return DFB_OK;
}

DFBResult
CoreInputHub_AddDevice( CoreInputHub                    *hub,
                        DFBInputDeviceID                 device_id,
                        const DFBInputDeviceDescription *desc )
{
     InputHubNotification notification;

     D_ASSERT( hub != NULL );
     D_ASSERT( desc != NULL );

     D_DEBUG_AT( Core_InputHub, "%s( ID %u, '%s' )\n", __FUNCTION__, device_id, desc->name );

     memset( &notification, 0, sizeof(notification) );

     notification.type = IHNT_DEVICE_ADD;

     notification.data.device_add.device_id   = device_id;
     notification.data.device_add.description = *desc;

     direct_mutex_lock( &hub->lock );

     OneQueue_Dispatch( hub->notify_qid, &notification, sizeof(notification) );

     direct_mutex_unlock( &hub->lock );

     return DFB_OK;
}

DFBResult
CoreInputHub_RemoveDevice( CoreInputHub     *hub,
                           DFBInputDeviceID  device_id )
{
     InputHubNotification notification;

     D_DEBUG_AT( Core_InputHub, "%s( ID %u )\n", __FUNCTION__, device_id );

     D_ASSERT( hub != NULL );

     memset( &notification, 0, sizeof(notification) );

     notification.type = IHNT_DEVICE_REMOVE;

     notification.data.device_remove.device_id = device_id;

     direct_mutex_lock( &hub->lock );

     OneQueue_Dispatch( hub->notify_qid, &notification, sizeof(notification) );

     direct_mutex_unlock( &hub->lock );

     return DFB_OK;
}

DFBResult
CoreInputHub_DispatchEvent( CoreInputHub        *hub,
                            DFBInputDeviceID     device_id,
                            const DFBInputEvent *event )
{
     InputHubNotification notification;

     D_DEBUG_AT( Core_InputHub, "%s( ID %u, %s )\n", __FUNCTION__, device_id, dfb_input_event_type_name(event->type) );

     D_ASSERT( hub != NULL );
     D_ASSERT( event != NULL );

     memset( &notification, 0, sizeof(notification) );

     notification.type = IHNT_EVENT_DISPATCH;

     notification.data.event_dispatch.device_id = device_id;
     notification.data.event_dispatch.event     = *event;

     direct_mutex_lock( &hub->lock );

     OneQueue_Dispatch( hub->notify_qid, &notification, sizeof(notification) );

     direct_mutex_unlock( &hub->lock );

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
CoreInputHubClient_Dispatch_DeviceAdd( CoreInputHubClient         *client,
                                       const InputHubNotification *notification )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     if (client->callbacks.DeviceAdd)
          client->callbacks.DeviceAdd( client->ctx, notification->data.device_add.device_id, &notification->data.device_add.description );
}

static void
CoreInputHubClient_Dispatch_DeviceRemove( CoreInputHubClient         *client,
                                          const InputHubNotification *notification )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     if (client->callbacks.DeviceRemove)
          client->callbacks.DeviceRemove( client->ctx, notification->data.device_remove.device_id );
}

static void
CoreInputHubClient_Dispatch_EventDispatch( CoreInputHubClient         *client,
                                           const InputHubNotification *notification )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     if (client->callbacks.EventDispatch)
          client->callbacks.EventDispatch( client->ctx, notification->data.event_dispatch.device_id, &notification->data.event_dispatch.event );
}

static void
CoreInputHubClient_Dispatch( void                  *context,
                             const OnePacketHeader *header,
                             void                  *data,
                             OneThread             *thread )
{
     CoreInputHubClient         *client       = context;
     const InputHubNotification *notification = data;

     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     switch (notification->type) {
          case IHNT_DEVICE_ADD:
               CoreInputHubClient_Dispatch_DeviceAdd( client, notification );
               break;

          case IHNT_DEVICE_REMOVE:
               CoreInputHubClient_Dispatch_DeviceRemove( client, notification );
               break;

          case IHNT_EVENT_DISPATCH:
               CoreInputHubClient_Dispatch_EventDispatch( client, notification );
               break;

          default:
               D_BUG( "unknown notification type %d", notification->type );
     }
}

/**********************************************************************************************************************/

DFBResult
CoreInputHubClient_Create( u32                                 remote_qid,
                           const CoreInputHubClientCallbacks  *callbacks,
                           void                               *ctx,
                           CoreInputHubClient                **ret_client )
{
     DFBResult           ret;
     CoreInputHubClient *client;

     D_DEBUG_AT( Core_InputHub, "%s( QID 0x%08x )\n", __FUNCTION__, remote_qid );

     D_ASSERT( callbacks != NULL );
     D_ASSERT( ret_client != NULL );

     One_Initialize();

     client = D_CALLOC( 1, sizeof(CoreInputHubClient) );
     if (!client)
          return D_OOM();

     client->remote_qid = remote_qid;
     client->callbacks  = *callbacks;
     client->ctx        = ctx;

     ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, ONE_QID_NONE, &client->listen_qid );
     if (ret)
          goto error;

     ret = OneThread_Create( "Input Hub Client", &client->thread );
     if (ret)
          goto error;

     ret = OneThread_AddQueue( client->thread, client->listen_qid, CoreInputHubClient_Dispatch, client );
     if (ret)
          goto error;

     *ret_client = client;

     return DFB_OK;


error:
     if (client->thread)
          OneThread_Destroy( client->thread );

     if (client->listen_qid)
          OneQueue_Destroy( client->listen_qid );

     D_FREE( client );

     return ret;
}

DFBResult
CoreInputHubClient_Destroy( CoreInputHubClient *client )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     OneThread_Destroy( client->thread );

     OneQueue_Destroy( client->listen_qid );

     D_FREE( client );

     return DFB_OK;
}

DFBResult
CoreInputHubClient_Activate( CoreInputHubClient *client )
{
     InputHubAttachRequest request;

     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     request.listen_qid = client->listen_qid;

     OneQueue_Dispatch( client->remote_qid, &request, sizeof(request) );

     return DFB_OK;
}

#else /* DIRECT_BUILD_ONE */


DFBResult
CoreInputHub_Create( u32            queue_id,
                     CoreInputHub **ret_hub )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     return DFB_UNSUPPORTED;
}

DFBResult
CoreInputHub_Destroy( CoreInputHub *hub )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     return DFB_UNSUPPORTED;
}

DFBResult
CoreInputHub_AddDevice( CoreInputHub                    *hub,
                        DFBInputDeviceID                 device_id,
                        const DFBInputDeviceDescription *desc )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     return DFB_UNSUPPORTED;
}

DFBResult
CoreInputHub_RemoveDevice( CoreInputHub     *hub,
                           DFBInputDeviceID  device_id )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     return DFB_UNSUPPORTED;
}

DFBResult
CoreInputHub_DispatchEvent( CoreInputHub        *hub,
                            DFBInputDeviceID     device_id,
                            const DFBInputEvent *event )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     return DFB_UNSUPPORTED;
}

/**********************************************************************************************************************/

DFBResult
CoreInputHubClient_Create( u32                                 remote_qid,
                           const CoreInputHubClientCallbacks  *callbacks,
                           void                               *ctx,
                           CoreInputHubClient                **ret_client )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     return DFB_UNSUPPORTED;
}

DFBResult
CoreInputHubClient_Destroy( CoreInputHubClient *client )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     return DFB_UNSUPPORTED;
}

DFBResult
CoreInputHubClient_Activate( CoreInputHubClient *client )
{
     D_DEBUG_AT( Core_InputHub, "%s()\n", __FUNCTION__ );

     return DFB_UNSUPPORTED;
}

#endif

