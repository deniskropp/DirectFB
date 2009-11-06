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

#include <gfx/clip.h>
#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/conf.h>

#include <sawman.h>

#include "sawman_config.h"
#include "sawman_draw.h"
#include "sawman_window.h"

#include "isawman.h"

#include "region.h"


D_DEBUG_DOMAIN( SaWMan_Auto,     "SaWMan/Auto",     "SaWMan auto configuration" );
D_DEBUG_DOMAIN( SaWMan_Update,   "SaWMan/Update",   "SaWMan window manager updates" );

/**********************************************************************************************************************/

/* put the update related functions here till we sort out a better place */

typedef struct {
     DirectLink link;
     DFBRegion  region;
} DFBLinkRegion;

typedef struct {
     DFBLinkRegion *regions;
     int number;
     int free;
} DFBLinkRegionPool;

static inline void dfb_linkregionpool_init( DFBLinkRegionPool *pool, DFBLinkRegion *regions, int number )
{
     pool->regions = regions;
     pool->number  = number;
     pool->free    = 0;
}

static inline DFBLinkRegion *dfb_linkregionpool_get( DFBLinkRegionPool *pool, DFBRegion *r )
{
     DFBLinkRegion *lr;

     if (pool->free == pool->number) {
          D_WARN("Out of link regions!!\n");
          return NULL;
     }

     lr = pool->regions + pool->free;
     pool->free++;

     if (r) 
          lr->region = *r;

     lr->link.magic = 0;
     return lr;
}

/* Add only the allowed part of region "region" to the region list "list". 
 * This creates at most 4 new regions, on all sides of "forbidden".
 */
static inline void dfb_linkregionpool_add_allowedpartofregion( DFBLinkRegionPool *pool,
                                                               DirectLink **list, DFBRegion *region,
                                                               DFBRegion *forbidden )
{
     DFBRegion *r = forbidden;

     if (region->y1 < r->y1) { /* up */
          DFBLinkRegion *lr = dfb_linkregionpool_get( pool, region );
          lr->region.y2 = r->y1-1;
          direct_list_append( list, &lr->link );
     }
     if (region->y2 > r->y2) { /* down */
          DFBLinkRegion *lr = dfb_linkregionpool_get( pool, region );
          lr->region.y1 = r->y2+1;
          direct_list_append( list, &lr->link );
     }
     if (region->x1 < r->x1) { /* left */
          DFBLinkRegion *lr = dfb_linkregionpool_get( pool, region );
          lr->region.x2 = r->x1-1;
          if (r->y1 > region->y1)
               lr->region.y1 = r->y1;
          if (r->y2 < region->y2)
               lr->region.y2 = r->y2;
          direct_list_append( list, &lr->link );
     }
     if (region->x2 > r->x2) { /* right */
          DFBLinkRegion *lr = dfb_linkregionpool_get( pool, region );
          lr->region.x1 = r->x2+1;
          if (r->y1 > region->y1)
               lr->region.y1 = r->y1;
          if (r->y2 < region->y2)
               lr->region.y2 = r->y2;
          direct_list_append( list, &lr->link );
     }
}

/* find combinable regions.
 * We assume all rectangles are non-overlapping.
 * We look for:
 *   (1) rectangle to the right, connecting in height
 *   (2) rectangle to the bottom, connecting in width
 * top/left is caught too by this principle
 * 
 * We sort the list in-place: entries will be removed only.
 * We never remove the first entry.
 */
static void collate( DirectLink **updates )
{
     DFBLinkRegion *linkRegion, *lr;

     collate_restart:

     direct_list_foreach(linkRegion, *updates) {
          /* compare against item+1..last */
          lr = (DFBLinkRegion*)(linkRegion->link.next);
          while (lr) {
               /* to the right */
               if (    (linkRegion->region.y1 == lr->region.y1)
                    && (linkRegion->region.y2 == lr->region.y2)
                    && (linkRegion->region.x2 == lr->region.x1 - 1) ) {
                    /* fold */
                    linkRegion->region.x2 = lr->region.x2;
                    direct_list_remove( updates, &lr->link );
                    goto collate_restart;
               }
               /* to the bottom */
               if (    (linkRegion->region.x1 == lr->region.x1)
                    && (linkRegion->region.x2 == lr->region.x2)
                    && (linkRegion->region.y2 == lr->region.y1 - 1) ) {
                    /* fold */
                    linkRegion->region.y2 = lr->region.y2;
                    direct_list_remove( updates, &lr->link );
                    goto collate_restart;
               }
               lr = (DFBLinkRegion*)(lr->link.next);
          }
     }
}


/**********************************************************************************************************************/

static void
update_region( SaWMan          *sawman,
               SaWManTier      *tier,
               CardState       *state,
               int              start,
               int              x1,
               int              y1,
               int              x2,
               int              y2 )
{
     int           i      = start;
     DFBRegion     region = { x1, y1, x2, y2 };
     CoreWindow   *window = NULL;
     SaWManWindow *sawwin = NULL;

     D_DEBUG_AT( SaWMan_Update, "%s( %p, %d, %d,%d - %d,%d )\n", __FUNCTION__, tier, start, x1, y1, x2, y2 );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_MAGIC_ASSERT( state, CardState );
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

          if (SAWMAN_VISIBLE_WINDOW( window ) && (tier->classes & (1 << window->config.stacking))) {
               if (dfb_region_intersect( &region,
                                         DFB_REGION_VALS_FROM_RECTANGLE( &sawwin->bounds )))
                    break;
          }

          i--;
     }

     /* Intersecting window found? */
     if (i >= 0) {
          D_MAGIC_ASSERT( sawwin, SaWManWindow );
          D_MAGIC_COREWINDOW_ASSERT( window );

          if (D_FLAGS_ARE_SET( window->config.options, DWOP_ALPHACHANNEL | DWOP_OPAQUE_REGION )) {
               DFBRegion opaque = DFB_REGION_INIT_TRANSLATED( &window->config.opaque,
                                                              sawwin->bounds.x,
                                                              sawwin->bounds.y );

               if (!dfb_region_region_intersect( &opaque, &region )) {
                    update_region( sawman, tier, state, i-1, x1, y1, x2, y2 );

                    sawman_draw_window( tier, sawwin, state, &region, true );
               }
               else {
                    if ((window->config.opacity < 0xff) || (window->config.options & DWOP_COLORKEYING)) {
                         /* draw everything below */
                         update_region( sawman, tier, state, i-1, x1, y1, x2, y2 );
                    }
                    else {
                         /* left */
                         if (opaque.x1 != x1)
                              update_region( sawman, tier, state, i-1, x1, opaque.y1, opaque.x1-1, opaque.y2 );

                         /* upper */
                         if (opaque.y1 != y1)
                              update_region( sawman, tier, state, i-1, x1, y1, x2, opaque.y1-1 );

                         /* right */
                         if (opaque.x2 != x2)
                              update_region( sawman, tier, state, i-1, opaque.x2+1, opaque.y1, x2, opaque.y2 );

                         /* lower */
                         if (opaque.y2 != y2)
                              update_region( sawman, tier, state, i-1, x1, opaque.y2+1, x2, y2 );
                    }

                    /* left */
                    if (opaque.x1 != region.x1) {
                         DFBRegion r = { region.x1, opaque.y1, opaque.x1 - 1, opaque.y2 };
                         sawman_draw_window( tier, sawwin, state, &r, true );
                    }

                    /* upper */
                    if (opaque.y1 != region.y1) {
                         DFBRegion r = { region.x1, region.y1, region.x2, opaque.y1 - 1 };
                         sawman_draw_window( tier, sawwin, state, &r, true );
                    }

                    /* right */
                    if (opaque.x2 != region.x2) {
                         DFBRegion r = { opaque.x2 + 1, opaque.y1, region.x2, opaque.y2 };
                         sawman_draw_window( tier, sawwin, state, &r, true );
                    }

                    /* lower */
                    if (opaque.y2 != region.y2) {
                         DFBRegion r = { region.x1, opaque.y2 + 1, region.x2, region.y2 };
                         sawman_draw_window( tier, sawwin, state, &r, true );
                    }

                    /* inner */
                    sawman_draw_window( tier, sawwin, state, &opaque, false );
               }
          }
          else {
               if (SAWMAN_TRANSLUCENT_WINDOW( window )) {
                    /* draw everything below */
                    update_region( sawman, tier, state, i-1, x1, y1, x2, y2 );
               }
               else {
                    DFBRegion dst = DFB_REGION_INIT_FROM_RECTANGLE( &sawwin->dst );

                    dfb_region_region_intersect( &dst, &region );

                    /* left */
                    if (dst.x1 != x1)
                         update_region( sawman, tier, state, i-1, x1, dst.y1, dst.x1-1, dst.y2 );

                    /* upper */
                    if (dst.y1 != y1)
                         update_region( sawman, tier, state, i-1, x1, y1, x2, dst.y1-1 );

                    /* right */
                    if (dst.x2 != x2)
                         update_region( sawman, tier, state, i-1, dst.x2+1, dst.y1, x2, dst.y2 );

                    /* lower */
                    if (dst.y2 != y2)
                         update_region( sawman, tier, state, i-1, x1, dst.y2+1, x2, y2 );
               }

               sawman_draw_window( tier, sawwin, state, &region, true );
          }
     }
     else
          sawman_draw_background( tier, state, &region );
}

static void
update_region2( SaWMan          *sawman,
               SaWManTier      *tier,
               CardState       *state,
               int              start,
               int              x1,
               int              y1,
               int              x2,
               int              y2 )
{
     int              i;
     SaWManWindow    *sawwin;
     CoreWindowStack *stack;
     misc_region_t    dirty;
     int              num, n;
     DFBBox          *boxes;
     DFBBox           extents = { x1, y1, x2 + 1, y2 + 1 };

     D_DEBUG_AT( SaWMan_Update, "%s( %p, %d, %d,%d - %d,%d )\n", __FUNCTION__, tier, start, x1, y1, x2, y2 );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( start < fusion_vector_size( &sawman->layout ) );
     D_ASSUME( x1 <= x2 );
     D_ASSUME( y1 <= y2 );

     if (x1 > x2 || y1 > y2)
          return;

     stack = tier->stack;
     D_ASSERT( stack != NULL );

     misc_region_init_with_extents( &dirty, NULL, &extents );

     fusion_vector_foreach (sawwin, i, sawman->layout) {
          CoreWindow *window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_ASSERT( window, CoreWindow );

          if (SAWMAN_VISIBLE_WINDOW( window ) && (tier->classes & (1 << window->config.stacking))) {
               misc_region_t visible;
               misc_region_t render;
               misc_region_t opt;

               D_DEBUG_AT( SaWMan_Update, " -=> [%d] <=-\n", i );

               MISC_REGION_DEBUG_AT( SaWMan_Update, &dirty, "dirty" );
     
               /* visible (window) */
               misc_region_init_updates( &visible, NULL, &sawwin->visible );
     
               /* render (extents) */
               misc_region_init_with_extents( &render, NULL, &extents );
     
               misc_region_init( &opt, NULL );
     
               /* render = visible & render(extents) */
               misc_region_intersect( &render, &visible, &render );
     
               /* opt = render & dirty */
               misc_region_intersect( &opt, &render, &dirty );
     
               if (window->config.opacity == 0xff &&
                   window->surface && (window->surface->config.caps & DSCAPS_PREMULTIPLIED) &&
                   (window->config.options & DWOP_ALPHACHANNEL) && !(window->config.options & DWOP_COLORKEYING) &&
                   (window->config.dst_geometry.mode == DWGM_DEFAULT) && stack->bg.mode == DLBM_COLOR &&
                   !stack->bg.color.a && !stack->bg.color.r && !stack->bg.color.g && !stack->bg.color.b)
               {
                    misc_region_t blend;
     
                    misc_region_init( &blend, NULL );
     
                    /* blend = render - opt */
                    misc_region_subtract( &blend, &render, &opt );
     
                    MISC_REGION_DEBUG_AT( SaWMan_Update, &visible, "visible" );
                    MISC_REGION_DEBUG_AT( SaWMan_Update, &render, "render" );
                    MISC_REGION_DEBUG_AT( SaWMan_Update, &opt, "opt" );
                    MISC_REGION_DEBUG_AT( SaWMan_Update, &blend, "blend" );
     
                    /////// FIXME: use batch blit!
     
                    /*
                     * Draw optimized window areas
                     */
                    boxes = misc_region_boxes( &opt, &num );
     
                    for (n=0; n<num; n++) {
                         DFBRegion draw = { boxes[n].x1, boxes[n].y1, boxes[n].x2 - 1, boxes[n].y2 - 1 };
     
                         sawman_draw_window( tier, sawwin, state, &draw, false );
                    }
     
     
                    /////// FIXME: use batch blit!
     
                    /*
                     * Draw blended window areas
                     */
                    boxes = misc_region_boxes( &blend, &num );
     
                    for (n=0; n<num; n++) {
                         DFBRegion draw = { boxes[n].x1, boxes[n].y1, boxes[n].x2 - 1, boxes[n].y2 - 1 };
     
                         sawman_draw_window( tier, sawwin, state, &draw, true );
                    }
     
     
                    misc_region_deinit( &blend );
               }
               else {
                    MISC_REGION_DEBUG_AT( SaWMan_Update, &visible, "visible" );
                    MISC_REGION_DEBUG_AT( SaWMan_Update, &render, "render" );
                    MISC_REGION_DEBUG_AT( SaWMan_Update, &opt, "clear" );
     
                    /////// FIXME: use fill rectangles!
     
                    /*
                     * Clear background
                     */
                    boxes = misc_region_boxes( &opt, &num );
     
                    for (n=0; n<num; n++) {
                         DFBRegion clear = { boxes[n].x1, boxes[n].y1, boxes[n].x2 - 1, boxes[n].y2 - 1 };
     
                         sawman_draw_background( tier, state, &clear );
                    }
     
     
                    /////// FIXME: use batch blit!
     
                    /*
                     * Draw visible window areas
                     */
                    boxes = misc_region_boxes( &render, &num );
     
                    for (n=0; n<num; n++) {
                         DFBRegion draw = { boxes[n].x1, boxes[n].y1, boxes[n].x2 - 1, boxes[n].y2 - 1 };
     
                         sawman_draw_window( tier, sawwin, state, &draw, true );
                    }
     
               }
     
     
               /* dirty -= render */
               misc_region_subtract( &dirty, &dirty, &render );
     
     
               misc_region_deinit( &opt );
               misc_region_deinit( &render );
               misc_region_deinit( &visible );
          }
     }

     D_DEBUG_AT( SaWMan_Update, " -=> done <=-\n" );

     MISC_REGION_DEBUG_AT( SaWMan_Update, &dirty, "dirty" );

     /////// FIXME: use fill rectangles!

     /*
      * Clear background
      */
     boxes = misc_region_boxes( &dirty, &num );

     for (n=0; n<num; n++) {
          DFBRegion clear = { boxes[n].x1, boxes[n].y1, boxes[n].x2 - 1, boxes[n].y2 - 1 };

          sawman_draw_background( tier, state, &clear );
     }

     misc_region_deinit( &dirty );
}

static void
update_region3( SaWMan          *sawman,
                SaWManTier      *tier,
                CardState       *state,
                int              start,
                int              x1,
                int              y1,
                int              x2,
                int              y2 )
{
     SaWManWindow    *sawwin = 0;
     CoreWindowStack *stack;

     int winNum;
     int u;

     DFBRegion updateRegion = {x1,y1,x2,y2};

     DFBLinkRegionPool  regionpool;
     DFBLinkRegion     *regionstorage;

     DirectLink *backgroundNotNeeded = 0;
     DirectLink *backgroundNeeded    = 0;

     int blackBackground = false;

     DFBLinkRegion *lr;
     DFBRegion *r;

     D_DEBUG_AT( SaWMan_Update, "%s( %p, %d, %d,%d - %d,%d )\n", __FUNCTION__, tier, start, x1, y1, x2, y2 );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( start < fusion_vector_size( &sawman->layout ) );
     D_ASSUME( x1 <= x2 );
     D_ASSUME( y1 <= y2 );

     if (x1 > x2 || y1 > y2)
          return;

     stack = tier->stack;
     D_ASSERT( stack != NULL );

     /* we need some intermediate storage */
     /* 50 = arbitrary */
     regionstorage = alloca(sizeof(DFBLinkRegion) * 50);
     dfb_linkregionpool_init( &regionpool, regionstorage, 50 );

     const int numberOfWindows  = fusion_vector_size( &sawman->layout );
     DirectLink *updatesBlend[numberOfWindows];
     DirectLink *updatesNoBlend[numberOfWindows];

     /* TODO: if we have a background picture,
      * there is a very strong case for optimizing using double blit. */

     blackBackground = (stack->bg.mode == DLBM_COLOR) && !stack->bg.color.a && !stack->bg.color.r &&  !stack->bg.color.g && !stack->bg.color.b;

     /* z-order: bottom to top */
     fusion_vector_foreach (sawwin, winNum, sawman->layout) {
          CoreWindow *window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_ASSERT( window, CoreWindow );

          updatesBlend[winNum]   = 0;
          updatesNoBlend[winNum] = 0;

          if (SAWMAN_VISIBLE_WINDOW( window ) && (tier->classes & (1 << window->config.stacking))) {

               D_DEBUG_AT( SaWMan_Update, " -=> [%d] <=- %d\n", winNum, SAWMAN_TRANSLUCENT_WINDOW(window) );

               /* check against bounding boxes to quickly discard non-overlapping windows */
               if (!dfb_rectangle_region_intersects(&window->config.bounds, &updateRegion) )
                    continue;
                                                    
               /* Optimizing. Only possible when the opacity is max and
                  there is no color keying because this requires additional blending */
               if (      (window->config.opacity == 0xff)
                     &&   window->surface
                     &&  (window->surface->config.caps & DSCAPS_PREMULTIPLIED)
                     &&  (window->config.options & DWOP_ALPHACHANNEL)
                     && !(window->config.options & DWOP_COLORKEYING)
                     &&  (window->config.dst_geometry.mode == DWGM_DEFAULT)
                     &&   blackBackground )
               {
                    DirectLink *updates    = 0;

                    D_DEBUG_AT( SaWMan_Update, " ---> window optimized\n" );

                    /* copy all applicable updates in a separate structure */
                    for (u=0; u < sawwin->visible.num_regions; u++) {

                         /* clip the visible regions to the update region */
                         if (!dfb_region_intersects( &sawwin->visible.regions[u], x1, y1, x2, y2 ))
                              continue;

                         lr = dfb_linkregionpool_get( &regionpool, &sawwin->visible.regions[u] );
                         dfb_region_clip( &lr->region, x1, y1, x2, y2 );
                         direct_list_append( &updates, &lr->link );
                    }

                    DFBLinkRegion *linkRegion = 0;
                    direct_list_foreach(linkRegion, updates) {

                         /* optimizing!
                            check for intersections between to-be-drawn lower windows
                            and this window.
                              If intersection with blend: -> blend.
                              If intersection with noBlend: -> blend, double source
                              Else: draw opaque (add to noBlend).
                         */
                         
                         for (u=winNum-1; u>=0; u--) {
                              direct_list_foreach(lr, updatesBlend[u]) {
                                   DFBRegion *R = &linkRegion->region;
                                   r = &lr->region;
                                   if (dfb_region_region_intersects( R, r )) {
                                        /* overlap with other window! */

                                        /* re-add remaing sections to reconsider */
                                        dfb_linkregionpool_add_allowedpartofregion( &regionpool, &updates, R, r );

                                        /* add intersection to updatesBlend[winNum] */
                                        dfb_region_clip( R, r->x1, r->y1, r->x2, r->y2 );
                                        DFBLinkRegion *lnk = dfb_linkregionpool_get( &regionpool, R );
                                        direct_list_append( &updatesBlend[winNum], &lnk->link );
                                        
                                        goto continueupdates;
                                   }
                              }
                              direct_list_foreach(lr, updatesNoBlend[u]) {
                                   DFBRegion *R = &linkRegion->region;
                                   r = &lr->region;
                                   if (dfb_region_region_intersects( R, r )) {
                                        /* overlap with other window! */
                                        /* intersection, blend double source */

                                        /* reorganise overlapped window;
                                         * we need to cut out the intersection,
                                         * and change the original entry to new ones */
                                        dfb_linkregionpool_add_allowedpartofregion( &regionpool, &updatesNoBlend[u], r, R );
                                        direct_list_remove( &updatesNoBlend[u], &lr->link );

                                        /* re-add remaing sections to reconsider */
                                        dfb_linkregionpool_add_allowedpartofregion( &regionpool, &updates, R, r );

                                        /* proceed to draw immediately 
                                         * we can store the window in another list, but this is more efficient */
                                        D_DEBUG_AT( SaWMan_Update, "    > optimized with double source\n" );
                                        dfb_region_clip( R, r->x1, r->y1, r->x2, r->y2 );
                                        SaWManWindow *sw = fusion_vector_at( &sawman->layout, u );
                                        sawman_draw_two_windows( tier, sw, sawwin, state, R );
                                   }
                              }
                         }

                         /* if we came here, it is a non-overlapping dull window */
                         DFBLinkRegion *lnk;
                         
                         lnk = dfb_linkregionpool_get( &regionpool, &linkRegion->region );
                         direct_list_append( &updatesNoBlend[winNum], &lnk->link );
                         lnk = dfb_linkregionpool_get( &regionpool, &linkRegion->region );
                         direct_list_append( &backgroundNotNeeded, &lnk->link );

                         continueupdates:
                              ;
                    }
               }
               else {
                    int translucent = SAWMAN_TRANSLUCENT_WINDOW(window);

                    D_DEBUG_AT( SaWMan_Update, " ---> default %s window\n", 
                                   translucent ? "blended" : "opaque" );

                    /* store the updates of this window inside the update region */
                    for (u=0; u < sawwin->visible.num_regions; u++) {
                         if (dfb_region_intersects( &sawwin->visible.regions[u], x1, y1, x2, y2 )) {
                              /* make a new region */
                              lr = dfb_linkregionpool_get( &regionpool, &sawwin->visible.regions[u] );
                              dfb_region_clip( &lr->region, x1, y1, x2, y2 );
                              if (translucent)
                                   direct_list_append( &updatesBlend[winNum], &lr->link );
                              else {
                                   /* ignore background */
                                   direct_list_append( &updatesNoBlend[winNum], &lr->link );
                                   DFBLinkRegion *lrbg = dfb_linkregionpool_get( &regionpool, &lr->region );
                                   direct_list_append( &backgroundNotNeeded, &lrbg->link );
                              }
                         }
                    }
               }
          }
     }

     /* draw background */

     /* the background has to be inverted, that is easiest accomplished by
        sorting the entries and then filling the update structure top-down */
     //~ TODO: do_some_kind_of_sorting(backgroundNotNeeded);

    {
     /* inversion, but only inside updatable area */
     int x,y,w,h;
     x=x1; y=y1; w=0;
     while (y<=y2) {
          h=y2-y+1; /* maximum */

          /* determine the height of the current band */
          direct_list_foreach(lr, backgroundNotNeeded) {
               r = &lr->region;
               if ( (r->y1 > y) && (r->y1 - y < h) )
                    h = r->y1 - y;
               if ( (r->y1 <= y) && (r->y2 >= y) && (r->y2 - y + 1 < h) )
                    h = r->y2 - y + 1; /* if a band is ended because of this, we can optimize and "hide" the region afterwards */
          }

          /* just "walk the band", looking at the x coordinates, and add updates */
          while (x<=x2) {
               w=x2-x+1; /* maximum */

               direct_list_foreach(lr, backgroundNotNeeded) {
                    r = &lr->region;
                    if ( (r->y1 <= y) && (r->y2 >= y) ) {
                         /* valid window; this window is part of the band */
                         if ( (r->x1 <= x) && (r->x2 >= x) ) {
                              /* IN a band; adjust x and w and go back to start */
                              x = r->x2+1;
                              w = x2-x+1;
                              if (w<0) w=0;
                              lr = (__typeof__(lr))backgroundNotNeeded;
                              /* we will miss the first element but we just had it. */
                              continue;
                         }
                         if (r->x2 < x) /* out of reach */
                              continue;
                         if ( (r->x1 - x) < w) { /* this window is blocking the band */
                              w = r->x1 - x;
                         }
                    }
               }

               if (w && h) {
                    DFBRegion u = { x, y, x+w-1, y+h-1 };
                    /* we can optimize by checking if above us is a band with the correct width */
                    lr = dfb_linkregionpool_get( &regionpool, &u );
                    direct_list_append( &backgroundNeeded, &lr->link );

               }

               x += w;
          }

          y += h;
          x  = x1;
     }
    }

     D_DEBUG_AT( SaWMan_Update, "     > background\n" );

     direct_list_foreach(lr, backgroundNeeded) {
          r = &lr->region;
          D_DEBUG_AT( SaWMan_Update, "     > %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION(r) );
          sawman_draw_background( tier, state, r );
     }

     /* draw all the windows */
     fusion_vector_foreach (sawwin, winNum, sawman->layout) {
          D_DEBUG_AT( SaWMan_Update, "     > window %d\n", winNum );
          
          /* collate the updates to reduce number of draw calls */
          /* (is this needed?) */
          //~ collate( &updatesNoBlend[winNum] );
          //~ collate( &updatesBlend[winNum] );

          direct_list_foreach(lr, updatesNoBlend[winNum]) {
               r = &lr->region;
               D_DEBUG_AT( SaWMan_Update, "     > nb %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION(r) );
               sawman_draw_window( tier, sawwin, state, r, false );
          }

          direct_list_foreach(lr, updatesBlend[winNum]) {
               r = &lr->region;
               D_DEBUG_AT( SaWMan_Update, "     > b %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION(r) );
               sawman_draw_window( tier, sawwin, state, r, true );
          }
     }

     D_DEBUG_AT( SaWMan_Update, " -=> done <=-\n" );
}

static void
repaint_tier( SaWMan              *sawman,
              SaWManTier          *tier,
              const DFBRegion     *updates,
              int                  num_updates,
              DFBSurfaceFlipFlags  flags )
{
     int              i;
     CoreLayer       *layer;
     CoreLayerRegion *region;
     CardState       *state;
     CoreSurface     *surface;
     DFBRegion        cursor_inter;
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     D_ASSERT( updates != NULL );
     D_ASSERT( num_updates > 0 );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     stack = tier->stack;
     D_ASSERT( stack != NULL );

     region = tier->region;
     D_ASSERT( region != NULL );

     layer   = dfb_layer_at( tier->layer_id );
     state   = &layer->state;
     surface = region->surface;

     if (/*!data->active ||*/ !surface)
          return;

     D_DEBUG_AT( SaWMan_Update, "%s( %p, %p )\n", __FUNCTION__, sawman, tier );

     /* Set destination. */
     state->destination  = surface;
     state->modified    |= SMF_DESTINATION;

     for (i=0; i<num_updates; i++) {
          const DFBRegion *update = &updates[i];

          DFB_REGION_ASSERT( update );

          D_DEBUG_AT( SaWMan_Update, "  -> %d, %d - %dx%d  (%d)\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( update ), i );

          if (!DFB_PLANAR_PIXELFORMAT(region->config.format))
               dfb_state_set_dst_colorkey( state, dfb_color_to_pixel( region->config.format,
                                                                      region->config.src_key.r,
                                                                      region->config.src_key.g,
                                                                      region->config.src_key.b ) );
          else
               dfb_state_set_dst_colorkey( state, 0 );

          /* Set clipping region. */
          dfb_state_set_clip( state, update );

          /* Compose updated region. */
          update_region2( sawman, tier, state,
                          fusion_vector_size( &sawman->layout ) - 1,
                          update->x1, update->y1, update->x2, update->y2 );

          /* Update cursor? */
          cursor_inter = tier->cursor_region;
          if (tier->cursor_drawn && dfb_region_region_intersect( &cursor_inter, update )) {
               DFBRectangle rect = DFB_RECTANGLE_INIT_FROM_REGION( &cursor_inter );

               D_ASSUME( tier->cursor_bs_valid );

               dfb_gfx_copy_to( surface, tier->cursor_bs, &rect,
                                rect.x - tier->cursor_region.x1,
                                rect.y - tier->cursor_region.y1, true );

               sawman_draw_cursor( stack, state, &cursor_inter );
          }
     }

     /* Reset destination. */
     state->destination  = NULL;
     state->modified    |= SMF_DESTINATION;

     /* Software cursor code relies on a valid back buffer. */
     if (stack->cursor.enabled)
          flags |= DSFLIP_BLIT;

     for (i=0; i<num_updates; i++) {
          const DFBRegion *update = &updates[i];

          DFB_REGION_ASSERT( update );

          /* Flip the updated region .*/
          dfb_layer_region_flip_update( region, update, flags );
     }

#ifdef SAWMAN_DUMP_TIER_FRAMES
     {
          DFBResult          ret;
          CoreSurfaceBuffer *buffer;

          D_MAGIC_ASSERT( surface, CoreSurface );

          if (fusion_skirmish_prevail( &surface->lock ))
               return;

          buffer = dfb_surface_get_buffer( surface, CSBR_FRONT );
          D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

          ret = dfb_surface_buffer_dump( buffer, "/", "tier" );

          fusion_skirmish_dismiss( &surface->lock );
     }
#endif
}

static SaWManWindow *
get_single_window( SaWMan     *sawman,
                   SaWManTier *tier,
                   bool       *ret_none )
{
     int           n;
     SaWManWindow *sawwin;
     SaWManWindow *single = NULL;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     fusion_vector_foreach_reverse (sawwin, n, sawman->layout) {
          CoreWindow *window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          if (SAWMAN_VISIBLE_WINDOW(window) && (tier->classes & (1 << window->config.stacking))) {
               if (single || (window->caps & (DWCAPS_INPUTONLY | DWCAPS_COLOR) ))
                    return NULL;

               single = sawwin;

               if (single->dst.x == 0 &&
                   single->dst.y == 0 &&
                   single->dst.w == tier->size.w &&
                   single->dst.h == tier->size.h &&
                   !SAWMAN_TRANSLUCENT_WINDOW(window))
                    break;
          }
     }

     if (ret_none && !single)
          *ret_none = true;

     return single;
}

static bool
get_border_only( SaWMan     *sawman,
                 SaWManTier *tier )
{
     int           n;
     SaWManWindow *sawwin;
     bool          none = true;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     fusion_vector_foreach_reverse (sawwin, n, sawman->layout) {
          CoreWindow *window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          none = false;

          if (SAWMAN_VISIBLE_WINDOW(window) && !(window->caps & DWCAPS_INPUTONLY))
               return false;
     }

     return !none;
}

/* FIXME: Split up in smaller functions and clean up things like forcing reconfiguration. */
DirectResult
sawman_process_updates( SaWMan              *sawman,
                        DFBSurfaceFlipFlags  flags )
{
     DirectResult  ret;
     int           idx = -1;
     SaWManTier   *tier;

     D_MAGIC_ASSERT( sawman, SaWMan );
     FUSION_SKIRMISH_ASSERT( &sawman->lock );

     D_DEBUG_AT( SaWMan_Update, "%s( %p, 0x%08x )\n", __FUNCTION__, sawman, flags );

     direct_list_foreach (tier, sawman->tiers) {
          int              n, d;
          int              total;
          int              bounding;
          bool             none = false;
          bool             border_only;
          SaWManWindow    *single;
          CoreLayer       *layer;
          CoreLayerShared *shared;
          int              screen_width;
          int              screen_height;
          DFBColorKey      single_key;

          idx++;

          layer = dfb_layer_at( tier->layer_id );
          D_ASSERT( layer != NULL );

          shared = layer->shared;
          D_ASSERT( shared != NULL );

          D_MAGIC_ASSERT( tier, SaWManTier );

          if (!tier->updates.num_regions)
               continue;

          D_DEBUG_AT( SaWMan_Update, "  -> %d updates (tier %d, layer %d)\n",
                      tier->updates.num_regions, idx, tier->layer_id );

          D_ASSERT( tier->region != NULL );

          D_DEBUG_AT( SaWMan_Update, "  -> [%d] %d updates, bounding %dx%d\n",
                      tier->layer_id, tier->updates.num_regions,
                      tier->updates.bounding.x2 - tier->updates.bounding.x1 + 1,
                      tier->updates.bounding.y2 - tier->updates.bounding.y1 + 1 );

          if (!tier->config.width || !tier->config.height)
               continue;

          dfb_screen_get_screen_size( layer->screen, &screen_width, &screen_height );

          single = get_single_window( sawman, tier, &none );

          if (none && !sawman_config->show_empty) {
               if (tier->active) {
                    D_DEBUG_AT( SaWMan_Auto, "  -> Disabling region...\n" );

                    tier->active        = false;
                    tier->single_window = NULL;  /* enforce configuration to reallocate buffers */

                    dfb_layer_region_disable( tier->region );
               }
               dfb_updates_reset( &tier->updates );
               continue;
          }

          border_only = get_border_only( sawman, tier );

          /* Remember color key before single mode is activated. */
          if (!tier->single_mode)
               tier->key = tier->context->primary.config.src_key;


          /* If the first mode after turning off the layer is not single, then we need
             this to force a reconfiguration to reallocate the buffers. */
          if (!tier->active) {
               tier->single_mode = true;               /* avoid endless loop */
               tier->border_only = !border_only;       /* enforce configuration to reallocate buffers */
          }

          if (single && !border_only) {
               CoreWindow             *window;
               CoreSurface            *surface;
               DFBDisplayLayerOptions  options = DLOP_NONE;
               DFBRectangle            dst  = single->dst;
               DFBRectangle            src  = single->src;
               DFBRegion               clip = DFB_REGION_INIT_FROM_DIMENSION( &tier->size );

               if (shared->description.caps & DLCAPS_SCREEN_LOCATION) {
                    dst.x = dst.x * screen_width  / tier->size.w;
                    dst.y = dst.y * screen_height / tier->size.h;
                    dst.w = dst.w * screen_width  / tier->size.w;
                    dst.h = dst.h * screen_height / tier->size.h;
               }
               else {
                    if (dst.w != src.w || dst.h != src.h)
                         goto no_single;

                    if (shared->description.caps & DLCAPS_SCREEN_POSITION) {
                         dfb_rectangle_intersect_by_region( &dst, &clip );

                         src.x += dst.x - single->dst.x;
                         src.y += dst.y - single->dst.y;
                         src.w  = dst.w;
                         src.h  = dst.h;

                         dst.x += (screen_width  - tier->size.w) / 2;
                         dst.y += (screen_height - tier->size.h) / 2;
                    }
               }

#ifdef SAWMAN_NO_LAYER_DOWNSCALE
               if (rect.w < src.w)
                    goto no_single;
#endif

#ifdef SAWMAN_NO_LAYER_DST_WINDOW
               if (dst.x != 0 || dst.y != 0 || dst.w != screen_width || dst.h != screen_height)
                    goto no_single;
#endif


               window = single->window;
               D_MAGIC_COREWINDOW_ASSERT( window );

               surface = window->surface;
               D_ASSERT( surface != NULL );

               if (window->config.options & DWOP_ALPHACHANNEL)
                    options |= DLOP_ALPHACHANNEL;

               if (window->config.options & DWOP_COLORKEYING)
                    options |= DLOP_SRC_COLORKEY;

               single_key = tier->single_key;

               if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format )) {
                    CorePalette *palette = surface->palette;

                    D_ASSERT( palette != NULL );
                    D_ASSERT( palette->num_entries > 0 );

                    dfb_surface_set_palette( tier->region->surface, surface->palette );

                    if (options & DLOP_SRC_COLORKEY) {
                         int index = window->config.color_key % palette->num_entries;

                         single_key.r     = palette->entries[index].r;
                         single_key.g     = palette->entries[index].g;
                         single_key.b     = palette->entries[index].b;
                         single_key.index = index;
                    }
               }
               else {
                    DFBColor color;

                    dfb_pixel_to_color( surface->config.format, window->config.color_key, &color );

                    single_key.r     = color.r;
                    single_key.g     = color.g;
                    single_key.b     = color.b;
                    single_key.index = window->config.color_key;
               }

               /* Complete reconfig? */
               if (tier->single_window  != single ||
                   !DFB_RECTANGLE_EQUAL( tier->single_src, src ) ||
                   tier->single_format  != surface->config.format ||
                   tier->single_options != options)
               {
                    DFBDisplayLayerConfig  config;

                    D_DEBUG_AT( SaWMan_Auto, "  -> Switching to %dx%d [%dx%d] %s single mode for %p on %p...\n",
                                single->src.w, single->src.h, src.w, src.h,
                                dfb_pixelformat_name( surface->config.format ), single, tier );

                    config.flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_OPTIONS | DLCONF_BUFFERMODE;
                    config.width       = src.w;
                    config.height      = src.h;
                    config.pixelformat = surface->config.format;
                    config.options     = options;
                    config.buffermode  = DLBM_FRONTONLY;

                    sawman->callback.layer_reconfig.layer_id = tier->layer_id;
                    sawman->callback.layer_reconfig.single   = (SaWManWindowHandle) single;
                    sawman->callback.layer_reconfig.config   = config;

                    switch (sawman_call( sawman, SWMCID_LAYER_RECONFIG, &sawman->callback.layer_reconfig )) {
                         case DFB_OK:
                              config = sawman->callback.layer_reconfig.config;
                         case DFB_NOIMPL: 
                              /* continue, no change demanded */
                              break;

                         default:
                              goto no_single;
                    }

                    if (dfb_layer_context_test_configuration( tier->context, &config, NULL ) != DFB_OK)
                         goto no_single;

                    tier->single_mode     = true;
                    tier->single_window   = single;
                    tier->single_width    = src.w;
                    tier->single_height   = src.h;
                    tier->single_src      = src;
                    tier->single_dst      = dst;
                    tier->single_format   = surface->config.format;
                    tier->single_options  = options;
                    tier->single_key      = single_key;

                    tier->active          = false;
                    tier->region->state  |= CLRSF_FROZEN;

                    dfb_updates_reset( &tier->updates );

                    dfb_layer_context_set_configuration( tier->context, &config );

                    if (shared->description.caps & DLCAPS_SCREEN_LOCATION)
                         dfb_layer_context_set_screenrectangle( tier->context, &dst );
                    else if (shared->description.caps & DLCAPS_SCREEN_POSITION)
                         dfb_layer_context_set_screenposition( tier->context, dst.x, dst.y );

                    dfb_layer_context_set_src_colorkey( tier->context,
                                                        tier->single_key.r, tier->single_key.g,
                                                        tier->single_key.b, tier->single_key.index );

                    dfb_gfx_copy_to( surface, tier->region->surface, &src, 0, 0, false );

                    tier->active = true;

                    dfb_layer_region_flip_update( tier->region, NULL, flags );

                    dfb_updates_reset( &tier->updates );
                    continue;
               }

               /* Update destination window */
               if (!DFB_RECTANGLE_EQUAL( tier->single_dst, dst )) {
                    tier->single_dst = dst;

                    D_DEBUG_AT( SaWMan_Auto, "  -> Changing single destination to %d,%d-%dx%d.\n",
                                DFB_RECTANGLE_VALS(&dst) );

                    dfb_layer_context_set_screenrectangle( tier->context, &dst );
               }
               else
                    dfb_gfx_copy_to( surface, tier->region->surface, &src, 0, 0, false );

               /* Update color key */
               if (!DFB_COLORKEY_EQUAL( single_key, tier->single_key )) {
                    D_DEBUG_AT( SaWMan_Auto, "  -> Changing single color key.\n" );

                    tier->single_key = single_key;

                    dfb_layer_context_set_src_colorkey( tier->context,
                                                        tier->single_key.r, tier->single_key.g,
                                                        tier->single_key.b, tier->single_key.index );
               }

               tier->active = true;

               dfb_layer_region_flip_update( tier->region, NULL, flags );

               dfb_updates_reset( &tier->updates );
               continue;
          }

no_single:

          if (tier->single_mode) {
               D_DEBUG_AT( SaWMan_Auto, "  -> Switching back from single mode...\n" );

               tier->border_only = !border_only;       /* enforce switch */
          }

          /* Switch border/default config? */
          if (tier->border_only != border_only) {
               const DFBDisplayLayerConfig *config;

               tier->border_only = border_only;

               if (border_only)
                    config = &tier->border_config;
               else
                    config = &tier->config;

               D_DEBUG_AT( SaWMan_Auto, "  -> Switching to %dx%d %s %s mode.\n", config->width, config->height,
                           dfb_pixelformat_name( config->pixelformat ), border_only ? "border" : "standard" );

               tier->active         = false;
               tier->region->state |= CLRSF_FROZEN;

               dfb_updates_reset( &tier->updates );

               /* Temporarily to avoid configuration errors. */
               dfb_layer_context_set_screenposition( tier->context, 0, 0 );

               ret = dfb_layer_context_set_configuration( tier->context, config );
               if (ret) {
                    D_DERROR( ret, "SaWMan/Auto: Switching to standard mode failed!\n" );
                    /* fixme */
               }

               tier->size.w = config->width;
               tier->size.h = config->height;

               /* Notify application manager about new tier size if previous mode was single. */
               if (tier->single_mode)
                    sawman_call( sawman, SWMCID_STACK_RESIZED, &tier->size );

               if (shared->description.caps & DLCAPS_SCREEN_LOCATION) {
                    DFBRectangle full = { 0, 0, screen_width, screen_height };

                    dfb_layer_context_set_screenrectangle( tier->context, &full );
               }
               else if (shared->description.caps & DLCAPS_SCREEN_POSITION) {
                    dfb_layer_context_set_screenposition( tier->context,
                                                          (screen_width  - config->width)  / 2,
                                                          (screen_height - config->height) / 2 );
               }

               if (config->options & DLOP_SRC_COLORKEY) {
                    if (DFB_PIXELFORMAT_IS_INDEXED( config->pixelformat )) {
                         int          index;
                         CoreSurface *surface;
                         CorePalette *palette;

                         surface = tier->region->surface;
                         D_MAGIC_ASSERT( surface, CoreSurface );

                         palette = surface->palette;
                         D_ASSERT( palette != NULL );
                         D_ASSERT( palette->num_entries > 0 );

                         index = tier->key.index % palette->num_entries;

                         dfb_layer_context_set_src_colorkey( tier->context,
                                                             palette->entries[index].r,
                                                             palette->entries[index].g,
                                                             palette->entries[index].b,
                                                             index );
                    }
                    else
                         dfb_layer_context_set_src_colorkey( tier->context,
                                                             tier->key.r, tier->key.g, tier->key.b, tier->key.index );
               }
          }

          if (!tier->active) {
               D_DEBUG_AT( SaWMan_Auto, "  -> Activating tier...\n" );

               tier->active = true;

               DFBRegion region = { 0, 0, tier->size.w - 1, tier->size.h - 1 };
               dfb_updates_add( &tier->updates, &region );
          }

          tier->single_mode   = false;
          tier->single_window = NULL;

          if (!tier->updates.num_regions)
               continue;


          dfb_updates_stat( &tier->updates, &total, &bounding );

          n = tier->updates.max_regions - tier->updates.num_regions + 1;
          d = n + 1;

          /* Try to optimize updates. In buffer swapping modes we can save the copy by updating everything. */
          if ((total > tier->size.w * tier->size.h) ||
              (total > tier->size.w * tier->size.h * 3 / 5 && (tier->context->config.buffermode == DLBM_BACKVIDEO ||
                                                               tier->context->config.buffermode == DLBM_TRIPLE)))
          {
               DFBRegion region = { 0, 0, tier->size.w - 1, tier->size.h - 1 };

               repaint_tier( sawman, tier, &region, 1, flags );
          }
          else if (tier->updates.num_regions < 2 || total < bounding * n / d)
               repaint_tier( sawman, tier, tier->updates.regions, tier->updates.num_regions, flags );
          else
               repaint_tier( sawman, tier, &tier->updates.bounding, 1, flags );

          dfb_updates_reset( &tier->updates );
     }

     return DFB_OK;
}

