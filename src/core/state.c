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

#define _GNU_SOURCE

#include <config.h>

#include <string.h>

#include <pthread.h>

#include <core/fusion/reactor.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/surfaces.h>

#include <misc/mem.h>


static ReactionResult destination_listener( const void *msg_data,
                                            void       *ctx );
static ReactionResult source_listener     ( const void *msg_data,
                                            void       *ctx );



int
dfb_state_init( CardState *state )
{
     pthread_mutexattr_t attr;

     DFB_ASSERT( state != NULL );

     memset( state, 0, sizeof(CardState) );

     state->modified  = SMF_ALL;
     state->src_blend = DSBF_SRCALPHA;
     state->dst_blend = DSBF_INVSRCALPHA;

     pthread_mutexattr_init( &attr );
     pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );

     pthread_mutex_init( &state->lock, &attr );

     pthread_mutexattr_destroy( &attr );

     state->initialized = true;

     return 0;
}

void
dfb_state_destroy( CardState *state )
{
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( state->initialized );

     state->initialized = false;

     if (state->gfxs)
          DFBFREE( state->gfxs );

     pthread_mutex_destroy( &state->lock );
}

void
dfb_state_set_destination( CardState *state, CoreSurface *destination )
{
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( state->initialized );

     dfb_state_lock( state );

     if (state->destination != destination) {
          if (state->destination) {
               dfb_surface_detach( state->destination,
                                   &state->destination_reaction );
               dfb_surface_unref( state->destination );
          }

          state->destination  = destination;
          state->modified    |= SMF_DESTINATION;

          if (destination) {
               dfb_surface_ref( destination );
               dfb_surface_attach( destination, destination_listener,
                                   state, &state->destination_reaction );
          }
     }

     dfb_state_unlock( state );
}

void
dfb_state_set_source( CardState *state, CoreSurface *source )
{
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( state->initialized );

     dfb_state_lock( state );

     if (state->source != source) {
          if (state->source) {
               dfb_surface_detach( state->source,
                                   &state->source_reaction );
               dfb_surface_unref( state->source );
          }

          state->source    = source;
          state->modified |= SMF_SOURCE;

          if (source) {
               dfb_surface_ref( source );
               dfb_surface_attach( source, source_listener,
                                   state, &state->source_reaction );
          }
     }

     dfb_state_unlock( state );
}

/**********************/

static ReactionResult
destination_listener( const void *msg_data,
                      void       *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     CardState                     *state        = ctx;

     DFB_ASSERT( state->initialized );

//     dfb_state_lock( state );

     if (notification->flags & (CSNF_DESTROY | CSNF_SIZEFORMAT |
                                CSNF_VIDEO | CSNF_FLIP | CSNF_PALETTE_CHANGE |
                                CSNF_PALETTE_UPDATE))
          state->modified |= SMF_DESTINATION;

     if (notification->flags & CSNF_DESTROY) {
          dfb_surface_unref( state->destination );
          state->destination = NULL;

//          dfb_state_unlock( state );

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

//     dfb_state_unlock( state );

     return RS_OK;
}

static ReactionResult
source_listener( const void *msg_data,
                 void       *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     CardState                     *state        = ctx;

     DFB_ASSERT( state->initialized );

//     dfb_state_lock( state );

     if (notification->flags & (CSNF_DESTROY | CSNF_SIZEFORMAT | CSNF_FIELD |
                                CSNF_VIDEO | CSNF_FLIP | CSNF_PALETTE_CHANGE |
                                CSNF_PALETTE_UPDATE))
          state->modified |= SMF_SOURCE;

     if (notification->flags & CSNF_DESTROY) {
          dfb_surface_unref( state->source );
          state->source = NULL;

//          dfb_state_unlock( state );

          return RS_REMOVE;
     }

//     dfb_state_unlock( state );

     return RS_OK;
}

