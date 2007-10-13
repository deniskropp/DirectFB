/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#include <fusiondale.h>

#include <direct/interface.h>
#include <direct/mem.h>

#include <fusion/shmalloc.h>

#include <coma/coma.h>
#include <coma/component.h>

#include "icomacomponent.h"

/**********************************************************************************************************************/

static ReactionResult IComaComponent_ListenerReaction( const void *msg_data,
                                                       void       *ctx );

/**********************************************************************************************************************/

static void
IComaComponent_Destruct( IComaComponent *thiz )
{
     int                  i;
     IComaComponent_data *data = thiz->priv;

     for (i=0; i<data->num_notifications; i++) {
          if (data->listeners[i].func)
               coma_component_detach( data->component, &data->listeners[i].reaction );
     }

     coma_component_unref( data->component );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IComaComponent_AddRef( IComaComponent *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IComaComponent)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IComaComponent_Release( IComaComponent *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IComaComponent)

     if (--data->ref == 0)
          IComaComponent_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IComaComponent_InitNotification( IComaComponent     *thiz,
                                 ComaNotificationID  id,
                                 ComaNotifyFunc      func,
                                 void               *ctx )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA (IComaComponent)

     coma_component_lock( data->component );

     ret = coma_component_init_notification( data->component, id, func, ctx );

     coma_component_unlock( data->component );

     return ret;
}

static DFBResult
IComaComponent_InitNotifications( IComaComponent             *thiz,
                                  const ComaNotificationInit *inits,
                                  int                         num_inits,
                                  void                       *ctx )
{
     int          i;
     DirectResult ret = DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA (IComaComponent)

     if (!inits || num_inits < 1)
          return DFB_INVARG;

     coma_component_lock( data->component );

     for (i=0; i<num_inits; i++) {
          ret = coma_component_init_notification( data->component, inits[i].id,
                                                  inits[i].func, inits[i].ctx ? : ctx );
          if (ret)
               break;
     }

     coma_component_unlock( data->component );

     return ret;
}

static DFBResult
IComaComponent_Call( IComaComponent *thiz,
                     ComaMethodID    method,
                     void           *arg,
                     int            *ret_val )
{
     DIRECT_INTERFACE_GET_DATA (IComaComponent)

     return coma_component_call( data->component, method, arg, ret_val );
}

static DFBResult
IComaComponent_Return( IComaComponent *thiz,
                       int             val,
                       unsigned int    magic )
{
     DIRECT_INTERFACE_GET_DATA (IComaComponent)

     return coma_component_return( data->component, magic, val );
}

static DFBResult
IComaComponent_Notify( IComaComponent     *thiz,
                       ComaNotificationID  id,
                       void               *arg )
{
     DIRECT_INTERFACE_GET_DATA (IComaComponent)

     if (id < 0 || id >= data->num_notifications)
          return DFB_LIMITEXCEEDED;

     return coma_component_notify( data->component, id, arg );
}

static DFBResult
IComaComponent_Listen( IComaComponent     *thiz,
                       ComaNotificationID  id,
                       ComaListenerFunc    func,
                       void               *ctx )
{
     DirectResult  ret;
     ComaListener *listener;

     DIRECT_INTERFACE_GET_DATA (IComaComponent)

     if (id < 0 || id >= data->num_notifications)
          return DFB_LIMITEXCEEDED;

     listener = &data->listeners[id];

     if (listener->func)
          return DFB_BUSY;

     ret = coma_component_attach_channel( data->component, id,
                                          IComaComponent_ListenerReaction,
                                          listener, &listener->reaction );
     if (ret)
          return ret;

     listener->func = func;
     listener->ctx  = ctx;

     return DFB_OK;
}

static DFBResult
IComaComponent_InitListeners( IComaComponent         *thiz,
                              const ComaListenerInit *inits,
                              int                     num_inits,
                              void                   *ctx )
{
     int           i;
     DirectResult  ret;
     ComaListener *listener;

     DIRECT_INTERFACE_GET_DATA (IComaComponent)

     if (!inits || num_inits < 1)
          return DFB_INVARG;

     for (i=0; i<num_inits; i++) {
          if (inits[i].id < 0 || inits[i].id >= data->num_notifications)
               return DFB_LIMITEXCEEDED;

          listener = &data->listeners[inits[i].id];

          if (listener->func)
               return DFB_BUSY;

          ret = coma_component_attach_channel( data->component, inits[i].id,
                                               IComaComponent_ListenerReaction,
                                               listener, &listener->reaction );
          if (ret)
               return ret;

          listener->func = inits[i].func;
          listener->ctx  = inits[i].ctx ? : ctx;
     }

     return DFB_OK;
}

static DFBResult
IComaComponent_Unlisten( IComaComponent     *thiz,
                         ComaNotificationID  id )
{
     ComaListener *listener;

     DIRECT_INTERFACE_GET_DATA (IComaComponent)

     if (id < 0 || id >= data->num_notifications)
          return DFB_LIMITEXCEEDED;

     listener = &data->listeners[id];

     if (!listener->func)
          return DFB_ITEMNOTFOUND;

     coma_component_detach( data->component, &listener->reaction );

     listener->func = NULL;

     return DFB_OK;
}

DFBResult
IComaComponent_Construct( IComaComponent *thiz,
                          Coma           *coma,
                          ComaComponent  *component,
                          int             num_notifications )
{
     DirectResult ret;

     /* Allocate interface data. */
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IComaComponent );

     ret = coma_component_ref( component );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Initialize interface data. */
     data->ref               = 1;
     data->coma              = coma;
     data->component         = component;
     data->num_notifications = num_notifications;
     data->listeners         = D_CALLOC( num_notifications, sizeof(ComaListener) );

     if (!data->listeners) {
          D_OOM();
          coma_component_unref( component );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_NOSYSTEMMEMORY;
     }

     /* Assign interface pointers. */
     thiz->AddRef            = IComaComponent_AddRef;
     thiz->Release           = IComaComponent_Release;
     thiz->InitNotification  = IComaComponent_InitNotification;
     thiz->InitNotifications = IComaComponent_InitNotifications;
     thiz->Call              = IComaComponent_Call;
     thiz->Return            = IComaComponent_Return;
     thiz->Notify            = IComaComponent_Notify;
     thiz->Listen            = IComaComponent_Listen;
     thiz->InitListeners     = IComaComponent_InitListeners;
     thiz->Unlisten          = IComaComponent_Unlisten;

     return DFB_OK;
}

/**********************************************************************************************************************/

static ReactionResult
IComaComponent_ListenerReaction( const void *msg_data,
                                 void       *ctx )
{
     void  * const *p_arg    = msg_data;
     ComaListener  *listener = ctx;

     D_ASSERT( msg_data != NULL );
     D_ASSERT( ctx != NULL );

     D_ASSUME( listener->func != NULL );

     if (listener->func)
          listener->func( listener->ctx, *p_arg );

     return RS_OK;
}

