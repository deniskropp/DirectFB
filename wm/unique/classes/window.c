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
#include <core/surfaces.h>
#include <core/windows_internal.h>

#include <misc/util.h>

#include <unique/context.h>
#include <unique/internal.h>
#include <unique/stret.h>
#include <unique/window.h>


D_DEBUG_DOMAIN( UniQuE_Window, "UniQuE/Window", "UniQuE's Window Region Class" );


static void
window_update( StretRegion     *region,
               void            *region_data,
               void            *update_data,
               unsigned long    arg,
               int              x,
               int              y,
               const DFBRegion *updates,
               int              num )
{
     int                      i;
     DFBSurfaceBlittingFlags  flags  = DSBLIT_NOFX;
     UniqueWindow            *window = region_data;
     CardState               *state  = update_data;
     bool                     alpha  = arg;
     bool                     visible;

     D_ASSERT( region != NULL );
     D_ASSERT( region_data != NULL );
     D_ASSERT( update_data != NULL );
     D_ASSERT( updates != NULL );

     D_MAGIC_ASSERT( window, UniqueWindow );
     D_MAGIC_ASSERT( state, CardState );

     D_ASSERT( window->surface != NULL );

     visible = D_FLAGS_IS_SET( window->flags, UWF_VISIBLE );

     D_DEBUG_AT( UniQuE_Window, "window_update( region %p, window %p, visible %s, num %d )\n",
                 region, window, visible ? "yes" : "no", num );
#if DIRECT_BUILD_DEBUG
     for (i=0; i<num; i++) {
          D_DEBUG_AT( UniQuE_Window, "    (%d)  %4d,%4d - %4dx%4d\n",
                      i, DFB_RECTANGLE_VALS_FROM_REGION( &updates[i] ) );
     }
#endif

     if (!visible)
          return;

     /* Use per pixel alpha blending. */
     if (alpha && (window->options & DWOP_ALPHACHANNEL))
          flags |= DSBLIT_BLEND_ALPHACHANNEL;

     /* Use global alpha blending. */
     if (window->opacity != 0xFF) {
          flags |= DSBLIT_BLEND_COLORALPHA;

          /* Set opacity as blending factor. */
          if (state->color.a != window->opacity) {
               state->color.a   = window->opacity;
               state->modified |= SMF_COLOR;
          }
     }

     /* Use source color keying. */
     if (window->options & DWOP_COLORKEYING) {
          flags |= DSBLIT_SRC_COLORKEY;

          /* Set window color key. */
          dfb_state_set_src_colorkey( state, window->color_key );
     }

     /* Use automatic deinterlacing. */
     if (window->surface->caps & DSCAPS_INTERLACED)
          flags |= DSBLIT_DEINTERLACE;

     /* Set blitting flags. */
     dfb_state_set_blitting_flags( state, flags );

     /* Set blitting source. */
     state->source    = window->surface;
     state->modified |= SMF_SOURCE;

     for (i=0; i<num; i++) {
          DFBRectangle src = DFB_RECTANGLE_INIT_FROM_REGION( &updates[i] );

          /* Blit from the window to the region being updated. */
          dfb_gfxcard_blit( &src, x + src.x, y + src.y, state );
     }

     /* Reset blitting source. */
     state->source    = NULL;
     state->modified |= SMF_SOURCE;
}

const StretRegionClass unique_window_region_class = {
     Update:   window_update
};

