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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


#include <pthread.h>

#include <core/fusion/shmalloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/gfxcard.h>
#include <core/input.h>
#include <core/state.h>
#include <core/system.h>
#include <core/windows.h>

#include <misc/conf.h>
#include <misc/util.h>
#include <misc/mem.h>
#include <gfx/util.h>


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

static void repaint_stack( CoreWindowStack *stack, DFBRegion *region,
                           DFBSurfaceFlipFlags flags );
static CoreWindow* window_at_pointer( CoreWindowStack *stack, int x, int y );
static int  handle_enter_leave_focus( CoreWindowStack *stack );
static void handle_wheel( CoreWindowStack *stack, int z );
static DFBWindowID new_window_id( CoreWindowStack *stack );

static CoreWindow* get_keyboard_window( CoreWindowStack     *stack,
                                        const DFBInputEvent *evt );

/*
 * Returns the stacking index of the window within its stack
 * or -1 if not found.
 */
static int
get_window_index( CoreWindow *window );

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

/*
 * Prohibit access to the window stack data.
 * Waits until stack is accessible.
 */
static inline void
stack_lock( CoreWindowStack *stack )
{
     fusion_skirmish_prevail( &stack->lock );
}

/*
 * Allow access to the window stack data.
 */
static inline void
stack_unlock( CoreWindowStack *stack )
{
     fusion_skirmish_dismiss( &stack->lock );
}

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

     DEBUGMSG("DirectFB/core/windows: destroying %p (%dx%d)%s\n", window,
              window->width, window->height, zombie ? " (ZOMBIE)" : "");

     if (window->stack) {
          stack_lock( window->stack );

          dfb_window_deinit( window );
          dfb_window_destroy( window, false );

          stack_unlock( window->stack );
     }

     fusion_object_destroy( object );
}

/*
 * Allocates and initializes a window stack.
 */
CoreWindowStack*
dfb_windowstack_new( DisplayLayer *layer, int width, int height )
{
     int               i;
     CoreWindowStack  *stack;

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( width > 0 );
     DFB_ASSERT( height > 0 );

     /* Allocate window stack data (completely shared) */
     stack = (CoreWindowStack*) shcalloc( 1, sizeof(CoreWindowStack) );

     /* Remember layer id for access to it's local data later */
     stack->layer_id = dfb_layer_id( layer );

     /* Create the pool of windows. */
     if (stack->layer_id == DLID_PRIMARY)
          stack->pool = fusion_object_pool_create( "Window Pool",
                                                   sizeof(CoreWindow),
                                                   sizeof(DFBWindowEvent),
                                                   window_destructor );
     else
          stack->pool = dfb_layer_window_stack( dfb_layer_at(DLID_PRIMARY) )->pool;

     /* Initialize the modify/update lock */
     fusion_skirmish_init( &stack->lock );

     /* Set default acceleration */
     stack->cursor.numerator   = 2;
     stack->cursor.denominator = 1;
     stack->cursor.threshold   = 4;

     /* Set default background mode. */
     stack->bg.mode = DLBM_COLOR;

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
     FusionLink *l;

     DFB_ASSERT( stack != NULL );

     l = stack->devices;
     while (l) {
          FusionLink  *next   = l->next;
          StackDevice *device = (StackDevice*) l;

          dfb_input_detach_global( dfb_input_device_at( device->id ),
                                   &device->reaction );

          shfree( device );

          l = next;
     }

     l = stack->grabbed_keys;
     while (l) {
          FusionLink *next = l->next;

          shfree( l );

          l = next;
     }

     if (stack->cursor.window)
          dfb_window_unlink( stack->cursor.window );

     if (stack->layer_id == DLID_PRIMARY)
          fusion_object_pool_destroy( stack->pool );

     fusion_skirmish_destroy( &stack->lock );

     if (stack->windows) {
          int i;

          for (i=0; i<stack->num_windows; i++)
               stack->windows[i]->stack = NULL;

          shfree( stack->windows );
     }

     shfree( stack );
}

void
dfb_windowstack_resize( CoreWindowStack *stack,
                        int              width,
                        int              height )
{
     DFB_ASSERT( stack != NULL );

     /* FIXME: function is called during layer lease, locking the stack here
        results in a dead lock */
     //stack_lock( stack );

     /* Store the width and height of the stack */
     stack->width  = width;
     stack->height = height;

     /* Setup new cursor clipping region */
     stack->cursor.region.x1 = 0;
     stack->cursor.region.y1 = 0;
     stack->cursor.region.x2 = width - 1;
     stack->cursor.region.y2 = height - 1;

     //stack_unlock( stack );
}

DFBResult
dfb_window_create( CoreWindowStack        *stack,
                   int                     x,
                   int                     y,
                   int                     width,
                   int                     height,
                   DFBWindowCapabilities   caps,
                   DFBSurfaceCapabilities  surface_caps,
                   DFBSurfacePixelFormat   pixelformat,
                   CoreWindow            **window )
{
     DFBResult               ret;
     CoreSurface            *surface;
     CoreSurfacePolicy       surface_policy = CSP_SYSTEMONLY;
     CoreWindow             *w;
     DisplayLayer           *layer = dfb_layer_at( stack->layer_id );
     CoreSurface            *layer_surface = dfb_layer_surface( layer );
     CardCapabilities        card_caps;

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
     else if (pixelformat == DSPF_UNKNOWN)
          pixelformat = layer_surface->format;
     
     /* Choose window surface policy */
     if (surface_caps & DSCAPS_VIDEOONLY) {
          surface_policy = CSP_VIDEOONLY;
     }
     else if (!(surface_caps & DSCAPS_SYSTEMONLY) &&
              layer_surface->back_buffer->policy != CSP_SYSTEMONLY)
     {
          if (dfb_config->window_policy != -1) {
               /* Use the explicitly specified policy. */
               surface_policy = dfb_config->window_policy;
          }
          else {
               /* Examine the hardware capabilities. */
               card_caps = dfb_gfxcard_capabilities();

               if (card_caps.accel & DFXL_BLIT) {
                    if ((card_caps.blitting & DSBLIT_BLEND_ALPHACHANNEL) ||
                        !(caps & DWCAPS_ALPHACHANNEL))
                         surface_policy = CSP_VIDEOHIGH;
               }
          }
     }

     if (caps & DWCAPS_DOUBLEBUFFER)
          surface_caps |= DSCAPS_FLIPPING;

     /* Create the window object. */
     w = (CoreWindow*) fusion_object_create( stack->pool );

     /* Create the window's surface using the layer's palette. */
     if (! (caps & DWCAPS_INPUTONLY)) {
          ret = dfb_surface_create( width, height, pixelformat, surface_policy,
                                    surface_caps, layer_surface->palette,
                                    &surface );
          if (ret) {
               fusion_object_destroy( &w->object );
               return ret;
          }

          dfb_surface_link( &w->surface, surface );
          dfb_surface_unref( surface );
     }

     w->id      = new_window_id( stack );

     w->x       = x;
     w->y       = y;
     w->width   = width;
     w->height  = height;

     w->caps    = caps;
     w->opacity = 0;

     if (caps & DWCAPS_ALPHACHANNEL)
          w->options = DWOP_ALPHACHANNEL;

     w->stack   = stack;

     w->events  = DWET_ALL;

     fusion_object_activate( &w->object );

     *window = w;

     return DFB_OK;;
}

void
dfb_window_init( CoreWindow *window )
{
     int i;
     CoreWindowStack *stack = window->stack;

     stack_lock( stack );

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i]->caps & DWHC_TOPMOST ||
              stack->windows[i]->stacking == DWSC_UPPER)
               break;

     window_insert( window, i );

     stack_unlock( stack );
}


void
dfb_window_deinit( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     DEBUGMSG("DirectFB/core/windows: dfb_window_deinit (%p) entered\n", window);

     if (stack) {
          stack_lock( stack );
          window_remove( window );
          stack_unlock( stack );
     }

     DEBUGMSG("DirectFB/core/windows: dfb_window_deinit (%p) exitting\n", window);
}

void
dfb_window_destroy( CoreWindow *window, bool unref )
{
     DFBWindowEvent   evt;
     CoreWindowStack *stack = window->stack;

     stack_lock( stack );

     if (window->destroyed) {
          DEBUGMSG("DirectFB/core/windows: in dfb_window_destroy (%p), "
                   "already destroyed!\n", window);
          stack_unlock( stack );
          return;
     }

     DEBUGMSG("DirectFB/core/windows: dfb_window_destroy (%p) entered\n", window);

     window->destroyed = true;

     evt.type = DWET_DESTROYED;
     dfb_window_dispatch( window, &evt );

     if (window->surface) {
          CoreSurface *surface = window->surface;

          DEBUGMSG("DirectFB/core/windows: dfb_window_destroy (%p) unlinking surface...\n", window);

          window->surface = NULL;

          dfb_surface_unlink( surface );
     }

     if (unref) {
          DEBUGMSG("DirectFB/core/windows: dfb_window_destroy (%p) unrefing window...\n", window);
          dfb_window_unref( window );
     }

     DEBUGMSG("DirectFB/core/windows: dfb_window_destroy (%p) exiting\n", window);

     stack_unlock( stack );
}

void
dfb_window_change_stacking( CoreWindow             *window,
                            DFBWindowStackingClass  stacking )
{
     int              index, i;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     stack_lock( stack );

     if (stacking == window->stacking) {
          stack_unlock( stack );
          return;
     }

     index = get_window_index( window );
     if (index < 0) {
          stack_unlock( stack );
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
               stack_unlock( stack );
               return;
     }

     window->stacking = stacking;

     update = window_restack( stack, index, i, true );

     if (update)
          window_restacked( window );

     stack_unlock( stack );
}

void
dfb_window_raise( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     stack_lock( stack );

     index = get_window_index( window );
     if (index < 0) {
          stack_unlock( stack );
          return;
     }

     update = window_restack( stack, index, index + 1, false );

     if (update)
          window_restacked( window );

     stack_unlock( stack );
}

void
dfb_window_lower( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     stack_lock( stack );

     index = get_window_index( window );
     if (index < 0) {
          stack_unlock( stack );
          return;
     }

     update = window_restack( stack, index, index - 1, false );

     if (update)
          window_restacked( window );

     stack_unlock( stack );
}

void
dfb_window_raisetotop( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     stack_lock( stack );

     index = get_window_index( window );
     if (index < 0) {
          stack_unlock( stack );
          return;
     }

     update = window_restack( stack, index, stack->num_windows - 1, false );

     if (update)
          window_restacked( window );

     stack_unlock( stack );
}

void
dfb_window_lowertobottom( CoreWindow *window )
{
     int              index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     stack_lock( stack );

     index = get_window_index( window );
     if (index < 0) {
          stack_unlock( stack );
          return;
     }

     update = window_restack( stack, index, 0, false );

     if (update)
          window_restacked( window );

     stack_unlock( stack );
}

void
dfb_window_putatop( CoreWindow *window,
                    CoreWindow *lower )
{
     int              index;
     int              lower_index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     stack_lock( stack );

     index = get_window_index( window );
     if (index < 0) {
          stack_unlock( stack );
          return;
     }

     lower_index = get_window_index( lower );
     if (lower_index < 0) {
          stack_unlock( stack );
          return;
     }

     if (index < lower_index)
          update = window_restack( stack, index, lower_index, false );
     else
          update = window_restack( stack, index, lower_index + 1, false );

     if (update)
          window_restacked( window );

     stack_unlock( stack );
}

void
dfb_window_putbelow( CoreWindow *window,
                     CoreWindow *upper )
{
     int              index;
     int              upper_index;
     bool             update = false;
     CoreWindowStack *stack  = window->stack;

     stack_lock( stack );

     index = get_window_index( window );
     if (index < 0) {
          stack_unlock( stack );
          return;
     }

     upper_index = get_window_index( upper );
     if (upper_index < 0) {
          stack_unlock( stack );
          return;
     }

     if (index > upper_index)
          update = window_restack( stack, index, upper_index, false );
     else
          update = window_restack( stack, index, upper_index - 1, false );

     if (update)
          window_restacked( window );

     stack_unlock( stack );
}


#define TRANSLUCENT_WINDOW(w) ((w)->opacity < 0xff || \
                               (w)->options & (DWOP_ALPHACHANNEL | \
                                               DWOP_COLORKEYING))

#define VISIBLE_WINDOW(w)     (!((w)->caps & DWCAPS_INPUTONLY) && \
                               (w)->opacity > 0 && !(w)->destroyed)


void
dfb_window_move( CoreWindow *window,
                 int         dx,
                 int         dy )
{
     DFBWindowEvent   evt;
     CoreWindowStack *stack = window->stack;

     stack_lock( stack );

     window->x += dx;
     window->y += dy;

     if (VISIBLE_WINDOW(window)) {
          DFBRegion region = { window->x, window->y,
               window->x + window->width - 1,
               window->y + window->height - 1};

          if (dx > 0)
               region.x1 -= dx;
          else if (dx < 0)
               region.x2 -= dx;

          if (dy > 0)
               region.y1 -= dy;
          else if (dy < 0)
               region.y2 -= dy;

          repaint_stack( stack, &region, 0 );
     }

     /* Send new position */
     evt.type = DWET_POSITION;
     evt.x = window->x;
     evt.y = window->y;
     dfb_window_dispatch( window, &evt );

     stack_unlock( stack );
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

     stack_lock( stack );

     if (window->surface) {
          DFBResult ret = dfb_surface_reformat( window->surface,
                                                width, height,
                                                window->surface->format );
          if (ret) {
               stack_unlock( stack );
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
               DFBRegion region = { window->x + window->width, window->y,
                    window->x + ow - 1,
                    window->y + MIN(window->height, oh) - 1};

               repaint_stack( stack, &region, 0 );
          }

          if (oh > window->height) {
               DFBRegion region = { window->x, window->y + window->height,
                    window->x + MAX(window->width, ow) - 1,
                    window->y + oh - 1};

               repaint_stack( stack, &region, 0 );
          }
     }

     /* Send new size */
     evt.type = DWET_SIZE;
     evt.w = window->width;
     evt.h = window->height;
     dfb_window_dispatch( window, &evt );

     stack_unlock( stack );

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
          DFBRegion region = { window->x, window->y,
               window->x + window->width - 1,
               window->y + window->height - 1};

          stack_lock( stack );

          window->opacity = opacity;

          repaint_stack( stack, &region, 0 );

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

          stack_unlock( stack );
     }
}

void
dfb_window_repaint( CoreWindow          *window,
                    DFBRegion           *region,
                    DFBSurfaceFlipFlags  flags )
{
     int              i;
     CoreWindowStack *stack = window->stack;

     if (!VISIBLE_WINDOW(window))
          return;

     stack_lock( stack );

     if (region) {
          region->x1 += window->x;
          region->x2 += window->x;
          region->y1 += window->y;
          region->y2 += window->y;
     }
     else {
          region = alloca( sizeof(DFBRegion) );

          region->x1 = window->x;
          region->y1 = window->y;
          region->x2 = window->x + window->width - 1;
          region->y2 = window->y + window->height - 1;
     }

     /* simple check if update is necessary */
     for (i = get_window_index(window) + 1; i < stack->num_windows; i++) {
          CoreWindow *upper = stack->windows[i];

          if (!VISIBLE_WINDOW(upper) || TRANSLUCENT_WINDOW(upper))
               continue;

          /* if the update region completely obscured by an opaque window */
          if (upper->x <= region->x1 && upper->y <= region->y1 &&
              upper->x + upper->width - 1 >= region->x2 &&
              upper->y + upper->height - 1 >= region->y2) {
               /* discard the update */
               stack_unlock( stack );
               return;
          }
     }

     repaint_stack( stack, region, flags );

     stack_unlock( stack );
}

DFBResult
dfb_window_grab_keyboard( CoreWindow *window )
{
     DFBResult        retval = DFB_OK;
     CoreWindowStack *stack  = window->stack;

     stack_lock( stack );

     if (stack->keyboard_window)
          retval = DFB_LOCKED;
     else
          stack->keyboard_window = window;

     stack_unlock( stack );

     return retval;
}

DFBResult
dfb_window_ungrab_keyboard( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     stack_lock( stack );

     if (stack->keyboard_window == window)
          stack->keyboard_window = NULL;

     stack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_window_grab_pointer( CoreWindow *window )
{
     DFBResult        retval = DFB_OK;
     CoreWindowStack *stack  = window->stack;

     stack_lock( stack );

     if (stack->pointer_window)
          retval = DFB_LOCKED;
     else
          stack->pointer_window = window;

     stack_unlock( stack );

     return retval;
}

DFBResult
dfb_window_ungrab_pointer( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     stack_lock( stack );

     if (stack->pointer_window == window) {
          stack->pointer_window = NULL;

          /* Possibly change focus to window now under the cursor */
          handle_enter_leave_focus( stack );
     }

     stack_unlock( stack );

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

     stack_lock( stack );

     fusion_list_foreach (l, stack->grabbed_keys) {
          GrabbedKey *key = (GrabbedKey*) l;

          if (key->symbol == symbol && key->modifiers == modifiers) {
               stack_unlock( stack );
               return DFB_LOCKED;
          }
     }

     grab = shcalloc( 1, sizeof(GrabbedKey) );

     grab->symbol    = symbol;
     grab->modifiers = modifiers;
     grab->owner     = window;

     fusion_list_prepend( &stack->grabbed_keys, &grab->link );

     for (i=0; i<8; i++)
          if (stack->keys[i].code != -1 && stack->keys[i].symbol == symbol)
               stack->keys[i].code = -1;

     stack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_window_ungrab_key( CoreWindow                 *window,
                       DFBInputDeviceKeySymbol     symbol,
                       DFBInputDeviceModifierMask  modifiers )
{
     FusionLink      *l;
     CoreWindowStack *stack = window->stack;

     stack_lock( stack );

     fusion_list_foreach (l, stack->grabbed_keys) {
          GrabbedKey *key = (GrabbedKey*) l;

          if (key->symbol    == symbol &&
              key->modifiers == modifiers &&
              key->owner     == window) {
               fusion_list_remove( &stack->grabbed_keys, &key->link );
               shfree( key );
               break;
          }
     }

     stack_unlock( stack );

     return DFB_OK;
}

void
dfb_window_dispatch( CoreWindow     *window,
                     DFBWindowEvent *event )
{
     DFB_ASSERT( window != NULL );
     DFB_ASSERT( event != NULL );

     if (! (event->type & window->events))
          return;

     event->clazz     = DFEC_WINDOW;
     event->window_id = window->id;

     if (window->stack) {
          CoreWindowStack *stack = window->stack;

          event->buttons   = stack->buttons;
          event->modifiers = stack->modifiers;
          event->locks     = stack->locks;

          event->cx        = stack->cursor.x;
          event->cy        = stack->cursor.y;
     }

     fusion_object_dispatch( &window->object, event, dfb_window_globals );
}

void
dfb_window_request_focus( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     DFB_ASSERT( !(window->options & DWOP_GHOST) );

     stack_lock( stack );

     switch_focus( stack, window );

     if (stack->entered_window && stack->entered_window != window) {
          DFBWindowEvent  we;
          CoreWindow     *entered = stack->entered_window;

          we.type = DWET_LEAVE;
          we.x    = stack->cursor.x - entered->x;
          we.y    = stack->cursor.y - entered->y;

          dfb_window_dispatch( entered, &we );

          stack->entered_window = NULL;
     }
     
     stack_unlock( stack );
}

void
dfb_windowstack_repaint_all( CoreWindowStack *stack )
{
     DFBRegion region = { 0, 0, stack->width - 1, stack->height - 1};

     stack_lock( stack );
     repaint_stack( stack, &region, 0 );
     stack_unlock( stack );
}

void
dfb_windowstack_flush_keys( CoreWindowStack *stack )
{
     int i;

     DFB_ASSERT( stack != NULL );

     stack_lock( stack );

     for (i=0; i<8; i++) {
          if (stack->keys[i].code != -1) {
               DFBWindowEvent we;

               we.type       = DWET_KEYUP;
               we.key_code   = stack->keys[i].code;
               we.key_id     = stack->keys[i].id;
               we.key_symbol = stack->keys[i].symbol;

               dfb_window_dispatch( stack->keys[i].owner, &we );

               stack->keys[i].code = -1;
          }
     }

     stack_unlock( stack );
}

void
dfb_windowstack_sync_buffers( CoreWindowStack *stack )
{
     DisplayLayer *layer;
     CoreSurface  *surface;

     DFB_ASSERT( stack != NULL );

     stack_lock( stack );

     layer = dfb_layer_at( stack->layer_id );
     surface = dfb_layer_surface( layer );

     if (surface->caps & (DSCAPS_FLIPPING | DSCAPS_TRIPLE))
          dfb_gfx_copy(surface, surface, NULL);

     stack_unlock( stack );
}

void
dfb_windowstack_handle_motion( CoreWindowStack          *stack,
                               int                       dx,
                               int                       dy )
{
     int            new_cx, new_cy;
     DFBWindowEvent we;

     DFB_ASSERT( stack != NULL );

     stack_lock( stack );

     if (!stack->cursor.enabled) {
          stack_unlock( stack );
          return;
     }

     new_cx = MIN( stack->cursor.x + dx, stack->cursor.region.x2);
     new_cy = MIN( stack->cursor.y + dy, stack->cursor.region.y2);

     new_cx = MAX( new_cx, stack->cursor.region.x1 );
     new_cy = MAX( new_cy, stack->cursor.region.y1 );

     if (new_cx == stack->cursor.x  &&  new_cy == stack->cursor.y) {
          stack_unlock( stack );
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

                    dfb_window_dispatch( stack->pointer_window, &we );
               }
               else {
                    if (!handle_enter_leave_focus( stack )
                        && stack->entered_window) {
                         we.type = DWET_MOTION;
                         we.x    = stack->cursor.x - stack->entered_window->x;
                         we.y    = stack->cursor.y - stack->entered_window->y;

                         dfb_window_dispatch( stack->entered_window, &we );
                    }
               }

               break;

          default:
               ;
     }

     HEAVYDEBUGMSG("DirectFB/windows: mouse at %d, %d\n", stack->cursor.x, stack->cursor.y);

     stack_unlock( stack );
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

     dev = shcalloc( 1, sizeof(StackDevice) );
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
                    DFBRectangle rect = { region->x1, region->y1,
                                          region->x2 - region->x1 + 1,
                                          region->y2 - region->y1 + 1 };

                    state->color     = stack->bg.color;
                    state->modified |= SMF_COLOR;

                    dfb_gfxcard_fillrectangle( &rect, state );
                    break;
               }
          case DLBM_IMAGE: {
                    DFBRectangle rect = { region->x1, region->y1,
                                          region->x2 - region->x1 + 1,
                                          region->y2 - region->y1 + 1 };

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
                                          stack->bg.image->height };

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
     DFBRegion region = { x1, y1, x2, y2 };

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

          if ((window->opacity < 0xff) ||
              (window->options & DWOP_COLORKEYING) ||
              (window->options &
               (DWOP_ALPHACHANNEL | DWOP_OPAQUE_REGION)) == DWOP_ALPHACHANNEL)
          {
               update_region( stack, state, i-1, x1, y1, x2, y2 );

               draw_window( window, state, &region, true );
          }
          else {
               DFBRegion opaque = region;

               if ((window->options & DWOP_ALPHACHANNEL) &&
                   !dfb_region_intersect( &opaque,
                                          window->x + window->opaque.x1,
                                          window->y + window->opaque.y1,
                                          window->x + window->opaque.x2,
                                          window->y + window->opaque.y2 ))
               {
                    update_region( stack, state, i-1, x1, y1, x2, y2 );

                    draw_window( window, state, &region, true );
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


                    if (window->options & DWOP_ALPHACHANNEL) {
                         /* left */
                         if (opaque.x1 != region.x1) {
                              DFBRegion r = { region.x1, opaque.y1,
                                              opaque.x1 - 1, opaque.y2 };
                              draw_window( window, state, &r, true );
                         }
                         
                         /* upper */
                         if (opaque.y1 != region.y1) {
                              DFBRegion r = { region.x1, region.y1,
                                              region.x2, opaque.y1 - 1 };
                              draw_window( window, state, &r, true );
                         }
                         
                         /* right */
                         if (opaque.x2 != region.x2) {
                              DFBRegion r = { opaque.x2 + 1, opaque.y1,
                                              region.x2, opaque.y2 };
                              draw_window( window, state, &r, true );
                         }
                         
                         /* lower */
                         if (opaque.y2 != region.y2) {
                              DFBRegion r = { region.x1, opaque.y2 + 1,
                                              region.x2, region.y2 };
                              draw_window( window, state, &r, true );
                         }
                    }

                    draw_window( window, state, &opaque, false );
               }
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
              x >= w->x  &&  x < w->x+w->width &&
              y >= w->y  &&  y < w->y+w->height)
               return w;
     }

     return NULL;
}

ReactionResult
_dfb_window_stack_inputdevice_react( const void *msg_data,
                                     void       *ctx )
{
     const DFBInputEvent *evt = (DFBInputEvent*)msg_data;

     DFBWindowEvent   we;
     CoreWindow      *window = NULL;
     CoreWindowStack *stack  = (CoreWindowStack*)ctx;
     DisplayLayer    *layer  = dfb_layer_at( stack->layer_id );

     /* FIXME: this is a bad check for exclusive access */
     if (dfb_layer_lease( layer ) )
          return RS_OK;

     dfb_layer_release( layer, false );

     stack_lock( stack );

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
                              handle_enter_leave_focus( stack );
                              break;

                         case DIKS_ALT:
                         case DIKS_CONTROL:
                              stack->wm_hack = 1;
                              stack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_A:
                         case DIKS_SMALL_C:
                         case DIKS_SMALL_S:
                         case DIKS_SMALL_X:
                         case DIKS_SMALL_D:
                         case DIKS_SMALL_P:
                              stack_unlock( stack );
                              return RS_OK;

                         default:
                              ;
                    }
                    break;

               case DIET_KEYPRESS:
                    switch (DFB_LOWER_CASE(evt->key_symbol)) {
                         case DIKS_ALT:
                              stack->wm_hack = 3;
                              stack_unlock( stack );
                              return RS_OK;

                         case DIKS_CONTROL:
                              stack->wm_hack = 2;
                              stack_unlock( stack );
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

                                        switch_focus( stack, window );

                                        break;
                                   }

                                   stack->wm_cycle = index;
                              }
                              stack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_C:
                              if (stack->entered_window) {
                                   DFBWindowEvent evt;
                                   evt.type = DWET_CLOSE;
                                   dfb_window_dispatch( stack->entered_window, &evt );
                              }
                              stack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_A:
                              if (stack->focused_window && 
                                  ! (stack->focused_window->options & DWOP_KEEP_STACKING)) {
                                   dfb_window_lowertobottom( stack->focused_window );
                                   switch_focus( stack, window_at_pointer( stack, -1, -1 ) );
                              }
                              stack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_S:
                              if (stack->focused_window && 
                                  ! (stack->focused_window->options & DWOP_KEEP_STACKING)) {
                                   dfb_window_raisetotop( stack->focused_window );
                              }
                              stack_unlock( stack );
                              return RS_OK;

                         case DIKS_SMALL_D: {
                                   CoreWindow *window = stack->entered_window;

                                   if (window &&
                                       !(window->options & DWOP_INDESTRUCTIBLE)) {
                                        dfb_window_deinit( window );
                                        dfb_window_destroy( window, false );
                                   }

                                   stack_unlock( stack );
                                   return RS_OK;
                              }

                         case DIKS_SMALL_P:
                              dfb_layer_cursor_set_opacity( layer, 0xff );
                              dfb_layer_cursor_enable( layer, true );

                              stack_unlock( stack );
                              return RS_OK;

                         default:
                              ;
                    }
                    break;

               case DIET_BUTTONRELEASE:
                    stack_unlock( stack );
                    return RS_OK;

               case DIET_BUTTONPRESS:
                    if (stack->entered_window &&
                        !(stack->entered_window->options & DWOP_KEEP_STACKING))
                         dfb_window_raisetotop( stack->entered_window );
                    stack_unlock( stack );
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

                    dfb_window_dispatch( window, &we );
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

                    dfb_window_dispatch( window, &we );
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
                              stack_unlock( stack );
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
                              stack_unlock( stack );
                              return RS_OK;
                    }
               }
               break;
          default:
               break;
     }

     stack_unlock( stack );
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

               dfb_window_dispatch( window, &we );
          }
     }
}

static int
handle_enter_leave_focus( CoreWindowStack *stack )
{
     /* if pointer is not grabbed */
     if (!stack->pointer_window) {
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

                    dfb_window_dispatch( before, &we );
               }

               /* switch focus and send enter event */
               switch_focus( stack, after );

               if (after) {
                    we.type = DWET_ENTER;
                    we.x    = stack->cursor.x - after->x;
                    we.y    = stack->cursor.y - after->y;

                    dfb_window_dispatch( after, &we );
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

static int
get_window_index( CoreWindow *window )
{
     int               i;
     CoreWindowStack  *stack   = window->stack;
     int               num     = stack->num_windows;
     CoreWindow      **windows = stack->windows;

     for (i=0; i<num; i++)
          if (windows[i] == window)
               return i;

     BUG( "window not found" );

     return -1;
}

static void
window_insert( CoreWindow *window,
               int         before )
{
     int              i;
     DFBWindowEvent   evt;
     CoreWindowStack *stack = window->stack;

     DFB_ASSERT( window->stack != NULL );

     if (!window->initialized) {
          if (before < 0  ||  before > stack->num_windows)
               before = stack->num_windows;

          stack->windows = shrealloc( stack->windows,
                                      sizeof(CoreWindow*) * (stack->num_windows+1) );

          for (i=stack->num_windows; i>before; i--)
               stack->windows[i] = stack->windows[i-1];

          stack->windows[before] = window;

          stack->num_windows++;

          window->initialized = true;
     }

     /* Send configuration */
     evt.type = DWET_POSITION_SIZE;
     evt.x    = window->x;
     evt.y    = window->y;
     evt.w    = window->width;
     evt.h    = window->height;
     dfb_window_dispatch( window, &evt );

     if (window->opacity)
          handle_enter_leave_focus( stack );
}

static void
window_remove( CoreWindow *window )
{
     int i;
     CoreWindowStack *stack = window->stack;
     DFBRegion region = { window->x, window->y,
          window->x + window->width - 1,
          window->y + window->height - 1};

     DFB_ASSERT( window->stack != NULL );

     window_withdraw( window );

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i] == window)
               break;

     if (i < stack->num_windows) {
          stack->num_windows--;

          for (; i<stack->num_windows; i++)
               stack->windows[i] = stack->windows[i+1];

          if (stack->num_windows) {
               stack->windows =
               shrealloc( stack->windows,
                          sizeof(CoreWindow*) * stack->num_windows );
          }
          else {
               shfree( stack->windows );
               stack->windows = NULL;
          }
     }

     window->initialized = false;

     /* If window was visible... */
     if (window->opacity) {
          /* Update the affected region */
          repaint_stack( stack, &region, 0 );

          /* Possibly change focus to window now under the cursor */
          handle_enter_leave_focus( stack );

          /* Always try to have a focused window */
          ensure_focus( stack );
     }

//     window->stack = NULL;
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
     if (window->opacity) {
          CoreWindowStack *stack  = window->stack;
          DFBRegion        region = { window->x, window->y,
               window->x + window->width - 1,
               window->y + window->height - 1};

          repaint_stack( stack, &region, 0 );

          /* Possibly change focus to window now under the cursor */
          handle_enter_leave_focus( stack );
     }
}

static void
window_withdraw( CoreWindow *window )
{
     int              i;
     FusionLink      *l;
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
          if (stack->keys[i].code != -1 &&
              stack->keys[i].owner == window) {
               DFBWindowEvent we;

               we.type       = DWET_KEYUP;
               we.key_code   = stack->keys[i].code;
               we.key_id     = stack->keys[i].id;
               we.key_symbol = stack->keys[i].symbol;

               dfb_window_dispatch( window, &we );

               stack->keys[i].code = -1;
          }
     }

     l = stack->grabbed_keys;
     while (l) {
          FusionLink *next = l->next;
          GrabbedKey *key  = (GrabbedKey*) l;

          if (key->owner == window) {
               fusion_list_remove( &stack->grabbed_keys, &key->link );
               shfree( key );
          }

          l = next;
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
          dfb_window_dispatch( from, &evt );
     }

     if (to) {
          if (to->surface && to->surface->palette) {
               DisplayLayer *layer   = dfb_layer_at( stack->layer_id );
               CoreSurface  *surface = dfb_layer_surface( layer );

               if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
                    dfb_surface_set_palette (dfb_layer_surface (layer),
                                             to->surface->palette);
          }

          evt.type = DWET_GOTFOCUS;
          dfb_window_dispatch( to, &evt );
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
          return stack->keyboard_window ?
          stack->keyboard_window : stack->focused_window;

     /* Implicitly grab (press) key or ungrab (release) it. */
     if (evt->type == DIET_KEYPRESS) {
          int         i;
          int         free_key = -1;
          CoreWindow *window;

          for (i=0; i<8; i++) {
               if (stack->keys[i].code == evt->key_code)
                    return stack->keys[i].owner;

               if (free_key == -1 && stack->keys[i].code == -1)
                    free_key = i;
          }

          if (free_key == -1) {
               CAUTION( "maximum number of owned keys reached" );
               return NULL;
          }

          window = stack->keyboard_window ?
                   stack->keyboard_window : stack->focused_window;

          stack->keys[free_key].symbol = evt->key_symbol;
          stack->keys[free_key].id     = evt->key_id;
          stack->keys[free_key].code   = evt->key_code;
          stack->keys[free_key].owner  = window;

          return window;
     }
     else {
          int i;

          for (i=0; i<8; i++) {
               if (stack->keys[i].code == evt->key_code) {
                    stack->keys[i].code = -1;

                    return stack->keys[i].owner;
               }
          }
     }

     return NULL;
}

