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

#include <config.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/windows.h>

#include <misc/util.h>
#include <gfx/util.h>


static void repaint_stack_for_window( CoreWindowStack     *stack, 
                                      DFBRegion           *region,
                                      DFBSurfaceFlipFlags  flags,
                                      int                  window );

static void repaint_stack( CoreWindowStack     *stack,
                           DFBRegion           *region,
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
          repaint_stack( stack, &reg, flags );
     else
          repaint_stack_for_window( stack, &reg, flags,
                                    dfb_windowstack_get_window_index(window) );

     dfb_windowstack_unlock( stack );
}

void
dfb_windowstack_repaint_all( CoreWindowStack *stack )
{
     DFBRegion region;
     
     DFB_ASSERT( stack != NULL );
     
     if (stack->hw_mode)
          return;

     if (dfb_windowstack_lock( stack ))
          return;

     region.x1 = 0;
     region.y1 = 0;
     region.x2 = stack->width  - 1;
     region.y2 = stack->height - 1;
     
     repaint_stack( stack, &region, 0 );
          
     dfb_windowstack_unlock( stack );
}

void
dfb_windowstack_sync_buffers( CoreWindowStack *stack )
{
     DisplayLayer *layer;
     CoreSurface  *surface;

     DFB_ASSERT( stack != NULL );

     if (stack->hw_mode)
          return;
     
     dfb_windowstack_lock( stack );

     layer = dfb_layer_at( stack->layer_id );
     surface = dfb_layer_surface( layer );

     if (surface->caps & (DSCAPS_FLIPPING | DSCAPS_TRIPLE))
          dfb_gfx_copy(surface, surface, NULL);

     dfb_windowstack_unlock( stack );
}

/******************************************************************************/

static void
draw_window( CoreWindow *window, CardState *state,
             DFBRegion *region, bool alpha_channel )
{
     DFBRectangle            srect;
     DFBSurfaceBlittingFlags flags = DSBLIT_NOFX;

     DFB_ASSERT( window != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( region != NULL );

     srect.x = region->x1 - window->x;
     srect.y = region->y1 - window->y;
     srect.w = region->x2 - region->x1 + 1;
     srect.h = region->y2 - region->y1 + 1;

     if (alpha_channel && (window->options & DWOP_ALPHACHANNEL))
          flags |= DSBLIT_BLEND_ALPHACHANNEL;

     if (window->opacity != 0xFF) {
          flags |= DSBLIT_BLEND_COLORALPHA;

          if (state->color.a != window->opacity) {
               state->color.a = window->opacity;
               state->modified |= SMF_COLOR;
          }
     }

     if (window->options & DWOP_COLORKEYING) {
          flags |= DSBLIT_SRC_COLORKEY;

          if (state->src_colorkey != window->color_key) {
               state->src_colorkey = window->color_key;
               state->modified |= SMF_SRC_COLORKEY;
          }
     }

     if (window->surface->caps & DSCAPS_INTERLACED)
          flags |= DSBLIT_DEINTERLACE;

     if (state->blittingflags != flags) {
          state->blittingflags  = flags;
          state->modified      |= SMF_BLITTING_FLAGS;
     }

     state->source    = window->surface;
     state->modified |= SMF_SOURCE;

     dfb_gfxcard_blit( &srect, region->x1, region->y1, state );

     state->source    = NULL;
     state->modified |= SMF_SOURCE;
}

static void
draw_background( CoreWindowStack *stack, CardState *state, DFBRegion *region )
{
     DFB_ASSERT( stack != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( region != NULL );

     switch (stack->bg.mode) {
          case DLBM_COLOR: {
                    CoreSurface *dest  = state->destination;
                    DFBColor    *color = &stack->bg.color;
                    DFBRectangle rect  = { region->x1, region->y1,
                         region->x2 - region->x1 + 1,
                         region->y2 - region->y1 + 1};

                    state->color = *color;

                    if (DFB_PIXELFORMAT_IS_INDEXED( dest->format ))
                         state->color_index = dfb_palette_search( dest->palette,
                                                                  color->r,
                                                                  color->g,
                                                                  color->b,
                                                                  color->a );

                    state->modified |= SMF_COLOR;

                    dfb_gfxcard_fillrectangle( &rect, state );
                    break;
               }
          case DLBM_IMAGE: {
                    DFBRectangle rect = { region->x1, region->y1,
                         region->x2 - region->x1 + 1,
                         region->y2 - region->y1 + 1};

                    DFB_ASSERT( stack->bg.image != NULL );

                    if (state->blittingflags != DSBLIT_NOFX) {
                         state->blittingflags  = DSBLIT_NOFX;
                         state->modified      |= SMF_BLITTING_FLAGS;
                    }

                    state->source    = stack->bg.image;
                    state->modified |= SMF_SOURCE;

                    dfb_gfxcard_blit( &rect, region->x1, region->y1, state );

                    state->source    = NULL;
                    state->modified |= SMF_SOURCE;
                    break;
               }
          case DLBM_TILE: {
                    DFBRectangle rect = { 0, 0,
                         stack->bg.image->width,
                         stack->bg.image->height};

                    DFBRegion orig_clip = state->clip;

                    DFB_ASSERT( stack->bg.image != NULL );

                    if (state->blittingflags != DSBLIT_NOFX) {
                         state->blittingflags  = DSBLIT_NOFX;
                         state->modified      |= SMF_BLITTING_FLAGS;
                    }

                    state->source    = stack->bg.image;
                    state->clip.x1   = region->x1;
                    state->clip.y1   = region->y1;
                    state->clip.x2   = region->x2;
                    state->clip.y2   = region->y2;
                    state->modified |= SMF_SOURCE | SMF_CLIP;

                    dfb_gfxcard_tileblit( &rect,
                                          (region->x1 / rect.w) * rect.w,
                                          (region->y1 / rect.h) * rect.h,
                                          (region->x2 / rect.w + 1) * rect.w,
                                          (region->y2 / rect.h + 1) * rect.h,
                                          state );

                    state->source    = NULL;
                    state->clip      = orig_clip;
                    state->modified |= SMF_SOURCE | SMF_CLIP;
                    break;
               }
          case DLBM_DONTCARE:
               break;
          default:
               BUG( "unknown background mode" );
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
     DFBRegion region = { x1, y1, x2, y2};

     /* check for empty region */
     DFB_ASSERT (x1 <= x2  &&  y1 <= y2);

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
              (window->options & DWOP_OPAQUE_REGION)) {
               DFBRegion opaque = region;

               if (!dfb_region_intersect( &opaque,
                                          window->x + window->opaque.x1,
                                          window->y + window->opaque.y1,
                                          window->x + window->opaque.x2,
                                          window->y + window->opaque.y2 )) {
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
               DFBRegion           *region,
               DFBSurfaceFlipFlags  flags )
{
     DisplayLayer *layer   = dfb_layer_at( stack->layer_id );
     CoreSurface  *surface = dfb_layer_surface( layer );
     CardState    *state   = dfb_layer_state( layer );

     if (!surface)
          return;
     
     if (!dfb_region_intersect( region, 0, 0,
                                surface->width - 1, surface->height - 1 ))
          return;

     if (dfb_layer_lease( layer ))
          return;

     state->destination = surface;
     state->clip        = *region;
     state->modified   |= SMF_DESTINATION | SMF_CLIP;

     update_region( stack, state, stack->num_windows - 1,
                    region->x1, region->y1, region->x2, region->y2 );

     if (surface->caps & (DSCAPS_FLIPPING | DSCAPS_TRIPLE)) {
          if (region->x1 == 0 &&
              region->y1 == 0 &&
              region->x2 == surface->width - 1 &&
              region->y2 == surface->height - 1) {
               dfb_layer_flip_buffers( layer, flags );
          }
          else {
               DFBRectangle rect = { region->x1, region->y1,
                    region->x2 - region->x1 + 1,
                    region->y2 - region->y1 + 1};

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC)
                    dfb_layer_wait_vsync( layer );

               dfb_back_to_front_copy( surface, &rect );
               dfb_layer_update_region( layer, region, flags );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAIT)
                    dfb_layer_wait_vsync( layer );
          }
     }
     else
          dfb_layer_update_region( layer, region, flags );

     dfb_layer_release( layer, false );

     state->destination = NULL;
}

/*
     recurseve procedure to call repaint
     skipping opaque windows that are above the window
     that changed
*/
static void
wind_of_change( CoreWindowStack     *stack,
                DFBRegion           *region,
                DFBSurfaceFlipFlags  flags,
                int                  current,
                int                  changed )
{
     DFB_ASSERT(current>=changed);

     /*
          got to the window that changed, redraw.
     */
     if (current == changed)
          repaint_stack( stack, region, flags );
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
              (opaque=*region,dfb_region_intersect( &opaque,
                                                    window->x, window->y,
                                                    window->x + window->width - 1,
                                                    window->y + window->height -1 ) ) 
              )||(
                 //can skip opaque region?
                 (window->options & DWOP_ALPHACHANNEL) &&
                 (window->options & DWOP_OPAQUE_REGION) &&
                 (window->opacity == 0xff) &&
                 !(window->options & DWOP_COLORKEYING) &&
                 (opaque=*region,dfb_region_intersect( &opaque,
                                                       window->x + window->opaque.x1,
                                                       window->y + window->opaque.y1,
                                                       window->x + window->opaque.x2,
                                                       window->y + window->opaque.y2 )) 
                 )) {
               /* left */
               if (opaque.x1 != region->x1) {
                    DFBRegion update = { region->x1, opaque.y1, opaque.x1-1, opaque.y2};
                    wind_of_change( stack, &update, flags, current-1, changed );
               }
               /* upper */
               if (opaque.y1 != region->y1) {
                    DFBRegion update = { region->x1, region->y1, region->x2, opaque.y1-1};
                    wind_of_change( stack, &update, flags, current-1, changed );
               }
               /* right */
               if (opaque.x2 != region->x2) {
                    DFBRegion update = { opaque.x2+1, opaque.y1, region->x2, opaque.y2};
                    wind_of_change( stack, &update, flags, current-1, changed );
               }
               /* lower */
               if (opaque.y2 != region->y2) {
                    DFBRegion update = { region->x1, opaque.y2+1, region->x2, region->y2};
                    wind_of_change( stack, &update, flags, current-1, changed );
               }
          }
          /*
               pass through
          */
          else
               wind_of_change( stack, region, flags, current-1, changed );
     }
}

static void
repaint_stack_for_window( CoreWindowStack     *stack,
                          DFBRegion           *region,
                          DFBSurfaceFlipFlags  flags,
                          int                  window )
{
     if (stack->num_windows && window >= 0) {
          DFB_ASSERT(window < stack->num_windows);

          wind_of_change( stack, region, flags, stack->num_windows - 1, window ); 
     }
     else
          repaint_stack( stack, region, flags );     
}

