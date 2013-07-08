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

#include <direct/interface.h>

#include <One/One.h>

#include "ifusiondale_one.h"
#include "icoma_one.h"
#include "ifusiondalemessenger_one.h"

static DirectResult Probe( void );
DirectResult Construct( IFusionDaleMessenger *thiz, IFusionDale *dale, OneThread *thread );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionDaleMessenger, One )

D_DEBUG_DOMAIN( IFusionDaleMessenger_One, "IFusionDaleMessenger/One", "IFusionDaleMessener One" );

/**************************************************************************************************/

static unsigned int curr_listener_id = 0x23;

typedef struct {
     char        name[FUSIONDALE_NAME_LENGTH];
     OneQID      qid;
     DirectLink *listeners;
     int         ref;
} EventMapEntry;

typedef struct {
     DirectLink                link;
     EventMapEntry            *event_entry;
     FDMessengerListenerID     id;
     FDMessengerEventCallback  callback;
     void                     *context;
} EventMapEntryItem;

static EventMapEntryItem*
event_map_entry_item_create( EventMapEntry               *event_entry,
                             const FDMessengerListenerID  id,
                             FDMessengerEventCallback     callback,
                             void                        *context )
{
     EventMapEntryItem *entry;

     entry = D_CALLOC( 1, sizeof(EventMapEntryItem) );
     if (!entry)
          return NULL;

     entry->id       = id;
     entry->callback = callback;
     entry->context  = context;

     return entry;
}

static bool
event_map_entry_item_compare( DirectMap  *map,
                              const void *key,
                              void       *object,
                              void       *ctx )
{
     const FDMessengerListenerID  map_key   = *(FDMessengerListenerID*)key;
     const EventMapEntryItem     *map_entry = object;

     return map_key == map_entry->id;
}

static unsigned int
event_map_entry_item_hash( DirectMap  *map,
                           const void *key,
                           void       *ctx )
{
     return *(FDMessengerListenerID*)key;
}

static DirectEnumerationResult
event_map_entry_item_free( DirectMap *map,
                           void      *object,
                           void      *ctx )
{
     EventMapEntryItem *entry = object;

     D_FREE( entry );

     return DENUM_OK;
}

static EventMapEntry*
event_map_entry_create( const char *name,
                        OneQID      qid )
{
     EventMapEntry *entry;

     entry = D_CALLOC( 1, sizeof(EventMapEntry) );
     if (!entry)
          return NULL;

     direct_snputs( entry->name, name, FUSIONDALE_NAME_LENGTH );

     entry->qid = qid;
     entry->ref = 1;

     return entry;
}

static bool
event_map_entry_compare_name( DirectMap  *map,
                              const void *key,
                              void       *object,
                              void       *ctx )
{
     const char    *map_key = key;
     EventMapEntry *map_entry = object;

     return strcmp( map_key, map_entry->name ) == 0;
}

static bool
event_map_entry_compare_id( DirectMap  *map,
                            const void *key,
                            void       *object,
                            void       *ctx )
{
     const OneQID   map_key   = *(OneQID*)key;
     EventMapEntry *map_entry = object;

     return map_key == map_entry->qid;
}

static unsigned int
event_map_entry_hash_name( DirectMap  *map,
                           const void *key,
                           void       *ctx )
{
     size_t        i       = 0;
     unsigned int  hash    = 0;
     const char   *map_key = key;

     while (map_key[i]) {
          hash = hash * 131 + map_key[i];

          i++;
     }

     return hash;
}

static unsigned int
event_map_entry_hash_id( DirectMap  *map,
                         const void *key,
                         void       *ctx )
{
     return *(OneQID*)key;
}

static DirectEnumerationResult
event_map_entry_free( DirectMap *map,
                      void      *object,
                      void      *ctx )
{
     EventMapEntry     *entry = object;
     EventMapEntryItem *listener;
     DirectLink        *next;

     direct_list_foreach_safe( listener, next, entry->listeners ) {
          event_map_entry_item_free( NULL, listener, NULL);
     }

     OneQueue_Destroy( entry->qid );

     D_FREE( entry );

     return DENUM_OK;
}

/**************************************************************************************************/

static void
IFusionDaleMessenger_One_Destruct( IFusionDaleMessenger *thiz )
{
     IFusionDaleMessenger_One_data *data = thiz->priv;

     direct_map_iterate( data->event_map_id, event_map_entry_free, data );
     direct_map_destroy( data->event_map_id );
     direct_map_destroy( data->event_map_name );
     direct_map_destroy( data->event_map_listeners );

     OneQueue_Destroy( data->event_qid );

     /* Deinitialize lock. */
     direct_mutex_deinit( &data->lock );

     One_Shutdown();

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IFusionDaleMessenger_One_AddRef( IFusionDaleMessenger *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionDaleMessenger_One)

     data->ref++;

     return DR_OK;
}

static DirectResult
IFusionDaleMessenger_One_Release( IFusionDaleMessenger *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionDaleMessenger_One)

     if (--data->ref == 0)
          IFusionDaleMessenger_One_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IFusionDaleMessenger_One_RegisterEvent( IFusionDaleMessenger *thiz,
                                        const char           *name,
                                        FDMessengerEventID   *ret_id )
{
     DirectResult           ret;
     IFusionDale_One_data  *dale_data;
     NSRequestType          type;
     void                  *datas[2];
     size_t                 lengths[2];
     EventMapEntry         *entry;
     RegisterEventRequest   request;
     RegisterEventResponse  response;
     OneQID                 tls_qid;
     size_t                 len;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger_One)

     DIRECT_INTERFACE_GET_DATA_FROM( data->dale, dale_data, IFusionDale_One );

     /* Check arguments. */
     if (!name || !ret_id)
          return DR_INVARG;

     /* Lock the messenger. */
     direct_mutex_lock( &data->lock );

     /* Try to lookup event by name. */
     entry = direct_map_lookup( data->event_map_name, name );
     if (entry) {
          entry->ref++;
          *ret_id = entry->qid;

          direct_mutex_unlock( &data->lock );

          return DR_OK;
     }

     /* Unlock the messenger. */
     direct_mutex_unlock( &data->lock );

     ret = FusionDaleTLS_GetResponseQID( data->dale, &tls_qid );
     if (ret) {
          D_DERROR( ret, "IFusionDaleMessenger/One: Could not get Queue from TLS!\n" );

          direct_mutex_unlock( &data->lock );

          return ret;
     }

     type = NS_REGISTER_EVENT;

     request.response_qid  = tls_qid;

     direct_snputs( request.name, name, FUSIONDALE_NAME_LENGTH );

     datas[0]   = &type;
     lengths[0] = sizeof(type);

     datas[1]   = &request;
     lengths[1] = sizeof(request);

     ret = OneQueue_DispatchV( dale_data->ns_qid, datas, lengths, 2 );
     if (ret) {
          D_DERROR( ret, "IFusionDaleMessenger/One: Could not send to Queue!\n" );

          return ret;
     }

     ret = OneQueue_Receive( &tls_qid, 1, &response, sizeof(response), &len, true, 0 );
     if (ret) {
          D_DERROR( ret, "IFusionDaleMessenger/One: Could not receive from Queue!\n" );

          return ret;
     }

     ret = OneQueue_Attach( response.event_qid, data->event_qid );
     if (ret)
          return ret;

     entry = event_map_entry_create( name, response.event_qid );
     if (!entry) {
          D_DERROR( ret, "IFusionDaleMessenger/One: Failed to create new Event entry!\n" );

          OneQueue_Detach( response.event_qid, data->event_qid );

          return DR_FAILURE;
     }

     /* Lock the messenger. */
     direct_mutex_lock( &data->lock );

     direct_map_insert( data->event_map_id, &response.event_qid, entry );
     direct_map_insert( data->event_map_name, name, entry );

     /* Return the event id. */
     *ret_id = response.event_qid;

     /* Unlock the messenger. */
     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionDaleMessenger_One_UnregisterEvent( IFusionDaleMessenger *thiz,
                                          FDMessengerEventID    event_id )
{
     DirectResult             ret;
     IFusionDale_One_data    *dale_data;
     NSRequestType          type;
     void                  *datas[2];
     size_t                 lengths[2];
     EventMapEntry           *entry;
     UnregisterEventRequest   request;
     UnregisterEventResponse  response;
     OneQID                   tls_qid;
     size_t                   len;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger_One)

     DIRECT_INTERFACE_GET_DATA_FROM( data->dale, dale_data, IFusionDale_One );

     /* Lock the messenger. */
     direct_mutex_lock( &data->lock );

     /* Try to lookup event by id. */
     entry = direct_map_lookup( data->event_map_id, &event_id );
     if (!entry) {
          direct_mutex_unlock( &data->lock );

          return DR_IDNOTFOUND;
     }

     entry->ref--;

     if (!entry->ref) {
          /* Unlock the messenger. */
          direct_mutex_unlock( &data->lock );

          OneQueue_Detach( entry->qid, data->event_qid );

          direct_map_remove( data->event_map_id, &event_id );
          direct_map_remove( data->event_map_name, entry->name );
          event_map_entry_free( NULL, entry, NULL );

          type = NS_UNREGISTER_EVENT;

          request.response_qid  = tls_qid;

          direct_snputs( request.name, entry->name, FUSIONDALE_NAME_LENGTH );

          datas[0]   = &type;
          lengths[0] = sizeof(type);

          datas[1]   = &request;
          lengths[1] = sizeof(request);

          ret = OneQueue_DispatchV( dale_data->ns_qid, datas, lengths, 2 );
          if (ret) {
               D_DERROR( ret, "IFusionDaleMessenger/One: Could not send to Queue!\n" );

               return ret;
          }
          
          ret = OneQueue_Receive( &tls_qid, 1, &response, sizeof(response), &len, false, 0 );
          if (ret) {
               D_DERROR( ret, "IFusionDaleMessenger/One: Could not receive from Queue!\n" );

               return ret;
          }

          return DR_OK;
     }

     /* Unlock the messenger. */
     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionDaleMessenger_One_IsEventRegistered( IFusionDaleMessenger *thiz,
                                            const char           *name )
{
     EventMapEntry *entry;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger_One)

     /* Check arguments */
     if (!name)
          return DR_INVARG;

     /* Lock the messenger. */
     direct_mutex_lock( &data->lock );

     /* Try to lookup event by name. */
     entry = direct_map_lookup( data->event_map_name, name );
     if (!entry) {
          direct_mutex_unlock( &data->lock );

          return DR_IDNOTFOUND;
     }

     /* Unlock the messenger. */
     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionDaleMessenger_One_RegisterListener( IFusionDaleMessenger     *thiz,
                                           FDMessengerEventID        event_id,
                                           FDMessengerEventCallback  callback,
                                           void                     *context,
                                           FDMessengerListenerID    *ret_id )
{
     EventMapEntry     *entry;
     EventMapEntryItem *item;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger_One)

     /* Check arguments */
     if (!callback || !ret_id)
          return DR_INVARG;

     /* Lock the messenger. */
     direct_mutex_lock( &data->lock );

     /* Try to lookup event by id. */
     entry = direct_map_lookup( data->event_map_id, &event_id );
     if (!entry) {
          direct_mutex_unlock( &data->lock );

          return DR_IDNOTFOUND;
     }

     item = event_map_entry_item_create( entry, curr_listener_id, callback, context );
     direct_map_insert( data->event_map_listeners, &curr_listener_id, item );
     direct_list_append( &entry->listeners, &item->link );

     *ret_id = curr_listener_id++;

     /* Unlock the messenger. */
     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionDaleMessenger_One_UnregisterListener( IFusionDaleMessenger  *thiz,
                                             FDMessengerListenerID  listener_id )
{
     EventMapEntryItem *item;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger_One)

     /* Lock the messenger. */
     direct_mutex_lock( &data->lock );

     item = direct_map_lookup( data->event_map_listeners, &listener_id );
     if (!item) {
          direct_mutex_unlock( &data->lock );

          return DR_IDNOTFOUND;
     }

     direct_map_remove( data->event_map_listeners, &listener_id );
     direct_list_remove( &item->event_entry->listeners, &item->link );

     event_map_entry_item_free( NULL, item, NULL );

     /* Unlock the messenger. */
     direct_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IFusionDaleMessenger_One_SendSimpleEvent( IFusionDaleMessenger *thiz,
                                          FDMessengerEventID    event_id,
                                          int                   param )
{
     DirectResult   ret;
     EventMapEntry *entry;
     void          *datas[2];
     size_t         lengths[2];

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger_One)

     /* Check arguments */
     if (!event_id)
          return DR_INVARG;

     /* Lock the messenger. */
     direct_mutex_lock( &data->lock );

     entry = direct_map_lookup( data->event_map_id, &event_id );
     if (!entry) {
          direct_mutex_unlock( &data->lock );

          return DR_IDNOTFOUND;
     }

     /* Unlock the messenger. */
     direct_mutex_unlock( &data->lock );

     datas[0]   = &event_id;
     lengths[0] = sizeof(FDMessengerEventID);

     datas[1]   = &param;
     lengths[1] = sizeof(int);

     ret = OneQueue_DispatchV( entry->qid, datas, lengths, 2 );

     return ret;
}

static DirectResult
IFusionDaleMessenger_One_SendEvent( IFusionDaleMessenger *thiz,
                                    FDMessengerEventID    event_id,
                                    int                   param,
                                    void                 *data_ptr,
                                    unsigned int          data_size )
{
     DirectResult   ret;
     EventMapEntry *entry;
     void          *datas[3];
     size_t         lengths[3];
     int            count = 2;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger_One)

     /* Check arguments */
     if (!event_id || !data_ptr || !data_size)
          return DR_INVARG;

     /* Lock the messenger. */
     direct_mutex_lock( &data->lock );

     entry = direct_map_lookup( data->event_map_id, &event_id );
     if (!entry) {
          direct_mutex_unlock( &data->lock );

          return DR_IDNOTFOUND;
     }

     /* Unlock the messenger. */
     direct_mutex_unlock( &data->lock );

     datas[0]   = &event_id;
     lengths[0] = sizeof(FDMessengerEventID);

     datas[1]   = &param;
     lengths[1] = sizeof(int);

     if (data_ptr && data_size > 0) {
          count++;

          datas[2]   = data_ptr;
          lengths[2] = data_size;
     }

     ret = OneQueue_DispatchV( entry->qid, datas, lengths, count );

     return ret;
}

static DirectResult
IFusionDaleMessenger_One_AllocateData( IFusionDaleMessenger  *thiz,
                                       unsigned int           data_size,
                                       void                 **ret_data )
{
     void *data_ptr = NULL;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger_One)

     /* Check arguments */
     if (!data_size || !ret_data)
          return DR_INVARG;

     /* Lock the messenger. */
     direct_mutex_lock( &data->lock );

     /* Unlock the messenger. */
     direct_mutex_unlock( &data->lock );

     if (!data_ptr)
          return DR_UNIMPLEMENTED;

     *ret_data = data_ptr;

     return DR_OK;
}

static void
IFusionMessenger_One_DispatchNotification( void                  *context,
                                           const OnePacketHeader *header,
                                           void                  *_data,
                                           OneThread             *thread )
{
     IFusionDaleMessenger_One_data *data     = context;
     size_t                         size     = header->uncompressed;
     char                          *cb_data  = _data;
     EventMapEntryItem             *listener;
     EventMapEntry                 *entry;
     FDMessengerEventID            *event_id;
     int                           *param;

     if (size < sizeof(FDMessengerEventID)) {
          D_WARN( "invalid packet, no id" );

          direct_mutex_unlock( &data->lock );

          return;
     }

     event_id  = (FDMessengerEventID*)(_data);
     size     -= sizeof(FDMessengerEventID);
     cb_data  += sizeof(FDMessengerEventID);

     /* Lock the messenger. */
     direct_mutex_lock( &data->lock );

     entry = direct_map_lookup( data->event_map_id, event_id );
     if (!entry) {
          direct_mutex_unlock( &data->lock );

          return;
     }

     if (!entry->listeners) {
          direct_mutex_unlock( &data->lock );

          return;
     }

     if (size < sizeof(int)) {
          D_WARN( "invalid packet, no param" );

          direct_mutex_unlock( &data->lock );

          return;
     }

     param    = (int *)(event_id + 1);
     size    -= sizeof(int);
     cb_data += sizeof(int);

     if (size < 1)
          cb_data = NULL;
     direct_list_foreach( listener, entry->listeners ) {
          listener->callback( *event_id, *param, cb_data, size, listener->context );
     }

     /* Unlock the messenger. */
     direct_mutex_unlock( &data->lock );
}

/**************************************************************************************************/

static DirectResult
Probe( void )
{
     /* This implementation has to be loaded explicitly. */
     return DR_UNSUPPORTED;
}

DirectResult
Construct( IFusionDaleMessenger *thiz,
           IFusionDale          *dale,
           OneThread            *thread )
{
     DirectResult ret;

     /* Allocate interface data. */
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionDaleMessenger_One );

     ret = One_Initialize();
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Initialize interface data. */
     data->ref    = 1;
     data->dale   = dale;
     data->thread = thread;

     ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, ONE_QID_NONE, &data->event_qid );
     if (ret) {
          thiz->Release( thiz );
          return ret;
     }

     OneQueue_SetName( data->event_qid, "Messenger/Event" );

     ret = OneThread_AddQueue( thread, data->event_qid, IFusionMessenger_One_DispatchNotification, data );
     if (ret) {
          OneQueue_Destroy( data->event_qid );
          thiz->Release( thiz );
          return ret;
     }

     D_DEBUG_AT( IFusionDaleMessenger_One, "  -> QID %u\n", data->event_qid );

     D_INFO( "FusionDaleMessengerOne/Event: QID %u\n", data->event_qid );

     ret = direct_map_create( 7, event_map_entry_compare_id, event_map_entry_hash_id, data, &data->event_map_id );
     if (ret) {
          OneQueue_Destroy( data->event_qid );
          thiz->Release( thiz );
          return ret;
     }

     ret = direct_map_create( 7, event_map_entry_compare_name, event_map_entry_hash_name, data, &data->event_map_name );
     if (ret) {
          OneQueue_Destroy( data->event_qid );
          direct_map_destroy( data->event_map_id );
          thiz->Release( thiz );
          return ret;
     }

     ret = direct_map_create( 14, event_map_entry_item_compare, event_map_entry_item_hash, data, &data->event_map_listeners );
     if (ret) {
          OneQueue_Destroy( data->event_qid );
          direct_map_destroy( data->event_map_id );
          direct_map_destroy( data->event_map_name );
          thiz->Release( thiz );
          return ret;
     }

     /* Initialize lock. */
     direct_mutex_init( &data->lock );

     /* Assign interface pointers. */
     thiz->AddRef             = IFusionDaleMessenger_One_AddRef;
     thiz->Release            = IFusionDaleMessenger_One_Release;
     thiz->RegisterEvent      = IFusionDaleMessenger_One_RegisterEvent;
     thiz->UnregisterEvent    = IFusionDaleMessenger_One_UnregisterEvent;
     thiz->IsEventRegistered  = IFusionDaleMessenger_One_IsEventRegistered;
     thiz->RegisterListener   = IFusionDaleMessenger_One_RegisterListener;
     thiz->UnregisterListener = IFusionDaleMessenger_One_UnregisterListener;
     thiz->SendSimpleEvent    = IFusionDaleMessenger_One_SendSimpleEvent;
     thiz->SendEvent          = IFusionDaleMessenger_One_SendEvent;
     thiz->AllocateData       = IFusionDaleMessenger_One_AllocateData;

     return DR_OK;
}

