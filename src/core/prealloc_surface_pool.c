/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <direct/debug.h>

#include <fusion/conf.h>
#include <fusion/shmalloc.h>
#include <fusion/shm/pool.h>

#include <core/core.h>
#include <core/surface_pool.h>

typedef struct {
     void *addr;
     int   pitch;
} PreallocAllocationData;

/**********************************************************************************************************************/

static int
preallocAllocationDataSize()
{
     return sizeof(PreallocAllocationData);
}

static DFBResult
preallocInitPool( CoreDFB                    *core,
                  CoreSurfacePool            *pool,
                  void                       *pool_data,
                  void                       *pool_local,
                  void                       *system_data,
                  CoreSurfacePoolDescription *ret_desc )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps     = CSPCAPS_NONE;
     ret_desc->access   = CSAF_CPU_READ | CSAF_CPU_WRITE;
     ret_desc->types    = CSTF_PREALLOCATED;
     ret_desc->priority = CSPP_DEFAULT;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "Preallocated Memory" );

     return DFB_OK;
}

static DFBResult
preallocTestConfig( CoreSurfacePool         *pool,
                    void                    *pool_data,
                    void                    *pool_local,
                    CoreSurfaceBuffer       *buffer,
                    const CoreSurfaceConfig *config )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( config != NULL );

     return (config->flags & CSCONF_PREALLOCATED) ? DFB_OK : DFB_UNSUPPORTED;
}

static DFBResult
preallocAllocateBuffer( CoreSurfacePool       *pool,
                        void                  *pool_data,
                        void                  *pool_local,
                        CoreSurfaceBuffer     *buffer,
                        CoreSurfaceAllocation *allocation,
                        void                  *alloc_data )
{
     int                     index;
     CoreSurface            *surface;
     PreallocAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     for (index=0; index<MAX_SURFACE_BUFFERS; index++) {
          if (surface->buffers[index] == buffer)
               break;
     }

     if (index == MAX_SURFACE_BUFFERS)
          return DFB_BUG;

     if (!(surface->config.flags & CSCONF_PREALLOCATED))
          return DFB_BUG;

     if (!surface->config.preallocated[index].addr ||
          surface->config.preallocated[index].pitch < DFB_BYTES_PER_LINE(surface->config.format,
                                                                         surface->config.size.w))
          return DFB_INVARG;

     alloc->addr  = surface->config.preallocated[index].addr;
     alloc->pitch = surface->config.preallocated[index].pitch;

     allocation->flags = CSALF_PREALLOCATED | CSALF_VOLATILE;
     allocation->size  = surface->config.preallocated[index].pitch *
                         DFB_PLANE_MULTIPLY( surface->config.format, surface->config.size.h );

     return DFB_OK;
}

static DFBResult
preallocDeallocateBuffer( CoreSurfacePool       *pool,
                          void                  *pool_data,
                          void                  *pool_local,
                          CoreSurfaceBuffer     *buffer,
                          CoreSurfaceAllocation *allocation,
                          void                  *alloc_data )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     return DFB_OK;
}

static DFBResult
preallocLock( CoreSurfacePool       *pool,
              void                  *pool_data,
              void                  *pool_local,
              CoreSurfaceAllocation *allocation,
              void                  *alloc_data,
              CoreSurfaceBufferLock *lock )
{
     PreallocAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     lock->addr  = alloc->addr;
     lock->pitch = alloc->pitch;

     return DFB_OK;
}

static DFBResult
preallocUnlock( CoreSurfacePool       *pool,
                void                  *pool_data,
                void                  *pool_local,
                CoreSurfaceAllocation *allocation,
                void                  *alloc_data,
                CoreSurfaceBufferLock *lock )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     return DFB_OK;
}

const SurfacePoolFuncs preallocSurfacePoolFuncs = {
     AllocationDataSize: preallocAllocationDataSize,
     InitPool:           preallocInitPool,

     TestConfig:         preallocTestConfig,

     AllocateBuffer:     preallocAllocateBuffer,
     DeallocateBuffer:   preallocDeallocateBuffer,

     Lock:               preallocLock,
     Unlock:             preallocUnlock
};

