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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/shmalloc.h>
#include <fusion/vector.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layers_internal.h>
#include <core/surfaces.h>
#include <core/palette.h>
#include <core/windows.h>
#include <core/windows_internal.h>
#include <core/windowstack.h>
#include <core/wm.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <core/wm_module.h>

#include "stret.h"

D_DEBUG_DOMAIN( WM_Default, "WM/Default", "Default window manager module" );


DFB_WINDOW_MANAGER( default )


typedef struct {
     DirectLink                    link;

     DFBInputDeviceKeySymbol       symbol;
     DFBInputDeviceModifierMask    modifiers;

     CoreWindow                   *owner;
} GrabbedKey;

/**************************************************************************************************/

typedef struct {
     StretRegionClassID            root_class;
     StretRegionClassID            window_class;
} WMShared;

typedef struct {
     CoreDFB                      *core;

     WMShared                     *shared;
} WMData;

typedef struct {
     int                           magic;

     CoreWindowStack              *stack;

     StretRegion                  *root;

     DFBInputDeviceButtonMask      buttons;
     DFBInputDeviceModifierMask    modifiers;
     DFBInputDeviceLockState       locks;

     int                           wm_level;
     int                           wm_cycle;

     FusionVector                  windows;

     CoreWindow                   *pointer_window;     /* window grabbing the pointer */
     CoreWindow                   *keyboard_window;    /* window grabbing the keyboard */
     CoreWindow                   *focused_window;     /* window having the focus */
     CoreWindow                   *entered_window;     /* window under the pointer */

     DirectLink                   *grabbed_keys;       /* List of currently grabbed keys. */

     struct {
          DFBInputDeviceKeySymbol      symbol;
          DFBInputDeviceKeyIdentifier  id;
          int                          code;
          CoreWindow                  *owner;
     } keys[8];
} StackData;

typedef struct {
     int                           magic;

     CoreWindow                   *window;

     StretRegion                  *region;

     StackData                    *stack_data;

     int                           priority;           /* derived from stacking class */

     CoreLayerRegionConfig         config;
} WindowData;

/**************************************************************************************************/

static void
post_event( CoreWindow     *window,
            StackData      *data,
            DFBWindowEvent *event )
{
     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );

     event->buttons   = data->buttons;
     event->modifiers = data->modifiers;
     event->locks     = data->locks;

     dfb_window_post_event( window, event );
}

static void
send_key_event( CoreWindow          *window,
                StackData           *data,
                const DFBInputEvent *event )
{
     DFBWindowEvent we;

     D_ASSERT( window != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );

     we.type       = (event->type == DIET_KEYPRESS) ? DWET_KEYDOWN : DWET_KEYUP;
     we.key_code   = event->key_code;
     we.key_id     = event->key_id;
     we.key_symbol = event->key_symbol;

     post_event( window, data, &we );
}

static void
send_button_event( CoreWindow          *window,
                   StackData           *data,
                   const DFBInputEvent *event )
{
     DFBWindowEvent we;

     D_ASSERT( window != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );

     we.type   = (event->type == DIET_BUTTONPRESS) ? DWET_BUTTONDOWN : DWET_BUTTONUP;
     we.x      = window->stack->cursor.x - window->x;
     we.y      = window->stack->cursor.y - window->y;
     we.button = (data->wm_level & 2) ? (event->button + 2) : event->button;

     post_event( window, data, &we );
}

/**************************************************************************************************/

static inline int
get_priority( const CoreWindow *window )
{
     D_ASSERT( window != NULL );

     if (window->caps & DWHC_TOPMOST)
          return 2;

     switch (window->stacking) {
          case DWSC_UPPER:
               return  1;

          case DWSC_MIDDLE:
               return  0;

          case DWSC_LOWER:
               return -1;

          default:
               D_BUG( "unknown stacking class" );
               break;
     }

     return 0;
}

static inline int
get_index( const StackData  *data,
           const CoreWindow *window )
{
     D_ASSERT( data != NULL );
     D_ASSERT( window != NULL );

     D_ASSERT( fusion_vector_contains( &data->windows, window ) );

     return fusion_vector_index_of( &data->windows, window );
}

static CoreWindow *
get_keyboard_window( CoreWindowStack     *stack,
                     StackData           *data,
                     const DFBInputEvent *event )
{
     DirectLink *l;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS || event->type == DIET_KEYRELEASE );

     /* Check explicit key grabs first. */
     direct_list_foreach (l, data->grabbed_keys) {
          GrabbedKey *key = (GrabbedKey*) l;

          if (key->symbol    == event->key_symbol &&
              key->modifiers == data->modifiers)
               return key->owner;
     }

     /* Don't do implicit grabs on keys without a hardware index. */
     if (event->key_code == -1)
          return (data->keyboard_window ?
                  data->keyboard_window : data->focused_window);

     /* Implicitly grab (press) or ungrab (release) key. */
     if (event->type == DIET_KEYPRESS) {
          int         i;
          int         free_key = -1;
          CoreWindow *window;

          /* Check active grabs. */
          for (i=0; i<8; i++) {
               /* Key is grabbed, send to owner (NULL if destroyed). */
               if (data->keys[i].code == event->key_code)
                    return data->keys[i].owner;

               /* Remember first free array item. */
               if (free_key == -1 && data->keys[i].code == -1)
                    free_key = i;
          }

          /* Key is not grabbed, check for explicit keyboard grab or focus. */
          window = data->keyboard_window ?
                   data->keyboard_window : data->focused_window;
          if (!window)
               return NULL;

          /* Check if a free array item was found. */
          if (free_key == -1) {
               D_WARN( "maximum number of owned keys reached" );
               return NULL;
          }

          /* Implicitly grab the key. */
          data->keys[free_key].symbol = event->key_symbol;
          data->keys[free_key].id     = event->key_id;
          data->keys[free_key].code   = event->key_code;
          data->keys[free_key].owner  = window;

          return window;
     }
     else {
          int i;

          /* Lookup owner and ungrab the key. */
          for (i=0; i<8; i++) {
               if (data->keys[i].code == event->key_code) {
                    data->keys[i].code = -1;

                    /* Return owner (NULL if destroyed). */
                    return data->keys[i].owner;
               }
          }
     }

     /* No owner for release event found, discard it. */
     return NULL;
}

static CoreWindow*
window_at_pointer( CoreWindowStack *stack,
                   StackData       *data,
                   int              x,
                   int              y )
{
     int         i;
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );

     if (!stack->cursor.enabled) {
          fusion_vector_foreach_reverse (window, i, data->windows)
               if (window->opacity && !(window->options & DWOP_GHOST))
                    return window;

          return NULL;
     }

     if (x < 0)
          x = stack->cursor.x;
     if (y < 0)
          y = stack->cursor.y;

     fusion_vector_foreach_reverse (window, i, data->windows) {
          if (!(window->options & DWOP_GHOST) && window->opacity &&
              x >= window->x  &&  x < window->x + window->width &&
              y >= window->y  &&  y < window->y + window->height)
          {
               int wx = x - window->x;
               int wy = y - window->y;

               if ( !(window->options & DWOP_SHAPED)  ||
                    !(window->options &(DWOP_ALPHACHANNEL|DWOP_COLORKEYING))
                    || !window->surface ||
                    ((window->options & DWOP_OPAQUE_REGION) &&
                     (wx >= window->opaque.x1  &&  wx <= window->opaque.x2 &&
                      wy >= window->opaque.y1  &&  wy <= window->opaque.y2)))
               {
                    return window;
               }
               else {
                    void        *data;
                    int          pitch;
                    CoreSurface *surface = window->surface;

                    if ( dfb_surface_soft_lock( surface, DSLF_READ,
                                                &data, &pitch, true ) == DFB_OK ) {
                         if (window->options & DWOP_ALPHACHANNEL) {
                              int alpha = -1;

                              D_ASSERT( DFB_PIXELFORMAT_HAS_ALPHA( surface->format ) );

                              switch (surface->format) {
                                   case DSPF_AiRGB:
                                        alpha = 0xff - (*(__u32*)(data + 4 * wx +
                                                                  pitch * wy) >> 24);
                                        break;
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
                                   return window;
                              }

                         }
                         if (window->options & DWOP_COLORKEYING) {
                              int pixel = 0;

                              switch (surface->format) {
                                   case DSPF_ARGB:
                                   case DSPF_AiRGB:
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

                              if ( pixel != window->color_key ) {
                                   dfb_surface_unlock( surface, true );
                                   return window;
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
switch_focus( CoreWindowStack *stack,
              StackData       *data,
              CoreWindow      *to )
{
     DFBWindowEvent  evt;
     CoreWindow     *from;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );

     from = data->focused_window;

     if (from == to)
          return;

     if (from) {
          evt.type = DWET_LOSTFOCUS;

          post_event( from, data, &evt );
     }

     if (to) {
          if (to->surface && to->surface->palette && !stack->hw_mode) {
               CoreSurface *surface;

               D_ASSERT( to->primary_region != NULL );

               if (dfb_layer_region_get_surface( to->primary_region, &surface ) == DFB_OK) {
                    if (DFB_PIXELFORMAT_IS_INDEXED( surface->format ))
                         dfb_surface_set_palette( surface, to->surface->palette );

                    dfb_surface_unref( surface );
               }
          }

          evt.type = DWET_GOTFOCUS;

          post_event( to, data, &evt );
     }

     data->focused_window = to;
}

static bool
update_focus( CoreWindowStack *stack,
              StackData       *data )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );

     /* if pointer is not grabbed */
     if (!data->pointer_window) {
          CoreWindow *before = data->entered_window;
          CoreWindow *after  = window_at_pointer( stack, data, -1, -1 );

          /* and the window under the cursor is another one now */
          if (before != after) {
               DFBWindowEvent we;

               /* send leave event */
               if (before) {
                    we.type = DWET_LEAVE;
                    we.x    = stack->cursor.x - before->x;
                    we.y    = stack->cursor.y - before->y;

                    post_event( before, data, &we );
               }

               /* switch focus and send enter event */
               switch_focus( stack, data, after );

               if (after) {
                    we.type = DWET_ENTER;
                    we.x    = stack->cursor.x - after->x;
                    we.y    = stack->cursor.y - after->y;

                    post_event( after, data, &we );
               }

               /* update pointer to window under the cursor */
               data->entered_window = after;

               return true;
          }
     }

     return false;
}

static void
ensure_focus( CoreWindowStack *stack,
              StackData       *data )
{
     int         i;
     CoreWindow *window;

     if (data->focused_window)
          return;

     fusion_vector_foreach_reverse (window, i, data->windows) {
          if (window->opacity && !(window->options & DWOP_GHOST)) {
               switch_focus( stack, data, window );
               break;
          }
     }
}

/**************************************************************************************************/
/**************************************************************************************************/

static void
repaint_stack( CoreWindowStack     *stack,
               StackData           *data,
               CoreLayerRegion     *region,
               const DFBRegion     *updates,
               int                  num,
               DFBSurfaceFlipFlags  flags )
{
     int        i;
     CoreLayer *layer;
     CardState *state;

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->context != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( region != NULL );
     D_ASSERT( updates != NULL );
     D_ASSERT( num > 0 );

     layer = dfb_layer_at( stack->context->layer_id );
     state = &layer->state;

     if (!stack->active || !region->surface)
          return;

     D_DEBUG_AT( WM_Default, "repaint_stack( num %d, flags 0x%08x )\n", num, flags );

     /* Set destination. */
     state->destination  = region->surface;
     state->modified    |= SMF_DESTINATION;

     for (i=0; i<num; i++) {
          const DFBRegion *update = &updates[i];

          D_DEBUG_AT( WM_Default, "    (%2d) %4d, %4d - %4dx%4d\n", i,
                      DFB_RECTANGLE_VALS_FROM_REGION( update ) );

          /* Set clipping region. */
          dfb_state_set_clip( state, update );

          /* Compose updated region. */
          stret_region_update( data->root, update, state );
     }

     /* Reset destination. */
     state->destination  = NULL;
     state->modified    |= SMF_DESTINATION;


     for (i=0; i<num; i++) {
          const DFBRegion *update = &updates[i];

          /* Flip the updated region .*/
          dfb_layer_region_flip_update( region, update, flags );
     }
}

/**************************************************************************************************/

static DFBResult
update_stack( CoreWindowStack     *stack,
              StackData           *data,
              DFBRegion           *region,
              DFBSurfaceFlipFlags  flags )
{
     DFBResult        ret;
     CoreLayerRegion *primary;

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->context != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( region != NULL );

     if (stack->hw_mode)
          return DFB_OK;

     if (!dfb_unsafe_region_intersect( region, 0, 0, stack->width - 1, stack->height - 1 ))
          return DFB_OK;

     /* Get the primary region. */
     ret = dfb_layer_context_get_primary_region( stack->context, false, &primary );
     if (ret)
          return ret;

     repaint_stack( stack, data, primary, region, 1, flags );

     /* Unref primary region. */
     dfb_layer_region_unref( primary );

     return DFB_OK;
}

static DFBResult
update_window( CoreWindow          *window,
               WindowData          *window_data,
               DFBRegion           *region,
               DFBSurfaceFlipFlags  flags,
               bool                 force_complete,
               bool                 force_invisible )
{
     StackData       *data;
     CoreWindowStack *stack;
     DFBRegion        regions[32];
     int              num_regions;

     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( window_data->stack_data != NULL );
     D_ASSERT( window_data->stack_data->stack != NULL );
     D_ASSERT( region != NULL );

     data  = window_data->stack_data;
     stack = data->stack;

     if (!VISIBLE_WINDOW(window) && !force_invisible)
          return DFB_OK;

     if (stack->hw_mode)
          return DFB_OK;

     if (!dfb_unsafe_region_intersect( region,
                                       - window->x, - window->y,
                                       - window->x + stack->width - 1,
                                       - window->y + stack->height - 1 ))
          return DFB_OK;

     if (force_complete || stret_region_visible( window_data->region,
                                                 region, regions, 32, &num_regions ))
     {
          dfb_region_translate( region, window->x, window->y );
          repaint_stack( stack, data, window->primary_region, region, 1, flags );
     }
     else if (num_regions > 0)
          repaint_stack( stack, data, window->primary_region, regions, num_regions, flags );

     return DFB_OK;
}

/**************************************************************************************************/
/**************************************************************************************************/

static void
insert_window( CoreWindowStack *stack,
               StackData       *data,
               CoreWindow      *window,
               WindowData      *window_data )
{
     int         index;
     CoreWindow *other;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );

     /*
      * Iterate from bottom to top,
      * stopping at the first window with a higher priority.
      */
     fusion_vector_foreach (other, index, data->windows) {
          WindowData *other_data = other->window_data;

          D_ASSERT( other->window_data != NULL );

          if (other->caps & DWHC_TOPMOST || other_data->priority > window_data->priority)
               break;
     }

     /* Insert the window at the acquired position. */
     fusion_vector_insert( &data->windows, window, index );

     stret_region_restack( window_data->region, index );
}

static void
withdraw_window( CoreWindowStack *stack,
                 StackData       *data,
                 CoreWindow      *window,
                 WindowData      *window_data )
{
     int i;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );

     D_ASSERT( window->stack != NULL );

     D_ASSERT( DFB_WINDOW_INITIALIZED( window ) );

     /* No longer be the 'entered window'. */
     if (data->entered_window == window)
          data->entered_window = NULL;

     /* Remove focus from window. */
     if (data->focused_window == window)
          data->focused_window = NULL;

     /* Release explicit keyboard grab. */
     if (data->keyboard_window == window)
          data->keyboard_window = NULL;

     /* Release explicit pointer grab. */
     if (data->pointer_window == window)
          data->pointer_window = NULL;

     /* Release all implicit key grabs. */
     for (i=0; i<8; i++) {
          if (data->keys[i].code != -1 && data->keys[i].owner == window) {
               if (!DFB_WINDOW_DESTROYED( window )) {
                    DFBWindowEvent we;

                    we.type       = DWET_KEYUP;
                    we.key_code   = data->keys[i].code;
                    we.key_id     = data->keys[i].id;
                    we.key_symbol = data->keys[i].symbol;

                    post_event( window, data, &we );
               }

               data->keys[i].code  = -1;
               data->keys[i].owner = NULL;
          }
     }
}

static void
remove_window( CoreWindowStack *stack,
               StackData       *data,
               CoreWindow      *window,
               WindowData      *window_data )
{
     DirectLink *l, *n;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );

     D_ASSERT( window->opacity == 0 );
     D_ASSERT( DFB_WINDOW_INITIALIZED( window ) );

     D_ASSERT( fusion_vector_contains( &data->windows, window ) );

     /* Release implicit grabs, focus etc. */
     withdraw_window( stack, data, window, window_data );

     /* Release all explicit key grabs. */
     direct_list_foreach_safe (l, n, data->grabbed_keys) {
          GrabbedKey *key = (GrabbedKey*) l;

          if (key->owner == window) {
               direct_list_remove( &data->grabbed_keys, &key->link );
               SHFREE( key );
          }
     }

     fusion_vector_remove( &data->windows, fusion_vector_index_of( &data->windows, window ) );
}

/**************************************************************************************************/

static DFBResult
move_window( CoreWindow *window,
             WindowData *data,
             int         dx,
             int         dy )
{
     DFBWindowEvent evt;

     window->x += dx;
     window->y += dy;

     stret_region_move( data->region, dx, dy );

     if (window->region) {
          data->config.dest.x += dx;
          data->config.dest.y += dy;

          dfb_layer_region_set_configuration( window->region, &data->config, CLRCF_DEST );
     }
     else if (VISIBLE_WINDOW(window)) {
          DFBRegion region = { 0, 0, window->width - 1, window->height - 1 };

          if (dx > 0)
               region.x1 -= dx;
          else if (dx < 0)
               region.x2 -= dx;

          if (dy > 0)
               region.y1 -= dy;
          else if (dy < 0)
               region.y2 -= dy;

          update_window( window, data, &region, 0, false, false );
     }

     /* Send new position */
     evt.type = DWET_POSITION;
     evt.x    = window->x;
     evt.y    = window->y;

     post_event( window, data->stack_data, &evt );

     return DFB_OK;
}

static DFBResult
resize_window( CoreWindow *window,
               WMData     *wm_data,
               WindowData *window_data,
               int         width,
               int         height )
{
     DFBResult        ret;
     DFBWindowEvent   evt;
     CoreWindowStack *stack = window->stack;
     int              ow    = window->width;
     int              oh    = window->height;

     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     if (width > 4096 || height > 4096)
          return DFB_LIMITEXCEEDED;

     if (window->surface) {
          ret = dfb_surface_reformat( wm_data->core, window->surface,
                                      width, height, window->surface->format );
          if (ret)
               return ret;
     }

     window->width  = width;
     window->height = height;

     dfb_region_intersect( &window->opaque, 0, 0, width - 1, height - 1 );

     stret_region_resize( window_data->region, width, height );

     if (VISIBLE_WINDOW (window)) {
          if (ow > window->width) {
               DFBRegion region = { window->width, 0, ow - 1, MIN(window->height, oh) - 1 };

               update_window( window, window_data, &region, 0, false, false );
          }

          if (oh > window->height) {
               DFBRegion region = { 0, window->height, MAX(window->width, ow) - 1, oh - 1 };

               update_window( window, window_data, &region, 0, false, false );
          }
     }

     /* Send new size */
     evt.type = DWET_SIZE;
     evt.w    = window->width;
     evt.h    = window->height;

     post_event( window, window_data->stack_data, &evt );

     update_focus( stack, window_data->stack_data );

     return DFB_OK;
}

static DFBResult
restack_window( CoreWindow             *window,
                WindowData             *window_data,
                CoreWindow             *relative,
                WindowData             *relative_data,
                int                     relation,
                DFBWindowStackingClass  stacking )
{
     StackData *data;
     int        old;
     int        index;
     int        priority;

     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( window_data->stack_data != NULL );

     D_ASSERT( relative == NULL || relative_data != NULL );

     D_ASSERT( relative == NULL || relative == window || relation != 0);

     data = window_data->stack_data;

     /* Change stacking class. */
     if (stacking != window->stacking) {
          window->stacking      = stacking;
          window_data->priority = get_priority( window );
     }

     /* Get the (new) priority. */
     priority = window_data->priority;

     /* Get the old index. */
     old = get_index( data, window );

     /* Calculate the desired index. */
     if (relative) {
          index = get_index( data, relative );

          if (relation > 0) {
               if (old < index)
                    index--;
          }
          else if (relation < 0) {
               if (old > index)
                    index++;
          }

          index += relation;

          if (index < 0)
               index = 0;
          else if (index > data->windows.count - 1)
               index = data->windows.count - 1;
     }
     else if (relation)
          index = data->windows.count - 1;
     else
          index = 0;

     /* Assure window won't be above any window with a higher priority. */
     while (index > 0) {
          int         below      = (old < index) ? index : index - 1;
          CoreWindow *other      = fusion_vector_at( &data->windows, below );
          WindowData *other_data = other->window_data;

          D_ASSERT( other->window_data != NULL );

          if (priority < other_data->priority)
               index--;
          else
               break;
     }

     /* Assure window won't be below any window with a lower priority. */
     while (index < data->windows.count - 1) {
          int         above      = (old > index) ? index : index + 1;
          CoreWindow *other      = fusion_vector_at( &data->windows, above );
          WindowData *other_data = other->window_data;

          D_ASSERT( other->window_data != NULL );

          if (priority > other_data->priority)
               index++;
          else
               break;
     }

     /* Return if index hasn't changed. */
     if (index == old)
          return DFB_OK;

     /* Actually change the stacking order now. */
     fusion_vector_move( &data->windows, old, index );

     stret_region_restack( window_data->region, index );

     dfb_window_repaint( window, NULL, 0, true, false );

     return DFB_OK;
}

static void
set_opacity( CoreWindow *window,
             WindowData *window_data,
             __u8        opacity )
{
     __u8             old;
     StackData       *data;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( window_data->stack_data != NULL );
     D_ASSERT( window_data->stack_data->stack != NULL );

     old   = window->opacity;
     data  = window_data->stack_data;
     stack = data->stack;

     if (!stack->hw_mode && !dfb_config->translucent_windows && opacity)
          opacity = 0xFF;

     if (old != opacity) {
          bool show = !old && opacity;
          bool hide = old && !opacity;

          window->opacity = opacity;

          if (show)
               stret_region_enable( window_data->region, SRF_ACTIVE );

          if (! (window->options & (DWOP_ALPHACHANNEL | DWOP_COLORKEYING))) {
               if (opacity == 0xff && old != 0xff)
                    stret_region_enable( window_data->region, SRF_OPAQUE );
               else if (opacity != 0xff && old == 0xff)
                    stret_region_disable( window_data->region, SRF_OPAQUE );
          }

          if (window->region) {
               window_data->config.opacity = opacity;

               dfb_layer_region_set_configuration( window->region, &window_data->config, CLRCF_OPACITY );
          }
          else
               dfb_window_repaint( window, NULL, 0, false, true );


          /* Check focus after window appeared or disappeared */
          if (show || hide)
               update_focus( stack, data );

          /* If window disappeared... */
          if (hide) {
               stret_region_disable( window_data->region, SRF_ACTIVE );

               /* Ungrab pointer/keyboard */
               withdraw_window( stack, data, window, window_data );

               /* Always try to have a focused window */
               ensure_focus( stack, data );
          }
     }
}

static DFBResult
grab_keyboard( CoreWindow *window,
               WindowData *window_data )
{
     StackData *data;

     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( window_data->stack_data != NULL );

     data = window_data->stack_data;

     if (data->keyboard_window)
          return DFB_LOCKED;

     data->keyboard_window = window;

     return DFB_OK;
}

static DFBResult
ungrab_keyboard( CoreWindow *window,
                 WindowData *window_data )
{
     StackData *data;

     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( window_data->stack_data != NULL );

     data = window_data->stack_data;

     if (data->keyboard_window == window)
          data->keyboard_window = NULL;

     return DFB_OK;
}

static DFBResult
grab_pointer( CoreWindow *window,
              WindowData *window_data )
{
     StackData *data;

     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( window_data->stack_data != NULL );

     data = window_data->stack_data;

     if (data->pointer_window)
          return DFB_LOCKED;

     data->pointer_window = window;

     return DFB_OK;
}

static DFBResult
ungrab_pointer( CoreWindow *window,
                WindowData *window_data )
{
     StackData       *data;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( window_data->stack_data != NULL );

     data  = window_data->stack_data;
     stack = data->stack;

     if (data->pointer_window == window) {
          data->pointer_window = NULL;

          /* Possibly change focus to window that's now under the cursor */
          update_focus( stack, data );
     }

     return DFB_OK;
}

static DFBResult
grab_key( CoreWindow                 *window,
          WindowData                 *window_data,
          DFBInputDeviceKeySymbol     symbol,
          DFBInputDeviceModifierMask  modifiers )
{
     int         i;
     StackData  *data;
     GrabbedKey *grab;

     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( window_data->stack_data != NULL );

     data = window_data->stack_data;

     /* Reject if already grabbed. */
     direct_list_foreach (grab, data->grabbed_keys)
          if (grab->symbol == symbol && grab->modifiers == modifiers)
               return DFB_LOCKED;

     /* Allocate grab information. */
     grab = SHCALLOC( 1, sizeof(GrabbedKey) );

     /* Fill grab information. */
     grab->symbol    = symbol;
     grab->modifiers = modifiers;
     grab->owner     = window;

     /* Add to list of key grabs. */
     direct_list_append( &data->grabbed_keys, &grab->link );

     /* Remove implicit grabs for this key. */
     for (i=0; i<8; i++)
          if (data->keys[i].code != -1 && data->keys[i].symbol == symbol)
               data->keys[i].code = -1;

     return DFB_OK;
}

static DFBResult
ungrab_key( CoreWindow                 *window,
            WindowData                 *window_data,
            DFBInputDeviceKeySymbol     symbol,
            DFBInputDeviceModifierMask  modifiers )
{
     DirectLink *l;
     StackData  *data;

     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( window_data->stack_data != NULL );

     data = window_data->stack_data;

     direct_list_foreach (l, data->grabbed_keys) {
          GrabbedKey *key = (GrabbedKey*) l;

          if (key->symbol == symbol && key->modifiers == modifiers && key->owner == window) {
               direct_list_remove( &data->grabbed_keys, &key->link );
               SHFREE( key );
               return DFB_OK;
          }
     }

     return DFB_IDNOTFOUND;
}

static DFBResult
request_focus( CoreWindow *window,
               WindowData *window_data )
{
     StackData       *data;
     CoreWindowStack *stack;
     CoreWindow      *entered;

     D_ASSERT( window != NULL );
     D_ASSERT( !(window->options & DWOP_GHOST) );
     D_ASSERT( window_data != NULL );
     D_ASSERT( window_data->stack_data != NULL );

     data  = window_data->stack_data;
     stack = data->stack;

     switch_focus( stack, data, window );

     entered = data->entered_window;

     if (entered && entered != window) {
          DFBWindowEvent  we;
          CoreWindow     *entered = data->entered_window;

          we.type = DWET_LEAVE;
          we.x    = stack->cursor.x - entered->x;
          we.y    = stack->cursor.y - entered->y;

          post_event( entered, data, &we );

          data->entered_window = NULL;
     }

     return DFB_OK;
}

/**************************************************************************************************/
/**************************************************************************************************/

static bool
handle_wm_key( CoreWindowStack     *stack,
               StackData           *data,
               const DFBInputEvent *event )
{
     int         i, num;
     CoreWindow *entered;
     CoreWindow *focused;
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( data->wm_level > 0 );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS );

     entered = data->entered_window;
     focused = data->focused_window;

     switch (DFB_LOWER_CASE(event->key_symbol)) {
          case DIKS_SMALL_X:
               num = fusion_vector_size( &data->windows );

               if (data->wm_cycle <= 0)
                    data->wm_cycle = num;

               if (num) {
                    int looped = 0;
                    int index  = MIN( num, data->wm_cycle );

                    while (index--) {
                         CoreWindow *window = fusion_vector_at( &data->windows, index );

                         if ((window->options & (DWOP_GHOST | DWOP_KEEP_STACKING)) ||
                             ! VISIBLE_WINDOW(window) || window == data->focused_window)
                         {
                              if (index == 0 && !looped) {
                                   looped = 1;
                                   index  = num - 1;
                              }

                              continue;
                         }

                         restack_window( window, window->window_data,
                                         NULL, NULL, 1, window->stacking );
                         request_focus( window, window->window_data );

                         break;
                    }

                    data->wm_cycle = index;
               }
               break;

          case DIKS_SMALL_S:
               fusion_vector_foreach (window, i, data->windows) {
                    if (VISIBLE_WINDOW(window) && window->stacking == DWSC_MIDDLE &&
                       ! (window->options & (DWOP_GHOST | DWOP_KEEP_STACKING)))
                    {
                         restack_window( window, window->window_data,
                                         NULL, NULL, 1, window->stacking );
                         request_focus( window, window->window_data );

                         break;
                    }
               }
               break;

          case DIKS_SMALL_C:
               if (entered) {
                    DFBWindowEvent event;

                    event.type = DWET_CLOSE;

                    post_event( entered, data, &event );
               }
               break;

          case DIKS_SMALL_E:
               update_focus( stack, data );
               break;

          case DIKS_SMALL_A:
               if (focused && !(focused->options & DWOP_KEEP_STACKING)) {
                    restack_window( focused, focused->window_data,
                                    NULL, NULL, 0, focused->stacking );
                    update_focus( stack, data );
               }
               break;

          case DIKS_SMALL_W:
               if (focused && !(focused->options & DWOP_KEEP_STACKING))
                    restack_window( focused, focused->window_data,
                                    NULL, NULL, 1, focused->stacking );
               break;

          case DIKS_SMALL_D:
               if (entered && !(entered->options & DWOP_INDESTRUCTIBLE))
                    dfb_window_destroy( entered );

               break;

          case DIKS_SMALL_P:
               /* Enable and show cursor. */
               dfb_windowstack_cursor_set_opacity( stack, 0xff );
               dfb_windowstack_cursor_enable( stack, true );

               /* Ungrab pointer. */
               data->pointer_window = NULL;

               /* TODO: set new cursor shape, the one current might be completely transparent */
               break;

          case DIKS_PRINT:
               if (dfb_config->screenshot_dir && focused && focused->surface)
                    dfb_surface_dump( focused->surface, dfb_config->screenshot_dir, "dfb_window" );

               break;

          default:
               return false;
     }

     return true;
}

static bool
is_wm_key( DFBInputDeviceKeySymbol key_symbol )
{
     switch (DFB_LOWER_CASE(key_symbol)) {
          case DIKS_SMALL_X:
          case DIKS_SMALL_S:
          case DIKS_SMALL_C:
          case DIKS_SMALL_E:
          case DIKS_SMALL_A:
          case DIKS_SMALL_W:
          case DIKS_SMALL_D:
          case DIKS_SMALL_P:
          case DIKS_PRINT:
               break;

          default:
               return false;
     }

     return true;
}


/**************************************************************************************************/

static DFBResult
handle_key_press( CoreWindowStack     *stack,
                  StackData           *data,
                  const DFBInputEvent *event )
{
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS );

     if (data->wm_level) {
          switch (event->key_symbol) {
               case DIKS_META:
                    data->wm_level |= 1;
                    break;

               case DIKS_CONTROL:
                    data->wm_level |= 2;
                    break;

               case DIKS_ALT:
                    data->wm_level |= 4;
                    break;

               default:
                    if (handle_wm_key( stack, data, event ))
                         return DFB_OK;

                    break;
          }
     }
     else if (event->key_symbol == DIKS_META) {
          data->wm_level |= 1;
          data->wm_cycle  = 0;
     }

     window = get_keyboard_window( stack, data, event );
     if (window)
          send_key_event( window, data, event );

     return DFB_OK;
}

static DFBResult
handle_key_release( CoreWindowStack     *stack,
                    StackData           *data,
                    const DFBInputEvent *event )
{
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYRELEASE );

     if (data->wm_level) {
          switch (event->key_symbol) {
               case DIKS_META:
                    data->wm_level &= ~1;
                    break;

               case DIKS_CONTROL:
                    data->wm_level &= ~2;
                    break;

               case DIKS_ALT:
                    data->wm_level &= ~4;
                    break;

               default:
                    if (is_wm_key( event->key_symbol ))
                         return DFB_OK;

                    break;
          }
     }

     window = get_keyboard_window( stack, data, event );
     if (window)
          send_key_event( window, data, event );

     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
handle_button_press( CoreWindowStack     *stack,
                     StackData           *data,
                     const DFBInputEvent *event )
{
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_BUTTONPRESS );

     if (!stack->cursor.enabled)
          return DFB_OK;

     switch (data->wm_level) {
          case 1:
               window = data->entered_window;
               if (window && !(window->options & DWOP_KEEP_STACKING))
                    dfb_window_raisetotop( data->entered_window );

               break;

          default:
               window = data->pointer_window ? data->pointer_window : data->entered_window;
               if (window)
                    send_button_event( window, data, event );

               break;
     }

     return DFB_OK;
}

static DFBResult
handle_button_release( CoreWindowStack     *stack,
                       StackData           *data,
                       const DFBInputEvent *event )
{
     CoreWindow *window;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_BUTTONRELEASE );

     if (!stack->cursor.enabled)
          return DFB_OK;

     switch (data->wm_level) {
          case 1:
               break;

          default:
               window = data->pointer_window ? data->pointer_window : data->entered_window;
               if (window)
                    send_button_event( window, data, event );

               break;
     }

     return DFB_OK;
}

/**************************************************************************************************/

static void
handle_motion( CoreWindowStack *stack,
               StackData       *data,
               int              dx,
               int              dy )
{
     int            new_cx, new_cy;
     DFBWindowEvent we;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );

     if (!stack->cursor.enabled)
          return;

     new_cx = MIN( stack->cursor.x + dx, stack->cursor.region.x2);
     new_cy = MIN( stack->cursor.y + dy, stack->cursor.region.y2);

     new_cx = MAX( new_cx, stack->cursor.region.x1 );
     new_cy = MAX( new_cy, stack->cursor.region.y1 );

     if (new_cx == stack->cursor.x  &&  new_cy == stack->cursor.y)
          return;

     dx = new_cx - stack->cursor.x;
     dy = new_cy - stack->cursor.y;

     stack->cursor.x = new_cx;
     stack->cursor.y = new_cy;

     D_ASSERT( stack->cursor.window != NULL );

     dfb_window_move( stack->cursor.window, dx, dy );

     switch (data->wm_level) {
          case 7:
          case 6:
          case 5:
          case 4: {
                    CoreWindow *window = data->entered_window;

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

          case 3:
          case 2: {
                    CoreWindow *window = data->entered_window;

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
                    CoreWindow *window = data->entered_window;

                    if (window && !(window->options & DWOP_KEEP_POSITION))
                         dfb_window_move( window, dx, dy );

                    break;
               }

          case 0:
               if (data->pointer_window) {
                    CoreWindow *window = data->pointer_window;

                    we.type = DWET_MOTION;
                    we.x    = stack->cursor.x - window->x;
                    we.y    = stack->cursor.y - window->y;

                    post_event( window, data, &we );
               }
               else {
                    if (!update_focus( stack, data ) && data->entered_window) {
                         CoreWindow *window = data->entered_window;

                         we.type = DWET_MOTION;
                         we.x    = stack->cursor.x - window->x;
                         we.y    = stack->cursor.y - window->y;

                         post_event( window, data, &we );
                    }
               }

               break;

          default:
               ;
     }
}

static void
handle_wheel( CoreWindowStack *stack,
              StackData       *data,
              int              dz )
{
     DFBWindowEvent we;
     CoreWindow *window = NULL;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );

     if (!stack->cursor.enabled)
          return;

     window = data->pointer_window ? data->pointer_window : data->entered_window;

     if (window) {
          if (data->wm_level) {
               int opacity = window->opacity + dz*7;

               if (opacity < 0x01)
                    opacity = 1;
               if (opacity > 0xFF)
                    opacity = 0xFF;

               dfb_window_set_opacity( window, opacity );
          }
          else {
               we.type = DWET_WHEEL;
               we.x    = stack->cursor.x - window->x;
               we.y    = stack->cursor.y - window->y;
               we.step = dz;

               post_event( window, data, &we );
          }
     }
}

static DFBResult
handle_axis_motion( CoreWindowStack     *stack,
                    StackData           *data,
                    const DFBInputEvent *event )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_AXISMOTION );

     if (event->flags & DIEF_AXISREL) {
          int rel = event->axisrel;

          /* handle cursor acceleration */
          if (rel > stack->cursor.threshold)
               rel += (rel - stack->cursor.threshold)
                      * stack->cursor.numerator
                      / stack->cursor.denominator;
          else if (rel < -stack->cursor.threshold)
               rel += (rel + stack->cursor.threshold)
                      * stack->cursor.numerator
                      / stack->cursor.denominator;

          switch (event->axis) {
               case DIAI_X:
                    handle_motion( stack, data, rel, 0 );
                    break;
               case DIAI_Y:
                    handle_motion( stack, data, 0, rel );
                    break;
               case DIAI_Z:
                    handle_wheel( stack, data, - event->axisrel );
                    break;
               default:
                    ;
          }
     }
     else if (event->flags & DIEF_AXISABS) {
          switch (event->axis) {
               case DIAI_X:
                    handle_motion( stack, data, event->axisabs - stack->cursor.x, 0 );
                    break;
               case DIAI_Y:
                    handle_motion( stack, data, 0, event->axisabs - stack->cursor.y );
                    break;
               default:
                    ;
          }
     }

     return DFB_OK;
}

/**************************************************************************************************/
/**************************************************************************************************/

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
     StackData       *data  = region_data;
     CardState       *state = update_data;

     D_ASSERT( region != NULL );
     D_ASSERT( region_data != NULL );
     D_ASSERT( update_data != NULL );
     D_ASSERT( updates != NULL );

     D_ASSERT( x == 0 );
     D_ASSERT( y == 0 );

     D_MAGIC_ASSERT( data, StackData );
     D_MAGIC_ASSERT( state, CardState );

     stack = data->stack;

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->bg.image != NULL || (stack->bg.mode != DLBM_IMAGE &&
                                           stack->bg.mode != DLBM_TILE) );

     switch (stack->bg.mode) {
          case DLBM_COLOR: {
               CoreSurface *dest  = state->destination;
               DFBColor    *color = &stack->bg.color;

               /* Set the background color. */
               if (DFB_PIXELFORMAT_IS_INDEXED( dest->format ))
                    dfb_state_set_color_index( state,
                                               dfb_palette_search( dest->palette, color->r,
                                                                   color->g, color->b, color->a ) );
               else
                    dfb_state_set_color( state, color );

               for (i=0; i<num; i++) {
                    DFBRectangle dst = DFB_RECTANGLE_INIT_FROM_REGION( &updates[i] );

                    /* Simply fill the background. */
                    dfb_gfxcard_fillrectangle( &dst, state );
               }

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
static const StretRegionClass root_class = {
     Update:   root_update
};

/**************************************************************************************************/

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
     DFBSurfaceBlittingFlags  flags = DSBLIT_NOFX;
     WindowData              *data  = region_data;
     CardState               *state = update_data;
     bool                     alpha = arg;
     CoreWindow              *window;

     D_ASSERT( region != NULL );
     D_ASSERT( region_data != NULL );
     D_ASSERT( update_data != NULL );
     D_ASSERT( updates != NULL );

     D_MAGIC_ASSERT( data, WindowData );
     D_MAGIC_ASSERT( state, CardState );

     window = data->window;

     if (!window->opacity)
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

static const StretRegionClass window_class = {
     Update:   window_update
};

/**************************************************************************************************/
/**************************************************************************************************/

static void
wm_get_info( CoreWMInfo *info )
{
     info->version.major = 0;
     info->version.minor = 2;

     snprintf( info->name, DFB_CORE_WM_INFO_NAME_LENGTH, "Default" );
     snprintf( info->vendor, DFB_CORE_WM_INFO_VENDOR_LENGTH, "Convergence GmbH" );

     info->wm_data_size     = sizeof(WMData);
     info->wm_shared_size   = sizeof(WMShared);
     info->stack_data_size  = sizeof(StackData);
     info->window_data_size = sizeof(WindowData);
}

static DFBResult
wm_initialize( CoreDFB *core, void *wm_data, void *shared_data )
{
     DFBResult  ret;
     WMData    *data   = wm_data;
     WMShared  *shared = shared_data;

     data->core   = core;
     data->shared = shared;

     ret = stret_class_register( &root_class, &shared->root_class );
     if (ret)
          return ret;

     ret = stret_class_register( &window_class, &shared->window_class );
     if (ret)
          return ret;

     return DFB_OK;
}

static DFBResult
wm_join( CoreDFB *core, void *wm_data, void *shared_data )
{
     DFBResult           ret;
     WMData             *data   = wm_data;
     WMShared           *shared = shared_data;
     StretRegionClassID  id;

     data->core   = core;
     data->shared = shared;


     ret = stret_class_register( &root_class, &id );
     if (ret)
          return ret;

     if (id != shared->root_class) {
          D_BUG( "root class id should be %d, but is %d", shared->root_class, id );
          stret_class_unregister( id );
          return DFB_BUG;
     }


     ret = stret_class_register( &window_class, &id );
     if (ret) {
          stret_class_unregister( shared->root_class );
          return ret;
     }

     if (id != shared->window_class) {
          D_BUG( "window class id should be %d, but is %d", shared->window_class, id );
          stret_class_unregister( id );
          stret_class_unregister( shared->root_class );
          return DFB_BUG;
     }

     return DFB_OK;
}

static DFBResult
wm_shutdown( bool emergency, void *wm_data, void *shared_data )
{
     WMShared *shared = shared_data;

     stret_class_unregister( shared->window_class );
     stret_class_unregister( shared->root_class );

     return DFB_OK;
}

static DFBResult
wm_leave( bool emergency, void *wm_data, void *shared_data )
{
     WMShared *shared = shared_data;

     stret_class_unregister( shared->window_class );
     stret_class_unregister( shared->root_class );

     return DFB_OK;
}

static DFBResult
wm_suspend( void *wm_data, void *shared_data )
{
     return DFB_OK;
}

static DFBResult
wm_resume( void *wm_data, void *shared_data )
{
     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
wm_init_stack( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data )
{
     int        i;
     DFBResult  ret;
     StackData *data   = stack_data;
     WMData    *wmdata = wm_data;
     WMShared  *shared;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     shared = wmdata->shared;

     data->stack = stack;

     fusion_vector_init( &data->windows, 64 );

     for (i=0; i<8; i++)
          data->keys[i].code = -1;

     ret = stret_region_create( shared->root_class, data, 0,
                                SRF_OUTPUT,
                                0, 0, INT_MAX, INT_MAX,
                                NULL, &data->root );
     if (ret) {
          fusion_vector_destroy( &data->windows );
          return ret;
     }

     D_MAGIC_SET( data, StackData );

     return DFB_OK;
}

static DFBResult
wm_close_stack( CoreWindowStack *stack,
                void            *wm_data,
                void            *stack_data )
{
     DirectLink *l, *next;
     StackData  *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     D_MAGIC_ASSERT( data, StackData );
     D_MAGIC_CLEAR( data );

     D_ASSUME( fusion_vector_is_empty( &data->windows ) );

     if (fusion_vector_has_elements( &data->windows )) {
          int         i;
          CoreWindow *window;

          fusion_vector_foreach (window, i, data->windows) {
               D_WARN( "setting window->stack = NULL" );
               window->stack = NULL;
          }
     }

     fusion_vector_destroy( &data->windows );

     stret_region_destroy( data->root );

     /* Free grabbed keys. */
     direct_list_foreach_safe (l, next, data->grabbed_keys)
          SHFREE( l );

     return DFB_OK;
}

static DFBResult
wm_process_input( CoreWindowStack     *stack,
                  void                *wm_data,
                  void                *stack_data,
                  const DFBInputEvent *event )
{
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( event != NULL );

     D_MAGIC_ASSERT( data, StackData );

     D_HEAVYDEBUG( "WM/Default: Processing input event (device %d, type 0x%08x, flags 0x%08x)...\n",
                   event->device_id, event->type, event->flags );

     /* FIXME: handle multiple devices */
     if (event->flags & DIEF_BUTTONS)
          data->buttons = event->buttons;

     if (event->flags & DIEF_MODIFIERS)
          data->modifiers = event->modifiers;

     if (event->flags & DIEF_LOCKS)
          data->locks = event->locks;

     switch (event->type) {
          case DIET_KEYPRESS:
               return handle_key_press( stack, data, event );

          case DIET_KEYRELEASE:
               return handle_key_release( stack, data, event );

          case DIET_BUTTONPRESS:
               return handle_button_press( stack, data, event );

          case DIET_BUTTONRELEASE:
               return handle_button_release( stack, data, event );

          case DIET_AXISMOTION:
               return handle_axis_motion( stack, data, event );

          default:
               D_ONCE( "unknown input event type" );
               break;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
wm_flush_keys( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data )
{
     int        i;
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     D_MAGIC_ASSERT( data, StackData );

     for (i=0; i<8; i++) {
          if (data->keys[i].code != -1) {
               DFBWindowEvent we;

               we.type       = DWET_KEYUP;
               we.key_code   = data->keys[i].code;
               we.key_id     = data->keys[i].id;
               we.key_symbol = data->keys[i].symbol;

               post_event( data->keys[i].owner, data, &we );

               data->keys[i].code = -1;
          }
     }

     return DFB_OK;
}

static DFBResult
wm_window_at( CoreWindowStack  *stack,
              void             *wm_data,
              void             *stack_data,
              int               x,
              int               y,
              CoreWindow      **ret_window )
{
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( ret_window != NULL );

     *ret_window = window_at_pointer( stack, data, x, y );

     return DFB_OK;
}

static DFBResult
wm_window_lookup( CoreWindowStack  *stack,
                  void             *wm_data,
                  void             *stack_data,
                  DFBWindowID       window_id,
                  CoreWindow      **ret_window )
{
     int         i;
     CoreWindow *window = NULL;
     StackData  *data   = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( ret_window != NULL );

     D_MAGIC_ASSERT( data, StackData );

     fusion_vector_foreach_reverse (window, i, data->windows) {
          if (window->id == window_id) {
               /* don't hand out the cursor window */
               if (window->caps & DWHC_TOPMOST)
                    window = NULL;

               break;
          }
     }

     *ret_window = window;

     return DFB_OK;
}

static DFBResult
wm_enum_windows( CoreWindowStack      *stack,
                 void                 *wm_data,
                 void                 *stack_data,
                 CoreWMWindowCallback  callback,
                 void                 *callback_ctx )
{
     int         i;
     CoreWindow *window = NULL;
     StackData  *data   = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( callback != NULL );

     D_MAGIC_ASSERT( data, StackData );

     fusion_vector_foreach_reverse (window, i, data->windows) {
          if (callback( window, callback_ctx ) != DFENUM_OK)
               break;
     }

     return DFB_OK;
}

static DFBResult
wm_warp_cursor( CoreWindowStack *stack,
                void            *wm_data,
                void            *stack_data,
                int              x,
                int              y )
{
     int        dx;
     int        dy;
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     D_MAGIC_ASSERT( data, StackData );

     dx = x - stack->cursor.x;
     dy = y - stack->cursor.y;

     handle_motion( stack, data, dx, dy );

     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
wm_add_window( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data,
               CoreWindow      *window,
               void            *window_data )
{
     DFBResult         ret;
     StretRegionFlags  flags  = SRF_NONE;
     WindowData       *data   = window_data;
     StackData        *sdata  = stack_data;
     WMData           *wmdata = wm_data;
     WMShared         *shared;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( sdata, StackData );

     shared = wmdata->shared;

     /* Initialize window data. */
     data->window     = window;
     data->stack_data = stack_data;
     data->priority   = get_priority( window );

     if (! (window->options & DWOP_GHOST))
          flags |= SRF_INPUT;

     if (! (window->caps & DWCAPS_INPUTONLY))
          flags |= SRF_OUTPUT;

     if (window->options & DWOP_SHAPED)
          flags |= SRF_SHAPED;

     ret = stret_region_create( shared->window_class, data, true, flags,
                                window->x, window->y, window->width, window->height,
                                sdata->root, &data->region );
     if (ret)
          return ret;

     if (window->region)
          dfb_layer_region_get_configuration( window->region, &data->config );

     D_MAGIC_SET( data, WindowData );

     /* Actually add the window to the stack. */
     insert_window( stack, sdata, window, data );

     /* Possibly switch focus to the new window. */
     update_focus( stack, sdata );

     return DFB_OK;
}

static DFBResult
wm_remove_window( CoreWindowStack *stack,
                  void            *wm_data,
                  void            *stack_data,
                  CoreWindow      *window,
                  void            *window_data )
{
     WindowData *data  = window_data;
     StackData  *sdata = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( data, WindowData );
     D_MAGIC_ASSERT( sdata, StackData );

     remove_window( stack, sdata, window, data );

     stret_region_destroy( data->region );

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
wm_move_window( CoreWindow *window,
                void       *wm_data,
                void       *window_data,
                int         dx,
                int         dy )
{
     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     return move_window( window, window_data, dx, dy );
}

static DFBResult
wm_resize_window( CoreWindow *window,
                  void       *wm_data,
                  void       *window_data,
                  int         width,
                  int         height )
{
     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     return resize_window( window, wm_data, window_data, width, height );
}

static DFBResult
wm_restack_window( CoreWindow             *window,
                   void                   *wm_data,
                   void                   *window_data,
                   CoreWindow             *relative,
                   void                   *relative_data,
                   int                     relation,
                   DFBWindowStackingClass  stacking )
{
     DFBResult   ret;
     WindowData *data = window_data;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( data, WindowData );

     D_ASSERT( relative == NULL || relative_data != NULL );

     D_ASSERT( relative == NULL || relative == window || relation != 0);

     D_ASSERT( data->stack_data != NULL );
     D_ASSERT( data->stack_data->stack != NULL );

     ret = restack_window( window, window_data, relative, relative_data, relation, stacking );
     if (ret)
          return ret;

     /* Possibly switch focus to window now under the cursor */
     update_focus( data->stack_data->stack, data->stack_data );

     return DFB_OK;
}

static DFBResult
wm_set_opacity( CoreWindow *window,
                void       *wm_data,
                void       *window_data,
                __u8        opacity )
{
     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     set_opacity( window, window_data, opacity );

     return DFB_OK;
}

static DFBResult
wm_grab( CoreWindow *window,
         void       *wm_data,
         void       *window_data,
         CoreWMGrab *grab )
{
     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( grab != NULL );

     switch (grab->target) {
          case CWMGT_KEYBOARD:
               return grab_keyboard( window, window_data );

          case CWMGT_POINTER:
               return grab_pointer( window, window_data );

          case CWMGT_KEY:
               return grab_key( window, window_data, grab->symbol, grab->modifiers );

          default:
               D_BUG( "unknown grab target" );
               break;
     }

     return DFB_BUG;
}

static DFBResult
wm_ungrab( CoreWindow *window,
           void       *wm_data,
           void       *window_data,
           CoreWMGrab *grab )
{
     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( grab != NULL );

     switch (grab->target) {
          case CWMGT_KEYBOARD:
               return ungrab_keyboard( window, window_data );

          case CWMGT_POINTER:
               return ungrab_pointer( window, window_data );

          case CWMGT_KEY:
               return ungrab_key( window, window_data, grab->symbol, grab->modifiers );

          default:
               D_BUG( "unknown grab target" );
               break;
     }

     return DFB_BUG;
}

static DFBResult
wm_request_focus( CoreWindow *window,
                  void       *wm_data,
                  void       *window_data )
{
     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     return request_focus( window, window_data );
}

/**************************************************************************************************/

static DFBResult
wm_update_stack( CoreWindowStack     *stack,
                 void                *wm_data,
                 void                *stack_data,
                 DFBRegion           *region,
                 DFBSurfaceFlipFlags  flags )
{
     StackData *data = stack_data;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( region != NULL );

     D_MAGIC_ASSERT( data, StackData );

     return update_stack( stack, data, region, flags );
}

static DFBResult
wm_update_window( CoreWindow          *window,
                  void                *wm_data,
                  void                *window_data,
                  DFBRegion           *region,
                  DFBSurfaceFlipFlags  flags,
                  bool                 force_complete,
                  bool                 force_invisible )
{
     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( region != NULL );

     return update_window( window, window_data, region, flags, force_complete, force_invisible );
}

