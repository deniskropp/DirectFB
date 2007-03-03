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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/gfxcard.h>
#include <core/palette.h>
#include <core/state.h>
#include <core/surfaces.h>
#include <core/windows_internal.h>

#include <misc/util.h>

#include <unique/context.h>
#include <unique/internal.h>
#include <unique/stret.h>


D_DEBUG_DOMAIN( UniQuE_Root, "UniQuE/Root", "UniQuE's Root Region Class" );


static void
root_update( StretRegion     *region,
             void            *region_data,
             void            *update_data,
             unsigned long    arg,
             int              x,
             int              y,
             const DFBRegion *updates,
             int              num )
{
     int              i;
     CoreWindowStack *stack;
     UniqueContext   *context = region_data;
     CardState       *state   = update_data;

     D_ASSERT( region != NULL );
     D_ASSERT( region_data != NULL );
     D_ASSERT( update_data != NULL );
     D_ASSERT( updates != NULL );

     D_ASSERT( x == 0 );
     D_ASSERT( y == 0 );

     D_MAGIC_ASSERT( context, UniqueContext );
     D_MAGIC_ASSERT( state, CardState );

     stack = context->stack;

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->bg.image != NULL || (stack->bg.mode != DLBM_IMAGE &&
                                           stack->bg.mode != DLBM_TILE) );

     D_DEBUG_AT( UniQuE_Root, "root_update( region %p, num %d )\n", region, num );
#if D_DEBUG_ENABLED
     for (i=0; i<num; i++) {
          D_DEBUG_AT( UniQuE_Root, "    (%d)  %4d,%4d - %4dx%4d\n",
                      i, DFB_RECTANGLE_VALS_FROM_REGION( &updates[i] ) );
     }
#endif

     switch (stack->bg.mode) {
          case DLBM_COLOR: {
               CoreSurface *dest  = state->destination;
               DFBColor    *color = &stack->bg.color;
               DFBRectangle rects[num];

               /* Set the background color. */
               if (DFB_PIXELFORMAT_IS_INDEXED( dest->format ))
                    dfb_state_set_color_index( state,
                                               dfb_palette_search( dest->palette, color->r,
                                                                   color->g, color->b, color->a ) );
               else
                    dfb_state_set_color( state, color );

               for (i=0; i<num; i++)
                    dfb_rectangle_from_region( &rects[i], &updates[i] );

               /* Simply fill the background. */
               dfb_gfxcard_fillrectangles( rects, num, state );

               break;
          }

          case DLBM_IMAGE: {
               CoreSurface *bg = stack->bg.image;

               /* Set blitting source. */
               state->source    = bg;
               state->modified |= SMF_SOURCE;

               /* Set blitting flags. */
               dfb_state_set_blitting_flags( state, DSBLIT_NOFX );

               /* Check the size of the background image. */
               if (bg->width == stack->width && bg->height == stack->height) {
                    for (i=0; i<num; i++) {
                         DFBRectangle dst = DFB_RECTANGLE_INIT_FROM_REGION( &updates[i] );

                         /* Simple blit for 100% fitting background image. */
                         dfb_gfxcard_blit( &dst, dst.x, dst.y, state );
                    }
               }
               else {
                    DFBRegion clip = state->clip;

                    for (i=0; i<num; i++) {
                         DFBRectangle src = { 0, 0, bg->width, bg->height };
                         DFBRectangle dst = { 0, 0, stack->width, stack->height };

                         /* Change clipping region. */
                         dfb_state_set_clip( state, &updates[i] );

                         /* Stretch blit for non fitting background images. */
                         dfb_gfxcard_stretchblit( &src, &dst, state );
                    }

                    /* Restore clipping region. */
                    dfb_state_set_clip( state, &clip );
               }

               /* Reset blitting source. */
               state->source    = NULL;
               state->modified |= SMF_SOURCE;

               break;
          }

          case DLBM_TILE: {
               CoreSurface  *bg   = stack->bg.image;
               DFBRegion     clip = state->clip;

               /* Set blitting source. */
               state->source    = bg;
               state->modified |= SMF_SOURCE;

               /* Set blitting flags. */
               dfb_state_set_blitting_flags( state, DSBLIT_NOFX );

               for (i=0; i<num; i++) {
                    DFBRectangle src = { 0, 0, bg->width, bg->height };

                    /* Change clipping region. */
                    dfb_state_set_clip( state, &updates[i] );

                    /* Tiled blit (aligned). */
                    dfb_gfxcard_tileblit( &src, 0, 0, stack->width, stack->height, state );
               }

               /* Restore clipping region. */
               dfb_state_set_clip( state, &clip );

               /* Reset blitting source. */
               state->source    = NULL;
               state->modified |= SMF_SOURCE;

               break;
          }

          case DLBM_DONTCARE:
               break;

          default:
               D_BUG( "unknown background mode" );
               break;
     }
}

/*
 * The root region is the desktop background.
 */
const StretRegionClass unique_root_region_class = {
     Update:   root_update
};

