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
#include <direct/memcpy.h>

#include <display/idirectfbsurface.h>

#include <gfx/clip.h>

#include "transform.h"
#include "util.h"


D_DEBUG_DOMAIN( IWater_TEST_Trans, "IWater_TEST/Trans", "IWater Interface TEST Transform" );

/**********************************************************************************************************************/

void
TEST_Transform_TypeToMatrix_16_16( WaterTransform *transform )
{
     int             i, s, c;
     WaterScalar     matrix[6];
     WaterScalarType scalar = WST_INTEGER;

     D_DEBUG_AT( IWater_TEST_Trans, "%s( %p )\n", __FUNCTION__, transform );

     scalar = transform->scalar;

     if (! (transform->flags & WTF_TYPE)) {
          if (! (transform->flags & WTF_MATRIX)) {
               memset( transform->matrix, 0, sizeof(transform->matrix) );

               transform->flags |= WTF_MATRIX;
          }

          if (scalar != WST_FIXED_16_16)
               D_UNIMPLEMENTED();

          return;
     }

     memset( matrix, 0, sizeof(matrix) );

     if (transform->type != WTT_ZERO) {
          matrix[0].i = 0x10000;
          matrix[4].i = 0x10000;
     }

     switch (transform->type) {
          case WTT_UNKNOWN:
          case WTT_ZERO:
               break;

          default:
               D_UNIMPLEMENTED();

          case WTT_IDENTITY:
               break;

          case WTT_TRANSLATE_X:
               matrix[2].i = TEST_SCALAR_TO_16_16( transform->matrix[0], scalar );
               break;

          case WTT_TRANSLATE_Y:
               matrix[5].i = TEST_SCALAR_TO_16_16( transform->matrix[0], scalar );
               break;

          case WTT_TRANSLATE_X | WTT_TRANSLATE_Y:
               matrix[2].i = TEST_SCALAR_TO_16_16( transform->matrix[0], scalar );
               matrix[5].i = TEST_SCALAR_TO_16_16( transform->matrix[1], scalar );
               break;

          case WTT_SCALE_X:
               matrix[0].i = TEST_SCALAR_TO_16_16( transform->matrix[0], scalar );
               break;

          case WTT_SCALE_Y:
               matrix[4].i = TEST_SCALAR_TO_16_16( transform->matrix[0], scalar );
               break;

          case WTT_SCALE_X | WTT_SCALE_Y:
               matrix[0].i = TEST_SCALAR_TO_16_16( transform->matrix[0], scalar );
               matrix[4].i = TEST_SCALAR_TO_16_16( transform->matrix[1], scalar );
               break;

          case WTT_ROTATE_FREE:
               /* FIXME: Generate sin/cos tables for fixed point! */
               c = (int)(cosf( TEST_SCALAR_TO_FLOAT( transform->matrix[0], scalar ) ) * 0x10000);
               s = (int)(sinf( TEST_SCALAR_TO_FLOAT( transform->matrix[0], scalar ) ) * 0x10000);

               matrix[0].i =  c;
               matrix[1].i = -s;
               matrix[3].i =  s;
               matrix[4].i =  c;
               break;
     }

     direct_memcpy( transform->matrix, matrix, sizeof(transform->matrix) );

     transform->flags |= WTF_MATRIX;
     transform->scalar = WST_FIXED_16_16;

     /* FIXME: Keep type, but clear it when matrix is modified. */
     transform->type   = WTF_NONE;
     transform->flags &= ~WTF_TYPE;

     for (i=0; i<6; i++)
          D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] %c%4d.%05u\n", i, TEST_FIXED_16_16_VALUES_05( transform->matrix[i].i ) );
}

void
TEST_Transform_Append_16_16( WaterTransform *transform, const WaterTransform *other )
{
     int                i;
     WaterScalar        matrix[6];
     const WaterScalar *a = &other->matrix[0];
     const WaterScalar *b = &transform->matrix[0];

     D_DEBUG_AT( IWater_TEST_Trans, "%s( %p, %p )\n", __FUNCTION__, transform, other );

     if (transform->scalar != WST_FIXED_16_16)
          D_UNIMPLEMENTED();

     if (other->scalar != WST_FIXED_16_16)
          D_UNIMPLEMENTED();

     for (i=0; i<6; i++)
          D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] %c%4d.%05u\n", i, TEST_FIXED_16_16_VALUES_05( transform->matrix[i].i ) );

     for (i=0; i<6; i++)
          D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] %c%4d.%05u\n", i, TEST_FIXED_16_16_VALUES_05( other->matrix[i].i ) );

     matrix[0].i = TEST_MULT2_ADD_16_16( a[0].i, b[0].i, a[3].i, b[1].i );
     matrix[1].i = TEST_MULT2_ADD_16_16( a[1].i, b[0].i, a[4].i, b[1].i );
     matrix[2].i = TEST_MULT2_ADD_16_16( a[2].i, b[0].i, a[5].i, b[1].i ) + b[2].i;

     matrix[3].i = TEST_MULT2_ADD_16_16( a[0].i, b[3].i, a[3].i, b[4].i );
     matrix[4].i = TEST_MULT2_ADD_16_16( a[1].i, b[3].i, a[4].i, b[4].i );
     matrix[5].i = TEST_MULT2_ADD_16_16( a[2].i, b[3].i, a[5].i, b[4].i ) + b[5].i;

     direct_memcpy( &transform->matrix[0], matrix, sizeof(matrix) );

     for (i=0; i<6; i++)
          D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] %c%4d.%05u\n", i, TEST_FIXED_16_16_VALUES_05( transform->matrix[i].i ) );
}

void
TEST_Transform_TypeToMatrix( WaterTransform *transform )
{
     int             i;
     float           s, c;
     WaterScalar     matrix[6];
     WaterScalarType scalar = WST_INTEGER;

     D_DEBUG_AT( IWater_TEST_Trans, "%s( %p )\n", __FUNCTION__, transform );

     scalar = transform->scalar;

     if (! (transform->flags & WTF_TYPE)) {
          if (! (transform->flags & WTF_MATRIX)) {
               memset( transform->matrix, 0, sizeof(transform->matrix) );

               transform->flags |= WTF_MATRIX;
          }

          if (scalar != WST_FLOAT)
               D_UNIMPLEMENTED();

          return;
     }

     memset( matrix, 0, sizeof(matrix) );

     if (transform->type != WTT_ZERO) {
          matrix[0].f = 1.0f;
          matrix[4].f = 1.0f;
     }

     switch (transform->type) {
          case WTT_UNKNOWN:
          case WTT_ZERO:
               break;

          default:
               D_UNIMPLEMENTED();

          case WTT_IDENTITY:
               break;

          case WTT_TRANSLATE_X:
               matrix[2].f = TEST_SCALAR_TO_FLOAT( transform->matrix[0], scalar );
               break;

          case WTT_TRANSLATE_Y:
               matrix[5].f = TEST_SCALAR_TO_FLOAT( transform->matrix[0], scalar );
               break;

          case WTT_TRANSLATE_X | WTT_TRANSLATE_Y:
               matrix[2].f = TEST_SCALAR_TO_FLOAT( transform->matrix[0], scalar );
               matrix[5].f = TEST_SCALAR_TO_FLOAT( transform->matrix[1], scalar );
               break;

          case WTT_SCALE_X:
               matrix[0].f = TEST_SCALAR_TO_FLOAT( transform->matrix[0], scalar );
               break;

          case WTT_SCALE_Y:
               matrix[4].f = TEST_SCALAR_TO_FLOAT( transform->matrix[0], scalar );
               break;

          case WTT_SCALE_X | WTT_SCALE_Y:
               matrix[0].f = TEST_SCALAR_TO_FLOAT( transform->matrix[0], scalar );
               matrix[4].f = TEST_SCALAR_TO_FLOAT( transform->matrix[1], scalar );
               break;

          case WTT_ROTATE_FREE:
               s = sinf( TEST_SCALAR_TO_FLOAT( transform->matrix[0], scalar ) );
               c = cosf( TEST_SCALAR_TO_FLOAT( transform->matrix[0], scalar ) );

               matrix[0].f =  c;
               matrix[1].f = -s;
               matrix[3].f =  s;
               matrix[4].f =  c;
               break;
     }

     direct_memcpy( transform->matrix, matrix, sizeof(transform->matrix) );

     transform->flags |= WTF_MATRIX;
     transform->scalar = WST_FLOAT;

     /* FIXME: Keep type, but clear it when matrix is modified. */
     transform->type   = WTF_NONE;
     transform->flags &= ~WTF_TYPE;

     for (i=0; i<6; i++)
          D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] %14.9f\n", i, transform->matrix[i].f );
}

void
TEST_Transform_Append( WaterTransform *transform, const WaterTransform *other )
{
     int                i;
     WaterScalar        matrix[6];
     const WaterScalar *a = &other->matrix[0];
     const WaterScalar *b = &transform->matrix[0];

     D_DEBUG_AT( IWater_TEST_Trans, "%s( %p, %p )\n", __FUNCTION__, transform, other );

     if (transform->scalar != WST_FLOAT)
          D_UNIMPLEMENTED();

     if (other->scalar != WST_FLOAT)
          D_UNIMPLEMENTED();

     for (i=0; i<6; i++)
          D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] %14.9f\n", i, transform->matrix[i].f );

     for (i=0; i<6; i++)
          D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] %14.9f\n", i, other->matrix[i].f );

     matrix[0].f = a[0].f * b[0].f + a[3].f * b[1].f;
     matrix[1].f = a[1].f * b[0].f + a[4].f * b[1].f;
     matrix[2].f = a[2].f * b[0].f + a[5].f * b[1].f + b[2].f;

     matrix[3].f = a[0].f * b[3].f + a[3].f * b[4].f;
     matrix[4].f = a[1].f * b[3].f + a[4].f * b[4].f;
     matrix[5].f = a[2].f * b[3].f + a[5].f * b[4].f + b[5].f;

     direct_memcpy( &transform->matrix[0], matrix, sizeof(matrix) );

     for (i=0; i<6; i++)
          D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] %14.9f\n", i, transform->matrix[i].f );
}

void
TEST_Transform_XY( WaterScalarType    scalar,
                   const WaterScalar *matrix,
                   int               *x,
                   int               *y )
{
     int x_, y_;

     switch (scalar) {
          case WST_INTEGER:
               x_ = *x * matrix[0].i + *y * matrix[1].i + matrix[2].i;
               y_ = *x * matrix[3].i + *y * matrix[4].i + matrix[5].i;
               break;

          case WST_FIXED_16_16:
               x_ = (TEST_MULT2_ADD_16_16( *x << 16, matrix[0].i, *y << 16, matrix[1].i ) + matrix[2].i + TEST_ROUND16) >> 16;
               y_ = (TEST_MULT2_ADD_16_16( *x << 16, matrix[3].i, *y << 16, matrix[4].i ) + matrix[5].i + TEST_ROUND16) >> 16;
               break;

          case WST_FLOAT:
               x_ = (int)((float)*x * matrix[0].f + (float)*y * matrix[1].f + matrix[2].f + 0.5f);
               y_ = (int)((float)*x * matrix[3].f + (float)*y * matrix[4].f + matrix[5].f + 0.5f);
               break;

          default:
               D_BUG( "unexpected scalar type 0x%08x", scalar );
               return;
     }

     D_DEBUG_AT( IWater_TEST_Trans, "(%4d,%4d) -> (%4d,%4d)\n", *x, *y, x_, y_ );

     *x = x_;
     *y = y_;
}

void
TEST_Transform_XY_float( WaterTransform    *transform,
                         float             *x,
                         float             *y )
{
     if (!(transform->flags & WTF_MATRIX)) {
          TEST_Transform_TypeToMatrix( transform );
     }

     *x = (int)((float)*x * transform->matrix[0].f + (float)*y * transform->matrix[1].f + transform->matrix[2].f + 0.5f);
     *y = (int)((float)*x * transform->matrix[3].f + (float)*y * transform->matrix[4].f + transform->matrix[5].f + 0.5f);
}

void
TEST_Transform_Points( const WaterTransform *transform,
                       DFBPoint             *points,
                       unsigned int          num_points )
{
     int                i, m   = 0;
     WaterScalarType    scalar = WST_INTEGER;
     const WaterScalar *mx     = &transform->matrix[0];
     WaterTransformType type   = transform->type;

     D_DEBUG_AT( IWater_TEST_Trans, "%s( %p, %p [%u] )\n", __FUNCTION__, transform, points, num_points );

     D_DEBUG_AT( IWater_TEST_Trans, "  -> SCALAR 0x%x\n", transform->scalar );

     scalar = transform->scalar;

     if (! (transform->flags & WTF_TYPE)) {
          if (transform->flags & WTF_MATRIX) {
               D_DEBUG_AT( IWater_TEST_Trans, "  -> MATRIX\n" );

               if (TEST_ANY_TRANSFORM( transform )) {
                    for (i=0; i<num_points; i++) {
                         TEST_Transform_XY( scalar, mx, &points[i].x, &points[i].y );
                    }
               }
          }

          return;
     }

     D_DEBUG_AT( IWater_TEST_Trans, "  -> TYPE   0x%08x\n", type );

     if (type & (WTT_SKEW_X | WTT_SKEW_Y)) {
          D_BUG( "skew point" );
          return;
     }

     if (type & WTT_ROTATE_FREE) {
          D_BUG( "rotate free point" );
          return;
     }

     switch (type) {
          case WTT_UNKNOWN:
               return;

          case WTT_ZERO:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->  ZERO\n" );
               memset( points, 0, sizeof(points[0]) * num_points );
               return;

          case WTT_IDENTITY:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->  IDENTITY\n" );
               return;

          case WTT_ROTATE_Q_90:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_90\n" );
               D_UNIMPLEMENTED();
               return;

          case WTT_ROTATE_Q_180:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_180\n" );
               D_UNIMPLEMENTED();
               return;

          case WTT_ROTATE_Q_270:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_270\n" );
               D_UNIMPLEMENTED();
               return;

          default:
               break;
     }

     if (type & WTT_TRANSLATE_X) {
          switch (scalar) {
               case WST_INTEGER:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %4d\n", m, mx[m].i );
                    for (i=0; i<num_points; i++)
                         points[i].x += mx[m].i;
                    break;

               case WST_FIXED_16_16:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %c%4d.%05u\n",
                                m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                    for (i=0; i<num_points; i++)
                         points[i].x += (mx[m].i + TEST_ROUND16) >> 16;
                    break;

               case WST_FLOAT:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %4.14f\n", m, mx[m].f );
                    for (i=0; i<num_points; i++)
                         points[i].x += (int)(mx[m].f + 0.5f);
                    break;

               default:
                    D_BUG( "unexpected scalar type 0x%08x", scalar );
                    break;
          }
          m++;

          type &= ~WTT_TRANSLATE_X;
     }
     if (type & WTT_TRANSLATE_Y) {
          switch (scalar) {
               case WST_INTEGER:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %4d\n", m, mx[m].i );
                    for (i=0; i<num_points; i++)
                         points[i].y += mx[m].i;
                    break;

               case WST_FIXED_16_16:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %c%4d.%05u\n",
                                m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                    for (i=0; i<num_points; i++)
                         points[i].y += (mx[m].i + TEST_ROUND16) >> 16;
                    break;

               case WST_FLOAT:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %4.14f\n", m, mx[m].f );
                    for (i=0; i<num_points; i++)
                         points[i].y += (int)(mx[m].f + 0.5f);
                    break;

               default:
                    D_BUG( "unexpected scalar type 0x%08x", scalar );
                    break;
          }
          m++;

          type &= ~WTT_TRANSLATE_Y;
     }
     if (type & WTT_SCALE_X) {
          switch (scalar) {
               case WST_INTEGER:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %4d\n", m, mx[m].i );
                    for (i=0; i<num_points; i++)
                         points[i].x *= mx[m].i;
                    break;

               case WST_FIXED_16_16:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %c%4d.%05u\n",
                                m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                    for (i=0; i<num_points; i++)
                         points[i].x = (points[i].x * mx[m].i + TEST_ROUND16) >> 16;
                    break;

               case WST_FLOAT:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %4.14f\n", m, mx[m].f );
                    for (i=0; i<num_points; i++)
                         points[i].x = (int)((float)points[i].x * mx[m].f + 0.5f);
                    break;

               default:
                    D_BUG( "unexpected scalar type 0x%08x", scalar );
                    break;
          }
          m++;

          type &= ~WTT_SCALE_X;
     }
     if (type & WTT_SCALE_Y) {
          switch (scalar) {
               case WST_INTEGER:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %4d\n", m, mx[m].i );
                    for (i=0; i<num_points; i++)
                         points[i].y *= mx[m].i;
                    break;

               case WST_FIXED_16_16:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %c%4d.%05u\n",
                                m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                    for (i=0; i<num_points; i++)
                         points[i].y = (points[i].y * mx[m].i + TEST_ROUND16) >> 16;
                    break;

               case WST_FLOAT:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %4.14f\n", m, mx[m].f );
                    for (i=0; i<num_points; i++)
                         points[i].y = (int)((float)points[i].y * mx[m].f + 0.5f);
                    break;

               default:
                    D_BUG( "unexpected scalar type 0x%08x", scalar );
                    break;
          }
          m++;

          type &= ~WTT_SCALE_Y;
     }
     if (type & WTT_FLIP_X) {
          D_DEBUG_AT( IWater_TEST_Trans, "  ->      FLIP_X\n" );
          D_UNIMPLEMENTED();

          type &= ~WTT_FLIP_X;
     }
     if (type & WTT_FLIP_Y) {
          D_DEBUG_AT( IWater_TEST_Trans, "  ->      FLIP_Y\n" );
          D_UNIMPLEMENTED();

          type &= ~WTT_FLIP_Y;
     }
     switch (type & WTT_ROTATE_MASK) {
          case WTT_ROTATE_Q_90:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_90\n" );
               D_UNIMPLEMENTED();
               break;

          case WTT_ROTATE_Q_180:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_180\n" );
               D_UNIMPLEMENTED();
               break;

          case WTT_ROTATE_Q_270:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_270\n" );
               D_UNIMPLEMENTED();
               break;

          case WTT_UNKNOWN:
               break;

          default:
               D_BUG( "unexpected rotation flags 0x%08x", type & WTT_ROTATE_MASK );
               break;
     }

     type &= ~WTT_ROTATE_MASK;

     if (type)
          D_WARN( "unhandled flags 0x%08x", type );
}

void
TEST_Transform_Rectangles( const WaterTransform *transform,
                           DFBRectangle         *rects,
                           unsigned int          num_rects )
{
     int                i, m   = 0;
     WaterScalarType    scalar = WST_INTEGER;
     const WaterScalar *mx     = &transform->matrix[0];
     WaterTransformType type   = transform->type;

     D_DEBUG_AT( IWater_TEST_Trans, "%s( %p, %p [%u] )\n", __FUNCTION__, transform, rects, num_rects );

     D_DEBUG_AT( IWater_TEST_Trans, "  -> SCALAR 0x%x\n", transform->scalar );

     scalar = transform->scalar;

     if (! (transform->flags & WTF_TYPE)) {
          if (transform->flags & WTF_MATRIX) {
               D_DEBUG_AT( IWater_TEST_Trans, "  -> MATRIX\n" );

               if (TEST_NONRECT_TRANSFORM( transform )) {
                    D_BUG( "transform free rect" );
                    return;
               }

               if (TEST_ANY_TRANSFORM( transform )) {
                    for (i=0; i<num_rects; i++) {
                         int x2 = rects[i].x + rects[i].w;
                         int y2 = rects[i].y + rects[i].h;

                         TEST_Transform_XY( scalar, mx, &rects[i].x, &rects[i].y );
                         TEST_Transform_XY( scalar, mx, &x2, &y2 );

                         rects[i].w = x2 - rects[i].x;
                         rects[i].h = y2 - rects[i].y;
                    }
               }
          }

          return;
     }

     D_DEBUG_AT( IWater_TEST_Trans, "  -> TYPE   0x%08x\n", type );

     if (type & (WTT_SKEW_X | WTT_SKEW_Y)) {
          D_BUG( "skew rect" );
          return;
     }

     if (type & WTT_ROTATE_FREE) {
          D_BUG( "rotate free rect" );
          return;
     }

     switch (type) {
          case WTT_UNKNOWN:
               return;

          case WTT_ZERO:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->  ZERO\n" );
               memset( rects, 0, sizeof(rects[0]) * num_rects );
               return;

          case WTT_IDENTITY:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->  IDENTITY\n" );
               return;

          case WTT_ROTATE_Q_90:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_90\n" );
               D_UNIMPLEMENTED();
               return;

          case WTT_ROTATE_Q_180:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_180\n" );
               D_UNIMPLEMENTED();
               return;

          case WTT_ROTATE_Q_270:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_270\n" );
               D_UNIMPLEMENTED();
               return;

          default:
               break;
     }

     if (type & WTT_TRANSLATE_X) {
          switch (scalar) {
               case WST_INTEGER:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %4d\n", m, mx[m].i );
                    for (i=0; i<num_rects; i++)
                         rects[i].x += mx[m].i;
                    break;

               case WST_FIXED_16_16:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %c%4d.%05u\n",
                                m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                    for (i=0; i<num_rects; i++)
                         rects[i].x += (mx[m].i + TEST_ROUND16) >> 16;
                    break;

               case WST_FLOAT:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %4.14f\n", m, mx[m].f );
                    for (i=0; i<num_rects; i++)
                         rects[i].x += (int)(mx[m].f + 0.5f);
                    break;

               default:
                    D_BUG( "unexpected scalar type 0x%08x", scalar );
                    break;
          }
          m++;

          type &= ~WTT_TRANSLATE_X;
     }
     if (type & WTT_TRANSLATE_Y) {
          switch (scalar) {
               case WST_INTEGER:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %4d\n", m, mx[m].i );
                    for (i=0; i<num_rects; i++)
                         rects[i].y += mx[m].i;
                    break;

               case WST_FIXED_16_16:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %c%4d.%05u\n",
                                m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                    for (i=0; i<num_rects; i++)
                         rects[i].y += (mx[m].i + TEST_ROUND16) >> 16;
                    break;

               case WST_FLOAT:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %4.14f\n", m, mx[m].f );
                    for (i=0; i<num_rects; i++)
                         rects[i].y += (int)(mx[m].f + 0.5f);
                    break;

               default:
                    D_BUG( "unexpected scalar type 0x%08x", scalar );
                    break;
          }
          m++;

          type &= ~WTT_TRANSLATE_Y;
     }
     if (type & WTT_SCALE_X) {
          switch (scalar) {
               case WST_INTEGER:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %4d\n", m, mx[m].i );
                    for (i=0; i<num_rects; i++) {
                         rects[i].x *= mx[m].i;
                         rects[i].w *= mx[m].i;
                    }
                    break;

               case WST_FIXED_16_16:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %c%4d.%05u\n",
                                m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                    for (i=0; i<num_rects; i++) {
                         rects[i].x = (rects[i].x * mx[m].i + TEST_ROUND16) >> 16;
                         rects[i].w = (rects[i].w * mx[m].i + TEST_ROUND16) >> 16;
                    }
                    break;

               case WST_FLOAT:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %4.14f\n", m, mx[m].f );
                    for (i=0; i<num_rects; i++) {
                         rects[i].x = (int)((float)rects[i].x * mx[m].f + 0.5f);
                         rects[i].w = (int)((float)rects[i].w * mx[m].f + 0.5f);
                    }
                    break;

               default:
                    D_BUG( "unexpected scalar type 0x%08x", scalar );
                    break;
          }
          m++;

          type &= ~WTT_SCALE_X;
     }
     if (type & WTT_SCALE_Y) {
          switch (scalar) {
               case WST_INTEGER:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %4d\n", m, mx[m].i );
                    for (i=0; i<num_rects; i++) {
                         rects[i].y *= mx[m].i;
                         rects[i].h *= mx[m].i;
                    }
                    break;

               case WST_FIXED_16_16:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %c%4d.%05u\n",
                                m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                    for (i=0; i<num_rects; i++) {
                         rects[i].y = (rects[i].y * mx[m].i + TEST_ROUND16) >> 16;
                         rects[i].h = (rects[i].h * mx[m].i + TEST_ROUND16) >> 16;
                    }
                    break;

               case WST_FLOAT:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %4.14f\n", m, mx[m].f );
                    for (i=0; i<num_rects; i++) {
                         rects[i].y = (int)((float)rects[i].y * mx[m].f + 0.5f);
                         rects[i].h = (int)((float)rects[i].h * mx[m].f + 0.5f);
                    }
                    break;

               default:
                    D_BUG( "unexpected scalar type 0x%08x", scalar );
                    break;
          }
          m++;

          type &= ~WTT_SCALE_Y;
     }
     if (type & WTT_FLIP_X) {
          D_DEBUG_AT( IWater_TEST_Trans, "  ->      FLIP_X\n" );
          D_UNIMPLEMENTED();

          type &= ~WTT_FLIP_X;
     }
     if (type & WTT_FLIP_Y) {
          D_DEBUG_AT( IWater_TEST_Trans, "  ->      FLIP_Y\n" );
          D_UNIMPLEMENTED();

          type &= ~WTT_FLIP_Y;
     }
     switch (type & WTT_ROTATE_MASK) {
          case WTT_ROTATE_Q_90:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_90\n" );
               D_UNIMPLEMENTED();
               break;

          case WTT_ROTATE_Q_180:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_180\n" );
               D_UNIMPLEMENTED();
               break;

          case WTT_ROTATE_Q_270:
               D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_270\n" );
               D_UNIMPLEMENTED();
               break;

          case WTT_UNKNOWN:
               break;

          default:
               D_BUG( "unexpected rotation flags 0x%08x", type & WTT_ROTATE_MASK );
               break;
     }

     type &= ~WTT_ROTATE_MASK;

     if (type)
          D_WARN( "unhandled flags 0x%08x", type );
}

void
TEST_Transform_Regions( const WaterTransform *transform,
                        DFBRegion            *regions,
                        unsigned int          num_regions )
{
     int                i, m   = 0;
     WaterScalarType    scalar = WST_INTEGER;
     const WaterScalar *mx     = &transform->matrix[0];

     D_DEBUG_AT( IWater_TEST_Trans, "%s( %p, %p [%u] )\n", __FUNCTION__, transform, regions, num_regions );

     D_DEBUG_AT( IWater_TEST_Trans, "  -> SCALAR 0x%x\n", transform->scalar );

     scalar = transform->scalar;

     if (transform->flags & WTF_TYPE) {
          D_DEBUG_AT( IWater_TEST_Trans, "  -> TYPE   0x%08x\n", transform->type );

          switch (transform->type) {
               case WTT_UNKNOWN:
                    break;

               case WTT_ZERO:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  ZERO\n" );
                    memset( regions, 0, sizeof(regions[0]) * num_regions );
                    return;

               case WTT_IDENTITY:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  IDENTITY\n" );
                    return;

               case WTT_ROTATE_Q_90:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_90\n" );
                    D_UNIMPLEMENTED();
                    return;

               case WTT_ROTATE_Q_180:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_180\n" );
                    D_UNIMPLEMENTED();
                    return;

               case WTT_ROTATE_Q_270:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_270\n" );
                    D_UNIMPLEMENTED();
                    return;

               default:
                    if (transform->type & WTT_TRANSLATE_X) {
                         switch (scalar) {
                              case WST_INTEGER:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %4d\n", m, mx[m].i );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].x1 += mx[m].i;
                                        regions[i].x2 += mx[m].i;
                                   }
                                   break;

                              case WST_FIXED_16_16:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %c%4d.%05u\n",
                                               m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].x1 += (mx[m].i + TEST_ROUND16) >> 16;
                                        regions[i].x2 += (mx[m].i + TEST_ROUND16) >> 16;
                                   }
                                   break;

                              case WST_FLOAT:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %4.14f\n", m, mx[m].f );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].x1 += (int)(mx[m].f + 0.5f);
                                        regions[i].x2 += (int)(mx[m].f + 0.5f);
                                   }
                                   break;

                              default:
                                   D_BUG( "unexpected scalar type 0x%08x", scalar );
                                   break;
                         }
                         m++;
                    }
                    if (transform->type & WTT_TRANSLATE_Y) {
                         switch (scalar) {
                              case WST_INTEGER:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %4d\n", m, mx[m].i );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].y1 += mx[m].i;
                                        regions[i].y2 += mx[m].i;
                                   }
                                   break;

                              case WST_FIXED_16_16:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %c%4d.%05u\n",
                                               m, TEST_FIXED_16_16_VALUES_05( mx[m].i >> 16 ) );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].y1 += (mx[m].i + TEST_ROUND16) >> 16;
                                        regions[i].y2 += (mx[m].i + TEST_ROUND16) >> 16;
                                   }
                                   break;

                              case WST_FLOAT:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %4.14f\n", m, mx[m].f );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].y1 += (int)(mx[m].f + 0.5f);
                                        regions[i].y2 += (int)(mx[m].f + 0.5f);
                                   }
                                   break;

                              default:
                                   D_BUG( "unexpected scalar type 0x%08x", scalar );
                                   break;
                         }
                         m++;
                    }
                    if (transform->type & WTT_SCALE_X) {
                         switch (scalar) {
                              case WST_INTEGER:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %4d\n", m, mx[m].i );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].x1 *= mx[m].i;
                                        regions[i].x2 *= mx[m].i;
                                   }
                                   break;

                              case WST_FIXED_16_16:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %c%4d.%05u\n",
                                               m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].x1 = (regions[i].x1 * mx[m].i + TEST_ROUND16) >> 16;
                                        regions[i].x2 = (regions[i].x2 * mx[m].i + TEST_ROUND16) >> 16;
                                   }
                                   break;

                              case WST_FLOAT:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %4.14f\n", m, mx[m].f );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].x1 = (int)((float)regions[i].x1 * mx[m].f + 0.5f);
                                        regions[i].x2 = (int)((float)regions[i].x2 * mx[m].f + 0.5f);
                                   }
                                   break;

                              default:
                                   D_BUG( "unexpected scalar type 0x%08x", scalar );
                                   break;
                         }
                         m++;
                    }
                    if (transform->type & WTT_SCALE_Y) {
                         switch (scalar) {
                              case WST_INTEGER:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %4d\n", m, mx[m].i );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].y1 *= mx[m].i;
                                        regions[i].y2 *= mx[m].i;
                                   }
                                   break;

                              case WST_FIXED_16_16:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %c%4d.%05u\n",
                                               m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].y1 = (regions[i].y1 * mx[m].i + TEST_ROUND16) >> 16;
                                        regions[i].y2 = (regions[i].y2 * mx[m].i + TEST_ROUND16) >> 16;
                                   }
                                   break;

                              case WST_FLOAT:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %4.14f\n", m, mx[m].f );
                                   for (i=0; i<num_regions; i++) {
                                        regions[i].y1 = (int)((float)regions[i].y1 * mx[m].f + 0.5f);
                                        regions[i].y2 = (int)((float)regions[i].y2 * mx[m].f + 0.5f);
                                   }
                                   break;

                              default:
                                   D_BUG( "unexpected scalar type 0x%08x", scalar );
                                   break;
                         }
                         m++;
                    }
#if D_DEBUG_ENABLED
                    if (transform->type & WTT_FLIP_X) {
                         D_DEBUG_AT( IWater_TEST_Trans, "  ->      FLIP_X\n" );
                         D_UNIMPLEMENTED();
                    }
                    if (transform->type & WTT_FLIP_Y) {
                         D_DEBUG_AT( IWater_TEST_Trans, "  ->      FLIP_Y\n" );
                         D_UNIMPLEMENTED();
                    }
#endif
                    if (transform->type & WTT_SKEW_X) {
                         D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SKEW_X\n", m );
                         D_UNIMPLEMENTED();
                         m++;
                    }
                    if (transform->type & WTT_SKEW_Y) {
                         D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SKEW_Y\n", m );
                         D_UNIMPLEMENTED();
                         m++;
                    }
                    switch (transform->type & WTT_ROTATE_MASK) {
                         case WTT_ROTATE_Q_90:
                              D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_90\n" );
                              D_UNIMPLEMENTED();
                              break;

                         case WTT_ROTATE_Q_180:
                              D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_180\n" );
                              D_UNIMPLEMENTED();
                              break;

                         case WTT_ROTATE_Q_270:
                              D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_270\n" );
                              D_UNIMPLEMENTED();
                              break;

                         case WTT_ROTATE_FREE:
                              D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] ROTATE_FREE\n", m );
                              D_UNIMPLEMENTED();
                              //m++;
                              break;

                         case WTT_UNKNOWN:
                              break;

                         default:
                              D_BUG( "unexpected rotation flags 0x%08x", transform->type & WTT_ROTATE_MASK );
                              break;
                    }
                    break;
          }
     }

     /* FIXME: Not correct condition. */
     if (!m) {
          TEST_Transform_TypeToMatrix_16_16( (void*) transform );

          for (i=0; i<num_regions; i++) {
               TEST_Transform_XY( scalar, mx, &regions[i].x1, &regions[i].y1 );
               TEST_Transform_XY( scalar, mx, &regions[i].x2, &regions[i].y2 );
          }
     }
}

void
TEST_Transform_Triangles( const WaterTransform *transform,
                          DFBTriangle          *triangles,
                          unsigned int          num_triangles )
{
     int                i, m   = 0;
     WaterScalarType    scalar = WST_INTEGER;
     const WaterScalar *mx     = &transform->matrix[0];

     D_DEBUG_AT( IWater_TEST_Trans, "%s( %p, %p [%u] )\n", __FUNCTION__, transform, triangles, num_triangles );

     D_DEBUG_AT( IWater_TEST_Trans, "  -> SCALAR 0x%x\n", transform->scalar );

     scalar = transform->scalar;

     if (transform->flags & WTF_TYPE) {
          D_DEBUG_AT( IWater_TEST_Trans, "  -> TYPE   0x%08x\n", transform->type );

          switch (transform->type) {
               case WTT_UNKNOWN:
                    break;

               case WTT_ZERO:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  ZERO\n" );
                    memset( triangles, 0, sizeof(triangles[0]) * num_triangles );
                    return;

               case WTT_IDENTITY:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  IDENTITY\n" );
                    return;

               case WTT_ROTATE_Q_90:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_90\n" );
                    D_UNIMPLEMENTED();
                    return;

               case WTT_ROTATE_Q_180:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_180\n" );
                    D_UNIMPLEMENTED();
                    return;

               case WTT_ROTATE_Q_270:
                    D_DEBUG_AT( IWater_TEST_Trans, "  ->  ROTATE_Q_270\n" );
                    D_UNIMPLEMENTED();
                    return;

               default:
                    if (transform->type & WTT_TRANSLATE_X) {
                         switch (scalar) {
                              case WST_INTEGER:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %4d\n", m, mx[m].i );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].x1 += mx[m].i;
                                        triangles[i].x2 += mx[m].i;
                                        triangles[i].x3 += mx[m].i;
                                   }
                                   break;

                              case WST_FIXED_16_16:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %c%4d.%05u\n",
                                               m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].x1 += (mx[m].i + TEST_ROUND16) >> 16;
                                        triangles[i].x2 += (mx[m].i + TEST_ROUND16) >> 16;
                                        triangles[i].x3 += (mx[m].i + TEST_ROUND16) >> 16;
                                   }
                                   break;

                              case WST_FLOAT:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_X %4.14f\n", m, mx[m].f );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].x1 += (int)(mx[m].f + 0.5f);
                                        triangles[i].x2 += (int)(mx[m].f + 0.5f);
                                        triangles[i].x3 += (int)(mx[m].f + 0.5f);
                                   }
                                   break;

                              default:
                                   D_BUG( "unexpected scalar type 0x%08x", scalar );
                                   break;
                         }
                         m++;
                    }
                    if (transform->type & WTT_TRANSLATE_Y) {
                         switch (scalar) {
                              case WST_INTEGER:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %4d\n", m, mx[m].i );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].y1 += mx[m].i;
                                        triangles[i].y2 += mx[m].i;
                                        triangles[i].y3 += mx[m].i;
                                   }
                                   break;

                              case WST_FIXED_16_16:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %c%4d.%05u\n",
                                               m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].y1 += (mx[m].i + TEST_ROUND16) >> 16;
                                        triangles[i].y2 += (mx[m].i + TEST_ROUND16) >> 16;
                                        triangles[i].y3 += (mx[m].i + TEST_ROUND16) >> 16;
                                   }
                                   break;

                              case WST_FLOAT:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] TRANSLATE_Y %4.14f\n", m, mx[m].f );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].y1 += (int)(mx[m].f + 0.5f);
                                        triangles[i].y2 += (int)(mx[m].f + 0.5f);
                                        triangles[i].y3 += (int)(mx[m].f + 0.5f);
                                   }
                                   break;

                              default:
                                   D_BUG( "unexpected scalar type 0x%08x", scalar );
                                   break;
                         }
                         m++;
                    }
                    if (transform->type & WTT_SCALE_X) {
                         switch (scalar) {
                              case WST_INTEGER:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %4d\n", m, mx[m].i );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].x1 *= mx[m].i;
                                        triangles[i].x2 *= mx[m].i;
                                        triangles[i].x3 *= mx[m].i;
                                   }
                                   break;

                              case WST_FIXED_16_16:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %c%4d.%05u\n",
                                               m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].x1 = (triangles[i].x1 * mx[m].i + TEST_ROUND16) >> 16;
                                        triangles[i].x2 = (triangles[i].x2 * mx[m].i + TEST_ROUND16) >> 16;
                                        triangles[i].x3 = (triangles[i].x3 * mx[m].i + TEST_ROUND16) >> 16;
                                   }
                                   break;

                              case WST_FLOAT:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_X %4.14f\n", m, mx[m].f );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].x1 = (int)((float)triangles[i].x1 * mx[m].f + 0.5f);
                                        triangles[i].x2 = (int)((float)triangles[i].x2 * mx[m].f + 0.5f);
                                        triangles[i].x3 = (int)((float)triangles[i].x3 * mx[m].f + 0.5f);
                                   }
                                   break;

                              default:
                                   D_BUG( "unexpected scalar type 0x%08x", scalar );
                                   break;
                         }
                         m++;
                    }
                    if (transform->type & WTT_SCALE_Y) {
                         switch (scalar) {
                              case WST_INTEGER:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %4d\n", m, mx[m].i );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].y1 *= mx[m].i;
                                        triangles[i].y2 *= mx[m].i;
                                        triangles[i].y3 *= mx[m].i;
                                   }
                                   break;

                              case WST_FIXED_16_16:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %c%4d.%05u\n",
                                               m, TEST_FIXED_16_16_VALUES_05( mx[m].i ) );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].y1 = (triangles[i].y1 * mx[m].i + TEST_ROUND16) >> 16;
                                        triangles[i].y2 = (triangles[i].y2 * mx[m].i + TEST_ROUND16) >> 16;
                                        triangles[i].y3 = (triangles[i].y3 * mx[m].i + TEST_ROUND16) >> 16;
                                   }
                                   break;

                              case WST_FLOAT:
                                   D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SCALE_Y %4.14f\n", m, mx[m].f );
                                   for (i=0; i<num_triangles; i++) {
                                        triangles[i].y1 = (int)((float)triangles[i].y1 * mx[m].f + 0.5f);
                                        triangles[i].y2 = (int)((float)triangles[i].y2 * mx[m].f + 0.5f);
                                        triangles[i].y3 = (int)((float)triangles[i].y3 * mx[m].f + 0.5f);
                                   }
                                   break;

                              default:
                                   D_BUG( "unexpected scalar type 0x%08x", scalar );
                                   break;
                         }
                         m++;
                    }
#if D_DEBUG_ENABLED
                    if (transform->type & WTT_FLIP_X) {
                         D_DEBUG_AT( IWater_TEST_Trans, "  ->      FLIP_X\n" );
                         D_UNIMPLEMENTED();
                    }
                    if (transform->type & WTT_FLIP_Y) {
                         D_DEBUG_AT( IWater_TEST_Trans, "  ->      FLIP_Y\n" );
                         D_UNIMPLEMENTED();
                    }
#endif
                    if (transform->type & WTT_SKEW_X) {
                         D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SKEW_X\n", m );
                         D_UNIMPLEMENTED();
                         m++;
                    }
                    if (transform->type & WTT_SKEW_Y) {
                         D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] SKEW_Y\n", m );
                         D_UNIMPLEMENTED();
                         m++;
                    }
                    switch (transform->type & WTT_ROTATE_MASK) {
                         case WTT_ROTATE_Q_90:
                              D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_90\n" );
                              D_UNIMPLEMENTED();
                              break;

                         case WTT_ROTATE_Q_180:
                              D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_180\n" );
                              D_UNIMPLEMENTED();
                              break;

                         case WTT_ROTATE_Q_270:
                              D_DEBUG_AT( IWater_TEST_Trans, "  ->      ROTATE_Q_270\n" );
                              D_UNIMPLEMENTED();
                              break;

                         case WTT_ROTATE_FREE:
                              D_DEBUG_AT( IWater_TEST_Trans, "  ->  [%d] ROTATE_FREE\n", m );
                              D_UNIMPLEMENTED();
                              //m++;
                              break;

                         case WTT_UNKNOWN:
                              break;

                         default:
                              D_BUG( "unexpected rotation flags 0x%08x", transform->type & WTT_ROTATE_MASK );
                              break;
                    }
                    break;
          }
     }

     /* FIXME: Not correct condition. */
     if (!m) {
          TEST_Transform_TypeToMatrix_16_16( (void*) transform );

          for (i=0; i<num_triangles; i++) {
               TEST_Transform_XY( scalar, mx, &triangles[i].x1, &triangles[i].y1 );
               TEST_Transform_XY( scalar, mx, &triangles[i].x2, &triangles[i].y2 );
               TEST_Transform_XY( scalar, mx, &triangles[i].x3, &triangles[i].y3 );
          }
     }
}

