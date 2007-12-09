/*
   TI Davinci driver - VID1 FB Memory for direct UYVY mode

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org>

   Code is derived from VMWare driver.

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

#include <asm/types.h>

#include <direct/debug.h>
#include <direct/mem.h>

#include <core/gfxcard.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include "davincifb.h"

#include "davinci_gfxdriver.h"

D_DEBUG_DOMAIN( Video_Surfaces, "Video/Surfaces", "Video Framebuffer Surface Pool" );
D_DEBUG_DOMAIN( Video_SurfLock, "Video/SurfLock", "Video Framebuffer Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int             magic;
} VideoPoolData;

typedef struct {
     int             magic;

     CoreDFB        *core;
     void           *mem;
     unsigned long   phys;
} VideoPoolLocalData;

typedef struct {
     int   magic;

     int   offset;
     int   pitch;
     int   size;
} VideoAllocationData;

/**********************************************************************************************************************/

static int
videoPoolDataSize()
{
     return sizeof(VideoPoolData);
}

static int
videoPoolLocalDataSize()
{
     return sizeof(VideoPoolLocalData);
}

static int
videoAllocationDataSize()
{
     return sizeof(VideoAllocationData);
}

static DFBResult
videoInitPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data,
               CoreSurfacePoolDescription *ret_desc )
{
     VideoPoolData      *data  = pool_data;
     VideoPoolLocalData *local = pool_local;
     DavinciDriverData  *ddrv  = dfb_gfxcard_get_driver_data();
     DavinciDeviceData  *ddev  = dfb_gfxcard_get_device_data();

     D_DEBUG_AT( Video_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps     = CSPCAPS_NONE;
     ret_desc->access   = CSAF_CPU_READ | CSAF_CPU_WRITE | CSAF_GPU_READ | CSAF_GPU_WRITE | CSAF_SHARED;
     ret_desc->types    = CSTF_LAYER | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority = CSPP_DEFAULT;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "Video Pool" );

     local->core = core;
     local->mem  = ddrv->fb[VID1].mem;
     local->phys = ddev->fix[VID1].smem_start;

     D_MAGIC_SET( data, VideoPoolData );
     D_MAGIC_SET( local, VideoPoolLocalData );

     return DFB_OK;
}

static DFBResult
videoJoinPool( CoreDFB         *core,
               CoreSurfacePool *pool,
               void            *pool_data,
               void            *pool_local,
               void            *system_data )
{
     VideoPoolData      *data  = pool_data;
     VideoPoolLocalData *local = pool_local;
     DavinciDriverData  *ddrv  = dfb_gfxcard_get_driver_data();
     DavinciDeviceData  *ddev  = dfb_gfxcard_get_device_data();

     D_DEBUG_AT( Video_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VideoPoolData );
     D_ASSERT( local != NULL );

     (void) data;

     local->core = core;
     local->mem  = ddrv->fb[VID1].mem;
     local->phys = ddev->fix[VID1].smem_start;

     D_MAGIC_SET( local, VideoPoolLocalData );

     return DFB_OK;
}

static DFBResult
videoDestroyPool( CoreSurfacePool *pool,
                  void            *pool_data,
                  void            *pool_local )
{
     VideoPoolData      *data  = pool_data;
     VideoPoolLocalData *local = pool_local;

     D_DEBUG_AT( Video_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VideoPoolData );
     D_MAGIC_ASSERT( local, VideoPoolLocalData );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
videoLeavePool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     VideoPoolData      *data  = pool_data;
     VideoPoolLocalData *local = pool_local;

     D_DEBUG_AT( Video_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VideoPoolData );
     D_MAGIC_ASSERT( local, VideoPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
videoTestConfig( CoreSurfacePool         *pool,
                 void                    *pool_data,
                 void                    *pool_local,
                 CoreSurfaceBuffer       *buffer,
                 const CoreSurfaceConfig *config )
{
     CoreSurface        *surface;
     VideoPoolData      *data  = pool_data;
     VideoPoolLocalData *local = pool_local;

     D_DEBUG_AT( Video_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VideoPoolData );
     D_MAGIC_ASSERT( local, VideoPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     (void) data;
     (void) local;

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if ((surface->type & CSTF_LAYER) && surface->resource_id == 1)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
videoAllocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     CoreSurface         *surface;
     VideoPoolData       *data  = pool_data;
     VideoPoolLocalData  *local = pool_local;
     VideoAllocationData *alloc = alloc_data;
     DavinciDeviceData   *ddev  = dfb_gfxcard_get_device_data();

     D_DEBUG_AT( Video_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VideoPoolData );
     D_MAGIC_ASSERT( local, VideoPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     (void) data;
     (void) local;

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if ((surface->type & CSTF_LAYER) && surface->resource_id == 1) {
          int index = dfb_surface_buffer_index( buffer );

          alloc->pitch  = ddev->fix[VID1].line_length;
          alloc->size   = surface->config.size.h * alloc->pitch;
          alloc->offset = index * alloc->size;

          D_DEBUG_AT( Video_Surfaces, "  -> offset %d, pitch %d, size %d\n", alloc->offset, alloc->pitch, alloc->size );

          allocation->size   = alloc->size;
          allocation->offset = alloc->offset;

          D_MAGIC_SET( alloc, VideoAllocationData );

          return DFB_OK;
     }

     return DFB_BUG;
}

static DFBResult
videoDeallocateBuffer( CoreSurfacePool       *pool,
                       void                  *pool_data,
                       void                  *pool_local,
                       CoreSurfaceBuffer     *buffer,
                       CoreSurfaceAllocation *allocation,
                       void                  *alloc_data )
{
     VideoPoolData       *data  = pool_data;
     VideoAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Video_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VideoPoolData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, VideoAllocationData );

     (void) data;

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
videoLock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     VideoPoolLocalData  *local = pool_local;
     VideoAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, VideoAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Video_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     lock->pitch  = alloc->pitch;
     lock->offset = alloc->offset;
     lock->addr   = local->mem  + alloc->offset;
     lock->phys   = local->phys + alloc->offset;

     D_DEBUG_AT( Video_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
videoUnlock( CoreSurfacePool       *pool,
             void                  *pool_data,
             void                  *pool_local,
             CoreSurfaceAllocation *allocation,
             void                  *alloc_data,
             CoreSurfaceBufferLock *lock )
{
     VideoAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, VideoAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Video_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     return DFB_OK;
}

const SurfacePoolFuncs davinciVideoSurfacePoolFuncs = {
     PoolDataSize:       videoPoolDataSize,
     PoolLocalDataSize:  videoPoolLocalDataSize,
     AllocationDataSize: videoAllocationDataSize,

     InitPool:           videoInitPool,
     JoinPool:           videoJoinPool,
     DestroyPool:        videoDestroyPool,
     LeavePool:          videoLeavePool,

     TestConfig:         videoTestConfig,
     AllocateBuffer:     videoAllocateBuffer,
     DeallocateBuffer:   videoDeallocateBuffer,

     Lock:               videoLock,
     Unlock:             videoUnlock
};

