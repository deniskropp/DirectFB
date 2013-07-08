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

#ifndef __FUSIONDALE_CORE_MESSENGER_PORT_H__
#define __FUSIONDALE_CORE_MESSENGER_PORT_H__

#include <fusiondale.h>

#include <fusion/object.h>
#include <fusion/reactor.h>

#include <core/dale_types.h>


typedef enum {
     CMPNF_NONE  = 0x00000000,

     CMPNF_EVENT = 0x00000001,

     CMPNF_ALL   = 0x00000001
} CoreMessengerPortNotificationFlags;

typedef struct {
     CoreMessengerPortNotificationFlags  flags;
     CoreMessengerPort                  *port;

     FDMessengerEventID                  event_id;
     int                                 param;
     void                               *data;
     unsigned int                        data_size;

     CoreMessengerDispatch              *dispatch;     /* for integrity check with node's next_dispatch */
} CoreMessengerPortNotification;

struct __FD_CoreMessengerPort {
     FusionObject           object;

     int                    magic;

     CoreMessenger         *messenger;

     FusionSkirmish        *lock;

     FusionHash            *nodes;
     FusionHash            *listeners;

     FDMessengerEventID     last_listener;

     GlobalReaction         reaction;
     Reaction               local_reaction;
};

typedef DirectEnumerationResult (*CoreMPListenerCallback)( CoreMessengerPort        *port,
                                                           FDMessengerEventCallback  listener,
                                                           void                     *listener_context,
                                                           void                     *context );

/*
 * Creates a pool of messenger port objects.
 */
FusionObjectPool *fd_messenger_port_pool_create( const FusionWorld *world );

/*
 * Generates fd_messenger_port_ref(), fd_messenger_port_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreMessengerPort, fd_messenger_port )


/*
 * Creation
 */

DirectResult fd_messenger_port_create( CoreDale           *core,
                                       CoreMessenger      *messenger,
                                       CoreMessengerPort **ret_port );

/*
 * Events
 */

DirectResult fd_messenger_port_add_event   ( CoreMessengerPort  *port,
                                             CoreMessengerEvent *event );

DirectResult fd_messenger_port_remove_event( CoreMessengerPort  *port,
                                             FDMessengerEventID  event_id );

DirectResult fd_messenger_port_send_event  ( CoreMessengerPort  *port,
                                             FDMessengerEventID  event_id,
                                             int                 param,
                                             void               *data_ptr,
                                             unsigned int        data_size );

/*
 * Listeners
 */

DirectResult fd_messenger_port_add_listener   ( CoreMessengerPort        *port,
                                                FDMessengerEventID        event_id,
                                                FDMessengerEventCallback  callback,
                                                void                     *context,
                                                FDMessengerListenerID    *ret_id );

DirectResult fd_messenger_port_remove_listener( CoreMessengerPort        *port,
                                                FDMessengerListenerID     listener_id );

DirectResult fd_messenger_port_enum_listeners ( CoreMessengerPort        *port,
                                                FDMessengerEventID        event_id,
                                                CoreMPListenerCallback    callback,
                                                void                     *context );

/*
 * Global reactions
 */
ReactionResult _fd_messenger_port_messenger_listener( const void *msg_data,
                                                      void       *ctx );

#endif
