/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <fusion/conf.h>
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

#include <gfx/clip.h>
#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/conf.h>
#include <misc/gfx_util.h>
#include <misc/util.h>

#include <core/wm_module.h>

#include <sawman/sawman_config.h>

#include <sawman/sawman_draw.h>
#include <sawman/sawman_updates.h>
#include <sawman/sawman_window.h>


DFB_WINDOW_MANAGER( sawman )


D_DEBUG_DOMAIN( SaWMan_WM      , "SaWMan/WM",       "SaWMan window manager module" );
D_DEBUG_DOMAIN( SaWMan_Cursor,   "SaWMan/Cursor",   "SaWMan window manager cursor" );
D_DEBUG_DOMAIN( SaWMan_Stacking, "SaWMan/Stacking", "SaWMan window manager stacking" );
D_DEBUG_DOMAIN( SaWMan_FlipOnce, "SaWMan/FlipOnce", "SaWMan window manager flip once" );
D_DEBUG_DOMAIN( SaWMan_Surface,  "SaWMan/Surface",  "SaWMan window manager surface" );

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

     we.type        = (event->type == DIET_KEYPRESS) ? DWET_KEYDOWN : DWET_KEYUP;
     we.flags       = (event->flags & DIEF_REPEAT) ? DWEF_REPEAT : 0;
     we.key_code    = event->key_code;
     we.key_id      = event->key_id;
     we.key_symbol  = event->key_symbol;

     we.flags      |= DWEF_DEVICE_ID;
     we.device_id   = event->device_id;

     if (event->flags & DIEF_REPEAT)
          we.flags |= DWEF_REPEAT;

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

     we.type       = (event->type == DIET_BUTTONPRESS) ? DWET_BUTTONDOWN : DWET_BUTTONUP;
     we.button     = event->button;

     we.flags     |= DWEF_DEVICE_ID;
     we.device_id  = event->device_id;

     sawman_window_get_cursor_position( sawman, stack, sawwin, &we.x, &we.y, &we.cx, &we.cy );

     if (window->config.options & DWOP_SCALE) {
          D_ASSERT( window->surface != NULL );

          we.x = we.x * window->surface->config.size.w / sawwin->bounds.w;
          we.y = we.y * window->surface->config.size.h / sawwin->bounds.h;
     }

     if (event->flags & DIEF_REPEAT)
          we.flags |= DWEF_REPEAT;

     sawman_post_event( sawman, sawwin, &we );
}

/**********************************************************************************************************************/

static SaWManWindow *
get_keyboard_window( StackData           *data,
                     const DFBInputEvent *event )
{
     SaWMan           *sawman;
     SaWManWindow     *sawwin;
     CoreWindow       *window;
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
               sawwin = sawman->unselkeys_window;
               if (!sawwin)
                    return NULL;

               break;
     }

     /* key is for this window */

     /* only do implicit grabbing if we have a hardware key code */
     if( event->key_code == -1 )
          return sawwin;

     /* do implicit grabbing */
     if( event->type == DIET_KEYPRESS ) {
          int i;
          int free_key = -1;

          for (i=0; i<SAWMAN_MAX_IMPLICIT_KEYGRABS; i++) {
               /* Key is grabbed, send to owner (NULL if destroyed). */
               if (sawman->keys[i].code == event->key_code)
                    return sawman->keys[i].owner;

               /* Remember first free array item. */
               if (free_key == -1 && sawman->keys[i].code == -1)
                    free_key = i;
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
     SaWManTier     *tier;
     bool            stereo_layer;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     window = sawwin->window;
     D_ASSERT( window != NULL );

     tier = sawman_tier_by_class( sawman, window->config.stacking );
     D_ASSERT( tier->region != NULL );
     stereo_layer = tier->region->config.options & DLOP_STEREO;

     bounds = &window->config.bounds;
     bounds->x += dx;
     bounds->y += dy;

     if (SAWMAN_VISIBLE_WINDOW(window) && (sawwin->flags & SWMWF_INSERTED)) {
          DFBRegion region = { 0, 0, bounds->w - 1, bounds->h - 1 };

          dfb_region_translate( &region, sawwin->window->config.z, 0 );
          sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER );
          dfb_region_translate( &region, -sawwin->window->config.z, 0 );

          if (stereo_layer) {
               dfb_region_translate( &region, -sawwin->window->config.z, 0 );
               sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
               dfb_region_translate( &region, sawwin->window->config.z, 0 );
          }

          dfb_region_translate( &region, sawwin->window->config.z, 0 );

          dfb_region_translate( &region, -dx, -dy );
          sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER );
          dfb_region_translate( &region, -sawwin->window->config.z, 0 );

          if (stereo_layer) {
               dfb_region_translate( &region, -sawwin->window->config.z, 0 );
               sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
               dfb_region_translate( &region, sawwin->window->config.z, 0 );
          }
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
     SaWManTier     *tier;
     bool            stereo_layer;

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

     tier = sawman_tier_by_class( sawman, window->config.stacking );
     D_ASSERT( tier->region != NULL );
     stereo_layer = tier->region->config.options & DLOP_STEREO;

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

     if (SAWMAN_VISIBLE_WINDOW (window) && (sawwin->flags & SWMWF_INSERTED)) {
          if (ow > bounds->w) {
               DFBRegion region = { bounds->w, 0, ow - 1, MIN(bounds->h, oh) - 1 };

               dfb_region_translate( &region, sawwin->window->config.z, 0 );
               sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER );
               dfb_region_translate( &region, -sawwin->window->config.z, 0 );

               if (stereo_layer) {
                    dfb_region_translate( &region, sawwin->window->config.z, 0 );
                    sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                    dfb_region_translate( &region, -sawwin->window->config.z, 0 );
               }
          }

          if (oh > bounds->h) {
               DFBRegion region = { 0, bounds->h, MAX(bounds->w, ow) - 1, oh - 1 };

               dfb_region_translate( &region, sawwin->window->config.z, 0 );
               sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER );
               dfb_region_translate( &region, -sawwin->window->config.z, 0 );

               if (stereo_layer) {
                    dfb_region_translate( &region, sawwin->window->config.z, 0 );
                    sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                    dfb_region_translate( &region, -sawwin->window->config.z, 0 );
               }
          }
     }

     /* Send new size */
     evt.type = DWET_SIZE;
     evt.w    = bounds->w;
     evt.h    = bounds->h;

     sawman_post_event( sawman, sawwin, &evt );

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
     SaWManTier     *tier;
     bool            stereo_layer;

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

     tier = sawman_tier_by_class( sawman, window->config.stacking );
     D_ASSERT(tier->region);
     stereo_layer = tier->region->config.options & DLOP_STEREO;

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
     if (SAWMAN_VISIBLE_WINDOW( window ) && (sawwin->flags & SWMWF_INSERTED)) {
          if (dfb_region_region_intersect( &new_region, &old_region )) {
               /* left */
               if (new_region.x1 > old_region.x1) {
                    DFBRegion region = { old_region.x1, old_region.y1,
                                         new_region.x1 - 1, new_region.y2 };

                    dfb_region_translate( &region, sawwin->window->config.z, 0 );
                    sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER );
                    dfb_region_translate( &region, -sawwin->window->config.z, 0 );
                    if (stereo_layer) {
                         dfb_region_translate( &region, -sawwin->window->config.z, 0 );
                         sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                         dfb_region_translate( &region, sawwin->window->config.z, 0 );
                    }
               }

               /* upper */
               if (new_region.y1 > old_region.y1) {
                    DFBRegion region = { old_region.x1, old_region.y1,
                                         old_region.x2, new_region.y1 - 1 };

                    dfb_region_translate( &region, sawwin->window->config.z, 0 );
                    sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER );
                    dfb_region_translate( &region, -sawwin->window->config.z, 0 );
                    if (stereo_layer) {
                         dfb_region_translate( &region, -sawwin->window->config.z, 0 );
                         sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                         dfb_region_translate( &region, sawwin->window->config.z, 0 );
                    }
               }

               /* right */
               if (new_region.x2 < old_region.x2) {
                    DFBRegion region = { new_region.x2 + 1, new_region.y1,
                                         old_region.x2, new_region.y2 };

                    dfb_region_translate( &region, sawwin->window->config.z, 0 );
                    sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER );
                    dfb_region_translate( &region, -sawwin->window->config.z, 0 );
                    if (stereo_layer) {
                         dfb_region_translate( &region, -sawwin->window->config.z, 0 );
                         sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                         dfb_region_translate( &region, sawwin->window->config.z, 0 );
                    }
               }

               /* lower */
               if (new_region.y2 < old_region.y2) {
                    DFBRegion region = { old_region.x1, new_region.y2 + 1,
                                         old_region.x2, old_region.y2 };

                    dfb_region_translate( &region, sawwin->window->config.z, 0 );
                    sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER );
                    dfb_region_translate( &region, -sawwin->window->config.z, 0 );
                    if (stereo_layer) {
                         dfb_region_translate( &region, -sawwin->window->config.z, 0 );
                         sawman_update_window( sawman, sawwin, &region, 0, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                         dfb_region_translate( &region, sawwin->window->config.z, 0 );
                    }
               }
          }
          else {
               dfb_region_translate( &old_region, sawwin->window->config.z, 0 );
               sawman_update_window( sawman, sawwin, &old_region, 0, SWMUF_UPDATE_BORDER );
               dfb_region_translate( &old_region, -sawwin->window->config.z, 0 );
               if (stereo_layer) {
                    dfb_region_translate( &old_region, -sawwin->window->config.z, 0 );
                    sawman_update_window( sawman, sawwin, &old_region, 0, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                    dfb_region_translate( &old_region, sawwin->window->config.z, 0 );
               }
          }
     }

     /* Send new position and size */
     evt.type = DWET_POSITION_SIZE;
     evt.x    = bounds->x;
     evt.y    = bounds->y;
     evt.w    = bounds->w;
     evt.h    = bounds->h;

     sawman_post_event( sawman, sawwin, &evt );

     return DFB_OK;
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

     ret = sawman_switch_focus( sawman, sawwin, SWMCFR_APPLICATION );
     if (ret)
          return ret;

     entered = sawman->entered_window;

     if (entered && entered != sawwin) {
          DFBWindowEvent we;

          D_MAGIC_ASSERT( entered, SaWManWindow );
          D_ASSERT( entered->window != NULL );

          we.type = DWET_LEAVE;

          sawman_window_get_cursor_position( sawman, stack, entered, &we.x, &we.y, &we.cx, &we.cy );

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

     if (sawman->focused_window && sawman->focused_window->window->config.cursor_flags & DWCF_EXPLICIT)
          sawwin = sawman->focused_window;
     else if (sawman->pointer_window)
          sawwin = sawman->pointer_window;
     else {
          sawwin = sawman->entered_window;
     }

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

     if (sawman->focused_window && sawman->focused_window->window->config.cursor_flags & DWCF_EXPLICIT)
          sawwin = sawman->focused_window;
     else if (sawman->pointer_window)
          sawwin = sawman->pointer_window;
     else {
          sawwin = sawman->entered_window;
     }

     if (sawwin)
          send_button_event( sawman, sawwin, stack, event );

     return DFB_OK;
}

static void
post_motion_event( SaWMan              *sawman,
                   SaWManWindow        *sawwin,
                   int                  x,   // SaWMan coordinates
                   int                  y,
                   DFBWindowEventFlags  flags,
                   DFBInputDeviceID     device_id )
{
     SaWManTier     *tier;
     DFBWindowEvent  we;
     int             cx, cy;
     int             sx, sy;

     D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %d,%d, 0x%04x )\n", __FUNCTION__, sawman, sawwin, x, y, flags );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( sawwin->window != NULL );

     /* Retrieve corresponding SaWManTier. */
     tier = sawman_tier_by_class( sawman, sawwin->window->config.stacking );
     D_MAGIC_ASSERT( tier, SaWManTier );

     D_DEBUG_AT( SaWMan_WM, "  -> Tier with resolution %dx%d\n", tier->size.w, tier->size.h );

     /* Convert to Tier coordinates */
     cx = (s64) x * (s64) tier->size.w / (s64) sawman->resolution.w;
     cy = (s64) y * (s64) tier->size.h / (s64) sawman->resolution.h;

     D_DEBUG_AT( SaWMan_WM, "  => %d, %d\n", cx, cy );

     /* Subtract offset of Window within layout (tier coordinates) */
     if (!(flags & DWEF_RELATIVE)) {
          D_DEBUG_AT( SaWMan_WM, "  -> Window at offset %d,%d\n", sawwin->dst.x, sawwin->dst.y );

          x = cx - sawwin->dst.x;
          y = cy - sawwin->dst.y;

          D_DEBUG_AT( SaWMan_WM, "  => %d, %d\n", x, y );
     }
     else {
          x = cx;
          y = cy;
     }

     sx = sawwin->window->config.cursor_resolution.w ?: sawwin->src.w;
     sy = sawwin->window->config.cursor_resolution.h ?: sawwin->src.h;

     D_DEBUG_AT( SaWMan_WM, "  -> Window with scaling %dx%d -> %dx%d\n", sawwin->dst.w, sawwin->dst.h, sx, sy );

     /* Convert to Window coordinates */
     x = x * sx / sawwin->dst.w;
     y = y * sy / sawwin->dst.h;

     D_DEBUG_AT( SaWMan_WM, "  => %d, %d\n", x, y );

     /* Fill event structure */
     we.type       = DWET_MOTION;
     we.flags      = flags;
     we.x          = x;
     we.y          = y;
     we.cx         = cx;
     we.cy         = cy;

     we.flags     |= DWEF_DEVICE_ID;
     we.device_id  = device_id;

     sawman_post_event( sawman, sawwin, &we );
}

static void
handle_motion( CoreWindowStack  *stack,
               SaWMan           *sawman,
               int               dx,
               int               dy,
               DFBInputDeviceID  device_id )
{
     SaWManWindow         *sawwin = NULL;
     DFBWindowCursorFlags  flags  = DWCF_NONE;
     int                   cx, cy;

     D_ASSERT( stack != NULL );
     D_MAGIC_ASSERT( sawman, SaWMan );

     D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %d,%d )\n", __FUNCTION__, stack, sawman, dx, dy );
     D_DEBUG_AT( SaWMan_WM, "  -> SaWMan with resolution %dx%d\n", sawman->resolution.w, sawman->resolution.h );

     if (!stack->cursor.enabled)
          return;


     cx = stack->cursor.x + dx;
     cy = stack->cursor.y + dy;


     if (sawman->focused_window && sawman->focused_window->window->config.cursor_flags & DWCF_EXPLICIT)
          sawwin = sawman->focused_window;
     else if (sawman->pointer_window)
          sawwin = sawman->pointer_window;
     else {
          sawman_update_focus( sawman, stack, cx, cy );

          sawwin = sawman->entered_window;
     }

     if (sawwin)
          flags = sawwin->window->config.cursor_flags;

     if (flags & DWCF_TRAPPED) {
          SaWManTier *tier;

          int x1 = sawwin->dst.x;
          int y1 = sawwin->dst.y;
          int x2 = sawwin->dst.x + sawwin->dst.w;
          int y2 = sawwin->dst.y + sawwin->dst.h;

          /* Retrieve corresponding SaWManTier. */
          tier = sawman_tier_by_class( sawman, sawwin->window->config.stacking );
          D_MAGIC_ASSERT( tier, SaWManTier );

          D_DEBUG_AT( SaWMan_WM, "  -> Tier with resolution %dx%d\n", tier->size.w, tier->size.h );

          D_DEBUG_AT( SaWMan_WM, "  -> Trapping in %d,%d-%dx%d\n", x1, y1, x2 - x1, y2 - y1 );

          x1 = (s64) x1 * (s64) sawman->resolution.w / (s64) tier->size.w;
          y1 = (s64) y1 * (s64) sawman->resolution.h / (s64) tier->size.h;
          x2 = (s64) x2 * (s64) sawman->resolution.w / (s64) tier->size.w;
          y2 = (s64) y2 * (s64) sawman->resolution.h / (s64) tier->size.h;

          D_DEBUG_AT( SaWMan_WM, "  -> Scaled to %d,%d-%dx%d\n", x1, y1, x2 - x1, y2 - y1 );

          if (cx < x1)
               cx = x1;
          else if (cx > x2)
               cx = x2;

          if (cy < y1)
               cy = y1;
          else if (cy > y2)
               cy = y2;

          D_DEBUG_AT( SaWMan_WM, "  -> Cursor trapped at %d,%d\n", cx, cy );
     }

     if (flags & DWCF_UNCLIPPED) {
          D_DEBUG_AT( SaWMan_WM, "  -> Cursor unclipped at %d,%d\n", cx, cy );
     }
     else {
          if (cx < 0)
               cx = 0;
          else if (cx >= sawman->resolution.w)
               cx = sawman->resolution.w - 1;

          if (cy < 0)
               cy = 0;
          else if (cy >= sawman->resolution.h)
               cy = sawman->resolution.h - 1;

          D_DEBUG_AT( SaWMan_WM, "  -> Cursor clipped at %d,%d\n", cx, cy );
     }


     if (flags & DWCF_FIXED)
          D_DEBUG_AT( SaWMan_WM, "  -> Cursor is fixed at %d,%d\n", stack->cursor.x, stack->cursor.y );
     else if (cx != stack->cursor.x || cy != stack->cursor.y) {
          D_DEBUG_AT( SaWMan_WM, "  -> Cursor moved %d,%d -> %d,%d\n", stack->cursor.x, stack->cursor.y, cx, cy );

          stack->cursor.x = cx;
          stack->cursor.y = cy;

          dfb_wm_update_cursor( stack, CCUF_POSITION );
     }


     if (sawwin) {
          if (flags & DWCF_RELATIVE)
               post_motion_event( sawman, sawwin, dx, dy, DWEF_RELATIVE, device_id );
          else
               post_motion_event( sawman, sawwin, cx, cy, DWEF_NONE, device_id );
     }
}

static void
handle_wheel( CoreWindowStack  *stack,
              SaWMan           *sawman,
              int               dz,
              DFBInputDeviceID  device_id )
{
     DFBWindowEvent  we;
     SaWManWindow   *sawwin;

     D_ASSERT( stack != NULL );
     D_MAGIC_ASSERT( sawman, SaWMan );

     if (!stack->cursor.enabled)
          return;

     if (sawman->focused_window && sawman->focused_window->window->config.cursor_flags & DWCF_EXPLICIT)
          sawwin = sawman->focused_window;
     else if (sawman->pointer_window)
          sawwin = sawman->pointer_window;
     else {
          sawwin = sawman->entered_window;
     }

     if (sawwin) {
          D_MAGIC_ASSERT( sawwin, SaWManWindow );
          D_ASSERT( sawwin->window != NULL );

          we.type       = DWET_WHEEL;
          we.step       = dz;

          we.flags     |= DWEF_DEVICE_ID;
          we.device_id  = device_id;

          sawman_window_get_cursor_position( sawman, stack, sawwin, &we.x, &we.y, &we.cx, &we.cy );

          sawman_post_event( sawman, sawwin, &we );
     }
}

static DFBResult
handle_axis_motion( CoreWindowStack     *stack,
                    SaWMan              *sawman,
                    SaWManTier          *tier,
                    const DFBInputEvent *event )
{
     D_DEBUG_AT( SaWMan_WM, "%s( %p, %p )\n", __FUNCTION__, stack, sawman );

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
                    sawman->cursor.dx += rel;
                    break;

               case DIAI_Y:
                    sawman->cursor.dy += rel;
                    break;

               case DIAI_Z:
                    handle_wheel( stack, sawman, - event->axisrel, event->device_id );
                    break;

               default:
                    ;
          }
     }
     else if (event->flags & DIEF_AXISABS) {
          int axismin = 0;
          int axisabs = event->axisabs;

          if (event->flags & DIEF_MIN) {
               axismin = event->min;

               axisabs -= axismin;
          }

          switch (event->axis) {
               case DIAI_X:
                    if (event->flags & DIEF_MAX)
                         axisabs = (s64) axisabs * (s64) sawman->resolution.w / (s64) (event->max - axismin + 1);

                    sawman->cursor.dx = axisabs - stack->cursor.x;
                    break;

               case DIAI_Y:
                    if (event->flags & DIEF_MAX)
                         axisabs = (s64) axisabs * (s64) sawman->resolution.h / (s64) (event->max - axismin + 1);

                    sawman->cursor.dy = axisabs - stack->cursor.y;
                    break;

               default:
                    ;
          }
     }

     if (!(event->flags & DIEF_FOLLOW) && (sawman->cursor.dx || sawman->cursor.dy)) {
          handle_motion( stack, sawman, sawman->cursor.dx, sawman->cursor.dy, event->device_id );

          sawman->cursor.dx = 0;
          sawman->cursor.dy = 0;
     }

     return DFB_OK;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static DFBResult
local_init( WMData  *wmdata,
            SaWMan  *sawman,
            CoreDFB *core )
{
     wmdata->core   = core;
     wmdata->world  = dfb_core_world( core );
     wmdata->sawman = sawman;

     sawman_config_init( NULL, NULL );

     return DFB_OK;
}

static void
local_deinit( WMData *wmdata )
{
     D_ASSERT( wmdata->refs == 0 );
}

static DFBResult
local_ref( WMData *wmdata )
{
     D_DEBUG_AT( SaWMan_WM, "%s( %p )\n", __FUNCTION__, wmdata );

     fusion_skirmish_prevail( &wmdata->update_skirmish );

     if (!wmdata->refs) {
          DFBResult ret;

          /* Initialise the graphics state used for rendering */
          dfb_state_init( &wmdata->state, core_dfb );

          /* Create a client to use the task manager if enabled */
          ret = CoreGraphicsStateClient_Init( &wmdata->client, &wmdata->state );
          if (ret) {
               dfb_state_destroy( &wmdata->state );
               fusion_skirmish_dismiss( &wmdata->update_skirmish );
               return ret;
          }
     }

     wmdata->refs++;

     D_DEBUG_AT( SaWMan_WM, "  -> %d refs\n", wmdata->refs );

     fusion_skirmish_dismiss( &wmdata->update_skirmish );

     return DFB_OK;
}

static void
local_unref( WMData *wmdata )
{
     D_DEBUG_AT( SaWMan_WM, "%s( %p )\n", __FUNCTION__, wmdata );

     D_ASSERT( wmdata->refs > 0 );

     fusion_skirmish_prevail( &wmdata->update_skirmish );

     if (!--wmdata->refs) {
          CoreGraphicsStateClient_Deinit( &wmdata->client );

          dfb_state_destroy( &wmdata->state );
     }

     D_DEBUG_AT( SaWMan_WM, "  -> %d refs\n", wmdata->refs );

     fusion_skirmish_dismiss( &wmdata->update_skirmish );
}

/**************************************************************************************************/

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
     DFBResult  ret;
     WMData    *data   = wm_data;
     SaWMan    *sawman = shared_data;

     fusion_skirmish_init2( &data->update_skirmish, "WM/Update", dfb_core_world(core), fusion_config->secure_fusion );

     ret = local_init( data, sawman, core );
     if (ret)
          return ret;

     return sawman_initialize( sawman, data->world, &data->process );
}

static DFBResult
wm_join( CoreDFB *core, void *wm_data, void *shared_data )
{
     DFBResult  ret;
     WMData    *data   = wm_data;
     SaWMan    *sawman = shared_data;

     ret = local_init( data, sawman, core );
     if (ret)
          return ret;

     return sawman_join( sawman, data->world, &data->process );
}

static DFBResult
wm_shutdown( bool emergency, void *wm_data, void *shared_data )
{
     WMData *data   = wm_data;
     SaWMan *sawman = shared_data;

     local_deinit( data );

     fusion_skirmish_destroy( &data->update_skirmish );

     return sawman_shutdown( sawman, data->world );
}

static DFBResult
wm_leave( bool emergency, void *wm_data, void *shared_data )
{
     WMData *data   = wm_data;
     SaWMan *sawman = shared_data;

     local_deinit( data );

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
     WMData     *wmdata = wm_data;
     SaWMan     *sawman = shared_data;
     SaWManTier *tier;

     D_ASSERT( wm_data != NULL );
     D_MAGIC_ASSERT( sawman, SaWMan );

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     sawman_post_init( sawman, wmdata->world );

     direct_list_foreach (tier, sawman->tiers) {
          const SaWManBorderInit *border;

          D_MAGIC_ASSERT( tier, SaWManTier );

          ret = dfb_layer_context_get_configuration( tier->context, &tier->config );
          if (ret)
               D_DERROR( ret, "SaWMan/PostInit: Could not get configuration of layer context!\n" );

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

     sawman_process_updates( sawman, DSFLIP_NONE, wmdata );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

/**********************************************************************************************************************/

static ReactionResult
sawman_surface_reaction( const void *msg_data,
                         void       *ctx )
{
     DFBResult                      ret;
     int                            i;
     const CoreSurfaceNotification *notification = msg_data;
     SaWManTier                    *tier         = ctx;
     SaWMan                        *sawman;
     WMData                        *wmdata;

     D_DEBUG_AT( SaWMan_Surface, "%s( %p, %p )\n", __FUNCTION__, msg_data, ctx );

     D_MAGIC_ASSERT( tier, SaWManTier );

     sawman = tier->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     wmdata = dfb_wm_get_data();
     D_ASSERT( wmdata != NULL );

     if (notification->flags & CSNF_DISPLAY) {
          D_DEBUG_AT( SaWMan_Surface, "  -> DISPLAY [%d]\n", notification->index );

          switch (tier->region->config.buffermode) {
               case DLBM_TRIPLE:
                    /* Lock SaWMan. */
                    ret = sawman_lock( sawman );
                    if (ret) {
                         D_DERROR( ret, "SaWMan/SurfaceReaction: Could not lock SaWMan!\n" );
                         return RS_OK;
                    }

                    local_ref( wmdata );

                    fusion_skirmish_prevail( &wmdata->update_skirmish );

                    D_ASSUME( tier->left.updated.num_regions > 0 || tier->right.updated.num_regions > 0 );

                    if (tier->left.updated.num_regions || tier->right.updated.num_regions) {
                         switch (tier->region->config.buffermode) {
                              case DLBM_TRIPLE:
                                   if (tier->region->config.options & DLOP_STEREO) {
                                        /* Copy back the updated region. */
                                        if (tier->left.updated.num_regions) {
                                             D_DEBUG_AT( SaWMan_Surface, "  -> copying %d updated regions (F->I) (left)\n", tier->left.updated.num_regions );

                                             for (i=0; i<tier->left.updated.num_regions; i++) {
                                                  D_DEBUG_AT( SaWMan_Surface, "    -> %4d,%4d - %4dx%4d  [%d]\n",
                                                              DFB_RECTANGLE_VALS_FROM_REGION( &tier->left.updated.regions[i] ), i );
                                             }

                                             dfb_gfx_copy_regions_client( tier->surface, CSBR_FRONT, DSSE_LEFT,
                                                                          tier->surface, CSBR_IDLE, DSSE_LEFT,
                                                                          tier->left.updated.regions, tier->left.updated.num_regions, 0, 0, &wmdata->client );
                                        }

                                        /* Copy back the updated region. */
                                        if (tier->right.updated.num_regions) {
                                             D_DEBUG_AT( SaWMan_Surface, "  -> copying %d updated regions (F->I) (right)\n", tier->right.updated.num_regions );

                                             for (i=0; i<tier->right.updated.num_regions; i++) {
                                                  D_DEBUG_AT( SaWMan_Surface, "    -> %4d,%4d - %4dx%4d  [%d]\n",
                                                              DFB_RECTANGLE_VALS_FROM_REGION( &tier->right.updated.regions[i] ), i );
                                             }

                                             dfb_gfx_copy_regions_client( tier->surface, CSBR_FRONT, DSSE_RIGHT,
                                                                          tier->surface, CSBR_IDLE, DSSE_RIGHT,
                                                                          tier->right.updated.regions, tier->right.updated.num_regions, 0, 0, &wmdata->client );
                                        }
                                   }
                                   else {
                                        /* Copy back the updated region. */
                                        if (tier->left.updated.num_regions) {
                                             D_DEBUG_AT( SaWMan_Surface, "  -> copying %d updated regions (F->I) (left)\n", tier->left.updated.num_regions );
     
                                             for (i=0; i<tier->left.updated.num_regions; i++) {
                                                  D_DEBUG_AT( SaWMan_Surface, "    -> %4d,%4d - %4dx%4d  [%d]\n",
                                                              DFB_RECTANGLE_VALS_FROM_REGION( &tier->left.updated.regions[i] ), i );
                                             }
     
                                             dfb_gfx_copy_regions_client( tier->surface, CSBR_FRONT, DSSE_LEFT, tier->surface, CSBR_IDLE, DSSE_LEFT,
                                                                          tier->left.updated.regions, tier->left.updated.num_regions, 0, 0, &wmdata->client );
                                        }
                                   }
                                   break;

                              default:
                                   break;
                         }

                         dfb_updates_reset( &tier->left.updated );
                         dfb_updates_reset( &tier->right.updated );
                    }

                    if (tier->left.updating.num_regions || tier->right.updating.num_regions) {
                         D_DEBUG_AT( SaWMan_Surface, "  -> flushing updating regions\n" );

                         sawman_flush_updating( sawman, tier, wmdata );
                    }

                    CoreGraphicsStateClient_Flush( &wmdata->client, 0, CGSCFF_NONE );

                    fusion_skirmish_dismiss( &wmdata->update_skirmish );

                    local_unref( wmdata );

                    /* Unlock SaWMan. */
                    sawman_unlock( sawman );
                    break;

               default:
                    break;
          }
     }

     return RS_OK;
}

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

     if (!sawman->lock)
          sawman->lock = &stack->context->lock;

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     if (!sawman->stack)
          sawman->stack = stack;

     data->stack  = stack;
     data->sawman = sawman;

     D_MAGIC_SET( data, StackData );

     if (!sawman_tier_by_layer( sawman, context->layer_id, &tier ) || tier->stack) {
          sawman_unlock( sawman );
          return DFB_OK;
     }

     D_INFO( "SaWMan: Initializing stack %p for tier %p, %dx%d, layer %d, context %p [%d]...\n",
             stack, tier, stack->width, stack->height, context->layer_id, context, context->object.ref.multi.id );

     tier->sawman  = sawman;
     tier->stack   = stack;
     tier->context = context;
     tier->size.w  = stack->width;
     tier->size.h  = stack->height;

     ret = dfb_layer_context_get_primary_region( context, true, &tier->region );
     if (ret) {
          sawman_unlock( sawman );
          D_MAGIC_CLEAR( data );
          return ret;
     }

     ret = dfb_layer_region_get_surface( tier->region, &tier->surface );
     if (ret) {
          dfb_layer_region_unref( tier->region );
          sawman_unlock( sawman );
          D_MAGIC_CLEAR( data );
          return ret;
     }

     dfb_layer_region_globalize( tier->region );
     dfb_surface_globalize( tier->surface );

     dfb_surface_attach( tier->surface, sawman_surface_reaction, tier, &tier->surface_reaction );

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

     D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %p )\n", __FUNCTION__, stack, wm_data, stack_data );

     sawman = data->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     if (!sawman->lock)
          return DFB_OK;

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_OK;
     }

     D_ASSERT( tier->context != NULL );

     dfb_surface_detach( tier->surface, &tier->surface_reaction );

     tier->stack   = NULL;
     tier->context = NULL;
     tier->size.w  = 0;
     tier->size.h  = 0;

     dfb_layer_region_unlink( &tier->region );

     dfb_surface_unlink( &tier->surface );

     /* Destroy backing store of software cursor. */
     if (tier->cursor_bs)
          dfb_surface_unlink( &tier->cursor_bs );

     if (tier->cursor_bs_right)
          dfb_surface_unlink( &tier->cursor_bs_right );

     direct_list_remove( &sawman->tiers, &tier->link );
     D_MAGIC_CLEAR( tier );
     SHFREE( sawman->shmpool, tier );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     D_MAGIC_CLEAR( data );

     if (sawman->lock == &stack->context->lock)
          sawman->lock = NULL;

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

     if (!sawman->lock)
          return DFB_OK;

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

     if (active) {
          local_ref( wm_data );

          return dfb_windowstack_repaint_all( stack );
     }
     else {
          local_unref( wm_data );

          tier->active        = false;
          tier->single_mode   = false;
          tier->single_window = NULL;
     }

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

     sawman_call( sawman, SWMCID_STACK_RESIZED, &tier->size, sizeof(tier->size), false );

     if (!tier->border_only)
          tier->config = tier->context->config;

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

     D_DEBUG_AT( SaWMan_WM, "%s( device %d, type 0x%08x, flags 0x%08x )...\n",
                 __FUNCTION__, event->device_id, event->type, event->flags );

     if (stack->context->layer_id != DLID_PRIMARY)
          return DFB_OK;

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Retrieve corresponding SaWManTier. */
     if (sawman->cursor.context && stack == sawman->cursor.context->stack) {
          direct_list_foreach (tier, sawman->tiers) {
               if (tier->layer_id == sawman->cursor.context->layer_id)
                    break;
          }

          if (!tier) {
               D_DEBUG_AT( SaWMan_WM, "  -> cursor tier not found!\n" );
               sawman_unlock( sawman );
               return DFB_OK;
          }

          stack = tier->stack;
     }
     else if (!sawman_tier_by_stack( sawman, stack, &tier ) || tier != (SaWManTier*)sawman->tiers) {
          D_DEBUG_AT( SaWMan_WM, "  -> not for me\n" );
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
     ret = sawman_call( sawman, SWMCID_INPUT_FILTER, &sawman->manager.event, sizeof(sawman->manager.event), true );

     sawman_process_updates( sawman, DSFLIP_NONE, wm_data );

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
               we.flags      = 0;
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

     sawwin = sawman_window_at_pointer( data->sawman, stack, x, y );

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

     SaWManWindowInfo *info;

     D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %p, %p, %p )\n", __FUNCTION__,
                 stack, wm_data, stack_data, window, window_data );

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

     /* Override with our own ID which is unique among all tiers, not only each layer context. */
     window->id = ++sawman->window_ids;

     /* Lookup parent window. */
     if (window->config.association) {
          SaWManWindow *parent = NULL;

          D_DEBUG_AT( SaWMan_WM, "  -> parent win id %u\n", window->config.association );

          direct_list_foreach (parent, sawman->windows) {
               D_MAGIC_ASSERT( parent, SaWManWindow );
               D_ASSERT( parent->window != NULL );

               if (parent->id == window->config.association)
                    break;
          }

          if (!parent) {
               D_ERROR( "SaWMan/WM: Can't find parent window with ID %d!\n", window->config.association );
               sawman_unlock( sawman );
               return DFB_IDNOTFOUND;
          }

#ifndef OLD_COREWINDOWS_STRUCTURE
          if (parent->window->toplevel != window->toplevel) {
               D_ERROR( "SaWMan/WM: Can't associate windows with different toplevel!\n" );
               sawman_unlock( sawman );
               return DFB_INVARG;
          }
#endif

          D_DEBUG_AT( SaWMan_WM, "  -> parent window %p\n", parent );

          sawwin->parent = parent;
     }

     info = &sawman->callback.preconfig;
     SAWMANWINDOWCONFIG_COPY( &info->config, &window->config );

     switch (ret = sawman_call( sawman, SWMCID_WINDOW_PRECONFIG, &info->config, sizeof(info->config), true )) {
          case DFB_OK:
               break;

          case DFB_NOIMPL:
               ret = DFB_OK;
               break;

          default:
               break;
     }

     SAWMANWINDOWCONFIG_COPY( &window->config, &info->config );

     sawman_process_updates( wmdata->sawman, DSFLIP_NONE, wmdata );

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
     DFBResult      ret;
     WMData        *wmdata = wm_data;
     SaWManWindow  *sawwin = window_data;
     StackData     *sdata  = stack_data;
     SaWMan        *sawman;
     SaWManTier    *tier;
     SaWManProcess *process;
     FusionID       identity;

     SaWManWindowInfo *info;

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

     identity = Core_GetIdentity();
     if (identity) {
          direct_list_foreach (process, sawman->processes) {
               if (process->fusion_id == identity)
                    break;
          }
     }
     else
          process = wmdata->process;

     if (!process) {
          D_ERROR( "SaWMan/WM: Could not find SaWManProcess of calling Fusionee!\n" );
          sawman_unlock( sawman );
          return DFB_ITEMNOTFOUND;
     }

     /* Initialize window data. */
     sawwin->sawman     = sawman;
     sawwin->shmpool    = sawman->shmpool;
     sawwin->id         = window->id;
     sawwin->caps       = window->caps;
     sawwin->window     = window;
     sawwin->stack      = stack;
     sawwin->stack_data = stack_data;

     sawwin->process.pid       = process->pid;
     sawwin->process.fusion_id = process->fusion_id;
     sawwin->process.flags     = process->flags;

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

     dfb_updates_init( &sawwin->left.visible, sawwin->left.visible_regions, D_ARRAY_SIZE(sawwin->left.visible_regions) );
     dfb_updates_init( &sawwin->left.updates, sawwin->left.updates_regions, D_ARRAY_SIZE(sawwin->left.updates_regions) );

     dfb_updates_init( &sawwin->right.visible, sawwin->right.visible_regions, D_ARRAY_SIZE(sawwin->right.visible_regions) );
     dfb_updates_init( &sawwin->right.updates, sawwin->right.updates_regions, D_ARRAY_SIZE(sawwin->right.updates_regions) );

     fusion_vector_init( &sawwin->children, 2, sawwin->shmpool );

     sawwin->priority   = sawman_window_priority( sawwin );

     direct_list_append( &sawman->windows, &sawwin->link );

     /* Send notification to windows watchers */
     dfb_wm_dispatch_WindowAdd( wmdata->core, window );

     info = &sawman->callback.info;
     info->handle      = (SaWManWindowHandle)sawwin;
     info->caps        = sawwin->caps;
     SAWMANWINDOWCONFIG_COPY( &info->config, &window->config );
     info->config.key_selection = window->config.key_selection;
     info->config.keys          = window->config.keys;
     info->config.num_keys      = window->config.num_keys;
     info->resource_id          = window->resource_id;
     info->application_id       = window->config.application_id;
     info->win_id               = window->id;
     info->flags = sawwin->flags
                   | (window->flags & CWF_FOCUSED ? SWMWF_FOCUSED : 0)
                   | (window->flags & CWF_ENTERED ? SWMWF_ENTERED : 0);

     if (sawman->manager.fusion_id)
          fusion_object_add_owner( &window->object, sawman->manager.fusion_id );

     switch (ret = sawman_call( sawman, SWMCID_WINDOW_ADDED, info, sizeof(*info), true )) {
          case DFB_OK:
               break;

          case DFB_NOIMPL:
               /* Actually add the window to the stack. */
               ret = sawman_insert_window( sawman, sawwin, NULL, true );
               break;

          default:
               direct_list_remove( &sawman->windows, &sawwin->link );
               sawman_unlock( sawman );
               return ret;
     }

     if (ret == DFB_OK && sawwin->parent && !sawwin->parent_window) {
          SaWManWindow *parent = sawwin->parent;

          D_MAGIC_ASSERT( parent, SaWManWindow );
          D_ASSERT( parent->window != NULL );
          D_ASSERT( parent->id == window->config.association );

          sawwin->parent_window = parent->window;

          ret = fusion_vector_add( &parent->children, sawwin );
          if (ret) {
               sawwin->parent_window = NULL;
               sawman_unlock( sawman );
               return ret;
          }
     }

     // FIXME: ok to simply reconfigure?
     if ((window->caps & (DWCAPS_LR_MONO | DWCAPS_STEREO)) &&
         !(tier->config.options & DLOP_STEREO)) {
          DFBDisplayLayerConfig *config = &tier->config;

          config->flags            |= DLCONF_OPTIONS | DLCONF_SURFACE_CAPS;
          config->options          |= DLOP_STEREO;
          config->surface_caps     |= DSCAPS_STEREO;
          dfb_layer_context_set_configuration( tier->context, config );
     }

     sawman_update_geometry( sawwin );

     sawman_process_updates( wmdata->sawman, DSFLIP_NONE, wmdata );

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
     int           i;
     WMData       *wmdata = wm_data;
     SaWManWindow *sawwin = window_data;
     SaWManWindow *child;
     StackData    *sdata  = stack_data;
     SaWMan       *sawman;
     SaWManTier   *tier;
     int           n;
     bool          mono_only;

     SaWManWindowInfo *info;

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

     info = &sawman->callback.info;
     info->handle      = (SaWManWindowHandle)sawwin;
     info->caps        = sawwin->caps;
     SAWMANWINDOWCONFIG_COPY( &info->config, &window->config );
     info->config.key_selection = window->config.key_selection;
     info->config.keys          = window->config.keys;
     info->config.num_keys      = window->config.num_keys;
     info->resource_id          = window->resource_id;
     info->application_id       = window->config.application_id;
     info->win_id               = window->id;
     info->flags = sawwin->flags
                   | (window->flags & CWF_FOCUSED ? SWMWF_FOCUSED : 0)
                   | (window->flags & CWF_ENTERED ? SWMWF_ENTERED : 0);

     switch (ret = sawman_call( sawman, SWMCID_WINDOW_REMOVED, info, sizeof(*info), true )) {
          case DFB_NOIMPL:
               ret = DFB_OK;

          case DFB_OK:
               fusion_vector_foreach (child, n, sawwin->children) {
                    child->parent        = NULL;
                    child->parent_window = NULL;
               }

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

                    sawwin->parent_window = NULL;
               }

               D_MAGIC_CLEAR( sawwin );
               break;

          default:
               direct_list_append( &sawman->windows, &sawwin->link );
               break;
     }

     /* Finally, Release all implicit key grabs here, but keep the keys muted until UP event. */
     if (sawman_config->keep_implicit_key_grabs) {
          for (i=0; i<SAWMAN_MAX_IMPLICIT_KEYGRABS; i++) {
               if (sawman->keys[i].code != -1 && sawman->keys[i].owner == sawwin) {
                    //sawman->keys[i].code  = -1;
                    sawman->keys[i].owner = NULL;
               }
          }
     }

     if (tier->single_window == sawwin)
          tier->single_window = NULL;

     dfb_updates_deinit( &sawwin->left.visible );
     dfb_updates_deinit( &sawwin->left.updates );

     dfb_updates_deinit( &sawwin->right.visible );
     dfb_updates_deinit( &sawwin->right.updates );

     // caution: sawwin changed after this

     // FIXME: ok to simply reconfigure?
     mono_only = true;
     fusion_vector_foreach_reverse (sawwin, n, sawman->layout) {
          CoreWindow *window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          if (SAWMAN_VISIBLE_WINDOW(window) && !(window->caps & DWCAPS_INPUTONLY) &&
              (window->caps & (DWCAPS_LR_MONO | DWCAPS_STEREO)))
               mono_only = false;
     }
     if (mono_only && (tier->config.options & DLOP_STEREO)) {
          DFBDisplayLayerConfig *config = &tier->config;

          config->flags            |= DLCONF_OPTIONS | DLCONF_SURFACE_CAPS;
          config->options          &= ~DLOP_STEREO;
          config->surface_caps     &= ~DSCAPS_STEREO;
          dfb_layer_context_set_configuration( tier->context, config );
     }

     /* Send notification to windows watchers */
     dfb_wm_dispatch_WindowRemove( wmdata->core, window );

     sawman_process_updates( sdata->sawman, DSFLIP_NONE, wmdata );

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
     CoreWindowStack  *stack;

     SaWManWindowInfo     *info;
     SaWManWindowConfig   *config;
     SaWManWindowConfig   *current;
     SaWManWindowReconfig *reconfig;

     DFBInputDeviceKeySymbol *shared_keys = 0;

     D_DEBUG_AT( SaWMan_WM, "%s( window %p, flags 0x%08x )\n", __FUNCTION__, window, flags );

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

     /* inform about an application ID change */
     if (flags & CWCF_APPLICATION_ID) {
          info = &sawman->callback.info;
          info->handle      = (SaWManWindowHandle)sawwin;
          info->caps        = sawwin->caps;
          SAWMANWINDOWCONFIG_COPY( &info->config, &window->config );
          info->config.key_selection = window->config.key_selection;
          info->config.keys          = window->config.keys;
          info->config.num_keys      = window->config.num_keys;
          info->resource_id          = window->resource_id;
          info->application_id       = updated->application_id;
          info->win_id               = window->id;
          info->flags = sawwin->flags
                        | (window->flags & CWF_FOCUSED ? SWMWF_FOCUSED : 0)
                        | (window->flags & CWF_ENTERED ? SWMWF_ENTERED : 0);

          ret = sawman_call( sawman, SWMCID_APPLICATION_ID_CHANGED, info, sizeof(*info), true );
          if (ret == DFB_NOIMPL)
               ret = DFB_OK;

          if (ret != DFB_OK) {
               sawman_unlock( sawman );
               return ret;
          }

          /* accepted */
          window->config.application_id = info->application_id;

          /* if no other flags, we are done */
          if (flags == CWCF_APPLICATION_ID) {
               sawman_unlock( sawman );
               return DFB_OK;
          }
     }

     /* Retrieve corresponding SaWManTier. */
     if (!sawman_tier_by_stack( sawman, stack, &tier )) {
          sawman_unlock( sawman );
          return DFB_UNSUPPORTED;
     }

     reconfig = &sawman->callback.reconfig;

     reconfig->caps   = sawwin->caps;
     reconfig->handle = (SaWManWindowHandle)sawwin;

     current  = &reconfig->current;
     config   = &reconfig->request;

     SAWMANWINDOWCONFIG_COPY( current, &window->config );
     *config = *current; /* make sure that all fields in "request" are valid */
     SAWMANWINDOWCONFIG_COPY_IF( config,  updated, flags );

     /* special consideration due to possibility of local pointer in request */
     current->key_selection = window->config.key_selection;
     current->keys          = window->config.keys;
     current->num_keys      = window->config.num_keys;
     if( flags & CWCF_KEY_SELECTION )
     {
          config->key_selection = updated->key_selection;
          config->keys          = 0;
          config->num_keys      = updated->num_keys;

          if( config->key_selection == DWKS_LIST ) {
               unsigned int bytes = sizeof(DFBInputDeviceKeySymbol) * config->num_keys;
               shared_keys = SHMALLOC( sawwin->shmpool, bytes );
               if (!shared_keys) {
                    D_ERROR( "SaWMan/WM: Could not allocate %d bytes for list "
                             "of selected keys (%d)!\n", bytes, config->num_keys );
                    return D_OOSHM();
               }
               config->keys = shared_keys;
               direct_memcpy( config->keys, updated->keys, bytes );
          }
     }

     reconfig->flags =
            (flags & CWCF_POSITION     ? SWMCF_POSITION     : 0)
          | (flags & CWCF_SIZE         ? SWMCF_SIZE         : 0)
          | (flags & CWCF_OPACITY      ? SWMCF_OPACITY      : 0)
          | (flags & CWCF_STACKING     ? SWMCF_STACKING     : 0)
          | (flags & CWCF_OPTIONS      ? SWMCF_OPTIONS      : 0)
          | (flags & CWCF_EVENTS       ? SWMCF_EVENTS       : 0)
          | (flags & CWCF_COLOR        ? SWMCF_COLOR        : 0)
          | (flags & CWCF_COLOR_KEY    ? SWMCF_COLOR_KEY    : 0)
          | (flags & CWCF_OPAQUE       ? SWMCF_OPAQUE       : 0)
          | (flags & CWCF_STEREO_DEPTH ? SWMCF_STEREO_DEPTH : 0)
          | (flags & CWCF_ASSOCIATION  ? SWMCF_ASSOCIATION  : 0)
          | (flags & CWCF_CURSOR_FLAGS ? SWMCF_CURSOR_FLAGS : 0)
          | (flags & CWCF_CURSOR_RESOLUTION ? SWMCF_CURSOR_RESOLUTION : 0)
          | (flags & CWCF_SRC_GEOMETRY ? SWMCF_SRC_GEOMETRY : 0)
          | (flags & CWCF_DST_GEOMETRY ? SWMCF_DST_GEOMETRY : 0)
          | (flags & CWCF_KEY_SELECTION? SWMCF_KEY_SELECTION: 0);

     switch (ret = sawman_call( sawman, SWMCID_WINDOW_RECONFIG, reconfig, sizeof(*reconfig), true )) {
          case DFB_OK: {
               SaWManWindowConfigFlags f = reconfig->flags;
               flags =
                      ( flags &   ~( CWCF_POSITION
                                   | CWCF_SIZE
                                   | CWCF_OPACITY
                                   | CWCF_STACKING
                                   | CWCF_OPTIONS
                                   | CWCF_EVENTS
                                   | CWCF_COLOR
                                   | CWCF_COLOR_KEY
                                   | CWCF_OPAQUE
                                   | CWCF_STEREO_DEPTH
                                   | CWCF_ASSOCIATION
                                   | CWCF_KEY_SELECTION
                                   | CWCF_SRC_GEOMETRY
                                   | CWCF_DST_GEOMETRY ) )
                    | (f & SWMCF_POSITION     ? CWCF_POSITION     : 0)
                    | (f & SWMCF_SIZE         ? CWCF_SIZE         : 0)
                    | (f & SWMCF_OPACITY      ? CWCF_OPACITY      : 0)
                    | (f & SWMCF_STACKING     ? CWCF_STACKING     : 0)
                    | (f & SWMCF_OPTIONS      ? CWCF_OPTIONS      : 0)
                    | (f & SWMCF_EVENTS       ? CWCF_EVENTS       : 0)
                    | (f & SWMCF_COLOR        ? CWCF_COLOR        : 0)
                    | (f & SWMCF_COLOR_KEY    ? CWCF_COLOR_KEY    : 0)
                    | (f & SWMCF_KEY_SELECTION? CWCF_KEY_SELECTION: 0)
                    | (f & SWMCF_OPAQUE       ? CWCF_OPAQUE       : 0)
                    | (f & SWMCF_STEREO_DEPTH ? CWCF_STEREO_DEPTH : 0)
                    | (f & SWMCF_ASSOCIATION  ? CWCF_ASSOCIATION  : 0)
                    | (f & SWMCF_CURSOR_FLAGS ? CWCF_CURSOR_FLAGS : 0)
                    | (f & SWMCF_CURSOR_RESOLUTION ? CWCF_CURSOR_RESOLUTION : 0)
                    | (f & SWMCF_SRC_GEOMETRY ? CWCF_SRC_GEOMETRY : 0)
                    | (f & SWMCF_DST_GEOMETRY ? CWCF_DST_GEOMETRY : 0);
               }
               break;

          case DFB_NOIMPL:
               break;

          default:
               sawman_unlock( sawman );
               return ret;
     }

     if (flags & CWCF_OPTIONS) {
          if ((window->config.options & DWOP_SCALE) && !(config->options & DWOP_SCALE) && window->surface) {
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

               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER | SWMUF_FORCE_COMPLETE );
               if ( tier->region->config.options & DLOP_STEREO ) 
                    sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, 
                         SWMUF_UPDATE_BORDER | SWMUF_FORCE_COMPLETE | SWMUF_RIGHT_EYE );
          }

          if ( (config->options ^ window->config.options) & (DWOP_INPUTONLY | DWOP_STEREO_SIDE_BY_SIDE_HALF) ) {
               if (config->options & DWOP_STEREO_SIDE_BY_SIDE_HALF) {
                    DFBDisplayLayerConfig *config = &tier->config;

                    config->flags            |= DLCONF_OPTIONS | DLCONF_SURFACE_CAPS;
                    config->options          |= DLOP_STEREO;
                    config->surface_caps     |= DSCAPS_STEREO;
                    dfb_layer_context_set_configuration( tier->context, config );
               }

               window->config.options = config->options;
               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE );
          }

          window->config.options = config->options;
     }

     if (flags & CWCF_EVENTS)
          window->config.events = config->events;

     if (flags & CWCF_COLOR) {
          window->config.color = config->color;
          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE );
     }

     if (flags & CWCF_COLOR_KEY)
          window->config.color_key = config->color_key;

     if (flags & CWCF_OPAQUE)
          window->config.opaque = config->opaque;

     if (flags & CWCF_OPACITY && !config->opacity)
          sawman_set_opacity( sawman, sawwin, config->opacity );

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
          sawman_restack_window( sawman, sawwin, sawwin, 0, config->stacking );

     if (flags & CWCF_OPACITY && config->opacity) {
          sawman_set_opacity( sawman, sawwin, config->opacity );

          /* Possibly switch focus to window now under the cursor */
          sawman_update_focus( sawman, stack, stack->cursor.x, stack->cursor.y );
     }

     if (flags & CWCF_STEREO_DEPTH) 
          sawman_set_stereo_depth( sawman, sawwin, config->z );

     if (flags & CWCF_KEY_SELECTION) {
          if (config->key_selection == DWKS_LIST) {
               unsigned int             bytes = sizeof(DFBInputDeviceKeySymbol) * config->num_keys;
               DFBInputDeviceKeySymbol *keys  = config->keys;

               D_ASSERT( config->keys != NULL );
               D_ASSERT( config->num_keys > 0 );

               if( shared_keys != config->keys) {
                    /* buffer difference, MUST reserve new area */

                    /* If old request buffer: must free */
                    if( shared_keys )
                         SHFREE( sawwin->shmpool, shared_keys );

                    keys = SHMALLOC( sawwin->shmpool, bytes );
                    if (!keys) {
                         D_ERROR( "SaWMan/WM: Could not allocate %d bytes for list "
                                  "of selected keys (%d)!\n", bytes, config->num_keys );
                         return D_OOSHM();
                    }

                    direct_memcpy( keys, config->keys, bytes );
               }

               /* sort always, also when buffer was reused */
               qsort( keys, config->num_keys, sizeof(DFBInputDeviceKeySymbol), keys_compare );

               if (window->config.keys)
                    SHFREE( sawwin->shmpool, window->config.keys );

               window->config.keys     = keys;
               window->config.num_keys = config->num_keys;
          }
          else if (window->config.keys) {
               SHFREE( sawwin->shmpool, window->config.keys );

               window->config.keys     = NULL;
               window->config.num_keys = 0;
          }

          window->config.key_selection = config->key_selection;
     }

     if (flags & CWCF_SRC_GEOMETRY)
          window->config.src_geometry = config->src_geometry;

     if (flags & CWCF_DST_GEOMETRY)
          window->config.dst_geometry = config->dst_geometry;

     if (flags & CWCF_ASSOCIATION && window->config.association != config->association) {
          SaWManWindow *parent = sawwin->parent;

          /* Dissociate first */
          if (sawwin->parent_window) {
               int index;

               sawwin->parent_window = NULL;

               index = fusion_vector_index_of( &parent->children, sawwin );
               D_ASSERT( index >= 0 );
               D_ASSERT( index < parent->children.count );

               fusion_vector_remove( &parent->children, index );

               sawwin->parent = NULL;

               window->config.association = 0;
          }



          /* Lookup new parent window. */
          if (config->association) {
               D_DEBUG_AT( SaWMan_WM, "  -> new parent win id %u\n", config->association );

               direct_list_foreach (parent, sawman->windows) {
                    D_MAGIC_ASSERT( parent, SaWManWindow );
                    D_ASSERT( parent->window != NULL );

                    if (parent->id == config->association)
                         break;
               }

               if (!parent) {
                    D_ERROR( "SaWMan/WM: Can't find parent window with ID %d!\n", config->association );
                    sawman_unlock( sawman );
                    return DFB_IDNOTFOUND;
               }

               D_MAGIC_ASSERT( parent, SaWManWindow );
               D_ASSERT( parent->window != NULL );

#ifndef OLD_COREWINDOWS_STRUCTURE
               if (parent->window->toplevel != window->toplevel) {
                    D_ERROR( "SaWMan/WM: Can't associate windows with different toplevel!\n" );
                    sawman_unlock( sawman );
                    return DFB_INVARG;
               }
#endif

               D_DEBUG_AT( SaWMan_WM, "  -> parent window %p\n", parent );

               sawwin->parent_window = parent->window;

               ret = fusion_vector_add( &parent->children, sawwin );
               if (ret) {
                    sawwin->parent_window = NULL;
                    sawman_unlock( sawman );
                    return ret;
               }


               sawwin->parent = parent;

               /* Write back new association */
               window->config.association = config->association;
          }
     }

     /* Update geometry? */
     if (flags & (CWCF_POSITION | CWCF_SIZE | CWCF_SRC_GEOMETRY | CWCF_DST_GEOMETRY | CWCF_ASSOCIATION))
          sawman_update_geometry( sawwin );

     if (flags & (CWCF_POSITION | CWCF_SIZE | CWCF_OPACITY | CWCF_OPTIONS | CWCF_STEREO_DEPTH))
          sawman_update_visible( sawman );

     /* Update cursor flags? */
     if (flags & CWCF_CURSOR_FLAGS)
          sawman_window_set_cursor_flags( sawman, sawwin, config->cursor_flags );

     if (flags & CWCF_CURSOR_RESOLUTION)
          window->config.cursor_resolution = config->cursor_resolution;

     /* Send notification to windows watchers */
     dfb_wm_dispatch_WindowConfig( wmdata->core, window, flags );

     sawman_process_updates( sawman, DSFLIP_NONE, wmdata );

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
     DFBResult          ret;
     SaWMan            *sawman;
     SaWManWindow      *sawwin = window_data;
     CoreWindowStack   *stack;
     StackData         *data;
     SaWManRestackArgs  args;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );

     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     D_ASSERT( relative == NULL || relative_data != NULL );
     D_ASSERT( relative == NULL || relative == window || relation != 0);

     D_DEBUG_AT( SaWMan_WM, "%s( sawwin %p, relative %p, relation %s )\n",
                 __FUNCTION__, sawwin, relative_data, (relation==1) ? "TOP" : "BOTTOM" );

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

     args.handle   = (SaWManWindowHandle)sawwin;
     args.relative = (SaWManWindowHandle)relative_data;
     args.relation = (relation==1) ? SWMWR_TOP : SWMWR_BOTTOM;

     switch (ret = sawman_call( sawman, SWMCID_WINDOW_RESTACK, &args, sizeof(args), false ) ) {
          case DFB_OK:
          case DFB_NOIMPL:
               break;

          default:
               sawman_unlock( sawman );
               return ret;
     }

     ret = sawman_restack_window( sawman, sawwin, relative_data, relation, window->config.stacking );
     if (ret) {
          sawman_unlock( sawman );
          return ret;
     }

     /* Possibly switch focus to window now under the cursor */
     sawman_update_focus( sawman, stack, stack->cursor.x, stack->cursor.y );

     sawman_update_visible( sawman );

     sawman_process_updates( data->sawman, DSFLIP_NONE, wm_data );

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
               else {
                    sawman->unselkeys_window = sawwin;
                    ret = DFB_OK;
               }

               break;

          default:
               D_BUG( "unknown grab target" );
               ret = DFB_BUG;
     }

     sawman_process_updates( wmdata->sawman, DSFLIP_NONE, wmdata );

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

               ret = DFB_OK;
               break;

          default:
               D_BUG( "unknown grab target" );
               ret = DFB_BUG;
     }

     sawman_process_updates( wmdata->sawman, DSFLIP_NONE, wmdata );

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

     sawman_process_updates( wmdata->sawman, DSFLIP_NONE, wmdata );

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return ret;
}

static DFBResult
wm_begin_updates( CoreWindow      *window,
                  void            *wm_data,
                  void            *window_data,
                  const DFBRegion *update )
{
     WMData       *wmdata = wm_data;
     SaWMan       *sawman;
     SaWManWindow *sawwin = window_data;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     D_DEBUG_AT( SaWMan_FlipOnce, "%s( %p ) <- window id %u\n", __FUNCTION__, window, window->id );

     sawman = wmdata->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );

     sawwin->flags |= SWMWF_UPDATING;

     sawwin->update_ms = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC ) / 1000LL;

     return DFB_OK;
}

static DFBResult
wm_set_cursor_position( CoreWindow             *window,
                        void                   *wm_data,
                        void                   *window_data,
                        int                     x,
                        int                     y )
{
     DFBResult        ret;
     WMData          *wmdata = wm_data;
     SaWMan          *sawman;
     SaWManTier      *tier;
     SaWManWindow    *sawwin = window_data;
     CoreWindowStack *stack;
     int              sx, sy;

     D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %p, %d,%d )\n", __FUNCTION__, window, wm_data, window_data, x, y );

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

     sx = sawwin->window->config.cursor_resolution.w ?: sawwin->src.w;
     sy = sawwin->window->config.cursor_resolution.h ?: sawwin->src.h;

     D_DEBUG_AT( SaWMan_WM, "  -> Window with scaling %dx%d -> %dx%d\n", sawwin->dst.w, sawwin->dst.h, sx, sy );

     /* Convert to Tier coordinates */
     x = x * sawwin->dst.w / sx + sawwin->dst.x;
     y = y * sawwin->dst.h / sy + sawwin->dst.y;

     D_DEBUG_AT( SaWMan_WM, "  => %d, %d\n", x, y );

     D_DEBUG_AT( SaWMan_WM, "  -> Tier with resolution %dx%d\n", tier->size.w, tier->size.h );

     /* Convert to SaWMan coordinates */
     x = (s64) x * (s64) sawman->resolution.w / (s64) tier->size.w;
     y = (s64) y * (s64) sawman->resolution.h / (s64) tier->size.h;

     D_DEBUG_AT( SaWMan_WM, "  => %d, %d\n", x, y );

     if (x < 0)
          x = 0;
     else if (x >= sawman->resolution.w)
          x = sawman->resolution.w - 1;

     if (y < 0)
          y = 0;
     else if (y >= sawman->resolution.h)
          y = sawman->resolution.h - 1;

     D_DEBUG_AT( SaWMan_WM, "  -> Cursor clipped at %d,%d\n", x, y );

     if (x != stack->cursor.x || y != stack->cursor.y) {
          D_DEBUG_AT( SaWMan_WM, "  -> Cursor moved %d,%d -> %d,%d\n", stack->cursor.x, stack->cursor.y, x, y );

          stack->cursor.x = x;
          stack->cursor.y = y;

          dfb_wm_update_cursor( stack, CCUF_POSITION );
     }

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
          dfb_updates_add( &tier->left.updates, region );

          ret = sawman_process_updates( sawman, flags, wm_data );
     }

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return ret;
}

static void
update_single( SaWMan              *sawman,
               SaWManWindow        *sawwin,
               SaWManTier          *tier,
               const DFBRegion     *regions,
               unsigned int         num,
               DFBSurfaceFlipFlags  flags,
               bool                 right_eye,
               WMData              *wmdata )
{
     int              i;
     CoreWindow      *window;
     CoreWindowStack *stack;

     window = sawwin->window;
     D_ASSERT( window != NULL );

     stack = sawwin->stack;
     D_ASSERT( stack != NULL );

     sawman_dispatch_tier_update( sawman, tier, right_eye, regions, num );

     fusion_skirmish_prevail( &wmdata->update_skirmish );

     for (i=0; i<num; i++) {
          DFBRectangle tmp = tier->single_src;

          if (!regions || dfb_rectangle_intersect_by_region( &tmp, &regions[i] )) {
               DFBRegion    reg;
               DFBRectangle src = tmp;
               DFBRegion    src_region;
               DFBRegion    cursor_visible;

               sawman_dispatch_blit( sawman, sawwin, right_eye, &src, &tmp, &regions[i] );

               tmp.x -= tier->single_src.x;
               tmp.y -= tier->single_src.y;

               D_ASSERT( window->surface != NULL );

               dfb_region_from_rectangle( &src_region, &src );

               dfb_gfx_copy_regions_client( window->surface, CSBR_FRONT, right_eye ? DSSE_RIGHT : DSSE_LEFT,
                                            tier->region->surface, CSBR_BACK, right_eye ? DSSE_RIGHT : DSSE_LEFT,
                                            &src_region, 1, tmp.x - src_region.x1, tmp.y - src_region.y1, &wmdata->client );

               dfb_region_from_rectangle( &reg, &tmp );

               /* Update cursor? */
               cursor_visible = tier->cursor_region;
               if (tier->cursor_drawn && dfb_region_region_intersect( &cursor_visible, &reg )) {
                    CardState    *state = &wmdata->state;
                    int           x;
                    int           y;
                    DFBDimension  size;

                    if (tier->single_mode) {
                         size.w = tier->single_width;
                         size.h = tier->single_height;
                    }
                    else {
                         size = tier->size;
                    }

                    x = (s64) stack->cursor.x * (s64) size.w / (s64) sawman->resolution.w;
                    y = (s64) stack->cursor.y * (s64) size.h / (s64) sawman->resolution.h;

                    D_ASSUME( tier->cursor_bs_valid );

                    D_DEBUG_AT( SaWMan_Cursor, "  -> saving background under cursor (%d,%d-%dx%d)\n",
                                DFB_RECTANGLE_VALS_FROM_REGION(&cursor_visible) );

                    dfb_gfx_copy_regions_client( tier->region->surface, CSBR_BACK, right_eye ? DSSE_RIGHT : DSSE_LEFT,
                                                 right_eye ? tier->cursor_bs_right : tier->cursor_bs, CSBR_BACK, DSSE_LEFT,
                                                 &cursor_visible, 1,
                                                 - tier->cursor_region.x1,
                                                 - tier->cursor_region.y1, &wmdata->client );


                    /* Set destination. */
                    state->destination  = tier->region->surface;
                    state->modified    |= SMF_DESTINATION;

                    /* Set clipping region. */
                    dfb_state_set_clip( state, &cursor_visible );

                    D_DEBUG_AT( SaWMan_Cursor, "  -> drawing cursor (%d,%d-%dx%d) at %d,%d\n",
                                DFB_RECTANGLE_VALS_FROM_REGION(&cursor_visible), x, y );

                    /* draw cursor */
                    sawman_draw_cursor( stack, state, tier->region->surface, &tier->cursor_region, x, y );

                    /* Reset destination. */
                    state->destination  = NULL;
                    state->modified    |= SMF_DESTINATION;
               }

               CoreGraphicsStateClient_Flush( &wmdata->client, 0, CGSCFF_NONE );

               dfb_layer_region_flip_update( tier->region, &reg, flags );
          }
     }

     fusion_skirmish_dismiss( &wmdata->update_skirmish );
}

static DFBResult
wm_update_window( CoreWindow          *window,
                  void                *wm_data,
                  void                *window_data,
                  const DFBRegion     *left_region,    /* surface coordinates! */
//#if DIRECTFB_VERSION_CODE >= VERSION_CODE(1,5,0)
                  const DFBRegion     *right_region,    /* surface coordinates! */
//#endif
                  DFBSurfaceFlipFlags  flags )
{
     DFBResult        ret;
     int              i;
     WMData          *wmdata = wm_data;
     SaWMan          *sawman;
     SaWManWindow    *sawwin = window_data;
     SaWManTier      *tier;
     CoreWindowStack *stack;
     StackData       *data;
     DFBWindowEvent   event;
     bool             stereo_layer;
     DFBRegion        left_update;
     DFBRegion        right_update;

     D_ASSERT( window != NULL );
     D_ASSERT( wm_data != NULL );
     D_ASSERT( window_data != NULL );
     DFB_REGION_ASSERT_IF( left_region );
     DFB_REGION_ASSERT_IF( right_region );

     sawman = wmdata->sawman;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     stack = sawwin->stack;
     D_ASSERT( stack != NULL );

     data = stack->stack_data;
     D_MAGIC_ASSERT( data, StackData );

#if D_DEBUG_ENABLED
     {
          char buf1[100];
          char buf2[100];

          if (left_region)
               snprintf( buf1, sizeof(buf1), "%d,%d-%dx%d", DFB_RECTANGLE_VALS_FROM_REGION( left_region ) );
          else
               snprintf( buf1, sizeof(buf1), "<0,0-%dx%d>", sawwin->bounds.w, sawwin->bounds.h );

          if (right_region)
               snprintf( buf2, sizeof(buf2), "%d,%d-%dx%d", DFB_RECTANGLE_VALS_FROM_REGION( right_region ) );
          else
               snprintf( buf2, sizeof(buf2), "<0,0-%dx%d>", sawwin->bounds.w, sawwin->bounds.h );

          D_DEBUG_AT( SaWMan_WM, "%s( %p, %p, %p, %s | %s )\n", __FUNCTION__, window, wm_data, window_data, buf1, buf2 );
     }
#endif

     if (left_region) {
          event.flags = DWEF_NONE;
          event.type  = DWET_UPDATE;
          event.x     = left_region->x1;
          event.y     = left_region->y1;
          event.w     = left_region->x2 - left_region->x1;
          event.h     = left_region->y2 - left_region->y1;

          sawman_post_event( sawman, sawwin, &event );
     }
     else {
          event.flags = DWEF_NONE;
          event.type  = DWET_UPDATE;
          event.x     = 0;
          event.y     = 0;
          event.w     = sawwin->src.w;
          event.h     = sawwin->src.h;

          sawman_post_event( sawman, sawwin, &event );
     }

     if (!SAWMAN_VISIBLE_WINDOW(window) || !data->active)
          return DFB_OK;

     /* Lock SaWMan. */
     ret = sawman_lock( sawman );
     if (ret)
          return ret;

     /* Check for window being inserted. */
     if (!(sawwin->flags & SWMWF_INSERTED)) {
          D_DEBUG_AT( SaWMan_WM, "  -> window %d not inserted!\n", window->id );
          sawman_unlock( sawman );
          return DFB_OK;
     }

     sawwin->flags &= ~SWMWF_UPDATING;

     /* Retrieve corresponding SaWManTier. */
     tier = sawman_tier_by_class( sawman, window->config.stacking );
     if (!tier) {
          D_ERROR( "SaWMan/WM: Cannot update window on unknown stack!\n" );
          sawman_unlock( sawman );
          return DFB_BUG;
     }

     D_ASSERT( tier->region != NULL );
     stereo_layer = tier->region->config.options & DLOP_STEREO;

     /*
      * Calculate actual left update
      */
     dfb_region_from_rectangle( &left_update, &sawwin->src );

     if (!left_region || dfb_region_region_intersect( &left_update, left_region ))
          left_region = &left_update;
     else
          left_region = NULL;

     /*
      * Calculate actual right update
      */
     dfb_region_from_rectangle( &right_update, &sawwin->src );

     if (!right_region || dfb_region_region_intersect( &right_update, right_region ))
          right_region = &right_update;
     else
          right_region = NULL;

     if (left_region)
          D_DEBUG_AT( SaWMan_WM, "  -> left  %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION(left_region) );

     if (right_region)
          D_DEBUG_AT( SaWMan_WM, "  -> right %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION(right_region) );

     if (!left_region && !right_region) {
          D_DEBUG_AT( SaWMan_WM, "  -> no update intersection!\n" );
          sawman_unlock( sawman );
          return DFB_OK;
     }

     if (flags & DSFLIP_QUEUE) {
          D_DEBUG_AT( SaWMan_FlipOnce, "  -> queuing update...\n" );

          dfb_updates_add( &sawwin->left.updates, sawwin->parent ? NULL : left_region );       // FIXME: will crash with NULL
          dfb_updates_add( &sawwin->right.updates, sawwin->parent ? NULL : right_region );     // FIXME: will crash with NULL
     }
     else {
          if (tier->single_mode && tier->single_window != NULL) {
               if (tier->single_window == sawwin) {
                    /* Save current buffers focus */
                    D_ASSERT( window->surface != NULL );
                    D_ASSERT( tier->region->surface != NULL );

                    if (flags & DSFLIP_FLUSH) {
                         D_DEBUG_AT( SaWMan_FlipOnce, "  -> flushing updates...\n" );

                         if (!sawwin->left.updates.num_regions && !sawwin->right.updates.num_regions) {
                              D_DEBUG_AT( SaWMan_FlipOnce, "  -> NO UPDATES\n" );
                              sawman_unlock( sawman );
                              return DFB_OK;
                         }

                         if (sawwin->left.updates.num_regions) {
                              D_DEBUG_AT( SaWMan_FlipOnce, "  -> %d left updates\n", sawwin->left.updates.num_regions );

                              update_single( sawman, sawwin, tier, sawwin->left.updates.regions, sawwin->left.updates.num_regions, flags, false, wmdata );
                         }

                         if (stereo_layer) {
                              if (sawwin->right.updates.num_regions) {
                                   D_DEBUG_AT( SaWMan_FlipOnce, "  -> %d right updates\n", sawwin->right.updates.num_regions );

                                   update_single( sawman, sawwin, tier, sawwin->right.updates.regions, sawwin->right.updates.num_regions, flags, true, wmdata );
                              }
                         }
                    }
                    else {
                         if (left_region)
                              update_single( sawman, sawwin, tier, left_region, 1, flags, false, wmdata );

                         if (stereo_layer && right_region)
                              update_single( sawman, sawwin, tier, right_region, 1, flags, true, wmdata );
                    }

                    if (flags & DSFLIP_FLUSH) {
                         dfb_updates_reset( &sawwin->left.updates );
                         dfb_updates_reset( &sawwin->right.updates );
                    }

                    sawwin->flags &= ~SWMWF_UPDATING;
               }
          }
          else {
               if (flags & DSFLIP_FLUSH) {
                    D_DEBUG_AT( SaWMan_FlipOnce, "  -> flushing updates...\n" );

                    if (!sawwin->left.updates.num_regions && !sawwin->right.updates.num_regions) {
                         D_DEBUG_AT( SaWMan_FlipOnce, "  -> NO UPDATES\n" );
                         sawman_unlock( sawman );
                         return DFB_OK;
                    }

                    D_DEBUG_AT( SaWMan_FlipOnce, "  -> %d left updates\n", sawwin->left.updates.num_regions );

                    for (i=0; i<sawwin->left.updates.num_regions; i++) {
                         sawman_update_window( sawman, sawwin,
                                               &sawwin->left.updates.regions[i],
                                               flags, SWMUF_SCALE_REGION );
                    }

                    if (stereo_layer) {
                         D_DEBUG_AT( SaWMan_FlipOnce, "  -> %d right updates\n", sawwin->right.updates.num_regions );

                         for (i=0; i<sawwin->right.updates.num_regions; i++) {
                              sawman_update_window( sawman, sawwin,
                                                    &sawwin->right.updates.regions[i],
                                                    flags, SWMUF_SCALE_REGION | SWMUF_RIGHT_EYE );
                         }
                    }

                    dfb_updates_reset( &sawwin->left.updates );
                    dfb_updates_reset( &sawwin->right.updates );
               }
               else {
                    sawman_update_window( sawman, sawwin,
                                          sawwin->parent ? NULL : left_region, /* FIXME? */
                                          flags, SWMUF_SCALE_REGION );

                    if (stereo_layer)
                         sawman_update_window( sawman, sawwin,
                                               sawwin->parent ? NULL : right_region, /* FIXME? */
                                               flags, SWMUF_SCALE_REGION | SWMUF_RIGHT_EYE );
               }

               sawwin->flags &= ~SWMWF_UPDATING;

               if (flags & DSFLIP_ONCE) {
                    D_DEBUG_AT( SaWMan_FlipOnce, "  -> flip once for window id %u\n", window->id );
                    tier->update_once = true;
               }

               sawman_process_updates( sawman, flags, wmdata );

               if (flags & DSFLIP_ONCE) {
                    while (tier->left.updates.num_regions || tier->right.updates.num_regions) {
                         D_DEBUG_AT( SaWMan_FlipOnce, "  -> waiting for updates...\n" );

                         switch (fusion_skirmish_wait( sawman->lock,
                                                       sawman_config->flip_once_timeout ?
                                                       sawman_config->flip_once_timeout + 10 : 0 ))
                         {
                              case DR_TIMEOUT:
                                   D_DEBUG_AT( SaWMan_FlipOnce, "  -> timeout waiting for updates!\n" );

                                   sawman_process_updates( sawman, flags, wmdata );
                                   break;

                              default:
                                   break;
                         }
                    }

                    D_DEBUG_AT( SaWMan_FlipOnce, "  -> updates done.\n" );
               }
          }
     }

     /* Unlock SaWMan. */
     sawman_unlock( sawman );

     return DFB_OK;
}

static DFBResult
update_hw_cursor( SaWMan                *sawman,
                  CoreWindowStack       *stack,
                  CoreCursorUpdateFlags  flags )
{
     DFBResult                   ret;
     int                         x, y;
     int                         mx, my;
     CoreLayer                  *layer;
     CoreLayerRegionConfig       config;
     CoreLayerRegionConfigFlags  config_flags = CLRCF_NONE;

     D_DEBUG_AT( SaWMan_Cursor, "%s( %p, %p, 0x%08x )\n", __FUNCTION__, sawman, stack, flags );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( stack != NULL );

     layer = dfb_layer_at( stack->context->layer_id );

     dfb_screen_get_layer_dimension( layer->screen, layer, &mx, &my );

     x = (s64) stack->cursor.x * (s64) mx / (s64) sawman->resolution.w;
     y = (s64) stack->cursor.y * (s64) my / (s64) sawman->resolution.h;

     D_DEBUG_AT( SaWMan_Cursor, "  -> position %4d,%4d (%d,%d)\n", x, y, stack->cursor.x, stack->cursor.y );

     // FIXME: initial update flags from DirectFB are CCUF_ENABLE only!
     if (!sawman->cursor.region->surface) {
          flags |= CCUF_POSITION | CCUF_SIZE | CCUF_SHAPE;

          config_flags = CLRCF_ALL;
     }

     config = sawman->cursor.context->primary.config;

     if (flags & CCUF_OPACITY) {
          if (stack->cursor.opacity) {
               if (!(sawman->cursor.region->state & CLRSF_ENABLED)) {
                    D_DEBUG_AT( SaWMan_Cursor, "  -> OPACITY  %d, enabling region\n", stack->cursor.opacity );

                    flags |= CCUF_ENABLE;
               }
          }
          else {
               if (sawman->cursor.region->state & CLRSF_ENABLED) {
                    D_DEBUG_AT( SaWMan_Cursor, "  -> OPACITY  %d, disabling region\n", stack->cursor.opacity );

                    flags |= CCUF_DISABLE;
               }
          }
     }

     if (flags & CCUF_DISABLE) {
          D_DEBUG_AT( SaWMan_Cursor, "  -> DISABLE\n" );

          dfb_layer_region_disable( sawman->cursor.region );
     }

     if (flags & (CCUF_POSITION | CCUF_SIZE | CCUF_SHAPE | CLRCF_SOURCE)) {
          DFBRegion clip = { 0, 0, mx-1, my-1 };

          if (flags & CCUF_POSITION)
               D_DEBUG_AT( SaWMan_Cursor, "  -> POSITION %4d,%4d (hot %d,%d)\n",
                           x, y, stack->cursor.hot.x, stack->cursor.hot.y );

          if (flags & CCUF_SIZE)
               D_DEBUG_AT( SaWMan_Cursor, "  -> SIZE     %4dx%4d\n",
                           stack->cursor.size.w, stack->cursor.size.h );

          config_flags |= CLRCF_DEST | CLRCF_SOURCE;

          config.dest.x = x - stack->cursor.hot.x;
          config.dest.y = y - stack->cursor.hot.y;
          config.dest.w = stack->cursor.size.w;
          config.dest.h = stack->cursor.size.h;

          config.source.x = 0;
          config.source.y = 0;
          config.source.w = stack->cursor.surface->config.size.w;
          config.source.h = stack->cursor.surface->config.size.h;

          if (!dfb_clip_blit_precheck( &clip, config.dest.w, config.dest.h, config.dest.x, config.dest.y )) {
               D_DEBUG_AT( SaWMan_Cursor, "  -> PRECHECK against %d,%d-%dx%d failed!\n", DFB_RECTANGLE_VALS_FROM_REGION(&clip) );
               return DFB_INVAREA;
          }

          dfb_clip_blit( &clip, &config.source, &config.dest.x, &config.dest.y );

          D_DEBUG_AT( SaWMan_Cursor, "  -> CLIPPED     %4d,%4d-%4dx%4d -> %4d,%4d\n",
                      DFB_RECTANGLE_VALS( &config.source ), config.dest.x, config.dest.y );

          config.dest.w = config.source.w;
          config.dest.h = config.source.h;
     }

     if (flags & CCUF_OPACITY) {
          D_DEBUG_AT( SaWMan_Cursor, "  -> OPACITY  %d\n", stack->cursor.opacity );

          if (stack->cursor.opacity != 255) {
               if (!(config.options & DLOP_OPACITY)) {
                    config_flags   |= CLRCF_OPTIONS;
                    config.options |= DLOP_OPACITY;
               }

               config_flags   |= CLRCF_OPACITY;
               config.opacity  = stack->cursor.opacity;
          }
          else {
               if (config.options & DLOP_OPACITY) {
                    config_flags   |= CLRCF_OPTIONS;
                    config.options &= ~DLOP_OPACITY;
               }
          }
     }

     if (flags & CCUF_SHAPE) {
          D_DEBUG_AT( SaWMan_Cursor, "  -> SHAPE (%dx%d %s, caps 0x%08x)\n",
                      stack->cursor.surface->config.size.w,
                      stack->cursor.surface->config.size.h,
                      dfb_pixelformat_name( stack->cursor.surface->config.format ),
                      stack->cursor.surface->config.caps );

          config_flags |= CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_SURFACE_CAPS | CLRCF_OPTIONS;

          config.width        = config.source.w;
          config.height       = config.source.h;
          config.format       = stack->cursor.surface->config.format;
          config.surface_caps = stack->cursor.surface->config.caps;

          if (DFB_PIXELFORMAT_HAS_ALPHA( stack->cursor.surface->config.format ))
               config.options |= DLOP_ALPHACHANNEL;
          else
               config.options &= ~DLOP_ALPHACHANNEL;
     }

     if (config_flags) {
          D_DEBUG_AT( SaWMan_Cursor, "  -> reconfiguring... (flags 0x%08x)\n", config_flags );

          ret = dfb_layer_region_set_configuration( sawman->cursor.region, &config, config_flags | CLRCF_FREEZE );
          if (ret) {
               D_DERROR( ret, "SaWMan/Cursor: Failed to reconfigure HW Cursor layer region!\n" );
               return ret;
          }

          D_ASSUME( sawman->cursor.region->state & CLRSF_FROZEN );

          sawman->cursor.region->config.keep_buffers = true;

          D_DEBUG_AT( SaWMan_Cursor, "  -> reconfiguring done.\n" );
     }

     if (flags & CCUF_SHAPE) {
          ret = dfb_layer_region_set_surface( sawman->cursor.region, stack->cursor.surface );
          if (ret) {
               D_DERROR( ret, "SaWMan/Cursor: Failed to set HW Cursor layer surface!\n" );
               return ret;
          }
     }

     if (flags & CCUF_ENABLE) {
          D_DEBUG_AT( SaWMan_Cursor, "  -> ENABLE\n" );

          D_ASSUME( sawman->cursor.region->state & CLRSF_FROZEN );

          dfb_layer_region_enable( sawman->cursor.region );
     }

     if (flags & (CCUF_SHAPE | CCUF_ENABLE) || config_flags) {
          D_DEBUG_AT( SaWMan_Cursor, "  -> updating region...\n" );

          dfb_layer_region_flip_update( sawman->cursor.region, NULL, DSFLIP_NONE );

          D_DEBUG_AT( SaWMan_Cursor, "  -> updating region done.\n" );
     }

     return DFB_OK;
}

static DFBResult
wm_update_cursor( CoreWindowStack       *stack,
                  void                  *wm_data,
                  void                  *stack_data,
                  CoreCursorUpdateFlags  flags )
{
     DFBResult         ret;
     int               i;
     DFBRegion         old_region;
     WMData           *wmdata   = wm_data;
     bool              restored = false;
     CoreLayerContext *context;
     CoreLayerRegion  *primary;
     CoreSurface      *surface;
     SaWMan           *sawman;
     SaWManTier       *tier;
     int               x;
     int               y;
     DFBDimension      size;
     DFBRegion         updates[2];
     int               updates_count = 0;
     DFBRegion         updates_united;
     StackData        *data = stack_data;

     D_DEBUG_AT( SaWMan_Cursor, "%s( %p, %p, %p, 0x%08x )\n", __FUNCTION__, stack, wm_data, stack_data, flags );

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

     if (tier->single_mode) {
          size.w = tier->single_width;
          size.h = tier->single_height;
     }
     else {
          size = tier->size;
     }

     x = (s64) stack->cursor.x * (s64) size.w / (s64) sawman->resolution.w;
     y = (s64) stack->cursor.y * (s64) size.h / (s64) sawman->resolution.h;

     D_DEBUG_AT( SaWMan_Cursor, "  -> position %4d,%4d (%d,%d)\n", x, y, stack->cursor.x, stack->cursor.y );

     old_region = tier->cursor_region;

     if (flags & (CCUF_ENABLE | CCUF_POSITION | CCUF_SIZE)) {
          tier->cursor_region.x1 = x - stack->cursor.hot.x;
          tier->cursor_region.y1 = y - stack->cursor.hot.y;
          tier->cursor_region.x2 = tier->cursor_region.x1 + stack->cursor.size.w - 1;
          tier->cursor_region.y2 = tier->cursor_region.y1 + stack->cursor.size.h - 1;

          if (!dfb_region_intersect( &tier->cursor_region, 0, 0, size.w - 1, size.h - 1 )) {
               tier->cursor_region.x1 = 1;
               tier->cursor_region.x2 = 0;
          }

          tier->cursor_bs_valid = false;
     }

     /*
      * HW Cursor mode?
      */
     if (sawman->cursor.region) {
          ret = update_hw_cursor( sawman, stack, flags );

          sawman_unlock( sawman );

          return ret;
     }

     /* Optimize case of invisible cursor moving. */
     if (!(flags & ~(CCUF_POSITION | CCUF_SHAPE)) && (!stack->cursor.opacity || !stack->cursor.enabled)) {
          D_DEBUG_AT( SaWMan_Cursor, "  -> invisible\n" );
          sawman_unlock( sawman );
          return DFB_OK;
     }

     /*
      * SW Cursor mode
      */
     context = stack->context;
     D_ASSERT( context != NULL );

     if (!tier->cursor_bs) {
          CoreSurface *cursor_bs;
          CoreSurface *cursor_bs_right;

          D_ASSUME( flags & CCUF_ENABLE );

          /* Create the cursor backing store surface. */
          ret = dfb_surface_create_simple( wmdata->core, stack->cursor.size.w, stack->cursor.size.h,
                                           context->config.pixelformat,
#if DIRECTFB_VERSION_CODE >= VERSION_CODE(1,5,0)
                                           context->config.colorspace,
#endif
                                           DSCAPS_NONE, CSTF_SHARED | CSTF_CURSOR, dfb_config->cursor_resource_id, NULL, &cursor_bs );
          if (ret) {
               D_ERROR( "WM/Default: Failed creating backing store for cursor!\n" );
               sawman_unlock( sawman );
               return ret;
          }

          /* Create the cursor backing store surface. */
          ret = dfb_surface_create_simple( wmdata->core, stack->cursor.size.w, stack->cursor.size.h,
                                           context->config.pixelformat,
#if DIRECTFB_VERSION_CODE >= VERSION_CODE(1,5,0)
                                           context->config.colorspace,
#endif
                                           DSCAPS_NONE, CSTF_SHARED | CSTF_CURSOR, dfb_config->cursor_resource_id, NULL, &cursor_bs_right );
          if (ret) {
               D_ERROR( "WM/Default: Failed creating backing store for cursor (right eye)!\n" );
               sawman_unlock( sawman );
               return ret;
          }

          ret = dfb_surface_globalize( cursor_bs );
          D_ASSERT( ret == DFB_OK );

          ret = dfb_surface_globalize( cursor_bs_right );
          D_ASSERT( ret == DFB_OK );

          tier->cursor_bs       = cursor_bs;
          tier->cursor_bs_right = cursor_bs_right;
     }

     D_ASSERT( tier->cursor_bs != NULL );
     D_ASSERT( tier->cursor_bs_right != NULL );

     /* Get the primary region. */
     primary = tier->region;
     D_ASSERT( primary != NULL );

     surface = primary->surface;
     D_ASSERT( surface != NULL );

     fusion_skirmish_prevail( &wmdata->update_skirmish );

     if (!wmdata->refs) {
          fusion_skirmish_dismiss( &wmdata->update_skirmish );
          sawman_unlock( sawman );
          return DFB_OK;
     }

     /* restore region under cursor */
     if (tier->cursor_drawn) {
          //DFBRectangle rect = { 0, 0,
          //                      old_region.x2 - old_region.x1 + 1,
          //                      old_region.y2 - old_region.y1 + 1 };

          D_ASSERT( stack->cursor.opacity || (flags & CCUF_OPACITY) );

          D_DEBUG_AT( SaWMan_Cursor, "  -> restore\n" );

          if (tier->active) {
               DFBRegion region = { 0, 0,
                                    old_region.x2 - old_region.x1,
                                    old_region.y2 - old_region.y1 };

               dfb_gfx_copy_regions_client( tier->cursor_bs, CSBR_BACK, DSSE_LEFT, surface, CSBR_BACK, DSSE_LEFT, &region, 1,
                                            old_region.x1, old_region.y1, &wmdata->client );

               if (primary->config.options & DLOP_STEREO) {
                    dfb_gfx_copy_regions_client( tier->cursor_bs_right, CSBR_BACK, DSSE_LEFT, surface, CSBR_BACK, DSSE_RIGHT, &region, 1,
                                                 old_region.x1, old_region.y1, &wmdata->client );
               }

               CoreGraphicsStateClient_Flush( &wmdata->client, 0, CGSCFF_NONE );

               restored = true;
          }

          tier->cursor_drawn = false;
     }

     if (flags & CCUF_SIZE) {
          D_DEBUG_AT( SaWMan_Cursor, "  -> resize\n" );

          ret = dfb_surface_reformat( tier->cursor_bs,
                                      stack->cursor.size.w, stack->cursor.size.h,
                                      tier->cursor_bs->config.format );
          if (ret)
               D_DERROR( ret, "WM/Default: Failed resizing backing store for cursor from %dx%d to %dx%d!\n",
                         tier->cursor_bs->config.size.w, tier->cursor_bs->config.size.h, stack->cursor.size.w, stack->cursor.size.h );

          ret = dfb_surface_reformat( tier->cursor_bs_right,
                                      stack->cursor.size.w, stack->cursor.size.h,
                                      tier->cursor_bs->config.format );
          if (ret)
               D_DERROR( ret, "WM/Default: Failed resizing backing store for cursor from %dx%d to %dx%d!\n",
                         tier->cursor_bs->config.size.w, tier->cursor_bs->config.size.h, stack->cursor.size.w, stack->cursor.size.h );
     }

     if (flags & CCUF_DISABLE) {
          D_DEBUG_AT( SaWMan_Cursor, "  -> disable\n" );

          dfb_surface_unlink( &tier->cursor_bs );
     }
     else if (stack->cursor.opacity && data->active && tier->cursor_region.x1 <= tier->cursor_region.x2) {
          CardState *state = &wmdata->state;

          /* backup region under cursor */
          if (!tier->cursor_bs_valid) {
               //DFBRectangle rect = DFB_RECTANGLE_INIT_FROM_REGION( &tier->cursor_region );

               D_DEBUG_AT( SaWMan_Cursor, "  -> backup\n" );

               D_ASSERT( !tier->cursor_drawn );

               dfb_gfx_copy_regions_client( surface, CSBR_BACK, DSSE_LEFT, tier->cursor_bs, CSBR_BACK, DSSE_LEFT,
                                            &tier->cursor_region, 1, -tier->cursor_region.x1, -tier->cursor_region.y1,
                                            &wmdata->client );

               if (primary->config.options & DLOP_STEREO) {
                    dfb_gfx_copy_regions_client( surface, CSBR_BACK, DSSE_RIGHT, tier->cursor_bs_right, CSBR_BACK, DSSE_LEFT,
                                                 &tier->cursor_region, 1, -tier->cursor_region.x1, -tier->cursor_region.y1,
                                                 &wmdata->client );
               }

               tier->cursor_bs_valid = true;
          }

          D_DEBUG_AT( SaWMan_Cursor, "  -> draw\n" );

          /* Set destination. */
          state->destination  = surface;
          state->modified    |= SMF_DESTINATION;

          state->to_eye    = DSSE_LEFT;
          state->modified |= SMF_TO;

          /* draw cursor */
          sawman_draw_cursor( stack, state, surface, &tier->cursor_region, x, y );

          if (primary->config.options & DLOP_STEREO) {
               state->to_eye    = DSSE_RIGHT;
               state->modified |= SMF_TO;

               /* draw cursor */
               sawman_draw_cursor( stack, state, surface, &tier->cursor_region, x, y );
          }

          /* Reset destination. */
          state->destination  = NULL;
          state->modified    |= SMF_DESTINATION;

          CoreGraphicsStateClient_Flush( &wmdata->client, 0, CGSCFF_NONE );

          tier->cursor_drawn = true;

          if (restored) {
               if (dfb_region_region_intersects( &old_region, &tier->cursor_region ))
                    dfb_region_region_union( &old_region, &tier->cursor_region );
               else
                    updates[updates_count++] = tier->cursor_region;

               updates[updates_count++] = old_region;
          }
          else
               updates[updates_count++] = tier->cursor_region;
     }
     else if (restored)
          updates[updates_count++] = old_region;

     D_DEBUG_AT( SaWMan_Cursor, "  -> %d updates\n", updates_count );

     if (updates_count) {
          switch (primary->config.buffermode) {
               case DLBM_TRIPLE:
                    /* Add the updated region .*/
                    for (i=0; i<updates_count; i++) {
                         const DFBRegion *update = &updates[i];

                         DFB_REGION_ASSERT( update );

                         dfb_updates_add( &tier->left.updating, update );

                         if (tier->region->config.options & DLOP_STEREO)
                              dfb_updates_add( &tier->right.updating, update );
                    }

                    if (!tier->left.updated.num_regions && !tier->right.updated.num_regions)
                         sawman_flush_updating( sawman, tier, wmdata );
                    break;

               case DLBM_BACKVIDEO:
                    /* Flip the united region. */
                    dfb_regions_unite( &updates_united, updates, updates_count );
                    dfb_layer_region_flip_update( primary, &updates_united, DSFLIP_BLIT );
                    break;

               default:
                    /* Flip the updated region .*/
                    for (i=0; i<updates_count; i++) {
                         const DFBRegion *update = &updates[i];

                         DFB_REGION_ASSERT( update );

                         dfb_layer_region_flip_update( primary, update, DSFLIP_NONE );
                    }
                    break;
          }
     }

     CoreGraphicsStateClient_Flush( &wmdata->client, 0, CGSCFF_NONE );

     fusion_skirmish_dismiss( &wmdata->update_skirmish );

     sawman_unlock( sawman );

     return DFB_OK;
}

