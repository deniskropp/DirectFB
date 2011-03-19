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


#include "pvr2d_system.h"

D_DEBUG_DOMAIN( PVR2D_Surfaces, "PVR2D/Surfaces", "PVR2D Framebuffer Surface Pool" );
D_DEBUG_DOMAIN( PVR2D_SurfLock, "PVR2D/SurfLock", "PVR2D Framebuffer Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int             magic;

} PVR2DPoolData;

typedef struct {
     int             magic;

     CoreDFB        *core;
     PVR2DData      *pvr2d;
} PVR2DPoolLocalData;

typedef struct {
     int   magic;

     int   offset;
     int   pitch;
     int   size;


     PVR2DMEMINFO *meminfo;
} PVR2DAllocationData;

/**********************************************************************************************************************/

static int
pvr2dPoolDataSize( void )
{
     return sizeof(PVR2DPoolData);
}

static int
pvr2dPoolLocalDataSize( void )
{
     return sizeof(PVR2DPoolLocalData);
}

static int
pvr2dAllocationDataSize( void )
{
     return sizeof(PVR2DAllocationData);
}

static DFBResult
pvr2dInitPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data,
               CoreSurfacePoolDescription *ret_desc )
{
     PVR2DPoolData      *data  = pool_data;
     PVR2DPoolLocalData *local = pool_local;
     PVR2DData          *pvr2d = system_data;
     PVR2DDataShared    *shared;

     D_DEBUG_AT( PVR2D_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( pvr2d != NULL );
     D_ASSERT( ret_desc != NULL );

     shared = pvr2d->shared;
     D_ASSERT( shared != NULL );

     ret_desc->caps                 = CSPCAPS_PHYSICAL | CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU]    = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_GPU]    = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_LAYER0] = CSAF_READ | CSAF_SHARED;
     ret_desc->types                = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority             = CSPP_DEFAULT;
     ret_desc->size                 = 0;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "PVR2D Pool" );

     local->core  = core;
     local->pvr2d = pvr2d;

     D_MAGIC_SET( data, PVR2DPoolData );
     D_MAGIC_SET( local, PVR2DPoolLocalData );

     return DFB_OK;
}

static DFBResult
pvr2dJoinPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data )
{
     PVR2DPoolData      *data  = pool_data;
     PVR2DPoolLocalData *local = pool_local;
     PVR2DData          *pvr2d = system_data;
     PVR2DDataShared    *shared;

     D_DEBUG_AT( PVR2D_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, PVR2DPoolData );
     D_ASSERT( local != NULL );
     D_ASSERT( pvr2d != NULL );

     shared = pvr2d->shared;
     D_ASSERT( shared != NULL );

     (void) data;

     local->core  = core;
     local->pvr2d = pvr2d;

     D_MAGIC_SET( local, PVR2DPoolLocalData );

     return DFB_OK;
}

static DFBResult
pvr2dDestroyPool( CoreSurfacePool *pool,
                  void            *pool_data,
                  void            *pool_local )
{
     PVR2DPoolData      *data  = pool_data;
     PVR2DPoolLocalData *local = pool_local;

     D_DEBUG_AT( PVR2D_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, PVR2DPoolData );
     D_MAGIC_ASSERT( local, PVR2DPoolLocalData );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
pvr2dLeavePool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     PVR2DPoolData      *data  = pool_data;
     PVR2DPoolLocalData *local = pool_local;

     D_DEBUG_AT( PVR2D_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, PVR2DPoolData );
     D_MAGIC_ASSERT( local, PVR2DPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
pvr2dTestConfig( CoreSurfacePool         *pool,
                 void                    *pool_data,
                 void                    *pool_local,
                 CoreSurfaceBuffer       *buffer,
                 const CoreSurfaceConfig *config )
{
     DFBResult           ret;
     CoreSurface        *surface;
     PVR2DPoolData      *data  = pool_data;
     PVR2DPoolLocalData *local = pool_local;

     (void) data;
     (void) local;

     D_DEBUG_AT( PVR2D_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, PVR2DPoolData );
     D_MAGIC_ASSERT( local, PVR2DPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->type & CSTF_LAYER)
          return DFB_OK;

     ret = DFB_OK;//dfb_surfacemanager_allocate( local->core, data->manager, buffer, NULL, NULL );

     D_DEBUG_AT( PVR2D_Surfaces, "  -> %s\n", DirectFBErrorString(ret) );

     return ret;
}

static DFBResult
pvr2dAllocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     CoreSurface         *surface;
     PVR2DPoolData       *data  = pool_data;
     PVR2DPoolLocalData  *local = pool_local;
     PVR2DAllocationData *alloc = alloc_data;
     PVR2DData           *pvr2d;

     (void) data;
     (void) local;

     D_DEBUG_AT( PVR2D_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, PVR2DPoolData );
     D_MAGIC_ASSERT( local, PVR2DPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     pvr2d = local->pvr2d;
     D_ASSERT( pvr2d != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     dfb_surface_calc_buffer_size( surface, 8, 1, &alloc->pitch, &alloc->size );

     if (surface->type & CSTF_LAYER) {
          alloc->meminfo = pvr2d->pFBMemInfo;
     }
     else {
          PVR2DERROR ePVR2DStatus;
     
          ePVR2DStatus = PVR2DMemAlloc( pvr2d->hPVR2DContext, alloc->size, 256, 0, &alloc->meminfo );
          if (ePVR2DStatus) {
               D_ERROR( "DirectFB/PVR2D: PVR2DMemAlloc( %d ) failed! (status %d)\n", alloc->size, ePVR2DStatus );
               return DFB_INIT;
          }
     }

     alloc->offset = 0;

     D_DEBUG_AT( PVR2D_Surfaces, "  -> offset %d, pitch %d, size %d\n", alloc->offset, alloc->pitch, alloc->size );

     allocation->size   = alloc->size;
     allocation->offset = alloc->offset;

     D_MAGIC_SET( alloc, PVR2DAllocationData );

     return DFB_OK;
}

static DFBResult
pvr2dDeallocateBuffer( CoreSurfacePool       *pool,
                       void                  *pool_data,
                       void                  *pool_local,
                       CoreSurfaceBuffer     *buffer,
                       CoreSurfaceAllocation *allocation,
                       void                  *alloc_data )
{
     CoreSurface         *surface;
     PVR2DPoolData       *data  = pool_data;
     PVR2DPoolLocalData  *local = pool_local;
     PVR2DAllocationData *alloc = alloc_data;
     PVR2DData           *pvr2d;

     (void) data;

     D_DEBUG_AT( PVR2D_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, PVR2DPoolData );
     D_MAGIC_ASSERT( local, VPSMemPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, PVR2DAllocationData );

     pvr2d = local->pvr2d;
     D_ASSERT( pvr2d != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->type & CSTF_LAYER) {
          alloc->meminfo = pvr2d->pFBMemInfo;
     }
     else {
          PVR2DERROR ePVR2DStatus;
     
          ePVR2DStatus = PVR2DMemFree( pvr2d->hPVR2DContext, alloc->meminfo );
          if (ePVR2DStatus) {
               D_ERROR( "DirectFB/PVR2D: PVR2DMemFree() failed! (status %d)\n", ePVR2DStatus );
               return DFB_INIT;
          }
     }

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
pvr2dMuckOut( CoreSurfacePool   *pool,
              void              *pool_data,
              void              *pool_local,
              CoreSurfaceBuffer *buffer )
{
     CoreSurface        *surface;
     PVR2DPoolData      *data  = pool_data;
     PVR2DPoolLocalData *local = pool_local;

     (void) data;
     (void) local;

     D_DEBUG_AT( PVR2D_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VPSMemPoolData );
     D_MAGIC_ASSERT( local, VPSMemPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     return DFB_UNSUPPORTED;//dfb_surfacemanager_displace( local->core, data->manager, buffer );
}

static DFBResult
pvr2dLock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     PVR2DPoolLocalData  *local = pool_local;
     PVR2DAllocationData *alloc = alloc_data;

     (void) local;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, PVR2DAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( PVR2D_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     lock->pitch  = alloc->pitch;
     lock->offset = alloc->offset;
     lock->addr   = alloc->meminfo->pBase;
     lock->phys   = alloc->meminfo->ui32DevAddr;
     lock->handle = alloc->meminfo;

     D_DEBUG_AT( PVR2D_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
pvr2dUnlock( CoreSurfacePool       *pool,
             void                  *pool_data,
             void                  *pool_local,
             CoreSurfaceAllocation *allocation,
             void                  *alloc_data,
             CoreSurfaceBufferLock *lock )
{
     PVR2DAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, PVR2DAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( PVR2D_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     return DFB_OK;
}

static const SurfacePoolFuncs _pvr2dSurfacePoolFuncs = {
     PoolDataSize:       pvr2dPoolDataSize,
     PoolLocalDataSize:  pvr2dPoolLocalDataSize,
     AllocationDataSize: pvr2dAllocationDataSize,

     InitPool:           pvr2dInitPool,
     JoinPool:           pvr2dJoinPool,
     DestroyPool:        pvr2dDestroyPool,
     LeavePool:          pvr2dLeavePool,

     TestConfig:         pvr2dTestConfig,
     AllocateBuffer:     pvr2dAllocateBuffer,
     DeallocateBuffer:   pvr2dDeallocateBuffer,
     MuckOut:            pvr2dMuckOut,

     Lock:               pvr2dLock,
     Unlock:             pvr2dUnlock
};

const SurfacePoolFuncs *pvr2dSurfacePoolFuncs = &_pvr2dSurfacePoolFuncs;

