/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
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

#include <config.h>

#include <string.h>

#include <directfb_util.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <time.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <gfx/convert.h>

#include <misc/util.h>


D_DEBUG_DOMAIN( DFB_Updates, "DirectFB/Updates", "DirectFB Updates" );

/**********************************************************************************************************************/

const DirectFBPixelFormatNames( dfb_pixelformat_names );
const DirectFBColorSpaceNames( dfb_colorspace_names );

/**********************************************************************************************************************/

bool
dfb_region_rectangle_intersect( DFBRegion          *region,
                                const DFBRectangle *rect )
{
     int x2 = rect->x + rect->w - 1;
     int y2 = rect->y + rect->h - 1;

     if (region->x2 < rect->x ||
         region->y2 < rect->y ||
         region->x1 > x2 ||
         region->y1 > y2)
          return false;

     if (region->x1 < rect->x)
          region->x1 = rect->x;

     if (region->y1 < rect->y)
          region->y1 = rect->y;

     if (region->x2 > x2)
          region->x2 = x2;

     if (region->y2 > y2)
          region->y2 = y2;

     return true;
}

bool
dfb_unsafe_region_intersect( DFBRegion *region,
                             int x1, int y1, int x2, int y2 )
{
     if (region->x1 > region->x2) {
          int temp = region->x1;
          region->x1 = region->x2;
          region->x2 = temp;
     }

     if (region->y1 > region->y2) {
          int temp = region->y1;
          region->y1 = region->y2;
          region->y2 = temp;
     }

     return dfb_region_intersect( region, x1, y1, x2, y2 );
}

bool
dfb_unsafe_region_rectangle_intersect( DFBRegion          *region,
                                       const DFBRectangle *rect )
{
     if (region->x1 > region->x2) {
          int temp = region->x1;
          region->x1 = region->x2;
          region->x2 = temp;
     }

     if (region->y1 > region->y2) {
          int temp = region->y1;
          region->y1 = region->y2;
          region->y2 = temp;
     }

     return dfb_region_rectangle_intersect( region, rect );
}

bool
dfb_rectangle_intersect_by_unsafe_region( DFBRectangle *rectangle,
                                          DFBRegion    *region )
{
     /* validate region */
     if (region->x1 > region->x2) {
          int temp = region->x1;
          region->x1 = region->x2;
          region->x2 = temp;
     }

     if (region->y1 > region->y2) {
          int temp = region->y1;
          region->y1 = region->y2;
          region->y2 = temp;
     }

     /* adjust position */
     if (region->x1 > rectangle->x) {
          rectangle->w -= region->x1 - rectangle->x;
          rectangle->x = region->x1;
     }

     if (region->y1 > rectangle->y) {
          rectangle->h -= region->y1 - rectangle->y;
          rectangle->y = region->y1;
     }

     /* adjust size */
     if (region->x2 < rectangle->x + rectangle->w - 1)
        rectangle->w = region->x2 - rectangle->x + 1;

     if (region->y2 < rectangle->y + rectangle->h - 1)
        rectangle->h = region->y2 - rectangle->y + 1;

     /* set size to zero if there's no intersection */
     if (rectangle->w <= 0 || rectangle->h <= 0) {
          rectangle->w = 0;
          rectangle->h = 0;

          return false;
     }

     return true;
}

bool
dfb_rectangle_intersect_by_region( DFBRectangle    *rectangle,
                                   const DFBRegion *region )
{
     /* adjust position */
     if (region->x1 > rectangle->x) {
          rectangle->w -= region->x1 - rectangle->x;
          rectangle->x = region->x1;
     }

     if (region->y1 > rectangle->y) {
          rectangle->h -= region->y1 - rectangle->y;
          rectangle->y = region->y1;
     }

     /* adjust size */
     if (region->x2 < rectangle->x + rectangle->w - 1)
        rectangle->w = region->x2 - rectangle->x + 1;

     if (region->y2 < rectangle->y + rectangle->h - 1)
        rectangle->h = region->y2 - rectangle->y + 1;

     /* set size to zero if there's no intersection */
     if (rectangle->w <= 0 || rectangle->h <= 0) {
          rectangle->w = 0;
          rectangle->h = 0;

          return false;
     }

     return true;
}

bool dfb_rectangle_intersect( DFBRectangle       *rectangle,
                              const DFBRectangle *clip )
{
     DFBRegion region = { clip->x, clip->y,
                          clip->x + clip->w - 1, clip->y + clip->h - 1 };

     /* adjust position */
     if (region.x1 > rectangle->x) {
          rectangle->w -= region.x1 - rectangle->x;
          rectangle->x = region.x1;
     }

     if (region.y1 > rectangle->y) {
          rectangle->h -= region.y1 - rectangle->y;
          rectangle->y = region.y1;
     }

     /* adjust size */
     if (region.x2 < rectangle->x + rectangle->w - 1)
          rectangle->w = region.x2 - rectangle->x + 1;

     if (region.y2 < rectangle->y + rectangle->h - 1)
          rectangle->h = region.y2 - rectangle->y + 1;

     /* set size to zero if there's no intersection */
     if (rectangle->w <= 0 || rectangle->h <= 0) {
          rectangle->w = 0;
          rectangle->h = 0;

          return false;
     }

     return true;
}

void dfb_rectangle_union ( DFBRectangle       *rect1,
                           const DFBRectangle *rect2 )
{
     if (!rect2->w || !rect2->h)
          return;

     /* FIXME: OPTIMIZE */

     if (rect1->w) {
          int temp = MIN (rect1->x, rect2->x);
          rect1->w = MAX (rect1->x + rect1->w, rect2->x + rect2->w) - temp;
          rect1->x = temp;
     }
     else {
          rect1->x = rect2->x;
          rect1->w = rect2->w;
     }

     if (rect1->h) {
          int temp = MIN (rect1->y, rect2->y);
          rect1->h = MAX (rect1->y + rect1->h, rect2->y + rect2->h) - temp;
          rect1->y = temp;
     }
     else {
          rect1->y = rect2->y;
          rect1->h = rect2->h;
     }
}

void
dfb_updates_init( DFBUpdates *updates,
                  DFBRegion  *regions,
                  int         max_regions )
{
     D_ASSERT( updates != NULL );
     D_ASSERT( regions != NULL );
     D_ASSERT( max_regions > 0 );

     updates->regions     = regions;
     updates->max_regions = max_regions;
     updates->num_regions = 0;

     D_MAGIC_SET( updates, DFBUpdates );
}

void
dfb_updates_add( DFBUpdates      *updates,
                 const DFBRegion *region )
{
     int i;

     D_MAGIC_ASSERT( updates, DFBUpdates );
     DFB_REGION_ASSERT( region );
     D_ASSERT( updates->regions != NULL );
     D_ASSERT( updates->num_regions >= 0 );
     D_ASSERT( updates->num_regions <= updates->max_regions );

     D_DEBUG_AT( DFB_Updates, "%s( %p, %4d,%4d-%4dx%4d )\n", __FUNCTION__, updates,
                 DFB_RECTANGLE_VALS_FROM_REGION(region) );

     if (updates->num_regions == 0) {
          D_DEBUG_AT( DFB_Updates, "  -> added as first\n" );

          updates->regions[0]  = updates->bounding = *region;
          updates->num_regions = 1;

          return;
     }

     for (i=0; i<updates->num_regions; i++) {
          if (dfb_region_region_extends( &updates->regions[i], region ) ||
              dfb_region_region_intersects( &updates->regions[i], region ))
          {
               D_DEBUG_AT( DFB_Updates, "  -> combined with [%d] %4d,%4d-%4dx%4d\n", i,
                           DFB_RECTANGLE_VALS_FROM_REGION(&updates->regions[i]) );

               dfb_region_region_union( &updates->regions[i], region );

               dfb_region_region_union( &updates->bounding, region );

               D_DEBUG_AT( DFB_Updates, "  -> resulting in  [%d] %4d,%4d-%4dx%4d\n", i,
                           DFB_RECTANGLE_VALS_FROM_REGION(&updates->regions[i]) );

               return;
          }
     }

     if (updates->num_regions == updates->max_regions) {
          dfb_region_region_union( &updates->bounding, region );

          updates->regions[0]  = updates->bounding;
          updates->num_regions = 1;

          D_DEBUG_AT( DFB_Updates, "  -> collapsing to [0] %4d,%4d-%4dx%4d\n",
                      DFB_RECTANGLE_VALS_FROM_REGION(&updates->regions[0]) );
     }
     else {
          updates->regions[updates->num_regions++] = *region;

          dfb_region_region_union( &updates->bounding, region );

          D_DEBUG_AT( DFB_Updates, "  -> added as      [%d] %4d,%4d-%4dx%4d\n", updates->num_regions - 1,
                      DFB_RECTANGLE_VALS_FROM_REGION(&updates->regions[updates->num_regions - 1]) );
     }
}

void
dfb_updates_stat( DFBUpdates *updates,
                  int        *ret_total,
                  int        *ret_bounding )
{
     int i;

     D_MAGIC_ASSERT( updates, DFBUpdates );
     D_ASSERT( updates->regions != NULL );
     D_ASSERT( updates->num_regions >= 0 );
     D_ASSERT( updates->num_regions <= updates->max_regions );

     if (updates->num_regions == 0) {
          if (ret_total)
               *ret_total = 0;

          if (ret_bounding)
               *ret_bounding = 0;

          return;
     }

     if (ret_total) {
          int total = 0;

          for (i=0; i<updates->num_regions; i++) {
               const DFBRegion *r = &updates->regions[i];

               total += (r->x2 - r->x1 + 1) * (r->y2 - r->y1 + 1);
          }

          *ret_total = total;
     }

     if (ret_bounding)
          *ret_bounding = (updates->bounding.x2 - updates->bounding.x1 + 1) *
                          (updates->bounding.y2 - updates->bounding.y1 + 1);
}

void
dfb_updates_get_rectangles( DFBUpdates   *updates,
                            DFBRectangle *ret_rects,
                            int          *ret_num )
{
     D_MAGIC_ASSERT( updates, DFBUpdates );
     D_ASSERT( updates->regions != NULL );
     D_ASSERT( updates->num_regions >= 0 );
     D_ASSERT( updates->num_regions <= updates->max_regions );

     switch (updates->num_regions) {
          case 0:
               *ret_num = 0;
               break;

          default: {
               int n, d, total, bounding;

               dfb_updates_stat( updates, &total, &bounding );

               n = updates->max_regions - updates->num_regions + 1;
               d = n + 1;

               /* Try to optimize updates. Use individual regions only if not too much overhead. */
               if (total < bounding * n / d) {
                    *ret_num = updates->num_regions;

                    for (n=0; n<updates->num_regions; n++) {
                         ret_rects[n].x = updates->regions[n].x1;
                         ret_rects[n].y = updates->regions[n].y1;
                         ret_rects[n].w = updates->regions[n].x2 - updates->regions[n].x1;
                         ret_rects[n].h = updates->regions[n].y2 - updates->regions[n].y1;
                    }

                    break;
               }
          }
          /* fall through */

          case 1:
               *ret_num = 1;

               ret_rects[0].x = updates->bounding.x1;
               ret_rects[0].y = updates->bounding.y1;
               ret_rects[0].w = updates->bounding.x2 - updates->bounding.x1;
               ret_rects[0].h = updates->bounding.y2 - updates->bounding.y1;
               break;
     }
}

const char *
dfb_pixelformat_name( DFBSurfacePixelFormat format )
{
     int i = 0;

     do {
          if (format == dfb_pixelformat_names[i].format)
               return dfb_pixelformat_names[i].name;
     } while (dfb_pixelformat_names[i++].format != DSPF_UNKNOWN);

     return "<invalid>";
}
 
const char *
dfb_colorspace_name( DFBSurfaceColorSpace colorspace )
{
     int i = 0;

     do {
          if (colorspace == dfb_colorspace_names[i].colorspace)
               return dfb_colorspace_names[i].name;
     } while (dfb_colorspace_names[i++].colorspace != DSCS_UNKNOWN);

     return "<invalid>";
}

DFBSurfacePixelFormat
dfb_pixelformat_for_depth( int depth )
{
     switch (depth) {
          case 2:
               return DSPF_LUT2;
          case 8:
               return DSPF_LUT8;
          case 12:
               return DSPF_ARGB4444;
          case 14:
               return DSPF_ARGB2554;
          case 15:
               return DSPF_ARGB1555;
          case 16:
               return DSPF_RGB16;
          case 18:
               return DSPF_RGB18;
          case 24:
               return DSPF_RGB24;
          case 32:
               return DSPF_RGB32;
     }

     return DSPF_UNKNOWN;
}

