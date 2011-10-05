/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#include <direct/debug.h>
#include <direct/list.h>

#include <fusion/conf.h>
#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/palette.h>
#include <core/screen.h>
#include <core/windows_internal.h>
#include <core/wm.h>

#include <gfx/clip.h>
#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/conf.h>

#include <sawman.h>

#include "sawman_config.h"
#include "sawman_draw.h"
#include "sawman_window.h"

#include "isawman.h"


D_DEBUG_DOMAIN( SaWMan_Update,   "SaWMan/Update",   "SaWMan window manager updates" );
D_DEBUG_DOMAIN( SaWMan_Geometry, "SaWMan/Geometry", "SaWMan window manager geometry" );
D_DEBUG_DOMAIN( SaWMan_Stacking, "SaWMan/Stacking", "SaWMan window manager stacking" );
D_DEBUG_DOMAIN( SaWMan_Cursor,   "SaWMan/Cursor",   "SaWMan window manager cursor" );

/**********************************************************************************************************************/

static void wind_of_change ( SaWMan              *sawman,
                             SaWManTier          *tier,
                             const DFBRegion     *update,
                             DFBSurfaceFlipFlags  flags,
                             int                  current,
                             int                  changed,
                             bool                 right_eye );

static void wind_of_showing( SaWMan              *sawman,
                             SaWManTier          *tier,
                             DFBRegion           *update,
                             int                  current,
                             int                  changed,
                             bool                *ret_showing );

static void update_visible ( SaWMan              *sawman,
                             int                  start,
                             int                  x1,
                             int                  y1,
                             int                  x2,
                             int                  y2,
                             bool                 right_eye );

/**********************************************************************************************************************/

DirectResult
sawman_switch_focus( SaWMan       *sawman,
                     SaWManWindow *to )
{
     DirectResult    ret;
     DFBWindowEvent  evt;
     SaWManWindow   *from;
     SaWManTier     *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT_IF( to, SaWManWindow );

     from = sawman->focused_window;
     D_MAGIC_ASSERT_IF( from, SaWManWindow );

     FUSION_SKIRMISH_ASSERT( sawman->lock );

     if (from == to)
          return DFB_OK;

     switch (ret = sawman_call( sawman, SWMCID_SWITCH_FOCUS, &to, sizeof(to), false )) {
          case DFB_OK:
          case DFB_NOIMPL:
               break;

          default:
               return ret;
     }

     if (from) {
          evt.type = DWET_LOSTFOCUS;

          sawman_post_event( sawman, from, &evt );

          if (sawman_window_border( from )) {
               sawman_update_window( sawman, from, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );

               tier = sawman_tier_by_class( sawman, from->window->config.stacking );
               D_ASSERT(tier->region != NULL);
               if (tier->region->config.options & DLOP_STEREO) 
                    sawman_update_window( sawman, from, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
          }
     }

     if (to) {
          CoreWindowStack *stack   = ((SaWManTier*) sawman->tiers)->stack;
          u8               opacity = (to->window->config.cursor_flags & DWCF_INVISIBLE) ? 0x00 : 0xff;

#ifndef OLD_COREWINDOWS_STRUCTURE
          CoreWindow *window = to->window;

          D_MAGIC_ASSERT( window, CoreWindow );

          if (window->toplevel) {
               CoreWindow *toplevel = window->toplevel;

               D_MAGIC_ASSERT( toplevel, CoreWindow );

               toplevel->subfocus = window;
          }
          else if (window->subfocus) {
               window = window->subfocus;
               D_MAGIC_ASSERT( window, CoreWindow );

               to = window->window_data;
               D_MAGIC_ASSERT( to, SaWManWindow );
          }
#endif

          evt.type = DWET_GOTFOCUS;

          sawman_post_event( sawman, to, &evt );

          if (sawman_window_border( to )) {
               sawman_update_window( sawman, to, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );

               tier = sawman_tier_by_class( sawman, to->window->config.stacking );
               D_ASSERT(tier->region != NULL);
               if (tier->region->config.options & DLOP_STEREO) 
                    sawman_update_window( sawman, to, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
          }


          if (stack->cursor.opacity != opacity) {
               stack->cursor.opacity = opacity;

               dfb_wm_update_cursor( stack, CCUF_OPACITY );
          }
     }

     sawman->focused_window = to;

     return DFB_OK;
}

DirectResult
sawman_post_event( SaWMan         *sawman,
                   SaWManWindow   *sawwin,
                   DFBWindowEvent *event )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( sawwin->window != NULL );
     D_ASSERT( event != NULL );

     event->buttons   = sawman->buttons;
     event->modifiers = sawman->modifiers;
     event->locks     = sawman->locks;

     dfb_window_post_event( sawwin->window, event );

     return DFB_OK;
}

DirectResult
sawman_update_window( SaWMan              *sawman,
                      SaWManWindow        *sawwin,
                      const DFBRegion     *region,
                      DFBSurfaceFlipFlags  flags,
                      SaWManUpdateFlags    update_flags )
{
     DFBRegion        area;
     CoreWindowStack *stack;
     CoreWindow      *window;
     SaWManTier      *tier;
     int              stereo_offset;
     DFBUpdates      *updates;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     DFB_REGION_ASSERT_IF( region );
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     stack  = sawwin->stack;
     window = sawwin->window;

     D_ASSERT( stack != NULL );
     D_MAGIC_COREWINDOW_ASSERT( window );

     D_DEBUG_AT( SaWMan_Update, "%s( %p, %p )\n", __FUNCTION__, sawwin, region );

     if (!SAWMAN_VISIBLE_WINDOW(window) && !(update_flags & SWMUF_FORCE_INVISIBLE))
          return DFB_OK;

     D_ASSUME( sawwin->flags & SWMWF_INSERTED );

     /* Make sure window is inserted. */
     if (!(sawwin->flags & SWMWF_INSERTED)) {
          D_DEBUG_AT( SaWMan_Update, "  -> window %d not inserted!\n", window->id );
          return DFB_OK;
     }

     tier = sawman_tier_by_class( sawman, window->config.stacking );
     updates = update_flags & SWMUF_RIGHT_EYE ? &tier->right.updates : &tier->left.updates;
     stereo_offset = window->config.z;       /* z is 0 for mono windows */
     if (update_flags & SWMUF_RIGHT_EYE)
          stereo_offset *= -1;

     if (region) {
          if ((update_flags & SWMUF_SCALE_REGION) && (window->config.options & DWOP_SCALE)) {
               int sw = sawwin->src.w;
               int sh = sawwin->src.h;
               int dw = sawwin->dst.w;
               int dh = sawwin->dst.h;

               /* horizontal */
               if (dw > sw) {
                    /* upscaling */
                    area.x1 = (region->x1 - 1) * dw / sw;
                    area.x2 = (region->x2 + 1) * dw / sw;
               }
               else {
                    /* downscaling */
                    area.x1 = region->x1 * dw / sw - 1;
                    area.x2 = region->x2 * dw / sw + 1;
               }

               /* vertical */
               if (dh > sh) {
                    /* upscaling */
                    area.y1 = (region->y1 - 1) * dh / sh;
                    area.y2 = (region->y2 + 1) * dh / sh;
               }
               else {
                    /* downscaling */
                    area.y1 = region->y1 * dh / sh - 1;
                    area.y2 = region->y2 * dh / sh + 1;
               }

               /* limit to window area */
               dfb_region_clip( &area, 0, 0, dw - 1, dh - 1 );

               /* screen offset */
               dfb_region_translate( &area, sawwin->dst.x, sawwin->dst.y );
          }
          else
               area = DFB_REGION_INIT_TRANSLATED( region, sawwin->dst.x, sawwin->dst.y );
     }
     else {
          if ((update_flags & SWMUF_UPDATE_BORDER) && sawman_window_border( sawwin )) 
               area = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->bounds );
          else
               area = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->dst );
     }

     /* apply stereo offset */
     dfb_region_translate( &area, stereo_offset, 0 );

     if (!dfb_unsafe_region_intersect( &area, 0, 0, tier->size.w - 1, tier->size.h - 1 ))
          return DFB_OK;

     if (update_flags & SWMUF_FORCE_COMPLETE)
          dfb_updates_add( updates, &area );
     else
          wind_of_change( sawman, tier, &area, flags,
                          fusion_vector_size( &sawman->layout ) - 1,
                          sawman_window_index( sawman, sawwin ),
                          update_flags & SWMUF_RIGHT_EYE );

     return DFB_OK;
}

DirectResult
sawman_showing_window( SaWMan       *sawman,
                       SaWManWindow *sawwin,
                       bool         *ret_showing )
{
     DFBRegion        area;
     CoreWindowStack *stack;
     CoreWindow      *window;
     SaWManTier      *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     stack  = sawwin->stack;
     window = sawwin->window;

     D_ASSERT( stack != NULL );
     D_MAGIC_COREWINDOW_ASSERT( window );

     if (!sawman_tier_by_stack( sawman, stack, &tier ))
          return DFB_BUG;

     *ret_showing = false;

     if (!SAWMAN_VISIBLE_WINDOW(window))
          return DFB_OK;

     /* Make sure window is inserted. */
     if (!(sawwin->flags & SWMWF_INSERTED))
          return DFB_OK;

     area = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->bounds );

     if (!dfb_unsafe_region_intersect( &area, 0, 0, stack->width - 1, stack->height - 1 ))
          return DFB_OK;

     if (fusion_vector_has_elements( &sawman->layout ) && window >= 0) {
          int num = fusion_vector_size( &sawman->layout );

          wind_of_showing( sawman, tier, &area, num - 1,
                           sawman_window_index( sawman, sawwin ), ret_showing );
     }
     else
          *ret_showing = true;

     return DFB_OK;
}

DirectResult
sawman_insert_window( SaWMan       *sawman,
                      SaWManWindow *sawwin,
                      SaWManWindow *relative,
                      bool          top )
{
     DirectResult  ret;
     int           index = 0;
     SaWManWindow *other;
     CoreWindow   *window;

     D_DEBUG_AT( SaWMan_Stacking, "%s( %p, %p, %p, %s )\n", __FUNCTION__,
                 sawman, sawwin, relative, top ? "top" : "below" );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_MAGIC_ASSERT_IF( relative, SaWManWindow );
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     window = sawwin->window;
     D_MAGIC_COREWINDOW_ASSERT( window );

#ifndef OLD_COREWINDOWS_STRUCTURE
     /* In case of a sub window, the order from sub window vector is followed */
     if (window->toplevel) {
          CoreWindow   *toplevel = window->toplevel;
          CoreWindow   *tmp;
          SaWManWindow *parent;

          /* Enforce association rules... */
          parent = sawwin->parent;
          if (parent) {
               D_MAGIC_ASSERT( parent, SaWManWindow );

               tmp = parent->window;
               D_ASSERT( tmp != NULL );
               D_ASSERT( tmp->toplevel == toplevel );

               if (window->config.options & DWOP_KEEP_UNDER) {
                    int under;

                    index = fusion_vector_index_of( &toplevel->subwindows, window );
                    under = fusion_vector_index_of( &toplevel->subwindows, parent );

                    if (index < under - 1) {
                         D_DEBUG_AT( SaWMan_Stacking, "  -> moving under (%d->%d)\n", index, under - 1 );

                         fusion_vector_move( &toplevel->subwindows, index, under - 1 );
                    }
                    else if (index > under - 1) {
                         D_DEBUG_AT( SaWMan_Stacking, "  -> moving under (%d<-%d)\n", under, index );

                         fusion_vector_move( &toplevel->subwindows, index, under );
                    }
               }
               else if (window->config.options & DWOP_KEEP_ABOVE) {
                    int above;

                    index = fusion_vector_index_of( &toplevel->subwindows, window );
                    above = fusion_vector_index_of( &toplevel->subwindows, parent );

                    if (index < above + 1) {
                         D_DEBUG_AT( SaWMan_Stacking, "  -> moving above (%d->%d)\n", index, above );

                         fusion_vector_move( &toplevel->subwindows, index, above );
                    }
                    else if (index > above + 1) {
                         D_DEBUG_AT( SaWMan_Stacking, "  -> moving above (%d<-%d)\n", above + 1, index );

                         fusion_vector_move( &toplevel->subwindows, index, above + 1 );
                    }
               }
          }

          /* Lookup our index in top level window */
          index = fusion_vector_index_of( &toplevel->subwindows, window );

          D_DEBUG_AT( SaWMan_Stacking, "  -> toplevel %p [%4d,%4d-%4dx%4d] (%d)\n",
                      toplevel, DFB_RECTANGLE_VALS(&toplevel->config.bounds), index );

          /* Get sub window below (or top level) */
          if (index == 0)
               tmp = toplevel;
          else
               tmp = fusion_vector_at( &toplevel->subwindows, index - 1 );

          D_DEBUG_AT( SaWMan_Stacking, "  -> relative %p [%4d,%4d-%4dx%4d] (%d)\n",
                      tmp, DFB_RECTANGLE_VALS(&tmp->config.bounds), index - 1 );

          /* Place on top */
          relative = tmp->window_data;
          top      = true;
     }
     else
#endif
     if (sawwin->parent && (window->config.options & (DWOP_KEEP_ABOVE|DWOP_KEEP_UNDER))) {
          D_MAGIC_ASSERT( sawwin->parent, SaWManWindow );

          relative = sawwin->parent;
          top      = (window->config.options & DWOP_KEEP_ABOVE) ? true : false;

          D_MAGIC_ASSERT( relative, SaWManWindow );

#ifndef OLD_COREWINDOWS_STRUCTURE
          if (top && relative->window->subwindows.count) {
               CoreWindow   *tmp;

               tmp      = fusion_vector_at( &relative->window->subwindows, relative->window->subwindows.count - 1 );
               relative = tmp->window_data;

               D_MAGIC_ASSERT( relative, SaWManWindow );
          }
#endif
     }


     if (relative)
          D_ASSUME( relative->priority == sawwin->priority );

     if (relative) {
          index = sawman_window_index( sawman, relative );
          D_ASSERT( index >= 0 );
          D_ASSERT( index < sawman->layout.count );

          if (top)
               index++;
     }
     else if (top) {
          /*
           * Iterate from bottom to top,
           * stopping at the first window with a higher priority.
           */
          fusion_vector_foreach (other, index, sawman->layout) {
               D_MAGIC_ASSERT( other, SaWManWindow );

               if (other->priority > sawwin->priority)
                    break;
          }
     }
     else {
          /*
           * Iterate from bottom to top,
           * stopping at the first window with equal or higher priority.
           */
          fusion_vector_foreach (other, index, sawman->layout) {
               D_MAGIC_ASSERT( other, SaWManWindow );

               if (other->priority >= sawwin->priority)
                    break;
          }
     }

     /* (Re)Insert the window at the acquired position. */
     if (sawwin->flags & SWMWF_INSERTED) {
          int old = sawman_window_index( sawman, sawwin );

          D_ASSERT( old >= 0 );
          D_ASSERT( old < sawman->layout.count );

          if (old < index)
               index--;

          if (old != index)
               fusion_vector_move( &sawman->layout, old, index );
     }
     else {
          ret = fusion_vector_insert( &sawman->layout, sawwin, index );
          if (ret)
               return ret;

          /* Set 'inserted' flag. */
          sawwin->flags |= SWMWF_INSERTED;
     }

     sawman_update_visible( sawman );

     return DFB_OK;
}

DirectResult
sawman_remove_window( SaWMan       *sawman,
                      SaWManWindow *sawwin )
{
     int               index;
     CoreWindow       *window;
     SaWManGrabbedKey *key;
     DirectLink       *next;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     window = sawwin->window;
     D_MAGIC_COREWINDOW_ASSERT( window );

     if (!(sawwin->flags & SWMWF_INSERTED)) {
          D_BUG( "window %d not inserted", window->id );
          return DFB_BUG;
     }

     sawman_withdraw_window( sawman, sawwin );

     index = sawman_window_index( sawman, sawwin );
     D_ASSERT( index >= 0 );
     D_ASSERT( index < sawman->layout.count );

     fusion_vector_remove( &sawman->layout, index );

     /* Release all explicit key grabs. */
     direct_list_foreach_safe (key, next, sawman->grabbed_keys) {
          if (key->owner == sawwin) {
               direct_list_remove( &sawman->grabbed_keys, &key->link );
               SHFREE( sawwin->shmpool, key );
          }
     }

     /* Release grab of unselected keys. */
     if (sawman->unselkeys_window == sawwin)
          sawman->unselkeys_window = NULL;

     /* Free key list. */
     if (window->config.keys) {
          SHFREE( sawwin->shmpool, window->config.keys );

          window->config.keys     = NULL;
          window->config.num_keys = 0;
     }

     sawwin->flags &= ~SWMWF_INSERTED;

     return DFB_OK;
}

DirectResult
sawman_withdraw_window( SaWMan       *sawman,
                        SaWManWindow *sawwin )
{
     int         i, index;
     CoreWindow *window;
     SaWManTier *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     window = sawwin->window;
     D_MAGIC_COREWINDOW_ASSERT( window );

     /* No longer be the 'entered window'. */
     if (sawman->entered_window == sawwin)
          sawman->entered_window = NULL;

     /* Remove focus from window. */
     if (sawman->focused_window == sawwin) {
          SaWManWindow *swin;
          CoreWindow   *cwin;

          sawman->focused_window = NULL;

          /* Always try to have a focused window */
          fusion_vector_foreach_reverse (swin, index, sawman->layout) {
               D_MAGIC_ASSERT( swin, SaWManWindow );

               cwin = swin->window;
               D_ASSERT( cwin != NULL );

               if (swin != sawwin && cwin->config.opacity && !(cwin->config.options & DWOP_GHOST)) {
                    sawman_switch_focus( sawman, swin );
                    break;
               }
          }
     }

#ifndef OLD_COREWINDOWS_STRUCTURE
     if (window->toplevel) {
          CoreWindow *toplevel = window->toplevel;

          D_MAGIC_ASSERT( toplevel, CoreWindow );

          if (toplevel->subfocus == window)
               toplevel->subfocus = NULL;
     }
#endif

     /* Release explicit keyboard grab. */
     if (sawman->keyboard_window == sawwin)
          sawman->keyboard_window = NULL;

     /* Release explicit pointer grab. */
     if (sawman->pointer_window == sawwin)
          sawman->pointer_window = NULL;

     /* Release all implicit key grabs. */
     for (i=0; i<SAWMAN_MAX_IMPLICIT_KEYGRABS; i++) {
          if (sawman->keys[i].code != -1 && sawman->keys[i].owner == sawwin) {
               if (!DFB_WINDOW_DESTROYED( window )) {
                    DFBWindowEvent we;

                    we.type       = DWET_KEYUP;
                    we.key_code   = sawman->keys[i].code;
                    we.key_id     = sawman->keys[i].id;
                    we.key_symbol = sawman->keys[i].symbol;

                    sawman_post_event( sawman, sawwin, &we );
               }

               sawman->keys[i].code  = -1;
               sawman->keys[i].owner = NULL;
          }
     }

     /* Hide window. */
     if (SAWMAN_VISIBLE_WINDOW(window) && (sawwin->flags & SWMWF_INSERTED)) {
          window->config.opacity = 0;

          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_FORCE_INVISIBLE | SWMUF_UPDATE_BORDER );

          tier = sawman_tier_by_class( sawman, sawwin->window->config.stacking );
          D_ASSERT(tier->region != NULL);
          if (tier->region->config.options & DLOP_STEREO) 
               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, 
                    SWMUF_FORCE_INVISIBLE | SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
apply_geometry( const DFBWindowGeometry *geometry,
                const DFBRegion         *clip,
                const DFBWindowGeometry *parent,
                DFBRectangle            *ret_rect )
{
     int width, height;

     D_ASSERT( geometry != NULL );
     DFB_REGION_ASSERT( clip );
     D_ASSERT( ret_rect != NULL );

     width  = clip->x2 - clip->x1 + 1;
     height = clip->y2 - clip->y1 + 1;

     switch (geometry->mode) {
          case DWGM_DEFAULT:
               D_DEBUG_AT( SaWMan_Geometry, " -- default\n" );
               *ret_rect = DFB_RECTANGLE_INIT_FROM_REGION( clip );
               D_DEBUG_AT( SaWMan_Geometry, " => %d,%d-%dx%d\n", DFB_RECTANGLE_VALS( ret_rect ) );
               return;

          case DWGM_FOLLOW:
               D_ASSERT( parent != NULL );
               D_DEBUG_AT( SaWMan_Geometry, " -- FOLLOW\n" );
               apply_geometry( parent, clip, NULL, ret_rect );
               break;

          case DWGM_RECTANGLE:
               D_DEBUG_AT( SaWMan_Geometry, " -- RECTANGLE [%d,%d-%dx%d]\n",
                           DFB_RECTANGLE_VALS( &geometry->rectangle ) );
               *ret_rect = geometry->rectangle;
               ret_rect->x += clip->x1;
               ret_rect->y += clip->y1;
               break;

          case DWGM_LOCATION:
               D_DEBUG_AT( SaWMan_Geometry, " -- LOCATION [%.3f,%.3f-%.3fx%.3f]\n",
                           geometry->location.x, geometry->location.y,
                           geometry->location.w, geometry->location.h );
               ret_rect->x = (int)(geometry->location.x * width  + 0.5f) + clip->x1;
               ret_rect->y = (int)(geometry->location.y * height + 0.5f) + clip->y1;
               ret_rect->w = (int)(geometry->location.w * width  + 0.5f);
               ret_rect->h = (int)(geometry->location.h * height + 0.5f);
               break;

          default:
               D_BUG( "invalid geometry mode %d", geometry->mode );
               return;
     }

     D_DEBUG_AT( SaWMan_Geometry, " -> %d,%d-%dx%d / clip [%d,%d-%dx%d]\n",
                 DFB_RECTANGLE_VALS( ret_rect ),
                 DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     if (!dfb_rectangle_intersect_by_region( ret_rect, clip )) {
          D_WARN( "invalid geometry" );
          dfb_rectangle_from_region( ret_rect, clip );
     }

     D_DEBUG_AT( SaWMan_Geometry, " => %d,%d-%dx%d\n", DFB_RECTANGLE_VALS( ret_rect ) );
}

DirectResult
sawman_update_geometry( SaWManWindow *sawwin )
{
     int           i;
     CoreWindow   *window;
     CoreWindow   *parent_window = NULL;
     CoreWindow   *toplevel;
     SaWManWindow *topsaw = NULL;
     SaWMan       *sawman;
     SaWManWindow *parent;
     SaWManWindow *child;
     CoreWindow   *childwin;
     DFBRegion     clip;
     DFBRectangle  src;
     DFBRectangle  dst;
     bool          src_updated = false;
     bool          dst_updated = false;
     SaWManTier   *tier;
     bool          stereo_layer;

     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     D_DEBUG_AT( SaWMan_Geometry, "%s( %p )\n", __FUNCTION__, sawwin );

     sawman = sawwin->sawman;
     D_MAGIC_ASSERT_IF( sawman, SaWMan );

     if (sawman)
          FUSION_SKIRMISH_ASSERT( sawman->lock );

     window = sawwin->window;
     D_MAGIC_COREWINDOW_ASSERT( window );

     parent = sawwin->parent;
     if (parent) {
          D_MAGIC_ASSERT( parent, SaWManWindow );

          parent_window = parent->window;
          D_ASSERT( parent_window != NULL );
     }

     toplevel = WINDOW_TOPLEVEL(window);
     if (toplevel) {
          topsaw = toplevel->window_data;
          D_MAGIC_ASSERT( topsaw, SaWManWindow );
     }

     if (parent && (window->config.options & DWOP_FOLLOW_BOUNDS))
          /* Initialize bounds from parent window (window association) */
          sawwin->bounds = parent->bounds;
     else
          /* Initialize bounds from base window configuration */
          sawwin->bounds = window->config.bounds;

     /*
      * In case of a sub window, the top level surface is the coordinate space instead of the layer surface
      */
     toplevel = WINDOW_TOPLEVEL(window);
     if (toplevel) {
          DFBDimension in, out;

          D_DEBUG_AT( SaWMan_Geometry, "  -> sub bounds %4d,%4d-%4dx%4d (base)\n", DFB_RECTANGLE_VALS(&sawwin->bounds) );

          D_DEBUG_AT( SaWMan_Geometry, "  o- top src    %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS(&topsaw->src) );
          D_DEBUG_AT( SaWMan_Geometry, "  o- top dst    %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS(&topsaw->dst) );

          /*
           * Translate against top level source geometry
           */
          sawwin->bounds.x -= topsaw->src.x;
          sawwin->bounds.y -= topsaw->src.y;

          D_DEBUG_AT( SaWMan_Geometry, "  -> sub bounds %4d,%4d-%4dx%4d (translated)\n", DFB_RECTANGLE_VALS(&sawwin->bounds) );

          /*
           * Take input dimension from top level source geometry
           */
          in.w = topsaw->src.w;
          in.h = topsaw->src.h;

          /*
           * Take output dimension from top level destination geometry
           */
          out.w = topsaw->dst.w;
          out.h = topsaw->dst.h;

          /*
           * Scale the sub window size if top level window is scaled
           */
          if (in.w != out.w || in.h != out.h) {
               D_DEBUG_AT( SaWMan_Geometry, "  o- scale in             %4dx%4d\n", in.w, in.h );
               D_DEBUG_AT( SaWMan_Geometry, "  o- scale out            %4dx%4d\n", out.w, out.h );

               sawwin->bounds.x = sawwin->bounds.x * out.w / in.w;
               sawwin->bounds.y = sawwin->bounds.y * out.h / in.h;
               sawwin->bounds.w = sawwin->bounds.w * out.w / in.w;
               sawwin->bounds.h = sawwin->bounds.h * out.h / in.h;

               D_DEBUG_AT( SaWMan_Geometry, "  -> sub bounds %4d,%4d-%4dx%4d (scaled)\n", DFB_RECTANGLE_VALS(&sawwin->bounds) );
          }

          /*
           * Translate to top level destination geometry
           */
          sawwin->bounds.x += topsaw->dst.x;
          sawwin->bounds.y += topsaw->dst.y;

          D_DEBUG_AT( SaWMan_Geometry, "  => sub bounds %4d,%4d-%4dx%4d (translated)\n", DFB_RECTANGLE_VALS(&sawwin->bounds) );
     }

     /* Calculate source geometry. */
     clip.x1 = 0;
     clip.y1 = 0;

     if (    window->caps & (DWCAPS_INPUTONLY | DWCAPS_COLOR)
          || window->config.options & DWOP_INPUTONLY ) {
          clip.x2 = sawwin->bounds.w - 1;
          clip.y2 = sawwin->bounds.h - 1;
     }
     else {
          CoreSurface *surface = window->surface;
          D_ASSERT( surface != NULL );

          clip.x2 = surface->config.size.w - 1;
          clip.y2 = surface->config.size.h - 1;
     }

     D_DEBUG_AT( SaWMan_Geometry, "  -> Applying source geometry...\n" );

     apply_geometry( &window->config.src_geometry, &clip,
                     parent_window ? &parent_window->config.src_geometry : NULL, &src );

     /* Calculate destination geometry. */
     clip = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->bounds );

     D_DEBUG_AT( SaWMan_Geometry, "  -> Applying destination geometry...\n" );

     apply_geometry( &window->config.dst_geometry, &clip,
                     parent_window ? &parent_window->config.dst_geometry : NULL, &dst );

     /* Adjust src/dst if clipped by top level window */
     if (toplevel) {
          DFBRegion topclip = DFB_REGION_INIT_FROM_RECTANGLE( &topsaw->dst );

          /*
           * Clip the sub window bounds against the top level window
           */
          dfb_clip_stretchblit( &topclip, &src, &dst );

          D_DEBUG_AT( SaWMan_Geometry, "  => sub dst    %4d,%4d-%4dx%4d (clipped)\n", DFB_RECTANGLE_VALS(&dst) );
          D_DEBUG_AT( SaWMan_Geometry, "  => sub src    %4d,%4d-%4dx%4d (clipped)\n", DFB_RECTANGLE_VALS(&src) );
     }

     tier = sawman_tier_by_class( sawman, sawwin->window->config.stacking );
     D_ASSERT(tier->region != NULL);
     stereo_layer = tier->region->config.options & DLOP_STEREO;

     /* Update source geometry. */
     if (!DFB_RECTANGLE_EQUAL( src, sawwin->src )) {
          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE );

          if (stereo_layer) 
               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE | SWMUF_RIGHT_EYE );

          sawwin->src = src;
          src_updated = true;
     }

     /* Update destination geometry. */
     if (!DFB_RECTANGLE_EQUAL( dst, sawwin->dst )) {
          if (!src_updated) {
               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE );
               if (stereo_layer) 
                    sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE | SWMUF_RIGHT_EYE );
          }

          sawwin->dst = dst;
          dst_updated = true;

          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE );
          if (stereo_layer) 
               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_NONE | SWMUF_RIGHT_EYE );
     }

     D_DEBUG_AT( SaWMan_Geometry, "  -> Updating children (associated windows)...\n" );

     fusion_vector_foreach (child, i, sawwin->children) {
          D_MAGIC_ASSERT( child, SaWManWindow );

          childwin = child->window;
          D_ASSERT( childwin != NULL );

          if ((childwin->config.src_geometry.mode == DWGM_FOLLOW && src_updated) ||
              (childwin->config.dst_geometry.mode == DWGM_FOLLOW && dst_updated) ||
              (childwin->config.options & DWOP_FOLLOW_BOUNDS))
               sawman_update_geometry( child );
     }

#ifndef OLD_COREWINDOWS_STRUCTURE
     D_DEBUG_AT( SaWMan_Geometry, "  -> Updating children (sub windows)...\n" );

     fusion_vector_foreach (childwin, i, window->subwindows) {
          D_ASSERT( childwin != NULL );

          sawman_update_geometry( childwin->window_data );
     }
#endif

     return DFB_OK;
}

DFBResult
sawman_restack_window( SaWMan                 *sawman,
                       SaWManWindow           *sawwin,
                       SaWManWindow           *relative,
                       int                     relation,
                       DFBWindowStackingClass  stacking )
{
     int           i;
     int           old;
     int           index;
     int           priority;
     StackData    *data;
     SaWManWindow *tmpsaw;
     CoreWindow   *window;
     SaWManTier   *tier;
     bool          stereo_layer;

#ifndef OLD_COREWINDOWS_STRUCTURE
     int           n;
     CoreWindow   *tmp;
#endif

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_MAGIC_ASSERT_IF( relative, SaWManWindow );
     D_ASSERT( sawwin->stack_data != NULL );

     D_ASSERT( relative == NULL || relative == sawwin || relation != 0);

     D_DEBUG_AT( SaWMan_Stacking, "%s( %p, %p, %p, %d, 0x%d )\n", __FUNCTION__,
                 sawman, sawwin, relative, relation, stacking );

     data   = sawwin->stack_data;
     window = sawwin->window;
     D_ASSERT( window != NULL );

     tier = sawman_tier_by_class( sawman, window->config.stacking );
     D_ASSERT( tier->region );
     stereo_layer = tier->region->config.options & DLOP_STEREO;

     /* Change stacking class. */
     if (stacking != window->config.stacking) {
          window->config.stacking = stacking;

          sawwin->priority = sawman_window_priority( sawwin );
     }

     /* Make sure window is inserted and not kept above/under parent. */
     if (!(sawwin->flags & SWMWF_INSERTED) || (window->config.options & (DWOP_KEEP_ABOVE|DWOP_KEEP_UNDER)))
          return DFB_OK;

     /* Get the (new) priority. */
     priority = sawwin->priority;

#ifndef OLD_COREWINDOWS_STRUCTURE
     /* In case of a sub window, the sub window vector is modified and the window reinserted */
     if (window->toplevel) {
          CoreWindow *toplevel = window->toplevel;

          /* Get the old index. */
          old = fusion_vector_index_of( &toplevel->subwindows, window );
          D_ASSERT( old >= 0 );

          D_DEBUG_AT( SaWMan_Stacking, "  -> old sub index %d\n", old );

          /* Calculate the desired index. */
          if (relative) {
               index = fusion_vector_index_of( &toplevel->subwindows, relative->window );
               if (index < 0)
                    return DFB_INVARG;

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
               else if (index > toplevel->subwindows.count - 1)
                    index = toplevel->subwindows.count - 1;
          }
          else if (relation)
               index = toplevel->subwindows.count - 1;
          else
               index = 0;

          D_DEBUG_AT( SaWMan_Stacking, "  -> new sub index %d\n", index );

          if (relation < 0) {
               while (index > 0) {
                    SaWManWindow *other = fusion_vector_at( &toplevel->subwindows, index );
                    SaWManWindow *under = fusion_vector_at( &toplevel->subwindows, index - 1 );

                    D_MAGIC_ASSERT( other, SaWManWindow );
                    D_MAGIC_ASSERT( under, SaWManWindow );

                    if ((other->window->config.options & DWOP_KEEP_ABOVE) ||
                        (under->window->config.options & DWOP_KEEP_UNDER))
                         index--;
                    else
                         break;
               }
          }
          else if (relation > 0) {
               while (index < toplevel->subwindows.count - 1) {
                    SaWManWindow *other = fusion_vector_at( &toplevel->subwindows, index );
                    SaWManWindow *above = fusion_vector_at( &toplevel->subwindows, index + 1 );

                    D_MAGIC_ASSERT( other, SaWManWindow );
                    D_MAGIC_ASSERT( above, SaWManWindow );

                    if ((above->window->config.options & DWOP_KEEP_ABOVE) ||
                        (other->window->config.options & DWOP_KEEP_UNDER))
                         index++;
                    else
                         break;
               }
          }

          D_DEBUG_AT( SaWMan_Stacking, "  -> new sub index %d\n", index );

          /* Return if index hasn't changed. */
          if (index == old)
               return DFB_OK;

          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
          if ( stereo_layer ) 
               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );

          /* Actually change the stacking order now. */
          fusion_vector_move( &toplevel->subwindows, old, index );

          /* Reinsert to move in layout as well. */
          sawman_insert_window( sawman, sawwin, NULL, false );

          /* Reinsert associated windows to ensure above/under rules apply. */
          fusion_vector_foreach (tmpsaw, i, sawwin->children) {
               if (tmpsaw->window->config.options & (DWOP_KEEP_ABOVE|DWOP_KEEP_UNDER)) {
                    sawman_update_window( sawman, tmpsaw, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
                    if ( stereo_layer ) 
                         sawman_update_window( sawman, tmpsaw, NULL, DSFLIP_NONE, 
                              SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                    sawman_insert_window( sawman, tmpsaw, NULL, false );
                    sawman_update_window( sawman, tmpsaw, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
                    if ( stereo_layer ) 
                         sawman_update_window( sawman, tmpsaw, NULL, DSFLIP_NONE, 
                              SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
               }
          }
     }
     else
#endif
     {
          /* Get the old index. */
          old = sawman_window_index( sawman, sawwin );
          D_ASSERT( old >= 0 );

          D_DEBUG_AT( SaWMan_Stacking, "  -> old index %d\n", old );

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

               if (relation > 0)
                    index += WINDOW_SUBWINDOWS_COUNT(window);

               if (index < 0)
                    index = 0;
               else if (index > sawman->layout.count - 1)
                    index = sawman->layout.count - 1;
          }
          else if (relation)
               index = sawman->layout.count - 1;
          else
               index = 0;

          D_DEBUG_AT( SaWMan_Stacking, "  -> new index %d\n", index );

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

          D_DEBUG_AT( SaWMan_Stacking, "  -> new index %d\n", index );

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

          D_DEBUG_AT( SaWMan_Stacking, "  -> new index %d\n", index );

          if (relation < 0) {
               /* Assure window won't be below a sub window (getting between sub and top or other sub window) */
               while (index > 0) {
                    SaWManWindow *other = fusion_vector_at( &sawman->layout, index );
                    SaWManWindow *under = fusion_vector_at( &sawman->layout, index - 1 );

                    D_MAGIC_ASSERT( other, SaWManWindow );
                    D_MAGIC_ASSERT( under, SaWManWindow );

                    if (WINDOW_TOPLEVEL(other->window) ||
                        (other->window->config.options & DWOP_KEEP_ABOVE) ||
                        (under->window->config.options & DWOP_KEEP_UNDER))
                         index--;
                    else
                         break;
               }
          }
          else if (relation > 0) {
               /* Assure window won't be below a sub window (getting between sub and top or other sub window) */
               while (index < sawman->layout.count - 1) {
                    SaWManWindow *other = fusion_vector_at( &sawman->layout, index );
                    SaWManWindow *above = fusion_vector_at( &sawman->layout, index + 1 );

                    D_MAGIC_ASSERT( other, SaWManWindow );
                    D_MAGIC_ASSERT( above, SaWManWindow );

                    if (WINDOW_TOPLEVEL(above->window) ||
                        (above->window->config.options & DWOP_KEEP_ABOVE) ||
                        (other->window->config.options & DWOP_KEEP_UNDER))
                         index++;
                    else
                         break;
               }
          }

          D_DEBUG_AT( SaWMan_Stacking, "  -> new index %d\n", index );

          /* Return if index hasn't changed. */
          if (index == old)
               return DFB_OK;

          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
          if ( stereo_layer ) 
               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, 
                                     SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );

          /* Actually change the stacking order now. */
          fusion_vector_move( &sawman->layout, old, index );

          D_DEBUG_AT( SaWMan_Stacking, "  -> now index %d\n", fusion_vector_index_of( &sawman->layout, sawwin ) );

#ifndef OLD_COREWINDOWS_STRUCTURE
          /* Reinsert sub windows to ensure they're in order (above top level). */
          fusion_vector_foreach (tmp, i, window->subwindows) {
               sawman_update_window( sawman, tmp->window_data, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
               if ( stereo_layer ) 
                    sawman_update_window( sawman, tmp->window_data, NULL, DSFLIP_NONE, 
                                          SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
               sawman_insert_window( sawman, tmp->window_data, NULL, false );
               sawman_update_window( sawman, tmp->window_data, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
               if ( stereo_layer ) 
                    sawman_update_window( sawman, tmp->window_data, NULL, DSFLIP_NONE, 
                                          SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
          }
#endif

          /* Reinsert associated windows to ensure above/under rules apply. */
          fusion_vector_foreach (tmpsaw, i, sawwin->children) {
               if (tmpsaw->window->config.options & (DWOP_KEEP_ABOVE|DWOP_KEEP_UNDER)) {
                    sawman_update_window( sawman, tmpsaw, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
                    if ( stereo_layer ) 
                         sawman_update_window( sawman, tmpsaw, NULL, DSFLIP_NONE, 
                                               SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                    sawman_insert_window( sawman, tmpsaw, NULL, false );
                    sawman_update_window( sawman, tmpsaw, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
                    if ( stereo_layer ) 
                         sawman_update_window( sawman, tmpsaw, NULL, DSFLIP_NONE, 
                                               SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );

#ifndef OLD_COREWINDOWS_STRUCTURE
                    /* Reinsert sub windows to ensure they're in order (above top level). */
                    fusion_vector_foreach (tmp, n, tmpsaw->window->subwindows) {
                         sawman_update_window( sawman, tmp->window_data, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
                         if ( stereo_layer ) 
                              sawman_update_window( sawman, tmp->window_data, NULL, DSFLIP_NONE, 
                                                    SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                         sawman_insert_window( sawman, tmp->window_data, NULL, false );
                         sawman_update_window( sawman, tmp->window_data, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
                         if ( stereo_layer ) 
                              sawman_update_window( sawman, tmp->window_data, NULL, DSFLIP_NONE, 
                                                    SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
                    }
#endif
               }
          }
     }

     sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER );
     if ( stereo_layer ) 
          sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );

     return DFB_OK;
}

DirectResult
sawman_set_stereo_depth( SaWMan       *sawman,
                         SaWManWindow *sawwin,
                         int           z )
{
     int              old_z;
     StackData       *data;
     CoreWindowStack *stack;
     CoreWindow      *window;
     SaWManTier      *tier;
     DFBRegion        region;
     int              dz;
     int              index;
     SaWManWindow    *relative;


     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( sawwin->stack_data != NULL );
     D_ASSERT( sawwin->stack != NULL );
     D_ASSERT( sawwin->window != NULL );

     data           = sawwin->stack_data;
     stack          = sawwin->stack;
     window         = sawwin->window;
     old_z          = window->config.z;
     tier           = sawman_tier_by_class( sawman, window->config.stacking );
     D_ASSERT(tier->region);

     if (old_z != z) {
          window->config.z = z;

          if (sawwin->flags & SWMWF_INSERTED) {
               index = sawman_window_index( sawman, sawwin );
               if (z > old_z) {
                    if (++index < fusion_vector_size( &sawman->layout )) {
                         relative = fusion_vector_at( &sawman->layout, index );
                         if (relative->window->config.z < z) {
                              sawman_restack_window( sawman, sawwin, relative, 1, window->config.stacking );
                         }
                    }
               }
               else
               {
                    if (--index >= 0) {
                         relative = fusion_vector_at( &sawman->layout, index );
                         if (relative->window->config.z > z) {
                              sawman_restack_window( sawman, sawwin, relative, -1, window->config.stacking );
                         }
                    }
               }

               dz = z - old_z;
               region.y1 = 0;
               region.y2 = sawwin->bounds.h - 1;

               if (dz > 0) {
                    region.x1 = -dz;
                    region.x2 = sawwin->bounds.w - 1;
               }
               else {
                    region.x1 = 0;
                    region.x2 = sawwin->bounds.w - 1 - dz;
               }
               sawman_update_window( sawman, sawwin, &region, DSFLIP_NONE, SWMUF_UPDATE_BORDER );

               if (tier->region->config.options & DLOP_STEREO) {
                    if (dz > 0) {
                         region.x1 = 0;
                         region.x2 = sawwin->bounds.w - 1 + dz;
                    }
                    else {
                         region.x1 = dz;
                         region.x2 = sawwin->bounds.w - 1;
                    }
                    sawman_update_window( sawman, sawwin, &region, DSFLIP_NONE, SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );
               }
          }
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

DirectResult
sawman_set_opacity( SaWMan       *sawman,
                    SaWManWindow *sawwin,
                    u8            opacity )
{
     u8               old;
     StackData       *data;
     CoreWindow      *window;
     CoreWindowStack *stack;
     SaWManTier      *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( sawwin->stack_data != NULL );
     D_ASSERT( sawwin->stack != NULL );
     D_ASSERT( sawwin->window != NULL );

     data   = sawwin->stack_data;
     window = sawwin->window;
     old    = window->config.opacity;
     stack  = sawwin->stack;
     tier   = sawman_tier_by_class(sawman, window->config.stacking);
     D_ASSERT(tier->region);

     if (!dfb_config->translucent_windows && opacity)
          opacity = 0xFF;

     if (old != opacity) {
          window->config.opacity = opacity;

          if (sawwin->flags & SWMWF_INSERTED) {
               sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, SWMUF_FORCE_INVISIBLE | SWMUF_UPDATE_BORDER );

               if (tier->region->config.options & DLOP_STEREO) 
                    sawman_update_window( sawman, sawwin, NULL, DSFLIP_NONE, 
                         SWMUF_FORCE_INVISIBLE | SWMUF_UPDATE_BORDER | SWMUF_RIGHT_EYE );

               /* Ungrab pointer/keyboard, pass focus... */
               if (old && !opacity) {
                    /* Possibly switch focus to window now under the cursor */
                    if (sawman->focused_window == sawwin)
                         sawman_update_focus( sawman, data->stack, data->stack->cursor.x, data->stack->cursor.y );

                    sawman_withdraw_window( sawman, sawwin );
               }
          }
     }

     return DFB_OK;
}

DirectResult
sawman_window_set_cursor_flags( SaWMan                *sawman,
                                SaWManWindow          *sawwin,
                                DFBWindowCursorFlags   flags )
{
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

     if (window->config.cursor_flags != flags) {
          window->config.cursor_flags = flags;

          if (sawwin == sawman->focused_window) {
               CoreCursorUpdateFlags update  = CCUF_NONE;
               u8                    opacity = (flags & DWCF_INVISIBLE) ? 0x00 : 0xff;

               if (!(flags & DWCF_UNCLIPPED)) {
                    int cx = stack->cursor.x;
                    int cy = stack->cursor.y;

                    if (cx < 0)
                         cx = 0;
                    else if (cx >= sawman->resolution.w)
                         cx = sawman->resolution.w - 1;

                    if (cy < 0)
                         cy = 0;
                    else if (cy >= sawman->resolution.h)
                         cy = sawman->resolution.h - 1;

                    if (cx != stack->cursor.x || cy != stack->cursor.y) {
                         D_DEBUG_AT( SaWMan_Cursor, "  -> Cursor clipped %d,%d -> %d,%d\n",
                                     stack->cursor.x, stack->cursor.y, cx, cy );
                                    printf( "===JK sawman  -> Cursor clipped %d,%d -> %d,%d\n",
                                     stack->cursor.x, stack->cursor.y, cx, cy );

                         stack->cursor.x = cx;
                         stack->cursor.y = cy;

                         update |= CCUF_POSITION;
                    }
               }

               if (stack->cursor.opacity != opacity) {
                    stack->cursor.opacity = opacity;

                    update |= CCUF_OPACITY;
               }

               if (update)
                    dfb_wm_update_cursor( stack, update );
          }
     }

     return DFB_OK;
}

bool
sawman_update_focus( SaWMan          *sawman,
                     CoreWindowStack *stack,
                     int              x,
                     int              y )
{
     D_MAGIC_ASSERT( sawman, SaWMan );

     /* if pointer is not grabbed */
     if (!sawman->pointer_window) {
          SaWManWindow *before = sawman->entered_window;
          SaWManWindow *after  = sawman_window_at_pointer( sawman, stack, x, y );

          /* and the window under the cursor is another one now */
          if (before != after) {
               DFBWindowEvent we;

               /* send leave event */
               if (before) {
                    D_MAGIC_ASSERT( before, SaWManWindow );
                    D_ASSERT( before->window != NULL );

                    we.type = DWET_LEAVE;

                    sawman_window_get_cursor_position( sawman, stack, before, &we.x, &we.y, &we.cx, &we.cy );

                    sawman_post_event( sawman, before, &we );
               }

               /* switch focus and send enter event */
               sawman_switch_focus( sawman, after );

               if (after) {
                    D_MAGIC_ASSERT( after, SaWManWindow );
                    D_ASSERT( after->window != NULL );

                    we.type = DWET_ENTER;

                    sawman_window_get_cursor_position( sawman, stack, after, &we.x, &we.y, &we.cx, &we.cy );

                    sawman_post_event( sawman, after, &we );
               }

               /* update pointer to window under the cursor */
               sawman->entered_window = after;

               return true;
          }
     }

     return false;
}

SaWManWindow*
sawman_window_at_pointer( SaWMan          *sawman,
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
          SaWManTier *tier;
          int         tx, ty;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );
          window = sawwin->window;
          D_ASSERT( window != NULL );

          /* Retrieve corresponding SaWManTier. */
          tier = sawman_tier_by_class( sawman, sawwin->window->config.stacking );
          D_MAGIC_ASSERT( tier, SaWManTier );

          /* Convert to Tier coordinates */
          tx = (s64) x * (s64) tier->size.w / (s64) sawman->resolution.w;
          ty = (s64) y * (s64) tier->size.h / (s64) sawman->resolution.h;

          if (!(window->config.options & DWOP_GHOST) && window->config.opacity &&
              tx >= sawwin->bounds.x  &&  tx < sawwin->bounds.x + sawwin->bounds.w &&
              ty >= sawwin->bounds.y  &&  ty < sawwin->bounds.y + sawwin->bounds.h)
               return sawwin;
     }

     return NULL;
}

void
sawman_window_get_cursor_position( SaWMan          *sawman,
                                   CoreWindowStack *stack,
                                   SaWManWindow    *sawwin,
                                   int             *ret_x,
                                   int             *ret_y,
                                   int             *ret_cx,
                                   int             *ret_cy )
{
     int         x, y;
     int         cx, cy;
     int         sx, sy;
     SaWManTier *tier;

     D_DEBUG_AT( SaWMan_Cursor, "%s()\n", __func__ );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( stack != NULL );
     D_MAGIC_ASSERT( sawwin, SaWManWindow );
     D_ASSERT( sawwin->window != NULL );

     /* Retrieve corresponding SaWManTier. */
     tier = sawman_tier_by_class( sawman, sawwin->window->config.stacking );
     D_MAGIC_ASSERT( tier, SaWManTier );

     x = stack->cursor.x;
     y = stack->cursor.y;

     /* Convert to Tier coordinates */
     cx = (s64) x * (s64) tier->size.w / (s64) sawman->resolution.w;
     cy = (s64) y * (s64) tier->size.h / (s64) sawman->resolution.h;

     /* Subtract offset of Window within layout (tier coordinates) */
     x = cx - sawwin->dst.x;
     y = cy - sawwin->dst.y;

     /* Convert to Window coordinates */
     sx = sawwin->window->config.cursor_resolution.w ?: sawwin->src.w;
     sy = sawwin->window->config.cursor_resolution.h ?: sawwin->src.h;

     x = x * sx / sawwin->dst.w;
     y = y * sy / sawwin->dst.h;

     cx = cx * sx / sawwin->dst.w;
     cy = cy * sy / sawwin->dst.h;

     if (ret_x)
          *ret_x = x;

     if (ret_y)
          *ret_y = y;

     if (ret_cx)
          *ret_cx = cx;

     if (ret_cy)
          *ret_cy = cy;

     D_DEBUG_AT( SaWMan_Cursor, "  => %d,%d - %d,%d\n", x, y, cx, cy );
}

int
sawman_window_border( const SaWManWindow *sawwin )
{
     SaWMan                 *sawman;
     const CoreWindow       *window;
     const SaWManTier       *tier;
     const SaWManBorderInit *border;
     int                     thickness = 0;

     D_MAGIC_ASSERT( sawwin, SaWManWindow );

     sawman = sawwin->sawman;
     D_MAGIC_ASSERT( sawman, SaWMan );
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     if (sawwin->caps & DWCAPS_NODECORATION)
          return 0;

     window = sawwin->window;
     D_MAGIC_COREWINDOW_ASSERT( window );

     tier = sawman_tier_by_class( sawwin->sawman, window->config.stacking );
     D_MAGIC_ASSERT( tier, SaWManTier );

     D_ASSERT( sawman_config != NULL );

     border = &sawman_config->borders[sawman_window_priority(sawwin)];

     thickness = border->thickness;
     if (thickness && border->resolution.w && border->resolution.h) {
          if (border->resolution.w != tier->size.w && border->resolution.h != tier->size.h) {
               int tw = thickness * tier->size.w / border->resolution.w;
               int th = thickness * tier->size.h / border->resolution.h;

               thickness = (tw + th + 1) / 2;
          }
     }

     return thickness;
}

void
sawman_update_visible( SaWMan *sawman )
{
     int         i;
     SaWManTier *tier;

     D_DEBUG_AT( SaWMan_Update, "%s()\n", __FUNCTION__ );

     dfb_updates_reset( &sawman->bg.visible );

     for (i=0; i<sawman->layout.count; i++) {
          SaWManWindow *window = sawman->layout.elements[i];

          D_MAGIC_ASSERT( window, SaWManWindow );

          dfb_updates_reset( &window->left.visible );
          dfb_updates_reset( &window->right.visible );
     }

     tier = (SaWManTier*) sawman->tiers;     // FIXME: using lowest tier for max resolution
     D_MAGIC_ASSERT( tier, SaWManTier );

     update_visible( sawman, sawman->layout.count - 1, 0, 0, tier->size.w, tier->size.h, false );
     update_visible( sawman, sawman->layout.count - 1, 0, 0, tier->size.w, tier->size.h, true );

#if D_DEBUG_ENABLED
     for (i=0; i<sawman->layout.count; i++) {
          SaWManWindow *window = sawman->layout.elements[i];

          D_MAGIC_ASSERT( window, SaWManWindow );

          D_DEBUG_AT( SaWMan_Update, "  -> [%2d] window %p, left %d, right %d\n", i, window,
                      window->left.visible.num_regions, window->right.visible.num_regions );

          if (window->left.visible.num_regions)
               D_DEBUG_AT( SaWMan_Update, "     = [%4d,%4d %4dx%4d] left\n",
                           DFB_RECTANGLE_VALS_FROM_REGION(&window->left.visible.bounding) );

          if (window->left.visible.num_regions > 1) {
               int n;

               for (n=0; n<window->left.visible.num_regions; n++) {
                    D_DEBUG_AT( SaWMan_Update, "      . %4d,%4d %4dx%4d\n",
                                DFB_RECTANGLE_VALS_FROM_REGION(&window->left.visible.regions[n]) );
               }
          }

          if (window->right.visible.num_regions)
               D_DEBUG_AT( SaWMan_Update, "     = [%4d,%4d %4dx%4d] right\n",
                           DFB_RECTANGLE_VALS_FROM_REGION(&window->right.visible.bounding) );

          if (window->right.visible.num_regions > 1) {
               int n;

               for (n=0; n<window->right.visible.num_regions; n++) {
                    D_DEBUG_AT( SaWMan_Update, "      . %4d,%4d %4dx%4d\n",
                                DFB_RECTANGLE_VALS_FROM_REGION(&window->right.visible.regions[n]) );
               }
          }
     }
#endif
}

/**********************************************************************************************************************/

/*
     skipping opaque windows that are above the window that changed
*/
static void
wind_of_change( SaWMan              *sawman,
                SaWManTier          *tier,
                const DFBRegion     *update,
                DFBSurfaceFlipFlags  flags,
                int                  current,
                int                  changed,
                bool                 right_eye )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_ASSERT( update != NULL );

     /*
          loop through windows above
     */
     for (; current > changed; current--) {
          CoreWindow       *window;
          SaWManWindow     *sawwin;
          DFBRegion         opaque;
          DFBWindowOptions  options;
          int               stereo_offset;

          D_ASSERT( changed >= 0 );
          D_ASSERT( current >= changed );
          D_ASSERT( current < fusion_vector_size( &sawman->layout ) );

          sawwin = fusion_vector_at( &sawman->layout, current );
          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          options = window->config.options;

          stereo_offset = right_eye ? -window->config.z : window->config.z;

          D_DEBUG_AT( SaWMan_Update, "--[%p] %4d,%4d-%4dx%4d : %d->%d\n",
                      tier, DFB_RECTANGLE_VALS_FROM_REGION( update ), current, changed );

          /*
               can skip opaque region
          */
          if ((tier->classes & (1 << window->config.stacking)) && (  (
              //can skip all opaque window?
              (window->config.opacity == 0xff) &&
              !(window->caps & DWCAPS_INPUTONLY) &&
              !(options & (DWOP_INPUTONLY | DWOP_COLORKEYING | DWOP_ALPHACHANNEL)) &&
              (opaque=*update,dfb_region_intersect( &opaque,
                                                    sawwin->dst.x + stereo_offset, sawwin->dst.y,
                                                    sawwin->dst.x + stereo_offset + sawwin->dst.w - 1,
                                                    sawwin->dst.y + sawwin->dst.h - 1 ) )
              )||(
                 //can skip opaque region?
                 (options & DWOP_ALPHACHANNEL) &&
                 (options & DWOP_OPAQUE_REGION) &&
                 (window->config.opacity == 0xff) &&
                 !(options & DWOP_COLORKEYING) &&
                 (opaque=*update,dfb_region_intersect( &opaque,  /* FIXME: Scaling */
                                                       sawwin->dst.x + stereo_offset + window->config.opaque.x1,
                                                       sawwin->dst.y + window->config.opaque.y1,
                                                       sawwin->dst.x + stereo_offset + window->config.opaque.x2,
                                                       sawwin->dst.y + window->config.opaque.y2 ))
                 )  ))
          {
               /* left */
               if (opaque.x1 != update->x1) {
                    DFBRegion left = { update->x1, opaque.y1, opaque.x1-1, opaque.y2};
                    wind_of_change( sawman, tier, &left, flags, current-1, changed, right_eye );
               }
               /* upper */
               if (opaque.y1 != update->y1) {
                    DFBRegion upper = { update->x1, update->y1, update->x2, opaque.y1-1};
                    wind_of_change( sawman, tier, &upper, flags, current-1, changed, right_eye );
               }
               /* right */
               if (opaque.x2 != update->x2) {
                    DFBRegion right = { opaque.x2+1, opaque.y1, update->x2, opaque.y2};
                    wind_of_change( sawman, tier, &right, flags, current-1, changed, right_eye );
               }
               /* lower */
               if (opaque.y2 != update->y2) {
                    DFBRegion lower = { update->x1, opaque.y2+1, update->x2, update->y2};
                    wind_of_change( sawman, tier, &lower, flags, current-1, changed, right_eye );
               }

               return;
          }
     }

     D_DEBUG_AT( SaWMan_Update, "+ UPDATE %4d,%4d-%4dx%4d\n",
                 DFB_RECTANGLE_VALS_FROM_REGION( update ) );

     dfb_updates_add( right_eye ? &tier->right.updates : &tier->left.updates, update );
}

static void
wind_of_showing( SaWMan     *sawman,
                 SaWManTier *tier,
                 DFBRegion  *update,
                 int         current,
                 int         changed,
                 bool       *ret_showing )
{
     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_ASSERT( update != NULL );

     if (*ret_showing)
          return;

     /*
          loop through windows above
     */
     for (; current > changed; current--) {
          CoreWindow       *window;
          SaWManWindow     *sawwin;
          DFBRegion         opaque;
          DFBWindowOptions  options;

          D_ASSERT( changed >= 0 );
          D_ASSERT( current >= changed );
          D_ASSERT( current < fusion_vector_size( &sawman->layout ) );

          sawwin = fusion_vector_at( &sawman->layout, current );
          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          options = window->config.options;

          /*
               can skip opaque region
          */
          if ((tier->classes & (1 << window->config.stacking)) && (  (
              //can skip all opaque window?
              (window->config.opacity == 0xff) &&
              !(options & (DWOP_COLORKEYING | DWOP_ALPHACHANNEL)) &&
              (opaque=*update,dfb_region_intersect( &opaque,
                                                    sawwin->dst.x, sawwin->dst.y,
                                                    sawwin->dst.x + sawwin->dst.w - 1,
                                                    sawwin->dst.y + sawwin->dst.h - 1 ) )
              )||(
                 //can skip opaque region?
                 (options & DWOP_ALPHACHANNEL) &&
                 (options & DWOP_OPAQUE_REGION) &&
                 (window->config.opacity == 0xff) &&
                 !(options & DWOP_COLORKEYING) &&
                 (opaque=*update,dfb_region_intersect( &opaque,  /* FIXME: Scaling */
                                                       sawwin->dst.x + window->config.opaque.x1,
                                                       sawwin->dst.y + window->config.opaque.y1,
                                                       sawwin->dst.x + window->config.opaque.x2,
                                                       sawwin->dst.y + window->config.opaque.y2 ))
                 )  ))
          {
               /* left */
               if (opaque.x1 != update->x1) {
                    DFBRegion left = { update->x1, opaque.y1, opaque.x1-1, opaque.y2};
                    wind_of_showing( sawman, tier, &left,  current-1, changed, ret_showing );
               }
               /* upper */
               if (opaque.y1 != update->y1) {
                    DFBRegion upper = { update->x1, update->y1, update->x2, opaque.y1-1};
                    wind_of_showing( sawman, tier, &upper, current-1, changed, ret_showing );
               }
               /* right */
               if (opaque.x2 != update->x2) {
                    DFBRegion right = { opaque.x2+1, opaque.y1, update->x2, opaque.y2};
                    wind_of_showing( sawman, tier, &right, current-1, changed, ret_showing );
               }
               /* lower */
               if (opaque.y2 != update->y2) {
                    DFBRegion lower = { update->x1, opaque.y2+1, update->x2, update->y2};
                    wind_of_showing( sawman, tier, &lower, current-1, changed, ret_showing );
               }

               return;
          }
     }

     *ret_showing = true;
}

static void
update_visible( SaWMan *sawman,
                int     start,
                int     x1,
                int     y1,
                int     x2,
                int     y2,
                bool    right_eye )
{
     int           i      = start;
     DFBRegion     region = { x1, y1, x2, y2 };
     CoreWindow   *window = NULL;
     SaWManWindow *sawwin = NULL;
     int           stereo_offset = 0;

     D_DEBUG_AT( SaWMan_Update, "%s( %d, %d,%d - %d,%d )\n", __FUNCTION__, start, x1, y1, x2, y2 );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_ASSERT( start < fusion_vector_size( &sawman->layout ) );
     D_ASSUME( x1 <= x2 );
     D_ASSUME( y1 <= y2 );

     if (x1 > x2 || y1 > y2)
          return;

     /* Find next intersecting window. */
     while (i >= 0) {
          sawwin = fusion_vector_at( &sawman->layout, i );
          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          stereo_offset = right_eye ? -window->config.z : window->config.z;

          if (SAWMAN_VISIBLE_WINDOW( window )) {
               if (dfb_region_intersect( &region,
                                         sawwin->bounds.x + stereo_offset, sawwin->bounds.y,
                                         sawwin->bounds.x + stereo_offset + sawwin->bounds.w - 1,
                                         sawwin->bounds.y + sawwin->bounds.h - 1))
                    break;
          }

          i--;
     }

     /* Intersecting window found? */
     if (i >= 0) {
          D_MAGIC_ASSERT( sawwin, SaWManWindow );
          D_MAGIC_COREWINDOW_ASSERT( window );

          if (SAWMAN_TRANSLUCENT_WINDOW( window )) {
               /* draw everything below */
               update_visible( sawman, i-1, x1, y1, x2, y2, right_eye );
          }
          else {
               DFBRegion dst = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->dst );

               dfb_region_translate( &dst, stereo_offset, 0 );

               dfb_region_region_intersect( &dst, &region );

               /* left */
               if (dst.x1 != x1)
                    update_visible( sawman, i-1, x1, dst.y1, dst.x1-1, dst.y2, right_eye );

               /* upper */
               if (dst.y1 != y1)
                    update_visible( sawman, i-1, x1, y1, x2, dst.y1-1, right_eye );

               /* right */
               if (dst.x2 != x2)
                    update_visible( sawman, i-1, dst.x2+1, dst.y1, x2, dst.y2, right_eye );

               /* lower */
               if (dst.y2 != y2)
                    update_visible( sawman, i-1, x1, dst.y2+1, x2, y2, right_eye );
          }

          dfb_updates_add( right_eye ? &sawwin->right.visible : &sawwin->left.visible, &region );
     }
     else
          dfb_updates_add( &sawman->bg.visible, &region );
}

