/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <string.h>

#include <pthread.h>

#include <fusion/fusion.h>
#include <fusion/reactor.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/surfaces.h>

#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/util.h>


static inline void
validate_clip( CardState *state,
               int        xmax,
               int        ymax )
{
     D_MAGIC_ASSERT( state, CardState );

     DFB_REGION_ASSERT( &state->clip );

     if (state->clip.x1 <= xmax &&
         state->clip.y1 <= ymax &&
         state->clip.x2 <= xmax &&
         state->clip.y2 <= ymax)
          return;

     if (state->clip.x1 > xmax)
          state->clip.x1 = xmax;

     if (state->clip.y1 > ymax)
          state->clip.y1 = ymax;

     if (state->clip.x2 > xmax)
          state->clip.x2 = xmax;

     if (state->clip.y2 > ymax)
          state->clip.y2 = ymax;

     state->modified |= SMF_CLIP;
}

int
dfb_state_init( CardState *state, CoreDFB *core )
{
     D_ASSERT( state != NULL );

     memset( state, 0, sizeof(CardState) );

     state->core      = core;
     state->fusion_id = fusion_id( dfb_core_world(core) );
     state->modified  = SMF_ALL;
     state->src_blend = DSBF_SRCALPHA;
     state->dst_blend = DSBF_INVSRCALPHA;

     direct_util_recursive_pthread_mutex_init( &state->lock );

     direct_serial_init( &state->dst_serial );
     direct_serial_init( &state->src_serial );

     D_MAGIC_SET( state, CardState );

     return 0;
}

void
dfb_state_destroy( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );

     D_ASSUME( state->destination == NULL );
     D_ASSUME( state->source == NULL );

     D_MAGIC_CLEAR( state );

     direct_serial_deinit( &state->dst_serial );
     direct_serial_deinit( &state->src_serial );

     if (state->gfxs) {
          GenefxState *gfxs = state->gfxs;

          if (gfxs->ABstart)
               D_FREE( gfxs->ABstart );

          D_FREE( gfxs );
     }

     if (state->num_translation) {
          D_ASSERT( state->index_translation != NULL );

          D_FREE( state->index_translation );
     }
     else
          D_ASSERT( state->index_translation == NULL );

     pthread_mutex_destroy( &state->lock );
}

void
dfb_state_set_destination( CardState *state, CoreSurface *destination )
{
     D_MAGIC_ASSERT( state, CardState );

     dfb_state_lock( state );

     if (state->destination != destination) {
          if (destination) {
               if (dfb_surface_ref( destination )) {
                    D_WARN( "could not ref() destination" );
                    return;
               }

               validate_clip( state, destination->width - 1, destination->height - 1 );
          }

          if (state->destination) {
               D_ASSERT( D_FLAGS_IS_SET( state->flags, CSF_DESTINATION ) );
               dfb_surface_unref( state->destination );
          }

          state->destination  = destination;
          state->modified    |= SMF_DESTINATION;

          if (destination) {
               direct_serial_copy( &state->dst_serial, &destination->serial );

               D_FLAGS_SET( state->flags, CSF_DESTINATION );
          }
          else
               D_FLAGS_CLEAR( state->flags, CSF_DESTINATION );
     }

     dfb_state_unlock( state );
}

void
dfb_state_set_source( CardState *state, CoreSurface *source )
{
     D_MAGIC_ASSERT( state, CardState );

     dfb_state_lock( state );

     if (state->source != source) {
          if (source && dfb_surface_ref( source )) {
               D_WARN( "could not ref() source" );
               return;
          }

          if (state->source) {
               D_ASSERT( D_FLAGS_IS_SET( state->flags, CSF_SOURCE ) );
               dfb_surface_unref( state->source );
          }

          state->source    = source;
          state->modified |= SMF_SOURCE;

          if (source) {
               direct_serial_copy( &state->src_serial, &source->serial );

               D_FLAGS_SET( state->flags, CSF_SOURCE );
          }
          else
               D_FLAGS_CLEAR( state->flags, CSF_SOURCE );
     }

     dfb_state_unlock( state );
}

void
dfb_state_update( CardState *state, bool update_source )
{
     D_MAGIC_ASSERT( state, CardState );

     DFB_REGION_ASSERT( &state->clip );

     if (D_FLAGS_IS_SET( state->flags, CSF_DESTINATION )) {
          CoreSurface *destination = state->destination;

          D_ASSERT( destination != NULL );

          if (direct_serial_update( &state->dst_serial, &destination->serial )) {
               validate_clip( state, destination->width - 1, destination->height - 1 );

               state->modified |= SMF_DESTINATION;
          }
     }

     if (update_source && D_FLAGS_IS_SET( state->flags, CSF_SOURCE )) {
          CoreSurface *source = state->source;

          D_ASSERT( source != NULL );

          if (direct_serial_update( &state->src_serial, &source->serial ))
               state->modified |= SMF_SOURCE;
     }
}

DFBResult
dfb_state_set_index_translation( CardState *state,
                                 const int *indices,
                                 int        num_indices )
{
     D_MAGIC_ASSERT( state, CardState );

     D_ASSERT( indices != NULL || num_indices == 0 );

     dfb_state_lock( state );

     if (state->num_translation != num_indices) {
          int *new_trans = D_REALLOC( state->index_translation,
                                      num_indices * sizeof(int) );

          D_ASSERT( num_indices || new_trans == NULL );

          if (num_indices && !new_trans) {
               dfb_state_unlock( state );
               return D_OOM();
          }

          state->index_translation = new_trans;
          state->num_translation   = num_indices;
     }

     if (num_indices)
          direct_memcpy( state->index_translation, indices, num_indices * sizeof(int) );

     state->modified |= SMF_INDEX_TRANSLATION;

     dfb_state_unlock( state );

     return DFB_OK;
}

