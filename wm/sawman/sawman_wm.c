/*
   (c) Copyright 2001-2007  directfb.org
   (c) Copyright 2000-2004  convergence (integrated) media GmbH.

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

//#define DIRECT_ENABLE_DEBUG

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
#include <direct/trace.h>
#include <direct/util.h>

#include <fusion/shmalloc.h>
#include <fusion/vector.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layers_internal.h>
#include <core/screen.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/palette.h>
#include <core/windows.h>
#include <core/windows_internal.h>
#include <core/windowstack.h>
#include <core/wm.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <core/wm_module.h>

#include <sawman_config.h>
#include <sawman_manager.h>

#include "sawman_draw.h"


DFB_WINDOW_MANAGER( sawman )


D_DEBUG_DOMAIN( SaWMan_WM, "SaWMan/WM", "SaWMan window manager module" );

/**********************************************************************************************************************/

typedef struct {
     CoreDFB                      *core;
     FusionWorld                  *world;
     SaWMan                       *sawman;
     SaWManProcess                *process;
} WMData;

typedef struct {
     int                           magic;

     bool                          active;

     SaWMan                       *sawman;

     CoreWindowStack              *stack;
} StackData;

/**********************************************************************************************************************/

static int
keys_compare( const void *key1,
              const void *key2 )
{
     return *(DFBInputDeviceKeySymbol*) key1 - *(DFBInputDeviceKeySymbol*) key2;
}

static void
send_key_event( SaWMan              *sawman,
                SaWManWindow        *sawwin,
                const DFBInputEvent *event )
{
     DFBWindowEvent we;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( event != NULL );

     we.type       = (event->type == DIET_KEYPRESS) ? DWET_KEYDOWN : DWET_KEYUP;
     we.key_code   = event->key_code;
     we.key_id     = event->key_id;
     we.key_symbol = event->key_symbol;

     sawman_post_event( sawman, sawwin, &we );
}

static void
send_button_event( SaWMan              *sawman,
                   SaWManWindow        *sawwin,
                   CoreWindowStack     *stack,
                   const DFBInputEvent *event )
{
     DFBWindowEvent  we;
     CoreWindow     *window;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( stack != NULL );
     D_ASSERT( event != NULL );

     window = sawwin->window;

     D_ASSERT( window != NULL );
     D_ASSERT( window->surface != NULL );

     we.type   = (event->type == DIET_BUTTONPRESS) ? DWET_BUTTONDOWN : DWET_BUTTONUP;
     we.x      = stack->cursor.x - window->config.bounds.x;
     we.y      = stack->cursor.y - window->config.bounds.y;
     we.button = event->button;

     if (window->config.options & DWOP_SCALE) {
          we.x = we.x * window->surface->config.size.w  / window->config.bounds.w;
          we.y = we.y * window->surface->config.size.h / window->config.bounds.h;
     }

     sawman_post_event( sawman, sawwin, &we );
}

/**********************************************************************************************************************/

static SaWManWindow *
get_keyboard_window( StackData           *data,
                     const DFBInputEvent *event )
{
     SaWMan           *sawman;
     SaWManGrabbedKey *key;

     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS || event->type == DIET_KEYRELEASE );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Check explicit key grabs first. */
     direct_list_foreach (key, sawman->grabbed_keys) {
          if (key->symbol    == event->key_symbol &&
              key->modifiers == sawman->modifiers)
               return key->owner;
     }

     /* Don't do implicit grabs on keys without a hardware index. */
     if (event->key_code == -1)
          return (sawman->keyboard_window ?
                  sawman->keyboard_window : sawman->focused_window);

     /* Implicitly grab (press) or ungrab (release) key. */
     if (event->type == DIET_KEYPRESS) {
          int           i;
          int           free_key = -1;
          SaWManWindow *sawwin;
          CoreWindow   *window;

          /* Check active grabs. */
          for (i=0; i<SAWMAN_MAX_IMPLICIT_KEYGRABS; i++) {
               /* Key is grabbed, send to owner (NULL if destroyed). */
               if (sawman->keys[i].code == event->key_code)
                    return sawman->keys[i].owner;

               /* Remember first free array item. */
               if (free_key == -1 && sawman->keys[i].code == -1)
                    free_key = i;
          }

          /* Key is not grabbed, check for explicit keyboard grab or focus. */
          sawwin = sawman->keyboard_window ?
                   sawman->keyboard_window : sawman->focused_window;
          if (!sawwin)
               return NULL;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_ASSERT( window != NULL );

          /* Check key selection. */
          switch (window->config.key_selection) {
               case DWKS_ALL:
                    break;

               case DWKS_LIST:
                    D_ASSERT( window->config.keys != NULL );
                    D_ASSERT( window->config.num_keys > 0 );

                    if (bsearch( &event->key_symbol,
                                 window->config.keys, window->config.num_keys,
                                 sizeof(DFBInputDeviceKeySymbol), keys_compare ))
                         break;

                    /* fall through */

               case DWKS_NONE:
                    return sawman->unselkeys_window;
          }

          /* Check if a free array item was found. */
          if (free_key == -1) {
               D_WARN( "maximum number of owned keys reached" );
               return NULL;
          }

          /* Implicitly grab the key. */
          sawman->keys[free_key].symbol = event->key_symbol;
          sawman->keys[free_key].id     = event->key_id;
          sawman->keys[free_key].code   = event->key_code;
          sawman->keys[free_key].owner  = sawwin;

          return sawwin;
     }
     else {
          int i;

          /* Lookup owner and ungrab the key. */
          for (i=0; i<SAWMAN_MAX_IMPLICIT_KEYGRABS; i++) {
               if (sawman->keys[i].code == event->key_code) {
                    sawman->keys[i].code = -1;

                    /* Return owner (NULL if destroyed). */
                    return sawman->keys[i].owner;
               }
          }
     }

     /* No owner for release event found, discard it. */
     return NULL;
}

static SaWManWindow*
window_at_pointer( SaWMan          *sawman,
                   CoreWindowStack *stack,
                   int              x,
                   int              y )
{
     int           i;
     SaWManWindow *sawwin;
     CoreWindow   *window;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( stack != NULL );

     if (!stack->cursor.enabled) {
          fusion_vector_foreach_reverse (sawwin, i, sawman->layout) {
               D_MAGIC_ASSERT( sawwin, SaWManWindow );
               window = sawwin->window;
               D_ASSERT( window != NULL );

               if (window->config.opacity && !(window->config.options & DWOP_GHOST))
                    return sawwin;
          }

          return NULL;
     }

     if (x < 0)
          x = stack->cursor.x;
     if (y < 0)
          y = stack->cursor.y;

     fusion_vector_foreach_reverse (sawwin, i, sawman->layout) {
          D_MAGIC_ASSERT( sawwin, SaWManWindow );
          window = sawwin->window;
          D_ASSERT( window != NULL );

          if (!(window->config.options & DWOP_GHOST) && window->config.opacity &&
              x >= window->config.bounds.x  &&  x < window->config.bounds.x + window->config.bounds.w &&
              y >= window->config.bounds.y  &&  y < window->config.bounds.y + window->config.bounds.h)
               return sawwin;
     }

     return NULL;
}

static bool
update_focus( SaWMan          *sawman,
              CoreWindowStack *stack )
{
     StackData *data;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( stack != NULL );

     data = stack->stack_data;

     D_MAGIC_ASSERT( data, StackData );

     /* if pointer is not grabbed */
     if (!sawman->pointer_window) {
          SaWManWindow *before = sawman->entered_window;
          SaWManWindow *after  = window_at_pointer( sawman, stack, -1, -1 );

          /* and the window under the cursor is another one now */
          if (before != after) {
               DFBWindowEvent we;

               /* send leave event */
               if (before) {
                    D_MAGIC_ASSERT( before, SaWManWindow );
                    D_ASSERT( before->window != NULL );

                    we.type = DWET_LEAVE;
                    we.x    = stack->cursor.x - before->window->config.bounds.x;
                    we.y    = stack->cursor.y - before->window->config.bounds.y;

                    sawman_post_event( sawman, before, &we );
               }

               /* switch focus and send enter event */
               sawman_switch_focus( sawman, after );

               if (after) {
                    D_MAGIC_ASSERT( after, SaWManWindow );
                    D_ASSERT( after->window != NULL );

                    we.type = DWET_ENTER;
                    we.x    = stack->cursor.x - after->window->config.bounds.x;
                    we.y    = stack->cursor.y - after->window->config.bounds.y;

                    sawman_post_event( sawman, after, &we );
               }

               /* update pointer to window under the cursor */
               sawman->entered_window = after;

               return true;
          }
     }

     return false;
}

/**********************************************************************************************************************/

static DFBResult
move_window( SaWMan       *sawman,
             SaWManWindow *sawwin,
             int           dx,
             int           dy )
{
     DFBWindowEvent  evt;
     DFBRectangle   *bounds;
     CoreWindow     *window;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     window = sawwin->window;
     D_ASSERT( window != NULL );

     bounds = &window->config.bounds;
     bounds->x += dx;
     bounds->y += dy;

     if (SAWMAN_VISIBLE_WINDOW(window)) {
          DFBRegion region = { 0, 0, bounds->w - 1, bounds->h - 1 };

          sawman_update_window( sawman, sawwin, &region, 0, false, false, false );

          dfb_region_translate( &region, -dx, -dy );

          sawman_update_window( sawman, sawwin, &region, 0, false, false, false );
     }

     /* Send new position */
     evt.type = DWET_POSITION;
     evt.x    = bounds->x;
     evt.y    = bounds->y;

     sawman_post_event( sawman, sawwin, &evt );

     return DFB_OK;
}

static DFBResult
resize_window( SaWMan       *sawman,
               SaWManWindow *sawwin,
               WMData       *wm_data,
               int           width,
               int           height )
{
     DFBResult       ret;
     DFBWindowEvent  evt;
     DFBRectangle   *bounds;
     CoreWindow     *window;
     int             ow;
     int             oh;

     D_DEBUG_AT( SaWMan_WM, "resize_window( %d, %d )\n", width, height );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     if (width > 4096 || height > 4096)
          return DFB_LIMITEXCEEDED;

     window = sawwin->window;
     D_ASSERT( window != NULL );
     D_ASSERT( window->surface != NULL );

     /* Resize the surface? */
     if (window->surface && !(window->config.options & DWOP_SCALE)) {
          ret = dfb_surface_reformat( window->surface,
                                      width, height, window->surface->config.format );
          if (ret)
               return ret;
     }

     bounds = &window->config.bounds;
     ow     = bounds->w;
     oh     = bounds->h;

     bounds->w = width;
     bounds->h = height;

     dfb_region_intersect( &window->config.opaque, 0, 0, width - 1, height - 1 );

     if (SAWMAN_VISIBLE_WINDOW (window)) {
          if (ow > bounds->w) {
               DFBRegion region = { bounds->w, 0, ow - 1, MIN(bounds->h, oh) - 1 };

               sawman_update_window( sawman, sawwin, &region, 0, false, false, false );
          }

          if (oh > bounds->h) {
               DFBRegion region = { 0, bounds->h, MAX(bounds->w, ow) - 1, oh - 1 };

               sawman_update_window( sawman, sawwin, &region, 0, false, false, false );
          }
     }

     /* Send new size */
     if (!(window->config.options & DWOP_SCALE)) {
          evt.type = DWET_SIZE;
          evt.w    = bounds->w;
          evt.h    = bounds->h;

          sawman_post_event( sawman, sawwin, &evt );
     }

     return DFB_OK;
}

static DFBResult
set_window_bounds( SaWMan       *sawman,
                   SaWManWindow *sawwin,
                   WMData       *wm_data,
                   int           x,
                   int           y,
                   int           width,
                   int           height )
{
     DFBResult       ret;
     DFBWindowEvent  evt;
     DFBRegion       old_region;
     DFBRegion       new_region;
     DFBRectangle   *bounds;
     CoreWindow     *window;

     D_DEBUG_AT( SaWMan_WM, "%s( %p [%d] %d, %d - %dx%d )\n", __FUNCTION__, sawwin, sawwin->id, x, y, width, height );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     if (width > 4096 || height > 4096)
          return DFB_LIMITEXCEEDED;

     window = sawwin->window;
     D_ASSERT( window != NULL );
     D_ASSERT( window->surface != NULL );

     /* Resize the surface? */
     if (window->surface && !(window->config.options & DWOP_SCALE)) {
          ret = dfb_surface_reformat( window->surface,
                                      width, height, window->surface->config.format );
          if (ret)
               return ret;
     }

     bounds = &window->config.bounds;

     old_region.x1 = bounds->x - x;
     old_region.y1 = bounds->y - y;
     old_region.x2 = old_region.x1 + bounds->w - 1;
     old_region.y2 = old_region.y1 + bounds->h - 1;

     bounds->x = x;
     bounds->y = y;
     bounds->w = width;
     bounds->h = height;

     new_region.x1 = 0;
     new_region.y1 = 0;
     new_region.x2 = width  - 1;
     new_region.y2 = height - 1;

     if (!dfb_region_region_intersect( &window->config.opaque, &new_region ))
          window->config.opaque = new_region;

     /* Update exposed area. */
     if (SAWMAN_VISIBLE_WINDOW( window )) {
          if (dfb_region_region_intersect( &new_region, &old_region )) {
               /* left */
               if (new_region.x1 > old_region.x1) {
                    DFBRegion region = { old_region.x1, old_region.y1,
                                         new_region.x1 - 1, new_region.y2 };

                    sawman_update_window( sawman, sawwin, &region, 0, false, false, false );
               }

               /* upper */
               if (new_region.y1 > old_region.y1) {
                    DFBRegion region = { old_region.x1, old_region.y1,
                                         old_region.x2, new_region.y1 - 1 };

                    sawman_update_window( sawman, sawwin, &region, 0, false, false, false );
               }

               /* right */
               if (new_region.x2 < old_region.x2) {
                    DFBRegion region = { new_region.x2 + 1, new_region.y1,
                                         old_region.x2, new_region.y2 };

                    sawman_update_window( sawman, sawwin, &region, 0, false, false, false );
               }

               /* lower */
               if (new_region.y2 < old_region.y2) {
                    DFBRegion region = { old_region.x1, new_region.y2 + 1,
                                         old_region.x2, old_region.y2 };

                    sawman_update_window( sawman, sawwin, &region, 0, false, false, false );
               }
          }
          else
               sawman_update_window( sawman, sawwin, &old_region, 0, false, false, false );
     }

     /* Send new position and size */
     evt.type = DWET_POSITION_SIZE;
     evt.x    = bounds->x;
     evt.y    = bounds->y;
     evt.w    = window->surface->config.size.w;
     evt.h    = window->surface->config.size.h;

     sawman_post_event( sawman, sawwin, &evt );

     return DFB_OK;
}

static DFBResult
restack_window( SaWMan                 *sawman,
                SaWManWindow           *sawwin,
                SaWManWindow           *relative,
                int                     relation,
                DFBWindowStackingClass  stacking )
{
     int         old;
     int         index;
     int         priority;
     StackData  *data;
     CoreWindow *window;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_MAGIC_ASSERT_IF( relative, SaWManWindow );
     D_ASSERT( sawwin->stack_data != NULL );

     D_ASSERT( relative == NULL || relative == sawwin || relation != 0);

     data = sawwin->stack_data;

     window = sawwin->window;

     D_ASSERT( window != NULL );

     /* Change stacking class. */
     if (stacking != window->config.stacking) {
          window->config.stacking = stacking;

          sawwin->priority = sawman_window_priority( sawwin );
     }

     /* Make sure window is inserted and not kept above/under parent. */
     if (!(sawwin->flags & SWMWF_INSERTED) && !(window->config.options & (DWOP_KEEP_ABOVE|DWOP_KEEP_UNDER)))
          return DFB_OK;

     /* Get the (new) priority. */
     priority = sawwin->priority;

     /* Get the old index. */
     old = sawman_window_index( sawman, sawwin );

     /* Calculate the desired index. */
     if (relative) {
          index = sawman_window_index( sawman, relative );

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
          else if (index > sawman->layout.count - 1)
               index = sawman->layout.count - 1;
     }
     else if (relation)
          index = sawman->layout.count - 1;
     else
          index = 0;

     /* Assure window won't be above any window with a higher priority. */
     while (index > 0) {
          int           below = (old < index) ? index : index - 1;
          SaWManWindow *other = fusion_vector_at( &sawman->layout, below );

          D_MAGIC_ASSERT( other, SaWManWindow );

          if (priority < other->priority)
               index--;
          else
               break;
     }

     /* Assure window won't be below any window with a lower priority. */
     while (index < sawman->layout.count - 1) {
          int           above = (old > index) ? index : index + 1;
          SaWManWindow *other = fusion_vector_at( &sawman->layout, above );

          D_MAGIC_ASSERT( other, SaWManWindow );

          if (priority > other->priority)
               index++;
          else
               break;
     }

     /* Return if index hasn't changed. */
     if (index == old)
          return DFB_OK;

     /* Actually change the stacking order now. */
     fusion_vector_move( &sawman->layout, old, index );

     sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, (index < old), false, false );

     return DFB_OK;
}

static void
set_opacity( SaWMan       *sawman,
             SaWManWindow *sawwin,
             u8            opacity )
{
     u8               old;
     StackData       *data;
     CoreWindowStack *stack;
     CoreWindow      *window;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( sawwin->stack_data != NULL );
     D_ASSERT( sawwin->stack != NULL );
     D_ASSERT( sawwin->window != NULL );

     data   = sawwin->stack_data;
     stack  = sawwin->stack;
     window = sawwin->window;
     old    = window->config.opacity;

     if (!dfb_config->translucent_windows && opacity)
          opacity = 0xFF;

     if (old != opacity) {
          window->config.opacity = opacity;

          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, false, true, false );

          /* Ungrab pointer/keyboard, pass focus... */
          if (old && !opacity) {
               /* Possibly switch focus to window now under the cursor */
               if (sawman->focused_window == sawwin)
                    update_focus( data->sawman, data->stack );

               sawman_withdraw_window( sawman, sawwin );
          }
     }
}

/**********************************************************************************************************************/

static DFBResult
grab_keyboard( SaWMan       *sawman,
               SaWManWindow *sawwin )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     if (sawman->keyboard_window)
          return DFB_LOCKED;

     sawman->keyboard_window = sawwin;

     return DFB_OK;
}

static DFBResult
ungrab_keyboard( SaWMan       *sawman,
                 SaWManWindow *sawwin )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     if (sawman->keyboard_window == sawwin)
          sawman->keyboard_window = NULL;

     return DFB_OK;
}

static DFBResult
grab_pointer( SaWMan       *sawman,
              SaWManWindow *sawwin )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     if (sawman->pointer_window)
          return DFB_LOCKED;

     sawman->pointer_window = sawwin;

     return DFB_OK;
}

static DFBResult
ungrab_pointer( SaWMan       *sawman,
                SaWManWindow *sawwin )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     if (sawman->pointer_window == sawwin)
          sawman->pointer_window = NULL;

     return DFB_OK;
}

static DFBResult
grab_key( SaWMan                     *sawman,
          SaWManWindow               *sawwin,
          DFBInputDeviceKeySymbol     symbol,
          DFBInputDeviceModifierMask  modifiers )
{
     int               i;
     StackData        *data;
     SaWManGrabbedKey *key;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     data = sawwin->stack_data;
     D_MAGIC_ASSERT( data, StackData );

     /* Reject if already grabbed. */
     direct_list_foreach (key, sawman->grabbed_keys)
          if (key->symbol == symbol && key->modifiers == modifiers)
               return DFB_LOCKED;

     /* Allocate grab information. */
     key = SHCALLOC( sawwin->shmpool, 1, sizeof(SaWManGrabbedKey) );

     /* Fill grab information. */
     key->symbol    = symbol;
     key->modifiers = modifiers;
     key->owner     = sawwin;

     /* Add to list of key grabs. */
     direct_list_append( &sawman->grabbed_keys, &key->link );

     /* Remove implicit grabs for this key. */
     for (i=0; i<SAWMAN_MAX_IMPLICIT_KEYGRABS; i++)
          if (sawman->keys[i].code != -1 && sawman->keys[i].symbol == symbol)
               sawman->keys[i].code = -1;

     return DFB_OK;
}

static DFBResult
ungrab_key( SaWMan                     *sawman,
            SaWManWindow               *sawwin,
            DFBInputDeviceKeySymbol     symbol,
            DFBInputDeviceModifierMask  modifiers )
{
     StackData        *data;
     SaWManGrabbedKey *key;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     data = sawwin->stack_data;
     D_MAGIC_ASSERT( data, StackData );

     direct_list_foreach (key, sawman->grabbed_keys) {
          if (key->symbol == symbol && key->modifiers == modifiers && key->owner == sawwin) {
               direct_list_remove( &sawman->grabbed_keys, &key->link );
               SHFREE( sawwin->shmpool, key );
               return DFB_OK;
          }
     }

     return DFB_IDNOTFOUND;
}

static DFBResult
request_focus( SaWMan       *sawman,
               SaWManWindow *sawwin )
{
     DFBResult        ret;
     CoreWindowStack *stack;
     SaWManWindow    *entered;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     stack = sawwin->stack;
     D_ASSERT( stack != NULL );

     ret = sawman_switch_focus( sawman, sawwin );
     if (ret)
          return ret;

     entered = sawman->entered_window;

     if (entered && entered != sawwin) {
          DFBWindowEvent we;

          D_MAGIC_ASSERT( entered, SaWManWindow );
          D_ASSERT( entered->window != NULL );

          we.type = DWET_LEAVE;
          we.x    = stack->cursor.x - entered->window->config.bounds.x;
          we.y    = stack->cursor.y - entered->window->config.bounds.y;

          sawman_post_event( sawman, entered, &we );

          sawman->entered_window = NULL;
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
handle_key_press( CoreWindowStack     *stack,
                  StackData           *data,
                  const DFBInputEvent *event )
{
     SaWMan       *sawman;
     SaWManWindow *sawwin;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYPRESS );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     sawwin = get_keyboard_window( data, event );
     if (sawwin)
          send_key_event( sawman, sawwin, event );

     return DFB_OK;
}

static DFBResult
handle_key_release( CoreWindowStack     *stack,
                    StackData           *data,
                    const DFBInputEvent *event )
{
     SaWMan       *sawman;
     SaWManWindow *sawwin;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_KEYRELEASE );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     sawwin = get_keyboard_window( data, event );
     if (sawwin)
          send_key_event( sawman, sawwin, event );

     return DFB_OK;
}

static DFBResult
handle_button_press( CoreWindowStack     *stack,
                     StackData           *data,
                     const DFBInputEvent *event )
{
     SaWMan       *sawman;
     SaWManWindow *sawwin;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_BUTTONPRESS );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     if (!stack->cursor.enabled)
          return DFB_OK;

     sawwin = sawman->pointer_window ? sawman->pointer_window : sawman->entered_window;
     if (sawwin)
          send_button_event( sawman, sawwin, stack, event );

     return DFB_OK;
}

static DFBResult
handle_button_release( CoreWindowStack     *stack,
                       StackData           *data,
                       const DFBInputEvent *event )
{
     SaWMan       *sawman;
     SaWManWindow *sawwin;

     D_ASSERT( stack != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( event != NULL );
     D_ASSERT( event->type == DIET_BUTTONRELEASE );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     if (!stack->cursor.enabled)
          return DFB_OK;

     sawwin = sawman->pointer_window ? sawman->pointer_window : sawman->entered_window;
     if (sawwin)
          send_button_event( sawman, sawwin, stack, event );

     return DFB_OK;
}

static void
handle_motion( CoreWindowStack *stack,
               SaWMan          *sawman,
               int              dx,
               int              dy )
{
     int            old_cx, old_cy;
     DFBWindowEvent we;

     D_ASSERT( stack != NULL );
     D_MAGIC_ASSERT( sawman, SaWMan );

     if (!stack->cursor.enabled)
          return;


     old_cx = stack->cursor.x;
     old_cy = stack->cursor.y;

     dfb_windowstack_cursor_warp( stack, old_cx + dx, old_cy + dy );

     dx = stack->cursor.x - old_cx;
     dy = stack->cursor.y - old_cy;

     if (!dx && !dy)
          return;


     if (sawman->pointer_window) {
          SaWManWindow *sawwin = sawman->pointer_window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );
          D_ASSERT( sawwin->window != NULL );

          we.type = DWET_MOTION;
          we.x    = stack->cursor.x - sawwin->window->config.bounds.x;
          we.y    = stack->cursor.y - sawwin->window->config.bounds.y;

          sawman_post_event( sawman, sawwin, &we );
     }
     else if (!update_focus( sawman, stack ) && sawman->entered_window) {
          SaWManWindow *sawwin = sawman->entered_window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );
          D_ASSERT( sawwin->window != NULL );

          we.type = DWET_MOTION;
          we.x    = stack->cursor.x - sawwin->window->config.bounds.x;
          we.y    = stack->cursor.y - sawwin->window->config.bounds.y;

          sawman_post_event( sawman, sawwin, &we );
     }
}

static void
handle_wheel( CoreWindowStack *stack,
              SaWMan          *sawman,
              int              dz )
{
     DFBWindowEvent  we;
     SaWManWindow   *sawwin;

     D_ASSERT( stack != NULL );
     D_MAGIC_ASSERT( sawman, SaWMan );

     if (!stack->cursor.enabled)
          return;

     sawwin = sawman->pointer_window ? sawman->pointer_window : sawman->entered_window;

     if (sawwin) {
          D_MAGIC_ASSERT( sawwin, SaWManWindow );
          D_ASSERT( sawwin->window != NULL );

          we.type = DWET_WHEEL;
          we.x    = stack->cursor.x - sawwin->window->config.bounds.x;
          we.y    = stack->cursor.y - sawwin->window->config.bounds.y;
          we.step = dz;

          sawman_post_event( sawman, sawwin, &we );
     }
}

static DFBResult
handle_axis_motion( CoreWindowStack     *stack,
                    SaWMan              *sawman,
                    SaWManTier          *tier,
                    const DFBInputEvent *event )
{
     D_ASSERT( stack != NULL );
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
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
                    tier->cursor_dx += rel;
                    break;

               case DIAI_Y:
                    tier->cursor_dy += rel;
                    break;

               case DIAI_Z:
                    handle_wheel( stack, sawman, - event->axisrel );
                    break;

               default:
                    ;
          }
     }
     else if (event->flags & DIEF_AXISABS) {
          switch (event->axis) {
               case DIAI_X:
                    tier->cursor_dx = event->axisabs - stack->cursor.x;
                    break;

               case DIAI_Y:
                    tier->cursor_dy = event->axisabs - stack->cursor.y;
                    break;

               default:
                    ;
          }
     }

     if (!(event->flags & DIEF_FOLLOW) && (tier->cursor_dx || tier->cursor_dy)) {
          handle_motion( stack, sawman, tier->cursor_dx, tier->cursor_dy );

          tier->cursor_dx = 0;
          tier->cursor_dy = 0;
     }

     return DFB_OK;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static void
wm_get_info( CoreWMInfo *info )
{
     info->version.major  = 0;
     info->version.minor  = 2;
     info->version.binary = 1;

     snprintf( info->name, DFB_CORE_WM_INFO_NAME_LENGTH, "SaWMan" );
     snprintf( info->vendor, DFB_CORE_WM_INFO_VENDOR_LENGTH, "directfb.org" );

     info->wm_data_size     = sizeof(WMData);
     info->wm_shared_size   = sizeof(SaWMan);
     info->stack_data_size  = sizeof(StackData);
     info->window_data_size = sizeof(SaWManWindow);
}

static DFBResult
wm_initialize( CoreDFB *core, void *wm_data, void *shared_data )
{
     WMData *data   = wm_data;
     SaWMan *sawman = shared_data;

     data->core   = core;
     data->world  = dfb_core_world( core );
     data->sawman = sawman;

     sawman_config_init( NULL, NULL );

     return sawman_initialize( sawman, data->world, &data->process );
}

static DFBResult
wm_join( CoreDFB *core, void *wm_data, void *shared_data )
{
     WMData *data   = wm_data;
     SaWMan *sawman = shared_data;

     data->core   = core;
     data->world  = dfb_core_world( core );
     data->sawman = sawman;

     sawman_config_init( NULL, NULL );

     return sawman_join( sawman, data->world, &data->process );
}

static DFBResult
wm_shutdown( bool emergency, void *wm_data, void *shared_data )
{
     WMData *data   = wm_data;
     SaWMan *sawman = shared_data;

     return sawman_shutdown( sawman, data->world );
}

static DFBResult
wm_leave( bool emergency, void *wm_data, void *shared_data )
{
     WMData *data   = wm_data;
     SaWMan *sawman = shared_data;

     return sawman_leave( sawman, data->world );
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

static DFBResult
wm_post_init( void *wm_data, void *shared_data )
{
     DFBResult   ret;
     SaWMan     *sawman = shared_data;
     SaWManTier *tier;

     D_ASSERT( wm_data != NULL );
     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     direct_list_foreach (tier, sawman->tiers) {
          const SaWManBorderInit *border;

          D_MAGIC_ASSERT( tier, SaWManTier );

          ret = dfb_layer_context_get_configuration( tier->context, &tier->config );
          if (ret)
               D_DERROR( ret, "SaWMan/PostInit: Could not get configuration of layer context!\n" );

          tier->context_lock  = tier->context->lock;
          tier->context->lock = sawman->lock;

          D_INFO( "SaWMan/Init: Layer  %d:  %dx%d, %s, options: %x\n",
                  tier->layer_id, tier->config.width, tier->config.height,
                  dfb_pixelformat_name( tier->config.pixelformat ), tier->config.options );


          border = &sawman_config->borders[(tier->classes & SWMSC_LOWER)  ? 0 :
                                           (tier->classes & SWMSC_MIDDLE) ? 1 : 2];

          tier->border_config = tier->config;

          if (border->resolution.w && border->resolution.h) {
               tier->border_config.width  = border->resolution.w;
               tier->border_config.height = border->resolution.h;
          }

          if (border->format)
               tier->border_config.pixelformat = border->format;

          tier->border_config.options = DLOP_SRC_COLORKEY;

          D_INFO( "SaWMan/Init: Border %d:  %dx%d, %s, options: %x\n",
                  tier->layer_id, tier->border_config.width, tier->border_config.height,
                  dfb_pixelformat_name( tier->border_config.pixelformat ), tier->border_config.options );
     }

     sawman_process_updates( sawman, DSFLIP_NONE );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
wm_init_stack( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data )
{
     DFBResult         ret;
     WMData           *wmdata = wm_data;
     StackData        *data   = stack_data;
     SaWMan           *sawman;
     SaWManTier       *tier;
     CoreLayerContext *context;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     sawman = wmdata->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     context = stack->context;
     D_ASSERT( context != NULL );
     D_ASSERT( context->config.pixelformat != DSPF_UNKNOWN );
     D_ASSERT( context->config.width > 0 );
     D_ASSERT( context->config.height > 0 );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     data->stack  = stack;
     data->sawman = sawman;

     D_MAGIC_SET( data, StackData );

     if (!sawman_tier_by_layer( sawman, context->layer_id, &tier ) || tier->stack) {
          sawman_unlock( sawman );
          return DFB_OK;
     }

     D_INFO( "SaWMan: Initializing stack %p for tier %p, %dx%d, layer %d, context %p [%d]...\n",
             stack, tier, stack->width, stack->height, context->layer_id, context, context->object.ref.multi.id );

     tier->stack   = stack;
     tier->context = context;
     tier->size.w  = stack->width;
     tier->size.h  = stack->height;
     tier->active  = true;

     ret = dfb_layer_context_get_primary_region( context, true, &tier->region );
     if (ret) {
          sawman_unlock( sawman );
          D_MAGIC_CLEAR( data );
          return ret;
     }

     dfb_layer_region_globalize( tier->region );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

static DFBResult
wm_close_stack( CoreWindowStack *stack,
                void            *wm_data,
                void            *stack_data )
{
     DFBResult    ret;
     StackData   *data = stack_data;
     SaWMan      *sawman;
     SaWManTier  *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     D_MAGIC_ASSERT( data, StackData );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );


     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_OK;
     }

     D_ASSERT( tier->context != NULL );

     tier->context->lock = tier->context_lock;

     tier->stack   = NULL;
     tier->context = NULL;
     tier->size.w  = 0;
     tier->size.h  = 0;

     dfb_layer_region_unlink( &tier->region );

     D_MAGIC_CLEAR( tier );

     /* Destroy backing store of software cursor. */
     if (tier->cursor_bs)
          dfb_surface_unlink( &tier->cursor_bs );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
wm_set_active( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data,
               bool             active )
{
     DFBResult    ret;
     StackData   *data = stack_data;
     SaWMan      *sawman;
     SaWManTier  *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_MAGIC_ASSERT( data, StackData );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %p, %s )\n", __FUNCTION__,
                 stack, wm_data, stack_data, active ? "active" : "inactive" );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_OK;
     }


     data->active = active;

     sawman_unlock( sawman );

     if (active)
          return DFB_OK;//dfb_windowstack_repaint_all( stack );

     /* Force release of all pressed keys. */
     return wm_flush_keys( stack, wm_data, stack_data );
}

static DFBResult
wm_resize_stack( CoreWindowStack *stack,
                 void            *wm_data,
                 void            *stack_data,
                 int              width,
                 int              height )
{
     DFBResult    ret;
     StackData   *data = stack_data;
     SaWMan      *sawman;
     SaWManTier  *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_MAGIC_ASSERT( data, StackData );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %p, %dx%d )\n", __FUNCTION__,
                 stack, wm_data, stack_data, width, height );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier ) || tier->single_mode) {
          sawman_unlock( sawman );
          return DFB_OK;
     }

     tier->size.w = width;
     tier->size.h = height;

     sawman_call( sawman, SWMCID_STACK_RESIZED, &tier->size );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

static DFBResult
wm_process_input( CoreWindowStack     *stack,
                  void                *wm_data,
                  void                *stack_data,
                  const DFBInputEvent *event )
{
     DFBResult    ret;
     StackData   *data = stack_data;
     SaWMan      *sawman;
     SaWManTier  *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->context != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( event != NULL );

     D_MAGIC_ASSERT( data, StackData );

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );

     D_DEBUG_AT( SaWMan_WM, "Processing input event (device %d, type 0x%08x, flags 0x%08x)...\n",
                   event->device_id, event->type, event->flags );

     if (stack->context->layer_id != DLID_PRIMARY)
          return DFB_OK;

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier ) || tier != (SaWManTier*)sawman->tiers) {
          sawman_unlock( sawman );
          return DFB_OK;
     }

     /* FIXME: handle multiple devices */
     if (event->flags & DIEF_BUTTONS)
          sawman->buttons = event->buttons;

     if (event->flags & DIEF_MODIFIERS)
          sawman->modifiers = event->modifiers;

     if (event->flags & DIEF_LOCKS)
          sawman->locks = event->locks;


     /* Store event in shared memory. */
     sawman->manager.event = *event;

     /* Call application manager executable. */
     ret = sawman_call( sawman, SWMCID_INPUT_FILTER, &sawman->manager.event );

     sawman_process_updates( sawman, DSFLIP_NONE );

     switch (ret) {
          case DFB_OK:
               event = &sawman->manager.event;
               break;

          case DFB_NOIMPL:
               break;

          default:
               /* filtered out */

               /* Unlock SaWMan. */
               sawman_unlock( sawman );

               return DFB_OK;
     }

     switch (event->type) {
          case DIET_KEYPRESS:
               ret = handle_key_press( stack, data, event );
               break;

          case DIET_KEYRELEASE:
               ret = handle_key_release( stack, data, event );
               break;

          case DIET_BUTTONPRESS:
               ret = handle_button_press( stack, data, event );
               break;

          case DIET_BUTTONRELEASE:
               ret = handle_button_release( stack, data, event );
               break;

          case DIET_AXISMOTION:
               ret = handle_axis_motion( stack, sawman, tier, event );
               break;

          default:
               D_ONCE( "unknown input event type" );
               ret = DFB_UNSUPPORTED;
               break;
     }

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return ret;
}

static DFBResult
wm_flush_keys( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data )
{
     int          i;
     DFBResult    ret;
     StackData   *data = stack_data;
     SaWMan      *sawman;
     SaWManTier  *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_MAGIC_ASSERT( data, StackData );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_OK;
     }

     for (i=0; i<SAWMAN_MAX_IMPLICIT_KEYGRABS; i++) {
          if (sawman->keys[i].code != -1) {
               DFBWindowEvent we;

               we.type       = DWET_KEYUP;
               we.key_code   = sawman->keys[i].code;
               we.key_id     = sawman->keys[i].id;
               we.key_symbol = sawman->keys[i].symbol;

               sawman_post_event( sawman, sawman->keys[i].owner, &we );

               sawman->keys[i].code = -1;
          }
     }

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

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
     DFBResult     ret;
     StackData    *data = stack_data;
     SaWMan       *sawman;
     SaWManWindow *sawwin;
     SaWManTier   *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( ret_window != NULL );

     D_MAGIC_ASSERT( data, StackData );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     sawwin = window_at_pointer( data->sawman, stack, x, y );

     *ret_window = sawwin ? sawwin->window : NULL;

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

static DFBResult
wm_window_lookup( CoreWindowStack  *stack,
                  void             *wm_data,
                  void             *stack_data,
                  DFBWindowID       window_id,
                  CoreWindow      **ret_window )
{
     DFBResult     ret;
     StackData    *data = stack_data;
     SaWMan       *sawman;
     SaWManWindow *window;
     SaWManTier   *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( ret_window != NULL );

     D_MAGIC_ASSERT( data, StackData );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     direct_list_foreach (window, sawman->windows) {
          D_MAGIC_ASSERT( window, SaWManWindow );

          if (window->id == window_id)
               break;
     }

     if (window) {
          D_ASSERT( window->window != NULL );

          *ret_window = window->window;
     }

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return window ? DFB_OK : DFB_ITEMNOTFOUND;
}

static DFBResult
wm_enum_windows( CoreWindowStack      *stack,
                 void                 *wm_data,
                 void                 *stack_data,
                 CoreWMWindowCallback  callback,
                 void                 *callback_ctx )
{
     DFBResult     ret;
     StackData    *data = stack_data;
     SaWMan       *sawman;
     SaWManWindow *window;
     SaWManTier   *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( callback != NULL );

     D_MAGIC_ASSERT( data, StackData );

     sawman = data->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     direct_list_foreach (window, sawman->windows) {
          D_MAGIC_ASSERT( window, SaWManWindow );
          D_ASSERT( window->window != NULL );

          if (callback( window->window, callback_ctx ) != DFENUM_OK)
               break;
     }

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
wm_get_insets( CoreWindowStack *stack,
               CoreWindow      *window,
               DFBInsets       *insets )
{
     insets->l = 0;
     insets->t = 0;
     insets->r = 0;
     insets->b = 0;

     return DFB_OK;
}


static DFBResult
wm_preconfigure_window( CoreWindowStack *stack,
                        void            *wm_data,
                        void            *stack_data,
                        CoreWindow      *window,
                        void            *window_data )
{
     DFBResult     ret;
     WMData       *wmdata = wm_data;
     SaWMan       *sawman;
     SaWManTier   *tier;
     SaWManWindow *sawwin = window_data;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     sawman = wmdata->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_OK;
     }

     if (window->parent_id) {
          SaWManWindow *parent = NULL;

          direct_list_foreach (parent, sawman->windows) {
               D_MAGIC_ASSERT( parent, SaWManWindow );
               D_ASSERT( parent->window != NULL );

               if (parent->id == window->parent_id)
                    break;
          }

          if (!parent) {
               D_ERROR( "SampleAppMan: Can't find parent window with ID %d!\n", window->parent_id );
               sawman_unlock( sawman );
               return DFB_IDNOTFOUND;
          }

          sawwin->parent = parent;
     }

     switch (ret = sawman_call( sawman, SWMCID_WINDOW_PRECONFIG, window )) {
          case DFB_OK:
               break;

          case DFB_NOIMPL:
               ret = DFB_OK;
               break;

          default:
               break;
     }

     sawman_process_updates( wmdata->sawman, DSFLIP_NONE );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return ret;
}

static DFBResult
wm_set_window_property( CoreWindowStack  *stack,
                        void             *wm_data,
                        void             *stack_data,
                        CoreWindow       *window,
                        void             *window_data,
                        const char       *key,
                        void             *value,
                        void            **ret_old_value )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( key != NULL );

     fusion_object_set_property((FusionObject*)window,
                                key,value,ret_old_value);
     return DFB_OK;
}

static DFBResult
wm_get_window_property( CoreWindowStack  *stack,
                        void             *wm_data,
                        void             *stack_data,
                        CoreWindow       *window,
                        void             *window_data,
                        const char       *key,
                        void            **ret_value )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( key != NULL );
     D_ASSERT( ret_value != NULL );

     *ret_value = fusion_object_get_property((FusionObject*)window,key);

     return DFB_OK;
}


static DFBResult
wm_remove_window_property( CoreWindowStack  *stack,
                           void             *wm_data,
                           void             *stack_data,
                           CoreWindow       *window,
                           void             *window_data,
                           const char       *key,
                           void            **ret_value )
{
     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( key != NULL );

     fusion_object_remove_property((FusionObject*)window,key,ret_value);

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
wm_add_window( CoreWindowStack *stack,
               void            *wm_data,
               void            *stack_data,
               CoreWindow      *window,
               void            *window_data )
{
     DFBResult     ret;
     WMData       *wmdata = wm_data;
     SaWManWindow *sawwin = window_data;
     StackData    *sdata  = stack_data;
     SaWMan       *sawman;
     SaWManTier   *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( sdata, StackData );

     sawman = sdata->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          D_ERROR( "SaWMan/WM: Cannot add window to unknown stack!\n" );
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     /* Initialize window data. */
     sawwin->sawman     = sawman;
     sawwin->shmpool    = sawman->shmpool;
     sawwin->process    = wmdata->process;
     sawwin->id         = window->id;
     sawwin->caps       = window->caps;
     sawwin->window     = window;
     sawwin->stack      = stack;
     sawwin->stack_data = stack_data;

     D_MAGIC_SET( sawwin, SaWManWindow );

     if (window->config.options & (DWOP_KEEP_ABOVE | DWOP_KEEP_UNDER)) {
          if (!sawwin->parent) {
               D_ERROR( "SaWMan/WM: Cannot use KEEP_ABOVE/UNDER without a parent!\n" );
               D_MAGIC_CLEAR( sawwin );
               sawman_unlock( sawman );
               return DFB_UNSUPPORTED;
          }

          if (window->config.options & DWOP_KEEP_ABOVE) {
               if (sawman_window_priority(sawwin->parent) > sawman_window_priority(sawwin)) {
                    D_MAGIC_CLEAR( sawwin );
                    sawman_unlock( sawman );
                    return DFB_INVARG;
               }
          }
          else {
               if (sawman_window_priority(sawwin->parent) < sawman_window_priority(sawwin)) {
                    D_MAGIC_CLEAR( sawwin );
                    sawman_unlock( sawman );
                    return DFB_INVARG;
               }
          }
     }

     fusion_vector_init( &sawwin->children, 2, sawwin->shmpool );

     sawwin->priority   = sawman_window_priority( sawwin );

     direct_list_append( &sawman->windows, &sawwin->link );

     switch (ret = sawman_call( sawman, SWMCID_WINDOW_ADDED, sawwin )) {
          case DFB_OK:
               break;

          case DFB_NOIMPL:
               /* Actually add the window to the stack. */
               ret = sawman_insert_window( sawman, sawwin, NULL, true );
               break;

          default:
               direct_list_remove( &sawman->windows, &sawwin->link );
               break;
     }

     if (ret == DFB_OK && sawwin->parent) {
          SaWManWindow *parent = sawwin->parent;

          D_MAGIC_ASSERT( parent, SaWManWindow );
          D_ASSERT( parent->window != NULL );
          D_ASSERT( parent->id == window->parent_id );

          ret = fusion_vector_add( &parent->children, sawwin );
          if (ret)
               return ret;
     }

     sawman_update_geometry( sawwin );

     sawman_process_updates( wmdata->sawman, DSFLIP_NONE );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

static DFBResult
wm_remove_window( CoreWindowStack *stack,
                  void            *wm_data,
                  void            *stack_data,
                  CoreWindow      *window,
                  void            *window_data )
{
     DFBResult     ret;
     SaWManWindow *sawwin = window_data;
     StackData    *sdata  = stack_data;
     SaWMan       *sawman;
     SaWManTier   *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     D_ASSERT( window != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_MAGIC_ASSERT( sdata, StackData );

     sawman = sdata->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Check for valid stack. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          D_ERROR( "SaWMan/WM: Cannot remove window from unknown stack!\n" );
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     /* Retrieve corresponding SaWManTier. */
     tier = sawman_tier_by_class( sawman, window->config.stacking );
     if (!tier) {
          sawman_unlock( sawman );
          return DFB_BUG;
     }

     direct_list_remove( &sawman->windows, &sawwin->link );

     switch (ret = sawman_call( sawman, SWMCID_WINDOW_REMOVED, sawwin )) {
          case DFB_NOIMPL:
               ret = DFB_OK;

          case DFB_OK:
               D_ASSERT( sawwin->children.count == 0 );

               /* Actually remove the window from the stack. */
               if (sawwin->flags & SWMWF_INSERTED)
                    sawman_remove_window( sawman, sawwin );
               else
                    sawman_withdraw_window( sawman, sawwin );

               /* Remove from parent window. */
               if (sawwin->parent) {
                    SaWManWindow *parent = sawwin->parent;

                    D_MAGIC_ASSERT( parent, SaWManWindow );

                    fusion_vector_remove( &parent->children, fusion_vector_index_of( &parent->children, sawwin ) );
                    sawwin->parent = NULL;
               }

               D_MAGIC_CLEAR( sawwin );
               break;

          default:
               direct_list_append( &sawman->windows, &sawwin->link );
               break;
     }

     if (tier->single_window == sawwin)
          tier->single_window = NULL;

     sawman_process_updates( sdata->sawman, DSFLIP_NONE );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return ret;
}

/**********************************************************************************************************************/

static DFBResult
wm_set_window_config( CoreWindow             *window,
                      void                   *wm_data,
                      void                   *window_data,
                      const CoreWindowConfig *updated,
                      CoreWindowConfigFlags   flags )
{
     DFBResult         ret;
     WMData           *wmdata = wm_data;
     SaWMan           *sawman;
     SaWManWindow     *sawwin = window_data;
     SaWManTier       *tier;
     CoreWindowConfig *config;
     CoreWindowStack  *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( window->stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( updated != NULL );

     sawman = wmdata->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     stack = sawwin->stack;
     D_ASSERT( stack != NULL );

     if ((flags & CWCF_OPTIONS) && (updated->options & (DWOP_KEEP_ABOVE | DWOP_KEEP_UNDER))) {
          if (!sawwin->parent)
               return DFB_UNSUPPORTED;

          if (updated->options & DWOP_KEEP_ABOVE) {
               if (sawman_window_priority(sawwin->parent) > sawman_window_priority(sawwin))
                    return DFB_INVARG;
          }
          else {
               if (sawman_window_priority(sawwin->parent) < sawman_window_priority(sawwin))
                    return DFB_INVARG;
          }
     }

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     sawwin->config.current = 
     sawwin->config.request = window->config;
     sawwin->config.flags   = flags;

     config = &sawwin->config.request;

     if (flags & CWCF_COLOR_KEY)
          config->color_key = updated->color_key;

     if (flags & CWCF_EVENTS)
          config->events = updated->events;

     if (flags & CWCF_OPACITY)
          config->opacity = updated->opacity;

     if (flags & CWCF_OPAQUE)
          config->opaque = updated->opaque;

     if (flags & CWCF_OPTIONS)
          config->options = updated->options;

     if (flags & CWCF_POSITION) {
          config->bounds.x = updated->bounds.x;
          config->bounds.y = updated->bounds.y;
     }

     if (flags & CWCF_SIZE) {
          config->bounds.w = updated->bounds.w;
          config->bounds.h = updated->bounds.h;
     }

     if (flags & CWCF_STACKING)
          config->stacking = updated->stacking;

     if (flags & CWCF_SRC_GEOMETRY)
          config->src_geometry = updated->src_geometry;

     if (flags & CWCF_DST_GEOMETRY)
          config->dst_geometry = updated->dst_geometry;

     switch (ret = sawman_call( sawman, SWMCID_WINDOW_CONFIG, sawwin )) {
          case DFB_OK:
               flags = sawwin->config.flags;
               break;

          case DFB_NOIMPL:
               break;

          default:
               sawman_unlock( sawman );
               return ret;
     }


     if (flags & CWCF_OPTIONS) {
          if ((window->config.options & DWOP_SCALE) && !(config->options & DWOP_SCALE)) {
               if (window->config.bounds.w != window->surface->config.size.w ||
                   window->config.bounds.h != window->surface->config.size.h)
               {
                    ret = dfb_surface_reformat( window->surface,
                                                window->config.bounds.w,
                                                window->config.bounds.h,
                                                window->surface->config.format );
                    if (ret) {
                         D_DERROR( ret, "WM/Default: Could not resize surface "
                                        "(%dx%d -> %dx%d) to remove DWOP_SCALE!\n",
                                   window->surface->config.size.w,
                                   window->surface->config.size.h,
                                   window->config.bounds.w,
                                   window->config.bounds.h );
                         sawman_unlock( sawman );
                         return ret;
                    }
               }
          }

          if (config->options & (DWOP_KEEP_ABOVE | DWOP_KEEP_UNDER)) {
               D_ASSERT( sawwin->parent );

               if (config->options & DWOP_KEEP_ABOVE) {
                    D_ASSERT( sawman_window_priority(sawwin->parent) <= sawman_window_priority(sawwin) );

                    sawman_insert_window( sawman, sawwin, sawwin->parent, true );
               }
               else {
                    D_ASSERT( sawman_window_priority(sawwin->parent) >= sawman_window_priority(sawwin) );

                    sawman_insert_window( sawman, sawwin, sawwin->parent, false );
               }

               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, true, false, false );
          }

          window->config.options = config->options;
     }

     if (flags & CWCF_EVENTS)
          window->config.events = config->events;

     if (flags & CWCF_COLOR_KEY)
          window->config.color_key = config->color_key;

     if (flags & CWCF_OPAQUE)
          window->config.opaque = config->opaque;

     if (flags & CWCF_OPACITY && !config->opacity)
          set_opacity( sawman, sawwin, config->opacity );

     if (flags == (CWCF_POSITION | CWCF_SIZE)) {
          ret = set_window_bounds( sawman, sawwin, wm_data,
                                   config->bounds.x, config->bounds.y,
                                   config->bounds.w, config->bounds.h);
          if (ret) {
               sawman_unlock( sawman );
               return ret;
          }
     }
     else {
          if (flags & CWCF_POSITION) {
               ret = move_window( sawman, sawwin,
                                  config->bounds.x - window->config.bounds.x,
                                  config->bounds.y - window->config.bounds.y );
               if (ret) {
                    sawman_unlock( sawman );
                    return ret;
               }
          }

          if (flags & CWCF_SIZE) {
               ret = resize_window( sawman, sawwin, wm_data, config->bounds.w, config->bounds.h );
               if (ret) {
                    sawman_unlock( sawman );
                    return ret;
               }
          }
     }

     if (flags & CWCF_STACKING)
          restack_window( sawman, sawwin, sawwin, 0, config->stacking );

     if (flags & CWCF_OPACITY && config->opacity) {
          set_opacity( sawman, sawwin, config->opacity );

          /* Possibly switch focus to window now under the cursor */
          update_focus( sawman, stack );
     }

     if (flags & CWCF_KEY_SELECTION) {
          if (config->key_selection == DWKS_LIST) {
               unsigned int             bytes = sizeof(DFBInputDeviceKeySymbol) * config->num_keys;
               DFBInputDeviceKeySymbol *keys;
     
               D_ASSERT( config->keys != NULL );
               D_ASSERT( config->num_keys > 0 );
     
               keys = SHMALLOC( window->stack->shmpool, bytes );
               if (!keys) {
                    D_ERROR( "SaWMan/WM: Could not allocate %d bytes for list "
                             "of selected keys (%d)!\n", bytes, config->num_keys );
                    return D_OOSHM();
               }
     
               direct_memcpy( keys, config->keys, bytes );

               qsort( keys, config->num_keys, sizeof(DFBInputDeviceKeySymbol), keys_compare );
     
               if (window->config.keys)
                    SHFREE( window->stack->shmpool, window->config.keys );
     
               window->config.keys     = keys;
               window->config.num_keys = config->num_keys;
          }
          else if (window->config.keys) {
               SHFREE( window->stack->shmpool, window->config.keys );

               window->config.keys     = NULL;
               window->config.num_keys = 0;
          }

          window->config.key_selection = config->key_selection;
     }

     if (flags & CWCF_SRC_GEOMETRY)
          window->config.src_geometry = config->src_geometry;

     if (flags & CWCF_DST_GEOMETRY)
          window->config.dst_geometry = config->dst_geometry;

     /* Update geometry? */
     if (flags & (CWCF_POSITION | CWCF_SIZE | CWCF_SRC_GEOMETRY | CWCF_DST_GEOMETRY))
          sawman_update_geometry( sawwin );

     sawman_process_updates( sawman, DSFLIP_NONE );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

static DFBResult
wm_restack_window( CoreWindow             *window,
                   void                   *wm_data,
                   void                   *window_data,
                   CoreWindow             *relative,
                   void                   *relative_data,
                   int                     relation )
{
     DFBResult        ret;
     SaWMan          *sawman;
     SaWManWindow    *sawwin = window_data;
     SaWManTier      *tier;
     CoreWindowStack *stack;
     StackData       *data;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     D_ASSERT( relative == NULL || relative_data != NULL );
     D_ASSERT( relative == NULL || relative == window || relation != 0);

     data = sawwin->stack_data;
     D_MAGIC_ASSERT( data, StackData );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     stack = sawwin->stack;
     D_ASSERT( stack != NULL );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     switch (ret = sawman_call( sawman, SWMCID_WINDOW_RESTACK, sawwin )) {
          case DFB_OK:
          case DFB_NOIMPL:
               break;

          default:
               sawman_unlock( sawman );
               return ret;
     }

     ret = restack_window( sawman, sawwin, relative_data, relation, window->config.stacking );
     if (ret) {
          sawman_unlock( sawman );
          return ret;
     }

     /* Possibly switch focus to window now under the cursor */
     update_focus( sawman, stack );

     sawman_process_updates( data->sawman, DSFLIP_NONE );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
wm_grab( CoreWindow *window,
         void       *wm_data,
         void       *window_data,
         CoreWMGrab *grab )
{
     DFBResult        ret;
     WMData          *wmdata = wm_data;
     SaWMan          *sawman;
     SaWManTier      *tier;
     SaWManWindow    *sawwin = window_data;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( grab != NULL );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     sawman = wmdata->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     stack = sawwin->stack;
     D_ASSERT( stack != NULL );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     switch (grab->target) {
          case CWMGT_KEYBOARD:
               ret = grab_keyboard( wmdata->sawman, window_data );
               break;

          case CWMGT_POINTER:
               ret = grab_pointer( wmdata->sawman, window_data );
               break;

          case CWMGT_KEY:
               ret = grab_key( wmdata->sawman, window_data, grab->symbol, grab->modifiers );
               break;

          case CWMGT_UNSELECTED_KEYS:
               if (sawman->unselkeys_window)
                    ret = DFB_LOCKED;
               else
                    sawman->unselkeys_window = sawwin;

               break;

          default:
               D_BUG( "unknown grab target" );
               ret = DFB_BUG;
     }

     sawman_process_updates( wmdata->sawman, DSFLIP_NONE );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return ret;
}

static DFBResult
wm_ungrab( CoreWindow *window,
           void       *wm_data,
           void       *window_data,
           CoreWMGrab *grab )
{
     DFBResult        ret;
     WMData          *wmdata = wm_data;
     SaWMan          *sawman;
     SaWManTier      *tier;
     SaWManWindow    *sawwin = window_data;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_ASSERT( grab != NULL );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     sawman = wmdata->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     stack = sawwin->stack;
     D_ASSERT( stack != NULL );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     switch (grab->target) {
          case CWMGT_KEYBOARD:
               ret = ungrab_keyboard( wmdata->sawman, window_data );
               break;

          case CWMGT_POINTER:
               ret = ungrab_pointer( wmdata->sawman, window_data );
               break;

          case CWMGT_KEY:
               ret = ungrab_key( wmdata->sawman, window_data, grab->symbol, grab->modifiers );
               break;

          case CWMGT_UNSELECTED_KEYS:
               if (sawman->unselkeys_window == sawwin)
                    sawman->unselkeys_window = NULL;

          default:
               D_BUG( "unknown grab target" );
               ret = DFB_BUG;
     }

     sawman_process_updates( wmdata->sawman, DSFLIP_NONE );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return ret;
}

static DFBResult
wm_request_focus( CoreWindow *window,
                  void       *wm_data,
                  void       *window_data )
{
     DFBResult        ret;
     WMData          *wmdata = wm_data;
     SaWMan          *sawman;
     SaWManTier      *tier;
     SaWManWindow    *sawwin = window_data;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     sawman = wmdata->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     stack = sawwin->stack;
     D_ASSERT( stack != NULL );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     ret = request_focus( wmdata->sawman, window_data );

     sawman_process_updates( wmdata->sawman, DSFLIP_NONE );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return ret;
}

/**********************************************************************************************************************/

static DFBResult
wm_update_stack( CoreWindowStack     *stack,
                 void                *wm_data,
                 void                *stack_data,
                 const DFBRegion     *region,
                 DFBSurfaceFlipFlags  flags )
{
     DFBResult    ret;
     StackData   *data = stack_data;
     SaWMan      *sawman;
     SaWManTier  *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );
     DFB_REGION_ASSERT( region );

     D_MAGIC_ASSERT( data, StackData );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %p, %d,%d-%dx%d )\n", __FUNCTION__,
                 stack, wm_data, stack_data, DFB_RECTANGLE_VALS_FROM_REGION( region ) );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     if (!tier->single_mode) {
          dfb_updates_add( &tier->updates, region );

          ret = sawman_process_updates( sawman, flags );
     }

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return ret;
}

static DFBResult
wm_update_window( CoreWindow          *window,
                  void                *wm_data,
                  void                *window_data,
                  const DFBRegion     *region,    /* surface coordinates! */
                  DFBSurfaceFlipFlags  flags )
{
     DFBResult        ret;
     WMData          *wmdata = wm_data;
     SaWMan          *sawman;
     SaWManWindow    *sawwin = window_data;
     SaWManTier      *tier;
     CoreWindowStack *stack;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     DFB_REGION_ASSERT_IF( region );

     sawman = wmdata->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     stack = sawwin->stack;
     D_ASSERT( stack != NULL );

     if (region)
          D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %p, %d,%d-%dx%d )\n", __FUNCTION__,
                      window, wm_data, window_data, DFB_RECTANGLE_VALS_FROM_REGION( region ) );
     else
          D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %p, <0,0-%dx%d> )\n", __FUNCTION__,
                      window, wm_data, window_data, window->config.bounds.w, window->config.bounds.h );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     tier = sawman_tier_by_class( sawman, window->config.stacking );
     if (!tier) {
          D_ERROR( "SaWMan/WM: Cannot update window on unknown stack!\n" );
          sawman_unlock( sawman );
          return DFB_BUG;
     }

     if (tier->single_mode && tier->single_window != NULL) {
          D_ASSERT( tier->region != NULL );

          if (tier->single_window == sawwin) {
               DFBRectangle tmp = sawwin->src;

               if (!region || dfb_rectangle_intersect_by_region( &tmp, region )) {
                    DFBRectangle src = tmp;

                    tmp.x -= sawwin->src.x;
                    tmp.y -= sawwin->src.y;

                    DFBRegion reg = DFB_REGION_INIT_FROM_RECTANGLE( &tmp );

                    dfb_gfx_copy_to( window->surface, tier->region->surface, &src, tmp.x, tmp.y, false );

                    dfb_layer_region_flip_update( tier->region, &reg, flags );
               }
          }
     }
     else {
          sawman_update_window( sawman, sawwin,
                                sawwin->parent ? NULL : region, /* FIXME? */
                                flags, false, false, true );

          sawman_process_updates( sawman, flags );
     }

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

static DFBResult
wm_update_cursor( CoreWindowStack       *stack,
                  void                  *wm_data,
                  void                  *stack_data,
                  CoreCursorUpdateFlags  flags )
{
     DFBResult         ret;
     DFBRegion         old_region;
     WMData           *wmdata   = wm_data;
     bool              restored = false;
     CoreLayerContext *context;
     CoreLayerRegion  *primary;
     CoreSurface      *surface;
     SaWMan           *sawman;
     SaWManTier       *tier;

     D_ASSERT( stack != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( stack_data != NULL );

     sawman = wmdata->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     old_region = tier->cursor_region;

     if (flags & (CCUF_ENABLE | CCUF_POSITION | CCUF_SIZE)) {
          tier->cursor_bs_valid  = false;

          tier->cursor_region.x1 = stack->cursor.x - stack->cursor.hot.x;
          tier->cursor_region.y1 = stack->cursor.y - stack->cursor.hot.y;
          tier->cursor_region.x2 = tier->cursor_region.x1 + stack->cursor.size.w - 1;
          tier->cursor_region.y2 = tier->cursor_region.y1 + stack->cursor.size.h - 1;

          if (!dfb_region_intersect( &tier->cursor_region, 0, 0, stack->width - 1, stack->height - 1 )) {
               D_BUG( "invalid cursor region" );
               sawman_unlock( sawman );
               return DFB_BUG;
          }
     }

     /* Optimize case of invisible cursor moving. */
     if (!(flags & ~(CCUF_POSITION | CCUF_SHAPE)) && (!stack->cursor.opacity || !stack->cursor.enabled)) {
          sawman_unlock( sawman );
          return DFB_OK;
     }

     context = stack->context;
     D_ASSERT( context != NULL );

     if (!tier->cursor_bs) {
          CoreSurface *cursor_bs;

          D_ASSUME( flags & CCUF_ENABLE );

          /* Create the cursor backing store surface. */
          ret = dfb_surface_create_simple( wmdata->core, stack->cursor.size.w, stack->cursor.size.h,
                                           context->config.pixelformat, DSCAPS_NONE,
                                           CSTF_SHARED | CSTF_CURSOR, 0, NULL, &cursor_bs );
          if (ret) {
               D_ERROR( "WM/Default: Failed creating backing store for cursor!\n" );
               sawman_unlock( sawman );
               return ret;
          }

          ret = dfb_surface_globalize( cursor_bs );
          D_ASSERT( ret == DFB_OK );

          tier->cursor_bs = cursor_bs;
     }

     D_ASSERT( tier->cursor_bs != NULL );

     /* Get the primary region. */
     ret = dfb_layer_context_get_primary_region( context, false, &primary );
     if (ret) {
          sawman_unlock( sawman );
          return ret;
     }

     surface = primary->surface;

     D_ASSERT( surface != NULL );

     if (flags & CCUF_ENABLE) {
         /* Ensure valid back buffer. From now on swapping is prevented until cursor is disabled.
          * FIXME: Keep a flag to know when back/front have been swapped and need a sync.
          */
         switch (context->config.buffermode) {
              case DLBM_BACKVIDEO:
              case DLBM_TRIPLE:
                   dfb_gfx_copy( surface, surface, NULL );
                   break;

              default:
                   break;
         }
     }

     /* restore region under cursor */
     if (tier->cursor_drawn) {
          DFBRectangle rect = { 0, 0,
                                old_region.x2 - old_region.x1 + 1,
                                old_region.y2 - old_region.y1 + 1 };

          D_ASSERT( stack->cursor.opacity || (flags & CCUF_OPACITY) );

          dfb_gfx_copy_to( tier->cursor_bs, surface, &rect, old_region.x1, old_region.y1, false );

          tier->cursor_drawn = false;

          restored = true;
     }

     if (flags & CCUF_SIZE) {
          ret = dfb_surface_reformat( tier->cursor_bs,
                                      stack->cursor.size.w, stack->cursor.size.h,
                                      tier->cursor_bs->config.format );
          if (ret)
               D_DERROR( ret, "WM/Default: Failed resizing backing store for cursor from %dx%d to %dx%d!\n",
                         tier->cursor_bs->config.size.w, tier->cursor_bs->config.size.h, stack->cursor.size.w, stack->cursor.size.h );
     }

     if (flags & CCUF_DISABLE) {
          dfb_surface_unlink( &tier->cursor_bs );
     }
     else if (stack->cursor.opacity) {
          CoreLayer *layer = dfb_layer_at( context->layer_id );
          CardState *state = &layer->state;

          /* backup region under cursor */
          if (!tier->cursor_bs_valid) {
               DFBRectangle rect = DFB_RECTANGLE_INIT_FROM_REGION( &tier->cursor_region );

               D_ASSERT( !tier->cursor_drawn );

               /* FIXME: this requires using blitted flipping all the time,
                  but fixing it seems impossible, for now DSFLIP_BLIT is forced
                  in repaint_stack() when the cursor is enabled. */
               dfb_gfx_copy_to( surface, tier->cursor_bs, &rect, 0, 0, true );

               tier->cursor_bs_valid = true;
          }

          /* Set destination. */
          state->destination  = surface;
          state->modified    |= SMF_DESTINATION;

          /* Set clipping region. */
          dfb_state_set_clip( state, &tier->cursor_region );

          /* draw cursor */
          sawman_draw_cursor( stack, state, &tier->cursor_region );

          /* Reset destination. */
          state->destination  = NULL;
          state->modified    |= SMF_DESTINATION;

          tier->cursor_drawn = true;

          if (restored) {
               if (dfb_region_region_intersects( &old_region, &tier->cursor_region ))
                    dfb_region_region_union( &old_region, &tier->cursor_region );
               else
                    dfb_layer_region_flip_update( primary, &tier->cursor_region, DSFLIP_BLIT );

               dfb_layer_region_flip_update( primary, &old_region, DSFLIP_BLIT );
          }
          else
               dfb_layer_region_flip_update( primary, &tier->cursor_region, DSFLIP_BLIT );
     }
     else if (restored)
          dfb_layer_region_flip_update( primary, &old_region, DSFLIP_BLIT );

     /* Unref primary region. */
     dfb_layer_region_unref( primary );

     sawman_unlock( sawman );

     return DFB_OK;
}

