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


/**********************************************************************************************************************/

typedef struct {
     FusionSHMPoolShared *shmpool;
} SharedPoolData;

typedef struct {
     CoreDFB     *core;
     FusionWorld *world;
} SharedPoolLocalData;

typedef struct {
     void *addr;
     int   pitch;
     int   size;
} SharedAllocationData;

/**********************************************************************************************************************/

static int
sharedPoolDataSize()
{
     return sizeof(SharedPoolData);
}

static int
sharedPoolLocalDataSize()
{
     return sizeof(SharedPoolLocalData);
}

static int
sharedAllocationDataSize()
{
     return sizeof(SharedAllocationData);
}

static DFBResult
sharedInitPool( CoreDFB                    *core,
                CoreSurfacePool            *pool,
                void                       *pool_data,
                void                       *pool_local,
                void                       *system_data,
                CoreSurfacePoolDescription *ret_desc )
{
     DFBResult            ret;
     SharedPoolData      *data  = pool_data;
     SharedPoolLocalData *local = pool_local;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     local->core  = core;
     local->world = dfb_core_world( core );

     ret = fusion_shm_pool_create( local->world, "Surface Memory Pool", 0x6000000,
                                   fusion_config->debugshm, &data->shmpool );
     if (ret)
          return ret;

     ret_desc->caps     = CSPCAPS_NONE;
     ret_desc->access   = CSAF_CPU_READ | CSAF_CPU_WRITE | CSAF_SHARED;
     ret_desc->types    = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_INTERNAL;
     ret_desc->priority = CSPP_DEFAULT;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "Shared Memory" );

     return DFB_OK;
}

static DFBResult
sharedDestroyPool( CoreSurfacePool *pool,
                   void            *pool_data,
                   void            *pool_local )
{
     SharedPoolData      *data  = pool_data;
     SharedPoolLocalData *local = pool_local;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     fusion_shm_pool_destroy( local->world, data->shmpool );

     return DFB_OK;
}

static DFBResult
sharedAllocateBuffer( CoreSurfacePool       *pool,
                      void                  *pool_data,
                      void                  *pool_local,
                      CoreSurfaceBuffer     *buffer,
                      CoreSurfaceAllocation *allocation,
                      void                  *alloc_data )
{
     CoreSurface          *surface;
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;

     D_MAGIC_ASSERT( surface, CoreSurface );

     dfb_surface_calc_buffer_size( surface, 8, 0, &alloc->pitch, &alloc->size );

     alloc->addr = SHMALLOC( data->shmpool, alloc->size );
     if (!alloc->addr)
          return D_OOSHM();

     allocation->size = alloc->size;

     return DFB_OK;
}

static DFBResult
sharedDeallocateBuffer( CoreSurfacePool       *pool,
                        void                  *pool_data,
                        void                  *pool_local,
                        CoreSurfaceBuffer     *buffer,
                        CoreSurfaceAllocation *allocation,
                        void                  *alloc_data )
{
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     SHFREE( data->shmpool, alloc->addr );

     return DFB_OK;
}

static DFBResult
sharedLock( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            CoreSurfaceBufferLock *lock )
{
     SharedAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     lock->addr  = alloc->addr;
     lock->pitch = alloc->pitch;

     return DFB_OK;
}

static DFBResult
sharedUnlock( CoreSurfacePool       *pool,
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

const SurfacePoolFuncs sharedSurfacePoolFuncs = {
     PoolDataSize:       sharedPoolDataSize,
     PoolLocalDataSize:  sharedPoolLocalDataSize,
     AllocationDataSize: sharedAllocationDataSize,
     InitPool:           sharedInitPool,
     DestroyPool:        sharedDestroyPool,

     AllocateBuffer:     sharedAllocateBuffer,
     DeallocateBuffer:   sharedDeallocateBuffer,

     Lock:               sharedLock,
     Unlock:             sharedUnlock
};

