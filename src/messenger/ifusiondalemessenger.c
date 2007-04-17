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

#include <core/dale_core.h>
#include <core/messenger.h>
#include <core/messenger_port.h>

#include "ifusiondalemessenger.h"

/**********************************************************************************************************************/

static void
IFusionDaleMessenger_Destruct( IFusionDaleMessenger *thiz )
{
     IFusionDaleMessenger_data *data = thiz->priv;

     fd_messenger_port_detach( data->port, &data->port->local_reaction );

     fd_messenger_port_unref( data->port );
     fd_messenger_unref( data->messenger );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionDaleMessenger_AddRef( IFusionDaleMessenger *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionDaleMessenger)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionDaleMessenger_Release( IFusionDaleMessenger *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionDaleMessenger)

     if (--data->ref == 0)
          IFusionDaleMessenger_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionDaleMessenger_RegisterEvent( IFusionDaleMessenger *thiz,
                                    const char           *name,
                                    FDMessengerEventID   *ret_id )
{
     DirectResult        ret;
     CoreMessengerEvent *event;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     /* Check arguments. */
     if (!name || !ret_id)
          return DFB_INVARG;

     /* Lock the messenger. */
     ret = fd_messenger_lock( data->messenger );
     if (ret)
          return ret;

     /* Try to lookup event by name. */
     ret = fd_messenger_lookup_event( data->messenger, name, &event );
     switch (ret) {
          case DFB_OK:
               /* Event is found (already registered). */
               ret = DFB_BUSY;
               break;

          case DFB_ITEMNOTFOUND:
               /* Create a new event. */
               ret = fd_messenger_create_event( data->messenger, name, &event );
               if (ret == DFB_OK)
                    break;

               /* fall through */

          default:
               /* Unexpected result. */
               fd_messenger_unlock( data->messenger );
               return ret;
     }

     /* Keep track of our registration. */
     fd_messenger_port_add_event( data->port, event );

     /* Return the event id. */
     *ret_id = event->id;

     /* Unlock the messenger. */
     fd_messenger_unlock( data->messenger );

     return ret;
}

static DFBResult
IFusionDaleMessenger_UnregisterEvent( IFusionDaleMessenger *thiz,
                                      FDMessengerEventID    event_id )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     /* Check arguments */
     if (!event_id)
          return DFB_INVARG;

     /* Lock the messenger (has to happen before port is locked!). */
     ret = fd_messenger_lock( data->messenger );
     if (ret)
          return ret;

     /* Let it go. */
     ret = fd_messenger_port_remove_event( data->port, event_id );

     /* Unlock the messenger. */
     fd_messenger_unlock( data->messenger );

     return ret;
}

static DFBResult
IFusionDaleMessenger_IsEventRegistered( IFusionDaleMessenger *thiz,
                                        const char           *name )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     /* Check arguments */
     if (!name)
          return DFB_INVARG;

     /* Lock the messenger. */
     ret = fd_messenger_lock( data->messenger );
     if (ret)
          return ret;

     /* Try to lookup event by name. */
     ret = fd_messenger_lookup_event( data->messenger, name, NULL );

     /* Unlock the messenger. */
     fd_messenger_unlock( data->messenger );

     return ret;
}

static DFBResult
IFusionDaleMessenger_RegisterListener( IFusionDaleMessenger     *thiz,
                                       FDMessengerEventID        event_id,
                                       FDMessengerEventCallback  callback,
                                       void                     *context,
                                       FDMessengerListenerID    *ret_id )
{
     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     /* Check arguments */
     if (!event_id || !callback || !ret_id)
          return DFB_INVARG;

     return fd_messenger_port_add_listener( data->port, event_id, callback, context, ret_id );
}

static DFBResult
IFusionDaleMessenger_UnregisterListener( IFusionDaleMessenger  *thiz,
                                         FDMessengerListenerID  listener_id )
{
     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     /* Check arguments */
     if (!listener_id)
          return DFB_INVARG;

     return fd_messenger_port_remove_listener( data->port, listener_id );
}

static DFBResult
IFusionDaleMessenger_SendSimpleEvent( IFusionDaleMessenger *thiz,
                                      FDMessengerEventID    event_id,
                                      int                   param )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     /* Check arguments */
     if (!event_id)
          return DFB_INVARG;

     /* Lock the messenger (has to happen before port is locked!). */
     ret = fd_messenger_lock( data->messenger );
     if (ret)
          return ret;

     /* Send the event. */
     ret = fd_messenger_port_send_event( data->port, event_id, param, NULL, 0 );

     /* Unlock the messenger. */
     fd_messenger_unlock( data->messenger );

     return ret;
}

static DFBResult
IFusionDaleMessenger_SendEvent( IFusionDaleMessenger *thiz,
                                FDMessengerEventID    event_id,
                                int                   param,
                                void                 *data_ptr,
                                unsigned int          data_size )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     /* Check arguments */
     if (!event_id || !data_ptr || !data_size)
          return DFB_INVARG;

     /* Lock the messenger (has to happen before port is locked!). */
     ret = fd_messenger_lock( data->messenger );
     if (ret)
          return ret;

     /* Send the event. */
     ret = fd_messenger_port_send_event( data->port, event_id, param, data_ptr, data_size );

     /* Unlock the messenger. */
     fd_messenger_unlock( data->messenger );

     return ret;
}

static DFBResult
IFusionDaleMessenger_AllocateData( IFusionDaleMessenger  *thiz,
                                   unsigned int           data_size,
                                   void                 **ret_data )
{
     DirectResult  ret;
     void         *data_ptr;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     /* Check arguments */
     if (!data_size || !ret_data)
          return DFB_INVARG;

     /* Lock the messenger. */
     ret = fd_messenger_lock( data->messenger );
     if (ret)
          return ret;

     /* Allocate the message data. */
     data_ptr = SHCALLOC( data->messenger->shmpool, 1, data_size );

     /* Unlock the messenger. */
     fd_messenger_unlock( data->messenger );

     if (!data_ptr)
          return D_OOSHM();

     *ret_data = data_ptr;

     return DFB_OK;
}

DFBResult
IFusionDaleMessenger_Construct( IFusionDaleMessenger *thiz,
                                CoreDale             *core,
                                CoreMessenger        *messenger )
{
     DirectResult       ret;
     CoreMessengerPort *port;

     /* Allocate interface data. */
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionDaleMessenger );

     /* Increase messenger's reference counter. */
     ret = fd_messenger_ref( messenger );
     if (ret)
          goto error;

     /* Create our port for the messenger. */
     ret = fd_messenger_port_create( core, messenger, &port );
     if (ret)
          goto error_port;

     /* Initialize interface data. */
     data->ref       = 1;
     data->core      = core;
     data->messenger = messenger;
     data->port      = port;

     /* Assign interface pointers. */
     thiz->AddRef             = IFusionDaleMessenger_AddRef;
     thiz->Release            = IFusionDaleMessenger_Release;
     thiz->RegisterEvent      = IFusionDaleMessenger_RegisterEvent;
     thiz->UnregisterEvent    = IFusionDaleMessenger_UnregisterEvent;
     thiz->IsEventRegistered  = IFusionDaleMessenger_IsEventRegistered;
     thiz->RegisterListener   = IFusionDaleMessenger_RegisterListener;
     thiz->UnregisterListener = IFusionDaleMessenger_UnregisterListener;
     thiz->SendSimpleEvent    = IFusionDaleMessenger_SendSimpleEvent;
     thiz->SendEvent          = IFusionDaleMessenger_SendEvent;
     thiz->AllocateData       = IFusionDaleMessenger_AllocateData;

     return DFB_OK;


error_port:
     fd_messenger_unref( messenger );

error:
     DIRECT_DEALLOCATE_INTERFACE( thiz );

     return ret;
}

