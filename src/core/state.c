/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <pthread.h>

#include <core/fusion/reactor.h>

#include "directfb.h"

#include "core.h"
#include "coretypes.h"

#include "state.h"
#include "surfaces.h"


static ReactionResult destination_listener( const void *msg_data,
                                            void       *ctx );
static ReactionResult source_listener     ( const void *msg_data,
                                            void       *ctx );



int dfb_state_init( CardState *state )
{
     return skirmish_init( &state->lock );
}

void dfb_state_destroy( CardState *state )
{
     skirmish_destroy( &state->lock );
}

void dfb_state_set_destination( CardState *state, CoreSurface *destination )
{
     if (state->destination != destination) {
          if (state->destination)
               reactor_detach( state->destination->reactor,
                               destination_listener, state );

          if (destination)
               reactor_attach( destination->reactor,
                               destination_listener, state );

          state->destination  = destination;
          state->modified    |= SMF_DESTINATION;
     }
}

void dfb_state_set_source( CardState *state, CoreSurface *source )
{
     if (state->source != source) {
          if (state->source)
               reactor_detach( state->source->reactor, source_listener, state );

          if (source)
               reactor_attach( source->reactor, source_listener, state );

          state->source    = source;
          state->modified |= SMF_SOURCE;
     }
}

/**********************/

static ReactionResult destination_listener( const void *msg_data,
                                            void       *ctx )
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*)msg_data;
     CardState               *state        = (CardState*)ctx;

     if (notification->flags & CSNF_DESTROY) {
          state->destination  = NULL;
          state->modified    |= SMF_DESTINATION;

          return RS_REMOVE;
     }

     if (notification->flags & CSNF_SIZEFORMAT) {
          CoreSurface *surface = notification->surface;

          /* if this really happens everything should be clipped */
          if (surface->width <= state->clip.x1)
               state->clip.x1 = surface->width - 1;
          if (surface->height <= state->clip.y1)
               state->clip.y1 = surface->height - 1;


          if (surface->width <= state->clip.x2)
               state->clip.x2 = surface->width - 1;
          if (surface->height <= state->clip.y2)
               state->clip.y2 = surface->height - 1;

          state->modified |= SMF_CLIP;
     }

     if (notification->flags & (CSNF_DESTROY | CSNF_SIZEFORMAT |
                                CSNF_VIDEO | CSNF_FLIP))
          state->modified |= SMF_DESTINATION;

     return RS_OK;
}

static ReactionResult source_listener( const void *msg_data, void *ctx )
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*)msg_data;
     CardState               *state        = (CardState*)ctx;

     if (notification->flags & CSNF_DESTROY) {
          state->source = NULL;
          state->modified |= SMF_SOURCE;

          return RS_REMOVE;
     }

     if (notification->flags & (CSNF_DESTROY | CSNF_SIZEFORMAT |
                                CSNF_VIDEO | CSNF_FLIP))
          state->modified |= SMF_SOURCE;

     return RS_OK;
}

