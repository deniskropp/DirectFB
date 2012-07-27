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

#ifndef __DIRECTFB_UTIL_H__
#define __DIRECTFB_UTIL_H__

#include <stdlib.h>
#include <errno.h>

#include <directfb.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <directfb_strings.h>

#include <direct/thread.h>
#include <direct/types.h>
#include <direct/debug.h>


bool DIRECTFB_API dfb_region_rectangle_intersect( DFBRegion          *region,
                                                  const DFBRectangle *rect );

bool DIRECTFB_API dfb_unsafe_region_intersect( DFBRegion *region,
                                               int x1, int y1, int x2, int y2 );

bool DIRECTFB_API dfb_unsafe_region_rectangle_intersect( DFBRegion          *region,
                                                         const DFBRectangle *rect );

bool DIRECTFB_API dfb_rectangle_intersect_by_unsafe_region( DFBRectangle *rectangle,
                                                            DFBRegion    *region );

bool DIRECTFB_API dfb_rectangle_intersect_by_region( DFBRectangle    *rectangle,
                                                     const DFBRegion *region );

bool DIRECTFB_API dfb_rectangle_intersect( DFBRectangle       *rectangle,
                                           const DFBRectangle *clip );

/* returns the result in the first rectangle */
void DIRECTFB_API dfb_rectangle_union ( DFBRectangle       *rect1,
                                        const DFBRectangle *rect2 );


/**********************************************************************************************************************/

#define DFB_RECTANGLE_CHECK(r)     \
      ((r) != NULL &&              \
       (r)->x1 <= (r)->x2 &&       \
       (r)->y1 <= (r)->y2)

#define DFB_RECTANGLE_CHECK_IF(r)  \
      ((r) == NULL ||              \
       ((r)->w >= 0 &&             \
        (r)->h >= 0))


#define DFB_RECT_FORMAT                                "%4d,%4d-%4dx%4d"

#define DFB_RECTANGLE_VALS(r)                          (r)->x, (r)->y, (r)->w, (r)->h

#define DFB_RECTANGLE_EMPTY(r)                         ((r)->w == 0 || (r)->h == 0)
#define DFB_RECTANGLE_FULL(r)                          (!DFB_RECTANGLE_EMPTY(r))

#define DFB_RECTANGLE_VALS_FROM_DIMENSION(d)           0, 0, (d)->w, (d)->h
#define DFB_RECTANGLE_INIT_FROM_DIMENSION(d)           (DFBRectangle){ DFB_RECTANGLE_VALS_FROM_DIMENSION(d) }

#define DFB_RECTANGLE_VALS_FROM_DIMENSION_VALS(w,h)    0, 0, (w), (h)
#define DFB_RECTANGLE_INIT_FROM_DIMENSION_VALS(w,h)    (DFBRectangle){ DFB_RECTANGLE_VALS_FROM_DIMENSION_VALS(w,h) }

#define DFB_RECTANGLE_VALS_FROM_REGION(r)              (r)->x1, (r)->y1, (r)->x2-(r)->x1+1, (r)->y2-(r)->y1+1
#define DFB_RECTANGLE_INIT_FROM_REGION(r)              (DFBRectangle){ DFB_RECTANGLE_VALS_FROM_REGION(r) }

#define DFB_RECTANGLE_VALS_FROM_REGION_TRANSLATED(r,x,y)    (r)->x1 + (x), (r)->y1 + (y), (r)->x2-(r)->x1+1, (r)->y2-(r)->y1+1
#define DFB_RECTANGLE_INIT_FROM_REGION_TRANSLATED(r,x,y)    (DFBRectangle){ DFB_RECTANGLE_VALS_FROM_REGION_TRANSLATED(r,x,y) }

#define DFB_RECTANGLE_VALS_FROM_BOX(b)                 (b)->x1, (b)->y1, (b)->x2-(b)->x1, (b)->y2-(b)->y1
#define DFB_RECTANGLE_INIT_FROM_BOX(b)                 (DFBRectangle){ DFB_RECTANGLE_VALS_FROM_BOX(b) }

#define DFB_RECTANGLE_VALS_FROM_BOX_TRANSLATED(b,x,y)  (b)->x1 + (x), (b)->y1 + (y), (b)->x2-(b)->x1, (b)->y2-(b)->y1
#define DFB_RECTANGLE_INIT_FROM_BOX_TRANSLATED(b,x,y)  (DFBRectangle){ DFB_RECTANGLE_VALS_FROM_BOX_TRANSLATED(b,x,y) }

#define DFB_RECTANGLE_VALS_FROM_BOX_VALS(x1,y1,x2,y2)  (b)->x1, (b)->y1, (b)->x2-(b)->x1, (b)->y2-(b)->y1
#define DFB_RECTANGLE_INIT_FROM_BOX_VALS(x1,y1,x2,y2)  (DFBRectangle){ DFB_RECTANGLE_VALS_FROM_BOX_VALS(x1,y1,x2,y2) }

#define DFB_RECTANGLE_CONTAINS_POINT(r,X,Y)            (((X) >= (r)->x) && ((X) < (r)->x + (r)->w) && \
                                                        ((Y) >= (r)->y) && ((Y) < (r)->y + (r)->h))

/**********************************************************************************************************************/

#define DFB_REGION_CHECK(r)     \
      ((r) != NULL &&           \
       (r)->x1 <= (r)->x2 &&    \
       (r)->y1 <= (r)->y2)

#define DFB_REGION_CHECK_IF(r)  \
      ((r) == NULL ||           \
       ((r)->x1 <= (r)->x2 &&   \
        (r)->y1 <= (r)->y2))


#define DFB_REGION_FORMAT                              "%4d,%4d-%4d,%4d"

#define DFB_REGION_VALS(r)                             (r)->x1, (r)->y1, (r)->x2, (r)->y2

#define DFB_REGION_VALS_FROM_DIMENSION(d)              0, 0, (d)->w-1, (d)->h-1
#define DFB_REGION_INIT_FROM_DIMENSION(d)              (DFBRegion){ DFB_REGION_VALS_FROM_DIMENSION(d) }

#define DFB_REGION_VALS_FROM_RECTANGLE(r)              (r)->x, (r)->y, (r)->x+(r)->w-1, (r)->y+(r)->h-1
#define DFB_REGION_INIT_FROM_RECTANGLE(r)              (DFBRegion){ DFB_REGION_VALS_FROM_RECTANGLE(r) }

#define DFB_REGION_VALS_FROM_RECTANGLE_VALS(x,y,w,h)   (x), (y), (x)+(w)-1, (y)+(h)-1
#define DFB_REGION_INIT_FROM_RECTANGLE_VALS(x,y,w,h)   (DFBRegion){ DFB_REGION_VALS_FROM_RECTANGLE_VALS(x,y,w,h) }

#define DFB_REGION_VALS_FROM_BOX(b)                    (b)->x1, (b)->y1, (b)->x2-1, (b)->y2-1
#define DFB_REGION_INIT_FROM_BOX(b)                    (DFBRegion){ DFB_REGION_VALS_FROM_BOX(b) }

#define DFB_REGION_VALS_TRANSLATED(r,x,y)              (r)->x1 + x, (r)->y1 + y, (r)->x2 + x, (r)->y2 + y
#define DFB_REGION_INIT_TRANSLATED(r,x,y)              (DFBRegion){ DFB_REGION_VALS_TRANSLATED(r,x,y) }

#define DFB_REGION_VALS_INTERSECTED(r,X1,Y1,X2,Y2)     (r)->x1 > (X1) ? (r)->x1 : (X1),      \
                                                       (r)->y1 > (Y1) ? (r)->y1 : (Y1),      \
                                                       (r)->x2 < (X2) ? (r)->x2 : (X2),      \
                                                       (r)->y2 < (Y2) ? (r)->y2 : (Y2)
#define DFB_REGION_INIT_INTERSECTED(r,X1,Y1,X2,Y2)     (DFBRegion){ DFB_REGION_VALS_INTERSECTED(r,X1,Y1,X2,Y2) }


#define DFB_REGION_CONTAINS_POINT(r,X,Y)               (((X) >= (r)->x1) && ((X) <= (r)->x2) && \
                                                        ((Y) >= (r)->y1) && ((Y) <= (r)->y2))

/**********************************************************************************************************************/

#define DFB_BOX_CHECK(b)      \
      ((b) != NULL &&         \
       (b)->x1 <= (b)->x2 &&  \
       (b)->y1 <= (b)->y2)

#define DFB_BOX_CHECK_IF(r)   \
      ((b) == NULL ||         \
       ((b)->x1 <= (b)->x2 && \
        (b)->y1 <= (b)->y2))


#define DFB_BOX_VALS(b)                                (b)->x1, (b)->y1, (b)->x2, (b)->y2
#define DFB_BOX_INIT(x1,y1,x2,y2)                      (DFBBox){ x1, y1, x2, y2 }

#define DFB_BOX_WIDTH(b)                               ((b)->x2 - (b)->x1)
#define DFB_BOX_HEIGHT(b)                              ((b)->y2 - (b)->y1)
#define DFB_BOX_SIZE(b)                                (DFB_BOX_WIDTH(b) * DFB_BOX_HEIGHT(b))

#define DFB_BOX_EQUAL(b1,b2)                           (((b1) == (b2)) || ( (b1)->x1 == (b2)->x1 && (b1)->y1 == (b2)->y1 && (b1)->x2 == (b2)->x2 && (b1)->y2 == (b2)->y2 ))
#define DFB_BOX_EMPTY(b)                               ((b)->x1 == (b)->x2 || (b)->y1 == (b)->y2)
#define DFB_BOX_FULL(b)                                (!DFB_BOX_EMPTY(b))
#define DFB_BOX_RESET(b)                               do { (b)->x2 = (b)->x1; (b)->y2 = (b)->y1; } while (0)

#define DFB_BOX_VALS_FROM_DIMENSION(d)                 0, 0, (d)->w, (d)->h
#define DFB_BOX_INIT_FROM_DIMENSION(d)                 (DFBBox){ DFB_BOX_VALS_FROM_DIMENSION(d) }

#define DFB_BOX_VALS_FROM_DIMENSION_VALS(w,h)          0, 0, (w), (h)
#define DFB_BOX_INIT_FROM_DIMENSION_VALS(w,h)          (DFBBox){ DFB_BOX_VALS_FROM_DIMENSION_VALS(w,h) }

#define DFB_BOX_VALS_FROM_RECTANGLE(r)                 (r)->x, (r)->y, (r)->x+(r)->w, (r)->y+(r)->h
#define DFB_BOX_INIT_FROM_RECTANGLE(r)                 (DFBBox){ DFB_BOX_VALS_FROM_RECTANGLE(r) }

#define DFB_BOX_VALS_FROM_RECTANGLE_VALS(x,y,w,h)      (x), (y), (x)+(w), (y)+(h)
#define DFB_BOX_INIT_FROM_RECTANGLE_VALS(X,Y,W,H)      (DFBBox){ DFB_BOX_VALS_FROM_RECTANGLE_VALS(X,Y,W,H) }

#define DFB_BOX_VALS_FROM_REGION(r)                    (r)->x1, (r)->y1, (r)->x2+1, (r)->y2+1
#define DFB_BOX_INIT_FROM_REGION(r)                    (DFBBox){ DFB_BOX_VALS_FROM_REGION(r) }

#define DFB_BOX_VALS_TRANSLATED(b,x,y)                 (b)->x1 + x, (b)->y1 + y, (b)->x2 + x, (b)->y2 + y
#define DFB_BOX_INIT_TRANSLATED(b,x,y)                 (DFBBox){ DFB_BOX_VALS_TRANSLATED(b,x,y) }

#define DFB_BOX_VALS_AT_ZERO(b)                        0, 0, DFB_BOX_WIDTH(b), DFB_BOX_HEIGHT(b)
#define DFB_BOX_INIT_AT_ZERO(b)                        (DFBBox){ DFB_BOX_VALS_AT_ZERO(b) }

/**********************************************************************************************************************/

#define DFB_DIMENSION_VALS(d)                          (d)->w, (d)->h
#define DFB_DIMENSION_INIT(w,h)                        (DFBDimension){ w, h }

#define DFB_DIMENSION_VALS_FROM_BOX(b)                 DFB_BOX_WIDTH(b), DFB_BOX_HEIGHT(b)
#define DFB_DIMENSION_INIT_FROM_BOX(b)                 (DFBDimension){ DFB_DIMENSION_VALS_FROM_BOX(b) }

/**********************************************************************************************************************/
/**********************************************************************************************************************/

#define DFB_LINE_VALS(l)                               (l)->x1, (l)->y1, (l)->x2, (l)->y2

/**********************************************************************************************************************/

#define DFB_POINT_VALS(p)                              (p)->x, (p)->y
#define DFB_POINT_INIT(x,y)                            (DFBPoint){ x, y }

#define DFB_POINT_VALS_FROM_BOX(b)                     (b)->x1, (b)->y1
#define DFB_POINT_INIT_FROM_BOX(b)                     (DFBPoint){ DFB_POINT_VALS_FROM_BOX(b) }

/**********************************************************************************************************************/

#define DFB_TRIANGLE_FORMAT                            "%4d,%4d-%4d,%4d-%4d,%4d"

#define DFB_TRIANGLE_VALS(t)                           (t)->x1, (t)->y1, (t)->x2, (t)->y2, (t)->x3, (t)->y3
#define DFB_TRIANGLE_INIT(x1,y1,x2,y2,x3,y3)           (DFBTriangle){ x1, y1, x2, y2, x3, y3 }

/**********************************************************************************************************************/

#define DFB_SPAN_VALS(s)                               (s)->x, (s)->w
#define DFB_SPAN_INIT(x,w)                             (DFBSpan){ x, w }
#define DFB_SPAN_VALS_AT(s,y)                          (s)->x, y, (s)->w

/**********************************************************************************************************************/

#define DFB_COLOR_FORMAT                               "%02x %02x %02x %02x"

#define DFB_COLOR_VALS(c)                              (c)->a, (c)->r, (c)->g, (c)->b
#define DFB_COLOR_INIT(a,r,g,b)                        (DFBColor){ a, r, g, b }

/**********************************************************************************************************************/

#define DFB_COLORKEY_VALS(c)                           (c)->r, (c)->g, (c)->b, (c)->index
#define DFB_COLORKEY_INIT(r,g,b,index)                 (DFBColorKey){ r, g, b, index }

/**********************************************************************************************************************/
/**********************************************************************************************************************/

#if D_DEBUG_ENABLED

#define DFB_RECTANGLE_ASSERT(r)                                                                               \
     do {                                                                                                     \
          D_ASSERT( (r) != NULL );                                                                            \
          D_ASSERT( (r)->w >= 0 );                                                                            \
          D_ASSERT( (r)->h >= 0 );                                                                            \
     } while (0)

#define DFB_RECTANGLE_ASSERT_IF(r)                                                                            \
     do {                                                                                                     \
          if ((r) != NULL) {                                                                                  \
               D_ASSERT( (r)->w >= 0 );                                                                       \
               D_ASSERT( (r)->h >= 0 );                                                                       \
          }                                                                                                   \
     } while (0)

#define DFB_REGION_ASSERT(r)                                                                                  \
     do {                                                                                                     \
          D_ASSERT( (r) != NULL );                                                                            \
          D_ASSERT( (r)->x1 <= (r)->x2 );                                                                     \
          D_ASSERT( (r)->y1 <= (r)->y2 );                                                                     \
     } while (0)

#define DFB_REGION_ASSERT_IF(r)                                                                               \
     do {                                                                                                     \
          if ((r) != NULL) {                                                                                  \
               D_ASSERT( (r)->x1 <= (r)->x2 );                                                                \
               D_ASSERT( (r)->y1 <= (r)->y2 );                                                                \
          }                                                                                                   \
     } while (0)

#define DFB_BOX_ASSERT(b)                                                                                     \
     do {                                                                                                     \
          D_ASSERT( (b) != NULL );                                                                            \
          D_ASSERT( (b)->x1 <= (b)->x2 );                                                                     \
          D_ASSERT( (b)->y1 <= (b)->y2 );                                                                     \
     } while (0)

#define DFB_BOX_ASSERT_IF(b)                                                                                  \
     do {                                                                                                     \
          if ((b) != NULL) {                                                                                  \
               D_ASSERT( (b)->x1 <= (b)->x2 );                                                                \
               D_ASSERT( (b)->y1 <= (b)->y2 );                                                                \
          }                                                                                                   \
     } while (0)


#define DFB_RECTANGLES_DEBUG_AT( Domain, rects, num )                                                         \
     do {                                                                                                     \
          unsigned int i;                                                                                     \
                                                                                                              \
          for (i=0; i<(num); i++)                                                                             \
               D_DEBUG_AT( Domain, "  -> [%3d] %4d,%4d-%4dx%4d\n", i, DFB_RECTANGLE_VALS(&(rects)[i]) );      \
     } while (0)

#define DFB_RECTANGLES2_DEBUG_AT( Domain, rects, rects2, num )                                                \
     do {                                                                                                     \
          unsigned int i;                                                                                     \
                                                                                                              \
          for (i=0; i<(num); i++)                                                                             \
               D_DEBUG_AT( Domain, "  -> [%3d] %4d,%4d-%4dx%4d <- %4d,%4d-%4dx%4d\n", i,                      \
                                   DFB_RECTANGLE_VALS(&(rects)[i]), DFB_RECTANGLE_VALS(&(rects2)[i]) );       \
     } while (0)

#define DFB_RECTANGLES_POINTS_DEBUG_AT( Domain, rects, points, num )                                          \
     do {                                                                                                     \
          unsigned int i;                                                                                     \
                                                                                                              \
          for (i=0; i<(num); i++)                                                                             \
               D_DEBUG_AT( Domain, "  -> [%3d] %4d,%4d-%4dx%4d <- %4d,%4d\n", i,                              \
                                   DFB_RECTANGLE_VALS(&(rects)[i]), DFB_POINT_VALS(&(points)[i]) );           \
     } while (0)


#define DFB_BOXES2_DEBUG_AT( Domain, boxes, boxes2, num )                                                     \
     do {                                                                                                     \
          unsigned int i;                                                                                     \
                                                                                                              \
          for (i=0; i<(num); i++)                                                                             \
               D_DEBUG_AT( Domain, "  -> [%3d] %4d,%4d-%4dx%4d <- %4d,%4d-%4dx%4d\n", i,                      \
                                   DFB_RECTANGLE_VALS_FROM_BOX(&(boxes)[i]), DFB_RECTANGLE_VALS_FROM_BOX(&(boxes2)[i]) );       \
     } while (0)


#define DFB_REGIONS_LOG( Domain, LEVEL, regs, num )                                                             \
     do {                                                                                                     \
          unsigned int i;                                                                                     \
                                                                                                              \
          for (i=0; i<(num); i++)                                                                             \
               D_DEBUG_AT( Domain, "  -> [%3d] %4d,%4d-%4dx%4d\n", i, DFB_RECTANGLE_VALS_FROM_REGION(&((regs)[i])) );\
     } while (0)

#define DFB_REGIONS_DEBUG_AT( Domain, regs, num )                                                             \
     DFB_REGIONS_LOG( Domain, DEBUG, regs, num )                                                              \


#define DFB_BOXES_LOG( Domain, _LEVEL, boxes, num )                                                           \
     do {                                                                                                     \
          unsigned int i;                                                                                     \
                                                                                                              \
          for (i=0; i<(num); i++)                                                                             \
               D_LOG( Domain, _LEVEL, "  -> [%3d] %4d,%4d-%4dx%4d\n", i, DFB_RECTANGLE_VALS_FROM_BOX(&(boxes)[i]) );\
     } while (0)

#define DFB_BOXES_DEBUG_AT( Domain, boxes, num )                                                              \
     do {                                                                                                     \
          DFB_BOXES_LOG( Domain, DEBUG, boxes, num );                                                         \
     } while (0)

#define DFB_LINES_DEBUG_AT( Domain, lines, num )                                                              \
     do {                                                                                                     \
          unsigned int i;                                                                                     \
                                                                                                              \
          for (i=0; i<(num); i++)                                                                             \
               D_DEBUG_AT( Domain, "  -> [%3d] %4d,%4d-%4d,%4d\n", i, DFB_LINE_VALS(&(lines)[i]) );           \
     } while (0)

#define DFB_POINTS_DEBUG_AT( Domain, points, num )                                                            \
     do {                                                                                                     \
          unsigned int i;                                                                                     \
                                                                                                              \
          for (i=0; i<(num); i++)                                                                             \
               D_DEBUG_AT( Domain, "  -> [%3d] %4d,%4d\n", i, DFB_POINT_VALS(&(points)[i]) );                 \
     } while (0)

#define DFB_TRIANGLES_DEBUG_AT( Domain, tris, num )                                                           \
     do {                                                                                                     \
          unsigned int i;                                                                                     \
                                                                                                              \
          for (i=0; i<(num); i++)                                                                             \
               D_DEBUG_AT( Domain, "  -> [%3d] %4d,%4d-%4d,%4d-%4d,%4d\n", i, DFB_TRIANGLE_VALS(&(tris)[i]) );\
     } while (0)

#define DFB_SPANS_DEBUG_AT( Domain, spans, num, y )                                                           \
     do {                                                                                                     \
          unsigned int i;                                                                                     \
                                                                                                              \
          for (i=0; i<(num); i++)                                                                             \
               D_DEBUG_AT( Domain, "  -> [%3d] %4d,%4d-%4d\n", i, DFB_SPAN_VALS_AT(&(spans)[i],y+i) );        \
     } while (0)


#else

#define DFB_RECTANGLE_ASSERT(r)                                                                               \
     do {                                                                                                     \
     } while (0)

#define DFB_RECTANGLE_ASSERT_IF(r)                                                                            \
     do {                                                                                                     \
     } while (0)

#define DFB_REGION_ASSERT(r)                                                                                  \
     do {                                                                                                     \
     } while (0)

#define DFB_REGION_ASSERT_IF(r)                                                                               \
     do {                                                                                                     \
     } while (0)

#define DFB_BOX_ASSERT(b)                                                                                     \
     do {                                                                                                     \
     } while (0)

#define DFB_BOX_ASSERT_IF(b)                                                                                  \
     do {                                                                                                     \
     } while (0)


#define DFB_RECTANGLES_DEBUG_AT( Domain, rects, num )                                                         \
     do {                                                                                                     \
     } while (0)

#define DFB_RECTANGLES2_DEBUG_AT( Domain, rects, rects2, num )                                                \
     do {                                                                                                     \
     } while (0)

#define DFB_RECTANGLES_POINTS_DEBUG_AT( Domain, rects, points, num )                                          \
     do {                                                                                                     \
     } while (0)

#define DFB_REGIONS_DEBUG_AT( Domain, regs, num )                                                             \
     do {                                                                                                     \
     } while (0)

#define DFB_BOXES_LOG( Domain, _LEVEL, boxes, num )                                                           \
     do {                                                                                                     \
     } while (0)

#define DFB_BOXES_DEBUG_AT( Domain, boxes, num )                                                              \
     do {                                                                                                     \
     } while (0)

#define DFB_LINES_DEBUG_AT( Domain, lines, num )                                                              \
     do {                                                                                                     \
     } while (0)

#define DFB_POINTS_DEBUG_AT( Domain, points, num )                                                            \
     do {                                                                                                     \
     } while (0)

#define DFB_TRIANGLES_DEBUG_AT( Domain, tris, num )                                                           \
     do {                                                                                                     \
     } while (0)

#define DFB_SPANS_DEBUG_AT( Domain, spans, num, y )                                                           \
     do {                                                                                                     \
     } while (0)

#endif

/**********************************************************************************************************************/
/**********************************************************************************************************************/



static __inline__ void
dfb_rectangle_from_region( DFBRectangle    *rect,
                           const DFBRegion *region )
{
     D_ASSERT( rect != NULL );
     DFB_REGION_ASSERT( region );

     rect->x = region->x1;
     rect->y = region->y1;
     rect->w = region->x2 - region->x1 + 1;
     rect->h = region->y2 - region->y1 + 1;
}

static __inline__ void
dfb_rectangle_from_box( DFBRectangle *rect,
                        const DFBBox *box )
{
     D_ASSERT( rect != NULL );
     DFB_BOX_ASSERT( box );

     rect->x = box->x1;
     rect->y = box->y1;
     rect->w = box->x2 - box->x1;
     rect->h = box->y2 - box->y1;
}

static __inline__ void
dfb_rectangle_from_rectangle_plus_insets( DFBRectangle       *rect,
                                          const DFBRectangle *inner,
                                          const DFBInsets    *insets )
{
     D_ASSERT( rect != NULL );
     DFB_RECTANGLE_ASSERT( inner );
     D_ASSERT( insets != NULL );

     rect->x = inner->x - insets->l;
     rect->y = inner->y - insets->t;
     rect->w = inner->w + insets->l + insets->r;
     rect->h = inner->h + insets->t + insets->b;
}

static __inline__ void
dfb_rectangle_from_rotated( DFBRectangle       *rectangle,
                            const DFBRectangle *from,
                            const DFBDimension *size,
                            int                 rotation )
{
     D_ASSERT( rectangle != NULL );

     DFB_RECTANGLE_ASSERT( from );
     D_ASSERT( size != NULL );
     D_ASSERT( size->w > 0 );
     D_ASSERT( size->h > 0 );
     D_ASSUME( rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270 );

     switch (rotation) {
          default:
               D_BUG( "invalid rotation %d", rotation );
          case 0:
               *rectangle = *from;
               break;

          case 90:
               rectangle->x = from->y;
               rectangle->y = size->w - from->x - from->w;
               rectangle->w = from->h;
               rectangle->h = from->w;
               break;

          case 180:
               rectangle->x = size->w - from->x - from->w;
               rectangle->y = size->h - from->y - from->h;
               rectangle->w = from->w;
               rectangle->h = from->h;
               break;

          case 270:
               rectangle->x = size->h - from->y - from->h;
               rectangle->y = from->x;
               rectangle->w = from->h;
               rectangle->h = from->w;
               break;
     }

     DFB_RECTANGLE_ASSERT( rectangle );
}

/**********************************************************************************************************************/

static __inline__ void
dfb_rectangle_translate( DFBRectangle *rect,
                         int           dx,
                         int           dy )
{
     DFB_RECTANGLE_ASSERT( rect );

     rect->x += dx;
     rect->y += dy;
}

static __inline__ void
dfb_rectangle_resize( DFBRectangle *rect,
                      int           width,
                      int           height )
{
     DFB_RECTANGLE_ASSERT( rect );

     D_ASSERT( width >= 0 );
     D_ASSERT( height >= 0 );

     rect->w = width;
     rect->h = height;
}

/**********************************************************************************************************************/

static __inline__ bool
dfb_rectangle_region_intersects( const DFBRectangle *rect,
                                 const DFBRegion    *region )
{
     DFB_RECTANGLE_ASSERT( rect );

     DFB_REGION_ASSERT( region );

     return (rect->x <= region->x2 &&
             rect->y <= region->y2 &&
             rect->x + rect->w > region->x1 &&
             rect->y + rect->h > region->y1);
}

static __inline__ bool
dfb_rectangle_box_intersect( DFBRectangle *rect,
                             const DFBBox *box )
{
     DFB_RECTANGLE_ASSERT( rect );
     DFB_BOX_ASSERT( box );

     if (DFB_BOX_EMPTY( box ) ||
         box->x2 <= rect->x ||
         box->y2 <= rect->y ||
         box->x1 >= rect->x + rect->w ||
         box->y1 >= rect->y + rect->h)
     {
          rect->w = rect->h = 0;

          return false;
     }

     if (rect->x < box->x1) {
          rect->w -= box->x1 - rect->x;
          rect->x  = box->x1;
     }

     if (rect->y < box->y1) {
          rect->h -= box->y1 - rect->y;
          rect->y  = box->y1;
     }

     if (rect->x + rect->w > box->x2)
          rect->w = box->x2 - rect->x;

     if (rect->y + rect->h > box->y2)
          rect->h = box->y2 - rect->y;

     return true;
}

static __inline__ void
dfb_rectangle_subtract( DFBRectangle    *rect,
                        const DFBInsets *insets )
{
     D_ASSERT( rect != NULL );
     D_ASSERT( insets != NULL );

     rect->x += insets->l;
     rect->y += insets->t;
     rect->w -= insets->l + insets->r;
     rect->h -= insets->t + insets->b;

     if (rect->w <= 0 || rect->h <= 0)
          rect->w = rect->h = 0;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static __inline__ void
dfb_region_from_rectangle( DFBRegion          *region,
                           const DFBRectangle *rect )
{
     D_ASSERT( region != NULL );

     DFB_RECTANGLE_ASSERT( rect );

     D_ASSERT( rect->w > 0 );
     D_ASSERT( rect->h > 0 );

     region->x1 = rect->x;
     region->y1 = rect->y;
     region->x2 = rect->x + rect->w - 1;
     region->y2 = rect->y + rect->h - 1;
}

static __inline__ void
dfb_region_from_rectangle_translated( DFBRegion          *region,
                                      const DFBRectangle *rect,
                                      const DFBPoint     *offset )
{
     D_ASSERT( region != NULL );
     D_ASSERT( offset != NULL );

     DFB_RECTANGLE_ASSERT( rect );

     D_ASSERT( rect->w > 0 );
     D_ASSERT( rect->h > 0 );

     region->x1 = offset->x + rect->x;
     region->y1 = offset->y + rect->y;
     region->x2 = offset->x + rect->x + rect->w - 1;
     region->y2 = offset->y + rect->y + rect->h - 1;
}

static __inline__ void
dfb_region_from_rotated( DFBRegion          *region,
                         const DFBRegion    *from,
                         const DFBDimension *size,
                         int                 rotation )
{
     D_ASSERT( region != NULL );

     DFB_REGION_ASSERT( from );
     D_ASSERT( size != NULL );
     D_ASSERT( size->w > 0 );
     D_ASSERT( size->h > 0 );
     D_ASSUME( rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270 );

     switch (rotation) {
          default:
               D_BUG( "invalid rotation %d", rotation );
          case 0:
               *region = *from;
               break;

          case 90:
               region->x1 = from->y1;
               region->y1 = size->w - from->x2 - 1;
               region->x2 = from->y2;
               region->y2 = size->w - from->x1 - 1;
               break;

          case 180:
               region->x1 = size->w - from->x2 - 1;
               region->y1 = size->h - from->y2 - 1;
               region->x2 = size->w - from->x1 - 1;
               region->y2 = size->h - from->y1 - 1;
               break;

          case 270:
               region->x1 = size->h - from->y2 - 1;
               region->y1 = from->x1;
               region->x2 = size->h - from->y1 - 1;
               region->y2 = from->x2;
               break;
     }

     DFB_REGION_ASSERT( region );
}

/**********************************************************************************************************************/

static __inline__ void
dfb_region_translate( DFBRegion *region,
                      int        dx,
                      int        dy )
{
     DFB_REGION_ASSERT( region );

     region->x1 += dx;
     region->y1 += dy;
     region->x2 += dx;
     region->y2 += dy;
}

static __inline__ void
dfb_region_resize( DFBRegion *region,
                   int        width,
                   int        height )
{
     DFB_REGION_ASSERT( region );

     D_ASSERT( width >= 0 );
     D_ASSERT( height >= 0 );

     region->x2 = region->x1 + width - 1;
     region->y2 = region->y1 + height - 1;
}

/**********************************************************************************************************************/

static __inline__ bool
dfb_region_intersects( const DFBRegion *region,
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

static __inline__ bool
dfb_region_region_intersects( const DFBRegion *region,
                              const DFBRegion *other )
{
     DFB_REGION_ASSERT( region );
     DFB_REGION_ASSERT( other );

     return (region->x1 <= other->x2 &&
             region->y1 <= other->y2 &&
             region->x2 >= other->x1 &&
             region->y2 >= other->y1);
}

static __inline__ bool
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

static __inline__ bool
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

/**********************************************************************************************************************/

static __inline__ bool
dfb_region_region_contains( const DFBRegion *a,
                            const DFBRegion *b )
{
     if (a->x1 > b->x1)
          return false;

     if (a->y1 > b->y1)
          return false;

     if (a->x2 < b->x2)
          return false;

     if (a->y2 < b->y2)
          return false;

     return true;
}

static __inline__ bool
dfb_region_region_extends( const DFBRegion *a,
                           const DFBRegion *b )
{
     if (a->x1 == b->x1 && a->x2 == b->x2)
          return (a->y1 == b->y2 - 1) || (a->y2 == b->y1 - 1);

     if (a->y1 == b->y1 && a->y2 == b->y2)
          return (a->x1 == b->x2 - 1) || (a->x2 == b->x1 - 1);

     return false;
}

static __inline__ void
dfb_region_region_union( DFBRegion       *region,
                         const DFBRegion *other )
{
     DFB_REGION_ASSERT( region );
     DFB_REGION_ASSERT( other );

     if (region->x1 > other->x1)
          region->x1 = other->x1;

     if (region->y1 > other->y1)
          region->y1 = other->y1;

     if (region->x2 < other->x2)
          region->x2 = other->x2;

     if (region->y2 < other->y2)
          region->y2 = other->y2;
}

static __inline__ void
dfb_region_clip( DFBRegion *region,
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

static __inline__ void
dfb_regions_unite( DFBRegion       *united,
                   const DFBRegion *regions,
                   unsigned int     num_regions )
{
     unsigned int i;

     D_ASSERT( united != NULL );
     D_ASSERT( regions != NULL );
     D_ASSERT( num_regions > 0 );

     *united = regions[0];

     for (i=1; i<num_regions; i++) {
          DFB_REGION_ASSERT( &regions[i] );

          dfb_region_region_union( united, &regions[i] );
     }
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static __inline__ void
dfb_box_from_rectangle( DFBBox             *box,
                        const DFBRectangle *rect )
{
     D_ASSERT( box != NULL );

     DFB_RECTANGLE_ASSERT( rect );

     box->x1 = rect->x;
     box->y1 = rect->y;
     box->x2 = rect->x + rect->w;
     box->y2 = rect->y + rect->h;
}

static __inline__ void
dfb_box_from_rectangle_translated( DFBBox             *box,
                                   const DFBRectangle *rect,
                                   const DFBPoint     *offset )
{
     D_ASSERT( box != NULL );
     DFB_RECTANGLE_ASSERT( rect );
     D_ASSERT( offset != NULL );

     box->x1 = offset->x + rect->x;
     box->y1 = offset->y + rect->y;
     box->x2 = offset->x + rect->x + rect->w;
     box->y2 = offset->y + rect->y + rect->h;
}

static __inline__ void
dfb_box_from_rotated( DFBBox             *box,
                      const DFBBox       *from,
                      const DFBDimension *size,
                      int                 rotation )
{
     D_ASSERT( box != NULL );

     DFB_REGION_ASSERT( from );
     D_ASSERT( size != NULL );
     D_ASSERT( size->w > 0 );
     D_ASSERT( size->h > 0 );
     D_ASSUME( rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270 );

     switch (rotation) {
          default:
               D_BUG( "invalid rotation %d", rotation );
          case 0:
               *box = *from;
               break;

          case 90:
               box->x1 = from->y1;
               box->y1 = size->w - from->x2;
               box->x2 = from->y2;
               box->y2 = size->w - from->x1;
               break;

          case 180:
               box->x1 = size->w - from->x2;
               box->y1 = size->h - from->y2;
               box->x2 = size->w - from->x1;
               box->y2 = size->h - from->y1;
               break;

          case 270:
               box->x1 = size->h - from->y2;
               box->y1 = from->x1;
               box->x2 = size->h - from->y1;
               box->y2 = from->x2;
               break;
     }

     DFB_REGION_ASSERT( box );
}

/**********************************************************************************************************************/

static __inline__ void
dfb_box_translate( DFBBox *box,
                   int     dx,
                   int     dy )
{
     DFB_REGION_ASSERT( box );

     box->x1 += dx;
     box->y1 += dy;
     box->x2 += dx;
     box->y2 += dy;
}

static __inline__ void
dfb_box_resize( DFBBox *box,
                int     width,
                int     height )
{
     DFB_REGION_ASSERT( box );

     D_ASSERT( width >= 0 );
     D_ASSERT( height >= 0 );

     box->x2 = box->x1 + width;
     box->y2 = box->y1 + height;
}

/**********************************************************************************************************************/

static __inline__ bool
dfb_box_box_contains( const DFBBox *a,
                      const DFBBox *b )
{
     if (a->x1 > b->x1)
          return false;

     if (a->y1 > b->y1)
          return false;

     if (a->x2 < b->x2)
          return false;

     if (a->y2 < b->y2)
          return false;

     return true;
}

static __inline__ bool
dfb_box_intersects( const DFBBox *box,
                    int           x1,
                    int           y1,
                    int           x2,
                    int           y2 )
{
     DFB_BOX_ASSERT( box );

     D_ASSERT( x1 <= x2 );
     D_ASSERT( y1 <= y2 );

     return (box->x1 < x2 &&
             box->y1 < y2 &&
             box->x2 > x1 &&
             box->y2 > y1);
}

static __inline__ bool
dfb_box_box_intersects( const DFBBox *box,
                        const DFBBox *other )
{
     DFB_BOX_ASSERT( box );
     DFB_BOX_ASSERT( other );

     return (box->x1 < other->x2 &&
             box->y1 < other->y2 &&
             box->x2 > other->x1 &&
             box->y2 > other->y1);
}

static __inline__ bool
dfb_box_intersect( DFBBox *box,
                   int x1, int y1, int x2, int y2 )
{
     DFB_BOX_ASSERT( box );

     D_ASSERT( x1 <= x2 );
     D_ASSERT( y1 <= y2 );

     if (DFB_BOX_EMPTY( box ))
          return false;

     if (box->x2 <= x1 ||
         box->y2 <= y1 ||
         box->x1 >= x2 ||
         box->y1 >= y2)
          return false;

     if (box->x1 < x1)
          box->x1 = x1;

     if (box->y1 < y1)
          box->y1 = y1;

     if (box->x2 > x2)
          box->x2 = x2;

     if (box->y2 > y2)
          box->y2 = y2;

     return true;
}

static __inline__ bool
dfb_box_box_intersect( DFBBox       *box,
                       const DFBBox *clip )
{
     DFB_BOX_ASSERT( box );
     DFB_BOX_ASSERT( clip );

     if (DFB_BOX_EMPTY( box ))
          return false;

     if (DFB_BOX_EMPTY( clip ) ||
         box->x2 <= clip->x1 ||
         box->y2 <= clip->y1 ||
         box->x1 >= clip->x2 ||
         box->y1 >= clip->y2)
     {
          return false;
     }

     if (box->x1 < clip->x1)
          box->x1 = clip->x1;

     if (box->y1 < clip->y1)
          box->y1 = clip->y1;

     if (box->x2 > clip->x2)
          box->x2 = clip->x2;

     if (box->y2 > clip->y2)
          box->y2 = clip->y2;

     return true;
}

static __inline__ bool
dfb_box_region_intersects( const DFBBox    *box,
                           const DFBRegion *clip )
{
     DFB_BOX_ASSERT( box );
     D_ASSERT( clip != NULL );

     if (DFB_BOX_EMPTY( box ))
          return false;

     if (clip->x1 > clip->x2 ||
         clip->y1 > clip->y2 ||
         box->x2 <= clip->x1 ||
         box->y2 <= clip->y1 ||
         box->x1 > clip->x2 ||
         box->y1 > clip->y2)
     {
          return false;
     }

     return true;
}

static __inline__ bool
dfb_box_region_intersect( DFBBox          *box,
                          const DFBRegion *clip )
{
     DFB_BOX_ASSERT( box );
     D_ASSERT( clip != NULL );

     if (DFB_BOX_EMPTY( box ))
          return false;

     if (clip->x1 > clip->x2 ||
         clip->y1 > clip->y2 ||
         box->x2 <= clip->x1 ||
         box->y2 <= clip->y1 ||
         box->x1 > clip->x2 ||
         box->y1 > clip->y2)
     {
          box->x2 = box->x1;
          box->y2 = box->y1;

          return false;
     }

     if (box->x1 < clip->x1)
          box->x1 = clip->x1;

     if (box->y1 < clip->y1)
          box->y1 = clip->y1;

     if (box->x2 > clip->x2 + 1)
          box->x2 = clip->x2 + 1;

     if (box->y2 > clip->y2 + 1)
          box->y2 = clip->y2 + 1;

     return true;
}

/**********************************************************************************************************************/

static __inline__ bool
dfb_box_box_extends( const DFBBox *a,
                     const DFBBox *b )
{
     DFB_BOX_ASSERT( a );
     DFB_BOX_ASSERT( b );

     if (a->x1 == b->x1 && a->x2 == b->x2)
          return (a->y1 == b->y2) || (a->y2 == b->y1);

     if (a->y1 == b->y1 && a->y2 == b->y2)
          return (a->x1 == b->x2) || (a->x2 == b->x1);

     return false;
}

static __inline__ void
dfb_box_box_union( DFBBox       *box,
                   const DFBBox *other )
{
     DFB_BOX_ASSERT( box );
     DFB_BOX_ASSERT( other );

     if (DFB_BOX_EMPTY( other ))
          return;

     if (DFB_BOX_EMPTY( box )) {
          *box = *other;
     }
     else {
          if (box->x1 > other->x1)
               box->x1 = other->x1;

          if (box->y1 > other->y1)
               box->y1 = other->y1;

          if (box->x2 < other->x2)
               box->x2 = other->x2;

          if (box->y2 < other->y2)
               box->y2 = other->y2;
     }
}

static __inline__ void
dfb_box_region_union( DFBBox          *box,
                      const DFBRegion *region )
{
     DFB_BOX_ASSERT( box );
     DFB_REGION_ASSERT( region );

     if (DFB_BOX_EMPTY( box )) {
          box->x1 = region->x1;
          box->y1 = region->y1;
          box->x2 = region->x2 + 1;
          box->y2 = region->y2 + 1;
     }
     else {
          if (box->x1 > region->x1)
               box->x1 = region->x1;

          if (box->y1 > region->y1)
               box->y1 = region->y1;

          if (box->x2 < region->x2 + 1)
               box->x2 = region->x2 + 1;

          if (box->y2 < region->y2 + 1)
               box->y2 = region->y2 + 1;
     }
}

static __inline__ void
dfb_box_clip( DFBBox *box,
              int     x1,
              int     y1,
              int     x2,
              int     y2 )
{
     DFB_BOX_ASSERT( box );

     D_ASSERT( dfb_box_intersects( box, x1, y1, x2, y2 ) );

     if (box->x1 < x1)
          box->x1 = x1;

     if (box->y1 < y1)
          box->y1 = y1;

     if (box->x2 > x2)
          box->x2 = x2;

     if (box->y2 > y2)
          box->y2 = y2;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/*
 * Compute line segment intersection.
 * Return true if intersection point exists within the given segment.
 */
static __inline__ bool
dfb_line_segment_intersect( const DFBRegion *line,
                            const DFBRegion *seg,
                            int             *x,
                            int             *y )
{
     int x1, x2, x3, x4;
     int y1, y2, y3, y4;
     int num, den;

     D_ASSERT( line != NULL );
     D_ASSERT( seg != NULL );

     x1 = seg->x1;  y1 = seg->y1;  x2 = seg->y2;  y2 = seg->y2;
     x3 = line->x1; y3 = line->y1; x4 = line->x2; y4 = line->y2;

     num = (x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3);
     den = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);

     if (!den) /* parallel */
          return false;

     if (num && ((num < 0) != (den < 0) || abs(num) > abs(den))) /* not within segment */
          return false;

     if (x)
          *x = (s64)(x2 - x1) * num / den + x1;
     if (y)
          *y = (s64)(y2 - y1) * num / den + y1;

     return true;
}

/**********************************************************************************************************************/

static __inline__ void
dfb_point_from_rotated_region( DFBPoint           *point,
                               const DFBRegion    *from,
                               const DFBDimension *size,
                               int                 rotation )
{
     D_ASSERT( point != NULL );

     DFB_REGION_ASSERT( from );
     D_ASSERT( size != NULL );
     D_ASSERT( size->w > 0 );
     D_ASSERT( size->h > 0 );
     D_ASSUME( rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270 );

     switch (rotation) {
          default:
               D_BUG( "invalid rotation %d", rotation );
          case 0:
               point->x = from->x1;
               point->y = from->y1;
               break;

          case 90:
               point->x = from->y1;
               point->y = size->w - from->x2 - 1;
               break;

          case 180:
               point->x = size->w - from->x2 - 1;
               point->y = size->h - from->y2 - 1;
               break;

          case 270:
               point->x = size->h - from->y2 - 1;
               point->y = from->x1;
               break;
     }

     D_ASSERT( point->x >= 0 );
     D_ASSERT( point->y >= 0 );
     D_ASSERT( point->x < size->w );
     D_ASSERT( point->y < size->h );
}

static __inline__ void
dfb_point_translate( DFBPoint *point,
                     int       dx,
                     int       dy )
{
     D_ASSERT( point != NULL );

     point->x += dx;
     point->y += dy;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/*
 * Copied declaration of DFBPixelFormatName from directfb_strings.h
 */
extern const struct DFBPixelFormatName DIRECTFB_API dfb_pixelformat_names[];
extern const struct DFBColorSpaceName  DIRECTFB_API dfb_colorspace_names[];


const char DIRECTFB_API *dfb_input_event_type_name ( DFBInputEventType     type );
const char DIRECTFB_API *dfb_pixelformat_name      ( DFBSurfacePixelFormat format );
const char DIRECTFB_API *dfb_colorspace_name       ( DFBSurfaceColorSpace  colorspace );
const char DIRECTFB_API *dfb_window_event_type_name( DFBWindowEventType    type );

DFBSurfacePixelFormat DIRECTFB_API dfb_pixelformat_for_depth( int depth );


typedef struct {
     int        magic;

     DFBRegion *regions;
     int        max_regions;
     int        num_regions;

     DFBRegion  bounding;
} DFBUpdates;

#define DFB_UPDATES_ASSERT(updates)                                        \
     do {                                                                  \
          D_MAGIC_ASSERT( updates, DFBUpdates );                           \
          D_ASSERT( (updates)->regions != NULL );                          \
          D_ASSERT( (updates)->max_regions > 0 );                          \
          D_ASSERT( (updates)->num_regions <= (updates)->max_regions );    \
     } while (0)


void DIRECTFB_API dfb_updates_init( DFBUpdates      *updates,
                                    DFBRegion       *regions,
                                    int              max_regions );

void DIRECTFB_API dfb_updates_add ( DFBUpdates      *updates,
                                    const DFBRegion *region );

void DIRECTFB_API dfb_updates_stat( DFBUpdates      *updates,
                                    int             *ret_total,
                                    int             *ret_bounding );

void DIRECTFB_API dfb_updates_get_rectangles( DFBUpdates   *updates,
                                              DFBRectangle *ret_rects,
                                              int          *ret_num );

static __inline__ void
dfb_updates_add_rect( DFBUpdates      *updates,
                      int              x,
                      int              y,
                      int              w,
                      int              h )
{
     DFBRegion region = { x, y, x+w-1, y+h-1 };

     dfb_updates_add( updates, &region );
}

static __inline__ void
dfb_updates_reset( DFBUpdates *updates )
{
     D_MAGIC_ASSERT( updates, DFBUpdates );

     updates->num_regions = 0;
}

static __inline__ void
dfb_updates_deinit( DFBUpdates *updates )
{
     D_MAGIC_ASSERT( updates, DFBUpdates );

     D_MAGIC_CLEAR( updates );
}

#ifdef __cplusplus
}
#endif

#endif
