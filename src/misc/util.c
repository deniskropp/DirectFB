/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <string.h>

#include "util.h"

void trim( char **s )
{
     int i;
     int len = strlen( *s );

     for (i = len-1; i >= 0; i--)
          if ((*s)[i] <= ' ')
               (*s)[i] = 0;
          else
               break;

     while (**s)
          if (**s <= ' ')
               (*s)++;
          else
               return;
}

/*
 * translates errno to DirectFB DFBResult
 */
DFBResult errno2dfb( int erno )
{
     switch (erno) {
          case 0:
               return DFB_OK;
          case ENOENT:
               return DFB_FILENOTFOUND;
          case EACCES:
          case EPERM:
               return DFB_ACCESSDENIED;
          case EBUSY:
          case EAGAIN:
               return DFB_BUSY;
          case ENODEV:
          case ENXIO:
          case ENOTSUP:
               return DFB_UNSUPPORTED;
     }

     return DFB_FAILURE;
}

int region_intersect( DFBRegion *region,
                      int x1, int y1, int x2, int y2 )
{
     if (region->x2 < x1 ||
         region->y2 < y1 ||
         region->x1 > x2 ||
         region->y1 > y2)
          return 0;

     region->x1 = MAX( region->x1, x1 );
     region->y1 = MAX( region->y1, y1 );
     region->x2 = MIN( region->x2, x2 );
     region->y2 = MIN( region->y2, y2 );

     return 1;
}

int region_rectangle_intersect( DFBRegion *region, DFBRectangle *rect )
{
     int x2 = rect->x + rect->w - 1;
     int y2 = rect->y + rect->h - 1;

     if (region->x2 < rect->x ||
         region->y2 < rect->y ||
         region->x1 > x2 ||
         region->y1 > y2)
          return 0;

     region->x1 = MAX( region->x1, rect->x );
     region->y1 = MAX( region->y1, rect->y );
     region->x2 = MIN( region->x2, x2 );
     region->y2 = MIN( region->y2, y2 );

     return 1;
}

int unsafe_region_intersect( DFBRegion *region,
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

     return region_intersect( region, x1, y1, x2, y2 );
}

int unsafe_region_rectangle_intersect( DFBRegion *region, DFBRectangle *rect )
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

     return region_rectangle_intersect( region, rect );
}

int rectangle_intersect_by_unsafe_region( DFBRectangle *rectangle,
                                          DFBRegion    *region )
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

     if (region->x1 > rectangle->x) {
          rectangle->w -= region->x1 - rectangle->x;
          rectangle->x = region->x1;
     }

     if (region->y1 > rectangle->y) {
          rectangle->h -= region->y1 - rectangle->y;
          rectangle->y = region->y1;
     }

     if (region->x2 <= rectangle->x + rectangle->w)
        rectangle->w = region->x2 - rectangle->x + 1;

     if (region->y2 <= rectangle->y + rectangle->h)
        rectangle->h = region->y2 - rectangle->y + 1;

     return (rectangle->w > 0  &&  rectangle->h > 0);
}

int rectangle_intersect( DFBRectangle *rectangle,
                         DFBRectangle *clip )
{
     DFBRegion region = { clip->x, clip->y,
                          clip->x + clip->w - 1, clip->y + clip->h - 1 };

     if (region.x1 > rectangle->x) {
          rectangle->w -= region.x1 - rectangle->x;
          rectangle->x = region.x1;
     }

     if (region.y1 > rectangle->y) {
          rectangle->h -= region.y1 - rectangle->y;
          rectangle->y = region.y1;
     }

     if (region.x2 <= rectangle->x + rectangle->w)
          rectangle->w = region.x2 - rectangle->x + 1;

     if (region.y2 <= rectangle->y + rectangle->h)
          rectangle->h = region.y2 - rectangle->y + 1;

     return (rectangle->w > 0  &&  rectangle->h > 0);
}

void rectangle_union ( DFBRectangle *rect1,
                       DFBRectangle *rect2 )
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
