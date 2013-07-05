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

#include <unistd.h>

#include <fusiondale.h>

#include <direct/interface.h>
#include <direct/mem.h>

#include <fusion/shmalloc.h>

#include <core/dale_core.h>
#include <core/messenger.h>
#include <core/messenger_port.h>

#include "ifusiondalemessenger.h"

D_DEBUG_DOMAIN( IFusionDale_Messenger, "IFusionDaleMessenger", "IFusionDaleMessenger" );

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

static DirectResult
IFusionDaleMessenger_AddRef( IFusionDaleMessenger *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionDaleMessenger)

     data->ref++;

     return DR_OK;
}

static DirectResult
IFusionDaleMessenger_Release( IFusionDaleMessenger *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionDaleMessenger)

     if (--data->ref == 0)
          IFusionDaleMessenger_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IFusionDaleMessenger_RegisterEvent( IFusionDaleMessenger *thiz,
                                    const char           *name,
                                    FDMessengerEventID   *ret_id )
{
     DirectResult        ret;
     CoreMessengerEvent *event;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     D_DEBUG_AT( IFusionDale_Messenger, "%s()\n", __FUNCTION__ );

     /* Check arguments. */
     if (!name || !ret_id)
          return DR_INVARG;

     /* Lock the messenger. */
     ret = fd_messenger_lock( data->messenger );
     if (ret)
          return ret;

     /* Try to lookup event by name. */
     ret = fd_messenger_lookup_event( data->messenger, name, &event );
     switch (ret) {
          case DR_OK:
               /* Event is found (already registered). */
               ret = DR_BUSY;
               break;

          case DR_ITEMNOTFOUND:
               /* Create a new event. */
               ret = fd_messenger_create_event( data->messenger, name, &event );
               if (ret == DR_OK)
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

static DirectResult
IFusionDaleMessenger_UnregisterEvent( IFusionDaleMessenger *thiz,
                                      FDMessengerEventID    event_id )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     D_DEBUG_AT( IFusionDale_Messenger, "%s()\n", __FUNCTION__ );

     /* Check arguments */
     if (!event_id)
          return DR_INVARG;

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

static DirectResult
IFusionDaleMessenger_IsEventRegistered( IFusionDaleMessenger *thiz,
                                        const char           *name )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     D_DEBUG_AT( IFusionDale_Messenger, "%s()\n", __FUNCTION__ );

     /* Check arguments */
     if (!name)
          return DR_INVARG;

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

static DirectResult
IFusionDaleMessenger_RegisterListener( IFusionDaleMessenger     *thiz,
                                       FDMessengerEventID        event_id,
                                       FDMessengerEventCallback  callback,
                                       void                     *context,
                                       FDMessengerListenerID    *ret_id )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     D_DEBUG_AT( IFusionDale_Messenger, "%s()\n", __FUNCTION__ );

     /* Check arguments */
     if (!event_id || !callback || !ret_id)
          return DR_INVARG;

     /* Lock the messenger (has to happen before port is locked!). */
     ret = fd_messenger_lock( data->messenger );
     if (ret)
          return ret;

     ret = fd_messenger_port_add_listener( data->port, event_id, callback, context, ret_id );

     /* Unlock the messenger. */
     fd_messenger_unlock( data->messenger );

     return ret;
}

static DirectResult
IFusionDaleMessenger_UnregisterListener( IFusionDaleMessenger  *thiz,
                                         FDMessengerListenerID  listener_id )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     D_DEBUG_AT( IFusionDale_Messenger, "%s()\n", __FUNCTION__ );

     /* Check arguments */
     if (!listener_id)
          return DR_INVARG;

     /* Lock the messenger (has to happen before port is locked!). */
     ret = fd_messenger_lock( data->messenger );
     if (ret)
          return ret;

     ret = fd_messenger_port_remove_listener( data->port, listener_id );

     /* Unlock the messenger. */
     fd_messenger_unlock( data->messenger );

     return ret;
}

static DirectResult
IFusionDaleMessenger_SendSimpleEvent( IFusionDaleMessenger *thiz,
                                      FDMessengerEventID    event_id,
                                      int                   param )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     D_DEBUG_AT( IFusionDale_Messenger, "%s()\n", __FUNCTION__ );

     /* Check arguments */
     if (!event_id)
          return DR_INVARG;

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

static DirectResult
IFusionDaleMessenger_SendEvent( IFusionDaleMessenger *thiz,
                                FDMessengerEventID    event_id,
                                int                   param,
                                void                 *data_ptr,
                                unsigned int          data_size )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     D_DEBUG_AT( IFusionDale_Messenger, "%s()\n", __FUNCTION__ );

     /* Check arguments */
     if (!event_id || !data_ptr || !data_size)
          return DR_INVARG;

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

static DirectResult
IFusionDaleMessenger_AllocateData( IFusionDaleMessenger  *thiz,
                                   unsigned int           data_size,
                                   void                 **ret_data )
{
     DirectResult  ret;
     void         *data_ptr;

     DIRECT_INTERFACE_GET_DATA(IFusionDaleMessenger)

     D_DEBUG_AT( IFusionDale_Messenger, "%s()\n", __FUNCTION__ );

     /* Check arguments */
     if (!data_size || !ret_data)
          return DR_INVARG;

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

     return DR_OK;
}

DirectResult
IFusionDaleMessenger_Construct( IFusionDaleMessenger *thiz,
                                CoreDale             *core,
                                CoreMessenger        *messenger )
{
     DirectResult       ret;
     CoreMessengerPort *port;

     /* Allocate interface data. */
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionDaleMessenger );

     D_DEBUG_AT( IFusionDale_Messenger, "%s()\n", __FUNCTION__ );

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

     return DR_OK;


error_port:
     fd_messenger_unref( messenger );

error:
     DIRECT_DEALLOCATE_INTERFACE( thiz );

     return ret;
}

