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

#ifndef __UTIL_H__
#define __UTIL_H__

#include <errno.h>

#include <directfb.h>

#include <direct/types.h>
#include <direct/debug.h>

bool dfb_region_intersect( DFBRegion *region,
                           int x1, int y1, int x2, int y2 );

bool dfb_region_region_intersect( DFBRegion       *region,
                                  const DFBRegion *clip );

bool dfb_region_rectangle_intersect( DFBRegion          *region,
                                     const DFBRectangle *rect );

bool dfb_unsafe_region_intersect( DFBRegion *region,
                                  int x1, int y1, int x2, int y2 );

bool dfb_unsafe_region_rectangle_intersect( DFBRegion          *region,
                                            const DFBRectangle *rect );

bool dfb_rectangle_intersect_by_unsafe_region( DFBRectangle *rectangle,
                                               DFBRegion    *region );

bool dfb_rectangle_intersect_by_region( DFBRectangle    *rectangle,
                                        const DFBRegion *region );

bool dfb_rectangle_intersect( DFBRectangle       *rectangle,
                              const DFBRectangle *clip );

/* returns the result in the first rectangle */
void dfb_rectangle_union ( DFBRectangle       *rect1,
                           const DFBRectangle *rect2 );


#define DFB_RECTANGLE_ASSERT(r)              \
     do {                                    \
          D_ASSERT( (r) != NULL );           \
          D_ASSERT( (r)->w >= 0 );           \
          D_ASSERT( (r)->h >= 0 );           \
     } while (0)

#define DFB_RECTANGLE_ASSERT_IF(r)           \
     do {                                    \
          if ((r) != NULL) {                 \
               D_ASSERT( (r)->w >= 0 );      \
               D_ASSERT( (r)->h >= 0 );      \
          }                                  \
     } while (0)

#define DFB_RECTANGLE_VALS(r)                (r)->x, (r)->y, (r)->w, (r)->h
#define DFB_RECTANGLE_VALS_FROM_REGION(r)    (r)->x1, (r)->y1, (r)->x2-(r)->x1+1, (r)->y2-(r)->y1+1
#define DFB_RECTANGLE_INIT_FROM_REGION(r)    (DFBRectangle){ DFB_RECTANGLE_VALS_FROM_REGION(r) }


#define DFB_REGION_ASSERT(r)                 \
     do {                                    \
          D_ASSERT( (r) != NULL );           \
          D_ASSERT( (r)->x1 <= (r)->x2 );    \
          D_ASSERT( (r)->y1 <= (r)->y2 );    \
     } while (0)

#define DFB_REGION_ASSERT_IF(r)                   \
     do {                                         \
          if ((r) != NULL) {                      \
               D_ASSERT( (r)->x1 <= (r)->x2 );    \
               D_ASSERT( (r)->y1 <= (r)->y2 );    \
          }                                       \
     } while (0)


#define DFB_REGION_VALS(r)                   (r)->x1, (r)->y1, (r)->x2, (r)->y2

#define DFB_REGION_VALS_FROM_RECTANGLE(r)    (r)->x, (r)->y, (r)->x+(r)->w-1, (r)->y+(r)->h-1
#define DFB_REGION_INIT_FROM_RECTANGLE(r)    (DFBRegion){ DFB_REGION_VALS_FROM_RECTANGLE(r) }

#define DFB_REGION_VALS_FROM_RECTANGLE_VALS(x,y,w,h)   (x), (y), (x)+(w)-1, (y)+(h)-1
#define DFB_REGION_INIT_FROM_RECTANGLE_VALS(x,y,w,h)   (DFBRegion){ DFB_REGION_VALS_FROM_RECTANGLE_VALS(x,y,w,h) }

#define DFB_REGION_VALS_TRANSLATED(r,x,y)    (r)->x1 + x, (r)->y1 + y, (r)->x2 + x, (r)->y2 + y
#define DFB_REGION_INIT_TRANSLATED(r,x,y)    (DFBRegion){ DFB_REGION_VALS_TRANSLATED(r,x,y) }

#define DFB_REGION_VALS_INTERSECTED(r,X1,Y1,X2,Y2)   (r)->x1 > (X1) ? (r)->x1 : (X1),      \
                                                     (r)->y1 > (Y1) ? (r)->y1 : (Y1),      \
                                                     (r)->x2 < (X2) ? (r)->x2 : (X2),      \
                                                     (r)->y2 < (Y2) ? (r)->y2 : (Y2)
#define DFB_REGION_INIT_INTERSECTED(r,X1,Y1,X2,Y2)   (DFBRegion){ DFB_REGION_VALS_INTERSECTED(r,X1,Y1,X2,Y2) }


static inline void dfb_rectangle_from_region( DFBRectangle    *rect,
                                              const DFBRegion *region )
{
     D_ASSERT( rect != NULL );

     DFB_REGION_ASSERT( region );

     rect->x = region->x1;
     rect->y = region->y1;
     rect->w = region->x2 - region->x1 + 1;
     rect->h = region->y2 - region->y1 + 1;
}


static inline void dfb_rectangle_from_rectangle_plus_insets( DFBRectangle       *rect,
                                                             const DFBRectangle *inner,
                                                             const DFBInsets    *insets )
{
     D_ASSERT( rect != NULL );
     D_ASSERT( insets != NULL );

     DFB_RECTANGLE_ASSERT( inner );

     rect->x = inner->x - insets->l;
     rect->y = inner->y - insets->t;
     rect->w = inner->w + insets->l + insets->r;
     rect->h = inner->h + insets->t + insets->b;
}


static inline void dfb_region_from_rectangle( DFBRegion          *region,
                                              const DFBRectangle *rect )
{
     D_ASSERT( region != NULL );

     DFB_RECTANGLE_ASSERT( rect );

     region->x1 = rect->x;
     region->y1 = rect->y;
     region->x2 = rect->x + rect->w - 1;
     region->y2 = rect->y + rect->h - 1;
}


static inline void dfb_rectangle_translate( DFBRectangle *rect,
                                            int           dx,
                                            int           dy )
{
     DFB_RECTANGLE_ASSERT( rect );

     rect->x += dx;
     rect->y += dy;
}

static inline void dfb_region_translate( DFBRegion *region,
                                         int        dx,
                                         int        dy )
{
     DFB_REGION_ASSERT( region );

     region->x1 += dx;
     region->y1 += dy;
     region->x2 += dx;
     region->y2 += dy;
}

static inline void dfb_rectangle_resize( DFBRectangle *rect,
                                         int           width,
                                         int           height )
{
     DFB_RECTANGLE_ASSERT( rect );

     D_ASSERT( width >= 0 );
     D_ASSERT( height >= 0 );

     rect->w = width;
     rect->h = height;
}

static inline void dfb_region_resize( DFBRegion *region,
                                      int        width,
                                      int        height )
{
     DFB_REGION_ASSERT( region );

     D_ASSERT( width >= 0 );
     D_ASSERT( height >= 0 );

     region->x2 = region->x1 + width - 1;
     region->y2 = region->y1 + height - 1;
}

static inline bool dfb_region_intersects( const DFBRegion *region,
                                          int              x1,
                                          int              y1,
                                          int              x2,
                                          int              y2 )
{
     DFB_REGION_ASSERT( region );

     D_ASSERT( x1 <= x2 );
     D_ASSERT( y1 <= y2 );

     return (region->x1 <= x2 &&
             region->y1 <= y2 &&
             region->x2 >= x1 &&
             region->y2 >= y1);
}

static inline bool dfb_region_region_intersects( const DFBRegion *region,
                                                 const DFBRegion *other )
{
     DFB_REGION_ASSERT( region );
     DFB_REGION_ASSERT( other );

     return (region->x1 <= other->x2 &&
             region->y1 <= other->y2 &&
             region->x2 >= other->x1 &&
             region->y2 >= other->y1);
}

static inline bool dfb_rectangle_region_intersects( const DFBRectangle *rect,
                                                    const DFBRegion    *region )
{
     DFB_RECTANGLE_ASSERT( rect );

     DFB_REGION_ASSERT( region );

     return (rect->x <= region->x2 &&
             rect->y <= region->y2 &&
             rect->x + rect->w > region->x1 &&
             rect->y + rect->h > region->y1);
}

static inline void dfb_region_clip( DFBRegion *region,
                                    int        x1,
                                    int        y1,
                                    int        x2,
                                    int        y2 )
{
     DFB_REGION_ASSERT( region );

     D_ASSERT( dfb_region_intersects( region, x1, y1, x2, y2 ) );

     if (region->x1 < x1)
          region->x1 = x1;

     if (region->y1 < y1)
          region->y1 = y1;

     if (region->x2 > x2)
          region->x2 = x2;

     if (region->y2 > y2)
          region->y2 = y2;
}

#endif
