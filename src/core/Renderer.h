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

#ifndef ___DirectFB__Renderer__H___
#define ___DirectFB__Renderer__H___

#ifdef __cplusplus
extern "C" {
#endif

#include <direct/thread.h>

#include <core/state.h>
#include <core/surface.h>

#include <directfb.h>


// C Wrapper


#ifdef __cplusplus
}


extern "C" {
#include <direct/thread.h>

#include <core/surface.h>

#include <directfb.h>
}


#include "Task.h"

#include <list>
#include <map>


namespace DirectFB {


/*

TODO
  - clipping
  - other render ops
  - transfers
  - locking in engine / task
  - allocations in e.g. GL thread

  - port Genefx to Engine
  - triangle to rectangle filling
  - rectangle drawing to filling

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



class Renderer
{
public:
     class Setup
     {
     public:
          unsigned int           tiles;
          SurfaceTask          **tasks;
          DFBRegion             *clips;

          Setup( int width, int height, unsigned int tiles = 1 )
               :
               tiles( tiles )
          {
               D_ASSERT( tiles > 0 );

               tasks = new SurfaceTask*[tiles];
               clips = new DFBRegion[tiles];

               memset( tasks, 0, sizeof(SurfaceTask*) * tiles );

               if (0) {
                    int tw = width / tiles;

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
     Renderer( CardState *state );
     ~Renderer();


     void Flush();

     void FillRectangles( const DFBRectangle *rects,
                          unsigned int        num_rects );

     // TODO: Add other drawing functions

     void Blit          ( const DFBRectangle *rects,
                          const DFBPoint     *points,
                          u32                 num );

     // TODO: Add other blitting functions


private:
     CardState             *state;

     Engine                *engine;
     Setup                 *setup;

     SurfaceAllocationMap   allocations;


     bool      checkEngine ( DFBAccelerationMask  accel );

     DFBResult bindEngine  ( Engine              *engine );
     void      unbindEngine();


     void      update    ( DFBAccelerationMask     accel );

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
};



class Engine {
     friend class Renderer;

protected:
     unsigned int cores;

     Engine()
          :
          cores( 1 )
     {
     }

public:
     virtual DFBResult bind          ( Renderer::Setup        *setup ) = 0;

     virtual DFBResult CheckState    ( DirectFB::SurfaceTask  *task,
                                       CardState              *state,
                                       DFBAccelerationMask     accel ) = 0;

     virtual DFBResult SetState      ( SurfaceTask            *task,
                                       CardState              *state,
                                       StateModificationFlags  modified,
                                       DFBAccelerationMask     accel ) = 0;



     virtual DFBResult FillRectangles( SurfaceTask            *task,
                                       const DFBRectangle     *rects,
                                       unsigned int            num_rects );

     // TODO: Add other drawing functions


     virtual DFBResult Blit          ( SurfaceTask            *task,
                                       const DFBRectangle     *rects,
                                       const DFBPoint         *points,
                                       u32                     num );

     // TODO: Add other blitting functions
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

