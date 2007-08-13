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

#include <asm/types.h>

#include <config.h>

#include <direct/debug.h>
#include <direct/mem.h>

#include <core/surface_pool.h>

#include <gfx/convert.h>

#include "fbdev.h"
#include "surfacemanager.h"

extern FBDev *dfb_fbdev;

D_DEBUG_DOMAIN( FBDev_Surfaces, "FBDev/Surfaces", "FBDev Framebuffer Surface Pool" );
D_DEBUG_DOMAIN( FBDev_SurfLock, "FBDev/SurfLock", "FBDev Framebuffer Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int             magic;

     SurfaceManager *manager;
} FBDevPoolData;

typedef struct {
     int             magic;

     CoreDFB        *core;
} FBDevPoolLocalData;

typedef struct {
     int   magic;

     int   offset;
     int   pitch;
     int   size;

     Chunk *chunk;
} FBDevAllocationData;

/**********************************************************************************************************************/

static int
fbdevPoolDataSize()
{
     return sizeof(FBDevPoolData);
}

static int
fbdevPoolLocalDataSize()
{
     return sizeof(FBDevPoolLocalData);
}

static int
fbdevAllocationDataSize()
{
     return sizeof(FBDevAllocationData);
}

static DFBResult
fbdevInitPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data,
               CoreSurfacePoolDescription *ret_desc )
{
     DFBResult           ret;
     FBDevPoolData      *data  = pool_data;
     FBDevPoolLocalData *local = pool_local;

     D_DEBUG_AT( FBDev_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( ret_desc != NULL );

     ret = dfb_surfacemanager_create( core, dfb_fbdev->shared->fix.smem_len, &data->manager );
     if (ret)
          return ret;

     ret_desc->caps     = CSPCAPS_NONE;
     ret_desc->access   = CSAF_CPU_READ | CSAF_CPU_WRITE | CSAF_GPU_READ | CSAF_GPU_WRITE | CSAF_SHARED;
     ret_desc->types    = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority = CSPP_DEFAULT;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "Frame Buffer Memory" );

     local->core = core;

     D_MAGIC_SET( data, FBDevPoolData );
     D_MAGIC_SET( local, FBDevPoolLocalData );


     D_ASSERT( dfb_fbdev != NULL );
     D_ASSERT( dfb_fbdev->shared != NULL );

     dfb_fbdev->shared->manager = data->manager;

     return DFB_OK;
}

static DFBResult
fbdevJoinPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data )
{
     FBDevPoolData      *data  = pool_data;
     FBDevPoolLocalData *local = pool_local;

     D_DEBUG_AT( FBDev_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBDevPoolData );
     D_ASSERT( local != NULL );

     (void) data;

     local->core = core;

     D_MAGIC_SET( local, FBDevPoolLocalData );

     return DFB_OK;
}

static DFBResult
fbdevDestroyPool( CoreSurfacePool *pool,
                  void            *pool_data,
                  void            *pool_local )
{
     FBDevPoolData      *data  = pool_data;
     FBDevPoolLocalData *local = pool_local;

     D_DEBUG_AT( FBDev_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBDevPoolData );
     D_MAGIC_ASSERT( local, FBDevPoolLocalData );

     dfb_surfacemanager_destroy( data->manager );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
fbdevLeavePool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     FBDevPoolData      *data  = pool_data;
     FBDevPoolLocalData *local = pool_local;

     D_DEBUG_AT( FBDev_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBDevPoolData );
     D_MAGIC_ASSERT( local, FBDevPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
fbdevTestConfig( CoreSurfacePool         *pool,
                 void                    *pool_data,
                 void                    *pool_local,
                 CoreSurfaceBuffer       *buffer,
                 const CoreSurfaceConfig *config )
{
     DFBResult           ret;
     CoreSurface        *surface;
     FBDevPoolData      *data  = pool_data;
     FBDevPoolLocalData *local = pool_local;

     D_DEBUG_AT( FBDev_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBDevPoolData );
     D_MAGIC_ASSERT( local, FBDevPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->type & CSTF_LAYER)
          return DFB_OK;

     ret = dfb_surfacemanager_allocate( local->core, data->manager, buffer, NULL );

     D_DEBUG_AT( FBDev_Surfaces, "  -> %s\n", DirectFBErrorString(ret) );

     return ret;
}

static DFBResult
fbdevAllocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     DFBResult            ret;
     CoreSurface         *surface;
     FBDevPoolData       *data  = pool_data;
     FBDevPoolLocalData  *local = pool_local;
     FBDevAllocationData *alloc = alloc_data;

     D_DEBUG_AT( FBDev_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBDevPoolData );
     D_MAGIC_ASSERT( local, FBDevPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if ((surface->type & CSTF_LAYER) && surface->resource_id == DLID_PRIMARY) {
          int i, index = dfb_surface_buffer_index( buffer );

          /* HACK FIXME_SC_2 ALLOCATE/SETMODE TWIST */
          for (i=0; i<surface->num_buffers; i++) {
               if (surface->buffers[i]->allocs.elements)
                    break;
          }

          if (i == surface->num_buffers && dfb_fbdev->shared->test_mode) {
               ret = dfb_fbdev_set_mode( surface, dfb_fbdev->shared->test_mode, &dfb_fbdev->shared->test_config );
               if (ret)
                    return ret;

               dfb_fbdev->shared->test_mode = NULL;
          }
          /* /HACK FIXME_SC_2 ALLOCATE/SETMODE TWIST */

          alloc->pitch  = dfb_fbdev->shared->fix.line_length;
          alloc->size   = surface->config.size.h * alloc->pitch;
          alloc->offset = index * alloc->size;

          D_INFO( "FBDev/Surface: Allocated %dx%d %dbit %s buffer at offset %d and pitch %d.\n",
                  surface->config.size.w, surface->config.size.h, dfb_fbdev->shared->current_var.bits_per_pixel,
                  dfb_pixelformat_name(buffer->format), alloc->offset, alloc->pitch );
     }
     else {
          Chunk *chunk;

          ret = dfb_surfacemanager_allocate( local->core, data->manager, buffer, &chunk );
          if (ret)
               return ret;

          D_MAGIC_ASSERT( chunk, Chunk );

          alloc->offset = chunk->offset;
          alloc->pitch  = chunk->pitch;
          alloc->size   = surface->config.size.h * alloc->pitch;

          alloc->chunk  = chunk;
     }

     D_DEBUG_AT( FBDev_Surfaces, "  -> offset %d, pitch %d, size %d\n", alloc->offset, alloc->pitch, alloc->size );

     allocation->size   = alloc->size;
     allocation->offset = alloc->offset;

     D_MAGIC_SET( alloc, FBDevAllocationData );

     return DFB_OK;
}

static DFBResult
fbdevDeallocateBuffer( CoreSurfacePool       *pool,
                       void                  *pool_data,
                       void                  *pool_local,
                       CoreSurfaceBuffer     *buffer,
                       CoreSurfaceAllocation *allocation,
                       void                  *alloc_data )
{
     FBDevPoolData       *data  = pool_data;
     FBDevAllocationData *alloc = alloc_data;

     D_DEBUG_AT( FBDev_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBDevPoolData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, FBDevAllocationData );

     if (alloc->chunk)
          dfb_surfacemanager_deallocate( data->manager, alloc->chunk );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
fbdevLock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     FBDevAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, FBDevAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( FBDev_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     lock->pitch  = alloc->pitch;
     lock->offset = alloc->offset;
     lock->addr   = dfb_fbdev->framebuffer_base + alloc->offset;
     lock->phys   = dfb_fbdev->shared->fix.smem_start + alloc->offset;

     D_DEBUG_AT( FBDev_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
fbdevUnlock( CoreSurfacePool       *pool,
             void                  *pool_data,
             void                  *pool_local,
             CoreSurfaceAllocation *allocation,
             void                  *alloc_data,
             CoreSurfaceBufferLock *lock )
{
     FBDevAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, FBDevAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( FBDev_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     return DFB_OK;
}

const SurfacePoolFuncs fbdevSurfacePoolFuncs = {
     PoolDataSize:       fbdevPoolDataSize,
     PoolLocalDataSize:  fbdevPoolLocalDataSize,
     AllocationDataSize: fbdevAllocationDataSize,

     InitPool:           fbdevInitPool,
     JoinPool:           fbdevJoinPool,
     DestroyPool:        fbdevDestroyPool,
     LeavePool:          fbdevLeavePool,

     TestConfig:         fbdevTestConfig,
     AllocateBuffer:     fbdevAllocateBuffer,
     DeallocateBuffer:   fbdevDeallocateBuffer,

     Lock:               fbdevLock,
     Unlock:             fbdevUnlock
};

