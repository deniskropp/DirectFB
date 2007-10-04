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
#include <direct/mem.h>

#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include "devmem.h"
#include "surfacemanager.h"

D_DEBUG_DOMAIN( DevMem_Surfaces, "DevMem/Surfaces", "DevMem Framebuffer Surface Pool" );
D_DEBUG_DOMAIN( DevMem_SurfLock, "DevMem/SurfLock", "DevMem Framebuffer Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int             magic;

     SurfaceManager *manager;
} DevMemPoolData;

typedef struct {
     int             magic;

     CoreDFB        *core;
     void           *mem;
} DevMemPoolLocalData;

typedef struct {
     int   magic;

     int   offset;
     int   pitch;
     int   size;

     Chunk *chunk;
} DevMemAllocationData;

/**********************************************************************************************************************/

static int
devmemPoolDataSize()
{
     return sizeof(DevMemPoolData);
}

static int
devmemPoolLocalDataSize()
{
     return sizeof(DevMemPoolLocalData);
}

static int
devmemAllocationDataSize()
{
     return sizeof(DevMemAllocationData);
}

static DFBResult
devmemInitPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data,
               CoreSurfacePoolDescription *ret_desc )
{
     DFBResult            ret;
     DevMemPoolData      *data   = pool_data;
     DevMemPoolLocalData *local  = pool_local;
     DevMemData          *devmem = system_data;

     D_DEBUG_AT( DevMem_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( devmem != NULL );
     D_ASSERT( devmem->shared != NULL );
     D_ASSERT( ret_desc != NULL );

     ret = dfb_surfacemanager_create( core, dfb_config->video_length, &data->manager );
     if (ret)
          return ret;

     ret_desc->caps     = CSPCAPS_NONE;
     ret_desc->access   = CSAF_CPU_READ | CSAF_CPU_WRITE | CSAF_GPU_READ | CSAF_GPU_WRITE | CSAF_SHARED;
     ret_desc->types    = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority = CSPP_DEFAULT;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "/dev/mem" );

     local->core = core;
     local->mem  = devmem->mem;

     D_MAGIC_SET( data, DevMemPoolData );
     D_MAGIC_SET( local, DevMemPoolLocalData );

     devmem->shared->manager = data->manager;

     return DFB_OK;
}

static DFBResult
devmemJoinPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data )
{
     DevMemPoolData      *data   = pool_data;
     DevMemPoolLocalData *local  = pool_local;
     DevMemData          *devmem = system_data;

     D_DEBUG_AT( DevMem_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DevMemPoolData );
     D_ASSERT( local != NULL );
     D_ASSERT( devmem != NULL );
     D_ASSERT( devmem->shared != NULL );

     (void) data;

     local->core = core;
     local->mem  = devmem->mem;

     D_MAGIC_SET( local, DevMemPoolLocalData );

     return DFB_OK;
}

static DFBResult
devmemDestroyPool( CoreSurfacePool *pool,
                  void            *pool_data,
                  void            *pool_local )
{
     DevMemPoolData      *data  = pool_data;
     DevMemPoolLocalData *local = pool_local;

     D_DEBUG_AT( DevMem_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DevMemPoolData );
     D_MAGIC_ASSERT( local, DevMemPoolLocalData );

     dfb_surfacemanager_destroy( data->manager );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
devmemLeavePool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     DevMemPoolData      *data  = pool_data;
     DevMemPoolLocalData *local = pool_local;

     D_DEBUG_AT( DevMem_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DevMemPoolData );
     D_MAGIC_ASSERT( local, DevMemPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
devmemTestConfig( CoreSurfacePool         *pool,
                 void                    *pool_data,
                 void                    *pool_local,
                 CoreSurfaceBuffer       *buffer,
                 const CoreSurfaceConfig *config )
{
     DFBResult           ret;
     CoreSurface        *surface;
     DevMemPoolData      *data  = pool_data;
     DevMemPoolLocalData *local = pool_local;

     D_DEBUG_AT( DevMem_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DevMemPoolData );
     D_MAGIC_ASSERT( local, DevMemPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->type & CSTF_LAYER)
          return DFB_OK;

     ret = dfb_surfacemanager_allocate( local->core, data->manager, buffer, NULL );

     D_DEBUG_AT( DevMem_Surfaces, "  -> %s\n", DirectFBErrorString(ret) );

     return ret;
}

static DFBResult
devmemAllocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     DFBResult             ret;
     Chunk                *chunk;
     CoreSurface          *surface;
     DevMemPoolData       *data  = pool_data;
     DevMemPoolLocalData  *local = pool_local;
     DevMemAllocationData *alloc = alloc_data;

     D_DEBUG_AT( DevMem_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DevMemPoolData );
     D_MAGIC_ASSERT( local, DevMemPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     ret = dfb_surfacemanager_allocate( local->core, data->manager, buffer, &chunk );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( chunk, Chunk );

     alloc->offset = chunk->offset;
     alloc->pitch  = chunk->pitch;
     alloc->size   = surface->config.size.h * alloc->pitch;

     alloc->chunk  = chunk;

     D_DEBUG_AT( DevMem_Surfaces, "  -> offset %d, pitch %d, size %d\n", alloc->offset, alloc->pitch, alloc->size );

     allocation->size   = alloc->size;
     allocation->offset = alloc->offset;

     D_MAGIC_SET( alloc, DevMemAllocationData );

     return DFB_OK;
}

static DFBResult
devmemDeallocateBuffer( CoreSurfacePool       *pool,
                       void                  *pool_data,
                       void                  *pool_local,
                       CoreSurfaceBuffer     *buffer,
                       CoreSurfaceAllocation *allocation,
                       void                  *alloc_data )
{
     DevMemPoolData       *data  = pool_data;
     DevMemAllocationData *alloc = alloc_data;

     D_DEBUG_AT( DevMem_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DevMemPoolData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, DevMemAllocationData );

     if (alloc->chunk)
          dfb_surfacemanager_deallocate( data->manager, alloc->chunk );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
devmemLock( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            CoreSurfaceBufferLock *lock )
{
     DevMemPoolLocalData  *local = pool_local;
     DevMemAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, DevMemAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( DevMem_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     lock->pitch  = alloc->pitch;
     lock->offset = alloc->offset;
     lock->addr   = local->mem + alloc->offset;
     lock->phys   = dfb_config->video_phys + alloc->offset;

     D_DEBUG_AT( DevMem_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
devmemUnlock( CoreSurfacePool       *pool,
             void                  *pool_data,
             void                  *pool_local,
             CoreSurfaceAllocation *allocation,
             void                  *alloc_data,
             CoreSurfaceBufferLock *lock )
{
     DevMemAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, DevMemAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( DevMem_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     return DFB_OK;
}

const SurfacePoolFuncs devmemSurfacePoolFuncs = {
     PoolDataSize:       devmemPoolDataSize,
     PoolLocalDataSize:  devmemPoolLocalDataSize,
     AllocationDataSize: devmemAllocationDataSize,

     InitPool:           devmemInitPool,
     JoinPool:           devmemJoinPool,
     DestroyPool:        devmemDestroyPool,
     LeavePool:          devmemLeavePool,

     TestConfig:         devmemTestConfig,
     AllocateBuffer:     devmemAllocateBuffer,
     DeallocateBuffer:   devmemDeallocateBuffer,

     Lock:               devmemLock,
     Unlock:             devmemUnlock
};

