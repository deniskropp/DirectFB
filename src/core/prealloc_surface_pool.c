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

D_DEBUG_DOMAIN( Core_PreAlloc, "Core/PreAlloc", "Core PreAlloc Surface Pool" );

/**********************************************************************************************************************/

static int
preallocAllocationDataSize( void )
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
     D_DEBUG_AT( Core_PreAlloc, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_NONE;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE;
     ret_desc->types             = CSTF_PREALLOCATED | CSTF_INTERNAL;
     ret_desc->priority          = CSPP_DEFAULT;

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
     D_DEBUG_AT( Core_PreAlloc, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( config != NULL );

     if (!(config->flags & CSCONF_PREALLOCATED))
          return DFB_UNSUPPORTED;

     if (Core_GetIdentity() != buffer->surface->object.identity)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
preallocPrealloc( CoreSurfacePool             *pool,
                  void                        *pool_data,
                  void                        *pool_local,
                  const DFBSurfaceDescription *description,
                  CoreSurfaceConfig           *config )
{
     unsigned int i, num = 1;

     D_DEBUG_AT( Core_PreAlloc, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     if (config->caps & DSCAPS_VIDEOONLY)
          return DFB_UNSUPPORTED;

     if (config->caps & DSCAPS_DOUBLE)
          num = 2;
     else if (config->caps & DSCAPS_TRIPLE)
          num = 3;

     for (i=0; i<num; i++) {
          config->preallocated[i].addr  = description->preallocated[i].data;
          config->preallocated[i].pitch = description->preallocated[i].pitch;
     }

     return DFB_OK;
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
     PreallocAllocationData *alloc    = alloc_data;
     FusionID                identity = Core_GetIdentity();

     D_DEBUG_AT( Core_PreAlloc, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_PreAlloc, "  -> surface identity %lu\n", surface->object.identity );
     D_DEBUG_AT( Core_PreAlloc, "  -> core identity    %lu\n", identity );

     if (!(surface->config.flags & CSCONF_PREALLOCATED))
          return DFB_BUG;

     if (identity != surface->object.identity) {
          D_ERROR( "DirectFB/PreAlloc: Cannot allocate buffer for other (%lu) than creator (%lu)!\n", identity, surface->object.identity );
          return DFB_ACCESSDENIED;
     }

     index = dfb_surface_buffer_index( buffer );

     if (!surface->config.preallocated[index].addr ||
          surface->config.preallocated[index].pitch < DFB_BYTES_PER_LINE(surface->config.format,
                                                                         surface->config.size.w))
          return DFB_BUG;

     alloc->addr  = surface->config.preallocated[index].addr;
     alloc->pitch = surface->config.preallocated[index].pitch;

     allocation->flags = CSALF_PREALLOCATED;
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
     D_DEBUG_AT( Core_PreAlloc, "%s()\n", __FUNCTION__ );

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
     CoreSurface            *surface;
     PreallocAllocationData *alloc    = alloc_data;
     FusionID                identity = Core_GetIdentity();

     D_DEBUG_AT( Core_PreAlloc, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (identity != surface->object.identity) {
          D_ERROR( "DirectFB/PreAlloc: Cannot lock buffer by other (%lu) than creator (%lu)!\n", identity, surface->object.identity );
          return DFB_ACCESSDENIED;
     }

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
     D_DEBUG_AT( Core_PreAlloc, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     return DFB_OK;
}

const SurfacePoolFuncs preallocSurfacePoolFuncs = {
     .AllocationDataSize = preallocAllocationDataSize,
     .InitPool           = preallocInitPool,

     .TestConfig         = preallocTestConfig,

     .PreAlloc           = preallocPrealloc,
     .AllocateBuffer     = preallocAllocateBuffer,
     .DeallocateBuffer   = preallocDeallocateBuffer,

     .Lock               = preallocLock,
     .Unlock             = preallocUnlock,
};

