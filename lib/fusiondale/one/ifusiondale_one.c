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



#include <config.h>

#include <fusiondale.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/map.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <One/One.h>

#include <core/messenger.h>

#include <ifusiondale.h>
#include <messenger/ifusiondalemessenger.h>

#include "ifusiondale_one.h"
#include "icoma_one.h"
#include "ifusiondalemessenger_one.h"

static DirectResult Probe( void );
static DirectResult Construct( IFusionDale *thiz, const char *host, int session );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionDale, One )


D_DEBUG_DOMAIN( IFusionDale_One, "IFusionDale/One", "IFusionDale One" );

/**************************************************************************************************/

typedef struct __DecoupledGetComponentCall DecoupledGetComponentCall;

struct __DecoupledGetComponentCall {
     DirectLink            link;

     IFusionDale_One_data *data;

     GetComponentRequest   request;

     DirectThread         *thread;
};

/**************************************************************************************************/

typedef struct {
     char           name[FUSIONDALE_NAME_LENGTH];
} NSMapKey;

typedef struct {
     NSMapKey       key;

     OneQID         qid;

     unsigned int   notifications;

     /* notification QIDs follow */
} NSMapEntry;

static bool
ns_map_compare( DirectMap    *map,
                const void   *key,
                void         *object,
                void         *ctx )
{
     const NSMapKey *map_key   = key;
     NSMapEntry     *map_entry = object;

     return strcmp( map_key->name, map_entry->key.name ) == 0;
}

static unsigned int
ns_map_hash( DirectMap    *map,
             const void   *key,
             void         *ctx )
{
     size_t          i    = 0;
     unsigned int    hash = 0;
     const NSMapKey *map_key = key;

     while (map_key->name[i]) {
          hash = hash * 131 + map_key->name[i];

          i++;
     }

     return hash;
}

static DirectEnumerationResult
ns_iterator( DirectMap *map,
             void      *object,
             void      *ctx )
{
     NSMapEntry *entry = object;

     D_FREE( entry );

     return DENUM_OK;
}

/**************************************************************************************************/

typedef struct {
     char           name[FUSIONDALE_NAME_LENGTH];
} EventMapKey;

typedef struct {
     EventMapKey    key;

     OneQID         qid;

     int            refs;
} EventMapEntry;

static bool
event_map_compare( DirectMap    *map,
                   const void   *key,
                   void         *object,
                   void         *ctx )
{
     const EventMapKey *map_key   = key;
     EventMapEntry     *map_entry = object;

     return strcmp( map_key->name, map_entry->key.name ) == 0;
}

static unsigned int
event_map_hash( DirectMap    *map,
                const void   *key,
                void         *ctx )
{
     size_t             i    = 0;
     unsigned int       hash = 0;
     const EventMapKey *map_key = key;

     while (map_key->name[i]) {
          hash = hash * 131 + map_key->name[i];

          i++;
     }

     return hash;
}

static DirectEnumerationResult
event_iterator( DirectMap *map,
                void      *object,
                void      *ctx )
{
     EventMapEntry *entry = object;

     D_FREE( entry );

     return DENUM_OK;
}

/**************************************************************************************************/

static DirectResult ShutdownNS( IFusionDale *thiz );

static void
IFusionDale_One_Destruct( IFusionDale *thiz )
{
     IFusionDale_One_data *data = thiz->priv;

     D_DEBUG_AT( IFusionDale_One, "%s (%p)\n", __FUNCTION__, thiz );

     if (data->thread)
          OneThread_Destroy( data->thread );

     ShutdownNS( thiz );

     One_Shutdown();

     if (data->core)
          fd_core_destroy( data->core, false );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     if (ifusiondale_singleton == thiz)
          ifusiondale_singleton = NULL;
}

/**************************************************************************************************/

static DirectResult
IFusionDale_One_AddRef( IFusionDale *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionDale_One);

     data->base.ref++;

     return DR_OK;
}

static DirectResult
IFusionDale_One_Release( IFusionDale *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionDale_One)

     if (--data->base.ref == 0)
          IFusionDale_One_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IFusionDale_One_EnterComa( IFusionDale  *thiz,
                           const char   *name,
                           IComa       **ret_coma )
{
     DirectResult          ret;
     DirectInterfaceFuncs *funcs;
     void                 *interface_ptr;

     DIRECT_INTERFACE_GET_DATA(IFusionDale_One)

     D_DEBUG_AT( IFusionDale_One, "%s( '%s' )\n", __FUNCTION__, name );

     D_ASSERT( name != NULL );
     D_ASSERT( ret_coma != NULL );

     ret = DirectGetInterface( &funcs, "IComa", "One", NULL, NULL );
     if (ret)
          return ret;

     ret = funcs->Allocate( &interface_ptr );
     if (ret)
          return ret;

     ret = funcs->Construct( thiz, interface_ptr, data->ns_qid, name, data->thread );
     if (ret)
          return ret;

     *ret_coma = interface_ptr;

     return DR_OK;
}

static DirectResult
IFusionDale_One_CreateMessenger( IFusionDale           *thiz,
                                 IFusionDaleMessenger **ret_interface )
{
     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     if (fusion_config->secure_fusion) {
          DirectResult          ret;
          DirectInterfaceFuncs *funcs;
          void                 *interface_ptr;

          DIRECT_INTERFACE_GET_DATA(IFusionDale_One)

          D_ASSERT( ret_interface != NULL );

          ret = DirectGetInterface( &funcs, "IFusionDaleMessenger", "One", NULL, NULL );
          if (ret)
               return ret;

          ret = funcs->Allocate( &interface_ptr );
          if (ret)
               return ret;

          ret = funcs->Construct( interface_ptr, thiz, data->thread );
          if (ret)
               return ret;

          *ret_interface = interface_ptr;
     }
     else {
          DirectResult          ret;
          CoreMessenger        *messenger;
          IFusionDaleMessenger *interface;

          DIRECT_INTERFACE_GET_DATA(IFusionDale_One)

          /* Check arguments */
          if (!ret_interface)
               return DR_INVARG;

          /* Create a new messenger. */
          ret = fd_messenger_create( data->core, &messenger );
          if (ret)
               return ret;

          DIRECT_ALLOCATE_INTERFACE( interface, IFusionDaleMessenger );

          ret = IFusionDaleMessenger_Construct( interface, data->core, messenger );

          fd_messenger_unref( messenger );

          if (ret)
               return ret;

          *ret_interface = interface;
     }

     return DR_OK;
}

static DirectResult
IFusionDale_One_GetMessenger( IFusionDale           *thiz,
                              IFusionDaleMessenger **ret_interface )
{
     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     if (fusion_config->secure_fusion) {
          return IFusionDale_One_CreateMessenger( thiz, ret_interface );
     }
     else {
          DirectResult          ret;
          CoreMessenger        *messenger, *tmp;
          IFusionDaleMessenger *interface;

          DIRECT_INTERFACE_GET_DATA(IFusionDale_One)

          /* Check arguments */
          if (!ret_interface)
               return DR_INVARG;

          /* Try to get the messenger. */
          ret = fd_core_get_messenger( data->core, 1, &messenger );
          switch (ret) {
               case DR_OK:
                    break;

               case DR_IDNOTFOUND:
                    /* Create a temporary messenger... */
                    ret = fd_messenger_create( data->core, &tmp );
                    if (ret)
                         return ret;

                    /* ...but get the first messenger, to work around race conditions... */
                    ret = fd_core_get_messenger( data->core, 1, &messenger );

                    /* ...and unref our temporary (most probably the same one). */
                    fd_messenger_unref( tmp );

                    if (ret)
                         return ret;
                    break;

               default:
                    return ret;
          }

          DIRECT_ALLOCATE_INTERFACE( interface, IFusionDaleMessenger );

          ret = IFusionDaleMessenger_Construct( interface, data->core, messenger );

          fd_messenger_unref( messenger );

          if (ret)
               return ret;

          *ret_interface = interface;
     }

     return DR_OK;
}

/**************************************************************************************************/

static DirectResult
DispatchCreateComponent( IFusionDale_One_data         *data,
                         const CreateComponentRequest *request )
{
     NSMapEntry              *entry;
     CreateComponentResponse  response;

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( IFusionDale_One, "  -> name '%s'\n", request->name );
     D_DEBUG_AT( IFusionDale_One, "  -> method QID 0x%08x\n", request->method_qid );

     entry = D_CALLOC( 1, sizeof(NSMapEntry) + request->notifications * sizeof(OneQID) );
     if (!entry)
          return D_OOM();

     entry->qid           = request->method_qid;
     entry->notifications = request->notifications;

     direct_snputs( entry->key.name, request->name, FUSIONDALE_NAME_LENGTH );

     direct_memcpy( entry + 1, request + 1, request->notifications * sizeof(OneQID) );

     if (direct_map_lookup( data->ns_components_map, &entry->key )) {
          D_FREE( entry );
          return DR_BUSY;
     }

     response.result = direct_map_insert( data->ns_components_map, &entry->key, entry );
     if (response.result)
          D_FREE( entry );

     direct_waitqueue_broadcast( &data->ns_wq );

     return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
}

static void *
NSGetComponent_Thread( DirectThread *thread,
                       void         *arg )
{
     DirectResult               ret;
     NSMapKey                   key;
     GetComponentResponse       response;
     DecoupledGetComponentCall *call    = arg;
     IFusionDale_One_data      *data    = call->data;
     const GetComponentRequest *request = &call->request;
     long long                  now     = direct_clock_get_abs_micros();
     long long                  end     = now + request->timeout_ms * 1000;

     D_DEBUG_AT( IFusionDale_One, "%s( '%s' )\n", __FUNCTION__, request->name );

     direct_snputs( key.name, request->name, FUSIONDALE_NAME_LENGTH );

     direct_mutex_lock( &data->ns_lock );

     while (!data->ns_stop) {
          NSMapEntry *entry;

          entry = direct_map_lookup( data->ns_components_map, &key );
          if (entry) {
               void   *datas[2];
               size_t  lengths[2];

               D_DEBUG_AT( IFusionDale_One, "  -> method QID 0x%08x\n", entry->qid );

               response.result        = DR_OK;
               response.method_qid    = entry->qid;
               response.notifications = entry->notifications;

               datas[0]   = &response;
               lengths[0] = sizeof(response);

               datas[1]   = entry + 1;
               lengths[1] = entry->notifications * sizeof(OneQID);

               ret = OneQueue_DispatchV( request->response_qid, datas, lengths, entry->notifications ? 2 : 1 );
               if (ret)
                    D_DEBUG_AT( IFusionDale_One, "  -> ERROR sending GetComponent response: %s\n", DirectResultString( ret ) );

               goto cleanup;
          }

          if (request->timeout_ms > 0) {
               now = direct_clock_get_abs_micros();

               if (end <= now) {
                    D_DEBUG_AT( IFusionDale_One, "  -> TIMEOUT! (waiting for %s)\n", request->name );

                    response.result = DR_TIMEOUT;

                    ret = OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
                    if (ret)
                         D_DEBUG_AT( IFusionDale_One, "  -> ERROR sending GetComponent response: %s\n", DirectResultString( ret ) );

                    goto cleanup;
               }

               direct_waitqueue_wait_timeout( &data->ns_wq, &data->ns_lock, end - now );
          }
          else
               direct_waitqueue_wait( &data->ns_wq, &data->ns_lock );
     }

cleanup:
     direct_list_remove( &data->ns_getcomponent_calls, &call->link );

     direct_mutex_unlock( &data->ns_lock );

     D_FREE( call );

     return NULL;
}

static DirectResult
DecoupleGetComponent( IFusionDale_One_data      *data,
                      const GetComponentRequest *request )
{
     DecoupledGetComponentCall *call;

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( IFusionDale_One, "  -> name '%s'\n", request->name );

     call = D_CALLOC( 1, sizeof(DecoupledGetComponentCall) );
     if (!call)
          return D_OOM();

     direct_list_append( &data->ns_getcomponent_calls, &call->link );

     call->data    = data;
     call->request = *request;
     call->thread  = direct_thread_create( DTT_DEFAULT, NSGetComponent_Thread, call, "NSGetComponent" );

     return DR_OK;
}

static DirectResult
DispatchGetComponent( IFusionDale_One_data      *data,
                      const GetComponentRequest *request )
{
     NSMapKey              key;
     NSMapEntry           *entry;
     GetComponentResponse  response;
     void                 *datas[2];
     size_t                lengths[2];

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( IFusionDale_One, "  -> name '%s'\n", request->name );

     direct_snputs( key.name, request->name, FUSIONDALE_NAME_LENGTH );

     entry = direct_map_lookup( data->ns_components_map, &key );
     if (!entry) {
//          if (request->timeout_ms)
               return DecoupleGetComponent( data, request );

          D_DEBUG_AT( IFusionDale_One, "  -> NO ENTRY FOUND (TIMEOUT == 0)\n" );

          response.result = DR_IDNOTFOUND;

          return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
     }

     D_DEBUG_AT( IFusionDale_One, "  -> method QID 0x%08x\n", entry->qid );

     response.result        = DR_OK;
     response.method_qid    = entry->qid;
     response.notifications = entry->notifications;

     datas[0]   = &response;
     lengths[0] = sizeof(response);

     datas[1]   = entry + 1;
     lengths[1] = entry->notifications * sizeof(OneQID);

     return OneQueue_DispatchV( request->response_qid, datas, lengths, entry->notifications ? 2 : 1 );
}

static DirectResult
DispatchPing( IFusionDale_One_data *data,
              const PingRequest    *request )
{
     PingResponse response;

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( IFusionDale_One, "  -> stamp %lld\n", request->stamp_us );

     response.stamp_us = direct_clock_get_abs_micros();

     return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
}

static DirectResult
DispatchRegisterEvent( IFusionDale_One_data       *data,
                       const RegisterEventRequest *request )
{
     DirectResult           ret;
     EventMapKey            key;
     EventMapEntry         *entry;
     RegisterEventResponse  response;
     OneQID                 qid;

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( IFusionDale_One, "  -> name '%s'\n", request->name );

     direct_snputs( key.name, request->name, FUSIONDALE_NAME_LENGTH );

     entry = direct_map_lookup( data->ns_events_map, &key );
     if (entry) {
          D_DEBUG_AT( IFusionDale_One, "  -> EVENT ENTRY FOUND\n" );

          entry->refs++;

          response.result    = DR_OK;
          response.event_qid = entry->qid;

          return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
     }

     entry = D_CALLOC( 1, sizeof(EventMapEntry) );
     if (!entry)
          return D_OOM();

     ret = OneQueue_New( ONE_QUEUE_VIRTUAL, ONE_QID_NONE, &qid );
     if (ret) {
          D_DEBUG_AT( IFusionDale_One, "  -> FAILED TO CREATE VIRTUAL QUEUE\n" );

          response.result = DR_FAILURE;

          return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
     }

     OneQueue_SetName( qid, request->name );

     direct_snputs( entry->key.name, request->name, FUSIONDALE_NAME_LENGTH );

     entry->refs = 1;
     entry->qid  = qid;

     response.result = direct_map_insert( data->ns_events_map, &entry->key, entry );
     if (response.result) {
          D_FREE( entry );

          OneQueue_Destroy( qid );

          response.result = DR_FAILURE;

          return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
     }

     response.result    = DR_OK;
     response.event_qid = qid;

     return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
}

static DirectResult
DispatchUnregisterEvent( IFusionDale_One_data         *data,
                         const UnregisterEventRequest *request )
{
     EventMapKey              key;
     EventMapEntry           *entry;
     UnregisterEventResponse  response;

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( IFusionDale_One, "  -> name '%s'\n", request->name );

     direct_snputs( key.name, request->name, FUSIONDALE_NAME_LENGTH );

     entry = direct_map_lookup( data->ns_events_map, &key );
     if (!entry) {
          D_DEBUG_AT( IFusionDale_One, "  -> NO EVENT ENTRY FOUND\n" );

          response.result = DR_ITEMNOTFOUND;

          return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
     }

     entry->refs--;

     if (!entry->refs) {
          OneQueue_Destroy( entry->qid );

          direct_map_remove( data->ns_events_map, &key );

          D_FREE( entry );
     }

     response.result = DR_OK;

     return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
}

static DirectResult
DispatchGetEvent( IFusionDale_One_data  *data,
                  const GetEventRequest *request )
{
     EventMapKey       key;
     EventMapEntry    *entry;
     GetEventResponse  response;

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( IFusionDale_One, "  -> name '%s'\n", request->name );

     direct_snputs( key.name, request->name, FUSIONDALE_NAME_LENGTH );

     entry = direct_map_lookup( data->ns_events_map, &key );
     if (!entry) {
          D_DEBUG_AT( IFusionDale_One, "  -> NO ENTRY FOUND\n" );

          response.result = DR_ITEMNOTFOUND;

          return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
     }

     D_DEBUG_AT( IFusionDale_One, "  -> event QID 0x%08x\n", entry->qid );

     response.result = DR_OK;

     return OneQueue_Dispatch( request->response_qid, &response, sizeof(response) );
}

#define RECEIVE_BUFFER_SIZE   16384

static void *
DispatchNS( DirectThread *thread,
            void         *arg )
{
     DirectResult          ret;
     IFusionDale_One_data *data = arg;
     char                 *buf;

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     buf = D_MALLOC( RECEIVE_BUFFER_SIZE );
     if (!buf) {
          D_OOM();
          return NULL;
     }

     while (!data->ns_stop) {
          size_t length;
          size_t offset;
//#define ONE_QUEUE_WORKAROUND
#ifndef ONE_QUEUE_WORKAROUND
          ret = OneQueue_Receive( &data->ns_qid, 1, buf, RECEIVE_BUFFER_SIZE, &length, false, 0 );
#else
          ret = OneQueue_Receive( &data->ns_qid, 1, buf, RECEIVE_BUFFER_SIZE, &length, false, 100 );
          if (ret == DR_TIMEOUT )
               continue;
#endif
          if (ret) {
               D_DERROR( ret, "IFusionDale/One: Could not receive from NS Queue!\n" );
               break;
          }

          D_DEBUG_AT( IFusionDale_One, "%s() -> received %zu bytes\n", __FUNCTION__, length );

          for (offset=0; offset < length; ) {
               OnePacketHeader        *header           = (OnePacketHeader *)(buf + offset);
               size_t                  size             = header->uncompressed;
               NSRequestType          *type             = (NSRequestType *)( header + 1 );
               CreateComponentRequest *create_component = (CreateComponentRequest *)( type + 1 );
               GetComponentRequest    *get_component    = (GetComponentRequest *)( type + 1 );
               PingRequest            *ping             = (PingRequest *)( type + 1 );
               RegisterEventRequest   *register_event   = (RegisterEventRequest *)( type + 1 );
               UnregisterEventRequest *unregister_event = (UnregisterEventRequest *)( type + 1 );
               GetEventRequest        *get_event        = (GetEventRequest *)( type + 1 );

               D_DEBUG_AT( IFusionDale_One, "  -> size %zu\n", size );

               offset += sizeof(OnePacketHeader) + size;

               if (offset > length) {
                    D_WARN( "invalid packet (offset %zu, length %zu)", offset, length );
                    continue;
               }

               if (size < sizeof(NSRequestType)) {
                    D_WARN( "invalid packet, no type" );
                    continue;
               }

               size -= sizeof(NSRequestType);

               switch (*type) {
                    case NS_CREATE_COMPONENT:
                         D_DEBUG_AT( IFusionDale_One, "  -> CREATE_COMPONENT\n" );

                         if (size < sizeof(CreateComponentRequest)) {
                              D_WARN( "invalid packet, short call request" );
                              continue;
                         }

                         size -= sizeof(CreateComponentRequest);

                         D_DEBUG_AT( IFusionDale_One, "  -> notifications %u\n", create_component->notifications );

                         if (size < create_component->notifications * sizeof(OneQID)) {
                              D_WARN( "invalid packet, short notification QID array (remaining size %zu, array size %zu)",
                                      size, create_component->notifications * sizeof(OneQID) );
                              continue;
                         }

                         direct_mutex_lock( &data->ns_lock );

                         DispatchCreateComponent( data, create_component );

                         direct_mutex_unlock( &data->ns_lock );
                         break;

                    case NS_GET_COMPONENT:
                         D_DEBUG_AT( IFusionDale_One, "  -> GET_COMPONENT\n" );

                         if (size < sizeof(GetComponentRequest)) {
                              D_WARN( "invalid packet, short call request" );
                              continue;
                         }

                         size -= sizeof(GetComponentRequest);

                         direct_mutex_lock( &data->ns_lock );

                         DispatchGetComponent( data, get_component );

                         direct_mutex_unlock( &data->ns_lock );
                         break;

                    case NS_PING:
                         D_DEBUG_AT( IFusionDale_One, "  -> PING\n" );

                         if (size < sizeof(PingRequest)) {
                              D_WARN( "invalid packet, short call request" );
                              continue;
                         }

                         size -= sizeof(PingRequest);

                         direct_mutex_lock( &data->ns_lock );

                         DispatchPing( data, ping );

                         direct_mutex_unlock( &data->ns_lock );
                         break;

                    case NS_REGISTER_EVENT:
                         D_DEBUG_AT( IFusionDale_One, "  -> REGISTER_EVENT\n" );

                         if (size < sizeof(RegisterEventRequest)) {
                              D_WARN( "invalid packet, short call request" );
                              continue;
                         }

                         size -= sizeof(RegisterEventRequest);

                         direct_mutex_lock( &data->ns_lock );

                         DispatchRegisterEvent( data, register_event );

                         direct_mutex_unlock( &data->ns_lock );
                         break;

                    case NS_UNREGISTER_EVENT:
                         D_DEBUG_AT( IFusionDale_One, "  -> UNREGISTER_EVENT\n" );

                         if (size < sizeof(UnregisterEventRequest)) {
                              D_WARN( "invalid packet, short call request" );
                              continue;
                         }

                         size -= sizeof(UnregisterEventRequest);

                         direct_mutex_lock( &data->ns_lock );

                         DispatchUnregisterEvent( data, unregister_event );

                         direct_mutex_unlock( &data->ns_lock );
                         break;

                    case NS_GET_EVENT:
                         D_DEBUG_AT( IFusionDale_One, "  -> GET_EVENT\n" );

                         if (size < sizeof(GetEventRequest)) {
                              D_WARN( "invalid packet, short call request" );
                              continue;
                         }

                         size -= sizeof(GetEventRequest);

                         direct_mutex_lock( &data->ns_lock );

                         DispatchGetEvent( data, get_event );

                         direct_mutex_unlock( &data->ns_lock );
                         break;
                    default:
                         D_WARN( "unknown request type %d", *type );
               }
          }
     }

     return NULL;
}

static DirectResult
InitialiseNS( IFusionDale *thiz,
              const char  *host )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDale_One)

     D_DEBUG_AT( IFusionDale_One, "%s( '%s' )\n", __FUNCTION__, host );

     ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, 0, &data->ns_qid );
     if (ret)
          return ret;

     OneQueue_SetName( data->ns_qid, "Coma/NS" );

     D_DEBUG_AT( IFusionDale_One, "  -> QID %u\n", data->ns_qid );

     D_INFO( "ComaOne/NameService: QID %u\n", data->ns_qid );

     ret = direct_map_create( 7, ns_map_compare, ns_map_hash, data, &data->ns_components_map );
     if (ret) {
          OneQueue_Destroy( data->ns_qid );
          return ret;
     }

     ret = direct_map_create( 7, event_map_compare, event_map_hash, data, &data->ns_events_map );
     if (ret) {
          OneQueue_Destroy( data->ns_qid );
          return ret;
     }

     direct_mutex_init( &data->ns_lock );
     direct_waitqueue_init( &data->ns_wq );

     data->ns_thread = direct_thread_create( DTT_DEFAULT, DispatchNS, data, "FD/One/NS" );

     return DR_OK;
}

static DirectResult
ShutdownNS( IFusionDale *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionDale_One)

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     if (data->ns_thread) {
          direct_mutex_lock( &data->ns_lock );

          data->ns_stop = true;

          direct_waitqueue_broadcast( &data->ns_wq );

          direct_mutex_unlock( &data->ns_lock );


          OneQueue_WakeUp( &data->ns_qid, 1 );

          direct_thread_join( data->ns_thread );
          direct_thread_destroy( data->ns_thread );

          OneQueue_Destroy( data->ns_qid );

          while (data->ns_getcomponent_calls) {
               D_DEBUG_AT( IFusionDale_One, "  -> waiting for decoupled NSGetComponent thread...\n" );

               direct_thread_sleep( 200000 );
          }

          direct_map_iterate( data->ns_components_map, ns_iterator, data );
          direct_map_destroy( data->ns_components_map );

          direct_map_iterate( data->ns_events_map, event_iterator, data );
          direct_map_destroy( data->ns_events_map );

          direct_mutex_deinit( &data->ns_lock );
          direct_waitqueue_deinit( &data->ns_wq );
     }

     return DR_OK;
}

/**************************************************************************************************/

static DirectResult
WaitForNS( IFusionDale *thiz )
{
     DirectResult ret;
     PingRequest  request;
     unsigned int loops = 100;

     DIRECT_INTERFACE_GET_DATA(IFusionDale_One)

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, ONE_QID_NONE, &request.response_qid );
     if (ret)
          return ret;

     OneQueue_SetName( request.response_qid, "Coma/Messenger/NS Waiter" );

     /* Wait til NS is up and running... */
     while (loops--) {
          NSRequestType  type = NS_PING;
          PingResponse   response;
          size_t         length;
          void          *datas[2];
          size_t         lengths[2];

          request.stamp_us = direct_clock_get_abs_micros();

          D_DEBUG_AT( IFusionDale_One, "  -> sending PING to QID %u...\n", data->ns_qid );

          datas[0]   = &type;
          lengths[0] = sizeof(type);

          datas[1]   = &request;
          lengths[1] = sizeof(request);

          ret = OneQueue_DispatchV( data->ns_qid, datas, lengths, 2 );
          if (ret) {
               switch (ret) {
                    case DR_INVARG:
                    case DR_OK:
                         break;

                    default:
                         D_DERROR( ret, "FD/One/NS: Cannot ping name service!\n" );
                         goto out;
               }
          }

          ret = OneQueue_Receive( &request.response_qid, 1, &response, sizeof(response), &length, true, 500 );
          if (ret) {
               switch (ret) {
                    case DR_TIMEOUT:
                         break;

                    case DR_OK:
                         D_DEBUG_AT( IFusionDale_One, "  -> got PING response!\n" );
                         goto out;

                    default:
                         D_DERROR( ret, "FD/One/NS: Cannot receive ping response!\n" );
                         goto out;
               }
          }
     }


out:
     OneQueue_Destroy( request.response_qid );

     return ret;
}

/**********************************************************************************************************************/

static void
tlshm_destroy( void *arg )
{
     FusionDaleTLS *fusiondale_tls = arg;

     if (fusiondale_tls->local)
          D_FREE( fusiondale_tls->local );

     D_FREE( fusiondale_tls );
}

/**************************************************************************************************/

static DirectResult
Probe( void )
{
     /* This implementation has to be loaded explicitly. */
     return DR_UNSUPPORTED;
}

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
static DirectResult
Construct( IFusionDale *thiz, const char *host, int session )
{
     DirectResult ret;
     bool         old_secure;

     D_DEBUG_AT( IFusionDale_One, "%s()\n", __FUNCTION__ );

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionDale_One)

     /* Create the core instance. */
     old_secure = fusion_config->secure_fusion;
     fusion_config->secure_fusion = false;
     ret = fd_core_create( &data->core );
     fusion_config->secure_fusion = old_secure;
     if (ret) {
          FusionDaleError( "FusionDale: fd_core_create() failed", ret );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ret = One_Initialize();
     if (ret) {
          IFusionDale_One_Release( thiz );
          return ret;
     }

     if (data->core->master) {
          ret = InitialiseNS( thiz, host );
          if (ret == DR_OK)
               data->core->shared->qid = data->ns_qid;
     }
     else {
          data->ns_qid = data->core->shared->qid;

          ret = WaitForNS( thiz );
     }

     if (ret) {
          IFusionDale_One_Release( thiz );
          return ret;
     }

     ret = OneThread_Create( "Coma", &data->thread );
     if (ret) {
          IFusionDale_One_Release( thiz );
          return ret;
     }

     D_DEBUG_AT( IFusionDale_One, "  -> thread created\n" );

     direct_tls_register( &data->tlshm_key, tlshm_destroy );

     D_DEBUG_AT( IFusionDale_One, "  -> TLS registered\n" );

     thiz->AddRef          = IFusionDale_One_AddRef;
     thiz->Release         = IFusionDale_One_Release;
     thiz->EnterComa       = IFusionDale_One_EnterComa;
     thiz->CreateMessenger = IFusionDale_One_CreateMessenger;
     thiz->GetMessenger    = IFusionDale_One_GetMessenger;

     D_DEBUG_AT( IFusionDale_One, "  -> returning\n" );

     return DR_OK;
}

