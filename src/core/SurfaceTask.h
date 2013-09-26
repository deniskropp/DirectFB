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



#ifndef ___DirectFB__SurfaceTask__H___
#define ___DirectFB__SurfaceTask__H___


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


DFB_SurfaceTask *SurfaceTask_New      ( CoreSurfaceAccessorID    accessor );
DFBResult        SurfaceTask_AddAccess( DFB_SurfaceTask         *task,
                                        CoreSurfaceAllocation   *allocation,
                                        CoreSurfaceAccessFlags   flags );

/*********************************************************************************************************************/

#ifdef __cplusplus
}


#include <direct/Magic.h>
#include <direct/Mutex.h>
#include <direct/Performer.h>
#include <direct/String.h>

#include <core/Fifo.h>
#include <core/Task.h>
#include <core/Util.h>

#include <list>
#include <map>
#include <queue>
#include <string>
#include <vector>


namespace DirectFB {


class SurfaceAllocationAccess : public Direct::Magic<SurfaceAllocationAccess> {
public:
     CoreSurfaceAllocation  *allocation;
     CoreSurfaceAccessFlags  flags;

     SurfaceAllocationAccess( CoreSurfaceAllocation  *allocation,
                              CoreSurfaceAccessFlags  flags )
          :
          allocation( allocation ),
          flags( flags )
     {
     }
};


class SurfaceTask : public Task
{
public:
     class Hook {
     public:
          virtual DFBResult setup( SurfaceTask *task ) { return DFB_OK; }
          virtual void      finalise( SurfaceTask *task ) {}
     };

     SurfaceTask( CoreSurfaceAccessorID accessor );

     DFBResult AddAccess( CoreSurfaceAllocation  *allocation,
                          CoreSurfaceAccessFlags  flags );

     DFBResult AddHook( Hook *hook );

protected:
     virtual DFBResult Setup();
     virtual void      Finalise();
public:
     virtual void                  Describe( Direct::String &string ) const;
     virtual const Direct::String &TypeName() const;

protected:
     virtual DFBResult CacheFlush();
     virtual DFBResult CacheInvalidate();

public://private:
     CoreSurfaceAccessorID                   accessor;
     std::vector<SurfaceAllocationAccess>    accesses; // FIXME: Use Direct::Vector
     std::vector<Hook*>                      hooks; // FIXME: Use Direct::Vector

     CoreSurfaceAccessFlags GetCacheFlags()
     {
          CoreSurfaceAccessFlags ret = CSAF_NONE;

          for (std::vector<SurfaceAllocationAccess>::const_iterator it=accesses.begin();
                it!=accesses.end() && ret != (CSAF_CACHE_FLUSH | CSAF_CACHE_INVALIDATE);
                it++)
          {
               if ((*it).flags & CSAF_CACHE_FLUSH)
                    ret = (CoreSurfaceAccessFlags)(ret | CSAF_CACHE_FLUSH);

               if ((*it).flags & CSAF_CACHE_INVALIDATE)
                    ret = (CoreSurfaceAccessFlags)(ret | CSAF_CACHE_INVALIDATE);
          }

          return ret;
     }

private:
     static const Direct::String _Type;
};


}


#endif // __cplusplus


#endif

