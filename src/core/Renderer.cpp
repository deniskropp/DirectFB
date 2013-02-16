/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include "Renderer.h"

extern "C" {
#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/surface_allocation.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>
#include <gfx/clip.h>
#include <gfx/util.h>
}

#include <Util.h>


D_DEBUG_DOMAIN( DirectFB_Renderer, "DirectFB/Renderer", "DirectFB Renderer" );

/*********************************************************************************************************************/


typedef struct {
     int                 magic;

     DirectFB::Renderer *last_renderer;
} RendererTLS;

static DirectTLS renderer_tls_key;

static void
renderer_tls_destroy( void *arg )
{
     RendererTLS *renderer_tls = (RendererTLS*) arg;

     D_MAGIC_ASSERT( renderer_tls, RendererTLS );

     D_MAGIC_CLEAR( renderer_tls );

     D_FREE( renderer_tls );
}


void
Renderer_TLS__init( void )
{
     direct_tls_register( &renderer_tls_key, renderer_tls_destroy );
}

void
Renderer_TLS__deinit( void )
{
     direct_tls_unregister( &renderer_tls_key );
}

static RendererTLS *
Renderer_GetTLS( void )
{
     RendererTLS *renderer_tls;

     renderer_tls = (RendererTLS*) direct_tls_get( renderer_tls_key );
     if (!renderer_tls) {
          renderer_tls = (RendererTLS*) D_CALLOC( 1, sizeof(RendererTLS) );
          if (!renderer_tls) {
               D_OOM();
               return NULL;
          }

          D_MAGIC_SET( renderer_tls, RendererTLS );

          direct_tls_set( renderer_tls_key, renderer_tls );
     }

     D_MAGIC_ASSERT( renderer_tls, RendererTLS );

     return renderer_tls;
}



namespace DirectFB {


#define TRANSFORM(_x,_y,p)  \
          (p).x = ((_x) * (matrix)[0] + (_y) * (matrix)[1] + (matrix)[2] + 0x8000) >> 16; \
          (p).y = ((_x) * (matrix)[3] + (_y) * (matrix)[4] + (matrix)[5] + 0x8000) >> 16;

#define TRANSFORM_XY(_x,_y,_nx,_ny)  \
          (_nx) = ((_x) * (matrix)[0] + (_y) * (matrix)[1] + (matrix)[2] + 0x8000) >> 16; \
          (_ny) = ((_x) * (matrix)[3] + (_y) * (matrix)[4] + (matrix)[5] + 0x8000) >> 16;



namespace Primitives {


class Rectangles : public Base {
public:
     Rectangles( const DFBRectangle  *rects,
                 unsigned int         num_rects,
                 DFBAccelerationMask  accel,
                 bool                 clipped = false,
                 bool                 del = false )
          :
          Base( accel, clipped, del ),
          rects( (DFBRectangle*) rects ),
          num_rects( num_rects )
     {
     }

     virtual ~Rectangles() {
          if (del)
               delete rects;
     }

     virtual unsigned int count() const {
          return num_rects;
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const s32           *matrix );

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine );

     DFBRectangle *rects;
     unsigned int  num_rects;
};



class Blits : public Base {
public:
     Blits( const DFBRectangle  *rects,
            const DFBPoint      *points,
            unsigned int         num_rects,
            DFBAccelerationMask  accel,
            bool                 clipped = false,
            bool                 del = false )
          :
          Base( accel, clipped, del ),
          rects( (DFBRectangle*) rects ),
          points( (DFBPoint*) points ),
          num_rects( num_rects )
     {
     }

     virtual ~Blits() {
          if (del) {
               delete rects;
               delete points;
          }
     }

     virtual unsigned int count() const {
          return num_rects;
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const s32           *matrix );

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine );

     DFBRectangle *rects;
     DFBPoint     *points;
     unsigned int  num_rects;
};



class StretchBlits : public Base {
public:
     StretchBlits( const DFBRectangle  *srects,
                   const DFBRectangle  *drects,
                   unsigned int         num_rects,
                   DFBAccelerationMask  accel,
                   bool                 clipped = false,
                   bool                 del = false )
          :
          Base( accel, clipped, del ),
          srects( (DFBRectangle*) srects ),
          drects( (DFBRectangle*) drects ),
          num_rects( num_rects )
     {
     }

     virtual ~StretchBlits() {
          if (del) {
               delete srects;
               delete drects;
          }
     }

     virtual unsigned int count() const {
          return num_rects;
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const s32           *matrix );

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine );

     DFBRectangle *srects;
     DFBRectangle *drects;
     unsigned int  num_rects;
};



class Lines : public Base {
public:
     Lines( const DFBRegion     *lines,
            unsigned int         num_lines,
            DFBAccelerationMask  accel,
            bool                 clipped = false,
            bool                 del = false )
          :
          Base( accel, clipped, del ),
          lines( (DFBRegion*) lines ),
          num_lines( num_lines )
     {
     }

     virtual ~Lines() {
          if (del)
               delete lines;
     }

     virtual unsigned int count() const {
          return num_lines;
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const s32           *matrix );

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine );

     DFBRegion    *lines;
     unsigned int  num_lines;
};



class Spans : public Base {
public:
     Spans( int                  y,
            const DFBSpan       *spans,
            unsigned int         num_spans,
            DFBAccelerationMask  accel,
            bool                 clipped = false,
            bool                 del = false )
          :
          Base( accel, clipped, del ),
          y( y ),
          spans( (DFBSpan*) spans ),
          num_spans( num_spans )
     {
     }

     virtual ~Spans() {
          if (del)
               delete spans;
     }

     virtual unsigned int count() const {
          return num_spans;
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const s32           *matrix );

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine );

     int           y;
     DFBSpan      *spans;
     unsigned int  num_spans;
};



class Trapezoids : public Base {
public:
     Trapezoids( const DFBTrapezoid  *traps,
                 unsigned int         num_traps,
                 DFBAccelerationMask  accel,
                 bool                 clipped = false,
                 bool                 del = false )
                 :
                 Base( accel, clipped, del ),
                 traps( (DFBTrapezoid*) traps ),
                 num_traps( num_traps )
     {
     }

     virtual ~Trapezoids() {
          if (del)
               delete traps;
     }

     virtual unsigned int count() const {
          return num_traps;
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const s32           *matrix );

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine );

     DFBTrapezoid *traps;
     unsigned int  num_traps;
};



class Triangles : public Base {
public:
     Triangles( const DFBTriangle   *tris,
                unsigned int         num_tris,
                DFBAccelerationMask  accel,
                bool                 clipped = false,
                bool                 del = false )
          :
          Base( accel, clipped, del ),
          tris( (DFBTriangle*) tris ),
          num_tris( num_tris )
     {
     }

     virtual ~Triangles() {
          if (del)
               delete tris;
     }

     virtual unsigned int count() const {
          return num_tris;
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const s32           *matrix );

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine );

     DFBTriangle  *tris;
     unsigned int  num_tris;
};



class TexTriangles : public Base {
public:
     TexTriangles( const DFBVertex      *vertices,
                   int                   num,
                   DFBTriangleFormation  formation,
                   DFBAccelerationMask   accel,
                   bool                  clipped = false,
                   bool                  del = false )
          :
          Base( accel, clipped, del ),
          vertices( (DFBVertex*) vertices ),
          num( num ),
          formation( formation )
     {
     }

     virtual ~TexTriangles() {
          if (del)
               delete vertices;
     }

     virtual unsigned int count() const {
          return num;
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const s32           *matrix );

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine );

     DFBVertex            *vertices;
     unsigned int          num;
     DFBTriangleFormation  formation;
};



class TexTriangles1616 : public Base {
public:
     TexTriangles1616( const DFBVertex1616  *vertices,
                       int                   num,
                       DFBTriangleFormation  formation,
                       DFBAccelerationMask   accel,
                       bool                  clipped = false,
                       bool                  del = false )
          :
          Base( accel, clipped, del ),
          vertices( (DFBVertex1616*) vertices ),
          num( num ),
          formation( formation )
     {
     }

     virtual ~TexTriangles1616() {
          if (del)
               delete vertices;
     }

     virtual unsigned int count() const {
          return num;
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const s32           *matrix );

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine );

     DFBVertex1616        *vertices;
     unsigned int          num;
     DFBTriangleFormation  formation;
};



class Quadrangles : public Base {
public:
     Quadrangles( const DFBPoint      *points,
                  unsigned int         num_quads,
                  DFBAccelerationMask  accel,
                  bool                 clipped = false,
                  bool                 del = false )
          :
          Base( accel, clipped, del ),
          points( (DFBPoint*) points ),
          num_quads( num_quads )
     {
     }

     virtual ~Quadrangles() {
          if (del)
               delete points;
     }

     virtual unsigned int count() const {
          return num_quads;
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const s32           *matrix );

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine );

     DFBPoint     *points;
     unsigned int  num_quads;
};


Base *
Rectangles::tesselate( DFBAccelerationMask  accel,
                       const s32           *matrix )
{
     switch (this->accel) {
          case DFXL_FILLRECTANGLE:
               switch (accel) {
                    case DFXL_FILLRECTANGLE:
                         /* fill rect to fill rect used for transformation only */
                         D_ASSUME( matrix != NULL );
                         if (!matrix)
                              return NULL;

                         {
                              DFBRectangle *newrects = new DFBRectangle[num_rects];

                              for (unsigned int i=0; i<num_rects; i++) {
                                   DFBPoint p1, p2;

                                   TRANSFORM( rects[i].x,              rects[i].y,              p1 );
                                   TRANSFORM( rects[i].x + rects[i].w, rects[i].y + rects[i].h, p2 );

                                   if (p1.x > p2.x)
                                        D_UTIL_SWAP( p1.x, p2.x );

                                   if (p1.y > p2.y)
                                        D_UTIL_SWAP( p1.y, p2.y );

                                   newrects[i].x = p1.x;
                                   newrects[i].y = p1.y;
                                   newrects[i].w = p2.x - p1.x;
                                   newrects[i].h = p2.y - p1.y;
                              }

                              return new Rectangles( newrects, num_rects, DFXL_FILLRECTANGLE, clipped, true );
                         }
                         break;

                    case DFXL_FILLQUADRANGLE:
                         {
                              DFBPoint *points = new DFBPoint[num_rects * 4];

                              if (matrix) {
                                   for (unsigned int i=0, n=0; i<num_rects; i++, n+=4) {
                                        TRANSFORM( rects[i].x,              rects[i].y,              points[n+0] );
                                        TRANSFORM( rects[i].x + rects[i].w, rects[i].y,              points[n+1] );
                                        TRANSFORM( rects[i].x + rects[i].w, rects[i].y + rects[i].h, points[n+2] );
                                        TRANSFORM( rects[i].x,              rects[i].y + rects[i].h, points[n+3] );
                                   }
                              }
                              else {
                                   for (unsigned int i=0, n=0; i<num_rects; i++, n+=4) {
                                        points[n+0].x = rects[i].x;              points[n+0].y = rects[i].y;
                                        points[n+1].x = rects[i].x + rects[i].w; points[n+1].y = rects[i].y;
                                        points[n+2].x = rects[i].x + rects[i].w; points[n+2].y = rects[i].y + rects[i].h;
                                        points[n+3].x = rects[i].x;              points[n+3].y = rects[i].y + rects[i].h;
                                   }
                              }

                              return new Quadrangles( points, num_rects, DFXL_FILLQUADRANGLE, clipped, true );
                         }
                         break;

                    default:
                         D_UNIMPLEMENTED();
               }
               break;

          case DFXL_DRAWRECTANGLE:
               switch (accel) {
                    case DFXL_DRAWLINE:
                         {
                              DFBRegion *lines = new DFBRegion[num_rects * 4];

                              if (matrix) {
                                   for (unsigned int i=0, n=0; i<num_rects; i++, n+=4) {
                                        TRANSFORM_XY( rects[i].x,              rects[i].y,              lines[n+0].x1, lines[n+0].y1 );
                                        TRANSFORM_XY( rects[i].x + rects[i].w, rects[i].y,              lines[n+0].x2, lines[n+0].y2 );

                                        lines[n+1].x1 = lines[n+0].x2;
                                        lines[n+1].y1 = lines[n+0].y2;
                                        TRANSFORM_XY( rects[i].x + rects[i].w, rects[i].y + rects[i].h, lines[n+1].x2, lines[n+1].y2 );

                                        lines[n+2].x1 = lines[n+1].x2;
                                        lines[n+2].y1 = lines[n+1].y2;
                                        TRANSFORM_XY( rects[i].x,              rects[i].y + rects[i].h, lines[n+2].x2, lines[n+2].y2 );

                                        lines[n+3].x1 = lines[n+2].x2;
                                        lines[n+3].y1 = lines[n+2].y2;
                                        lines[n+3].x2 = lines[n+0].x1;
                                        lines[n+3].y2 = lines[n+0].y1;
                                   }
                              }
                              else {
                                   for (unsigned int i=0, n=0; i<num_rects; i++, n+=4) {
                                        lines[n+0].x1 = rects[i].x;
                                        lines[n+0].y1 = rects[i].y;
                                        lines[n+0].x2 = rects[i].x + rects[i].w;
                                        lines[n+0].y2 = rects[i].y;

                                        lines[n+1].x1 = lines[n+0].x2;
                                        lines[n+1].y1 = lines[n+0].y2;
                                        lines[n+1].x2 = rects[i].x + rects[i].w;
                                        lines[n+1].y2 = rects[i].y + rects[i].h;

                                        lines[n+2].x1 = lines[n+1].x2;
                                        lines[n+2].y1 = lines[n+1].y2;
                                        lines[n+2].x2 = rects[i].x;
                                        lines[n+2].y2 = rects[i].y + rects[i].h;

                                        lines[n+3].x1 = lines[n+2].x2;
                                        lines[n+3].y1 = lines[n+2].y2;
                                        lines[n+3].x2 = lines[n+0].x1;
                                        lines[n+3].y2 = lines[n+0].y1;
                                   }
                              }

                              return new Lines( lines, num_rects * 4, DFXL_DRAWLINE, clipped, true );
                         }
                         break;

                    case DFXL_FILLRECTANGLE:
                         {
                              DFBRectangle *newrects = new DFBRectangle[num_rects * 4];
                              unsigned int  num      = 0;

                              for (unsigned int i=0; i<num_rects; i++) {
                                   newrects[num].x = rects[i].x;
                                   newrects[num].y = rects[i].y;
                                   newrects[num].w = rects[i].w;
                                   newrects[num].h = 1;

                                   num++;


                                   if (rects[i].h > 1) {
                                        newrects[num].x = rects[i].x;
                                        newrects[num].y = rects[i].y + rects[i].h - 1;
                                        newrects[num].w = rects[i].w;
                                        newrects[num].h = 1;

                                        num++;


                                        if (rects[i].h > 2) {
                                             newrects[num].x = rects[i].x;
                                             newrects[num].y = rects[i].y + 1;
                                             newrects[num].w = 1;
                                             newrects[num].h = rects[i].h - 2;

                                             num++;


                                             if (rects[i].w > 1) {
                                                  newrects[num].x = rects[i].x + rects[i].w - 1;
                                                  newrects[num].y = rects[i].y + 1;
                                                  newrects[num].w = 1;
                                                  newrects[num].h = rects[i].h - 2;

                                                  num++;
                                             }
                                        }
                                   }
                              }

                              if (matrix) {
                                   for (unsigned int i=0; i<num; i++) {
                                        DFBPoint p1, p2;

                                        TRANSFORM( newrects[i].x,                 newrects[i].y,                 p1 );
                                        TRANSFORM( newrects[i].x + newrects[i].w, newrects[i].y + newrects[i].h, p2 );

                                        if (p1.x > p2.x)
                                             D_UTIL_SWAP( p1.x, p2.x );

                                        if (p1.y > p2.y)
                                             D_UTIL_SWAP( p1.y, p2.y );

                                        newrects[i].x = p1.x;
                                        newrects[i].y = p1.y;
                                        newrects[i].w = p2.x - p1.x;
                                        newrects[i].h = p2.y - p1.y;
                                   }
                              }

                              return new Rectangles( newrects, num_rects * 4, DFXL_FILLRECTANGLE, clipped, true );
                         }
                         break;

                    default:
                         D_UNIMPLEMENTED();
               }
               break;

          default:
               D_BUG( "unexpected accel 0x%08x", this->accel );
     }

     return NULL;
}

void
Rectangles::render( Renderer::Setup *setup,
                    Engine          *engine )
{
     switch (this->accel) {
          case DFXL_FILLRECTANGLE:
               /// loop
               for (unsigned int i=0; i<setup->tiles_render; i++) {
                    if (!(setup->task_mask & (1 << i)))
                         continue;

                    if (engine->caps.clipping & DFXL_FILLRECTANGLE) {
                         engine->FillRectangles( setup->tasks[i], rects, num_rects );
                    }
                    else {
                         Util::TempArray<DFBRectangle> copied_rects( num_rects );
                         unsigned int                  copied_num = 0;

                         for (unsigned int n=0; n<num_rects; n++) {
                              copied_rects.array[copied_num] = rects[n];

                              if (dfb_clip_rectangle( &setup->clips_clipped[i], &copied_rects.array[copied_num] ))
                                   copied_num++;
                         }

                         if (copied_num)
                              engine->FillRectangles( setup->tasks[i], copied_rects.array, copied_num );
                    }
               }
               break;

          case DFXL_DRAWRECTANGLE:
               /// loop
               for (unsigned int i=0; i<setup->tiles_render; i++) {
                    if (!(setup->task_mask & (1 << i)))
                         continue;

                    if (engine->caps.clipping & DFXL_DRAWRECTANGLE) {
                         engine->DrawRectangles( setup->tasks[i], rects, num_rects );
                    }
                    else {
                         Util::TempArray<DFBRectangle> copied_rects( num_rects * 4 );
                         unsigned int                  copied_num = 0;

                         for (unsigned int n=0; n<num_rects; n++) {
                              if (dfb_rectangle_region_intersects( &rects[n], &setup->clips_clipped[i] )) {
                                   DFBRectangle out_rects[4];
                                   int          out_num = 0;

                                   dfb_build_clipped_rectangle_outlines( &rects[n], &setup->clips_clipped[i], out_rects, &out_num );

                                   for (int j=0; j<out_num; j++)
                                        copied_rects.array[copied_num++] = out_rects[j];
                              }
                         }

                         if (copied_num)
                              engine->FillRectangles( setup->tasks[i], copied_rects.array, copied_num );
                    }
               }
               break;

          default:
               D_BUG( "unexpected accel 0x%08x", this->accel );
     }
}


Base *
Blits::tesselate( DFBAccelerationMask  accel,
                  const s32           *matrix )
{
     switch (this->accel) {
          case DFXL_BLIT:
               switch (accel) {
                    case DFXL_BLIT:
                         /* blit to blit used for transformation only */
                         D_ASSUME( matrix != NULL );
                         if (!matrix)
                              return NULL;

                         {
                              DFBRectangle *newrects  = new DFBRectangle[num_rects];
                              DFBPoint     *newpoints = new DFBPoint[num_rects];

                              for (unsigned int i=0; i<num_rects; i++) {
                                   DFBPoint p1, p2;

                                   TRANSFORM( points[i].x,              points[i].y,              p1 );
                                   TRANSFORM( points[i].x + rects[i].w, points[i].y + rects[i].h, p2 );

                                   if (p1.x > p2.x)
                                        D_UTIL_SWAP( p1.x, p2.x );

                                   if (p1.y > p2.y)
                                        D_UTIL_SWAP( p1.y, p2.y );

                                   newrects[i].x = rects[i].x;
                                   newrects[i].y = rects[i].y;
                                   newrects[i].w = p2.x - p1.x;
                                   newrects[i].h = p2.y - p1.y;

                                   D_ASSERT( newrects[i].w == rects[i].w );
                                   D_ASSERT( newrects[i].h == rects[i].h );

                                   newpoints[i] = p1;
                              }

                              return new Blits( newrects, newpoints, num_rects, DFXL_BLIT, clipped, true );
                         }
                         break;

                    case DFXL_STRETCHBLIT:
                         break;

                    case DFXL_TEXTRIANGLES:
                         /* blit to tex triangles used for transformation only */
                         D_ASSUME( matrix != NULL );
                         if (!matrix)
                              return NULL;

                         {
                              DFBVertex1616 *vertices = new DFBVertex1616[num_rects * 6];

                              for (unsigned int i=0, n=0; i<num_rects; i++, n+=6) {
                                   DFBPoint p1, p2, p3, p4;
                                   int      x1, y1, x2, y2;

                                   x1 = points[i].x;
                                   y1 = points[i].y;
                                   x2 = points[i].x + rects[i].w;
                                   y2 = points[i].y + rects[i].h;

                                   TRANSFORM( x1, y1, p1 );
                                   TRANSFORM( x2, y1, p2 );
                                   TRANSFORM( x2, y2, p3 );
                                   TRANSFORM( x1, y2, p4 );

                                   vertices[n+0].x = p1.x << 16;
                                   vertices[n+0].y = p1.y << 16;
                                   vertices[n+0].z = 0;
                                   vertices[n+0].w = 0x10000;
                                   vertices[n+0].s = rects[i].x << 16;
                                   vertices[n+0].t = rects[i].y << 16;

                                   vertices[n+1].x = p2.x << 16;
                                   vertices[n+1].y = p2.y << 16;
                                   vertices[n+1].z = 0;
                                   vertices[n+1].w = 0x10000;
                                   vertices[n+1].s = (rects[i].x + rects[i].w - 1) << 16;
                                   vertices[n+1].t = rects[i].y << 16;

                                   vertices[n+2].x = p3.x << 16;
                                   vertices[n+2].y = p3.y << 16;
                                   vertices[n+2].z = 0;
                                   vertices[n+2].w = 0x10000;
                                   vertices[n+2].s = (rects[i].x + rects[i].w - 1) << 16;
                                   vertices[n+2].t = (rects[i].y + rects[i].h - 1) << 16;

                                   vertices[n+3].x = p1.x << 16;
                                   vertices[n+3].y = p1.y << 16;
                                   vertices[n+3].z = 0;
                                   vertices[n+3].w = 0x10000;
                                   vertices[n+3].s = rects[i].x << 16;
                                   vertices[n+3].t = rects[i].y << 16;

                                   vertices[n+4].x = p3.x << 16;
                                   vertices[n+4].y = p3.y << 16;
                                   vertices[n+4].z = 0;
                                   vertices[n+4].w = 0x10000;
                                   vertices[n+4].s = (rects[i].x + rects[i].w - 1) << 16;
                                   vertices[n+4].t = (rects[i].y + rects[i].h - 1) << 16;

                                   vertices[n+5].x = p4.x << 16;
                                   vertices[n+5].y = p4.y << 16;
                                   vertices[n+5].z = 0;
                                   vertices[n+5].w = 0x10000;
                                   vertices[n+5].s = rects[i].x << 16;
                                   vertices[n+5].t = (rects[i].y + rects[i].h - 1) << 16;
                              }

                              return new TexTriangles1616( vertices, num_rects * 6, DTTF_LIST, DFXL_TEXTRIANGLES, clipped, true );
                         }
                         break;

                    default:
                         D_UNIMPLEMENTED();
               }
               break;

          default:
               D_BUG( "unexpected accel 0x%08x", this->accel );
     }

     return NULL;
}

void
Blits::render( Renderer::Setup *setup,
               Engine          *engine )
{
     /// loop
     for (unsigned int i=0; i<setup->tiles_render; i++) {
          if (!(setup->task_mask & (1 << i)))
               continue;

          if (engine->caps.clipping & DFXL_BLIT) {
               engine->Blit( setup->tasks[i], rects, points, num_rects );
          }
          else {
               Util::TempArray<DFBRectangle> copied_rects( num_rects );
               Util::TempArray<DFBPoint>     copied_points( num_rects );
               unsigned int                  copied_num = 0;

               for (unsigned int n=0; n<num_rects; n++) {
                    if (dfb_clip_blit_precheck( &setup->clips_clipped[i],
                                                rects[n].w, rects[n].h,
                                                points[n].x, points[n].y ))
                    {
                         copied_rects.array[copied_num]  = rects[n];
                         copied_points.array[copied_num] = points[n];

                         dfb_clip_blit( &setup->clips_clipped[i], &copied_rects.array[copied_num],
                                        &copied_points.array[copied_num].x, &copied_points.array[copied_num].y );

                         copied_num++;
                    }
               }

               if (copied_num)
                    engine->Blit( setup->tasks[i], copied_rects.array, copied_points.array, copied_num );
          }
     }
}


Base *
StretchBlits::tesselate( DFBAccelerationMask  accel,
                         const s32           *matrix )
{
     switch (this->accel) {
          case DFXL_STRETCHBLIT:
               switch (accel) {
                    case DFXL_STRETCHBLIT:
                         /* stretch blit to stretch blit used for transformation only */
                         D_ASSUME( matrix != NULL );
                         if (!matrix)
                              return NULL;

                         {
                              DFBRectangle *newsrects = new DFBRectangle[num_rects];
                              DFBRectangle *newdrects = new DFBRectangle[num_rects];

                              // TODO: can be optimised for translate only case
                              for (unsigned int i=0; i<num_rects; i++) {
                                   DFBPoint p1, p2;

                                   TRANSFORM( drects[i].x,               drects[i].y,               p1 );
                                   TRANSFORM( drects[i].x + drects[i].w, drects[i].y + drects[i].h, p2 );

                                   if (p1.x > p2.x)
                                        D_UTIL_SWAP( p1.x, p2.x );

                                   if (p1.y > p2.y)
                                        D_UTIL_SWAP( p1.y, p2.y );

                                   newdrects[i].x = p1.x;
                                   newdrects[i].y = p1.y;
                                   newdrects[i].w = p2.x - p1.x;
                                   newdrects[i].h = p2.y - p1.y;

                                   newsrects[i] = srects[i];
                              }

                              return new StretchBlits( newsrects, newdrects, num_rects, DFXL_STRETCHBLIT, clipped, true );
                         }
                         break;

                    case DFXL_TEXTRIANGLES:
                         /* blit to tex triangles used for transformation only */
                         D_ASSUME( matrix != NULL );
                         if (!matrix)
                              return NULL;

                         {
                              DFBVertex1616 *vertices = new DFBVertex1616[num_rects * 6];

                              for (unsigned int i=0, n=0; i<num_rects; i++, n+=6) {
                                   DFBPoint p1, p2, p3, p4;
                                   int      x1, y1, x2, y2;

                                   x1 = drects[i].x;
                                   y1 = drects[i].y;
                                   x2 = drects[i].x + drects[i].w;
                                   y2 = drects[i].y + drects[i].h;

                                   TRANSFORM( x1, y1, p1 );
                                   TRANSFORM( x2, y1, p2 );
                                   TRANSFORM( x2, y2, p3 );
                                   TRANSFORM( x1, y2, p4 );

                                   vertices[n+0].x = p1.x << 16;
                                   vertices[n+0].y = p1.y << 16;
                                   vertices[n+0].z = 0;
                                   vertices[n+0].w = 0x10000;
                                   vertices[n+0].s = srects[i].x << 16;
                                   vertices[n+0].t = srects[i].y << 16;

                                   vertices[n+1].x = p2.x << 16;
                                   vertices[n+1].y = p2.y << 16;
                                   vertices[n+1].z = 0;
                                   vertices[n+1].w = 0x10000;
                                   vertices[n+1].s = (srects[i].x + srects[i].w - 1) << 16;
                                   vertices[n+1].t = srects[i].y << 16;

                                   vertices[n+2].x = p3.x << 16;
                                   vertices[n+2].y = p3.y << 16;
                                   vertices[n+2].z = 0;
                                   vertices[n+2].w = 0x10000;
                                   vertices[n+2].s = (srects[i].x + srects[i].w - 1) << 16;
                                   vertices[n+2].t = (srects[i].y + srects[i].h - 1) << 16;

                                   vertices[n+3].x = p1.x << 16;
                                   vertices[n+3].y = p1.y << 16;
                                   vertices[n+3].z = 0;
                                   vertices[n+3].w = 0x10000;
                                   vertices[n+3].s = srects[i].x << 16;
                                   vertices[n+3].t = srects[i].y << 16;

                                   vertices[n+4].x = p3.x << 16;
                                   vertices[n+4].y = p3.y << 16;
                                   vertices[n+4].z = 0;
                                   vertices[n+4].w = 0x10000;
                                   vertices[n+4].s = (srects[i].x + srects[i].w - 1) << 16;
                                   vertices[n+4].t = (srects[i].y + srects[i].h - 1) << 16;

                                   vertices[n+5].x = p4.x << 16;
                                   vertices[n+5].y = p4.y << 16;
                                   vertices[n+5].z = 0;
                                   vertices[n+5].w = 0x10000;
                                   vertices[n+5].s = srects[i].x << 16;
                                   vertices[n+5].t = (srects[i].y + srects[i].h - 1) << 16;
                              }

                              return new TexTriangles1616( vertices, num_rects * 6, DTTF_LIST, DFXL_TEXTRIANGLES, clipped, true );
                         }
                         break;

                    default:
                         D_UNIMPLEMENTED();
               }
               break;

          default:
               D_BUG( "unexpected accel 0x%08x", this->accel );
     }

     return NULL;
}

void
StretchBlits::render( Renderer::Setup *setup,
                      Engine          *engine )
{
     /// loop
     for (unsigned int i=0; i<setup->tiles_render; i++) {
          if (!(setup->task_mask & (1 << i)))
               continue;

          if (engine->caps.clipping & DFXL_STRETCHBLIT) {
               engine->StretchBlit( setup->tasks[i], srects, drects, num_rects );
          }
          else {
               Util::TempArray<DFBRectangle> copied_srects( num_rects );
               Util::TempArray<DFBRectangle> copied_drects( num_rects );
               unsigned int                  copied_num = 0;

               for (unsigned int n=0; n<num_rects; n++) {
                    if (dfb_clip_blit_precheck( &setup->clips_clipped[i],
                                                drects[n].w, drects[n].h,
                                                drects[n].x, drects[n].y ))
                    {
                         copied_srects.array[copied_num] = srects[n];
                         copied_drects.array[copied_num] = drects[n];

                         dfb_clip_stretchblit( &setup->clips_clipped[i],
                                               &copied_srects.array[copied_num],
                                               &copied_drects.array[copied_num] );

                         copied_num++;
                    }
               }

               if (copied_num)
                    engine->StretchBlit( setup->tasks[i], copied_srects.array, copied_drects.array, copied_num );
          }
     }
}


Base *
Lines::tesselate( DFBAccelerationMask  accel,
                  const s32           *matrix )
{
     switch (accel) {
          case DFXL_DRAWLINE:
               /* draw line to draw line used for transformation only */
               D_ASSUME( matrix != NULL );
               if (!matrix)
                    return NULL;

               {
                    DFBRegion *newlines = new DFBRegion[num_lines];

                    for (unsigned int i=0; i<num_lines; i++) {
                         TRANSFORM_XY( lines[i].x1, lines[i].y1, newlines[i].x1, newlines[i].y1 );
                         TRANSFORM_XY( lines[i].x2, lines[i].y2, newlines[i].x2, newlines[i].y2 );
                    }

                    return new Lines( newlines, num_lines, DFXL_DRAWLINE, clipped, true );
               }
               break;

          default:
               D_UNIMPLEMENTED();
     }

     return NULL;
}

void
Lines::render( Renderer::Setup *setup,
               Engine          *engine )
{
     /// loop
     for (unsigned int i=0; i<setup->tiles_render; i++) {
          if (!(setup->task_mask & (1 << i)))
               continue;

          if (engine->caps.clipping & DFXL_DRAWLINE) {
               engine->DrawLines( setup->tasks[i], lines, num_lines );
          }
          else {
               Util::TempArray<DFBRegion> copied_lines( num_lines );
               unsigned int               copied_num = 0;

               for (unsigned int n=0; n<num_lines; n++) {
                    copied_lines.array[copied_num] = lines[n];

                    if (dfb_clip_line( &setup->clips_clipped[i], &copied_lines.array[copied_num] ))
                         copied_num++;
               }

               if (copied_num)
                    engine->DrawLines( setup->tasks[i], copied_lines.array, copied_num );
          }
     }
}


Base *
Spans::tesselate( DFBAccelerationMask  accel,
                  const s32           *matrix )
{
     switch (accel) {
          case DFXL_FILLRECTANGLE:
               {
                    DFBRectangle *rects = new DFBRectangle[num_spans];

                    if (matrix) {
                         for (unsigned int i=0; i<num_spans; i++) {
                              DFBPoint p1, p2;

                              TRANSFORM( spans[i].x,              y + i,     p1 );
                              TRANSFORM( spans[i].x + spans[i].w, y + i + 1, p2 );

                              rects[i].x = p1.x;
                              rects[i].y = p1.y;
                              rects[i].w = p2.x - p1.x;
                              rects[i].h = p2.y - p1.y;
                         }
                    }
                    else {
                         for (unsigned int i=0; i<num_spans; i++) {
                              rects[i].x = spans[i].x;
                              rects[i].y = y + i;
                              rects[i].w = spans[i].w;
                              rects[i].h = 1;
                         }
                    }

                    return new Rectangles( rects, num_spans, DFXL_FILLRECTANGLE, clipped, true );
               }
               break;

          case DFXL_DRAWLINE:
               {
                    DFBRegion *lines = new DFBRegion[num_spans];

                    if (matrix) {
                         for (unsigned int i=0; i<num_spans; i++) {
                              DFBPoint p1, p2;

                              TRANSFORM( spans[i].x,                  y + i, p1 );
                              TRANSFORM( spans[i].x + spans[i].w - 1, y + i, p2 );

                              lines[i].x1 = p1.x;
                              lines[i].y1 = p1.y;
                              lines[i].x2 = p2.x;
                              lines[i].y2 = p2.y;
                         }
                    }
                    else {
                         for (unsigned int i=0; i<num_spans; i++) {
                              lines[i].x1 = spans[i].x;
                              lines[i].y1 = y + i;
                              lines[i].x2 = spans[i].x + spans[i].w - 1;
                              lines[i].y2 = y + i;
                         }
                    }

                    return new Lines( lines, num_spans, DFXL_DRAWLINE, clipped, true );
               }
               break;

          case DFXL_FILLTRIANGLE:
               {
                    DFBTriangle *tris = new DFBTriangle[num_spans*2];

                    if (matrix) {
                         for (unsigned int i=0, n=0; i<num_spans; i++, n+=2) {
                              DFBPoint p1, p2;

                              TRANSFORM( spans[i].x,              y + i,     p1 );
                              TRANSFORM( spans[i].x + spans[i].w, y + i + 1, p2 );

                              tris[n+0].x1 = p1.x;
                              tris[n+0].y1 = p1.y;
                              tris[n+0].x2 = p2.x;
                              tris[n+0].y2 = p1.y;
                              tris[n+0].x3 = p2.x;
                              tris[n+0].y3 = p2.y;

                              tris[n+1].x1 = p1.x;
                              tris[n+1].y1 = p1.y;
                              tris[n+1].x2 = p2.x;
                              tris[n+1].y2 = p2.y;
                              tris[n+1].x3 = p1.x;
                              tris[n+1].y3 = p2.y;
                         }

                         return new Triangles( tris, num_spans*2, DFXL_FILLTRIANGLE, clipped, true );
                    }
                    else
                         D_UNIMPLEMENTED();
               }
               break;

          default:
               D_UNIMPLEMENTED();
     }

     return NULL;
}

void
Spans::render( Renderer::Setup *setup,
               Engine          *engine )
{
     /// loop
     for (unsigned int i=0; i<setup->tiles_render; i++) {
          if (!(setup->task_mask & (1 << i)))
               continue;

          if (engine->caps.clipping & DFXL_FILLSPAN) {
               engine->FillSpans( setup->tasks[i], y, spans, num_spans );
          }
          else {
               D_UNIMPLEMENTED();
          }
     }
}


typedef struct {
   int xi;
   int xf;
   int mi;
   int mf;
   int _2dy;
} DDA;

#define SETUP_DDA(xs,ys,xe,ye,dda)         \
     do {                                  \
          int dx = (xe) - (xs);            \
          int dy = (ye) - (ys);            \
          dda.xi = (xs);                   \
          if (dy != 0) {                   \
               dda.mi = dx / dy;           \
               dda.mf = 2*(dx % dy);       \
               dda.xf = -dy;               \
               dda._2dy = 2 * dy;          \
               if (dda.mf < 0) {           \
                    dda.mf += 2 * ABS(dy); \
                    dda.mi--;              \
               }                           \
          }                                \
          else {                           \
               dda.mi = 0;                 \
               dda.mf = 0;                 \
               dda.xf = 0;                 \
               dda._2dy = 0;               \
          }                                \
     } while (0)


#define INC_DDA(dda)                       \
     do {                                  \
          dda.xi += dda.mi;                \
          dda.xf += dda.mf;                \
          if (dda.xf > 0) {                \
               dda.xi++;                   \
               dda.xf -= dda._2dy;         \
          }                                \
     } while (0)

Base *
Triangles::tesselate( DFBAccelerationMask  accel,
                      const s32           *matrix )
{
     switch (accel) {
          case DFXL_FILLRECTANGLE:
               {
                    unsigned int                 lines = 0;
                    Util::TempArray<DFBTriangle> sorted( num_tris );

                    for (unsigned int i=0; i<num_tris; i++) {
                         if (matrix) {
                              TRANSFORM_XY( tris[i].x1, tris[i].y1, sorted.array[i].x1, sorted.array[i].y1 );
                              TRANSFORM_XY( tris[i].x2, tris[i].y2, sorted.array[i].x2, sorted.array[i].y2 );
                              TRANSFORM_XY( tris[i].x3, tris[i].y3, sorted.array[i].x3, sorted.array[i].y3 );
                         }
                         else
                              sorted.array[i] = tris[i];

                         dfb_sort_triangle( &sorted.array[i] );

                         lines += sorted.array[i].y3 - sorted.array[i].y1 + 1;
                    }


                    DFBRectangle *rects = new DFBRectangle[lines];
                    unsigned int  num   = 0;

                    for (unsigned int i=0; i<num_tris; i++) {
                         int                y, yend;
                         DDA                dda1, dda2;
                         const DFBTriangle *tri  = &sorted.array[i];

                         dda1.xi = 0;
                         dda2.xi = 0;

                         y = tri->y1;
                         yend = tri->y3;

                         SETUP_DDA(tri->x1, tri->y1, tri->x3, tri->y3, dda1);
                         SETUP_DDA(tri->x1, tri->y1, tri->x2, tri->y2, dda2);

                         while (y <= yend) {
                              DFBRectangle rect;

                              if (y == tri->y2) {
                                   if (tri->y2 == tri->y3)
                                        break;
                                   SETUP_DDA(tri->x2, tri->y2, tri->x3, tri->y3, dda2);
                              }

                              rect.w = ABS(dda1.xi - dda2.xi);
                              rect.x = MIN(dda1.xi, dda2.xi);

                              if (rect.w > 0) {
                                   rect.y = y;
                                   rect.h = 1;

                                   if (rect.w > 0)
                                        rects[num++] = rect;
                              }

                              INC_DDA(dda1);
                              INC_DDA(dda2);

                              y++;
                         }
                    }

                    return new Rectangles( rects, num, DFXL_FILLRECTANGLE, clipped, true );
               }
               break;

          default:
               D_UNIMPLEMENTED();
     }

     return NULL;
}

void
Triangles::render( Renderer::Setup *setup,
                   Engine          *engine )
{
     /// loop
     for (unsigned int i=0; i<setup->tiles_render; i++) {
          if (!(setup->task_mask & (1 << i)))
               continue;

          if (engine->caps.clipping & DFXL_FILLTRIANGLE) {
               engine->FillTriangles( setup->tasks[i], tris, num_tris );
          }
          else {
               D_UNIMPLEMENTED();
          }
     }
}


Base *
Trapezoids::tesselate( DFBAccelerationMask  accel,
                       const s32           *matrix )
{
     switch (accel) {
          case DFXL_FILLRECTANGLE:
               if (matrix) {
                    D_UNIMPLEMENTED();
               }
               else {
                    unsigned int lines = 0;

                    for (unsigned int i=0; i<num_traps; i++) {
                         lines += ABS(traps[i].y2 - traps[i].y1) + 1;
                    }


                    DFBRectangle *rects = new DFBRectangle[lines];
                    unsigned int  num   = 0;

                    for (unsigned int i=0; i<num_traps; i++) {
                         int          y, yend;
                         DDA          dda1, dda2;
                         DFBTrapezoid trap = traps[i];

                         dda1.xi = 0;
                         dda2.xi = 0;

                         if (trap.y1 > trap.y2) {
                              D_UTIL_SWAP( trap.x1, trap.x2 );
                              D_UTIL_SWAP( trap.y1, trap.y2 );
                              D_UTIL_SWAP( trap.w1, trap.w2 );
                         }

                         y    = trap.y1;
                         yend = trap.y2;

                         SETUP_DDA(trap.x1,           trap.y1, trap.x2,           trap.y2, dda1);
                         SETUP_DDA(trap.x1 + trap.w1, trap.y1, trap.x2 + trap.w2, trap.y2, dda2);

                         while (y <= yend) {
                              DFBRectangle rect;

                              rect.w = dda2.xi - dda1.xi;

                              if (rect.w > 0) {
                                   rect.x = dda1.xi;
                                   rect.y = y;
                                   rect.h = 1;

                                   rects[num++] = rect;
                              }

                              INC_DDA(dda1);
                              INC_DDA(dda2);

                              y++;
                         }
                    }

                    return new Rectangles( rects, num, DFXL_FILLRECTANGLE, clipped, true );
               }
               break;

          case DFXL_FILLTRIANGLE:
               if (matrix) {
                    DFBTriangle *tris = new DFBTriangle[num_traps * 2];

                    for (unsigned int i=0, n=0; i<num_traps; i++, n+=2) {
                         DFBPoint     p1, p2, p3, p4;
                         DFBTrapezoid trap = traps[i];

                         TRANSFORM( trap.x1,           trap.y1, p1 );
                         TRANSFORM( trap.x1 + trap.w1, trap.y1, p2 );
                         TRANSFORM( trap.x2 + trap.w2, trap.y2, p3 );
                         TRANSFORM( trap.x2,           trap.y2, p4 );

                         tris[n+0].x1 = p1.x;
                         tris[n+0].y1 = p1.y;
                         tris[n+0].x2 = p2.x;
                         tris[n+0].y2 = p2.y;
                         tris[n+0].x3 = p3.x;
                         tris[n+0].y3 = p3.y;

                         tris[n+1].x1 = p1.x;
                         tris[n+1].y1 = p1.y;
                         tris[n+1].x2 = p3.x;
                         tris[n+1].y2 = p3.y;
                         tris[n+1].x3 = p4.x;
                         tris[n+1].y3 = p4.y;
                    }

                    return new Triangles( tris, num_traps * 2, DFXL_FILLTRIANGLE, clipped, true );
               }
               else {
                    D_UNIMPLEMENTED();
               }
               break;

          default:
               D_UNIMPLEMENTED();
     }

     return NULL;
}

void
Trapezoids::render( Renderer::Setup *setup,
                    Engine          *engine )
{
     /// loop
     for (unsigned int i=0; i<setup->tiles_render; i++) {
          if (!(setup->task_mask & (1 << i)))
               continue;

          if (engine->caps.clipping & DFXL_FILLTRAPEZOID) {
               engine->FillTrapezoids( setup->tasks[i], traps, num_traps );
          }
          else {
               D_UNIMPLEMENTED();
          }
     }
}


Base *
TexTriangles::tesselate( DFBAccelerationMask  accel,
                         const s32           *matrix )
{
     switch (accel) {
          default:
               D_UNIMPLEMENTED();
     }

     return NULL;
}

void
TexTriangles::render( Renderer::Setup *setup,
                      Engine          *engine )
{
     /// loop
     for (unsigned int i=0; i<setup->tiles_render; i++) {
          if (!(setup->task_mask & (1 << i)))
               continue;

          if (engine->caps.clipping & DFXL_TEXTRIANGLES) {
               engine->TextureTriangles( setup->tasks[i], vertices, num, formation );
          }
          else {
               D_UNIMPLEMENTED();
          }
     }
}


Base *
TexTriangles1616::tesselate( DFBAccelerationMask  accel,
                             const s32           *matrix )
{
     switch (accel) {
          default:
               D_UNIMPLEMENTED();
     }

     return NULL;
}

void
TexTriangles1616::render( Renderer::Setup *setup,
                          Engine          *engine )
{
     /// loop
     for (unsigned int i=0; i<setup->tiles_render; i++) {
          if (!(setup->task_mask & (1 << i)))
               continue;

          if (engine->caps.clipping & DFXL_TEXTRIANGLES) {
               engine->TextureTriangles( setup->tasks[i], vertices, num, formation );
          }
          else {
               D_UNIMPLEMENTED();
          }
     }
}


Base *
Quadrangles::tesselate( DFBAccelerationMask  accel,
                        const s32           *matrix )
{
     switch (accel) {
          case DFXL_FILLTRIANGLE:
               {
                    DFBTriangle *tris = new DFBTriangle[num_quads * 2];

                    for (unsigned int i=0, n=0; i<num_quads*4; i+=4, n+=2) {
                         tris[n+0].x1 = points[i+0].x;
                         tris[n+0].y1 = points[i+0].y;

                         tris[n+0].x2 = points[i+1].x;
                         tris[n+0].y2 = points[i+1].y;

                         tris[n+0].x3 = points[i+2].x;
                         tris[n+0].y3 = points[i+2].y;


                         tris[n+1].x1 = points[i+0].x;
                         tris[n+1].y1 = points[i+0].y;

                         tris[n+1].x2 = points[i+2].x;
                         tris[n+1].y2 = points[i+2].y;

                         tris[n+1].x3 = points[i+3].x;
                         tris[n+1].y3 = points[i+3].y;
                    }

                    return new Triangles( tris, num_quads * 2, DFXL_FILLTRIANGLE, clipped, true );
               }
               break;

          default:
               D_UNIMPLEMENTED();
     }

     return NULL;
}

void
Quadrangles::render( Renderer::Setup *setup,
                     Engine          *engine )
{
     /// loop
     for (unsigned int i=0; i<setup->tiles_render; i++) {
          if (!(setup->task_mask & (1 << i)))
               continue;

          if (engine->caps.clipping & DFXL_FILLQUADRANGLE) {
               engine->FillQuadrangles( setup->tasks[i], points, num_quads );
          }
          else {
               D_UNIMPLEMENTED();
          }
     }
}


}





Renderer::Renderer( CardState *state )
     :
     state( state ),
     engine( NULL ),
     setup( NULL ),
     operations( 0 )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

}

Renderer::~Renderer()
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     Flush();
}


void
Renderer::Flush()
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     if (engine) {
          unbindEngine();

          RendererTLS *tls = Renderer_GetTLS();

          if (tls->last_renderer == this)
               tls->last_renderer = NULL;
     }
}

void
Renderer::FlushCurrent()
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     RendererTLS *tls = Renderer_GetTLS();

     if (tls->last_renderer)
          tls->last_renderer->Flush();
}


DFBResult
Renderer::enterLock( CoreSurfaceBufferLock  *lock,
                     CoreSurfaceAllocation  *allocation,
                     CoreSurfaceAccessFlags  flags )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( flags 0x%02x )\n", __FUNCTION__, flags );

     /*
        FIXME: move to engine / task
     */
     dfb_surface_buffer_lock_init( lock, setup->tasks[0]->accessor, flags );
     dfb_surface_pool_lock( allocation->pool, allocation, lock );

     D_DEBUG_AT( DirectFB_Renderer, "  -> %p (%d)\n", lock->addr, lock->pitch );

     return DFB_OK;
}

DFBResult
Renderer::leaveLock( CoreSurfaceBufferLock *lock )
{
     /*
        FIXME: move to engine / task
     */
     if (lock->buffer) {
          D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

          dfb_surface_pool_unlock( lock->allocation->pool, lock->allocation, lock );
          dfb_surface_buffer_lock_deinit( lock );
     }

     return DFB_OK;
}

DFBResult
Renderer::updateLock( CoreSurfaceBufferLock  *lock,
                      CoreSurface            *surface,
                      CoreSurfaceBufferRole   role,
                      DFBSurfaceStereoEye     eye,
                      u32                     flips,
                      CoreSurfaceAccessFlags  flags )
{
     DFBResult              ret;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;
     SurfaceAllocationMap::iterator it;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );


     // FIXME: don't need to unlock/lock every time in most cases, move to engine

     leaveLock( lock );


     SurfaceAllocationKey key( surface->object.id, role, eye, flips );

     it = allocations.find( key );
     if (it != allocations.end()) {
          allocation = (*it).second;

     }
     else {
          dfb_surface_lock( surface );

          // FIXME: move to helper class, e.g. to SurfaceTask
          //

          buffer = dfb_surface_get_buffer3( surface, role, eye, flips );

          allocation = dfb_surface_buffer_find_allocation( buffer, setup->tasks[0]->accessor, flags, true );
          if (!allocation) {
               /* If no allocation exists, create one. */
               ret = dfb_surface_pools_allocate( buffer, setup->tasks[0]->accessor, flags, &allocation );
               if (ret) {
                    D_DERROR( ret, "DirectFB/Renderer: Buffer allocation failed!\n" );
                    dfb_surface_unlock( surface );
                    return ret;
               }
          }

          dfb_surface_allocation_update( allocation, flags );

          setup->tasks[0]->AddAccess( allocation, flags );

          dfb_surface_unlock( surface );

#if 1
//          long long t1 = direct_clock_get_abs_millis();
          // FIXME: this is a temporary solution, slaves will be blocked via kernel module later
//          printf("count %d\n", allocation->task_count);
          unsigned int timeout = 5000;
          while (allocation->task_count > 5) {
               if (!--timeout) {
                    D_ERROR( "DirectFB/Renderer: timeout!\n" );
                    direct_trace_print_stacks();
                    TaskManager::dumpTasks();
                    break;
               }
               //printf("blocked\n");
               usleep( 1000 );
          }
//          long long t2 = direct_clock_get_abs_millis();
//          D_INFO("blocked %lld\n", t2 - t1);
#endif

          allocations.insert( SurfaceAllocationMapPair( key, allocation ) );
     }


     enterLock( lock, allocation, flags );

     return DFB_OK;
}

void
Renderer::update( DFBAccelerationMask accel )
{
     CoreSurfaceAccessFlags access = CSAF_WRITE;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( accel 0x%08x )\n", __FUNCTION__, accel );
     D_DEBUG_AT( DirectFB_Renderer, "  -> modified  0x%08x\n", state->modified );
     D_DEBUG_AT( DirectFB_Renderer, "  -> mod_hw    0x%08x\n", state->mod_hw );
     D_DEBUG_AT( DirectFB_Renderer, "  -> state_mod 0x%08x\n", state_mod );

     /* find locking flags */
     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                      DSBLIT_BLEND_COLORALPHA   |
                                      DSBLIT_DST_COLORKEY))
               access = (CoreSurfaceAccessFlags)(access | CSAF_READ);
     }
     else if (state->drawingflags & (DSDRAW_BLEND | DSDRAW_DST_COLORKEY))
          access = (CoreSurfaceAccessFlags)(access | CSAF_READ);


     if (state_mod & SMF_DESTINATION) {
          D_ASSERT( state->destination != NULL );

          updateLock( &state->dst, state->destination, state->to, state->to_eye, state->destination->flips,
                      (CoreSurfaceAccessFlags)( CSAF_WRITE | CSAF_READ ) );

          state_mod = (StateModificationFlags)(state_mod & ~SMF_DESTINATION);
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          D_ASSERT( state->source != NULL );

          if (state_mod & SMF_SOURCE) {
               updateLock( &state->src, state->source, state->from, state->from_eye, state->source->flips, CSAF_READ );

               state_mod = (StateModificationFlags)(state_mod & ~SMF_SOURCE);
          }

          /* if using a mask... */
          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               D_ASSERT( state->source_mask != NULL );

               if (state_mod & SMF_SOURCE_MASK) {
                    updateLock( &state->src_mask, state->source_mask, state->from, state->from_eye, state->source_mask->flips, CSAF_READ );

                    state_mod = (StateModificationFlags)(state_mod & ~SMF_SOURCE_MASK);
               }
          }

          /* if using source2... */
          if (accel == DFXL_BLIT2) {
               D_ASSERT( state->source2 != NULL );

               if (state_mod & SMF_SOURCE2) {
                    updateLock( &state->src2, state->source2, state->from, state->from_eye, state->source2->flips, CSAF_READ );

                    state_mod = (StateModificationFlags)(state_mod & ~SMF_SOURCE2);
               }
          }
     }

     if (setup->tiles_render == 1) {
          setup->task_mask = 1;

          if (state_mod & SMF_CLIP) {
               state_mod = (StateModificationFlags)(state_mod & ~SMF_CLIP);
          }

          if (state->mod_hw || !(state->set & accel))
               engine->SetState( setup->tasks[0], state, state->mod_hw, accel );
     }
     else {
          D_ASSERT( setup->tiles == setup->tiles_render );

          if (state_mod & SMF_CLIP) {
               D_ASSERT( setup->tiles <= 32 );

               setup->task_mask = 0;

               for (unsigned int i=0; i<setup->tiles; i++) {
                    setup->clips_clipped[i].x1 = MAX( state->clip.x1, setup->clips[i].x1 );
                    setup->clips_clipped[i].y1 = MAX( state->clip.y1, setup->clips[i].y1 );
                    setup->clips_clipped[i].x2 = MIN( state->clip.x2, setup->clips[i].x2 );
                    setup->clips_clipped[i].y2 = MIN( state->clip.y2, setup->clips[i].y2 );

                    if (setup->clips_clipped[i].x1 <= setup->clips_clipped[i].x2 &&
                        setup->clips_clipped[i].y1 <= setup->clips_clipped[i].y2)
                         setup->task_mask |= (1 << i);
               }

               state_mod = (StateModificationFlags)(state_mod & ~SMF_CLIP);
          }

          if (state->mod_hw || !(state->set & accel)) {
               DFBRegion              clip     = state->clip;
               StateModificationFlags modified = state->mod_hw;

               /// loop, clip switch, task mask (total clip)

               for (unsigned int i=0; i<setup->tiles; i++) {
                    state->clip = setup->clips_clipped[i];

                    engine->SetState( setup->tasks[i], state, modified, accel );
               }

               state->clip = clip;
          }
     }

     D_DEBUG_AT( DirectFB_Renderer, "  -> state_mod 0x%08x\n", state_mod );
}

void
Renderer::render( Primitives::Base *primitives )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( '%s' ) <- modified 0x%08x\n",
                 __FUNCTION__, Util::DFBAccelerationMask_Name(primitives->accel).c_str(), state->modified );

     RendererTLS *tls = Renderer_GetTLS();

     if (tls->last_renderer != this) {
          if (tls->last_renderer)
               tls->last_renderer->Flush();

          tls->last_renderer = this;
     }

     if (state->modified & (SMF_RENDER_OPTIONS | SMF_MATRIX)) {
          if (state->render_options & DSRO_MATRIX) {
               D_DEBUG_AT( DirectFB_Renderer, "  -> new transform 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n",
                           state->matrix[0], state->matrix[1], state->matrix[2], state->matrix[3], state->matrix[4], state->matrix[5] );

               if (state->matrix[0] == 0x10000 &&
                   state->matrix[1] == 0x00000 &&
                   state->matrix[2] == 0x00000 &&
                   state->matrix[3] == 0x00000 &&
                   state->matrix[4] == 0x10000 &&
                   state->matrix[5] == 0x00000)
               {
                    transform_type = WTT_IDENTITY;
               }
               else {
                    transform_type = WTT_UNKNOWN;

                    if (state->matrix[1] == 0 && state->matrix[3] == 0) {
                         if (state->matrix[2] != 0)
                              transform_type = (WaterTransformType)(transform_type | WTT_TRANSLATE_X);

                         if (state->matrix[5] != 0)
                              transform_type = (WaterTransformType)(transform_type | WTT_TRANSLATE_Y);

                         if (state->matrix[0] < 0)
                              transform_type = (WaterTransformType)(transform_type | WTT_FLIP_X);

                         if (state->matrix[0] != 0x10000 && state->matrix[0] != -0x10000)
                              transform_type = (WaterTransformType)(transform_type | WTT_SCALE_X);

                         if (state->matrix[4] < 0)
                              transform_type = (WaterTransformType)(transform_type | WTT_FLIP_Y);

                         if (state->matrix[4] != 0x10000 && state->matrix[4] != -0x10000)
                              transform_type = (WaterTransformType)(transform_type | WTT_SCALE_Y);

                         if (transform_type == WTT_UNKNOWN)
                              transform_type = WTT_IDENTITY;
                         else
                              transform_type = (WaterTransformType)(transform_type & ~WTT_UNKNOWN);
                    }
               }
          }
          else
               transform_type = WTT_IDENTITY;

          D_DEBUG_AT( DirectFB_Renderer, "  -> new transform type 0x%04x\n", transform_type );
     }


     D_DEBUG_AT( DirectFB_Renderer, "  -> state_mod 0x%08x\n", state_mod );

     state->mod_hw = (StateModificationFlags)(state->mod_hw | state->modified);
     state_mod     = (StateModificationFlags)(state_mod     | state->modified);

     D_DEBUG_AT( DirectFB_Renderer, "  -> state_mod 0x%08x\n", state_mod );

     /* If destination or blend functions have been changed... */
     if (state->modified & (SMF_DESTINATION | SMF_SRC_BLEND | SMF_DST_BLEND | SMF_RENDER_OPTIONS)) {
          /* ...force rechecking for all functions. */
          state->checked = DFXL_NONE;
     }
     else {
          /* If source/mask or blitting flags have been changed... */
          if (state->modified & (SMF_SOURCE | SMF_BLITTING_FLAGS | SMF_SOURCE_MASK | SMF_SOURCE_MASK_VALS)) {
               /* ...force rechecking for all blitting functions. */
               state->checked = (DFBAccelerationMask)(state->checked & ~DFXL_ALL_BLIT);
          }
          else if (state->modified & SMF_SOURCE2) {
               /* Otherwise force rechecking for blit2 function if source2 has been changed. */
               state->checked = (DFBAccelerationMask)(state->checked & ~DFXL_BLIT2);
          }

          /* If drawing flags have been changed... */
          if (state->modified & SMF_DRAWING_FLAGS) {
               /* ...force rechecking for all drawing functions. */
               state->checked = (DFBAccelerationMask)(state->checked & ~DFXL_ALL_DRAW);
          }
     }

     state->modified = SMF_NONE;

     Primitives::Base    *tesselated = primitives;
     DFBAccelerationMask  accel      = primitives->accel;
     WaterTransformType   transform  = transform_type;
     Engine              *next_engine;

     do {
          next_engine = getEngine( accel, transform );
          if (!next_engine) {
               DFBAccelerationMask next_accel = getTransformAccel( accel, transform );

               D_DEBUG_AT( DirectFB_Renderer, "  -> next_accel '%s'\n", Util::DFBAccelerationMask_Name(next_accel).c_str() );

               if (!next_accel) {
                    D_WARN( "no tesselation for '%s' transform 0x%04x",
                            Util::DFBAccelerationMask_Name(accel).c_str(), transform );
                    goto out;
               }


               Primitives::Base *output = tesselated->tesselate( next_accel, transform ? state->matrix : NULL );

               if (!output) {
                    D_WARN( "no tesselation from '%s' to '%s'",
                            Util::DFBAccelerationMask_Name(accel).c_str(), Util::DFBAccelerationMask_Name(next_accel).c_str() );
                    goto out;
               }

               if (tesselated != primitives)
                    delete tesselated;

               tesselated = output;
               transform  = WTT_IDENTITY;
               accel      = next_accel;
          }
     } while (!next_engine);



     D_DEBUG_AT( DirectFB_Renderer, "  -> next_engine %p\n", next_engine );
     D_DEBUG_AT( DirectFB_Renderer, "  -> engine      %p\n", engine );

     if (engine) {
          D_DEBUG_AT( DirectFB_Renderer, "  -> state mod 0x%08x\n", state_mod );
          D_DEBUG_AT( DirectFB_Renderer, "  -> count %d / %d\n",
                      operations + tesselated->count(), engine->caps.max_operations );

          if (state_mod & SMF_DESTINATION ||
              next_engine != engine ||
              operations + tesselated->count() > engine->caps.max_operations ||
              engine->check( setup ))
          {
               unbindEngine();
          }
     }

     if (!engine) {
          DFBResult ret = bindEngine( next_engine, accel );
          if (ret)
               goto out;
     }

     operations += tesselated->count();

     update( accel );

     tesselated->render( setup, engine );

out:
     if (tesselated != primitives)
          delete tesselated;
}

/**********************************************************************************************************************/

void
Renderer::DrawRectangles( const DFBRectangle *rects,
                          unsigned int        num_rects )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( %p [%d] )\n", __FUNCTION__, rects, num_rects );

     Primitives::Rectangles primitives( rects, num_rects, DFXL_DRAWRECTANGLE );

     render( &primitives );
}

void
Renderer::DrawLines( const DFBRegion *lines,
                     unsigned int     num_lines )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     Primitives::Lines primitives( lines, num_lines, DFXL_DRAWLINE );

     render( &primitives );
}

void
Renderer::FillRectangles( const DFBRectangle *rects,
                          unsigned int        num_rects )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( %p [%d] )\n", __FUNCTION__, rects, num_rects );

     Primitives::Rectangles primitives( rects, num_rects, DFXL_FILLRECTANGLE );

     render( &primitives );
}

void
Renderer::FillQuadrangles( const DFBPoint *points,
                           unsigned int    num_quads )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( %p [%d] )\n", __FUNCTION__, points, num_quads );

     Primitives::Quadrangles primitives( points, num_quads, DFXL_FILLQUADRANGLE );

     render( &primitives );
}

void
Renderer::FillTriangles( const DFBTriangle *tris,
                         unsigned int       num_tris )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( %p [%d] )\n", __FUNCTION__, tris, num_tris );

     Primitives::Triangles primitives( tris, num_tris, DFXL_FILLTRIANGLE );

     render( &primitives );
}

void
Renderer::FillTrapezoids( const DFBTrapezoid *traps,
                          unsigned int        num_traps )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( %p [%d] )\n", __FUNCTION__, traps, num_traps );

     Primitives::Trapezoids primitives( traps, num_traps, DFXL_FILLTRAPEZOID );

     render( &primitives );
}

void
Renderer::FillSpans( int            y,
                     const DFBSpan *spans,
                     unsigned int   num_spans )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( %d, %p [%d] )\n", __FUNCTION__, y, spans, num_spans );

     Primitives::Spans primitives( y, spans, num_spans, DFXL_FILLSPAN );

     render( &primitives );
}

void
Renderer::Blit( const DFBRectangle     *rects,
                const DFBPoint         *points,
                u32                     num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( %p, %p [%d] )\n", __FUNCTION__, rects, points, num );

     Primitives::Blits primitives( rects, points, num, DFXL_BLIT );

     render( &primitives );
}

void
Renderer::Blit2( const DFBRectangle     *rects,
                 const DFBPoint         *points1,
                 const DFBPoint         *points2,
                 u32                     num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

#if 0
     if (checkEngine( DFXL_BLIT2, num )) {
          update( DFXL_BLIT2 );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles_render; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               engine->Blit2( setup->tasks[i], rects, points1, points2, num );
          }
     }
#endif
}

void
Renderer::StretchBlit( const DFBRectangle     *srects,
                       const DFBRectangle     *drects,
                       u32                     num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( %p %p [%d] )\n", __FUNCTION__, srects, drects, num );

     Primitives::StretchBlits primitives( srects, drects, num, DFXL_STRETCHBLIT );

     render( &primitives );
}

void
Renderer::TileBlit( const DFBRectangle     *rects,
                    const DFBPoint         *points1,
                    const DFBPoint         *points2,
                    u32                     num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

#if 0
     if (checkEngine( DFXL_TILEBLIT, num )) {
          update( DFXL_TILEBLIT );

          // FIXME: clipping, transform

          /// loop
          for (unsigned int i=0; i<setup->tiles_render; i++) {
               if (!(setup->task_mask & (1 << i)))
                    continue;

               engine->TileBlit( setup->tasks[i], rects, points1, points2, num );
          }
     }
#endif
}

void
Renderer::TextureTriangles( const DFBVertex      *vertices,
                            int                   num,
                            DFBTriangleFormation  formation )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( %p [%d], formation %d )\n", __FUNCTION__, vertices, num, formation );

     Primitives::TexTriangles primitives( vertices, num, formation, DFXL_TEXTRIANGLES );

     render( &primitives );
}

/**********************************************************************************************************************/

DFBAccelerationMask
Renderer::getTransformAccel( DFBAccelerationMask accel,
                             WaterTransformType  type )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( '%s', type 0x%04x )\n",
                 __FUNCTION__, Util::DFBAccelerationMask_Name(accel).c_str(), type );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
               if ((type & (WTT_TRANSLATE_MASK | WTT_SCALE_MASK | WTT_FLIP_MASK)) == type)
                    return DFXL_FILLRECTANGLE;

               return DFXL_FILLQUADRANGLE;

          case DFXL_DRAWRECTANGLE:
               if (type == WTT_IDENTITY)
                    return DFXL_FILLRECTANGLE;

               if ((type & (WTT_TRANSLATE_MASK | WTT_SCALE_MASK | WTT_FLIP_MASK)) == type)
                    return DFXL_DRAWRECTANGLE;

               return DFXL_DRAWLINE;

          case DFXL_DRAWLINE:
               return DFXL_DRAWLINE;

          case DFXL_FILLTRIANGLE:
               return DFXL_FILLRECTANGLE;

          case DFXL_FILLTRAPEZOID:
               if (type == WTT_IDENTITY)
                    return DFXL_FILLRECTANGLE;

               return DFXL_FILLTRIANGLE;

          case DFXL_FILLQUADRANGLE:
               return DFXL_FILLTRIANGLE;

          case DFXL_FILLSPAN:
               if (type == WTT_IDENTITY)
                    return DFXL_FILLRECTANGLE;

               if ((type & (WTT_TRANSLATE_MASK | WTT_SCALE_MASK | WTT_FLIP_MASK)) == type)
                    return DFXL_FILLSPAN;

               return DFXL_FILLTRIANGLE;

          case DFXL_BLIT:
               if ((type & (WTT_TRANSLATE_MASK /*| WTT_FLIP_MASK*/)) == type)   // FIXME: make blits use DSBLIT_FLIP when WTT_FLIP_MASK
                    return DFXL_BLIT;

               if ((type & (WTT_TRANSLATE_MASK | WTT_SCALE_MASK /*| WTT_FLIP_MASK*/)) == type)   // FIXME: make blits use DSBLIT_FLIP when WTT_FLIP_MASK
                    return DFXL_STRETCHBLIT;

               return DFXL_TEXTRIANGLES;

          case DFXL_STRETCHBLIT:
               if ((type & (WTT_TRANSLATE_MASK | WTT_SCALE_MASK /*| WTT_FLIP_MASK*/)) == type)   // FIXME: make blits use DSBLIT_FLIP when WTT_FLIP_MASK
                    return DFXL_STRETCHBLIT;

               return DFXL_TEXTRIANGLES;

          case DFXL_TEXTRIANGLES:
               return DFXL_TEXTRIANGLES;

          default:
               D_BUG( "unknown accel 0x%08x", accel );
     }

     return DFXL_NONE;
}

Engine *
Renderer::getEngine( DFBAccelerationMask  accel,
                     WaterTransformType   transform )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s( '%s', transform 0x%04x )\n", __FUNCTION__,
                 Util::DFBAccelerationMask_Name(accel).c_str(), transform );

     if (engine && (transform & engine->caps.transforms) == transform) {
          /* If the function needs to be checked... */
          if (!(state->checked & accel)) {
               /* Unset unchecked functions. */
               state->accel = (DFBAccelerationMask)(state->accel & state->checked);

               /// use task[0]
               if (engine->CheckState( state, accel ) == DFB_OK)
                    state->accel = (DFBAccelerationMask)(state->accel | accel);

               /* Add the function to 'checked functions'. */
               state->checked = (DFBAccelerationMask)(state->checked | state->accel);
          }

          if (state->accel & accel)
               return engine;
     }

     for (std::list<Engine*>::const_iterator it = engines.begin(); it != engines.end(); ++it) {
          Engine *engine = *it;

          // TODO: add engine mask for selection by user

          if (dfb_config->software_only && !engine->caps.software) {
               D_DEBUG_AT( DirectFB_Renderer, "  -> skipping engine, software only!\n" );
               continue;
          }

          if ((transform & engine->caps.transforms) == transform &&
              engine->CheckState( state, accel ) == DFB_OK)
          {
               return engine;
          }
     }

     return NULL;
}

DFBResult
Renderer::bindEngine( Engine              *engine,
                      DFBAccelerationMask  accel )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     D_ASSERT( this->engine == NULL );
     D_ASSERT( setup == NULL );

     /// loop
     setup = new Setup( state->destination->config.size.w, state->destination->config.size.h, engine->caps.cores );

     ret = engine->bind( setup );
     if (ret) {
          D_DERROR( ret, "DirectFB/Renderer: Failed to bind engine!\n" );
          return ret;
     }

#if D_DEBUG_ENABLED
     for (unsigned int i=0; i<setup->tiles; i++) {
          D_ASSERT( setup->tasks[i] != NULL );
          DFB_REGION_ASSERT( &setup->clips[i] );
     }
#endif

     /// prepare for par flush
     for (unsigned int i=1; i<setup->tiles; i++) {
          setup->tasks[0]->AddSlave( setup->tasks[i] );
     }

     state->modified = SMF_NONE;
     state->mod_hw   = SMF_ALL;
     state->set      = DFXL_NONE;
     state->accel    = accel;
     state->checked  = accel;

     state_mod       = SMF_ALL;

     this->engine = engine;

     return DFB_OK;
}

void
Renderer::unbindEngine()
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     D_ASSERT( engine != NULL );
     D_ASSERT( setup != NULL );

     leaveLock( &state->src2 );
     leaveLock( &state->src_mask );
     leaveLock( &state->src );
     leaveLock( &state->dst );

//     state->accel   = DFXL_NONE;
//     state->checked = DFXL_NONE;

     /// par flush
     setup->tasks[0]->Flush();

     delete setup;
     setup = NULL;

     engine     = NULL;
     operations = 0;

     allocations.clear();
}


/*********************************************************************************************************************/

std::list<Engine*>  Renderer::engines;


DFBResult
Renderer::RegisterEngine( Engine *engine )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     engines.push_back( engine );

     return DFB_OK;
}

void
Renderer::UnregisterEngine( Engine *engine )
{
     D_DEBUG_AT( DirectFB_Renderer, "Renderer::%s()\n", __FUNCTION__ );

     engines.remove( engine );
}

/*********************************************************************************************************************/

DFBResult
Engine::DrawRectangles( SurfaceTask        *task,
                        const DFBRectangle *rects,
                        unsigned int        num_rects )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::DrawLines( SurfaceTask     *task,
                   const DFBRegion *lines,
                   unsigned int     num_lines )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::FillRectangles( SurfaceTask        *task,
                        const DFBRectangle *rects,
                        unsigned int        num_rects )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::FillTriangles( SurfaceTask       *task,
                       const DFBTriangle *tris,
                       unsigned int       num_tris )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::FillTrapezoids( SurfaceTask        *task,
                        const DFBTrapezoid *traps,
                        unsigned int        num_traps )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::FillSpans( SurfaceTask   *task,
                   int            y,
                   const DFBSpan *spans,
                   unsigned int   num_spans )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::FillQuadrangles( SurfaceTask    *task,
                         const DFBPoint *points,
                         unsigned int    num_quads )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::Blit( SurfaceTask        *task,
              const DFBRectangle *rects,
              const DFBPoint     *points,
              u32                 num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::Blit2( SurfaceTask        *task,
               const DFBRectangle *rects,
               const DFBPoint     *points1,
               const DFBPoint     *points2,
               u32                 num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::StretchBlit( SurfaceTask        *task,
                     const DFBRectangle *srects,
                     const DFBRectangle *drects,
                     u32                 num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::TileBlit( SurfaceTask        *task,
                  const DFBRectangle *rects,
                  const DFBPoint     *points1,
                  const DFBPoint     *points2,
                  u32                 num )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::TextureTriangles( SurfaceTask          *task,
                          const DFBVertex      *vertices,
                          unsigned int          num,
                          DFBTriangleFormation  formation )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}

DFBResult
Engine::TextureTriangles( SurfaceTask          *task,
                          const DFBVertex1616  *vertices,
                          unsigned int          num,
                          DFBTriangleFormation  formation )
{
     D_DEBUG_AT( DirectFB_Renderer, "Engine::%s()\n", __FUNCTION__ );

     return DFB_UNIMPLEMENTED;
}



















D_DEBUG_DOMAIN( Test_MyEngine, "Test/MyEngine", "Test MyEngine" );
D_DEBUG_DOMAIN( Test_MyTask,   "Test/MyTask",   "Test MyTask" );










class MyEngine;

class MyTask : public DirectFB::SurfaceTask
{
public:
     MyTask( MyEngine *engine )
          :
          SurfaceTask( CSAID_CPU ),
          engine( engine )
     {
     }

     virtual ~MyTask()
     {
     }

protected:
     virtual DFBResult Push();
     virtual DFBResult Run();

private:
     friend class MyEngine;

     MyEngine *engine;

     typedef enum {
          TYPE_SET_DESTINATION,
          TYPE_SET_SOURCE,
          TYPE_SET_COLOR,
          TYPE_FILL_RECTS,
          TYPE_BLIT
     } Type;

     std::vector<u32> commands;
};


class MyEngine : public DirectFB::Engine {
public:
     MyEngine()
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          for (int i=0; i<1; i++) {
               char name[] = "MyEngineX";

               name[8] = '0' + i;

               threads[i] = direct_thread_create( DTT_DEFAULT, myEngineLoop, this, name );
          }
     }


     virtual DFBResult bind          ( Renderer::Setup        *setup )
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          for (unsigned int i=0; i<setup->tiles; i++) {
               setup->tasks[i] = new MyTask( this );
          }

          return DFB_OK;
     }

     virtual DFBResult check         ( Renderer::Setup        *setup )
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          return DFB_OK;
     }

     virtual DFBResult CheckState    ( CardState              *state,
                                       DFBAccelerationMask     accel )
     {
          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          if (DFB_BLITTING_FUNCTION( accel )) {
               if (accel != DFXL_BLIT || //state->blittingflags ||
                   state->destination->config.format != DSPF_ARGB || state->source->config.format != DSPF_ARGB)
                    return DFB_UNSUPPORTED;
          }
          else {
               if (accel != DFXL_FILLRECTANGLE || state->drawingflags ||
                   state->destination->config.format != DSPF_ARGB)
                    return DFB_UNSUPPORTED;
          }

          return DFB_OK;
     }

     virtual DFBResult SetState      ( DirectFB::SurfaceTask  *task,
                                       CardState              *state,
                                       StateModificationFlags  modified,
                                       DFBAccelerationMask     accel )
     {
          MyTask *mytask = (MyTask *)task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          if (modified & SMF_DESTINATION) {
               mytask->commands.push_back( MyTask::TYPE_SET_DESTINATION );
               mytask->commands.push_back( (long long)(long)state->dst.addr >> 32 );
               mytask->commands.push_back( (u32)(long)state->dst.addr );
               mytask->commands.push_back( state->dst.pitch );

               state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_DESTINATION);
          }

          if (modified & SMF_COLOR) {
               mytask->commands.push_back( MyTask::TYPE_SET_COLOR );
               mytask->commands.push_back( PIXEL_ARGB( state->color.a, state->color.r, state->color.g, state->color.b ) );

               state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_COLOR);
          }

          if (DFB_BLITTING_FUNCTION( accel )) {
               if (modified & SMF_SOURCE) {
                    mytask->commands.push_back( MyTask::TYPE_SET_SOURCE );
                    mytask->commands.push_back( (long long)(long)state->src.addr >> 32 );
                    mytask->commands.push_back( (u32)(long)state->src.addr );
                    mytask->commands.push_back( state->src.pitch );

                    state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_SOURCE);
               }
          }

          return DFB_OK;
     }


     virtual DFBResult FillRectangles( DirectFB::SurfaceTask  *task,
                                       const DFBRectangle     *rects,
                                       unsigned int            num_rects )
     {
          MyTask *mytask = (MyTask *)task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s( %d )\n", __FUNCTION__, num_rects );

          mytask->commands.push_back( MyTask::TYPE_FILL_RECTS );
          mytask->commands.push_back( num_rects );

          for (unsigned int i=0; i<num_rects; i++) {
               mytask->commands.push_back( rects[i].x );
               mytask->commands.push_back( rects[i].y );
               mytask->commands.push_back( rects[i].w );
               mytask->commands.push_back( rects[i].h );
          }

          return DFB_OK;
     }


     virtual DFBResult Blit( DirectFB::SurfaceTask  *task,
                             const DFBRectangle     *rects,
                             const DFBPoint         *points,
                             u32                     num )
     {
          MyTask *mytask = (MyTask *)task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s( %d )\n", __FUNCTION__, num );

          mytask->commands.push_back( MyTask::TYPE_BLIT );
          mytask->commands.push_back( num );

          for (unsigned int i=0; i<num; i++) {
               mytask->commands.push_back( rects[i].x );
               mytask->commands.push_back( rects[i].y );
               mytask->commands.push_back( rects[i].w );
               mytask->commands.push_back( rects[i].h );
               mytask->commands.push_back( points[i].x );
               mytask->commands.push_back( points[i].y );
          }

          return DFB_OK;
     }


private:
     friend class MyTask;

     DirectFB::FIFO<MyTask*>  fifo;
     DirectThread            *threads[1];

     static void *
     myEngineLoop( DirectThread *thread,
                   void         *arg )
     {
          MyEngine *engine = (MyEngine *)arg;
          MyTask   *task;

          D_DEBUG_AT( Test_MyEngine, "MyEngine::%s()\n", __FUNCTION__ );

          while (true) {
               task = engine->fifo.pull();

               task->Run();
          }

          return NULL;
     }
};


DFBResult
MyTask::Push()
{
     D_DEBUG_AT( Test_MyTask, "MyTask::%s()\n", __FUNCTION__ );

     engine->fifo.push( this );

     return DFB_OK;
}

DFBResult
MyTask::Run()
{
     u32     ptr1      = 0;
     u32     ptr2      = 0;
     void   *ptr       = 0;
     u32     pitch     = 0;
     void   *src_ptr   = 0;
     u32     src_pitch = 0;
     u32     color     = 0;
     u32     num;
     size_t  size;

     D_DEBUG_AT( Test_MyTask, "MyTask::%s()\n", __FUNCTION__ );

     size = commands.size();
     if (size > 0) {
          u32 *buffer = &commands[0];

          for (unsigned int i=0; i<size; i++) {
               D_DEBUG_AT( Test_MyTask, "  -> next command at [%d]\n", i );

               switch (buffer[i]) {
                    case MyTask::TYPE_SET_DESTINATION:
                         D_DEBUG_AT( Test_MyTask, "  -> SET_DESTINATION\n" );

                         ptr1  = buffer[++i];
                         ptr2  = buffer[++i];
                         ptr   = (void*)(long)(((long long)ptr1 << 32) | ptr2);
                         D_DEBUG_AT( Test_MyTask, "  -> 0x%08x 0x%08x = %p\n", ptr1, ptr2, ptr );

                         pitch = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> pitch %d\n", pitch );
                         break;

                    case MyTask::TYPE_SET_SOURCE:
                         D_DEBUG_AT( Test_MyTask, "  -> SET_SOURCE\n" );

                         ptr1    = buffer[++i];
                         ptr2    = buffer[++i];
                         src_ptr = (void*)(long)(((long long)ptr1 << 32) | ptr2);
                         D_DEBUG_AT( Test_MyTask, "  -> 0x%08x 0x%08x = %p\n", ptr1, ptr2, src_ptr );

                         src_pitch = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> pitch %d\n", src_pitch );
                         break;

                    case MyTask::TYPE_SET_COLOR:
                         D_DEBUG_AT( Test_MyTask, "  -> SET_COLOR\n" );

                         color = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> 0x%08x\n", color );
                         break;

                    case MyTask::TYPE_FILL_RECTS:
                         D_DEBUG_AT( Test_MyTask, "  -> FILL_RECTS\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> num %d\n", num );

                         for (u32 n=0; n<num; n++) {
                              int x = buffer[++i];
                              int y = buffer[++i];
                              int w = buffer[++i];
                              int h = buffer[++i];

                              D_DEBUG_AT( Test_MyTask, "  -> %4d,%4d-%4dx%4d\n", x, y, w, h );

                              u32 *d = (u32 *)((u8*)ptr + pitch * y + x * 4);

                              while (h--) {
                                   for (int X=0; X<w; X++) {
                                        d[X] = color;
                                   }

                                   d += pitch/4;
                              }
                         }
                         break;

                    case MyTask::TYPE_BLIT:
                         D_DEBUG_AT( Test_MyTask, "  -> BLIT\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( Test_MyTask, "  -> num %d\n", num );

                         for (u32 n=0; n<num; n++) {
                              int x  = buffer[++i];
                              int y  = buffer[++i];
                              int w  = buffer[++i];
                              int h  = buffer[++i];
                              int dx = buffer[++i];
                              int dy = buffer[++i];

                              D_DEBUG_AT( Test_MyTask, "  -> %4d,%4d-%4dx%4d -> %4d,%4d\n", x, y, w, h, dx, dy );

                              u32 *d = (u32 *)((u8*)ptr + pitch * dy + dx * 4);
                              u32 *s = (u32 *)((u8*)src_ptr + src_pitch * y + x * 4);

                              while (h--) {
                                   for (int X=0; X<w; X++) {
                                        d[X] = s[X];
                                   }

                                   d += pitch/4;
                                   s += src_pitch/4;
                              }
                         }
                         break;

                    default:
                         D_BUG( "unknown type %d", buffer[i] );
               }
          }
     }

     Done();

     return DFB_OK;
}


extern "C" {
     void
     register_myengine()
     {
          Renderer::RegisterEngine( new MyEngine() );
     }
}



}

