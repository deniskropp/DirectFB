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

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/layers_internal.h>
#include <core/palette.h>
#include <core/state.h>
#include <core/windows.h>
#include <core/windows_internal.h>
#include <core/windowstack.h>

#include <misc/util.h>
#include <gfx/util.h>


static void repaint_stack_for_window( CoreWindowStack     *stack,
                                      CoreLayerRegion     *region,
                                      DFBRegion           *update,
                                      DFBSurfaceFlipFlags  flags,
                                      int                  window );

static void repaint_stack( CoreWindowStack     *stack,
                           CoreLayerRegion     *region,
                           DFBRegion           *update,
                           DFBSurfaceFlipFlags  flags );

/******************************************************************************/

void
dfb_window_repaint( CoreWindow          *window,
                    DFBRegion           *region,
                    DFBSurfaceFlipFlags  flags,
                    bool                 force_complete,
                    bool                 force_invisible )
{
     DFBRegion        reg;
     CoreWindowStack *stack = window->stack;

     if (!force_invisible && !VISIBLE_WINDOW(window))
          return;

     if (stack->hw_mode)
          return;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     if (region) {
          reg.x1 = region->x1 + window->x;
          reg.y1 = region->y1 + window->y;
          reg.x2 = region->x2 + window->x;
          reg.y2 = region->y2 + window->y;
     }
     else {
          reg.x1 = window->x;
          reg.y1 = window->y;
          reg.x2 = window->x + window->width  - 1;
          reg.y2 = window->y + window->height - 1;
     }

     if (force_complete)
          repaint_stack( stack, window->primary_region, &reg, flags );
     else
          repaint_stack_for_window( stack, window->primary_region, &reg, flags,
                                    dfb_windowstack_get_window_index(stack,
                                                                     window) );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );
}

void
dfb_windowstack_repaint_all( CoreWindowStack *stack )
{
     DFBRegion        region;
     CoreLayerRegion *primary;

     D_ASSERT( stack != NULL );

     if (stack->hw_mode)
          return;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     /* Get the primary region. */
     if (dfb_layer_context_get_primary_region( stack->context,
                                               false, &primary ))
     {
          dfb_windowstack_unlock( stack );
          return;
     }

     region.x1 = 0;
     region.y1 = 0;
     region.x2 = stack->width  - 1;
     region.y2 = stack->height - 1;

     repaint_stack( stack, primary, &region, 0 );

     /* Unref primary region. */
     dfb_layer_region_unref( primary );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );
}

/******************************************************************************/

static void
draw_window( CoreWindow *window, CardState *state,
             DFBRegion *region, bool alpha_channel )
{
     DFBRectangle            src;
     DFBSurfaceBlittingFlags flags = DSBLIT_NOFX;

     D_ASSERT( window != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( region != NULL );

     /* Initialize source rectangle. */
     dfb_rectangle_from_region( &src, region );

     /* Subtract window offset. */
     src.x -= window->x;
     src.y -= window->y;

     /* Use per pixel alpha blending. */
     if (alpha_channel && (window->options & DWOP_ALPHACHANNEL))
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
          if (state->src_colorkey != window->color_key) {
               state->src_colorkey  = window->color_key;
               state->modified     |= SMF_SRC_COLORKEY;
          }
     }

     /* Use automatic deinterlacing. */
     if (window->surface->caps & DSCAPS_INTERLACED)
          flags |= DSBLIT_DEINTERLACE;

     /* Set blitting flags. */
     if (state->blittingflags != flags) {
          state->blittingflags  = flags;
          state->modified      |= SMF_BLITTING_FLAGS;
     }

     /* Set blitting source. */
     state->source    = window->surface;
     state->modified |= SMF_SOURCE;

     /* Blit from the window to the region being updated. */
     dfb_gfxcard_blit( &src, region->x1, region->y1, state );

     /* Reset blitting source. */
     state->source    = NULL;
     state->modified |= SMF_SOURCE;
}

static void
draw_background( CoreWindowStack *stack, CardState *state, DFBRegion *region )
{
     DFBRectangle dst;

     D_ASSERT( stack != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( region != NULL );

     D_ASSERT( (stack->bg.mode != DLBM_IMAGE &&
                  stack->bg.mode != DLBM_TILE) || stack->bg.image != NULL );

     /* Initialize destination rectangle. */
     dfb_rectangle_from_region( &dst, region );

     switch (stack->bg.mode) {
          case DLBM_COLOR: {
               CoreSurface *dest  = state->destination;
               DFBColor    *color = &stack->bg.color;

               /* Set the background color. */
               state->color     = *color;
               state->modified |= SMF_COLOR;

               /* Lookup index of background color. */
               if (DFB_PIXELFORMAT_IS_INDEXED( dest->format ))
                    state->color_index = dfb_palette_search( dest->palette,
                                                             color->r,
                                                             color->g,
                                                             color->b,
                                                             color->a );

               /* Simply fill the background. */
               dfb_gfxcard_fillrectangle( &dst, state );

               break;
          }

          case DLBM_IMAGE: {
               CoreSurface *bg = stack->bg.image;

               /* Set blitting source. */
               state->source    = bg;
               state->modified |= SMF_SOURCE;

               /* Set blitting flags. */
               if (state->blittingflags != DSBLIT_NOFX) {
                    state->blittingflags  = DSBLIT_NOFX;
                    state->modified      |= SMF_BLITTING_FLAGS;
               }

               /* Check the size of the background image. */
               if (bg->width == stack->width && bg->height == stack->height) {
                    /* Simple blit for 100% fitting background image. */
                    dfb_gfxcard_blit( &dst, dst.x, dst.y, state );
               }
               else {
                    DFBRegion    clip = state->clip;
                    DFBRectangle src  = { 0, 0, bg->width, bg->height };

                    /* Change clipping region. */
                    state->clip      = *region;
                    state->modified |= SMF_CLIP;

                    /*
                     * Scale image to fill the whole screen
                     * clipped to the region being updated.
                     */
                    dst.x = 0;
                    dst.y = 0;
                    dst.w = stack->width;
                    dst.h = stack->height;

                    /* Stretch blit for non fitting background images. */
                    dfb_gfxcard_stretchblit( &src, &dst, state );

                    /* Restore clipping region. */
                    state->clip      = clip;
                    state->modified |= SMF_CLIP;
               }

               /* Reset blitting source. */
               state->source    = NULL;
               state->modified |= SMF_SOURCE;

               break;
          }

          case DLBM_TILE: {
               CoreSurface  *bg   = stack->bg.image;
               DFBRegion     clip = state->clip;
               DFBRectangle  src  = { 0, 0, bg->width, bg->height };

               /* Set blitting flags. */
               if (state->blittingflags != DSBLIT_NOFX) {
                    state->blittingflags  = DSBLIT_NOFX;
                    state->modified      |= SMF_BLITTING_FLAGS;
               }

               /* Set blitting source and clipping region. */
               state->source    = stack->bg.image;
               state->clip      = *region;
               state->modified |= SMF_SOURCE | SMF_CLIP;

               /* Tiled blit (aligned). */
               dfb_gfxcard_tileblit( &src,
                                     (region->x1 / src.w) * src.w,
                                     (region->y1 / src.h) * src.h,
                                     (region->x2 / src.w + 1) * src.w,
                                     (region->y2 / src.h + 1) * src.h,
                                     state );

               /* Reset blitting source and restore clipping region. */
               state->source    = NULL;
               state->clip      = clip;
               state->modified |= SMF_SOURCE | SMF_CLIP;

               break;
          }

          case DLBM_DONTCARE:
               break;

          default:
               D_BUG( "unknown background mode" );
               break;
     }
}

static void
update_region( CoreWindowStack *stack,
               CardState       *state,
               int              start,
               int              x1,
               int              y1,
               int              x2,
               int              y2 )
{
     int       i      = start;
     DFBRegion region = { x1, y1, x2, y2 };

     /* check for empty region */
     D_ASSERT (x1 <= x2  &&  y1 <= y2);

     while (i >= 0) {
          if (VISIBLE_WINDOW(stack->windows[i])) {
               int       wx2    = stack->windows[i]->x +
                                  stack->windows[i]->width - 1;
               int       wy2    = stack->windows[i]->y +
                                  stack->windows[i]->height - 1;

               if (dfb_region_intersect( &region, stack->windows[i]->x,
                                         stack->windows[i]->y, wx2, wy2 ))
                    break;
          }

          i--;
     }

     if (i >= 0) {
          CoreWindow *window = stack->windows[i];

          if ((window->options & DWOP_ALPHACHANNEL) &&
              (window->options & DWOP_OPAQUE_REGION))
          {
               DFBRegion opaque;

               opaque.x1 = window->x + window->opaque.x1;
               opaque.y1 = window->y + window->opaque.y1;
               opaque.x2 = window->x + window->opaque.x2;
               opaque.y2 = window->y + window->opaque.y2;

               if (!dfb_region_region_intersect( &opaque, &region )) {
                    update_region( stack, state, i-1, x1, y1, x2, y2 );

                    draw_window( window, state, &region, true );
               }
               else {
                    if ((window->opacity < 0xff) ||
                        (window->options & DWOP_COLORKEYING)) {
                         /* draw everything below */
                         update_region( stack, state, i-1, x1, y1, x2, y2 );
                    }
                    else {
                         /* left */
                         if (opaque.x1 != x1)
                              update_region( stack, state, i-1, x1, opaque.y1, opaque.x1-1, opaque.y2 );

                         /* upper */
                         if (opaque.y1 != y1)
                              update_region( stack, state, i-1, x1, y1, x2, opaque.y1-1 );

                         /* right */
                         if (opaque.x2 != x2)
                              update_region( stack, state, i-1, opaque.x2+1, opaque.y1, x2, opaque.y2 );

                         /* lower */
                         if (opaque.y2 != y2)
                              update_region( stack, state, i-1, x1, opaque.y2+1, x2, y2 );
                    }

                    /* left */
                    if (opaque.x1 != region.x1) {
                         DFBRegion r = { region.x1, opaque.y1,
                                         opaque.x1 - 1, opaque.y2};
                         draw_window( window, state, &r, true );
                    }

                    /* upper */
                    if (opaque.y1 != region.y1) {
                         DFBRegion r = { region.x1, region.y1,
                                         region.x2, opaque.y1 - 1};
                         draw_window( window, state, &r, true );
                    }

                    /* right */
                    if (opaque.x2 != region.x2) {
                         DFBRegion r = { opaque.x2 + 1, opaque.y1,
                                         region.x2, opaque.y2};
                         draw_window( window, state, &r, true );
                    }

                    /* lower */
                    if (opaque.y2 != region.y2) {
                         DFBRegion r = { region.x1, opaque.y2 + 1,
                                         region.x2, region.y2};
                         draw_window( window, state, &r, true );
                    }

                    /* inner */
                    draw_window( window, state, &opaque, false );
               }
          }
          else {
               if ((window->opacity < 0xff) ||
                   (window->options & (DWOP_COLORKEYING | DWOP_ALPHACHANNEL))) {
                    /* draw everything below */
                    update_region( stack, state, i-1, x1, y1, x2, y2 );
               }
               else {
                    /* left */
                    if (region.x1 != x1)
                         update_region( stack, state, i-1, x1, region.y1, region.x1-1, region.y2 );

                    /* upper */
                    if (region.y1 != y1)
                         update_region( stack, state, i-1, x1, y1, x2, region.y1-1 );

                    /* right */
                    if (region.x2 != x2)
                         update_region( stack, state, i-1, region.x2+1, region.y1, x2, region.y2 );

                    /* lower */
                    if (region.y2 != y2)
                         update_region( stack, state, i-1, x1, region.y2+1, x2, y2 );
               }

               draw_window( window, state, &region, true );
          }
     }
     else
          draw_background( stack, state, &region );
}

static void
repaint_stack( CoreWindowStack     *stack,
               CoreLayerRegion     *region,
               DFBRegion           *update,
               DFBSurfaceFlipFlags  flags )
{
     CoreLayer *layer;
     CardState *state;

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->context != NULL );
     D_ASSERT( region != NULL );

     layer = dfb_layer_at( stack->context->layer_id );
     state = &layer->state;

     if (!stack->active ||
         !dfb_region_intersect( update, 0, 0,
                                stack->width - 1, stack->height - 1 ))
     {
          return;
     }

     if (!region->surface)
          return;

     state->destination = region->surface;
     state->clip        = *update;
     state->modified   |= SMF_DESTINATION | SMF_CLIP;

     update_region( stack, state, stack->num_windows - 1,
                    update->x1, update->y1, update->x2, update->y2 );

     dfb_layer_region_flip_update( region, update, flags );

     state->destination = NULL;
}

/*
     recurseve procedure to call repaint
     skipping opaque windows that are above the window
     that changed
*/
static void
wind_of_change( CoreWindowStack     *stack,
                CoreLayerRegion     *region,
                DFBRegion           *update,
                DFBSurfaceFlipFlags  flags,
                int                  current,
                int                  changed )
{
     D_ASSERT(current>=changed);

     /*
          got to the window that changed, redraw.
     */
     if (current == changed)
          repaint_stack( stack, region, update, flags );
     else {
          CoreWindow *window = stack->windows[current];
          DFBRegion opaque;

          /*
               can skip opaque region
          */
          if ((
              //can skip all opaque window?
              (window->opacity == 0xff) &&
              !(window->options & (DWOP_COLORKEYING | DWOP_ALPHACHANNEL)) &&
              (opaque=*update,dfb_region_intersect( &opaque,
                                                    window->x, window->y,
                                                    window->x + window->width - 1,
                                                    window->y + window->height -1 ) )
              )||(
                 //can skip opaque region?
                 (window->options & DWOP_ALPHACHANNEL) &&
                 (window->options & DWOP_OPAQUE_REGION) &&
                 (window->opacity == 0xff) &&
                 !(window->options & DWOP_COLORKEYING) &&
                 (opaque=*update,dfb_region_intersect( &opaque,
                                                       window->x + window->opaque.x1,
                                                       window->y + window->opaque.y1,
                                                       window->x + window->opaque.x2,
                                                       window->y + window->opaque.y2 ))
                 )) {
               /* left */
               if (opaque.x1 != update->x1) {
                    DFBRegion left = { update->x1, opaque.y1, opaque.x1-1, opaque.y2};
                    wind_of_change( stack, region, &left, flags, current-1, changed );
               }
               /* upper */
               if (opaque.y1 != update->y1) {
                    DFBRegion upper = { update->x1, update->y1, update->x2, opaque.y1-1};
                    wind_of_change( stack, region, &upper, flags, current-1, changed );
               }
               /* right */
               if (opaque.x2 != update->x2) {
                    DFBRegion right = { opaque.x2+1, opaque.y1, update->x2, opaque.y2};
                    wind_of_change( stack, region, &right, flags, current-1, changed );
               }
               /* lower */
               if (opaque.y2 != update->y2) {
                    DFBRegion lower = { update->x1, opaque.y2+1, update->x2, update->y2};
                    wind_of_change( stack, region, &lower, flags, current-1, changed );
               }
          }
          /*
               pass through
          */
          else
               wind_of_change( stack, region, update, flags, current-1, changed );
     }
}

static void
repaint_stack_for_window( CoreWindowStack     *stack,
                          CoreLayerRegion     *region,
                          DFBRegion           *update,
                          DFBSurfaceFlipFlags  flags,
                          int                  window )
{
     if (stack->num_windows && window >= 0) {
          D_ASSERT(window < stack->num_windows);

          wind_of_change( stack, region, update, flags, stack->num_windows - 1, window );
     }
     else
          repaint_stack( stack, region, update, flags );
}

