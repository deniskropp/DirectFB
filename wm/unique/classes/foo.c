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


D_DEBUG_DOMAIN( UniQuE_Foo, "UniQuE/Foo", "UniQuE's Foo Region Class" );


static void
foo_update( StretRegion     *region,
            void            *region_data,
            void            *update_data,
            unsigned long    arg,
            int              x,
            int              y,
            const DFBRegion *updates,
            int              num )
{
     int           i;
     DFBRegion     clip;
     DFBDimension  size;
     WindowData   *data  = region_data;
     CardState    *state = update_data;
     WMShared     *shared;
     CoreWindow   *window;

     D_ASSERT( region != NULL );
     D_ASSERT( region_data != NULL );
     D_ASSERT( update_data != NULL );
     D_ASSERT( updates != NULL );

     D_MAGIC_ASSERT( data, WindowData );
     D_MAGIC_ASSERT( state, CardState );

     window = data->window;
     shared = data->shared;

     D_ASSERT( window != NULL );
     D_ASSERT( shared != NULL );
     D_ASSERT( shared->foo_surface != NULL );

     D_DEBUG_AT( UniQuE_Foo, "foo_update( region %p, window %p, opacity %d, num %d )\n",
                 region, window, window->opacity, num );
#if DIRECT_BUILD_DEBUG
     for (i=0; i<num; i++) {
          D_DEBUG_AT( UniQuE_Foo, "    (%d)  %4d,%4d - %4dx%4d\n",
                      i, DFB_RECTANGLE_VALS_FROM_REGION( &updates[i] ) );
     }
#endif

     if (!window->opacity)
          return;

     stret_region_get_size( region, &size );

     /* Set blitting flags. */
     dfb_state_set_blitting_flags( state, DSBLIT_BLEND_ALPHACHANNEL );

     /* Set blitting source. */
     state->source    = shared->foo_surface;
     state->modified |= SMF_SOURCE;

     switch (arg) {
          case UFI_N:
          case UFI_E:
          case UFI_S:
          case UFI_W:
               clip = state->clip;

/*               for (i=0; i<num; i++) {
                    DFBRegion    update = DFB_REGION_INIT_TRANSLATED( &updates[i], x, y );
                    DFBRectangle source = shared->foo_rects[arg];
                    DFBRectangle dest   = { x, y, size.w, size.h };

                    dfb_state_set_clip( state, &update );

                    dfb_gfxcard_stretchblit( &source, &dest, state );
               }*/
               for (i=0; i<num; i++) {
                    DFBRegion    update = DFB_REGION_INIT_TRANSLATED( &updates[i], x, y );
                    DFBRectangle source = shared->foo_rects[arg];

                    dfb_state_set_clip( state, &update );

                    dfb_gfxcard_tileblit( &source, x, y, x + size.w - 1, y + size.h - 1, state );
               }

               dfb_state_set_clip( state, &clip );
               break;

          case UFI_NE:
          case UFI_SE:
          case UFI_SW:
          case UFI_NW:
               for (i=0; i<num; i++) {
                    DFBRectangle rect = DFB_RECTANGLE_INIT_FROM_REGION( &updates[i] );

                    dfb_rectangle_translate( &rect, shared->foo_rects[arg].x, shared->foo_rects[arg].y );

                    dfb_gfxcard_blit( &rect, x + updates[i].x1, y + updates[i].y1, state );
               }
               break;

          default:
               D_BUG( "invalid arg" );
     }

     /* Reset blitting source. */
     state->source    = NULL;
     state->modified |= SMF_SOURCE;
}

const StretRegionClass unique_foo_region_class = {
     Update:   foo_update
};

