/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/shmalloc.h>
#include <fusion/vector.h>

#include <core/layers_internal.h>
#include <core/windows_internal.h>

#include <unique/device.h>
#include <unique/internal.h>


D_DEBUG_DOMAIN( UniQuE_InputChan, "UniQuE/InputChan", "UniQuE's Input Channel" );


static const ReactionFunc unique_input_channel_globals[] = {
     _unique_window_input_channel_listener,
     NULL
};

/**************************************************************************************************/

DFBResult
unique_input_channel_create( UniqueContext       *context,
                             UniqueInputChannel **ret_channel )
{
     UniqueInputChannel *channel;

     D_DEBUG_AT( UniQuE_InputChan, "unique_input_channel_create( context %p )\n", context );

     D_MAGIC_ASSERT( context, UniqueContext );

     D_ASSERT( ret_channel != NULL );

     /* Allocate channel data. */
     channel = SHCALLOC( 1, sizeof(UniqueInputChannel) );
     if (!channel) {
          D_WARN( "out of (shared) memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     /* Initialize channel data. */
     channel->context = context;

     /* Create reactor for dispatching events. */
     channel->reactor = fusion_reactor_new( sizeof(UniqueInputEvent), "UniQuE Input Channel" );
     if (!channel->reactor) {
          SHFREE( channel );
          return DFB_FUSION;
     }

     fusion_reactor_set_lock( channel->reactor, &context->stack->context->lock );

     D_MAGIC_SET( channel, UniqueInputChannel );

     D_DEBUG_AT( UniQuE_InputChan, "  -> channel created (%p)\n", channel );

     *ret_channel = channel;

     return DFB_OK;
}

DFBResult
unique_input_channel_destroy( UniqueInputChannel *channel )
{
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     D_DEBUG_AT( UniQuE_InputChan, "unique_input_channel_destroy( %p )\n", channel );

     fusion_reactor_free( channel->reactor );

     D_MAGIC_CLEAR( channel );

     SHFREE( channel );

     return DFB_OK;
}


DFBResult
unique_input_channel_attach( UniqueInputChannel *channel,
                             ReactionFunc        func,
                             void               *ctx,
                             Reaction           *reaction )
{
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     return fusion_reactor_attach( channel->reactor, func, ctx, reaction );
}

DFBResult
unique_input_channel_detach( UniqueInputChannel *channel,
                             Reaction           *reaction )
{
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     return fusion_reactor_detach( channel->reactor, reaction );
}

DFBResult
unique_input_channel_attach_global( UniqueInputChannel *channel,
                                    int                 index,
                                    void               *ctx,
                                    GlobalReaction     *reaction )
{
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     return fusion_reactor_attach_global( channel->reactor, index, ctx, reaction );
}

DFBResult
unique_input_channel_detach_global( UniqueInputChannel *channel,
                                    GlobalReaction     *reaction )
{
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     return fusion_reactor_detach_global( channel->reactor, reaction );
}

DFBResult
unique_input_channel_dispatch( UniqueInputChannel     *channel,
                               const UniqueInputEvent *event )
{
     D_MAGIC_ASSERT( channel, UniqueInputChannel );

     D_ASSERT( event != NULL );

     D_DEBUG_AT( UniQuE_InputChan, "unique_input_channel_dispatch( %p, %p ) <- type 0x%08x\n",
                 channel, event, event->type );

     switch (event->type) {
          case UIET_MOTION:
               D_DEBUG_AT( UniQuE_InputChan, "  -> MOTION   %d, %d, buttons 0x%04x\n",
                           event->pointer.x, event->pointer.y, event->pointer.buttons );
               break;
          case UIET_BUTTON:
               D_DEBUG_AT( UniQuE_InputChan, "  -> BUTTON   %d, %d, buttons 0x%04x, button %d, %s\n",
                           event->pointer.x, event->pointer.y,
                           event->pointer.buttons, event->pointer.button,
                           event->pointer.press ? "pressed" : "released" );
               break;
          case UIET_WHEEL:
               D_DEBUG_AT( UniQuE_InputChan, "  -> WHEEL    %d\n", event->wheel.value );
               break;
          case UIET_KEY:
               D_DEBUG_AT( UniQuE_InputChan, "  -> KEY      0x%08x, modifiers 0x%04x, %s\n",
                           event->keyboard.key_symbol, event->keyboard.modifiers,
                           event->keyboard.press ? "pressed" : "released" );
               break;
          case UIET_CHANNEL:
               D_DEBUG_AT( UniQuE_InputChan, "  -> CHANNEL  %d, %d, index %d, %s\n",
                           event->channel.x, event->channel.y, event->channel.index,
                           event->channel.selected ? "selected" : "deselected" );
               break;
          default:
               D_DEBUG_AT( UniQuE_InputChan, "  -> unknown type 0x%08x\n", event->type );
               break;
     }

     return fusion_reactor_dispatch( channel->reactor, event, true, unique_input_channel_globals );
}

