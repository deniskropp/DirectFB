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

#include <config.h>

#include <fusiondale.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/interface.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <coma/coma.h>

#include "ifusiondale_one.h"
#include "icoma_one.h"
#include "icomacomponent_one.h"


static DirectResult Probe( void );
static DirectResult Construct( IComaComponent   *thiz,
                               IComa            *coma,
                               const char       *name,
                               OneQID            method_qid,
                               ComaMethodFunc    method_func,
                               void             *method_ctx,
                               const OneQID     *notification_qids,
                               unsigned int      num_notifications,
                               OneThread        *thread );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IComaComponent, One )


D_DEBUG_DOMAIN( IComaComponent_One, "IComaComponent/One", "IComaComponent One" );

/**************************************************************************************************/

typedef struct {
     int                    magic;

     OneQID                 response_qid;

     void                  *data;
     unsigned int           length;
} CallData;

typedef struct {
     ComaListenerFunc       func;       /* 'Notification Received' callback */
     void                  *ctx;        /* Optional context pointer for callback */
} Listener;

typedef struct {
     OneQID                 qid;

     ComaNotifyFunc         func;       /* Optional 'Notification Dispatched' callback */
     void                  *ctx;        /* Optional context pointer for callback */

     ComaNotificationFlags  flags;      /* Notification flags */
} Notification;

/*
 * private data struct of IComaComponent_One
 */
typedef struct {
     int                  ref;      /* reference counter */

     IComa               *coma;

     char                 name[FUSIONDALE_NAME_LENGTH];

     OneQID               method_qid;
     ComaMethodFunc       method_func;
     void                *method_ctx;

     OneQID               notify_qid;

     DirectThread        *dispatch_thread;
     bool                 dispatch_stop;

     DirectThread        *notify_thread;
     bool                 notify_stop;

     DirectMutex          calls_lock;
     DirectHash          *calls;
     unsigned int         serials;

     unsigned int         num_notifications;

     Listener            *listeners;
     Notification        *notifications;

     OneThread           *thread;
} IComaComponent_One_data;

/**************************************************************************************************/

static bool
call_iterator( DirectHash    *hash,
               unsigned long  key,
               void          *value,
               void          *ctx )
{
     CallData *call = value;

     D_MAGIC_ASSERT( call, CallData );

     D_WARN( "freeing pending call %p (response QID 0x%08x, data %p, length %u)",
             call, call->response_qid, call->data, call->length );

     D_MAGIC_CLEAR( call );

     D_FREE( call );

     return true;
}

static void
IComaComponent_One_Destruct( IComaComponent *thiz )
{
     int                      i;
     IComaComponent_One_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     /* If method_func is non-NULL, we're the creator */
     if (data->method_func) {
          if (data->dispatch_thread) {
               data->dispatch_stop = true;

               OneQueue_WakeUp( &data->method_qid, 1 );

               direct_thread_join( data->dispatch_thread );
               direct_thread_destroy( data->dispatch_thread );
          }

          OneQueue_Destroy( data->method_qid );

          for (i=0; i<data->num_notifications; i--)
               OneQueue_Destroy( data->notifications[i].qid );
     }


     data->notify_stop = true;

     OneQueue_WakeUp( &data->notify_qid, 1 );

     if (data->notify_thread) {
          direct_thread_join( data->notify_thread );
          direct_thread_destroy( data->notify_thread );
     }

     OneQueue_Destroy( data->notify_qid );


     direct_hash_iterate( data->calls, call_iterator, data );
     direct_hash_destroy( data->calls );
     direct_mutex_deinit( &data->calls_lock );

     if (data->notifications)
          D_FREE( data->notifications );

     if (data->listeners)
          D_FREE( data->listeners );

     One_Shutdown();

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IComaComponent_One_AddRef( IComaComponent *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     data->ref++;

     return DR_OK;
}

static DirectResult
IComaComponent_One_Release( IComaComponent *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     if (--data->ref == 0)
          IComaComponent_One_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IComaComponent_One_InitNotification( IComaComponent        *thiz,
                                     ComaNotificationID     id,
                                     ComaNotifyFunc         func,
                                     void                  *ctx,
                                     ComaNotificationFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     D_DEBUG_AT( IComaComponent_One, "%s( id %lu, func %p, ctx %p, flags 0x%08x )\n", __FUNCTION__, id, func, ctx, flags );

     if (id >= data->num_notifications)
          return DR_INVARG;

     data->notifications[id].func  = func;
     data->notifications[id].ctx   = ctx;
     data->notifications[id].flags = flags;

     return DR_OK;
}

static DirectResult
IComaComponent_One_InitNotifications( IComaComponent             *thiz,
                                      const ComaNotificationInit *inits,
                                      int                         num_inits,
                                      void                       *ctx )
{
     int i;

     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     D_DEBUG_AT( IComaComponent_One, "%s( inits %p, num %d, ctx %p )\n", __FUNCTION__, inits, num_inits, ctx );

     for (i=0; i<num_inits; i++) {
          D_DEBUG_AT( IComaComponent_One, "  -> id %lu, func %p, ctx %p, flags 0x%08x\n",
                      inits[i].id, inits[i].func, inits[i].ctx, inits[i].flags );

          if (inits[i].id >= data->num_notifications)
               return DR_INVARG;

          data->notifications[inits[i].id].func  = inits[i].func;
          data->notifications[inits[i].id].ctx   = inits[i].ctx ? inits[i].ctx : ctx;
          data->notifications[inits[i].id].flags = inits[i].flags;
     }

     return DR_OK;
}

static DirectResult
IComaComponent_One_Call( IComaComponent  *thiz,
                         ComaMethodID     method,
                         void            *arg,
                         int             *ret_val )
{
     DirectResult               ret;
     void                      *datas[3];
     size_t                     lengths[3];
     ComaComponentRequestType   type;
     ComaComponentCallRequest   request;
     size_t                     length;
     IComa_One_data            *coma_data;
     char                       static_buf[3000];
     ComaComponentCallResponse *response = (ComaComponentCallResponse*) static_buf;

     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     DIRECT_INTERFACE_GET_DATA_FROM( data->coma, coma_data, IComa_One );

     D_DEBUG_AT( IComaComponent_One, "%s( %s, method %lu, %p )\n", __FUNCTION__, data->name, method, arg );

     ret = FusionDaleTLS_GetResponseQID( coma_data->dale, &request.response_qid );
     if (ret)
          return ret;

     request.method_id = method;

     if (arg) {
          FusionDaleTLS *fusiondale_tls;

          ret = FusionDaleTLS_Get( coma_data->dale, &fusiondale_tls );
          if (ret)
               return ret;

          if (!fusiondale_tls)
               return DR_BUG;

          if (fusiondale_tls->local != arg)
               return DR_INVARG;

          request.length = fusiondale_tls->length;
     }
     else
          request.length = 0;

     D_DEBUG_AT( IComaComponent_One, "  -> length %u\n", request.length );

     if (sizeof(ComaComponentCallResponse) + request.length > sizeof(static_buf)) {
          response = D_MALLOC( sizeof(ComaComponentCallResponse) + request.length );
          if (!response)
               return D_OOM();
     }

     type = COMA_COMPONENT_CALL;

     datas[0]   = &type;
     lengths[0] = sizeof(type);

     datas[1]   = &request;
     lengths[1] = sizeof(request);

     datas[2]   = arg;
     lengths[2] = request.length;

     D_DEBUG_AT( IComaComponent_One, "  -> calling...\n" );

     ret = OneQueue_DispatchV( data->method_qid, datas, lengths, request.length ? 3 : 2 );
     if (ret)
          goto error;

     D_DEBUG_AT( IComaComponent_One, "  -> dispatched, receiving response...\n" );

     ret = OneQueue_Receive( &request.response_qid, 1, response, sizeof(ComaComponentCallResponse) + request.length, &length, true, 0 );
     if (ret)
          goto error;

     D_DEBUG_AT( IComaComponent_One, "  -> received %zu bytes\n", length );

     if (length < sizeof(ComaComponentCallResponse)) {
          D_WARN( "invalid response" );
          ret = DR_IO;
          goto error;
     }

     D_DEBUG_AT( IComaComponent_One, "  -> result is '%s'\n", DirectResultString( response->result ) );

     ret = response->result;
     if (ret)
          goto error;

     D_DEBUG_AT( IComaComponent_One, "  -> ret_val is %d\n", response->ret_val );

     if (length < sizeof(ComaComponentCallResponse) + request.length) {
          D_WARN( "short response" );
          ret = DR_INCOMPLETE;
          goto error;
     }

     if (arg)
          direct_memcpy( arg, response + 1, request.length );

     if (ret_val)
          *ret_val = response->ret_val;


error:
     if (response != (ComaComponentCallResponse*) static_buf)
          D_FREE( response );

     return ret;
}

static DirectResult
IComaComponent_One_Return( IComaComponent *thiz,
                           int             val,
                           unsigned int    serial )
{
     DirectResult               ret;
     CallData                  *call;
     ComaComponentCallResponse  response;
     void                      *datas[2];
     size_t                     lengths[2];

     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     D_DEBUG_AT( IComaComponent_One, "%s( val %d, serial %u )\n", __FUNCTION__, val, serial );

     direct_mutex_lock( &data->calls_lock );
     call = direct_hash_lookup( data->calls, serial );
     direct_mutex_unlock( &data->calls_lock );
     if (!call)
          return DR_INVARG;

     D_MAGIC_ASSERT( call, CallData );

     response.result  = DR_OK;
     response.ret_val = val;

     datas[0]   = &response;
     lengths[0] = sizeof(response);

     datas[1]   = call->data;
     lengths[1] = call->length;

     ret = OneQueue_DispatchV( call->response_qid, datas, lengths, call->length ? 2 : 1 );

     direct_mutex_lock( &data->calls_lock );
     direct_hash_remove( data->calls, serial );
     direct_mutex_unlock( &data->calls_lock );

     D_MAGIC_CLEAR( call );

     D_FREE( call );

     return ret;
}

static DirectResult
IComaComponent_One_Notify( IComaComponent     *thiz,
                           ComaNotificationID  id,
                           void               *arg )
{
     DirectResult    ret;
     ComaAllocation *allocation = NULL;
     unsigned int    length = 0;
     void           *datas[3];
     size_t          lengths[3];

     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     D_DEBUG_AT( IComaComponent_One, "%s( id %lu, arg %p )\n", __FUNCTION__, id, arg );

     if (id >= data->num_notifications)
          return DR_INVARG;

     if (arg) {
          allocation = arg - sizeof(ComaAllocation);

          if (allocation->magic != D_MAGIC( "ComaAllocation" ))
               return DR_INVARG;

          length = allocation->length;

          D_DEBUG_AT( IComaComponent_One, "  -> length %u\n", length );
     }

     datas[0]   = &id;
     lengths[0] = sizeof(ComaNotificationID);

     datas[1]   = &length;
     lengths[1] = sizeof(length);

     datas[2]   = arg;
     lengths[2] = length;

     D_DEBUG_AT( IComaComponent_One, "  -> dispatch (QID 0x%08x)...\n", data->notifications[id].qid );

     ret = OneQueue_DispatchV( data->notifications[id].qid, datas, lengths, length ? 3 : 2 );
     if (ret)
          return ret;

     if (data->notifications[id].func)
          data->notifications[id].func( data->notifications[id].ctx, id, arg );

     if (data->notifications[id].flags & CNF_DEALLOC_ARG)
          data->coma->Deallocate( data->coma, arg );

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
IComaComponent_One_Listen( IComaComponent     *thiz,
                           ComaNotificationID  id,
                           ComaListenerFunc    func,
                           void               *ctx )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     D_DEBUG_AT( IComaComponent_One, "%s( id %lu, func %p, ctx %p )\n", __FUNCTION__, id, func, ctx );

     if (id >= data->num_notifications)
          return DR_INVARG;

     if (data->listeners[id].func)
          return DR_BUSY;

     ret = OneQueue_Attach( data->notifications[id].qid, data->notify_qid );
     if (ret)
          return ret;

     data->listeners[id].func = func;
     data->listeners[id].ctx  = ctx;

     return DR_OK;
}

static DirectResult
IComaComponent_One_InitListeners( IComaComponent         *thiz,
                                  const ComaListenerInit *inits,
                                  int                     num_inits,
                                  void                   *ctx )
{
     DirectResult ret;
     int          i;

     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     D_DEBUG_AT( IComaComponent_One, "%s( inits %p, num %d, ctx %p )\n", __FUNCTION__, inits, num_inits, ctx );

     for (i=0; i<num_inits; i++) {
          D_DEBUG_AT( IComaComponent_One, "  -> id %lu, func %p, ctx %p\n", inits[i].id, inits[i].func, inits[i].ctx );

          if (inits[i].id >= data->num_notifications)
               return DR_INVARG;

          if (data->listeners[inits[i].id].func)
               return DR_BUSY;

          ret = OneQueue_Attach( data->notifications[inits[i].id].qid, data->notify_qid );
          if (ret)
               return ret;

          data->listeners[inits[i].id].func = inits[i].func;
          data->listeners[inits[i].id].ctx  = inits[i].ctx;
     }

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
IComaComponent_One_Unlisten( IComaComponent     *thiz,
                             ComaNotificationID  id )
{
     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     D_DEBUG_AT( IComaComponent_One, "%s( id %lu )\n", __FUNCTION__, id );

     if (id >= data->num_notifications)
          return DR_INVARG;

     if (!data->listeners[id].func)
          return DR_INVARG;

     OneQueue_Detach( data->notifications[id].qid, data->notify_qid );

     data->listeners[id].func = NULL;
     data->listeners[id].ctx  = NULL;

     return DR_OK;
}

/**********************************************************************************************************************/

static void
DispatchComponent( void                  *context,
                   const OnePacketHeader *header,
                   void                  *_data,
                   OneThread             *thread );

static void *
DispatchComponentThread( DirectThread *thread,
                         void         *arg );

static DirectResult
IComaComponent_One_Activate( IComaComponent *thiz )
{
     char buf[FUSIONDALE_NAME_LENGTH+100];

     DIRECT_INTERFACE_GET_DATA(IComaComponent_One)

     D_DEBUG_AT( IComaComponent_One, "%s()\n", __FUNCTION__ );

     if (!data->method_func)
          return DR_ACCESSDENIED;

     if (data->dispatch_thread)
          return DR_BUSY;

     direct_snprintf( buf, sizeof(buf), "%s/D", data->name );

     if (data->thread)
          OneThread_AddQueue( data->thread, data->method_qid, DispatchComponent, data );
     else
          data->dispatch_thread = direct_thread_create( DTT_DEFAULT, DispatchComponentThread, data, buf );

     return DR_OK;
}

/**********************************************************************************************************************/

#define RECEIVE_BUFFER_SIZE   65536

/**********************************************************************************************************************/

static DirectResult
DispatchCall( IComaComponent_One_data  *data,
              ComaComponentCallRequest *request )
{
     CallData     *call;
     unsigned int  serial;

     D_DEBUG_AT( IComaComponent_One, "%s( %s, method %lu )\n", __FUNCTION__, data->name, request->method_id );

     call = D_MALLOC( sizeof(CallData) + request->length );
     if (!call)
          return D_OOM();

     call->response_qid = request->response_qid;
     call->data         = call + 1;
     call->length       = request->length;

     direct_memcpy( call->data, request + 1, request->length );

     direct_mutex_lock( &data->calls_lock );

     do {
          serial = data->serials++;
     } while (direct_hash_lookup( data->calls, serial ));

     D_DEBUG_AT( IComaComponent_One, "  -> serial       %u\n", serial );

     D_MAGIC_SET( call, CallData );

     direct_hash_insert( data->calls, serial, call );

     direct_mutex_unlock( &data->calls_lock );

     data->method_func( data->method_ctx, request->method_id, call->data, serial );

     return DR_OK;
}

static void
DispatchComponent( void                  *context,
                   const OnePacketHeader *header,
                   void                  *_data,
                   OneThread             *thread )
{
     IComaComponent_One_data  *data = context;
     size_t                    size = header->uncompressed;
     ComaComponentRequestType *type = (ComaComponentRequestType *)( _data );
     ComaComponentCallRequest *call = (ComaComponentCallRequest *)( type + 1 );

     D_DEBUG_AT( IComaComponent_One, "%s()\n", __FUNCTION__ );
     D_DEBUG_AT( IComaComponent_One, "  -> size %zu\n", size );

     if (size < sizeof(ComaComponentRequestType)) {
          D_WARN( "invalid packet, no type" );
          return;
     }

     size -= sizeof(ComaComponentRequestType);

     switch (*type) {
          case COMA_COMPONENT_CALL:
               D_DEBUG_AT( IComaComponent_One, "  -> CALL\n" );

               if (size < sizeof(ComaComponentCallRequest)) {
                    D_WARN( "invalid packet, short call request" );
                    return;
               }

               size -= sizeof(ComaComponentCallRequest);

               D_DEBUG_AT( IComaComponent_One, "  -> method      %lu\n", call->method_id );
               D_DEBUG_AT( IComaComponent_One, "  -> length      %u\n", call->length );

               if (size < call->length) {
                    D_WARN( "invalid packet, short call data (remaining size %zu, data length %u)", size, call->length );
                    return;
               }

               DispatchCall( data, call );
               break;

          default:
               D_WARN( "unknown request type %d", *type );
     }
}

static void *
DispatchComponentThread( DirectThread *thread,
                         void         *arg )
{
     DirectResult             ret;
     IComaComponent_One_data *data = arg;
     char                    *buf;

     D_DEBUG_AT( IComaComponent_One, "%s()\n", __FUNCTION__ );

     buf = D_MALLOC( RECEIVE_BUFFER_SIZE );
     if (!buf) {
          D_OOM();
          return NULL;
     }

     while (!data->dispatch_stop) {
          size_t length;
          size_t offset;

          ret = OneQueue_Receive( &data->method_qid, 1, buf, RECEIVE_BUFFER_SIZE, &length, false, 0 );
          if (ret) {
               D_DERROR( ret, "IComaComponent/One: Could not receive from Component Queue!\n" );
               break;
          }

          D_DEBUG_AT( IComaComponent_One, "%s() -> received %zu bytes\n", __FUNCTION__, length );

          for (offset=0; offset < length; ) {
               OnePacketHeader          *header = (OnePacketHeader *)(buf + offset);
               size_t                    size   = header->uncompressed;
               ComaComponentRequestType *type   = (ComaComponentRequestType *)( header + 1 );
               ComaComponentCallRequest *call   = (ComaComponentCallRequest *)( type + 1 );

               D_DEBUG_AT( IComaComponent_One, "  -> size %zu\n", size );

               offset += sizeof(OnePacketHeader) + size;

               if (offset > length) {
                    D_WARN( "invalid packet (offset %zu, length %zu)", offset, length );
                    continue;
               }


               if (size < sizeof(ComaComponentRequestType)) {
                    D_WARN( "invalid packet, no type" );
                    continue;
               }

               size -= sizeof(ComaComponentRequestType);

               switch (*type) {
                    case COMA_COMPONENT_CALL:
                         D_DEBUG_AT( IComaComponent_One, "  -> CALL\n" );

                         if (size < sizeof(ComaComponentCallRequest)) {
                              D_WARN( "invalid packet, short call request" );
                              continue;
                         }

                         size -= sizeof(ComaComponentCallRequest);

                         D_DEBUG_AT( IComaComponent_One, "  -> method      %lu\n", call->method_id );
                         D_DEBUG_AT( IComaComponent_One, "  -> length      %u\n", call->length );

                         if (size < call->length) {
                              D_WARN( "invalid packet, short call data (remaining size %zu, data length %u)", size, call->length );
                              continue;
                         }

                         DispatchCall( data, call );
                         break;

                    default:
                         D_WARN( "unknown request type %d", *type );
               }
          }
     }

     D_FREE( buf );

     return NULL;
}

/**********************************************************************************************************************/

static DirectResult
DispatchNotification( IComaComponent_One_data *data,
                      ComaNotificationID       id,
                      void                    *arg )
{
     D_DEBUG_AT( IComaComponent_One, "%s( id %lu, arg %p )\n", __FUNCTION__, id, arg );

     if (data->listeners[id].func)
          data->listeners[id].func( data->listeners[id].ctx, arg );
     else
          D_DEBUG_AT( IComaComponent_One, "  -> NO LISTENER FUNCTION\n" );

     return DR_OK;
}

static void
DispatchNotify( void                  *context,
                const OnePacketHeader *header,
                void                  *_data,
                OneThread             *thread )
{
     IComaComponent_One_data *data = context;
     size_t                   size = header->uncompressed;
     ComaNotificationID      *id   = (ComaNotificationID *)( _data );
     unsigned int            *len  = (unsigned int *)( id + 1 );
     IComa_One_data          *coma_data;

     D_MAGIC_ASSERT( (IAny*)data->coma, DirectInterface );

     coma_data = (IComa_One_data*) data->coma->priv;

     D_DEBUG_AT( IComaComponent_One, "%s()\n", __FUNCTION__ );
     D_DEBUG_AT( IComaComponent_One, "  -> size %zu\n", size );

     if (size < sizeof(ComaNotificationID)) {
          D_WARN( "invalid packet, no id" );
          return;
     }

     size -= sizeof(ComaNotificationID);


     if (size < sizeof(unsigned int)) {
          D_WARN( "invalid packet, no length" );
          return;
     }

     size -= sizeof(unsigned int);


     D_DEBUG_AT( IComaComponent_One, "  -> notification %lu\n", *id );
     D_DEBUG_AT( IComaComponent_One, "  -> length       %u\n", *len );

     if (size < *len) {
          D_WARN( "invalid packet, short notification data (remaining size %zu, data length %u)", size, *len );
          return;
     }

     FusionDaleTLS_SetNotificationLength( coma_data->dale, *len );

     DispatchNotification( data, *id, *len ? len + 1 : NULL );
}

static void *
DispatchNotifyThread( DirectThread *thread,
                      void         *arg )
{
     DirectResult             ret;
     IComaComponent_One_data *data = arg;
     char                    *buf;

     D_DEBUG_AT( IComaComponent_One, "%s()\n", __FUNCTION__ );

     buf = D_MALLOC( RECEIVE_BUFFER_SIZE );
     if (!buf) {
          D_OOM();
          return NULL;
     }

     while (!data->notify_stop) {
          size_t length;
          size_t offset;

          ret = OneQueue_Receive( &data->notify_qid, 1, buf, RECEIVE_BUFFER_SIZE, &length, false, 0 );
          if (ret) {
               D_DERROR( ret, "IComaComponent/One: Could not receive from Component Queue!\n" );
               break;
          }

          D_DEBUG_AT( IComaComponent_One, "%s() -> received %zu bytes\n", __FUNCTION__, length );

          for (offset=0; offset < length; ) {
               OnePacketHeader          *header = (OnePacketHeader *)(buf + offset);
               size_t                    size   = header->uncompressed;
               ComaNotificationID       *id     = (ComaNotificationID *)( header + 1 );
               unsigned int             *len    = (unsigned int *)( id + 1 );

               D_DEBUG_AT( IComaComponent_One, "  -> size %zu\n", size );

               offset += sizeof(OnePacketHeader) + size;

               if (offset > length) {
                    D_WARN( "invalid packet (offset %zu, length %zu)", offset, length );
                    continue;
               }


               if (size < sizeof(ComaNotificationID)) {
                    D_WARN( "invalid packet, no id" );
                    continue;
               }

               size -= sizeof(ComaNotificationID);


               if (size < sizeof(unsigned int)) {
                    D_WARN( "invalid packet, no length" );
                    continue;
               }

               size -= sizeof(unsigned int);


               D_DEBUG_AT( IComaComponent_One, "  -> notification %lu\n", *id );
               D_DEBUG_AT( IComaComponent_One, "  -> length       %u\n", *len );

               if (size < *len) {
                    D_WARN( "invalid packet, short notification data (remaining size %zu, data length %u)", size, *len );
                    continue;
               }

               DispatchNotification( data, *id, *len ? len + 1 : NULL );
          }
     }

     D_FREE( buf );

     return NULL;
}

/**********************************************************************************************************************/

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
Construct( IComaComponent   *thiz,
           IComa            *coma,
           const char       *name,
           OneQID            method_qid,
           ComaMethodFunc    method_func,
           void             *method_ctx,
           const OneQID     *notification_qids,
           unsigned int      num_notifications,
           OneThread        *thread )
{
     DirectResult ret;
     unsigned int i;
     char         buf[FUSIONDALE_NAME_LENGTH+100];

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IComaComponent_One)

     D_DEBUG_AT( IComaComponent_One, "%s( '%s' )\n", __FUNCTION__, name );

     ret = One_Initialize();
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref               = 1;
     data->coma              = coma;
     data->method_qid        = method_qid;
     data->method_func       = method_func;
     data->method_ctx        = method_ctx;
     data->num_notifications = num_notifications;
     data->thread            = thread;

     if (num_notifications) {
          data->listeners = D_CALLOC( num_notifications, sizeof(Listener) );
          if (!data->listeners) {
               ret = D_OOM();
               goto error;
          }

          data->notifications = D_CALLOC( num_notifications, sizeof(Notification) );
          if (!data->notifications) {
               ret = D_OOM();
               goto error;
          }

          for (i=0; i<num_notifications; i++)
               data->notifications[i].qid = notification_qids[i];
     }

     direct_snputs( data->name, name, FUSIONDALE_NAME_LENGTH );

     ret = OneQueue_New( ONE_QUEUE_NO_FLAGS, ONE_QID_NONE, &data->notify_qid );
     if (ret)
          goto error;

     direct_snprintf( buf, sizeof(buf), "%s/N", data->name );

     OneQueue_SetName( data->notify_qid, buf );

     direct_recursive_mutex_init( &data->calls_lock );
     direct_hash_create( 7, &data->calls );

     if (thread)
          OneThread_AddQueue( thread, data->notify_qid, DispatchNotify, data );
     else
          data->notify_thread = direct_thread_create( DTT_DEFAULT, DispatchNotifyThread, data, buf );

     thiz->AddRef            = IComaComponent_One_AddRef;
     thiz->Release           = IComaComponent_One_Release;
     thiz->InitNotification  = IComaComponent_One_InitNotification;
     thiz->InitNotifications = IComaComponent_One_InitNotifications;
     thiz->Call              = IComaComponent_One_Call;
     thiz->Return            = IComaComponent_One_Return;
     thiz->Notify            = IComaComponent_One_Notify;
     thiz->Listen            = IComaComponent_One_Listen;
     thiz->InitListeners     = IComaComponent_One_InitListeners;
     thiz->Unlisten          = IComaComponent_One_Unlisten;
     thiz->Activate          = IComaComponent_One_Activate;

     return DR_OK;


error:
     if (data->listeners)
          D_FREE( data->listeners );

     if (data->notifications)
          D_FREE( data->notifications );

     One_Shutdown();

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     return ret;
}

