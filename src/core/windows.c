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
#include <core/layers.h>
#include <core/gfxcard.h>
#include <core/input.h>
#include <core/state.h>
#include <core/system.h>
#include <core/windows.h>
#include <core/palette.h>

#include <misc/conf.h>
#include <misc/util.h>
#include <misc/mem.h>
#include <gfx/util.h>

#include <core/layers_internal.h>

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

static CoreWindow* window_at_pointer( CoreWindowStack *stack, int x, int y );
static int  handle_enter_leave_focus( CoreWindowStack *stack );
static void handle_wheel( CoreWindowStack *stack, int z );
static DFBWindowID new_window_id( CoreWindowStack *stack );

static CoreWindow* get_keyboard_window( CoreWindowStack     *stack,
                                        const DFBInputEvent *evt );


/*
 * Moves the window within the stack from the old stacking index
 * to the new one. Returns if restacking actually happened.
 */
static bool
window_restack( CoreWindowStack *stack,
                int old_index, int new_index, bool ignore_stackingclass );

/*
 * inserts a window into the windowstack pointed to by window->stack,
 * this function is called by dfb_window_init.
 */
static void
window_insert( CoreWindow *window,
               int         before );

/*
 * removes a window from the windowstack pointed to by window->stack,
 * this function is called by dfb_window_deinit.
 */
static void
window_remove( CoreWindow *window );

/*
 * sets any of the entered/focused/pointer/keyboard window pointers
 * to NULL that match this window
 */
static void
window_withdraw( CoreWindow *window );

/*
 * Called by restacking functions. If the window is visible the region
 * of the stack will be updated and the focus validated.
 */
static void
window_restacked( CoreWindow *window );

static DFBEnumerationResult
stack_attach_devices( InputDevice *device,
                      void        *ctx );

/*
 * Sends focus lost/got events and sets stack->focused_window.
 */
static void
switch_focus( CoreWindowStack *stack, CoreWindow *to );

/*
 * Tries to focus any window starting with the highest one
 * if there's currently no focused window.
 */
static void
ensure_focus( CoreWindowStack *stack );


static const React dfb_window_globals[] = {
     NULL
};

/*
 * Window destructor.
 */
static void
window_destructor( FusionObject *object, bool zombie )
{
     CoreWindow *window = (CoreWindow*) object;

     DEBUGMSG("DirectFB/core/windows: destroying %p (%dx%d%s)\n", window,
              window->width, window->height, zombie ? " ZOMBIE" : "");

     dfb_window_destroy( window );

     window_remove( window );

     fusion_object_destroy( object );
}

/******************************************************************************/

FusionObjectPool *
dfb_window_pool_create()
{
     return fusion_object_pool_create( "Window Pool",
                                       sizeof(CoreWindow),
                                       sizeof(DFBWindowEvent),
                                       window_destructor );
}

/******************************************************************************/

/*
 * Allocates and initializes a window stack.
 */
CoreWindowStack*
dfb_windowstack_new( CoreLayer *layer, int width, int height )
{
     int               i;
     CoreWindowStack  *stack;

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( width > 0 );
     DFB_ASSERT( height > 0 );

     /* Allocate window stack data (completely shared) */
     stack = (CoreWindowStack*) SHCALLOC( 1, sizeof(CoreWindowStack) );

     /* Remember layer id for access to it's local data later */
     stack->layer_id = layer->shared->id;

     /* Initialize the modify/update lock */
     fusion_skirmish_init( &stack->lock );

     /* Set default acceleration */
     stack->cursor.numerator   = 2;
     stack->cursor.denominator = 1;
     stack->cursor.threshold   = 4;

     /* Set default background mode, primary layer is handled by directfb.c */
     stack->bg.mode = (stack->layer_id == DLID_PRIMARY) ? DLBM_DONTCARE :
                      DLBM_COLOR;

     /* Setup size and cursor clipping region */
     dfb_windowstack_resize( stack, width, height );

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

     /* Destroy the stack lock. */
     fusion_skirmish_destroy( &stack->lock );

     /* see FIXME above */
     DFB_ASSUME( !stack->windows );

     if (stack->windows) {
          int i;

          for (i=0; i<stack->num_windows; i++) {
               CAUTION( "setting window->stack = NULL" );
               stack->windows[i]->stack = NULL;
          }

          SHFREE( stack->windows );
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

     /* FIXME: function is called during layer lease, locking the stack here
        results in a dead lock */
     //dfb_windowstack_lock( stack );

     /* Store the width and height of the stack */
     stack->width  = width;
     stack->height = height;

     /* Setup new cursor clipping region */
     stack->cursor.region.x1 = 0;
     stack->cursor.region.y1 = 0;
     stack->cursor.region.x2 = width - 1;
     stack->cursor.region.y2 = height - 1;

     //dfb_windowstack_unlock( stack );
}

DFBResult
dfb_window_create( CoreWindowStack        *stack,
                   CoreLayer              *layer,
                   int                     x,
                   int                     y,
                   int                     width,
                   int                     height,
                   DFBWindowCapabilities   caps,
                   DFBSurfaceCapabilities  surface_caps,
                   DFBSurfacePixelFormat   pixelformat,
                   DFBDisplayLayerConfig  *config,
                   CoreWindow            **ret_window )
{
     DFBResult               ret;
     CoreSurface            *surface;
     CoreSurfacePolicy       surface_policy = CSP_SYSTEMONLY;
     CoreWindow             *window;
     CardCapabilities        card_caps;

     DFB_ASSERT( stack != NULL );
     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( width > 0 );
     DFB_ASSERT( height > 0 );
     DFB_ASSERT( config != NULL );
     DFB_ASSERT( ret_window != NULL );

     if (width > 4096 || height > 4096)
          return DFB_BUFFERTOOLARGE;

     surface_caps &= DSCAPS_INTERLACED | DSCAPS_SEPARATED |
                     DSCAPS_STATIC_ALLOC | DSCAPS_SYSTEMONLY | DSCAPS_VIDEOONLY;

     if (!dfb_config->translucent_windows) {
          caps &= ~DWCAPS_ALPHACHANNEL;

          if (DFB_PIXELFORMAT_HAS_ALPHA(pixelformat))
               pixelformat = DSPF_UNKNOWN;
     }

     /* Choose pixel format. */
     if (caps & DWCAPS_ALPHACHANNEL) {
          if (pixelformat == DSPF_UNKNOWN)
               pixelformat = DSPF_ARGB;
          else if (! DFB_PIXELFORMAT_HAS_ALPHA(pixelformat))
               return DFB_INVARG;
     }
     else if (pixelformat == DSPF_UNKNOWN) {
          if (config->flags & DLCONF_PIXELFORMAT)
               pixelformat = config->pixelformat;
          else {
               CAUTION( "layer config has no pixel format, using RGB16" );

               pixelformat = DSPF_RGB16;
          }
     }

     /* Choose window surface policy */
     if ((surface_caps & DSCAPS_VIDEOONLY) ||
         (config->buffermode == DLBM_WINDOWS))
     {
          surface_policy = CSP_VIDEOONLY;
     }
     else if (!(surface_caps & DSCAPS_SYSTEMONLY) &&
              config->buffermode != DLBM_BACKSYSTEM)
     {
          if (dfb_config->window_policy != -1) {
               /* Use the explicitly specified policy. */
               surface_policy = dfb_config->window_policy;
          }
          else {
               /* Examine the hardware capabilities. */
               dfb_gfxcard_get_capabilities( &card_caps );

               if (card_caps.accel & DFXL_BLIT) {
                    if ((card_caps.blitting & DSBLIT_BLEND_ALPHACHANNEL) ||
                        !(caps & DWCAPS_ALPHACHANNEL))
                         surface_policy = CSP_VIDEOHIGH;
               }
          }
     }

     surface_caps &= ~(DSCAPS_SYSTEMONLY | DSCAPS_VIDEOONLY);

     switch (surface_policy) {
          case CSP_SYSTEMONLY:
               surface_caps |= DSCAPS_SYSTEMONLY;
               break;

          case CSP_VIDEOONLY:
               surface_caps |= DSCAPS_VIDEOONLY;
               break;

          default:
               break;
     }

     if (caps & DWCAPS_DOUBLEBUFFER)
          surface_caps |= DSCAPS_FLIPPING;

     /* Create the window object. */
     window = dfb_core_create_window( layer->core );

     window->id      = new_window_id( stack );

     window->x       = x;
     window->y       = y;
     window->width   = width;
     window->height  = height;

     window->caps    = caps;
     window->opacity = 0;

     window->stack   = stack;
     window->events  = DWET_ALL;

     /* Auto enable blending for ARGB only, not LUT8. */
     if ((caps & DWCAPS_ALPHACHANNEL) && pixelformat == DSPF_ARGB)
          window->options = DWOP_ALPHACHANNEL;

     /* Create the window's surface using the layer's palette if possible. */
     if (! (caps & DWCAPS_INPUTONLY)) {
          if (config->buffermode == DLBM_WINDOWS) {
               ret = dfb_surface_create( layer->core,
                                         width, height, pixelformat,
                                         surface_policy, surface_caps,
                                         NULL, &surface );
          }
          else {
               CoreSurface *layer_surface = dfb_layer_surface( layer );

               DFB_ASSERT( layer_surface != NULL );

               ret = dfb_surface_create( layer->core,
                                         width, height, pixelformat,
                                         surface_policy, surface_caps,
                                         layer_surface->palette, &surface );
          }

          if (ret) {
               fusion_object_destroy( &window->object );
               return ret;
          }

          dfb_surface_link( &window->surface, surface );
          dfb_surface_unref( surface );

          if (config->buffermode == DLBM_WINDOWS) {
               ret = dfb_layer_add_window( layer, window );
               if (ret) {
                    dfb_surface_unlink( &window->surface );
                    fusion_object_destroy( &window->object );
                    return ret;
               }
          }

          dfb_surface_attach_global( surface, DFB_WINDOW_SURFACE_LISTENER,
                                     window, &window->surface_reaction );
     }

     fusion_object_activate( &window->object );

     *ret_window = window;

     return DFB_OK;;
}

void
dfb_window_init( CoreWindow *window )
{
     int i;
     CoreWindowStack *stack = window->stack;

     dfb_windowstack_lock( stack );

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i]->caps & DWHC_TOPMOST ||
              stack->windows[i]->stacking == DWSC_UPPER)
               break;

     window_insert( window, i );

     dfb_windowstack_unlock( stack );
}

void
dfb_window_destroy( CoreWindow *window )
{
     DFBWindowEvent   evt;
     CoreWindowStack *stack;

     DFB_ASSERT( window != NULL );

     DEBUGMSG( "DirectFB/core/windows: "
               "dfb_window_destroy (%p) [%4d,%4d - %4dx%4d]\n",
               window, window->x, window->y, window->width, window->height );

     DFB_ASSUME( window->stack != NULL );

     stack = window->stack;
     if (!stack)
          return;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     /* Avoid multiple destructions. */
     if (window->destroyed) {
          dfb_windowstack_unlock( stack );
          return;
     }

     /* Make sure the window is no longer visible. */
     dfb_window_set_opacity( window, 0 );

     /* Notify listeners. */
     evt.type = DWET_DESTROYED;
     dfb_window_post_event( window, &evt );

     /* Indicate destruction. */
     window->destroyed = true;

     /* Remove from hardware */
     if (window->window_data) {
          dfb_layer_remove_window( dfb_layer_at(stack->layer_id), window );
          window->window_data = NULL;
     }

     /* Unlink the window's surface. */
     if (window->surface) {
          dfb_surface_detach_global( window->surface,
                                     &window->surface_reaction );

          dfb_surface_unlink( &window->surface );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );
}

void
dfb_window_change_stacking( CoreWindow             *window,
                            DFBWindowStackingClass  stacking )
{
     int              index, i;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     dfb_windowstack_lock( stack );

     if (stacking == window->stacking) {
          dfb_windowstack_unlock( stack );
          return;
     }

     index = dfb_windowstack_get_window_index( window );
     if (index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     switch (stacking) {
          case DWSC_LOWER:
               /* become the top lower class window */
               for (i=index; i>0; i--) {
                    if (stack->windows[i-1]->stacking == DWSC_LOWER)
                         break;
               }
               break;

          case DWSC_UPPER:
               /* become the bottom upper class window */
               for (i=index; i<stack->num_windows-1; i++) {
                    if (stack->windows[i+1]->stacking == DWSC_UPPER)
                         break;
               }
               break;

          case DWSC_MIDDLE:
               if (window->stacking == DWSC_UPPER) {
                    /* become the top middle class window */
                    for (i=index; i>0; i--) {
                         if (stack->windows[i-1]->stacking != DWSC_UPPER)
                              break;
                    }
               }
               else {
                    /* become the bottom middle class window */
                    for (i=index; i<stack->num_windows-1; i++) {
                         if (stack->windows[i+1]->stacking != DWSC_LOWER)
                              break;
                    }
               }
               break;

          default:
               BUG("unknown stacking class");
               dfb_windowstack_unlock( stack );
               return;
     }

     window->stacking = stacking;

     update = window_restack( stack, index, i, true );

     if (update)
          window_restacked( window );

     dfb_windowstack_unlock( stack );
}

void
dfb_window_raise( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     dfb_windowstack_lock( stack );

     index = dfb_windowstack_get_window_index( window );
     if (index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     update = window_restack( stack, index, index + 1, false );

     if (update)
          window_restacked( window );

     dfb_windowstack_unlock( stack );
}

void
dfb_window_lower( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     dfb_windowstack_lock( stack );

     index = dfb_windowstack_get_window_index( window );
     if (index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     update = window_restack( stack, index, index - 1, false );

     if (update)
          window_restacked( window );

     dfb_windowstack_unlock( stack );
}

void
dfb_window_raisetotop( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     dfb_windowstack_lock( stack );

     index = dfb_windowstack_get_window_index( window );
     if (index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     update = window_restack( stack, index, stack->num_windows - 1, false );

     if (update)
          window_restacked( window );

     dfb_windowstack_unlock( stack );
}

void
dfb_window_lowertobottom( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     dfb_windowstack_lock( stack );

     index = dfb_windowstack_get_window_index( window );
     if (index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     update = window_restack( stack, index, 0, false );

     if (update)
          window_restacked( window );

     dfb_windowstack_unlock( stack );
}

void
dfb_window_putatop( CoreWindow *window,
                    CoreWindow *lower )
{
     int              index;
     int              lower_index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     dfb_windowstack_lock( stack );

     index = dfb_windowstack_get_window_index( window );
     if (index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     lower_index = dfb_windowstack_get_window_index( lower );
     if (lower_index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     if (index < lower_index)
          update = window_restack( stack, index, lower_index, false );
     else
          update = window_restack( stack, index, lower_index + 1, false );

     if (update)
          window_restacked( window );

     dfb_windowstack_unlock( stack );
}

void
dfb_window_putbelow( CoreWindow *window,
                     CoreWindow *upper )
{
     int              index;
     int              upper_index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     dfb_windowstack_lock( stack );

     index = dfb_windowstack_get_window_index( window );
     if (index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     upper_index = dfb_windowstack_get_window_index( upper );
     if (upper_index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     if (index > upper_index)
          update = window_restack( stack, index, upper_index, false );
     else
          update = window_restack( stack, index, upper_index - 1, false );

     if (update)
          window_restacked( window );

     dfb_windowstack_unlock( stack );
}

void
dfb_window_move( CoreWindow *window,
                 int         dx,
                 int         dy )
{
     DFBWindowEvent   evt;
     CoreWindowStack *stack = window->stack;

     dfb_windowstack_lock( stack );

     window->x += dx;
     window->y += dy;

     if (VISIBLE_WINDOW(window)) {
          DFBRegion region = { 0, 0, window->width - 1, window->height - 1 };

          if (dx > 0)
               region.x1 -= dx;
          else if (dx < 0)
               region.x2 -= dx;

          if (dy > 0)
               region.y1 -= dy;
          else if (dy < 0)
               region.y2 -= dy;

          dfb_window_repaint( window, &region, 0, false, false );
     }

     if (window->window_data)
          dfb_layer_update_window( dfb_layer_at(stack->layer_id),
                                   window, CWUF_POSITION );

     /* Send new position */
     evt.type = DWET_POSITION;
     evt.x = window->x;
     evt.y = window->y;
     dfb_window_post_event( window, &evt );

     dfb_windowstack_unlock( stack );
}

DFBResult
dfb_window_resize( CoreWindow   *window,
                   int           width,
                   int           height )
{
     DFBWindowEvent   evt;
     CoreWindowStack *stack = window->stack;
     int              ow    = window->width;
     int              oh    = window->height;
     CoreLayer       *layer = dfb_layer_at( stack->layer_id );

     DFB_ASSERT( width > 0 );
     DFB_ASSERT( height > 0 );

     if (width > 4096 || height > 4096)
          return DFB_BUFFERTOOLARGE;

     dfb_windowstack_lock( stack );

     if (window->surface) {
          DFBResult ret = dfb_surface_reformat( layer->core,
                                                window->surface,
                                                width, height,
                                                window->surface->format );
          if (ret) {
               dfb_windowstack_unlock( stack );
               return ret;
          }

          window->width = window->surface->width;
          window->height = window->surface->height;
     }
     else {
          window->width  = width;
          window->height = height;
     }

     if (VISIBLE_WINDOW (window)) {
          if (ow > window->width) {
               DFBRegion region = { window->width, 0,
                                    ow - 1, MIN(window->height, oh) - 1 };

               dfb_window_repaint( window, &region, 0, false, false );
          }

          if (oh > window->height) {
               DFBRegion region = { 0, window->height,
                                    MAX(window->width, ow) - 1, oh - 1 };

               dfb_window_repaint( window, &region, 0, false, false );
          }
     }

     if (window->window_data)
          dfb_layer_update_window( dfb_layer_at(stack->layer_id),
                                   window, CWUF_SIZE | CWUF_SURFACE );

     /* Send new size */
     evt.type = DWET_SIZE;
     evt.w = window->width;
     evt.h = window->height;
     dfb_window_post_event( window, &evt );

     handle_enter_leave_focus( stack );

     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

void
dfb_window_set_opacity( CoreWindow *window,
                        __u8        opacity )
{
     int              old_opacity = window->opacity;
     CoreWindowStack *stack       = window->stack;

     if (!dfb_config->translucent_windows && opacity)
          opacity = 0xFF;

     if (old_opacity != opacity) {
          dfb_windowstack_lock( stack );

          window->opacity = opacity;

          dfb_window_repaint( window, NULL, 0, false, true );

          if (window->window_data)
               dfb_layer_update_window( dfb_layer_at(stack->layer_id),
                                        window, CWUF_OPACITY );

          /* Check focus after window appeared or disappeared */
          if ((!old_opacity && opacity) || !opacity)
               handle_enter_leave_focus( stack );

          /* If window disappeared... */
          if (!opacity) {
               /* Ungrab pointer/keyboard */
               window_withdraw( window );

               /* Always try to have a focused window */
               ensure_focus( stack );
          }

          dfb_windowstack_unlock( stack );
     }
}

DFBResult
dfb_window_grab_keyboard( CoreWindow *window )
{
     DFBResult        retval = DFB_OK;
     CoreWindowStack *stack  = window->stack;

     dfb_windowstack_lock( stack );

     if (stack->keyboard_window)
          retval = DFB_LOCKED;
     else
          stack->keyboard_window = window;

     dfb_windowstack_unlock( stack );

     return retval;
}

DFBResult
dfb_window_ungrab_keyboard( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     dfb_windowstack_lock( stack );

     if (stack->keyboard_window == window)
          stack->keyboard_window = NULL;

     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_window_grab_pointer( CoreWindow *window )
{
     DFBResult        retval = DFB_OK;
     CoreWindowStack *stack  = window->stack;

     dfb_windowstack_lock( stack );

     if (stack->pointer_window)
          retval = DFB_LOCKED;
     else
          stack->pointer_window = window;

     dfb_windowstack_unlock( stack );

     return retval;
}

DFBResult
dfb_window_ungrab_pointer( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     dfb_windowstack_lock( stack );

     if (stack->pointer_window == window) {
          stack->pointer_window = NULL;

          /* Possibly change focus to window now under the cursor */
          handle_enter_leave_focus( stack );
     }

     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_window_grab_key( CoreWindow                 *window,
                     DFBInputDeviceKeySymbol     symbol,
                     DFBInputDeviceModifierMask  modifiers )
{
     int              i;
     FusionLink      *l;
     GrabbedKey      *grab;
     CoreWindowStack *stack = window->stack;

     dfb_windowstack_lock( stack );

     fusion_list_foreach (l, stack->grabbed_keys) {
          GrabbedKey *key = (GrabbedKey*) l;

          if (key->symbol == symbol && key->modifiers == modifiers) {
               dfb_windowstack_unlock( stack );
               return DFB_LOCKED;
          }
     }

     grab = SHCALLOC( 1, sizeof(GrabbedKey) );

     grab->symbol    = symbol;
     grab->modifiers = modifiers;
     grab->owner     = window;

     fusion_list_prepend( &stack->grabbed_keys, &grab->link );

     for (i=0; i<8; i++)
          if (stack->keys[i].code != -1 && stack->keys[i].symbol == symbol)
               stack->keys[i].code = -1;

     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_window_ungrab_key( CoreWindow                 *window,
                       DFBInputDeviceKeySymbol     symbol,
                       DFBInputDeviceModifierMask  modifiers )
{
     FusionLink      *l;
     CoreWindowStack *stack = window->stack;

     dfb_windowstack_lock( stack );

     fusion_list_foreach (l, stack->grabbed_keys) {
          GrabbedKey *key = (GrabbedKey*) l;

          if (key->symbol    == symbol &&
              key->modifiers == modifiers &&
              key->owner     == window) {
               fusion_list_remove( &stack->grabbed_keys, &key->link );
               SHFREE( key );
               break;
          }
     }

     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

void
dfb_window_post_event( CoreWindow     *window,
                       DFBWindowEvent *event )
{
     DFB_ASSERT( window != NULL );
     DFB_ASSERT( event != NULL );

     DFB_ASSUME( !window->destroyed );

     if (! (event->type & window->events))
          return;

     gettimeofday( &event->timestamp, NULL );

     event->clazz     = DFEC_WINDOW;
     event->window_id = window->id;

     DFB_ASSUME( window->stack != NULL );

     if (window->stack) {
          CoreWindowStack *stack = window->stack;

          event->buttons   = stack->buttons;
          event->modifiers = stack->modifiers;
          event->locks     = stack->locks;

          event->cx        = stack->cursor.x;
          event->cy        = stack->cursor.y;
     }

     dfb_window_dispatch( window, event, dfb_window_globals );
}

void
dfb_window_request_focus( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     DFB_ASSERT( !(window->options & DWOP_GHOST) );

     dfb_windowstack_lock( stack );

     switch_focus( stack, window );

     if (stack->entered_window && stack->entered_window != window) {
          DFBWindowEvent  we;
          CoreWindow     *entered = stack->entered_window;

          we.type = DWET_LEAVE;
          we.x    = stack->cursor.x - entered->x;
          we.y    = stack->cursor.y - entered->y;

          dfb_window_post_event( entered, &we );

          stack->entered_window = NULL;
     }

     dfb_windowstack_unlock( stack );
}

void
dfb_windowstack_flush_keys( CoreWindowStack *stack )
{
     int i;

     DFB_ASSERT( stack != NULL );

     dfb_windowstack_lock( stack );

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

     dfb_windowstack_lock( stack );

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
                    if (!handle_enter_leave_focus( stack )
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

     dfb_windowstack_unlock( stack );
}


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

     dfb_input_attach_global( device, DFB_WINDOW_STACK_INPUTDEVICE_REACT,
                              ctx, &dev->reaction );

     return DFENUM_OK;
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

ReactionResult
_dfb_window_stack_inputdevice_react( const void *msg_data,
                                     void       *ctx )
{
     const DFBInputEvent *evt = msg_data;

     DFBWindowEvent   we;
     CoreWindow      *window = NULL;
     CoreWindowStack *stack  = ctx;
     CoreLayer       *layer  = dfb_layer_at( stack->layer_id );

     /* FIXME: this is a bad check for exclusive access */
     if (dfb_layer_lease( layer ) )
          return RS_OK;

     dfb_layer_release( layer, false );

     dfb_windowstack_lock( stack );

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
                              //handle_enter_leave_focus( stack );
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

                                        if (window_restack( stack, index, stack->num_windows - 1, false ))
                                             window_restacked( window );

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
                              switch_focus( stack, window_at_pointer( stack, -1, -1 ) );
                              dfb_windowstack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_A:
                              if (stack->focused_window &&
                                  ! (stack->focused_window->options & DWOP_KEEP_STACKING))
                              {
                                   dfb_window_lowertobottom( stack->focused_window );
                                   switch_focus( stack, window_at_pointer( stack, -1, -1 ) );
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
                              dfb_layer_cursor_set_opacity( layer, 0xff );
                              dfb_layer_cursor_enable( layer, true );

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

static int
handle_enter_leave_focus( CoreWindowStack *stack )
{
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
               switch_focus( stack, after );

               if (after) {
                    we.type = DWET_ENTER;
                    we.x    = stack->cursor.x - after->x;
                    we.y    = stack->cursor.y - after->y;

                    dfb_window_post_event( after, &we );
               }

               /* update pointer to window under the cursor */
               stack->entered_window = after;

               return 1;
          }
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

static void
window_insert( CoreWindow *window,
               int         before )
{
     int              i;
     DFBWindowEvent   evt;
     CoreWindowStack *stack = window->stack;

     DFB_ASSERT( window->stack != NULL );
     DFB_ASSERT( !window->destroyed );

     DFB_ASSUME( !window->initialized );

     if (window->initialized)
          return;

     window->initialized = true;

     if (before < 0  ||  before > stack->num_windows)
          before = stack->num_windows;

     stack->windows = SHREALLOC( stack->windows,
                                 sizeof(CoreWindow*) * (stack->num_windows+1) );

     for (i=stack->num_windows; i>before; i--)
          stack->windows[i] = stack->windows[i-1];

     stack->windows[before] = window;

     stack->num_windows++;

     /* Send configuration */
     evt.type = DWET_POSITION_SIZE;
     evt.x    = window->x;
     evt.y    = window->y;
     evt.w    = window->width;
     evt.h    = window->height;
     dfb_window_post_event( window, &evt );

     if (window->opacity)
          handle_enter_leave_focus( stack );
}

static void
window_remove( CoreWindow *window )
{
     int              i;
     int              index;
     FusionLink      *l;
     CoreWindowStack *stack = window->stack;

     DFB_ASSERT( window->stack != NULL );
     DFB_ASSERT( window->opacity == 0 );

     DFB_ASSUME( window->initialized );

     window_withdraw( window );

     l = stack->grabbed_keys;
     while (l) {
          FusionLink *next = l->next;
          GrabbedKey *key  = (GrabbedKey*) l;

          if (key->owner == window) {
               fusion_list_remove( &stack->grabbed_keys, &key->link );
               SHFREE( key );
          }

          l = next;
     }

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i] == window)
               break;

     index = i;
     if (i < stack->num_windows) {
          stack->num_windows--;

          for (; i<stack->num_windows; i++)
               stack->windows[i] = stack->windows[i+1];

          if (stack->num_windows) {
               stack->windows =
               SHREALLOC( stack->windows,
                          sizeof(CoreWindow*) * stack->num_windows );
          }
          else {
               SHFREE( stack->windows );
               stack->windows = NULL;
          }
     }

     window->initialized = false;

     window->stack = NULL;
}

static bool
window_restack( CoreWindowStack *stack,
                int              old_index,
                int              new_index,
                bool             ignore_stackingclass )
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
               if ((ignore_stackingclass ||
                    stack->windows[i+1]->stacking == stack->windows[i]->stacking) &&
                   !(stack->windows[i+1]->caps & DWHC_TOPMOST)) {
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
               if (ignore_stackingclass ||
                   stack->windows[i-1]->stacking == stack->windows[i]->stacking) {
                    CoreWindow *temp = stack->windows[i];
                    stack->windows[i] = stack->windows[i-1];
                    stack->windows[i-1] = temp;

                    ret = true;
               }
               else
                    break;
          }
     }

     return ret;
}

static void
window_restacked( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     dfb_window_repaint( window, NULL, 0, true, false );

     /* Possibly change focus to window now under the cursor */
     handle_enter_leave_focus( stack );
}

static void
window_withdraw( CoreWindow *window )
{
     int              i;
     CoreWindowStack *stack;

     DFB_ASSERT( window != NULL );
     DFB_ASSERT( window->stack != NULL );

     stack = window->stack;

     if (stack->entered_window == window)
          stack->entered_window = NULL;

     if (stack->focused_window == window)
          stack->focused_window = NULL;

     if (stack->keyboard_window == window)
          stack->keyboard_window = NULL;

     if (stack->pointer_window == window)
          stack->pointer_window = NULL;

     for (i=0; i<8; i++) {
          if (stack->keys[i].code != -1 && stack->keys[i].owner == window) {
               if (!window->destroyed) {
                    DFBWindowEvent we;

                    we.type       = DWET_KEYUP;
                    we.key_code   = stack->keys[i].code;
                    we.key_id     = stack->keys[i].id;
                    we.key_symbol = stack->keys[i].symbol;

                    dfb_window_post_event( window, &we );
               }

               stack->keys[i].owner = NULL;
          }
     }
}

static void
switch_focus( CoreWindowStack *stack, CoreWindow *to )
{
     DFBWindowEvent  evt;
     CoreWindow     *from = stack->focused_window;

     if (from == to)
          return;

     if (from) {
          evt.type = DWET_LOSTFOCUS;
          dfb_window_post_event( from, &evt );
     }

     if (to) {
          DFB_ASSUME(! (to->options & DWOP_GHOST));

          if (to->surface && to->surface->palette && !stack->hw_mode) {
               CoreLayer *layer = dfb_layer_at( stack->layer_id );

               if (dfb_layer_lease( layer ) == FUSION_SUCCESS) {
                    CoreSurface *surface = dfb_layer_surface( layer );

                    if (surface && DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
                         dfb_surface_set_palette (surface, to->surface->palette);

                    dfb_layer_release( layer, false );
               }
          }

          evt.type = DWET_GOTFOCUS;
          dfb_window_post_event( to, &evt );
     }

     stack->focused_window = to;
}

static void
ensure_focus( CoreWindowStack *stack )
{
     int i;

     if (stack->focused_window)
          return;

     for (i=stack->num_windows-1; i>=0; i--) {
          CoreWindow *window = stack->windows[i];

          if (window->opacity && !(window->options & DWOP_GHOST)) {
               switch_focus( stack, window );
               break;
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


/*
 * listen to the layer's surface
 */
ReactionResult
_dfb_window_surface_listener( const void *msg_data, void *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     CoreWindow                    *window       = ctx;

     DFB_ASSERT( notification != NULL );
     DFB_ASSERT( notification->surface != NULL );

     DFB_ASSERT( window != NULL );
     DFB_ASSERT( window->stack != NULL );
     DFB_ASSERT( window->surface == notification->surface );

     if (notification->flags & CSNF_DESTROY) {
          CAUTION( "window surface destroyed" );
          return RS_REMOVE;
     }

     if (notification->flags & (CSNF_PALETTE_CHANGE | CSNF_PALETTE_UPDATE)) {
          if (window->window_data) {
               CoreLayer *layer = dfb_layer_at( window->stack->layer_id );

               DFB_ASSERT( layer != NULL );

               dfb_layer_update_window( layer, window, CWUF_PALETTE );
          }
     }

     return RS_OK;
}

