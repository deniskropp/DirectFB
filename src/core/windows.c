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
#include <core/layers.h>
#include <core/layer_context.h>
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

static DFBWindowID new_window_id( CoreWindowStack *stack );


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
     CoreWindow      *window = (CoreWindow*) object;
     CoreWindowStack *stack  = window->stack;

     DEBUGMSG("DirectFB/core/windows: destroying %p (%dx%d%s)\n", window,
              window->width, window->height, zombie ? " ZOMBIE" : "");

     dfb_windowstack_lock( stack );

     dfb_window_destroy( window );

     window_remove( window );

     if (stack->cursor.window == window)
          stack->cursor.window = NULL;

     dfb_windowstack_unlock( stack );

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

DFBResult
dfb_window_create( CoreWindowStack        *stack,
                   int                     x,
                   int                     y,
                   int                     width,
                   int                     height,
                   DFBWindowCapabilities   caps,
                   DFBSurfaceCapabilities  surface_caps,
                   DFBSurfacePixelFormat   pixelformat,
                   CoreWindow            **ret_window )
{
     DFBResult          ret;
     CoreSurface       *surface;
     CoreSurfacePolicy  surface_policy = CSP_SYSTEMONLY;
     CoreLayer         *layer;
     CoreLayerContext  *context;
     CoreWindow        *window;
     CardCapabilities   card_caps;

     DFB_ASSERT( stack != NULL );
     DFB_ASSERT( stack->context != NULL );
     DFB_ASSERT( width > 0 );
     DFB_ASSERT( height > 0 );
     DFB_ASSERT( ret_window != NULL );

     context = stack->context;
     layer   = dfb_layer_at( context->layer_id );

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
          if (context->config.flags & DLCONF_PIXELFORMAT)
               pixelformat = context->config.pixelformat;
          else {
               CAUTION( "layer config has no pixel format, using RGB16" );

               pixelformat = DSPF_RGB16;
          }
     }

     /* Choose window surface policy */
     if ((surface_caps & DSCAPS_VIDEOONLY) ||
         (context->config.buffermode == DLBM_WINDOWS))
     {
          surface_policy = CSP_VIDEOONLY;
     }
     else if (!(surface_caps & DSCAPS_SYSTEMONLY) &&
              context->config.buffermode != DLBM_BACKSYSTEM)
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
          surface_caps |= DSCAPS_DOUBLE;

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
          CoreLayerRegion *region;

          /* Get the primary region of the layer context. */
          ret = dfb_layer_context_get_primary_region( context, true, &region );
          if (ret) {
               fusion_object_destroy( &window->object );
               return ret;
          }

          /* Link the primary region into the window structure. */
          dfb_layer_region_link( &window->primary_region, region );
          dfb_layer_region_unref( region );

          /* Create the surface for the window. */
          ret = dfb_surface_create( layer->core,
                                    width, height, pixelformat,
                                    surface_policy, surface_caps,
                                    region->surface ?
                                    region->surface->palette : NULL, &surface );
          if (ret) {
               dfb_layer_region_unlink( &window->primary_region );
               fusion_object_destroy( &window->object );
               return ret;
          }

          /* Link the surface into the window structure. */
          dfb_surface_link( &window->surface, surface );
          dfb_surface_unref( surface );

          /* Attach our global listener to the surface. */
          dfb_surface_attach_global( surface, DFB_WINDOW_SURFACE_LISTENER,
                                     window, &window->surface_reaction );
     }

     /* Finally activate the object. */
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

     /* Indicate destruction. */
     window->destroyed = true;

     /* Hardware allocated? */
     if (window->region) {
          /* Disable region (removing it from hardware). */
          dfb_layer_region_disable( window->region );

          /* Unlink from structure. */
          dfb_layer_region_unlink( &window->region );
     }

     /* Unlink the window's surface. */
     if (window->surface) {
          dfb_surface_detach_global( window->surface,
                                     &window->surface_reaction );

          dfb_surface_unlink( &window->surface );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );


     /* Notify listeners. */
     evt.type = DWET_DESTROYED;
     dfb_window_post_event( window, &evt );
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

     index = dfb_windowstack_get_window_index( stack, window );
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

     index = dfb_windowstack_get_window_index( stack, window );
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

     index = dfb_windowstack_get_window_index( stack, window );
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

     index = dfb_windowstack_get_window_index( stack, window );
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

     index = dfb_windowstack_get_window_index( stack, window );
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

     index = dfb_windowstack_get_window_index( stack, window );
     if (index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     lower_index = dfb_windowstack_get_window_index( stack, lower );
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

     index = dfb_windowstack_get_window_index( stack, window );
     if (index < 0) {
          dfb_windowstack_unlock( stack );
          return;
     }

     upper_index = dfb_windowstack_get_window_index( stack, upper );
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

/*     if (window->window_data)
          dfb_layer_update_window( dfb_layer_at(stack->layer_id),
                                   window, CWUF_POSITION );*/

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
     CoreLayer       *layer = dfb_layer_at( stack->context->layer_id );

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

     dfb_region_intersect( &window->opaque, 0, 0, width - 1, height - 1 );

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

/*     if (window->window_data)
          dfb_layer_update_window( dfb_layer_at(stack->layer_id),
                                   window, CWUF_SIZE | CWUF_SURFACE );*/

     /* Send new size */
     evt.type = DWET_SIZE;
     evt.w = window->width;
     evt.h = window->height;
     dfb_window_post_event( window, &evt );

     dfb_windowstack_update_focus( stack );

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

/*          if (window->window_data)
               dfb_layer_update_window( dfb_layer_at(stack->layer_id),
                                        window, CWUF_OPACITY );*/

          /* Check focus after window appeared or disappeared */
          if ((!old_opacity && opacity) || !opacity)
               dfb_windowstack_update_focus( stack );

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
          dfb_windowstack_update_focus( stack );
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

     DFB_ASSUME( !window->destroyed || event->type == DWET_DESTROYED );

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

     dfb_windowstack_switch_focus( stack, window );

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

DFBWindowID
dfb_window_id( const CoreWindow *window )
{
     DFB_ASSERT( window != NULL );

     return window->id;
}

/******************************************************************************/

static DFBWindowID
new_window_id( CoreWindowStack *stack )
{
     int i;

restart:

     for (i=stack->num_windows-1; i>=0; i--) {
          CoreWindow *w = stack->windows[i];

          if (w->id == stack->id_pool) {
               stack->id_pool++;
               goto restart;
          }
     }

     return stack->id_pool++;
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
          dfb_windowstack_update_focus( stack );
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

     /* Unlink the primary region of the context. */
     if (window->primary_region) {
          dfb_layer_region_unlink( &window->primary_region );
     }
     else
          DFB_ASSUME( window->caps & DWCAPS_INPUTONLY );

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
     dfb_windowstack_update_focus( stack );
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

               stack->keys[i].code  = -1;
               stack->keys[i].owner = NULL;
          }
     }
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
               dfb_windowstack_switch_focus( stack, window );
               break;
          }
     }
}


/*
 * listen to the window's surface
 */
ReactionResult
_dfb_window_surface_listener( const void *msg_data, void *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     CoreWindow                    *window       = ctx;

     (void) window;

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
/*          if (window->window_data) {
               CoreLayer *layer = dfb_layer_at( window->stack->layer_id );

               DFB_ASSERT( layer != NULL );

               dfb_layer_update_window( layer, window, CWUF_PALETTE );
          }*/
     }

     return RS_OK;
}

