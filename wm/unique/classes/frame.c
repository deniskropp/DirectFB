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

#include <core/gfxcard.h>
#include <core/state.h>
#include <core/windows_internal.h>

#include <misc/util.h>

#include <unique/context.h>
#include <unique/stret.h>
#include <unique/internal.h>


D_DEBUG_DOMAIN( UniQuE_Frame, "UniQuE/Frame", "UniQuE's Frame Region Class" );


static void
frame_update( StretRegion     *region,
              void            *region_data,
              void            *update_data,
              unsigned long    arg,
              int              x,
              int              y,
              const DFBRegion *updates,
              int              num )
{
     int                     i;
     DFBRegion               clip;
     DFBSurfaceDrawingFlags  flags = DSDRAW_NOFX;
     WindowData             *data  = region_data;
     CardState              *state = update_data;
     CoreWindow             *window;

     D_ASSERT( region != NULL );
     D_ASSERT( region_data != NULL );
     D_ASSERT( update_data != NULL );
     D_ASSERT( updates != NULL );

     D_MAGIC_ASSERT( data, WindowData );
     D_MAGIC_ASSERT( state, CardState );

     window = data->window;

     D_DEBUG_AT( UniQuE_Frame, "frame_update( region %p, window %p, opacity %d, num %d )\n",
                 region, window, window->opacity, num );
#if DIRECT_BUILD_DEBUG
     for (i=0; i<num; i++) {
          D_DEBUG_AT( UniQuE_Frame, "    (%d)  %4d,%4d - %4dx%4d\n",
                      i, DFB_RECTANGLE_VALS_FROM_REGION( &updates[i] ) );
     }
#endif

     if (!window->opacity)
          return;

     /* Save clipping region. */
     clip = state->clip;

     /* Use alpha blending. */
     //if (window->opacity != 0xFF)
          flags |= DSDRAW_BLEND;

     /* Set color. */
     state->color     = data->stack_data->context->color;
     state->modified |= SMF_COLOR;

     /* Set drawing flags. */
     dfb_state_set_drawing_flags( state, flags );

     for (i=0; i<num; i++) {
          DFBRectangle rect;
          DFBRegion    update = DFB_REGION_INIT_TRANSLATED( &updates[i], x, y );

          /* Change clipping region. */
          dfb_state_set_clip( state, &update );


          /* top */
          rect = (DFBRectangle) { x, y,
                                  window->width + data->insets.l + data->insets.r, data->insets.t };

          dfb_gfxcard_fillrectangle( &rect, state );


          /* left */
          rect = (DFBRectangle) { x, y + data->insets.t, data->insets.l, window->height };

          dfb_gfxcard_fillrectangle( &rect, state );


          /* right */
          rect = (DFBRectangle) { x + data->insets.l + window->width,
                                  y + data->insets.t, data->insets.r, window->height };

          dfb_gfxcard_fillrectangle( &rect, state );


          /* bottom */
          rect = (DFBRectangle) { x, y + data->insets.t + window->height,
                                  window->width + data->insets.l + data->insets.r, data->insets.b };

          dfb_gfxcard_fillrectangle( &rect, state );
     }

     /* Restore clipping region. */
     dfb_state_set_clip( state, &clip );
}

const StretRegionClass unique_frame_region_class = {
     Update:   frame_update
};

