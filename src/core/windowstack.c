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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#include <pthread.h>

#include <core/fusion/shmalloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/gfxcard.h>
#include <core/input.h>
#include <core/state.h>
#include <core/system.h>
#include <core/windows.h>
#include <core/windowstack.h>
#include <core/palette.h>

#include <misc/conf.h>
#include <misc/util.h>
#include <misc/mem.h>
#include <gfx/util.h>

#include <core/layers_internal.h>
#include <core/windows_internal.h>


#define CURSORFILE         DATADIR"/cursor.dat"


typedef struct {
     FusionLink       link;

     DFBInputDeviceID id;
     GlobalReaction   reaction;
} StackDevice;

typedef struct {
     FusionLink                  link;

     DFBInputDeviceKeySymbol     symbol;
     DFBInputDeviceModifierMask  modifiers;

     CoreWindow                 *owner;
} GrabbedKey;


static DFBResult   load_default_cursor ( CoreWindowStack *stack );
static DFBResult   create_cursor_window( CoreWindowStack *stack,
                                         int              width,
                                         int              height );

static CoreWindow* window_at_pointer   ( CoreWindowStack *stack, int x, int y );

static void        handle_wheel        ( CoreWindowStack *stack, int z );

static CoreWindow* get_keyboard_window ( CoreWindowStack     *stack,
                                         const DFBInputEvent *evt );


static DFBEnumerationResult
stack_attach_devices( InputDevice *device,
                      void        *ctx );


/******************************************************************************/

/*
 * Allocates and initializes a window stack.
 */
CoreWindowStack*
dfb_windowstack_create( CoreLayerContext *context )
{
     int               i;
     CoreWindowStack  *stack;

     DFB_ASSERT( context != NULL );

     /* Allocate window stack data (completely shared) */
     stack = (CoreWindowStack*) SHCALLOC( 1, sizeof(CoreWindowStack) );

     /* Store context which we belong to. */
     stack->context = context;

     /* Set default acceleration */
     stack->cursor.numerator   = 2;
     stack->cursor.denominator = 1;
     stack->cursor.threshold   = 4;

     /* Set default background mode. */
     stack->bg.mode = DLBM_COLOR;

     for (i=0; i<8; i++)
          stack->keys[i].code = -1;

     /* Attach to all input devices */
     dfb_input_enumerate_devices( stack_attach_devices, stack );

     return stack;
}

void
dfb_windowstack_destroy( CoreWindowStack *stack )
{
     FusionLink *l, *next;

     DFB_ASSERT( stack != NULL );

     /* Detach all input devices. */
     l = stack->devices;
     while (l) {
          FusionLink  *next   = l->next;
          StackDevice *device = (StackDevice*) l;

          dfb_input_detach_global( dfb_input_device_at( device->id ),
                                   &device->reaction );

          SHFREE( device );

          l = next;
     }

     /* Unlink cursor window. */
     if (stack->cursor.window)
          dfb_window_unlink( &stack->cursor.window );

     DFB_ASSUME( !stack->windows );

     if (stack->windows) {
          int i;

          for (i=0; i<stack->num_windows; i++) {
               CAUTION( "setting window->stack = NULL" );
               stack->windows[i]->stack = NULL;
          }

          SHFREE( stack->windows );
     }

     /* detach listener from background surface and unlink it */
     if (stack->bg.image) {
          dfb_surface_detach_global( stack->bg.image,
                                     &stack->bg.image_reaction );

          dfb_surface_unlink( &stack->bg.image );
     }

     /* Free grabbed keys. */
     fusion_list_foreach_safe (l, next, stack->grabbed_keys)
          SHFREE( l );

     /* Free stack data. */
     SHFREE( stack );
}

void
dfb_windowstack_resize( CoreWindowStack *stack,
                        int              width,
                        int              height )
{
     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     /* Store the width and height of the stack */
     stack->width  = width;
     stack->height = height;

     /* Setup new cursor clipping region */
     stack->cursor.region.x1 = 0;
     stack->cursor.region.y1 = 0;
     stack->cursor.region.x2 = width - 1;
     stack->cursor.region.y2 = height - 1;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );
}

/*
 * Prohibit access to the window stack data.
 * Waits until stack is accessible.
 */
inline FusionResult
dfb_windowstack_lock( CoreWindowStack *stack )
{
     DFB_ASSERT( stack != NULL );
     DFB_ASSERT( stack->context != NULL );

     return dfb_layer_context_lock( stack->context );
}

/*
 * Allow access to the window stack data.
 */
inline FusionResult
dfb_windowstack_unlock( CoreWindowStack *stack )
{
     DFB_ASSERT( stack != NULL );
     DFB_ASSERT( stack->context != NULL );

     return dfb_layer_context_unlock( stack->context );
}

/*
 * Returns the stacking index of the window within its stack
 * or -1 if not found.
 */
int
dfb_windowstack_get_window_index( CoreWindowStack *stack,
                                  CoreWindow      *window )
{
     int          i;
     int          num;
     CoreWindow **windows;

     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return -1;

     num     = stack->num_windows;
     windows = stack->windows;

     /* Lookup window. */
     for (i=0; i<num; i++) {
          if (windows[i] == window) {
               /* Unlock the window stack. */
               dfb_windowstack_unlock( stack );

               /* Return the index. */
               return i;
          }
     }

     CAUTION( "window not found" );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return -1;
}

void
dfb_windowstack_flush_keys( CoreWindowStack *stack )
{
     int i;

     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     for (i=0; i<8; i++) {
          if (stack->keys[i].code != -1) {
               DFBWindowEvent we;

               we.type       = DWET_KEYUP;
               we.key_code   = stack->keys[i].code;
               we.key_id     = stack->keys[i].id;
               we.key_symbol = stack->keys[i].symbol;

               dfb_window_post_event( stack->keys[i].owner, &we );

               stack->keys[i].code = -1;
          }
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );
}

void
dfb_windowstack_handle_motion( CoreWindowStack          *stack,
                               int                       dx,
                               int                       dy )
{
     int            new_cx, new_cy;
     DFBWindowEvent we;

     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     if (!stack->cursor.enabled) {
          dfb_windowstack_unlock( stack );
          return;
     }

     new_cx = MIN( stack->cursor.x + dx, stack->cursor.region.x2);
     new_cy = MIN( stack->cursor.y + dy, stack->cursor.region.y2);

     new_cx = MAX( new_cx, stack->cursor.region.x1 );
     new_cy = MAX( new_cy, stack->cursor.region.y1 );

     if (new_cx == stack->cursor.x  &&  new_cy == stack->cursor.y) {
          dfb_windowstack_unlock( stack );
          return;
     }

     dx = new_cx - stack->cursor.x;
     dy = new_cy - stack->cursor.y;

     stack->cursor.x = new_cx;
     stack->cursor.y = new_cy;

     DFB_ASSERT( stack->cursor.window != NULL );

     dfb_window_move( stack->cursor.window, dx, dy );

     switch (stack->wm_hack) {
          case 3: {
                    CoreWindow *window = stack->entered_window;

                    if (window) {
                         int opacity = window->opacity + dx;

                         if (opacity < 8)
                              opacity = 8;
                         else if (opacity > 255)
                              opacity = 255;

                         dfb_window_set_opacity( window, opacity );
                    }

                    break;
               }

          case 2: {
                    CoreWindow *window = stack->entered_window;

                    if (window && !(window->options & DWOP_KEEP_SIZE)) {
                         int width  = window->width  + dx;
                         int height = window->height + dy;

                         if (width  <   48) width  = 48;
                         if (height <   48) height = 48;
                         if (width  > 2048) width  = 2048;
                         if (height > 2048) height = 2048;

                         if (width != window->width || height != window->height)
                              dfb_window_resize( window, width, height );
                    }

                    break;
               }

          case 1: {
                    CoreWindow *window = stack->entered_window;

                    if (window && !(window->options & DWOP_KEEP_POSITION))
                         dfb_window_move( window, dx, dy );

                    break;
               }

          case 0:
               if (stack->pointer_window) {
                    we.type = DWET_MOTION;
                    we.x    = stack->cursor.x - stack->pointer_window->x;
                    we.y    = stack->cursor.y - stack->pointer_window->y;

                    dfb_window_post_event( stack->pointer_window, &we );
               }
               else {
                    if (!dfb_windowstack_update_focus( stack )
                        && stack->entered_window) {
                         we.type = DWET_MOTION;
                         we.x    = stack->cursor.x - stack->entered_window->x;
                         we.y    = stack->cursor.y - stack->entered_window->y;

                         dfb_window_post_event( stack->entered_window, &we );
                    }
               }

               break;

          default:
               ;
     }

     HEAVYDEBUGMSG("DirectFB/windows: mouse at %d, %d\n", stack->cursor.x, stack->cursor.y);

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );
}

bool
dfb_windowstack_update_focus( CoreWindowStack *stack )
{
     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return false;

     /* if pointer is not grabbed */
     if (!stack->pointer_window && !stack->wm_hack) {
          CoreWindow    *before = stack->entered_window;
          CoreWindow    *after = window_at_pointer( stack, -1, -1 );

          /* and the window under the cursor is another one now */
          if (before != after) {
               DFBWindowEvent we;

               /* send leave event */
               if (before) {
                    we.type = DWET_LEAVE;
                    we.x    = stack->cursor.x - before->x;
                    we.y    = stack->cursor.y - before->y;

                    dfb_window_post_event( before, &we );
               }

               /* switch focus and send enter event */
               dfb_windowstack_switch_focus( stack, after );

               if (after) {
                    we.type = DWET_ENTER;
                    we.x    = stack->cursor.x - after->x;
                    we.y    = stack->cursor.y - after->y;

                    dfb_window_post_event( after, &we );
               }

               /* update pointer to window under the cursor */
               stack->entered_window = after;

               /* Unlock the window stack. */
               dfb_windowstack_unlock( stack );

               return true;
          }
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return false;
}

/*
 * background handling
 */

DFBResult
dfb_windowstack_set_background_mode ( CoreWindowStack               *stack,
                                      DFBDisplayLayerBackgroundMode  mode )
{
     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* nothing to do if mode is the same */
     if (mode != stack->bg.mode) {
          /* for these modes a surface is required */
          if ((mode == DLBM_IMAGE || mode == DLBM_TILE) && !stack->bg.image) {
               dfb_windowstack_unlock( stack );
               return DFB_MISSINGIMAGE;
          }

          /* set new mode */
          stack->bg.mode = mode;

          /* force an update of the window stack */
          if (mode != DLBM_DONTCARE)
               dfb_windowstack_repaint_all( stack );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_set_background_image( CoreWindowStack *stack,
                                      CoreSurface     *image )
{
     DFB_ASSERT( stack != NULL );
     DFB_ASSERT( image != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* if the surface is changed */
     if (stack->bg.image != image) {
          /* detach listener from old surface and unlink it */
          if (stack->bg.image) {
               dfb_surface_detach_global( stack->bg.image,
                                          &stack->bg.image_reaction );

               dfb_surface_unlink( &stack->bg.image );
          }

          /* link surface object */
          dfb_surface_link( &stack->bg.image, image );

          /* attach listener to new surface */
          dfb_surface_attach_global( image,
                                     DFB_WINDOWSTACK_BACKGROUND_IMAGE_LISTENER,
                                     stack, &stack->bg.image_reaction );
     }

     /* force an update of the window stack */
     if (stack->bg.mode == DLBM_IMAGE || stack->bg.mode == DLBM_TILE)
          dfb_windowstack_repaint_all( stack );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_set_background_color( CoreWindowStack *stack,
                                      DFBColor        *color )
{
     DFB_ASSERT( stack != NULL );
     DFB_ASSERT( color != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* do nothing if color didn't change */
     if (!DFB_COLOR_EQUAL( stack->bg.color, *color )) {
          /* set new color */
          stack->bg.color = *color;

          /* force an update of the window stack */
          if (stack->bg.mode == DLBM_COLOR)
               dfb_windowstack_repaint_all( stack );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

/*
 * cursor control
 */

DFBResult
dfb_windowstack_cursor_enable( CoreWindowStack *stack, bool enable )
{
     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     stack->cursor.set = true;

     if (dfb_config->no_cursor) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     if (enable) {
          if (!stack->cursor.window) {
               DFBResult ret;

               ret = load_default_cursor( stack );
               if (ret) {
                    dfb_windowstack_unlock( stack );
                    return ret;
               }
          }

          dfb_window_set_opacity( stack->cursor.window,
                                  stack->cursor.opacity );

          stack->cursor.enabled = 1;
     }
     else {
          if (stack->cursor.window)
               dfb_window_set_opacity( stack->cursor.window, 0 );

          stack->cursor.enabled = 0;
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_set_opacity( CoreWindowStack *stack, __u8 opacity )
{
     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (stack->cursor.enabled) {
          DFB_ASSERT( stack->cursor.window );

          dfb_window_set_opacity( stack->cursor.window, opacity );
     }

     stack->cursor.opacity = opacity;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_set_shape( CoreWindowStack *stack,
                                  CoreSurface     *shape,
                                  int              hot_x,
                                  int              hot_y )
{
     DFBResult ret;
     int       dx, dy;

     DFB_ASSERT( stack != NULL );
     DFB_ASSERT( shape != NULL );

     if (dfb_config->no_cursor)
          return DFB_OK;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (!stack->cursor.window) {
          ret = create_cursor_window( stack, shape->width, shape->height );
          if (ret) {
               dfb_windowstack_unlock( stack );
               return ret;
          }
     }
     else if (stack->cursor.window->width  != shape->width  ||
              stack->cursor.window->height != shape->height)
     {
          ret = dfb_window_resize( stack->cursor.window,
                                   shape->width, shape->height );
          if (ret) {
               dfb_windowstack_unlock( stack );
               return ret;
          }
     }

     dfb_gfx_copy( shape, stack->cursor.window->surface, NULL );

     dx = stack->cursor.x - hot_x - stack->cursor.window->x;
     dy = stack->cursor.y - hot_y - stack->cursor.window->y;

     if (dx || dy)
          dfb_window_move( stack->cursor.window, dx, dy );
     else
          dfb_window_repaint( stack->cursor.window, NULL, 0, false, false );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_warp( CoreWindowStack *stack, int x, int y )
{
     int dx, dy;

     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     dx = x - stack->cursor.x;
     dy = y - stack->cursor.y;

     dfb_windowstack_handle_motion( stack, dx, dy );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_cursor_set_acceleration( CoreWindowStack *stack,
                                         int              numerator,
                                         int              denominator,
                                         int              threshold )
{
     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     stack->cursor.numerator   = numerator;
     stack->cursor.denominator = denominator;
     stack->cursor.threshold   = threshold;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_get_cursor_position( CoreWindowStack *stack, int *x, int *y )
{
     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     if (x)
          *x = stack->cursor.x;

     if (y)
          *y = stack->cursor.y;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_windowstack_switch_focus( CoreWindowStack *stack, CoreWindow *to )
{
     DFBWindowEvent  evt;
     CoreWindow     *from;

     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     from = stack->focused_window;

     if (from == to) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     if (from) {
          evt.type = DWET_LOSTFOCUS;
          dfb_window_post_event( from, &evt );
     }

     if (to) {
          if (to->surface && to->surface->palette && !stack->hw_mode) {
               CoreLayerRegion  *region;
               CoreLayerContext *context = stack->context;

               if (dfb_layer_context_get_primary_region( context,
                                                         &region ) == DFB_OK)
               {
                    CoreSurface *surface = region->surface;

                    if (surface && DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
                         dfb_surface_set_palette (surface, to->surface->palette);

                    dfb_layer_region_unref( region );
               }
          }

          evt.type = DWET_GOTFOCUS;
          dfb_window_post_event( to, &evt );
     }

     stack->focused_window = to;

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

/******************************************************************************/

ReactionResult
_dfb_windowstack_inputdevice_listener( const void *msg_data,
                                       void       *ctx )
{
     const DFBInputEvent *evt = msg_data;

     DFBWindowEvent   we;
     CoreWindow      *window  = NULL;
     CoreWindowStack *stack   = ctx;

     DFB_ASSERT( evt != NULL );
     DFB_ASSERT( stack != NULL );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return RS_REMOVE;

     /* Return if stack is suspended. */
     if (!stack->active) {
          dfb_windowstack_unlock( stack );
          return RS_OK;
     }

     /* FIXME: handle multiple devices */
     if (evt->flags & DIEF_BUTTONS)
          stack->buttons = evt->buttons;

     if (evt->flags & DIEF_MODIFIERS)
          stack->modifiers = evt->modifiers;

     if (evt->flags & DIEF_LOCKS)
          stack->locks = evt->locks;

     if (stack->wm_hack) {
          switch (evt->type) {
               case DIET_KEYRELEASE:
                    switch (evt->key_symbol) {
                         case DIKS_META:
                         case DIKS_CAPS_LOCK:
                              stack->wm_hack = 0;
                              stack->wm_cycle = 0;
                              //dfb_windowstack_update_focus( stack );
                              break;

                         case DIKS_ALT:
                         case DIKS_CONTROL:
                              stack->wm_hack = 1;
                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_A:
                         case DIKS_SMALL_C:
                         case DIKS_SMALL_S:
                         case DIKS_SMALL_X:
                         case DIKS_SMALL_D:
                         case DIKS_SMALL_P:
                         case DIKS_PRINT:
                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         default:
                              ;
                    }
                    break;

               case DIET_KEYPRESS:
                    switch (DFB_LOWER_CASE(evt->key_symbol)) {
                         case DIKS_ALT:
                              stack->wm_hack = 3;
                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         case DIKS_CONTROL:
                              stack->wm_hack = 2;
                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_X:
                              if (stack->wm_cycle <= 0)
                                   stack->wm_cycle = stack->num_windows;

                              if (stack->num_windows) {
                                   int looped = 0;
                                   int index = MIN( stack->num_windows,
                                                    stack->wm_cycle );

                                   while (index--) {
                                        CoreWindow *window = stack->windows[index];

                                        if ((window->options & (DWOP_GHOST | DWOP_KEEP_STACKING)) ||
                                            ! VISIBLE_WINDOW(window) ||
                                            window == stack->focused_window) {
                                             if (index == 0 && !looped) {
                                                  looped = 1;
                                                  index = stack->num_windows - 1;
                                             }

                                             continue;
                                        }

                                        dfb_window_raisetotop( window );
                                        dfb_window_request_focus( window );

                                        break;
                                   }

                                   stack->wm_cycle = index;
                              }
                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_C:
                              if (stack->entered_window) {
                                   DFBWindowEvent evt;
                                   evt.type = DWET_CLOSE;
                                   dfb_window_post_event( stack->entered_window, &evt );
                              }
                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_E:
                              dfb_windowstack_switch_focus( stack, window_at_pointer( stack, -1, -1 ) );
                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_A:
                              if (stack->focused_window &&
                                  ! (stack->focused_window->options & DWOP_KEEP_STACKING))
                              {
                                   dfb_window_lowertobottom( stack->focused_window );
                                   dfb_windowstack_switch_focus( stack, window_at_pointer( stack, -1, -1 ) );
                              }
                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_S:
                              if (stack->focused_window &&
                                  ! (stack->focused_window->options & DWOP_KEEP_STACKING))
                              {
                                   dfb_window_raisetotop( stack->focused_window );
                              }
                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_D: {
                              CoreWindow *window = stack->entered_window;

                              if (window &&
                                  !(window->options & DWOP_INDESTRUCTIBLE))
                              {
                                   dfb_window_destroy( window );
                              }

                              dfb_windowstack_unlock( stack );
                              return RS_OK;
                         }

                         case DIKS_SMALL_P:
                              dfb_windowstack_cursor_set_opacity( stack, 0xff );
                              dfb_windowstack_cursor_enable( stack, true );

                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         case DIKS_PRINT:
                              if (dfb_config->screenshot_dir) {
                                   CoreWindow *window = stack->focused_window;

                                   if (window && window->surface)
                                        dfb_surface_dump( window->surface,
                                                          dfb_config->screenshot_dir,
                                                          "dfb_window" );
                              }

                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         default:
                              ;
                    }
                    break;

               case DIET_BUTTONRELEASE:
                    dfb_windowstack_unlock( stack );
                    return RS_OK;

               case DIET_BUTTONPRESS:
                    if (stack->entered_window &&
                        !(stack->entered_window->options & DWOP_KEEP_STACKING))
                         dfb_window_raisetotop( stack->entered_window );
                    dfb_windowstack_unlock( stack );
                    return RS_OK;

               default:
                    ;
          }
     }

     switch (evt->type) {
          case DIET_KEYPRESS:
               if (evt->key_symbol == DIKS_CAPS_LOCK ||
                   evt->key_symbol == DIKS_META)
                    stack->wm_hack = 1;
               /* fall through */
          case DIET_KEYRELEASE:
               window = get_keyboard_window( stack, evt );

               if (window) {
                    we.type = (evt->type == DIET_KEYPRESS) ? DWET_KEYDOWN :
                              DWET_KEYUP;
                    we.key_code   = evt->key_code;
                    we.key_id     = evt->key_id;
                    we.key_symbol = evt->key_symbol;

                    dfb_window_post_event( window, &we );
               }

               break;

          case DIET_BUTTONPRESS:
          case DIET_BUTTONRELEASE:
               if (!stack->cursor.enabled)
                    break;

               window = (stack->pointer_window ?
                         stack->pointer_window : stack->entered_window);

               if (window) {
                    we.type = (evt->type == DIET_BUTTONPRESS) ? DWET_BUTTONDOWN :
                              DWET_BUTTONUP;
                    we.button = evt->button;
                    we.x      = stack->cursor.x - window->x;
                    we.y      = stack->cursor.y - window->y;

                    dfb_window_post_event( window, &we );
               }

               break;

          case DIET_AXISMOTION:
               if (evt->flags & DIEF_AXISREL) {
                    int rel = evt->axisrel;

                    /* handle cursor acceleration */
                    if (rel > stack->cursor.threshold)
                         rel += (rel - stack->cursor.threshold)
                                * stack->cursor.numerator
                                / stack->cursor.denominator;
                    else if (rel < -stack->cursor.threshold)
                         rel += (rel + stack->cursor.threshold)
                                * stack->cursor.numerator
                                / stack->cursor.denominator;

                    switch (evt->axis) {
                         case DIAI_X:
                              dfb_windowstack_handle_motion( stack, rel, 0 );
                              break;
                         case DIAI_Y:
                              dfb_windowstack_handle_motion( stack, 0, rel );
                              break;
                         case DIAI_Z:
                              handle_wheel( stack, - evt->axisrel );
                              break;
                         default:
                              dfb_windowstack_unlock( stack );
                              return RS_OK;
                    }
               }
               else if (evt->flags & DIEF_AXISABS) {
                    switch (evt->axis) {
                         case DIAI_X:
                              dfb_windowstack_handle_motion( stack,
                                                             evt->axisabs - stack->cursor.x, 0 );
                              break;
                         case DIAI_Y:
                              dfb_windowstack_handle_motion( stack,
                                                             0, evt->axisabs - stack->cursor.y );
                              break;
                         default:
                              dfb_windowstack_unlock( stack );
                              return RS_OK;
                    }
               }
               break;
          default:
               break;
     }

     dfb_windowstack_unlock( stack );
     return RS_OK;
}

/*
 * listen to the background image
 */
ReactionResult
_dfb_windowstack_background_image_listener( const void *msg_data,
                                            void       *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     CoreWindowStack               *stack        = ctx;

     DFB_ASSERT( notification != NULL );
     DFB_ASSERT( stack != NULL );

     if (notification->flags & CSNF_DESTROY) {
          if (stack->bg.image == notification->surface) {
               ERRORMSG("DirectFB/core/layers: Surface for background vanished.\n");

               stack->bg.mode  = DLBM_COLOR;
               stack->bg.image = NULL;

               dfb_windowstack_repaint_all( stack );
          }

          return RS_REMOVE;
     }

     if (notification->flags & (CSNF_FLIP | CSNF_SIZEFORMAT))
          dfb_windowstack_repaint_all( stack );

     return RS_OK;
}

/******************************************************************************/

/*
 * internals
 */

static DFBEnumerationResult
stack_attach_devices( InputDevice *device,
                      void        *ctx )
{
     StackDevice     *dev;
     CoreWindowStack *stack = (CoreWindowStack*) ctx;

     dev = SHCALLOC( 1, sizeof(StackDevice) );
     if (!dev) {
          ERRORMSG( "DirectFB/core/windows: Could not allocate %d bytes\n",
                    sizeof(StackDevice) );
          return DFENUM_CANCEL;
     }

     dev->id = dfb_input_device_id( device );

     fusion_list_prepend( &stack->devices, &dev->link );

     dfb_input_attach_global( device, DFB_WINDOWSTACK_INPUTDEVICE_LISTENER,
                              ctx, &dev->reaction );

     return DFENUM_OK;
}

/*
 * internal function that installs the cursor window
 * and fills it with data from 'cursor.dat'
 */
static DFBResult
load_default_cursor( CoreWindowStack *stack )
{
     DFBResult        ret;
     int              i;
     int              pitch;
     void            *data;
     FILE            *f;
     CoreWindow      *window;

     DFB_ASSERT( stack != NULL );

     if (!stack->cursor.window) {
          ret = create_cursor_window( stack, 40, 40 );
          if (ret)
               return ret;
     }

     window = stack->cursor.window;

     /* lock the surface of the window */
     ret = dfb_surface_soft_lock( window->surface,
                                  DSLF_WRITE, &data, &pitch, 0 );
     if (ret) {
          ERRORMSG( "DirectFB/core/layers: "
                    "cannot lock the surface for cursor window data!\n" );
          return ret;
     }

     /* initialize as empty cursor */
     memset( data, 0, 40 * pitch);

     /* open the file containing the cursors image data */
     f = fopen( CURSORFILE, "rb" );
     if (!f) {
          ret = errno2dfb( errno );

          /* ignore a missing cursor file */
          if (ret == DFB_FILENOTFOUND)
               ret = DFB_OK;
          else
               PERRORMSG( "`" CURSORFILE "` could not be opened!\n" );

          goto finish;
     }

     /* read from file directly into the cursor window surface */
     for (i=0; i<40; i++) {
          if (fread( data, MIN (40*4, pitch), 1, f ) != 1) {
               ret = errno2dfb( errno );

               ERRORMSG( "DirectFB/core/layers: "
                         "unexpected end or read error of cursor data!\n" );

               goto finish;
          }
#ifdef WORDS_BIGENDIAN
          {
               int i = MIN (40, pitch/4);
               __u32 *tmp_data = data;

               while (i--) {
                    *tmp_data = (*tmp_data & 0xFF000000) >> 24 |
                                (*tmp_data & 0x00FF0000) >>  8 |
                                (*tmp_data & 0x0000FF00) <<  8 |
                                (*tmp_data & 0x000000FF) << 24;
                    ++tmp_data;
               }
          }
#endif
          data += pitch;
     }

finish:
     if (f)
          fclose( f );

     dfb_surface_unlock( window->surface, 0 );

     dfb_window_repaint( window, NULL, 0, false, false );

     return ret;
}

static DFBResult
create_cursor_window( CoreWindowStack *stack,
                      int              width,
                      int              height )
{
     DFBResult   ret;
     CoreWindow *window;

     DFB_ASSERT( stack != NULL );
     DFB_ASSERT( stack->cursor.window == NULL );

     stack->cursor.x       = stack->width  / 2;
     stack->cursor.y       = stack->height / 2;
     stack->cursor.opacity = 0xFF;

     /* create a super-top-most event-and-focus-less window */
     ret = dfb_window_create( stack, stack->cursor.x, stack->cursor.y,
                              width, height, DWHC_TOPMOST | DWCAPS_ALPHACHANNEL,
                              DSCAPS_NONE, DSPF_UNKNOWN, NULL, &window );
     if (ret) {
          ERRORMSG( "DirectFB/Core/layers: "
                    "Failed creating a window for software cursor!\n" );
          return ret;
     }

     window->events   = 0;
     window->options |= DWOP_GHOST;

     dfb_window_link( &stack->cursor.window, window );

     dfb_window_unref( window );

     dfb_window_init( window );
     dfb_window_set_opacity( window, stack->cursor.opacity );

     return DFB_OK;
}

static CoreWindow*
window_at_pointer( CoreWindowStack *stack,
                   int              x,
                   int              y )
{
     int i;

     if (!stack->cursor.enabled) {
          for (i=stack->num_windows-1; i>=0; i--) {
               CoreWindow *window = stack->windows[i];

               if (window->opacity && !(window->options & DWOP_GHOST))
                    return window;
          }

          return NULL;
     }

     if (x < 0)
          x = stack->cursor.x;
     if (y < 0)
          y = stack->cursor.y;

     for (i=stack->num_windows-1; i>=0; i--) {
          CoreWindow *w = stack->windows[i];

          if (!(w->options & DWOP_GHOST) && w->opacity &&
              x >= w->x  &&  x < w->x + w->width &&
              y >= w->y  &&  y < w->y + w->height) {
               int wx = x - w->x;
               int wy = y - w->y;

               if ( !(w->options & DWOP_SHAPED)  ||
                    !(w->options &(DWOP_ALPHACHANNEL|DWOP_COLORKEYING))
                    || !w->surface ||
                    ((w->options & DWOP_OPAQUE_REGION) &&
                     (wx >= w->opaque.x1  &&  wx <= w->opaque.x2 &&
                      wy >= w->opaque.y1  &&  wy <= w->opaque.y2))) {
                    return w;
               }
               else {
                    void        *data;
                    int          pitch;
                    CoreSurface *surface = w->surface;

                    if ( dfb_surface_soft_lock( surface, DSLF_READ,
                                                &data, &pitch, true ) == DFB_OK ) {
                         if (w->options & DWOP_ALPHACHANNEL) {
                              int alpha = -1;

                              DFB_ASSERT( DFB_PIXELFORMAT_HAS_ALPHA( surface->format ) );

                              switch (surface->format) {
                                   case DSPF_ARGB:
                                        alpha = *(__u32*)(data +
                                                          4 * wx + pitch * wy) >> 24;
                                        break;
                                   case DSPF_ARGB1555:
                                        alpha = *(__u16*)(data + 2 * wx +
                                                          pitch * wy) & 0x8000;
                                        alpha = alpha ? 0xff : 0x00;
                                        break;
                                   case DSPF_ALUT44:
                                        alpha = *(__u8*)(data +
                                                         wx + pitch * wy) & 0xf0;
                                        alpha |= alpha >> 4;
                                        break;
                                   case DSPF_LUT8: {
                                        CorePalette *palette = surface->palette;
                                        __u8         pix     = *((__u8*) data + wx +
                                                                 pitch * wy);

                                        if (palette && pix < palette->num_entries) {
                                             alpha = palette->entries[pix].a;
                                             break;
                                        }

                                        /* fall through */
                                   }

                                   default:
                                        break;
                              }

                              if (alpha) { /* alpha == -1 on error */
                                   dfb_surface_unlock( surface, true );
                                   return w;
                              }

                         }
                         if (w->options & DWOP_COLORKEYING) {
                              int pixel = 0;

                              switch (surface->format) {
                                   case DSPF_ARGB:
                                   case DSPF_RGB32:
                                        pixel = *(__u32*)(data +
                                                          4 * wx + pitch * wy)
                                                & 0x00ffffff;
                                        break;

                                   case DSPF_RGB24:   //endianess? boh...
                                        pixel = (*(__u32*)(data +
                                                           3 * wx + pitch * wy))
                                                & 0x00ffffff;
                                        break;

                                   case DSPF_RGB16:
                                        pixel = *(__u16*)(data + 2 * wx +
                                                          pitch * wy);
                                        break;

                                   case DSPF_ARGB1555:
                                        pixel = *(__u16*)(data + 2 * wx +
                                                          pitch * wy)
                                                & 0x7fff;
                                        break;

                                   case DSPF_RGB332:
                                   case DSPF_LUT8:
                                        pixel = *(__u8*)(data +
                                                         wx + pitch * wy);
                                        break;

                                   case DSPF_ALUT44:
                                        pixel = *(__u8*)(data +
                                                         wx + pitch * wy)
                                                & 0x0f;
                                        break;

                                   default:
                                        break;
                              }

                              if ( pixel != w->color_key ) {
                                   dfb_surface_unlock( surface, true );
                                   return w;
                              }

                         }

                         dfb_surface_unlock( surface, true );
                    }
               }
          }
     }

     return NULL;
}

static void
handle_wheel( CoreWindowStack *stack, int dz )
{
     DFBWindowEvent we;
     CoreWindow *window = NULL;

     if (!stack->cursor.enabled)
          return;

     window = (stack->pointer_window ?
               stack->pointer_window : stack->entered_window);


     if (window) {
          if (stack->wm_hack) {
               int opacity = window->opacity + dz*7;

               if (opacity < 0x01)
                    opacity = 1;
               if (opacity > 0xFF)
                    opacity = 0xFF;

               dfb_window_set_opacity( window, (__u8)opacity );
          }
          else {
               we.type = DWET_WHEEL;

               we.x      = stack->cursor.x - window->x;
               we.y      = stack->cursor.y - window->y;
               we.step   = dz;

               dfb_window_post_event( window, &we );
          }
     }
}

static CoreWindow *
get_keyboard_window( CoreWindowStack     *stack,
                     const DFBInputEvent *evt )
{
     FusionLink *l;

     DFB_ASSERT( stack != NULL );
     DFB_ASSERT( evt != NULL );
     DFB_ASSERT( evt->type == DIET_KEYPRESS || evt->type == DIET_KEYRELEASE );

     /* Check explicit key grabs first. */
     fusion_list_foreach (l, stack->grabbed_keys) {
          GrabbedKey *key = (GrabbedKey*) l;

          if (key->symbol    == evt->key_symbol &&
              key->modifiers == stack->modifiers)
               return key->owner;
     }

     /* Don't do implicit grabs on keys without a hardware index. */
     if (evt->key_code == -1)
          return (stack->keyboard_window ?
                  stack->keyboard_window : stack->focused_window);

     /* Implicitly grab (press) or ungrab (release) key. */
     if (evt->type == DIET_KEYPRESS) {
          int         i;
          int         free_key = -1;
          CoreWindow *window;

          /* Check active grabs. */
          for (i=0; i<8; i++) {
               /* Key is grabbed, send to owner (NULL if destroyed). */
               if (stack->keys[i].code == evt->key_code)
                    return stack->keys[i].owner;

               /* Remember first free array item. */
               if (free_key == -1 && stack->keys[i].code == -1)
                    free_key = i;
          }

          /* Key is not grabbed, check for explicit keyboard grab or focus. */
          window = stack->keyboard_window ?
                   stack->keyboard_window : stack->focused_window;
          if (!window)
               return NULL;

          /* Check if a free array item was found. */
          if (free_key == -1) {
               CAUTION( "maximum number of owned keys reached" );
               return NULL;
          }

          /* Implicitly grab the key. */
          stack->keys[free_key].symbol = evt->key_symbol;
          stack->keys[free_key].id     = evt->key_id;
          stack->keys[free_key].code   = evt->key_code;
          stack->keys[free_key].owner  = window;

          return window;
     }
     else {
          int i;

          /* Lookup owner and ungrab the key. */
          for (i=0; i<8; i++) {
               if (stack->keys[i].code == evt->key_code) {
                    stack->keys[i].code = -1;

                    /* Return owner (NULL if destroyed). */
                    return stack->keys[i].owner;
               }
          }
     }

     /* No owner for release event found, discard it. */
     return NULL;
}

