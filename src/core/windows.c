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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <malloc.h>

#include <pthread.h>

#include <core/fusion/shmalloc.h>

#include "directfb.h"

#include "coredefs.h"
#include "coretypes.h"

#include "layers.h"
#include "gfxcard.h"
#include "input.h"
#include "state.h"

#include "windows.h"

#include "misc/conf.h"
#include "misc/util.h"
#include "misc/mem.h"
#include "gfx/util.h"


static void repaint_stack( CoreWindowStack *stack, DFBRegion *region );
static CoreWindow* window_at_pointer( CoreWindowStack *stack, int x, int y );
static int  handle_enter_leave_focus( CoreWindowStack *stack );
static void handle_wheel( CoreWindowStack *stack, int z );
static DFBWindowID new_window_id( CoreWindowStack *stack );

/*
 * Returns the stacking index of the window within its stack
 * or -1 if not found.
 */
static inline int
get_window_index( CoreWindow *window );

/*
 * Moves the window within the stack from the old stacking index
 * to the new one. Returns if restacking actually happened.
 */
static bool
restack_window( CoreWindowStack *stack, int old_index, int new_index );

/*
 * Called by restacking functions. If the window is visible the region
 * of the stack will be updated and the focus validated.
 */
static void
window_restacked( CoreWindow *window );

static ReactionResult
stack_inputdevice_react( const void *msg_data,
                         void       *ctx );
static DFBEnumerationResult
stack_attach_devices( InputDevice *device,
                      void        *ctx );
static DFBEnumerationResult
stack_detach_devices( InputDevice *device,
                      void        *ctx );

CoreWindowStack*
dfb_windowstack_new( DisplayLayer *layer )
{
     CardCapabilities  caps;
     CoreWindowStack  *stack;

     stack = (CoreWindowStack*) shcalloc( 1, sizeof(CoreWindowStack) );

     stack->layer = layer->shared;

     stack->wsp_opaque = stack->wsp_alpha = CSP_SYSTEMONLY;

     caps = dfb_gfxcard_capabilities();

     if (caps.accel & DFXL_BLIT) {
          stack->wsp_opaque = CSP_VIDEOHIGH;
          if (caps.blitting & DSBLIT_BLEND_ALPHACHANNEL)
               stack->wsp_alpha = CSP_VIDEOHIGH;
     }

     if (dfb_config->window_policy != -1) {
          stack->wsp_opaque = stack->wsp_alpha = dfb_config->window_policy;
     }

     skirmish_init( &stack->update );

     dfb_input_enumerate_devices( stack_attach_devices, stack );

     stack->cursor_region.x1 = 0;
     stack->cursor_region.y1 = 0;
     stack->cursor_region.x2 = layer->shared->width - 1;
     stack->cursor_region.y2 = layer->shared->height - 1;

     /* initialize state for repaints */
     dfb_state_init( &stack->state );
     stack->state.modified  = SMF_ALL;
     stack->state.src_blend = DSBF_SRCALPHA;
     stack->state.dst_blend = DSBF_INVSRCALPHA;
     dfb_state_set_destination( &stack->state, layer->shared->surface );

     return stack;
}

void
dfb_windowstack_destroy( CoreWindowStack *stack )
{
     int i;

     dfb_state_set_destination( &stack->state, NULL );

     dfb_input_enumerate_devices( stack_detach_devices, stack );

     skirmish_destroy( &stack->update );

     for (i=0; i<stack->num_windows; i++)
          dfb_window_destroy( stack->windows[i] );

     if (stack->windows)
          shfree( stack->windows );

     dfb_state_destroy( &stack->state );

     shfree( stack );
}

void
dfb_window_insert( CoreWindow *window,
                   int         before )
{
     int i;
     CoreWindowStack *stack = window->stack;

     if (before < 0  ||  before > stack->num_windows)
          before = stack->num_windows;

     skirmish_prevail( &stack->update );

     stack->windows = shrealloc( stack->windows,
                                 sizeof(CoreWindow*) * (stack->num_windows+1) );

     for (i=stack->num_windows; i>before; i--)
          stack->windows[i] = stack->windows[i-1];

     stack->windows[before] = window;

     stack->num_windows++;

     skirmish_dismiss( &stack->update );

     if (!(window->caps & DWHC_GHOST)) {
          DFBWindowEvent evt;

          evt.type = DWET_POSITION_SIZE;
          evt.x = window->x;
          evt.y = window->y;
          evt.w = window->width;
          evt.h = window->height;
          dfb_window_dispatch( window, &evt );
     }

     if (window->opacity)
          handle_enter_leave_focus( stack );
}

void
dfb_window_remove( CoreWindow *window )
{
     int i;
     CoreWindowStack *stack = window->stack;
     DFBRegion region = { window->x, window->y,
                          window->x + window->width - 1,
                          window->y + window->height - 1 };

     DFB_ASSERT( window->stack != NULL );

     skirmish_prevail( &stack->update );

     if (stack->entered_window == window)
          stack->entered_window = NULL;

     if (stack->focused_window == window)
          stack->focused_window = NULL;

     if (stack->keyboard_window == window)
          stack->keyboard_window = NULL;

     if (stack->pointer_window == window)
          stack->pointer_window = NULL;

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i] == window)
               break;

     if (i < stack->num_windows) {
          stack->num_windows--;

          for (; i<stack->num_windows; i++)
               stack->windows[i] = stack->windows[i+1];

          if (stack->windows) {
               stack->windows =
                    shrealloc( stack->windows,
                               sizeof(CoreWindow*) * stack->num_windows );
          }
          else {
               shfree( stack->windows );
               stack->windows = NULL;
          }
     }

     window->stack = NULL;

     skirmish_dismiss( &stack->update );

     repaint_stack( stack, &region );

     if (window->opacity)
          handle_enter_leave_focus( stack );


     skirmish_prevail( &stack->update );

     if (!stack->focused_window) {
          for (i=stack->num_windows-1; i>=0; i--) {
               if (!(stack->windows[i]->caps & DWHC_GHOST)) {
                    skirmish_dismiss( &stack->update );
                    dfb_window_request_focus( stack->windows[i] );
                    skirmish_prevail( &stack->update );
                    break;
               }
          }
     }

     skirmish_dismiss( &stack->update );
}

CoreWindow*
dfb_window_create( CoreWindowStack       *stack,
                   int                    x,
                   int                    y,
                   unsigned int           width,
                   unsigned int           height,
                   DFBWindowCapabilities  caps,
                   DFBSurfacePixelFormat  pixelformat )
{
     DFBResult               ret;
     CoreSurface            *surface;
     int                     surface_policy;
     DFBSurfacePixelFormat   surface_format;
     DFBSurfaceCapabilities  surface_caps;
     CoreWindow             *window;

     if (caps & DWCAPS_ALPHACHANNEL) {
          if (pixelformat != DSPF_UNKNOWN && pixelformat != DSPF_ARGB)
               return NULL;

          surface_policy = stack->wsp_alpha;
          surface_format = DSPF_ARGB;
     }
     else {
          surface_policy = stack->wsp_opaque;

          if (pixelformat != DSPF_UNKNOWN)
               surface_format = pixelformat;
          else
               surface_format = stack->layer->surface->format;
     }

     if (caps & DWCAPS_DOUBLEBUFFER)
          surface_caps = DSCAPS_FLIPPING;
     else
          surface_caps = DSCAPS_NONE;

     ret = dfb_surface_create( width, height, surface_format, surface_policy,
                               surface_caps, &surface );
     if (ret)
          return NULL;

     window = (CoreWindow*) shcalloc( 1, sizeof(CoreWindow) );

     window->id      = new_window_id( stack );

     window->surface = surface;

     window->x       = x;
     window->y       = y;
     window->width   = width;
     window->height  = height;

     window->caps    = caps;
     window->opacity = 0;

     window->stack   = stack;

     window->reactor = reactor_new(sizeof(DFBWindowEvent));

     return window;
}

void
dfb_window_init( CoreWindow *window )
{
     int i;
     CoreWindowStack *stack = window->stack;

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i]->caps & DWHC_GHOST)
               break;

     dfb_window_insert( window, i );
}

void
dfb_window_destroy( CoreWindow *window )
{
     DFBWindowEvent evt;

     evt.type = DWET_DESTROYED;
     dfb_window_dispatch( window, &evt );

     dfb_surface_destroy( window->surface );

     reactor_free( window->reactor );

     shfree( window );
}

void
dfb_window_raise( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     skirmish_prevail( &stack->update );

     index = get_window_index( window );
     if (index < 0)
          return;

     update = restack_window( stack, index, index + 1 );

     skirmish_dismiss( &stack->update );

     if (update)
          window_restacked( window );
}

void
dfb_window_lower( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     skirmish_prevail( &stack->update );

     index = get_window_index( window );
     if (index < 0)
          return;

     update = restack_window( stack, index, index - 1 );

     skirmish_dismiss( &stack->update );

     if (update)
          window_restacked( window );
}

void
dfb_window_raisetotop( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     skirmish_prevail( &stack->update );

     index = get_window_index( window );
     if (index < 0)
          return;

     update = restack_window( stack, index, stack->num_windows - 1 );

     skirmish_dismiss( &stack->update );

     if (update)
          window_restacked( window );
}

void
dfb_window_lowertobottom( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     skirmish_prevail( &stack->update );

     index = get_window_index( window );
     if (index < 0)
          return;

     update = restack_window( stack, index, 0 );

     skirmish_dismiss( &stack->update );

     if (update)
          window_restacked( window );
}

void
dfb_window_putatop( CoreWindow *window,
                    CoreWindow *lower )
{
     int              index;
     int              lower_index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     skirmish_prevail( &stack->update );

     index = get_window_index( window );
     if (index < 0)
          return;

     lower_index = get_window_index( lower );
     if (lower_index < 0)
          return;

     if (index < lower_index)
          update = restack_window( stack, index, lower_index );
     else
          update = restack_window( stack, index, lower_index + 1 );

     skirmish_dismiss( &stack->update );

     if (update)
          window_restacked( window );
}

void
dfb_window_putbelow( CoreWindow *window,
                     CoreWindow *upper )
{
     int              index;
     int              upper_index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     skirmish_prevail( &stack->update );

     index = get_window_index( window );
     if (index < 0)
          return;

     upper_index = get_window_index( upper );
     if (upper_index < 0)
          return;

     if (index > upper_index)
          update = restack_window( stack, index, upper_index );
     else
          update = restack_window( stack, index, upper_index - 1 );

     skirmish_dismiss( &stack->update );

     if (update)
          window_restacked( window );
}

void
dfb_window_move( CoreWindow *window,
                 int         dx,
                 int         dy )
{
     CoreWindowStack *stack = window->stack;

     window->x += dx;
     window->y += dy;

     if (window->opacity) {
          DFBRegion region = { window->x, window->y,
                               window->x + window->width - 1,
                               window->y + window->height - 1 };

          if (dx > 0)
               region.x1 -= dx;
          else if (dx < 0)
               region.x2 -= dx;

          if (dy > 0)
               region.y1 -= dy;
          else if (dy < 0)
               region.y2 -= dy;

          repaint_stack( stack, &region );
     }

     if (!(window->caps & DWHC_GHOST)) {
          DFBWindowEvent evt;

          evt.type = DWET_POSITION;
          evt.x = window->x;
          evt.y = window->y;
          dfb_window_dispatch( window, &evt );
     }
}

DFBResult
dfb_window_resize( CoreWindow   *window,
                   unsigned int  width,
                   unsigned int  height )
{
     DFBResult        ret;
     CoreWindowStack *stack = window->stack;
     int              ow    = window->width;
     int              oh    = window->height;

     ret = dfb_surface_reformat( window->surface,
                                 width, height,
                                 window->surface->format );
     if (ret)
          return ret;

     window->width = window->surface->width;
     window->height = window->surface->height;

     if (window->opacity) {
          if (ow > window->width) {
               DFBRegion region = { window->x + window->width, window->y,
                                    window->x + ow - 1,
                                    window->y + MIN(window->height, oh) - 1 };
								
               repaint_stack( stack, &region );
          }

          if (oh > window->height) {
               DFBRegion region = { window->x, window->y + window->height,
                                    window->x + MAX(window->width, ow) - 1,
                                    window->y + oh - 1 };
		
               repaint_stack( stack, &region );
          }
     }

     if (!(window->caps & DWHC_GHOST)) {
          DFBWindowEvent evt;

          evt.type = DWET_SIZE;
          evt.w = window->width;
          evt.h = window->height;
          dfb_window_dispatch( window, &evt );
     }

     return DFB_OK;
}

void
dfb_window_set_opacity( CoreWindow *window,
                        __u8        opacity )
{
     int old_opacity = window->opacity;
     CoreWindowStack *stack = window->stack;

     if (dfb_config->no_window_opacity && opacity)
          opacity = 0xFF;

     if (old_opacity != opacity) {
          DFBRegion region = { window->x, window->y,
                               window->x + window->width - 1,
                               window->y + window->height - 1 };

          window->opacity = opacity;

          repaint_stack( stack, &region );

          if ((old_opacity && !opacity) || (!old_opacity && opacity))
               handle_enter_leave_focus( stack );
     }
}

void
dfb_window_repaint( CoreWindow *window,
                    DFBRegion  *region )
{
     CoreWindowStack *stack = window->stack;

     if (!window->opacity)
          return;

     if (region) {
          region->x1 += window->x;
          region->x2 += window->x;
          region->y1 += window->y;
          region->y2 += window->y;

          repaint_stack( stack, region );
     }
     else {
          DFBRegion reg = { window->x, window->y,
                            window->x + window->width - 1,
                            window->y + window->height - 1 };

          repaint_stack( stack, &reg );
     }
}

DFBResult
dfb_window_grab_keyboard( CoreWindow *window )
{
     DFBResult retval = DFB_OK;
     CoreWindowStack *stack = window->stack;

     skirmish_prevail( &stack->update );

     if (stack->keyboard_window)
          retval = DFB_LOCKED;
     else
          stack->keyboard_window = window;

     skirmish_dismiss( &stack->update );

     return retval;
}

DFBResult
dfb_window_ungrab_keyboard( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     skirmish_prevail( &stack->update );

     if (stack->keyboard_window == window)
          stack->keyboard_window = NULL;

     skirmish_dismiss( &stack->update );

     return DFB_OK;
}

DFBResult
dfb_window_grab_pointer( CoreWindow *window )
{
     DFBResult retval = DFB_OK;
     CoreWindowStack *stack = window->stack;

     skirmish_prevail( &stack->update );

     if (stack->pointer_window)
          retval = DFB_LOCKED;
     else
          stack->pointer_window = window;

     skirmish_dismiss( &stack->update );

     return retval;
}

DFBResult
dfb_window_ungrab_pointer( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     skirmish_prevail( &stack->update );

     if (stack->pointer_window == window)
          stack->pointer_window = NULL;

     skirmish_dismiss( &stack->update );

     handle_enter_leave_focus( stack );

     return DFB_OK;
}

void
dfb_window_attach( CoreWindow *window,
                   React       react,
                   void       *ctx )
{
     reactor_attach( window->reactor, react, ctx );
}

void
dfb_window_detach( CoreWindow *window,
                   React       react,
                   void       *ctx )
{
     reactor_detach( window->reactor, react, ctx );
}

void
dfb_window_dispatch( CoreWindow     *window,
                     DFBWindowEvent *event )
{
     event->window_id = window->id;

     reactor_dispatch( window->reactor, event, true );
}

void
dfb_window_request_focus( CoreWindow *window )
{
     DFBWindowEvent evt;

     CoreWindowStack *stack   = window->stack;
     CoreWindow      *current = stack->focused_window;

     DFB_ASSERT( !(window->caps & DWHC_GHOST) );

     if (current == window)
          return;

     if (current) {
          evt.type = DWET_LOSTFOCUS;
          dfb_window_dispatch( current, &evt );
     }

     evt.type = DWET_GOTFOCUS;
     dfb_window_dispatch( window, &evt );

     stack->focused_window = window;
}

void
dfb_windowstack_repaint_all( CoreWindowStack *stack )
{
     CoreSurface *surface = stack->layer->surface;
     DFBRegion    region  = { 0, 0, surface->width -1, surface->height - 1 };

     repaint_stack( stack, &region );
}

/*
 * internals
 */

static DFBEnumerationResult
stack_attach_devices( InputDevice *device,
                      void        *ctx )
{
     dfb_input_attach( device, stack_inputdevice_react, ctx );

     return DFENUM_OK;
}

static DFBEnumerationResult
stack_detach_devices( InputDevice *device,
                      void        *ctx )
{
     dfb_input_detach( device, stack_inputdevice_react, ctx );

     return DFENUM_OK;
}

#define TRANSPARENT_WINDOW(w) ((w)->opacity < 0xff || \
                               (w)->caps & DWCAPS_ALPHACHANNEL || \
                               (w)->options & DWOP_COLORKEYING)

static void
update_region( CoreWindowStack *stack,
               int              start,
               int              x1,
               int              y1,
               int              x2,
               int              y2 )
{
     int                 i      = start;
     unsigned int        edges  = 0;
     DFBRegion           region = { x1, y1, x2, y2 };
     DisplayLayerShared *layer  = stack->layer;

     /* check for empty region */
     DFB_ASSERT (x1 <= x2  &&  y1 <= y2);

     while (i >= 0) {
          if (stack->windows[i]->opacity > 0) {
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
          if (region.x1 != x1)
               edges |= 0x1;
          if (region.y1 != y1)
               edges |= 0x2;
          if (region.x2 != x2)
               edges |= 0x4;
          if (region.y2 != y2)
               edges |= 0x8;

          if (TRANSPARENT_WINDOW(stack->windows[i]))
               update_region( stack, i-1, x1, y1, x2, y2 );
          else if (edges) {
               /* left */
               if (edges & 0x1)
                    update_region( stack, i-1, x1, region.y1, region.x1-1, region.y2 );

               /* upper */
               if (edges & 0x2)
                    update_region( stack, i-1, x1, y1, x2, region.y1-1 );

               /* right */
               if (edges & 0x4)
                    update_region( stack, i-1, region.x2+1, region.y1, x2, region.y2 );

               /* lower */
               if (edges & 0x8)
                    update_region( stack, i-1, x1, region.y2+1, x2, y2 );
          }

          {
               CoreWindow              *window = stack->windows[i];
               DFBSurfaceBlittingFlags  flags  = DSBLIT_NOFX;
               DFBRectangle             srect  = { region.x1 - window->x,
                                                   region.y1 - window->y,
                                                   region.x2 - region.x1 + 1,
                                                   region.y2 - region.y1 + 1 };

               if (window->caps & DWCAPS_ALPHACHANNEL)
                    flags |= DSBLIT_BLEND_ALPHACHANNEL;

               if (window->opacity != 0xFF) {
                    flags |= DSBLIT_BLEND_COLORALPHA;

                    if (stack->state.color.a != window->opacity) {
                         stack->state.color.a = window->opacity;
                         stack->state.modified |= SMF_COLOR;
                    }
               }

               if (window->options & DWOP_COLORKEYING) {
                    flags |= DSBLIT_SRC_COLORKEY;

                    if (stack->state.src_colorkey != window->color_key) {
                         stack->state.src_colorkey = window->color_key;
                         stack->state.modified |= SMF_SRC_COLORKEY;
                    }
               }

               if (stack->state.blittingflags != flags) {
                    stack->state.blittingflags  = flags;
                    stack->state.modified      |= SMF_BLITTING_FLAGS;
               }

               stack->state.source    = window->surface;
               stack->state.modified |= SMF_SOURCE;

               dfb_gfxcard_blit( &srect, region.x1, region.y1, &stack->state );
          }
     }
     else {
          if (layer->bg.mode != DLBM_DONTCARE) {

               switch (layer->bg.mode) {
                    case DLBM_COLOR: {
                         DFBRectangle rect = { x1, y1, x2 - x1 + 1, y2 - y1 + 1 };

                         stack->state.color     = layer->bg.color;
                         stack->state.modified |= SMF_COLOR;

                         dfb_gfxcard_fillrectangle( &rect, &stack->state );
                         break;
                    }
                    case DLBM_IMAGE: {
                         DFBRectangle rect = { x1, y1, x2 - x1 + 1, y2 - y1 + 1 };

                         if (stack->state.blittingflags != DSBLIT_NOFX) {
                              stack->state.blittingflags  = DSBLIT_NOFX;
                              stack->state.modified      |= SMF_BLITTING_FLAGS;
                         }

                         stack->state.source    = layer->bg.image;
                         stack->state.modified |= SMF_SOURCE;

                         dfb_gfxcard_blit( &rect, x1, y1, &stack->state );
                         break;
                    }
                    case DLBM_TILE: {
                         DFBRectangle rect =
                         { 0, 0, layer->bg.image->width, layer->bg.image->height };

                         if (stack->state.blittingflags != DSBLIT_NOFX) {
                              stack->state.blittingflags  = DSBLIT_NOFX;
                              stack->state.modified      |= SMF_BLITTING_FLAGS;
                         }

                         stack->state.source    = layer->bg.image;
                         stack->state.modified |= SMF_SOURCE;

                         dfb_gfxcard_tileblit( &rect,
                                               (x1 / rect.w) * rect.w,
                                               (y1 / rect.h) * rect.h,
                                               (x2 / rect.w + 1) * rect.w,
                                               (y2 / rect.h + 1) * rect.h,
                                               &stack->state );
                         break;
                    }
                    default:
                         ;
               }
          }
     }
}

static void
repaint_stack( CoreWindowStack *stack,
               DFBRegion       *region )
{
     DisplayLayerShared *layer   = stack->layer;
     CoreSurface        *surface = layer->surface;

     if (layer->exclusive)
          return;

     if (!dfb_region_intersect( region, 0, 0,
                                surface->width - 1, surface->height - 1 ))
          return;

     skirmish_prevail( &stack->update );

     stack->state.clip      = *region;
     stack->state.modified |= SMF_CLIP;

     update_region( stack, stack->num_windows - 1,
                    region->x1, region->y1, region->x2, region->y2 );

     if (surface->caps & DSCAPS_FLIPPING) {
          if (region->x1 == 0 &&
              region->y1 == 0 &&
              region->x2 == layer->width - 1 &&
              region->y2 == layer->height - 1 && 0)
          {
               /* FIXME */
               //layer->FlipBuffers( layer );
          }
          else {
               DFBRectangle rect = { region->x1, region->y1,
                                     region->x2 - region->x1 + 1,
                                     region->y2 - region->y1 + 1 };

               dfb_back_to_front_copy( surface, &rect );
          }
     }

     skirmish_dismiss( &stack->update );
}

static CoreWindow*
window_at_pointer( CoreWindowStack *stack,
                   int              x,
                   int              y )
{
     int i;

     if (x < 0)
          x = stack->cx;
     if (y < 0)
          y = stack->cy;

     for (i=stack->num_windows-1; i>=0; i--) {
          CoreWindow *w = stack->windows[i];

          if (!(w->caps & DWHC_GHOST)  &&  w->opacity  &&
                                           x >= w->x  &&  x < w->x+w->width &&
                                           y >= w->y  &&  y < w->y+w->height)
               return w;
     }

     return NULL;
}

static ReactionResult
stack_inputdevice_react( const void *msg_data,
                         void       *ctx )
{
     const DFBInputEvent *evt = (DFBInputEvent*)msg_data;

     DFBWindowEvent   we;
     CoreWindow      *window = NULL;
     CoreWindowStack *stack  = (CoreWindowStack*)ctx;

     if (stack->layer->exclusive)
          return RS_OK;

     if (stack->wm_hack) {
          switch (evt->type) {
               case DIET_KEYRELEASE:
                    switch (evt->keycode) {
                         case DIKC_CAPSLOCK:
                              stack->wm_hack = 0;
                              handle_enter_leave_focus( stack );
                              return RS_OK;

                         case DIKC_CTRL:
                              stack->wm_hack = 1;
                              return RS_OK;

                         default:
                              ;
                    }
                    break;

               case DIET_KEYPRESS:
                    switch (evt->keycode) {
                         case DIKC_CTRL:
                              stack->wm_hack = 2;
                              return RS_OK;

                         case DIKC_C:
                              if (stack->entered_window) {
                                   DFBWindowEvent evt;
                                   evt.type = DWET_CLOSE;
                                   dfb_window_dispatch( stack->entered_window, &evt );
                              }
                              return RS_OK;

                         case DIKC_D:
                              if (stack->entered_window) {
                                   CoreWindow *window = stack->entered_window;
                                   dfb_window_remove( window );
                                   dfb_window_destroy( window );
                              }
                              return RS_OK;

                         default:
                              ;
                    }
                    break;

               case DIET_BUTTONRELEASE:
                    return RS_OK;

               case DIET_BUTTONPRESS:
                    if (stack->entered_window)
                         dfb_window_raisetotop( stack->entered_window );
                    return RS_OK;

               default:
                    ;
          }
     }

     switch (evt->type) {
          case DIET_KEYPRESS:
               if (evt->keycode == DIKC_CAPSLOCK) {
                    stack->wm_hack = 1;
                    break;
               }
               /* fall through */
          case DIET_KEYRELEASE:
               window = (stack->keyboard_window ?
                         stack->keyboard_window : stack->focused_window);

               if (window) {
                    we.type = (evt->type == DIET_KEYPRESS) ? DWET_KEYDOWN :
                                                             DWET_KEYUP;
                    we.key_ascii   = evt->key_ascii;
                    we.key_unicode = evt->key_unicode;
                    we.keycode     = evt->keycode;
                    we.modifiers   = evt->modifiers;
                    we.locks   = evt->locks;

                    dfb_window_dispatch( window, &we );
               }

               break;
          case DIET_BUTTONPRESS:
          case DIET_BUTTONRELEASE:
               if (!stack->cursor)
                    break;

               window = (stack->pointer_window ?
                         stack->pointer_window : stack->entered_window);

               if (window) {
                    we.type = (evt->type == DIET_BUTTONPRESS) ? DWET_BUTTONDOWN :
                                                                DWET_BUTTONUP;
                    we.button = evt->button;
                    we.cx     = stack->cx;
                    we.cy     = stack->cy;
                    we.x      = we.cx - window->x;
                    we.y      = we.cy - window->y;

                    dfb_window_dispatch( window, &we );
               }

               break;
          case DIET_AXISMOTION:
               if (evt->flags & DIEF_AXISREL) {
                    switch (evt->axis) {
                         case DIAI_X:
                              dfb_windowstack_handle_motion( stack,
                                                             evt->axisrel, 0 );
                              break;
                         case DIAI_Y:
                              dfb_windowstack_handle_motion( stack,
                                                             0, evt->axisrel );
                              break;
                         case DIAI_Z:
                              handle_wheel( stack, evt->axisrel );
                              break;
                         default:
                              return RS_OK;
                    }
               }
               else if (evt->flags & DIEF_AXISABS) {
                    switch (evt->axis) {
                         case DIAI_X:
                              dfb_windowstack_handle_motion( stack,
                                                             evt->axisabs - stack->cx, 0 );
                              break;
                         case DIAI_Y:
                              dfb_windowstack_handle_motion( stack, 0,
                                                             evt->axisabs - stack->cy);
                              break;
                         default:
                              return RS_OK;
                    }
               }
               break;
          default:
               break;
     }

     return RS_OK;
}

void
dfb_windowstack_handle_motion( CoreWindowStack *stack,
                               int              dx,
                               int              dy )
{
     int new_cx, new_cy;
     DFBWindowEvent we;

     if (!stack->cursor)
          return;

     new_cx = MIN( stack->cx + dx, stack->cursor_region.x2);
     new_cy = MIN( stack->cy + dy, stack->cursor_region.y2);

     new_cx = MAX( new_cx, stack->cursor_region.x1 );
     new_cy = MAX( new_cy, stack->cursor_region.y1 );

     if (new_cx == stack->cx  &&  new_cy == stack->cy)
          return;

     dx = new_cx - stack->cx;
     dy = new_cy - stack->cy;

     stack->cx = new_cx;
     stack->cy = new_cy;


     dfb_window_move( stack->cursor_window, dx, dy );

     switch (stack->wm_hack) {
          case 2:
               if (stack->entered_window) {
                    CoreWindow *window = stack->entered_window;
                    int         width  = window->width + dx;
                    int         height = window->height + dy;

                    if (width  <   10) width  = 10;
                    if (height <   10) height = 10;
                    if (width  > 2048) width  = 2048;
                    if (height > 2048) height = 2048;

                    if (width != window->width || height != window->height)
                         dfb_window_resize( window, width, height );
               }

               break;

          case 1:
               if (stack->entered_window)
                    dfb_window_move( stack->entered_window, dx, dy );

               break;

          case 0:
               we.cx   = stack->cx;
               we.cy   = stack->cy;

               if (stack->pointer_window) {
                    we.type = DWET_MOTION;
                    we.x    = we.cx - stack->pointer_window->x;
                    we.y    = we.cy - stack->pointer_window->y;
          
                    dfb_window_dispatch( stack->pointer_window, &we );
               }
               else {
                    if (!handle_enter_leave_focus( stack )
                        && stack->entered_window)
                    {
                         we.type = DWET_MOTION;
                         we.x    = we.cx - stack->entered_window->x;
                         we.y    = we.cy - stack->entered_window->y;
          
                         dfb_window_dispatch( stack->entered_window, &we );
                    }
               }

               break;

          default:
               ;
     }

     HEAVYDEBUGMSG("DirectFB/windows: mouse at %d, %d\n", stack->cx, stack->cy);
}


static void
handle_wheel( CoreWindowStack *stack, int dz )
{
     DFBWindowEvent we;
     CoreWindow *window = NULL;

     if (!stack->cursor)
          return;

     window = (stack->pointer_window ?
               stack->pointer_window : stack->entered_window);


     if (window) {
          if (stack->wm_hack) {
               int opacity = window->opacity + dz*4;

               if (opacity < 0x01)
                    opacity = 1;
               if (opacity > 0xFF)
                    opacity = 0xFF;

               dfb_window_set_opacity( window, (__u8)opacity );
          }
          else {
               we.type = DWET_WHEEL;

               we.cx     = stack->cx;
               we.cy     = stack->cy;
               we.x      = we.cx - window->x;
               we.y      = we.cy - window->y;
               we.step   = dz;

               dfb_window_dispatch( window, &we );
          }
     }
}


static int
handle_enter_leave_focus( CoreWindowStack *stack )
{
     CoreWindow    *before = stack->entered_window;
     CoreWindow    *after = window_at_pointer( stack, -1, -1 );

     if (before != after) {
          DFBWindowEvent we;

          we.cx   = stack->cx;
          we.cy   = stack->cy;

          if (before) {
               we.type = DWET_LEAVE;
               we.x    = we.cx - before->x;
               we.y    = we.cy - before->y;

               dfb_window_dispatch( before, &we );
          }

          if (after) {
               dfb_window_request_focus( after );

               we.type = DWET_ENTER;
               we.x    = we.cx - after->x;
               we.y    = we.cy - after->y;

               dfb_window_dispatch( after, &we );
          }

          stack->entered_window = after;

          return 1;
     }

     return 0;
}

static DFBWindowID
new_window_id( CoreWindowStack *stack )
{
     static DFBWindowID id_pool = 0;

     int i;

     for (i=stack->num_windows-1; i>=0; i--) {
          CoreWindow *w = stack->windows[i];

          if (w->id == id_pool) {
               id_pool++;

               return new_window_id( stack );
          }
     }

     return id_pool++;
}

static inline int
get_window_index( CoreWindow *window )
{
     int               i       = 0;
     CoreWindowStack  *stack   = window->stack;
     int               num     = stack->num_windows;
     CoreWindow      **windows = stack->windows;

     for (; i<num; i++)
          if (windows[i] == window)
               return i;

     BUG( "window not found" );

     return -1;
}

static bool
restack_window( CoreWindowStack *stack,
                int              old_index,
                int              new_index )
{
     bool ret = false;

     if (new_index < 0)
          new_index = 0;
     else if (new_index >= stack->num_windows)
          new_index = stack->num_windows;

     if (old_index < 0)
          old_index = 0;
     else if (old_index >= stack->num_windows)
          old_index = stack->num_windows;

     if (old_index == new_index)
          return false;

     if (old_index < new_index) {
          int i;

          for (i=old_index; i<new_index; i++) {
               if (!(stack->windows[i+1]->caps & DWHC_GHOST)) {
                    CoreWindow *temp = stack->windows[i];
                    stack->windows[i] = stack->windows[i+1];
                    stack->windows[i+1] = temp;

                    ret = true;
               }
               else
                    break;
          }
     }
     else {
          int i;

          for (i=old_index; i>new_index; i--) {
               CoreWindow *temp = stack->windows[i];
               stack->windows[i] = stack->windows[i-1];
               stack->windows[i-1] = temp;

               ret = true;
          }
     }

     return ret;
}

static void
window_restacked( CoreWindow *window )
{
     if (window->opacity) {
          DFBRegion region = { window->x, window->y,
                               window->x + window->width - 1,
                               window->y + window->height - 1 };

          repaint_stack( window->stack, &region );

          handle_enter_leave_focus( window->stack );
     }
}
