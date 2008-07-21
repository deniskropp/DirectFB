/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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
#include <direct/mem.h>

#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include "x11.h"
#include "surfacemanager.h"

D_DEBUG_DOMAIN( VPSMem_Surfaces, "VPSMem/Surfaces", "VPSMem Framebuffer Surface Pool" );
D_DEBUG_DOMAIN( VPSMem_SurfLock, "VPSMem/SurfLock", "VPSMem Framebuffer Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int             magic;

     SurfaceManager *manager;

     void           *mem;
     unsigned int    length;
} VPSMemPoolData;

typedef struct {
     int             magic;

     CoreDFB        *core;
} VPSMemPoolLocalData;

typedef struct {
     int   magic;

     int   offset;
     int   pitch;
     int   size;

     Chunk *chunk;
} VPSMemAllocationData;

/**********************************************************************************************************************/

static int
vpsmemPoolDataSize()
{
     return sizeof(VPSMemPoolData);
}

static int
vpsmemPoolLocalDataSize()
{
     return sizeof(VPSMemPoolLocalData);
}

static int
vpsmemAllocationDataSize()
{
     return sizeof(VPSMemAllocationData);
}

static DFBResult
vpsmemInitPool( CoreDFB                    *core,
                CoreSurfacePool            *pool,
                void                       *pool_data,
                void                       *pool_local,
                void                       *system_data,
                CoreSurfacePoolDescription *ret_desc )
{
     DFBResult            ret;
     VPSMemPoolData      *data  = pool_data;
     VPSMemPoolLocalData *local = pool_local;

     D_DEBUG_AT( VPSMem_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( ret_desc != NULL );

     data->mem = SHMALLOC( dfb_x11->data_shmpool, dfb_x11->vpsmem_length );
     if (!data->mem) {
          dfb_x11->vpsmem_length = 0;
          return D_OOSHM();
     }

     data->length = dfb_x11->vpsmem_length;

     ret = dfb_surfacemanager_create( core, data->length, &data->manager );
     if (ret)
          return ret;

     ret_desc->caps     = CSPCAPS_NONE;
     ret_desc->access   = CSAF_CPU_READ | CSAF_CPU_WRITE | CSAF_GPU_READ | CSAF_GPU_WRITE | CSAF_SHARED;
     ret_desc->types    = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority = CSPP_DEFAULT;
     ret_desc->size     = data->length;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "Virtual Physical" );

     local->core = core;

     D_MAGIC_SET( data, VPSMemPoolData );
     D_MAGIC_SET( local, VPSMemPoolLocalData );

     return DFB_OK;
}

static DFBResult
vpsmemJoinPool( CoreDFB                    *core,
                CoreSurfacePool            *pool,
                void                       *pool_data,
                void                       *pool_local,
                void                       *system_data )
{
     VPSMemPoolData      *data  = pool_data;
     VPSMemPoolLocalData *local = pool_local;

     D_DEBUG_AT( VPSMem_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VPSMemPoolData );
     D_ASSERT( local != NULL );

     (void) data;

     local->core = core;

     D_MAGIC_SET( local, VPSMemPoolLocalData );

     return DFB_OK;
}

static DFBResult
vpsmemDestroyPool( CoreSurfacePool *pool,
                   void            *pool_data,
                   void            *pool_local )
{
     VPSMemPoolData      *data  = pool_data;
     VPSMemPoolLocalData *local = pool_local;

     D_DEBUG_AT( VPSMem_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VPSMemPoolData );
     D_MAGIC_ASSERT( local, VPSMemPoolLocalData );

     dfb_surfacemanager_destroy( data->manager );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
vpsmemLeavePool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     VPSMemPoolData      *data  = pool_data;
     VPSMemPoolLocalData *local = pool_local;

     D_DEBUG_AT( VPSMem_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VPSMemPoolData );
     D_MAGIC_ASSERT( local, VPSMemPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
vpsmemTestConfig( CoreSurfacePool         *pool,
                  void                    *pool_data,
                  void                    *pool_local,
                  CoreSurfaceBuffer       *buffer,
                  const CoreSurfaceConfig *config )
{
     DFBResult            ret;
     CoreSurface         *surface;
     VPSMemPoolData      *data  = pool_data;
     VPSMemPoolLocalData *local = pool_local;

     D_DEBUG_AT( VPSMem_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VPSMemPoolData );
     D_MAGIC_ASSERT( local, VPSMemPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     ret = dfb_surfacemanager_allocate( local->core, data->manager, buffer, NULL, NULL );

     D_DEBUG_AT( VPSMem_Surfaces, "  -> %s\n", DirectFBErrorString(ret) );

     return ret;
}

static DFBResult
vpsmemAllocateBuffer( CoreSurfacePool       *pool,
                      void                  *pool_data,
                      void                  *pool_local,
                      CoreSurfaceBuffer     *buffer,
                      CoreSurfaceAllocation *allocation,
                      void                  *alloc_data )
{
     DFBResult             ret;
     Chunk                *chunk;
     CoreSurface          *surface;
     VPSMemPoolData       *data  = pool_data;
     VPSMemPoolLocalData  *local = pool_local;
     VPSMemAllocationData *alloc = alloc_data;

     D_DEBUG_AT( VPSMem_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VPSMemPoolData );
     D_MAGIC_ASSERT( local, VPSMemPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     ret = dfb_surfacemanager_allocate( local->core, data->manager, buffer, allocation, &chunk );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( chunk, Chunk );

     alloc->offset = chunk->offset;
     alloc->pitch  = chunk->pitch;
     alloc->size   = surface->config.size.h * alloc->pitch;

     alloc->chunk  = chunk;

     D_DEBUG_AT( VPSMem_Surfaces, "  -> offset %d, pitch %d, size %d\n", alloc->offset, alloc->pitch, alloc->size );

     allocation->size   = alloc->size;
     allocation->offset = alloc->offset;

     D_MAGIC_SET( alloc, VPSMemAllocationData );

     return DFB_OK;
}

static DFBResult
vpsmemDeallocateBuffer( CoreSurfacePool       *pool,
                        void                  *pool_data,
                        void                  *pool_local,
                        CoreSurfaceBuffer     *buffer,
                        CoreSurfaceAllocation *allocation,
                        void                  *alloc_data )
{
     VPSMemPoolData       *data  = pool_data;
     VPSMemAllocationData *alloc = alloc_data;

     D_DEBUG_AT( VPSMem_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VPSMemPoolData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, VPSMemAllocationData );

     dfb_surfacemanager_deallocate( data->manager, alloc->chunk );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
vpsmemMuckOut( CoreSurfacePool   *pool,
               void              *pool_data,
               void              *pool_local,
               CoreSurfaceBuffer *buffer )
{
     CoreSurface           *surface;
     VPSMemPoolData        *data  = pool_data;
     VPSMemPoolLocalData   *local = pool_local;

     D_DEBUG_AT( VPSMem_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VPSMemPoolData );
     D_MAGIC_ASSERT( local, VPSMemPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     return dfb_surfacemanager_displace( local->core, data->manager, buffer );
}

static DFBResult
vpsmemLock( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            CoreSurfaceBufferLock *lock )
{
     VPSMemPoolData       *data  = pool_data;
     VPSMemAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VPSMemPoolData );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, VPSMemAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( VPSMem_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     lock->pitch  = alloc->pitch;
     lock->offset = alloc->offset;
     lock->addr   = data->mem + alloc->offset;
     lock->phys   = dfb_config->video_phys + alloc->offset;

     D_DEBUG_AT( VPSMem_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
vpsmemUnlock( CoreSurfacePool       *pool,
              void                  *pool_data,
              void                  *pool_local,
              CoreSurfaceAllocation *allocation,
              void                  *alloc_data,
              CoreSurfaceBufferLock *lock )
{
     VPSMemAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, VPSMemAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( VPSMem_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     return DFB_OK;
}

const SurfacePoolFuncs vpsmemSurfacePoolFuncs = {
     PoolDataSize:       vpsmemPoolDataSize,
     PoolLocalDataSize:  vpsmemPoolLocalDataSize,
     AllocationDataSize: vpsmemAllocationDataSize,

     InitPool:           vpsmemInitPool,
     JoinPool:           vpsmemJoinPool,
     DestroyPool:        vpsmemDestroyPool,
     LeavePool:          vpsmemLeavePool,

     TestConfig:         vpsmemTestConfig,
     AllocateBuffer:     vpsmemAllocateBuffer,
     DeallocateBuffer:   vpsmemDeallocateBuffer,

     MuckOut:            vpsmemMuckOut,

     Lock:               vpsmemLock,
     Unlock:             vpsmemUnlock
};

