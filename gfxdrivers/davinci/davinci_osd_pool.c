/*
   TI Davinci driver - OSD0 FB Memory for direct RGB16 mode

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

D_DEBUG_DOMAIN( OSD_Surfaces, "OSD/Surfaces", "OSD Framebuffer Surface Pool" );
D_DEBUG_DOMAIN( OSD_SurfLock, "OSD/SurfLock", "OSD Framebuffer Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int             magic;
} OSDPoolData;

typedef struct {
     int             magic;

     CoreDFB        *core;
     void           *mem;
     unsigned long   phys;
} OSDPoolLocalData;

typedef struct {
     int   magic;

     int   offset;
     int   pitch;
     int   size;
} OSDAllocationData;

/**********************************************************************************************************************/

static int
osdPoolDataSize()
{
     return sizeof(OSDPoolData);
}

static int
osdPoolLocalDataSize()
{
     return sizeof(OSDPoolLocalData);
}

static int
osdAllocationDataSize()
{
     return sizeof(OSDAllocationData);
}

static DFBResult
osdInitPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data,
             CoreSurfacePoolDescription *ret_desc )
{
     OSDPoolData       *data  = pool_data;
     OSDPoolLocalData  *local = pool_local;
     DavinciDriverData *ddrv  = dfb_gfxcard_get_driver_data();
     DavinciDeviceData *ddev  = dfb_gfxcard_get_device_data();

     D_DEBUG_AT( OSD_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps     = CSPCAPS_NONE;
     ret_desc->access   = CSAF_CPU_READ | CSAF_CPU_WRITE | CSAF_GPU_READ | CSAF_GPU_WRITE | CSAF_SHARED;
     ret_desc->types    = CSTF_LAYER | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority = CSPP_DEFAULT;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "OSD Pool" );

     local->core = core;
     local->mem  = ddrv->fb[OSD0].mem;
     local->phys = ddev->fix[OSD0].smem_start;

     D_MAGIC_SET( data, OSDPoolData );
     D_MAGIC_SET( local, OSDPoolLocalData );

     return DFB_OK;
}

static DFBResult
osdJoinPool( CoreDFB         *core,
             CoreSurfacePool *pool,
             void            *pool_data,
             void            *pool_local,
             void            *system_data )
{
     OSDPoolData       *data  = pool_data;
     OSDPoolLocalData  *local = pool_local;
     DavinciDriverData *ddrv  = dfb_gfxcard_get_driver_data();
     DavinciDeviceData *ddev  = dfb_gfxcard_get_device_data();

     D_DEBUG_AT( OSD_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, OSDPoolData );
     D_ASSERT( local != NULL );

     (void) data;

     local->core = core;
     local->mem  = ddrv->fb[OSD0].mem;
     local->phys = ddev->fix[OSD0].smem_start;

     D_MAGIC_SET( local, OSDPoolLocalData );

     return DFB_OK;
}

static DFBResult
osdDestroyPool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     OSDPoolData      *data  = pool_data;
     OSDPoolLocalData *local = pool_local;

     D_DEBUG_AT( OSD_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, OSDPoolData );
     D_MAGIC_ASSERT( local, OSDPoolLocalData );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
osdLeavePool( CoreSurfacePool *pool,
              void            *pool_data,
              void            *pool_local )
{
     OSDPoolData      *data  = pool_data;
     OSDPoolLocalData *local = pool_local;

     D_DEBUG_AT( OSD_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, OSDPoolData );
     D_MAGIC_ASSERT( local, OSDPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
osdTestConfig( CoreSurfacePool         *pool,
               void                    *pool_data,
               void                    *pool_local,
               CoreSurfaceBuffer       *buffer,
               const CoreSurfaceConfig *config )
{
     CoreSurface      *surface;
     OSDPoolData      *data  = pool_data;
     OSDPoolLocalData *local = pool_local;

     D_DEBUG_AT( OSD_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, OSDPoolData );
     D_MAGIC_ASSERT( local, OSDPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     (void) data;
     (void) local;

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if ((surface->type & CSTF_LAYER) && surface->resource_id == DLID_PRIMARY && surface->config.format == DSPF_RGB16)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
osdAllocateBuffer( CoreSurfacePool       *pool,
                   void                  *pool_data,
                   void                  *pool_local,
                   CoreSurfaceBuffer     *buffer,
                   CoreSurfaceAllocation *allocation,
                   void                  *alloc_data )
{
     CoreSurface       *surface;
     OSDPoolData       *data  = pool_data;
     OSDPoolLocalData  *local = pool_local;
     OSDAllocationData *alloc = alloc_data;
     DavinciDeviceData *ddev  = dfb_gfxcard_get_device_data();

     D_DEBUG_AT( OSD_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, OSDPoolData );
     D_MAGIC_ASSERT( local, OSDPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     (void) data;
     (void) local;

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if ((surface->type & CSTF_LAYER) && surface->resource_id == DLID_PRIMARY && surface->config.format == DSPF_RGB16) {
          int index = dfb_surface_buffer_index( buffer );

          alloc->pitch  = ddev->fix[OSD0].line_length;
          alloc->size   = surface->config.size.h * alloc->pitch;
          alloc->offset = index * alloc->size;

          D_DEBUG_AT( OSD_Surfaces, "  -> offset %d, pitch %d, size %d\n", alloc->offset, alloc->pitch, alloc->size );

          allocation->size   = alloc->size;
          allocation->offset = alloc->offset;

          D_MAGIC_SET( alloc, OSDAllocationData );

          return DFB_OK;
     }

     return DFB_BUG;
}

static DFBResult
osdDeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     OSDPoolData       *data  = pool_data;
     OSDAllocationData *alloc = alloc_data;

     D_DEBUG_AT( OSD_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, OSDPoolData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, OSDAllocationData );

     (void) data;

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
osdLock( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         CoreSurfaceBufferLock *lock )
{
     OSDPoolLocalData  *local = pool_local;
     OSDAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, OSDAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( OSD_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     lock->pitch  = alloc->pitch;
     lock->offset = alloc->offset;
     lock->addr   = local->mem  + alloc->offset;
     lock->phys   = local->phys + alloc->offset;

     D_DEBUG_AT( OSD_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
osdUnlock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     OSDAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, OSDAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( OSD_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     return DFB_OK;
}

const SurfacePoolFuncs davinciOSDSurfacePoolFuncs = {
     PoolDataSize:       osdPoolDataSize,
     PoolLocalDataSize:  osdPoolLocalDataSize,
     AllocationDataSize: osdAllocationDataSize,

     InitPool:           osdInitPool,
     JoinPool:           osdJoinPool,
     DestroyPool:        osdDestroyPool,
     LeavePool:          osdLeavePool,

     TestConfig:         osdTestConfig,
     AllocateBuffer:     osdAllocateBuffer,
     DeallocateBuffer:   osdDeallocateBuffer,

     Lock:               osdLock,
     Unlock:             osdUnlock
};

