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

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <sys/time.h>
#include <time.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <misc/util.h>


static struct timeval start_time = { 0, 0 };

long long
dfb_get_micros()
{
     struct timeval tv;

     if (start_time.tv_sec == 0) {
          gettimeofday( &start_time, NULL );
          return 0;
     }

     gettimeofday( &tv, NULL );

     return (long long)(tv.tv_sec - start_time.tv_sec) * (long long) 1000000 +
            (long long)(tv.tv_usec - start_time.tv_usec);
}

long long
dfb_get_millis()
{
     return dfb_get_micros() / (long long) 1000;
}

bool
dfb_region_intersect( DFBRegion *region,
                      int x1, int y1, int x2, int y2 )
{
     if (region->x2 < x1 ||
         region->y2 < y1 ||
         region->x1 > x2 ||
         region->y1 > y2)
          return false;

     if (region->x1 < x1)
          region->x1 = x1;

     if (region->y1 < y1)
          region->y1 = y1;

     if (region->x2 > x2)
          region->x2 = x2;

     if (region->y2 > y2)
          region->y2 = y2;

     return true;
}

bool
dfb_region_region_intersect( DFBRegion       *region,
                             const DFBRegion *clip )
{
     if (region->x2 < clip->x1 ||
         region->y2 < clip->y1 ||
         region->x1 > clip->x2 ||
         region->y1 > clip->y2)
          return false;

     if (region->x1 < clip->x1)
          region->x1 = clip->x1;

     if (region->y1 < clip->y1)
          region->y1 = clip->y1;

     if (region->x2 > clip->x2)
          region->x2 = clip->x2;

     if (region->y2 > clip->y2)
          region->y2 = clip->y2;

     return true;
}

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
     if (region->x2 <= rectangle->x + rectangle->w)
        rectangle->w = region->x2 - rectangle->x + 1;

     if (region->y2 <= rectangle->y + rectangle->h)
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
     if (region.x2 <= rectangle->x + rectangle->w)
          rectangle->w = region.x2 - rectangle->x + 1;

     if (region.y2 <= rectangle->y + rectangle->h)
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
