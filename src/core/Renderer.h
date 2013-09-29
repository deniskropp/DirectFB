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



#ifndef ___DirectFB__Renderer__H___
#define ___DirectFB__Renderer__H___

#ifdef __cplusplus

#include <direct/Types++.h>

extern "C" {
#endif

#include <direct/thread.h>

#include <core/state.h>
#include <core/surface.h>

#include <directfb.h>
#include <directfb_graphics.h>


// C Wrapper

void Renderer_TLS__init( void );
void Renderer_TLS__deinit( void );

void Renderer_DeleteEngines( void );

#ifdef __cplusplus
}


extern "C" {
#include <direct/thread.h>

#include <core/CoreGraphicsStateClient.h>
#include <core/surface.h>

#include <directfb.h>
}


#include <core/SurfaceTask.h>

#include <list>
#include <map>

#include <direct/LockWQ.h>
#include <direct/Magic.h>


namespace DirectFB {


/*

TODO
  - clipping
  - locking in engine / task
  - allocations in e.g. GL thread

  - finish port Genefx to Engine
  - triangle to rectangle filling
  - rectangle drawing to filling


 - lock mask surface?

*/


class Engine;


class SurfaceAllocationKey
{
public:
     DFBSurfaceID             surfaceID;
     CoreSurfaceBufferRole    role;
     DFBSurfaceStereoEye      eye;
     u32                      flips;

     SurfaceAllocationKey( DFBSurfaceID             surfaceID,
                           CoreSurfaceBufferRole    role,
                           DFBSurfaceStereoEye      eye,
                           u32                      flips )
          :
          surfaceID( surfaceID ),
          role( role ),
          eye( eye ),
          flips( flips )
     {
     }

     bool operator < (const SurfaceAllocationKey &other) const {
          if (surfaceID == other.surfaceID) {
               if (role == other.role) {
                    if (eye == other.eye)
                         return flips < other.flips;

                    return eye < other.eye;
               }

               return role < other.role;
          }

          return surfaceID < other.surfaceID;
     }
};

typedef std::map<SurfaceAllocationKey,CoreSurfaceAllocation*>  SurfaceAllocationMap;
typedef std::pair<SurfaceAllocationKey,CoreSurfaceAllocation*> SurfaceAllocationMapPair;


namespace Primitives {

class Base;

}


class Throttle : public Direct::Magic<Throttle>
{
     friend class Renderer;

     class Hook : public SurfaceTask::Hook {
     public:
          Hook( Throttle &throttle,
                u32       cookie )
               :
               throttle( throttle ),
               cookie( cookie )
          {
          }

     private:
          DFBResult setup( SurfaceTask *task );
          void      finalise( SurfaceTask *task );

          Throttle &throttle;
          u32       cookie;
     };

public:
     Throttle( Renderer &renderer );
     virtual ~Throttle();

     void ref();
     void unref();

     DFBResult waitDone( unsigned long timeout_us = 0 );

protected:
     virtual void AddTask( SurfaceTask *task, u32 cookie );
     virtual void SetThrottle( int percent ) = 0;

     CoreGraphicsState *gfx_state;

private:
     unsigned int       ref_count;
     unsigned int       task_count;
     Direct::LockWQ     lwq;
};

class Renderer : public Direct::Magic<Renderer>
{
public:
     typedef ::DirectFB::Throttle Throttle;

     class Setup
     {
     public:
          unsigned int   tiles;
          SurfaceTask  **tasks;
          DFBRegion     *clips;
          DFBRegion     *clips_clipped;
          unsigned int   task_mask;
          unsigned int   tiles_render;

          Setup( int width, int height, unsigned int tiles = 1 )
               :
               tiles( tiles ),
               tiles_render( tiles )
          {
               D_ASSERT( tiles > 0 );

               tasks         = new SurfaceTask*[tiles];
               clips         = new DFBRegion[tiles*2];
               clips_clipped = clips + tiles;

               memset( tasks, 0, sizeof(SurfaceTask*) * tiles );

               // FIXME: add Renderer control API for this
               if (0) {
                    int tw = ((width / tiles) + 7) & ~7;

                    tiles = (width / tw) + !!(tiles % tw);

                    for (unsigned int i=0; i<tiles; i++) {
                         clips[i].x1 = tw * i;
                         clips[i].x2 = (i == tiles-1) ? width - 1 : (clips[i].x1 + tw - 1); // TODO: may move remainder to first task,
                                                                                            // assuming it is emitted slightly earlier

                         clips[i].y1 = 0;
                         clips[i].y2 = height - 1;
                    }
               }
               else {
                    int th = height / tiles;

                    for (unsigned int i=0; i<tiles; i++) {
                         clips[i].x1 = 0;
                         clips[i].x2 = width - 1;

                         clips[i].y1 = th * i;
                         clips[i].y2 = (i == tiles-1) ? height - 1 : (clips[i].y1 + th - 1); // TODO: may move remainder to first task,
                                                                                             // assuming it is emitted slightly earlier
                    }
               }
          }

          ~Setup()
          {
               delete[] tasks;
               delete[] clips;
          }
     };

public:
     Renderer( CardState            *state,
               CoreGraphicsState    *gfx_state,
               const Direct::String &name = "NONAME" );
     ~Renderer();

     void SetThrottle( Throttle *throttle );

     void Flush( u32 cookie = 0, CoreGraphicsStateClientFlushFlags flags = CGSCFF_NONE );

     static void      FlushCurrent( u32 cookie = 0 );
     static Renderer *GetCurrent();


     void DrawRectangles  ( const DFBRectangle     *rects,
                            unsigned int            num_rects );

     void DrawLines       ( const DFBRegion        *lines,
                            unsigned int            num_lines );

     void FillRectangles  ( const DFBRectangle     *rects,
                            unsigned int            num_rects );

     void FillQuadrangles ( const DFBPoint         *points,
                            unsigned int            num_quads );

     void FillTriangles   ( const DFBTriangle      *tris,
                            unsigned int            num_tris );

     void FillTrapezoids  ( const DFBTrapezoid     *traps,
                            unsigned int            num_traps );

     void FillSpans       ( int                     y,
                            const DFBSpan          *spans,
                            unsigned int            num_spans );


     void Blit            ( const DFBRectangle     *rects,
                            const DFBPoint         *points,
                            u32                     num );

     void Blit2           ( const DFBRectangle     *rects,
                            const DFBPoint         *points1,
                            const DFBPoint         *points2,
                            u32                     num );

     void StretchBlit     ( const DFBRectangle     *srects,
                            const DFBRectangle     *drects,
                            u32                     num );

     void TileBlit        ( const DFBRectangle     *rects,
                            const DFBPoint         *points1,
                            const DFBPoint         *points2,
                            u32                     num );

     void TextureTriangles( const DFBVertex        *vertices,
                            int                     num,
                            DFBTriangleFormation    formation );


public:
     CardState             *state;
     CoreGraphicsState     *gfx_state;
     const Direct::String   name;

private:
     StateModificationFlags state_mod;
     WaterTransformType     transform_type;

     Throttle              *throttle;

     DirectThread          *thread;     // where the renderer is used (while engine is bound)
     Engine                *engine;
     Setup                 *setup;
     unsigned int           operations;

     SurfaceAllocationMap   allocations;


     DFBAccelerationMask getTransformAccel( DFBAccelerationMask accel,
                                            WaterTransformType  type );

     Engine   *getEngine   ( DFBAccelerationMask  accel,
                             WaterTransformType   transform );

     DFBResult bindEngine  ( Engine              *engine,
                             DFBAccelerationMask  accel );
     DFBResult rebindEngine( DFBAccelerationMask  accel );
     void      unbindEngine( u32                               cookie,
                             CoreGraphicsStateClientFlushFlags flags,
                             bool                              discard = false );
     void      flushTask   ( u32                               cookie,
                             CoreGraphicsStateClientFlushFlags flags,
                             bool                              discard = false );

     void      render    ( Primitives::Base       *primitives );

     DFBResult update    ( DFBAccelerationMask     accel );

     DFBResult updateLock( CoreSurfaceBufferLock  *lock,
                           CoreSurface            *surface,
                           CoreSurfaceBufferRole   role,
                           DFBSurfaceStereoEye     eye,
                           u32                     flips,
                           CoreSurfaceAccessFlags  flags );

     DFBResult enterLock ( CoreSurfaceBufferLock  *lock,
                           CoreSurfaceAllocation  *allocation,
                           CoreSurfaceAccessFlags  flags );
     DFBResult leaveLock ( CoreSurfaceBufferLock  *lock );


     /* Engines */

private:
     static std::list<Engine*>     engines;

public:
     static DFBResult RegisterEngine  ( Engine *engine );
     static void      UnregisterEngine( Engine *engine );
     static void      DeleteEngines   ();
};


namespace Primitives {

class Base {
public:
     DFBAccelerationMask accel;
     bool                clipped;
     bool                del;

     Base( DFBAccelerationMask accel,
           bool                clipped,
           bool                del )
          :
          accel( accel ),
          clipped( clipped ),
          del( del )
     {
     }

     virtual ~Base() {
     }

     virtual Base *tesselate( DFBAccelerationMask  accel,
                              const DFBRegion     *clip,
                              const s32           *matrix )
     {
          return NULL;
     }

     virtual unsigned int count() const = 0;

     virtual void render( Renderer::Setup *setup,
                          Engine          *engine ) = 0;
};

}



class Engine {
     friend class Renderer;

public:
     class Description {
     public:
          Direct::String      name;

          Description()
               :
               name( "NONAME" )
          {
          }
     };

     class Capabilities : public DFBGraphicsEngineCapabilities {
     public:
          Capabilities()
          {
               software         = false;
               cores            = 1;
               clipping         = DFXL_NONE;
               render_options   = DSRO_NONE;
               max_scale_down_x = UINT_MAX;
               max_scale_down_y = UINT_MAX;
               max_operations   = UINT_MAX;
               transforms       = WTT_IDENTITY;
          }
     };

     Capabilities caps;
     Description  desc;

protected:
     Engine()
     {
     }

     virtual ~Engine()
     {
     }

public:
     virtual DFBResult bind            ( Renderer::Setup        *setup ) = 0;
     virtual DFBResult check           ( Renderer::Setup        *setup ) = 0;

     virtual DFBResult CheckState      ( CardState              *state,    // FIXME: make const
                                         DFBAccelerationMask     accel ) = 0;

     virtual DFBResult SetState        ( SurfaceTask            *task,
                                         CardState              *state,
                                         StateModificationFlags  modified,
                                         DFBAccelerationMask     accel ) = 0;



     virtual DFBResult DrawRectangles  ( SurfaceTask            *task,
                                         const DFBRectangle     *rects,
                                         unsigned int           &num_rects );

     virtual DFBResult DrawLines       ( SurfaceTask            *task,
                                         const DFBRegion        *lines,
                                         unsigned int           &num_lines );

     virtual DFBResult FillRectangles  ( SurfaceTask            *task,
                                         const DFBRectangle     *rects,
                                         unsigned int           &num_rects );

     virtual DFBResult FillTriangles   ( SurfaceTask            *task,
                                         const DFBTriangle      *tris,
                                         unsigned int           &num_tris );

     virtual DFBResult FillTrapezoids  ( SurfaceTask            *task,
                                         const DFBTrapezoid     *traps,
                                         unsigned int           &num_traps );

     virtual DFBResult FillSpans       ( SurfaceTask            *task,
                                         int                     y,
                                         const DFBSpan          *spans,
                                         unsigned int           &num_spans );

     virtual DFBResult FillQuadrangles ( SurfaceTask            *task,
                                         const DFBPoint         *points,
                                         unsigned int           &num_quads );



     virtual DFBResult Blit            ( SurfaceTask            *task,
                                         const DFBRectangle     *rects,
                                         const DFBPoint         *points,
                                         u32                    &num );

     virtual DFBResult Blit2           ( SurfaceTask            *task,
                                         const DFBRectangle     *rects,
                                         const DFBPoint         *points1,
                                         const DFBPoint         *points2,
                                         u32                    &num );

     virtual DFBResult StretchBlit     ( SurfaceTask            *task,
                                         const DFBRectangle     *srects,
                                         const DFBRectangle     *drects,
                                         u32                    &num );

     virtual DFBResult TileBlit        ( SurfaceTask            *task,
                                         const DFBRectangle     *rects,
                                         const DFBPoint         *points1,
                                         const DFBPoint         *points2,
                                         u32                    &num );

     virtual DFBResult TextureTriangles( SurfaceTask            *task,
                                         const DFBVertex        *vertices,
                                         unsigned int           &num,
                                         DFBTriangleFormation    formation );

     virtual DFBResult TextureTriangles( SurfaceTask            *task,
                                         const DFBVertex1616    *vertices,
                                         unsigned int           &num,
                                         DFBTriangleFormation    formation );
};


/*


Multi Core GPU Models

1) Engine per core

2) Single engine with parallel tasks handled by multiple cores

3) Single engine as in 2) but with special SLI/tile code creating multiple tasks at once



Optimised engine/task implementation would encode command buffer once and dispatch with
different clip (for different tiles) the same command buffer to multiple cores.


Multiple engines could be used on top of different subsets of the same set of cores,
e.g. one full performance four core engine and one with only one core allocated.

*/


}


#endif // __cplusplus

#endif

