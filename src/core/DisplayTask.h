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



#ifndef ___DirectFB__DisplayTask__H___
#define ___DirectFB__DisplayTask__H___


#ifdef __cplusplus

#include <direct/Types++.h>

extern "C" {
#endif

#include <direct/fifo.h>
#include <direct/thread.h>
#include <direct/trace.h>

#include <core/surface.h>

#include <gfx/util.h>

#include <directfb.h>


DFBResult        DisplayTask_Generate ( CoreLayerRegion         *region,
                                        const DFBRegion         *left_update,
                                        const DFBRegion         *right_update,
                                        DFBSurfaceFlipFlags      flags,
                                        long long                pts,
                                        DFB_DisplayTask        **ret_task );

long long        DisplayTask_GetPTS   ( DFB_DisplayTask         *task );


/*********************************************************************************************************************/

#ifdef __cplusplus
}


#include <direct/Magic.h>
#include <direct/Mutex.h>
#include <direct/Performer.h>
#include <direct/String.h>

#include <core/Fifo.h>
#include <core/SurfaceTask.h>
#include <core/Util.h>

#include <list>
#include <map>
#include <queue>
#include <string>
#include <vector>


namespace DirectFB {


class DisplayTask : public SurfaceTask
{
public:
     DisplayTask( CoreLayerRegion       *region,
                  const DFBRegion       *left_update,
                  const DFBRegion       *right_update,
                  DFBSurfaceFlipFlags    flip_flags,
                  long long              pts,
                  CoreSurfaceAllocation *left_allocation,
                  CoreSurfaceAllocation *right_allocation,
                  bool                   stereo );

     ~DisplayTask();

     static DFBResult Generate( CoreLayerRegion      *region,
                                const DFBRegion      *left_update,
                                const DFBRegion      *right_update,
                                DFBSurfaceFlipFlags   flags,
                                long long             pts,
                                DisplayTask         **ret_task );

protected:
     virtual void      Flush();
     virtual DFBResult Setup();
     virtual DFBResult Run();
     virtual void      Finalise();
public:
     virtual void                  Describe( Direct::String &string ) const;
     virtual const Direct::String &TypeName() const;

private:
     CoreLayerRegion       *region;
     DFBRegion             *left_update;
     DFBRegion             *right_update;
     DFBRegion              left_update_region;
     DFBRegion              right_update_region;
     DFBSurfaceFlipFlags    flip_flags;
     long long              pts;
     CoreSurfaceAllocation *left_allocation;
     CoreSurfaceAllocation *right_allocation;
     bool                   stereo;
     CoreLayer             *layer;
     CoreLayerContext      *context;
     int                    index;

public:
     long long GetPTS() const {
          return pts;
     }

private:
     static const Direct::String _Type;
};


}

#endif // __cplusplus


#endif

