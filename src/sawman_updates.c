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
D_DEBUG_DOMAIN( SaWMan_FlipOnce, "SaWMan/FlipOnce", "SaWMan window manager flip once" );
D_DEBUG_DOMAIN( SaWMan_Surface,  "SaWMan/Surface",  "SaWMan window manager surface" );

/**********************************************************************************************************************/

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
          D_MAGIC_COREWINDOW_ASSERT( window );

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
                                        dfb_region_clip( R, r->x1, r->y1, r->x2, r->y2 );
                                        SaWManWindow *sw = fusion_vector_at( &sawman->layout, u );
                                        D_DEBUG_AT( SaWMan_Update, "     > window %d and %d\n", u, winNum );
                                        D_DEBUG_AT( SaWMan_Update, "     > nb %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION(R) );
                                        sawman_draw_two_windows( tier, sw, sawwin, state, R );

                                        goto continueupdates;
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

    {
     /* inversion, but only inside updatable area */
     /* we follow a simple algorithm:
      * start top-left, determine the smallest band height
      * walk from left to right and add non-occupied regions,
      * while checking if we have regions above to fold into
      */
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
                    h = r->y2 - y + 1; /* if a band is ended because of this, we could optimize and "hide" the region afterwards */
          }

          /* just "walk the band", looking at the x coordinates, and add updates */
          while (x<=x2) {
               w=x2-x+1; /* maximum */

               walk_the_band:

               direct_list_foreach(lr, backgroundNotNeeded) {
                    r = &lr->region;
                    if ( (r->y1 <= y) && (r->y2 >= y) ) {
                         /* valid window; this window is part of the band */
                         if ( (r->x1 <= x) && (r->x2 >= x) ) {
                              /* IN a band; adjust x and w and go back to start */
                              x = r->x2+1;
                              w = x2-x+1;
                              if (w<0) w=0;

                              goto walk_the_band;
                         }
                         if (r->x2 < x) /* out of reach */
                              continue;
                         if ( (r->x1 - x) < w) { /* this window is blocking the band */
                              w = r->x1 - x;
                         }
                    }
               }

               if (w && h) {
                    DFBRegion u         = { x, y, x+w-1, y+h-1 };
                    bool      collapsed = false;

                    /* we can optimize by checking if above us is a band with the correct width */
                    direct_list_foreach(lr, backgroundNeeded) {
                         r = &lr->region;
                         if ( (r->x1 == u.x1) && (r->x2 == u.x2) && (r->y2 + 1 == u.y1) ) {
                              r->y2 = u.y2;
                              collapsed = true;
                              break;
                         }
                    }

                    if (!collapsed) {
                         lr = dfb_linkregionpool_get( &regionpool, &u );
                         direct_list_append( &backgroundNeeded, &lr->link );
                    }
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
          /* dfb_collate( &updatesNoBlend[winNum] ); */
          /* dfb_collate( &updatesBlend[winNum] ); */

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

     dfb_linkregionpool_delete( &regionpool );

     D_DEBUG_AT( SaWMan_Update, " -=> done <=-\n" );
}

void
sawman_flush_updating( SaWMan     *sawman,
                       SaWManTier *tier )
{
     int i;

     D_DEBUG_AT( SaWMan_Surface, "%s( %p, %p )\n", __FUNCTION__, sawman, tier );

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );

     D_ASSUME( tier->updating.num_regions > 0 );
     D_ASSUME( tier->updated.num_regions == 0 );

     if (tier->updating.num_regions) {
          /* 
           * save here as it might get reset in case surface reaction
           * is called synchronously during dfb_layer_region_flip_update()
           */
          int num_regions = tier->updating.num_regions;

          D_DEBUG_AT( SaWMan_Surface, "  -> making updated = updating\n" );

          /* Make updated = updating */
          direct_memcpy( &tier->updated, &tier->updating, sizeof(DFBUpdates) );
          direct_memcpy( &tier->updated_regions[0], &tier->updating_regions[0], sizeof(DFBRegion) * tier->updating.num_regions );
          tier->updated.regions = &tier->updated_regions[0];

          D_DEBUG_AT( SaWMan_Surface, "  -> clearing updating\n" );

          /* Clear updating */
          dfb_updates_reset( &tier->updating );

          D_DEBUG_AT( SaWMan_Surface, "  -> flipping the region\n" );

          /* Flip the whole layer. */
          dfb_layer_region_flip_update( tier->region, NULL, DSFLIP_ONSYNC );

          D_DEBUG_AT( SaWMan_Surface, "  -> copying %d updated regions (F->B)\n", num_regions );

          for (i=0; i<num_regions; i++) {
               D_DEBUG_AT( SaWMan_Surface, "    -> %4d,%4d - %4dx%4d  [%d]\n",
                           DFB_RECTANGLE_VALS_FROM_REGION( &tier->updated.regions[i] ), i );
          }

          /* Copy back the updated region .*/
          dfb_gfx_copy_regions( tier->surface, CSBR_FRONT, tier->surface, CSBR_BACK,
                                tier->updated.regions, num_regions, 0, 0 );
     }
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
     FUSION_SKIRMISH_ASSERT( sawman->lock );

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
          update_region3( sawman, tier, state,
                          fusion_vector_size( &sawman->layout ) - 1,
                          update->x1, update->y1, update->x2, update->y2 );

          /* Update cursor? */
          cursor_inter = tier->cursor_region;
          if (tier->cursor_drawn && dfb_region_region_intersect( &cursor_inter, update )) {
               int          x, y;
               DFBRectangle rect = DFB_RECTANGLE_INIT_FROM_REGION( &cursor_inter );

               D_ASSUME( tier->cursor_bs_valid );

               dfb_gfx_copy_to( surface, tier->cursor_bs, &rect,
                                rect.x - tier->cursor_region.x1,
                                rect.y - tier->cursor_region.y1, true );

               x = (s64) stack->cursor.x * (s64) tier->size.w / (s64) sawman->resolution.w;
               y = (s64) stack->cursor.y * (s64) tier->size.h / (s64) sawman->resolution.h;

               sawman_draw_cursor( stack, state, surface, &cursor_inter, x, y );
          }
     }

     /* Reset destination. */
     state->destination  = NULL;
     state->modified    |= SMF_DESTINATION;

     switch (region->config.buffermode) {
          case DLBM_TRIPLE:
               /* Add the updated region .*/
               for (i=0; i<num_updates; i++) {
                    const DFBRegion *update = &updates[i];

                    DFB_REGION_ASSERT( update );

                    D_DEBUG_AT( SaWMan_Surface, "  -> adding %d, %d - %dx%d  (%d) to updating\n",
                                DFB_RECTANGLE_VALS_FROM_REGION( update ), i );

                    dfb_updates_add( &tier->updating, update );
               }

               if (!tier->updated.num_regions)
                    sawman_flush_updating( sawman, tier );
               break;

          case DLBM_BACKVIDEO:
               /* Flip the whole region. */
               dfb_layer_region_flip_update( region, NULL, flags );

               /* Copy back the updated region. */
               dfb_gfx_copy_regions( surface, CSBR_FRONT, surface, CSBR_BACK, updates, num_updates, 0, 0 );
               break;

          default:
               /* Flip the updated region .*/
               for (i=0; i<num_updates; i++) {
                    const DFBRegion *update = &updates[i];

                    DFB_REGION_ASSERT( update );

                    dfb_layer_region_flip_update( region, update, flags );
               }
               break;
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

     if (1) {
          SaWManTierUpdate update;

          direct_memcpy( &update.regions[0], updates, sizeof(DFBRegion) * num_updates );

          update.num_regions = num_updates;
          update.classes     = tier->classes;

          fusion_reactor_dispatch_channel( tier->reactor, SAWMAN_TIER_UPDATE, &update, sizeof(update), true, NULL );
     }
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
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     fusion_vector_foreach_reverse (sawwin, n, sawman->layout) {
          CoreWindow *window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          if (SAWMAN_VISIBLE_WINDOW(window) && (tier->classes & (1 << window->config.stacking))) {
               if (      single 
                    || ( window->caps & (DWCAPS_INPUTONLY | DWCAPS_COLOR) ) 
                    || ( window->config.options & DWOP_INPUTONLY ) )
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
     FUSION_SKIRMISH_ASSERT( sawman->lock );

     fusion_vector_foreach_reverse (sawwin, n, sawman->layout) {
          CoreWindow *window;

          D_MAGIC_ASSERT( sawwin, SaWManWindow );

          window = sawwin->window;
          D_MAGIC_COREWINDOW_ASSERT( window );

          none = false;

          if (     SAWMAN_VISIBLE_WINDOW(window) 
              && !(window->caps & DWCAPS_INPUTONLY)
              && !(window->config.options & DWOP_INPUTONLY) )
               return false;
     }

     return !none;
}

static bool
windows_updating( SaWMan     *sawman,
                  SaWManTier *tier )
{
     int           i;
     SaWManWindow *window;

     D_MAGIC_ASSERT( sawman, SaWMan );
     D_MAGIC_ASSERT( tier, SaWManTier );

     fusion_vector_foreach (window, i, sawman->layout) {
          D_MAGIC_ASSERT( window, SaWManWindow );
          
          if (window->flags & SWMWF_UPDATING) {
               long long diff = direct_clock_get_millis() - window->update_ms;

               if (!sawman_config->flip_once_timeout || diff < sawman_config->flip_once_timeout) {
                    D_DEBUG_AT( SaWMan_FlipOnce, "  -> update blocking on window id %u (%lld ms, flags 0x%08x)\n",
                                window->id, diff, window->flags );

                    return true;
               }

               D_DEBUG_AT( SaWMan_FlipOnce, "  -> ignoring blocking of window id %u (%lld ms, flags 0x%08x)\n",
                           window->id, diff, window->flags );
          }
     }

     D_DEBUG_AT( SaWMan_FlipOnce, "  -> update not blocked by any window\n" );

     return false;
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
     FUSION_SKIRMISH_ASSERT( sawman->lock );

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

          if (tier->update_once) {
               if (windows_updating( sawman, tier ))
                    continue;

               tier->update_once = false;
          }

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

                    if (sawman->cursor.context && tier->context->layer_id == sawman->cursor.context->layer_id)
                         dfb_layer_activate_context( dfb_layer_at(sawman->cursor.context->layer_id), sawman->cursor.context );
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

                    if (1) {
                         SaWManTierUpdate update;

                         update.regions[0].x1 = 0;
                         update.regions[0].y1 = 0;
                         update.regions[0].x2 = tier->single_width  - 1;
                         update.regions[0].y2 = tier->single_height - 1;

                         update.num_regions   = 1;
                         update.classes       = tier->classes;

                         fusion_reactor_dispatch_channel( tier->reactor, SAWMAN_TIER_UPDATE, &update, sizeof(update), true, NULL );
                    }
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

               fusion_skirmish_notify( sawman->lock );
               continue;
          }

no_single:

          if (tier->single_mode) {
               D_DEBUG_AT( SaWMan_Auto, "  -> Switching back from single mode...\n" );

               tier->border_only = !border_only;       /* enforce switch */
          }

          /* Switch border/default config? */
          if (tier->border_only != border_only) {
               DFBDisplayLayerConfig *config;

               tier->border_only = border_only;

               if (border_only)
                    config = &tier->border_config;
               else
                    config = &tier->config;

               D_DEBUG_AT( SaWMan_Auto, "  -> Switching to %dx%d %s %s mode.\n", config->width, config->height,
                           dfb_pixelformat_name( config->pixelformat ), border_only ? "border" : "standard" );

               sawman->callback.layer_reconfig.layer_id = tier->layer_id;
               sawman->callback.layer_reconfig.single   = SAWMAN_WINDOW_NONE;
               sawman->callback.layer_reconfig.config   = *config;
               ret = sawman_call( sawman, SWMCID_LAYER_RECONFIG, &sawman->callback.layer_reconfig );

               /* on DFB_OK we try to overrule the default configuration */
               if ( !ret && !dfb_layer_context_test_configuration( tier->context, &(sawman->callback.layer_reconfig.config), NULL ) ) {
                    *config = sawman->callback.layer_reconfig.config;
                    D_DEBUG_AT( SaWMan_Auto, "  -> Overruled to %dx%d %s %s mode.\n", config->width, config->height,
                         dfb_pixelformat_name( config->pixelformat ), border_only ? "border" : "standard" );
               }

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

               if (sawman->cursor.context && tier->context->layer_id == sawman->cursor.context->layer_id)
                    dfb_layer_activate_context( dfb_layer_at(tier->context->layer_id), tier->context );
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

          fusion_skirmish_notify( sawman->lock );
     }

     return DFB_OK;
}

