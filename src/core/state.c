/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <string.h>

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



int
dfb_state_init( CardState *state )
{
     DFB_ASSERT( state != NULL );
     
     memset( state, 0, sizeof(CardState) );
     
     state->modified  = SMF_ALL;
     state->src_blend = DSBF_SRCALPHA;
     state->dst_blend = DSBF_INVSRCALPHA;
     
     return pthread_mutex_init( &state->lock, NULL );
}

void
dfb_state_destroy( CardState *state )
{
     DFB_ASSERT( state != NULL );
     
     pthread_mutex_destroy( &state->lock );
}

void
dfb_state_set_destination( CardState *state, CoreSurface *destination )
{
     DFB_ASSERT( state != NULL );

     if (state->destination != destination) {
          if (state->destination)
               dfb_surface_detach( state->destination,
                                   destination_listener, state );

          if (destination)
               dfb_surface_attach( destination,
                                   destination_listener, state );

          state->destination  = destination;
          state->modified    |= SMF_DESTINATION;
     }
}

void
dfb_state_set_source( CardState *state, CoreSurface *source )
{
     DFB_ASSERT( state != NULL );

     if (state->source != source) {
          if (state->source)
               dfb_surface_detach( state->source, source_listener, state );

          if (source)
               dfb_surface_attach( source, source_listener, state );

          state->source    = source;
          state->modified |= SMF_SOURCE;
     }
}

/**********************/

static ReactionResult
destination_listener( const void *msg_data,
                      void       *ctx )
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*)msg_data;
     CardState               *state        = (CardState*)ctx;

     if (notification->flags & (CSNF_DESTROY | CSNF_SIZEFORMAT |
                                CSNF_VIDEO | CSNF_FLIP | CSNF_PALETTE))
          state->modified |= SMF_DESTINATION;

     if (notification->flags & CSNF_DESTROY) {
          state->destination = NULL;

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

     return RS_OK;
}

static ReactionResult
source_listener( const void *msg_data,
                 void       *ctx )
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*)msg_data;
     CardState               *state        = (CardState*)ctx;

     if (notification->flags & (CSNF_DESTROY | CSNF_SIZEFORMAT |
                                CSNF_VIDEO | CSNF_FLIP | CSNF_PALETTE))
          state->modified |= SMF_SOURCE;

     if (notification->flags & CSNF_DESTROY) {
          state->source = NULL;

          return RS_REMOVE;
     }

     return RS_OK;
}

