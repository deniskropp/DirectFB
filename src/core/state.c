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

#include <directfb.h>

#include "core.h"
#include "state.h"

static SLResult destination_listener( CoreSurface *surface, unsigned int flags,
                                      void *ctx )
{
     CardState *state = (CardState*)ctx;

     if (flags & CSN_DESTROY) {
          state->destination = NULL;
          state->modified |= SMF_DESTINATION;

          return SL_REMOVE;
     }

     if (flags & CSN_SIZEFORMAT) {
          /* if this really happens everything should be clipped */
          if (surface->width <= state->clip.x2)
               state->clip.x1 = surface->width - 1;
          if (surface->height <= state->clip.y2)
               state->clip.y1 = surface->height - 1;
          
          
          if (surface->width <= state->clip.x2)
               state->clip.x2 = surface->width - 1;
          if (surface->height <= state->clip.y2)
               state->clip.y2 = surface->height - 1;
          
          state->modified |= SMF_CLIP;
     }

     state->modified |= SMF_DESTINATION;

     return SL_OK;
}

void state_set_destination( CardState *state, CoreSurface *destination )
{
     if (state->destination != destination) {
          if (state->destination)
               surface_remove_listener( state->destination,
                                        destination_listener, state );

          if (destination)
               surface_install_listener( destination,
                                         destination_listener,
                                         (CSN_DESTROY | CSN_SIZEFORMAT |
                                          CSN_VIDEO | CSN_FLIP),
                                         state );

          state->destination = destination;
          state->modified |= SMF_DESTINATION;
     }
}

static SLResult source_listener( CoreSurface *surface, unsigned int flags,
                                 void *ctx )
{
     CardState *state = (CardState*)ctx;

     if (flags & CSN_DESTROY) {
          state->source = NULL;
          state->modified |= SMF_SOURCE;

          return SL_REMOVE;
     }

     state->modified |= SMF_SOURCE;

     return SL_OK;
}

void state_set_source( CardState *state, CoreSurface *source )
{
     if (state->source != source) {
          if (state->source)
               surface_remove_listener( state->source,
                                        source_listener, state );

          if (source)
               surface_install_listener( source,
                                         source_listener,
                                         (CSN_DESTROY | CSN_SIZEFORMAT |
                                          CSN_VIDEO | CSN_FLIP),
                                         state );

          state->source = source;
          state->modified |= SMF_SOURCE;
     }
}
