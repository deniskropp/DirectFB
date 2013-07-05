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


#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <math.h>

#include <directfb.h>
#include <directfb_strings.h>
#include <directfb_water.h>

#include <direct/debug.h>
#include <direct/interface.h>

#include <display/idirectfbsurface.h>

#include "elements.h"
#include "transform.h"
#include "util.h"


static inline void
SetWaterColor( State            *state,
               const WaterColor *color )
{
     DFBColor dfbcolor = { color->a, color->r, color->g, color->b };

     dfb_state_set_color( &state->state, &dfbcolor );
}


D_DEBUG_DOMAIN( IWater_TEST_Elem, "IWater_TEST/Elem", "IWater Interface" );

/**********************************************************************************************************************/

DFBResult
TEST_Render_Point( State                    *state,
                   const WaterElementHeader *header,
                   const WaterScalar        *values,
                   unsigned int              num_values )
{
     int          i, n;
     DFBRectangle rects[num_values/2];

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     for (n=0, i=0; i<num_values; n++, i+=2) {
          rects[n].x = values[i+0].i;
          rects[n].y = values[i+1].i;
          rects[n].w = 1;
          rects[n].h = 1;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d rects\n", n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &rects[i] ), i );
#endif

     TEST_Transform_Rectangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, rects, n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &rects[i] ), i );
#endif

     SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

     dfb_gfxcard_fillrectangles( rects, n, &state->state );

     return DFB_OK;
}

DFBResult
TEST_Render_Span( State                    *state,
                  const WaterElementHeader *header,
                  const WaterScalar        *values,
                  unsigned int              num_values )
{
     int          i, n;
     DFBRectangle rects[num_values/3];

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     for (n=0, i=0; i<num_values; n++, i+=3) {
          rects[n].x = values[i+0].i;
          rects[n].y = values[i+1].i;
          rects[n].w = values[i+2].i;
          rects[n].h = 1;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d rects\n", n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &rects[i] ), i );
#endif

     TEST_Transform_Rectangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, rects, n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &rects[i] ), i );
#endif

     SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

     dfb_gfxcard_fillrectangles( rects, n, &state->state );

     return DFB_OK;
}

DFBResult
TEST_Render_Line( State                    *state,
                  const WaterElementHeader *header,
                  const WaterScalar        *values,
                  unsigned int              num_values )
{
     int       i, n;
     DFBRegion lines[num_values/4];

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     for (n=0, i=0; i<num_values; n++, i+=4) {
          lines[n].x1 = values[i+0].i;
          lines[n].y1 = values[i+1].i;
          lines[n].x2 = values[i+2].i;
          lines[n].y2 = values[i+3].i;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d lines\n", n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d [%d]\n", DFB_REGION_VALS( &lines[i] ), i );
#endif

     TEST_Transform_Regions( &state->attributes[WAT_RENDER_TRANSFORM].transform, lines, n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d [%d]\n", DFB_REGION_VALS( &lines[i] ), i );
#endif

     SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

     dfb_gfxcard_drawlines( &lines[0], n, &state->state );

     return DFB_OK;
}

DFBResult
TEST_Render_LineStripLoop( State                    *state,
                           const WaterElementHeader *header,
                           const WaterScalar        *values,
                           unsigned int              num_values )
{
     int       i, n;
     DFBRegion lines[num_values/2];

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u], %s )\n", __FUNCTION__, values, num_values,
                 header->type == WET_LINE_LOOP ? "loop" : "strip" );

     lines[0].x1 = values[0].i;
     lines[0].y1 = values[1].i;

     for (n=1, i=2; i<num_values-2; n++, i+=2) {
          lines[n-1].x2 = lines[n].x1 = values[i+0].i;
          lines[n-1].y2 = lines[n].y1 = values[i+1].i;
     }

     lines[n-1].x2 = values[i+0].i;
     lines[n-1].y2 = values[i+1].i;

     if (header->type == WET_LINE_LOOP) {
          lines[n].x1 = lines[n-1].x2;
          lines[n].y1 = lines[n-1].y2;

          lines[n].x2 = lines[0].x1;
          lines[n].y2 = lines[0].y1;

          n++;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d lines\n", n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d [%d]\n", DFB_REGION_VALS( &lines[i] ), i );
#endif

     TEST_Transform_Regions( &state->attributes[WAT_RENDER_TRANSFORM].transform, lines, n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d [%d]\n", DFB_REGION_VALS( &lines[i] ), i );
#endif

     SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

     dfb_gfxcard_drawlines( &lines[0], n, &state->state );

     return DFB_OK;
}

/**********************************************************************************************************************/

static int
build_rectangle_outlines( const DFBRectangle *rect,
                          DFBRectangle       *ret_rects )
{
     int num = 1;

     DFB_RECTANGLE_ASSERT( rect );

     D_ASSERT( ret_rects != NULL );

     ret_rects[0].x = rect->x;
     ret_rects[0].y = rect->y;
     ret_rects[0].w = rect->w;
     ret_rects[0].h = 1;

     if (rect->h > 1) {
          num++;

          ret_rects[1].x = rect->x;
          ret_rects[1].y = rect->y + rect->h - 1;
          ret_rects[1].w = rect->w;
          ret_rects[1].h = 1;

          if (rect->h > 2) {
               num++;

               ret_rects[2].x = rect->x;
               ret_rects[2].y = rect->y + 1;
               ret_rects[2].w = 1;
               ret_rects[2].h = rect->h - 2;

               if (rect->w > 1) {
                    num++;

                    ret_rects[3].x = rect->x + rect->w - 1;
                    ret_rects[3].y = rect->y + 1;
                    ret_rects[3].w = 1;
                    ret_rects[3].h = rect->h - 2;
               }
          }
     }

     return num;
}

DFBResult
TEST_Render_Rectangle( State                    *state,
                       const WaterElementHeader *header,
                       const WaterScalar        *values,
                       unsigned int              num_values )
{
     int               i, l, n;
     DFBRectangle      rects[num_values/4];
     WaterElementFlags flags = header->flags;
     DFBTriangle       tri[8];

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     if (TEST_NONRECT_TRANSFORM( &state->attributes[WAT_RENDER_TRANSFORM].transform )) {
          int ins = 0;

          if (flags & WEF_DRAW)
               ins = 1;

          for (i=0; i<num_values; i+=4) {
               if (flags & WEF_FILL) {
#ifdef TEST_USE_TRIANGLES
                    D_DEBUG_AT( IWater_TEST_Elem, "  -> FILL [%d]\n", i );

                    /* First */
                    tri[0].x1 = values[i+0].i + ins;
                    tri[0].y1 = values[i+1].i + ins;

                    tri[0].x2 = values[i+0].i + values[i+2].i - ins;
                    tri[0].y2 = values[i+1].i + ins;

                    tri[0].x3 = values[i+0].i + values[i+2].i - ins;
                    tri[0].y3 = values[i+1].i + values[i+3].i - ins;

                    /* Second */
                    tri[1].x1 = values[i+0].i + ins;
                    tri[1].y1 = values[i+1].i + ins;

                    tri[1].x2 = values[i+0].i + values[i+2].i - ins;
                    tri[1].y2 = values[i+1].i + values[i+3].i - ins;

                    tri[1].x3 = values[i+0].i + ins;
                    tri[1].y3 = values[i+1].i + values[i+3].i - ins;

                    D_DEBUG_AT( IWater_TEST_Elem, "  -> %d triangles\n", 2 );

#if D_DEBUG_ENABLED
                    for (l=0; l<2; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d - %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &tri[l] ), l );
#endif

                    TEST_Transform_Triangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, &tri[0], 2 );

#if D_DEBUG_ENABLED
                    for (l=0; l<2; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d - %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &tri[l] ), l );
#endif

                    SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );

                    dfb_gfxcard_filltriangles( tri, 2, &state->state );
#else
                    DFBPoint points[4];

                    D_DEBUG_AT( IWater_TEST_Elem, "  -> FILL [%d]\n", i );

                    /* First */
                    points[0].x = values[i+0].i + ins;
                    points[0].y = values[i+1].i + ins;

                    points[1].x = values[i+0].i + values[i+2].i - ins;
                    points[1].y = points[0].y;

                    points[2].x = points[1].x;
                    points[2].y = values[i+1].i + values[i+3].i - ins;

                    points[3].x = points[0].x;
                    points[3].y = points[2].y;

#if D_DEBUG_ENABLED
                    for (l=0; l<4; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d [%d]\n", DFB_POINT_VALS( &points[l] ), l );
#endif

                    TEST_Transform_Points( &state->attributes[WAT_RENDER_TRANSFORM].transform, &points[0], 4 );

#if D_DEBUG_ENABLED
                    for (l=0; l<4; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d [%d]\n", DFB_POINT_VALS( &points[l] ), l );
#endif

                    SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );

                    dfb_gfxcard_fillquadrangles( points, 1, &state->state );
#endif
               }
               if (flags & WEF_DRAW) {
                    int          num;
                    DFBRectangle rect;
                    DFBRectangle outlines[4];

                    D_DEBUG_AT( IWater_TEST_Elem, "  -> DRAW [%d]\n", i );

                    rect.x = values[i+0].i;
                    rect.y = values[i+1].i;
                    rect.w = values[i+2].i;
                    rect.h = values[i+3].i;

                    num = build_rectangle_outlines( &rect, &outlines[0] );

#if D_DEBUG_ENABLED
                    D_DEBUG_AT( IWater_TEST_Elem, "  -> %d outlines\n", num );
                    for (l=0; l<num; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &outlines[l] ), l );
#endif

                    for (l=0, n=0; l<num; l++, n+=2) {
                         /* First */
                         tri[n+0].x1 = outlines[l].x;
                         tri[n+0].y1 = outlines[l].y;

                         tri[n+0].x2 = outlines[l].x + outlines[l].w;
                         tri[n+0].y2 = outlines[l].y;

                         tri[n+0].x3 = outlines[l].x + outlines[l].w;
                         tri[n+0].y3 = outlines[l].y + outlines[l].h;

                         /* Second */
                         tri[n+1].x1 = outlines[l].x;
                         tri[n+1].y1 = outlines[l].y;

                         tri[n+1].x2 = outlines[l].x + outlines[l].w;
                         tri[n+1].y2 = outlines[l].y + outlines[l].h;

                         tri[n+1].x3 = outlines[l].x;
                         tri[n+1].y3 = outlines[l].y + outlines[l].h;
                    }

                    D_DEBUG_AT( IWater_TEST_Elem, "  -> %d triangles\n", n );

#if D_DEBUG_ENABLED
                    for (l=0; l<n; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d - %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &tri[l] ), l );
#endif

                    TEST_Transform_Triangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, &tri[0], n );

#if D_DEBUG_ENABLED
                    for (l=0; l<n; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d - %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &tri[l] ), l );
#endif

                    SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

                    dfb_gfxcard_filltriangles( tri, n, &state->state );
               }
          }
          return DFB_OK;
     }

     for (n=0, i=0; i<num_values; n++, i+=4) {
          rects[n].x = values[i+0].i;
          rects[n].y = values[i+1].i;
          rects[n].w = values[i+2].i;
          rects[n].h = values[i+3].i;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d rects\n", n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &rects[i] ), i );
#endif

     if (flags & WEF_FILL) {
          if (flags & WEF_DRAW) {
               D_DEBUG_AT( IWater_TEST_Elem, "  -> DRAW + FILL\n" );

               if (TEST_ANY_TRANSFORM( &state->attributes[WAT_RENDER_TRANSFORM].transform )) {
                    for (i=0; i<n; i++) {
                         int          num;
                         DFBRectangle outlines[4];

                         num = build_rectangle_outlines( &rects[i], &outlines[0] );

                         rects[i].x += 1;
                         rects[i].y += 1;
                         rects[i].w -= 2;
                         rects[i].h -= 2;

                         TEST_Transform_Rectangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, &rects[i], 1 );

                         D_DEBUG_AT( IWater_TEST_Elem, "  -> inner %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &rects[i] ), i );

                         SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );

                         dfb_gfxcard_fillrectangles( &rects[i], 1, &state->state );


                         TEST_Transform_Rectangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, outlines, num );
#if D_DEBUG_ENABLED
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %d transformed outlines\n", num );
                         for (l=0; l<num; l++)
                              D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &outlines[l] ), l );
#endif

                         SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

                         dfb_gfxcard_fillrectangles( &outlines[0], num, &state->state );
                    }
               }
               else {
                    for (i=0; i<n; i++) {
                         SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

                         dfb_gfxcard_drawrectangle( &rects[i], &state->state );

                         rects[i].x += 1;
                         rects[i].y += 1;
                         rects[i].w -= 2;
                         rects[i].h -= 2;

                         SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );

                         dfb_gfxcard_fillrectangles( &rects[i], 1, &state->state );
                    }
               }
          }
          else {
               D_DEBUG_AT( IWater_TEST_Elem, "  -> FILL only\n" );

               SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );

               if (TEST_ANY_TRANSFORM( &state->attributes[WAT_RENDER_TRANSFORM].transform )) {
                    TEST_Transform_Rectangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, rects, n );

#if D_DEBUG_ENABLED
                    D_DEBUG_AT( IWater_TEST_Elem, "  -> %d transformed rects\n", n );
                    for (i=0; i<n; i++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &rects[i] ), i );
#endif
               }

               dfb_gfxcard_fillrectangles( &rects[0], n, &state->state );
          }
     }
     else if (flags & WEF_DRAW) {
          D_DEBUG_AT( IWater_TEST_Elem, "  -> DRAW only\n" );

          SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

          if (TEST_ANY_TRANSFORM( &state->attributes[WAT_RENDER_TRANSFORM].transform )) {
               for (i=0; i<n; i++) {
                    int          num;
                    DFBRectangle outlines[4];

                    num = build_rectangle_outlines( &rects[i], &outlines[0] );

                    TEST_Transform_Rectangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, outlines, num );
#if D_DEBUG_ENABLED
                    D_DEBUG_AT( IWater_TEST_Elem, "  -> %d transformed outlines\n", num );
                    for (l=0; l<num; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &outlines[l] ), l );
#endif

                    dfb_gfxcard_fillrectangles( &outlines[0], num, &state->state );
               }
          }
          else
               for (i=0; i<n; i++)
                    dfb_gfxcard_drawrectangle( &rects[i], &state->state );
     }
     else
          D_DEBUG_AT( IWater_TEST_Elem, "  -> NEITHER DRAW NOR FILL?!!\n" );

     return DFB_OK;
}

DFBResult
TEST_Render_Rectangle_To_FillQuad( State                    *state,
                                   const WaterElementHeader *header,
                                   const WaterScalar        *values,
                                   unsigned int              num_values,
                                   WaterElementHeader       *ret_element,
                                   WaterScalar              *ret_values,
                                   unsigned int             *ret_num_values )
{
     int               i, l, n;
     DFBRectangle      rects[num_values/4];
     WaterElementFlags flags = header->flags;
     DFBTriangle       tri[8];

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

//     if (TEST_NONRECT_TRANSFORM( &state->attributes[WAT_RENDER_TRANSFORM].transform )) {
          int ins = 0;

          if (flags & WEF_DRAW)
               ins = 1;

          for (i=0; i<num_values; i+=4) {
               if (flags & WEF_FILL) {
                    DFBPoint points[4];

                    D_DEBUG_AT( IWater_TEST_Elem, "  -> FILL [%d]\n", i );

                    /* First */
                    points[0].x = values[i+0].i + ins;
                    points[0].y = values[i+1].i + ins;

                    points[1].x = values[i+0].i + values[i+2].i - ins;
                    points[1].y = points[0].y;

                    points[2].x = points[1].x;
                    points[2].y = values[i+1].i + values[i+3].i - ins;

                    points[3].x = points[0].x;
                    points[3].y = points[2].y;

#if D_DEBUG_ENABLED
                    for (l=0; l<4; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d [%d]\n", DFB_POINT_VALS( &points[l] ), l );
#endif

                    TEST_Transform_Points( &state->attributes[WAT_RENDER_TRANSFORM].transform, &points[0], 4 );

#if D_DEBUG_ENABLED
                    for (l=0; l<4; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d [%d]\n", DFB_POINT_VALS( &points[l] ), l );
#endif


                    ret_values[i+0].i = points[0].x;
                    ret_values[i+1].i = points[0].y;
                    ret_values[i+2].i = points[1].x;
                    ret_values[i+3].i = points[1].y;
                    ret_values[i+4].i = points[2].x;
                    ret_values[i+5].i = points[2].y;
                    ret_values[i+6].i = points[3].x;
                    ret_values[i+7].i = points[3].y;


                    ret_element->type  = WET_QUADRANGLE;
                    ret_element->flags = WEF_FILL;

                    *ret_num_values = 8;
               }
               return DFB_OK;

               if (flags & WEF_DRAW) {
                    int          num;
                    DFBRectangle rect;
                    DFBRectangle outlines[4];

                    D_DEBUG_AT( IWater_TEST_Elem, "  -> DRAW [%d]\n", i );

                    rect.x = values[i+0].i;
                    rect.y = values[i+1].i;
                    rect.w = values[i+2].i;
                    rect.h = values[i+3].i;

                    num = build_rectangle_outlines( &rect, &outlines[0] );

#if D_DEBUG_ENABLED
                    D_DEBUG_AT( IWater_TEST_Elem, "  -> %d outlines\n", num );
                    for (l=0; l<num; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &outlines[l] ), l );
#endif

                    for (l=0, n=0; l<num; l++, n+=2) {
                         /* First */
                         tri[n+0].x1 = outlines[l].x;
                         tri[n+0].y1 = outlines[l].y;

                         tri[n+0].x2 = outlines[l].x + outlines[l].w;
                         tri[n+0].y2 = outlines[l].y;

                         tri[n+0].x3 = outlines[l].x + outlines[l].w;
                         tri[n+0].y3 = outlines[l].y + outlines[l].h;

                         /* Second */
                         tri[n+1].x1 = outlines[l].x;
                         tri[n+1].y1 = outlines[l].y;

                         tri[n+1].x2 = outlines[l].x + outlines[l].w;
                         tri[n+1].y2 = outlines[l].y + outlines[l].h;

                         tri[n+1].x3 = outlines[l].x;
                         tri[n+1].y3 = outlines[l].y + outlines[l].h;
                    }

                    D_DEBUG_AT( IWater_TEST_Elem, "  -> %d triangles\n", n );

#if D_DEBUG_ENABLED
                    for (l=0; l<n; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d - %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &tri[l] ), l );
#endif

                    TEST_Transform_Triangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, &tri[0], n );

#if D_DEBUG_ENABLED
                    for (l=0; l<n; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d - %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &tri[l] ), l );
#endif

                    SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

                    dfb_gfxcard_filltriangles( tri, n, &state->state );
               }
          }
          return DFB_OK;
//     }

     for (n=0, i=0; i<num_values; n++, i+=4) {
          rects[n].x = values[i+0].i;
          rects[n].y = values[i+1].i;
          rects[n].w = values[i+2].i;
          rects[n].h = values[i+3].i;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d rects\n", n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &rects[i] ), i );
#endif

     if (flags & WEF_FILL) {
          if (flags & WEF_DRAW) {
               D_DEBUG_AT( IWater_TEST_Elem, "  -> DRAW + FILL\n" );

               if (TEST_ANY_TRANSFORM( &state->attributes[WAT_RENDER_TRANSFORM].transform )) {
                    for (i=0; i<n; i++) {
                         int          num;
                         DFBRectangle outlines[4];

                         num = build_rectangle_outlines( &rects[i], &outlines[0] );

                         rects[i].x += 1;
                         rects[i].y += 1;
                         rects[i].w -= 2;
                         rects[i].h -= 2;

                         TEST_Transform_Rectangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, &rects[i], 1 );

                         D_DEBUG_AT( IWater_TEST_Elem, "  -> inner %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &rects[i] ), i );

                         SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );
                         dfb_gfxcard_fillrectangles( &rects[i], 1, &state->state );


                         TEST_Transform_Rectangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, outlines, num );
#if D_DEBUG_ENABLED
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %d transformed outlines\n", num );
                         for (l=0; l<num; l++)
                              D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &outlines[l] ), l );
#endif

                         SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );
                         dfb_gfxcard_fillrectangles( &outlines[0], num, &state->state );
                    }
               }
               else {
                    for (i=0; i<n; i++) {
                         dfb_gfxcard_drawrectangle( &rects[i], &state->state );

                         rects[i].x += 1;
                         rects[i].y += 1;
                         rects[i].w -= 2;
                         rects[i].h -= 2;

                         dfb_gfxcard_fillrectangles( &rects[i], 1, &state->state );
                    }
               }
          }
          else {
               D_DEBUG_AT( IWater_TEST_Elem, "  -> FILL only\n" );

               if (TEST_ANY_TRANSFORM( &state->attributes[WAT_RENDER_TRANSFORM].transform )) {
                    TEST_Transform_Rectangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, rects, n );

#if D_DEBUG_ENABLED
                    D_DEBUG_AT( IWater_TEST_Elem, "  -> %d transformed rects\n", n );
                    for (i=0; i<n; i++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &rects[i] ), i );
#endif
               }

               SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );
               dfb_gfxcard_fillrectangles( &rects[0], n, &state->state );
          }
     }
     else if (flags & WEF_DRAW) {
          D_DEBUG_AT( IWater_TEST_Elem, "  -> DRAW only\n" );

          SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

          if (TEST_ANY_TRANSFORM( &state->attributes[WAT_RENDER_TRANSFORM].transform )) {
               for (i=0; i<n; i++) {
                    int          num;
                    DFBRectangle outlines[4];

                    num = build_rectangle_outlines( &rects[i], &outlines[0] );

                    TEST_Transform_Rectangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, outlines, num );
#if D_DEBUG_ENABLED
                    D_DEBUG_AT( IWater_TEST_Elem, "  -> %d transformed outlines\n", num );
                    for (l=0; l<num; l++)
                         D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4dx%4d [%d]\n", DFB_RECTANGLE_VALS( &outlines[l] ), l );
#endif

                    dfb_gfxcard_fillrectangles( &outlines[0], num, &state->state );
               }
          }
          else
               for (i=0; i<n; i++)
                    dfb_gfxcard_drawrectangle( &rects[i], &state->state );
     }
     else
          D_DEBUG_AT( IWater_TEST_Elem, "  -> NEITHER DRAW NOR FILL?!!\n" );

     return DFB_OK;
}

DFBResult
TEST_Render_Triangle( State                    *state,
                      const WaterElementHeader *header,
                      const WaterScalar        *values,
                      unsigned int              num_values )
{
     int               i, n;
     DFBTriangle       triangles[num_values/2 - 2];
     WaterElementFlags flags = header->flags;

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     switch (WATER_ELEMENT_TYPE_INDEX(header->type)) {
          case WATER_ELEMENT_TYPE_INDEX( WET_TRIANGLE ):
               D_DEBUG_AT( IWater_TEST_Elem, "  -> TRIANGLE\n" );

               for (n=0, i=0; i<num_values; n++, i+=6) {
                    triangles[n].x1 = values[i+0].i;
                    triangles[n].y1 = values[i+1].i;
                    triangles[n].x2 = values[i+2].i;
                    triangles[n].y2 = values[i+3].i;
                    triangles[n].x3 = values[i+4].i;
                    triangles[n].y3 = values[i+5].i;
               }
               break;

          case WATER_ELEMENT_TYPE_INDEX( WET_TRIANGLE_FAN ):
               D_DEBUG_AT( IWater_TEST_Elem, "  -> TRIANGLE_FAN\n" );

               triangles[0].x1 = values[0].i;
               triangles[0].y1 = values[1].i;
               triangles[0].x2 = values[2].i;
               triangles[0].y2 = values[3].i;
               triangles[0].x3 = values[4].i;
               triangles[0].y3 = values[5].i;

               for (n=1, i=6; i<num_values; n++, i+=2) {
                    triangles[n].x1 = triangles[0].x1;
                    triangles[n].y1 = triangles[0].y1;
                    triangles[n].x2 = triangles[n-1].x3;
                    triangles[n].y2 = triangles[n-1].y3;
                    triangles[n].x3 = values[i+0].i;
                    triangles[n].y3 = values[i+1].i;
               }
               break;

          case WATER_ELEMENT_TYPE_INDEX( WET_TRIANGLE_STRIP ):
               D_DEBUG_AT( IWater_TEST_Elem, "  -> TRIANGLE_STRIP\n" );

               triangles[0].x1 = values[0].i;
               triangles[0].y1 = values[1].i;
               triangles[0].x2 = values[2].i;
               triangles[0].y2 = values[3].i;
               triangles[0].x3 = values[4].i;
               triangles[0].y3 = values[5].i;

               for (n=1, i=6; i<num_values; n++, i+=2) {
                    triangles[n].x1 = triangles[n-1].x2;    /* FIXME: correct? */
                    triangles[n].y1 = triangles[n-1].y2;
                    triangles[n].x2 = triangles[n-1].x3;
                    triangles[n].y2 = triangles[n-1].y3;
                    triangles[n].x3 = values[i+0].i;
                    triangles[n].y3 = values[i+1].i;
               }
               break;

          default:
               D_BUG( "unexpected element type" );
               return DFB_BUG;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d triangles\n", n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d - %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &triangles[i] ), i );
#endif

     if (TEST_ANY_TRANSFORM( &state->attributes[WAT_RENDER_TRANSFORM].transform ))
          TEST_Transform_Triangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, triangles, n );

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d transformed triangles\n", n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d - %4d,%4d - %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &triangles[i] ), i );
#endif

     if (flags & WEF_FILL) {
          if (flags & WEF_DRAW) {
               D_DEBUG_AT( IWater_TEST_Elem, "  -> DRAW + FILL\n" );

               SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );
               SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );

               dfb_gfxcard_filltriangles( triangles, n, &state->state );
//               dfb_gfxcard_drawtriangles( triangles, n, &state->state );
          }
          else {
               D_DEBUG_AT( IWater_TEST_Elem, "  -> FILL only\n" );

               SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );

               dfb_gfxcard_filltriangles( triangles, n, &state->state );
          }
     }
     else if (flags & WEF_DRAW) {
          D_DEBUG_AT( IWater_TEST_Elem, "  -> DRAW only\n" );

          SetWaterColor( state, &state->attributes[WAT_DRAW_COLOR].color );

//               dfb_gfxcard_drawtriangles( triangles, n, &state->state );
     }
     else
          D_DEBUG_AT( IWater_TEST_Elem, "  -> NEITHER DRAW NOR FILL?!!\n" );

     return DFB_OK;
}

DFBResult
TEST_Render_Trapezoid( State                    *state,
                       const WaterElementHeader *header,
                       const WaterScalar        *values,
                       unsigned int              num_values )
{
     int         i, n;
     DFBTriangle tris[num_values/3];

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     for (n=0, i=0; i<num_values; n+=2, i+=6) {
          tris[n+0].x1 = values[i+0].i;
          tris[n+0].y1 = values[i+1].i;
          tris[n+0].x2 = values[i+0].i + values[i+2].i;
          tris[n+0].y2 = values[i+1].i;
          tris[n+0].x3 = values[i+3].i + values[i+5].i;
          tris[n+0].y3 = values[i+4].i;

          tris[n+1].x1 = values[i+0].i;
          tris[n+1].y1 = values[i+1].i;
          tris[n+1].x2 = values[i+3].i + values[i+5].i;
          tris[n+1].y2 = values[i+4].i;
          tris[n+1].x3 = values[i+3].i;
          tris[n+1].y3 = values[i+4].i;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d tris\n", n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d, %4d,%4d, %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &tris[i] ), i );
#endif

     TEST_Transform_Triangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, tris, n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d, %4d,%4d, %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &tris[i] ), i );
#endif

     SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );

     dfb_gfxcard_filltriangles( tris, n, &state->state );

     return DFB_OK;
}

DFBResult
TEST_Render_Quadrangle( State                    *state,
                        const WaterElementHeader *header,
                        const WaterScalar        *values,
                        unsigned int              num_values )
{
     int         i, n;
#if 0
     DFBTriangle tris[num_values/8 * 2];

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     for (n=0, i=0; i<num_values; n+=2, i+=8) {
          tris[n+0].x1 = values[i+0].i;
          tris[n+0].y1 = values[i+1].i;
          tris[n+0].x2 = values[i+2].i;
          tris[n+0].y2 = values[i+3].i;
          tris[n+0].x3 = values[i+4].i;
          tris[n+0].y3 = values[i+5].i;

          tris[n+1].x1 = values[i+0].i;
          tris[n+1].y1 = values[i+1].i;
          tris[n+1].x2 = values[i+4].i;
          tris[n+1].y2 = values[i+5].i;
          tris[n+1].x3 = values[i+6].i;
          tris[n+1].y3 = values[i+7].i;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d tris\n", n );

     if (!n)
          return DFB_OK;

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d, %4d,%4d, %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &tris[i] ), i );
#endif

     TEST_Transform_Triangles( &state->attributes[WAT_RENDER_TRANSFORM].transform, tris, n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d, %4d,%4d, %4d,%4d [%d]\n", DFB_TRIANGLE_VALS( &tris[i] ), i );
#endif

     SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );

     dfb_gfxcard_filltriangles( tris, n, &state->state );
#else
     DFBPoint points[num_values/2];

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     for (n=0, i=0; i<num_values; n++, i+=2) {
          points[n].x = values[i+0].i;
          points[n].y = values[i+1].i;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d points\n", n );

     if (!n)
          return DFB_OK;

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d [%d]\n", DFB_POINT_VALS( &points[i] ), i );
#endif

     TEST_Transform_Points( &state->attributes[WAT_RENDER_TRANSFORM].transform, points, n );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i++)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d [%d]\n", DFB_POINT_VALS( &points[i] ), i );
#endif

     SetWaterColor( state, &state->attributes[WAT_FILL_COLOR].color );

     dfb_gfxcard_fillquadrangles( points, n / 4, &state->state );
#endif
     return DFB_OK;
}

DFBResult
TEST_Render_Trapezoid_To_Quadrangle( State                    *state,
                                     const WaterElementHeader *header,
                                     const WaterScalar        *values,
                                     unsigned int              num_values,
                                     WaterElementHeader       *ret_header,
                                     WaterScalar              *ret_values,
                                     unsigned int             *ret_num_values )
{
     int i, n;

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     ret_header->type  = WET_QUADRANGLE;
     ret_header->flags = header->flags;

     *ret_num_values   = num_values / 6 * 8;

     for (n=0, i=0; i<num_values; n+=8, i+=6) {
          DFBPoint p[4];

          p[0].x = values[i+0].i;
          p[0].y = values[i+1].i;

          p[1].x = values[i+0].i + values[i+2].i;
          p[1].y = values[i+1].i;

          p[2].x = values[i+3].i + values[i+5].i;
          p[2].y = values[i+4].i;

          p[3].x = values[i+3].i;
          p[3].y = values[i+4].i;

          TEST_Transform_Points( &state->attributes[WAT_RENDER_TRANSFORM].transform, p, 4 );

          ret_values[n+0].i = p[0].x;
          ret_values[n+1].i = p[0].y;
                                           
          ret_values[n+2].i = p[1].x;
          ret_values[n+3].i = p[1].y;
                                           
          ret_values[n+4].i = p[2].x;
          ret_values[n+5].i = p[2].y;
                                           
          ret_values[n+6].i = p[3].x;
          ret_values[n+7].i = p[3].y;
     }

     D_DEBUG_AT( IWater_TEST_Elem, "  -> %d quads\n", n/8 );

#if D_DEBUG_ENABLED
     for (i=0; i<n; i+=8)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d, %4d,%4d, %4d,%4d, %4d,%4d [%d]\n",
                      ret_values[i+0].i,
                      ret_values[i+1].i,
                      ret_values[i+2].i,
                      ret_values[i+3].i,
                      ret_values[i+4].i,
                      ret_values[i+5].i,
                      ret_values[i+6].i,
                      ret_values[i+7].i, i/8 );
#endif

     return DFB_OK;
}

DFBResult
TEST_Render_Polygon( State                    *state,
                     const WaterElementHeader *header,
                     const WaterScalar        *values,
                     unsigned int              num_values )
{
     int i;
     WaterElementHeader tri_element;

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     tri_element.type   = WET_TRIANGLE_FAN;
     tri_element.flags  = header->flags;
     tri_element.scalar = header->scalar;

     D_UNIMPLEMENTED();

     for (i=0; i<num_values; i+=2)
          D_DEBUG_AT( IWater_TEST_Elem, "  -> %4d,%4d [%d]\n",
                      values[i+0].i,
                      values[i+1].i, i/2 );

     return TEST_Render_Triangle( state, &tri_element, values, num_values );
}

DFBResult
TEST_Render_Circle( State                    *state,
                    const WaterElementHeader *header,
                    const WaterScalar        *values,
                    unsigned int              num_values )
{
     int                i, n;
     WaterScalar        vals[num_values * 4 / 3];
     WaterElementHeader temp;

     D_DEBUG_AT( IWater_TEST_Elem, "%s( %p [%u] )\n", __FUNCTION__, values, num_values );

     D_UNIMPLEMENTED();

     /*
      * First transformation of a representation: circles to rectangles! (FIXME)
      */
     for (n=0, i=0; i<num_values; n+=4, i+=3) {
          vals[n+0].i = values[i+0].i - values[i+2].i;
          vals[n+1].i = values[i+1].i - values[i+2].i;
          vals[n+2].i = values[i+2].i + values[i+2].i;
          vals[n+3].i = values[i+2].i + values[i+2].i;
     }

     /*
      * Generate transformed temporary element
      */
     temp.type   = WET_RECTANGLE;
     temp.flags  = header->flags;
     temp.scalar = header->scalar;

     /*
      * Render temporary element
      */
     return TEST_Render_Rectangle( state, &temp, vals, num_values * 4 / 3 );
}

