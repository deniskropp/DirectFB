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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/mem.h>

#include <core/surface_pool.h>

#include "x11.h"
#include "x11image.h"
#include "x11_surface_pool.h"

D_DEBUG_DOMAIN( X11_Surfaces, "X11/Surfaces", "X11 System Surface Pool" );

/**********************************************************************************************************************/

typedef struct {
} x11PoolData;

typedef struct {
     pthread_mutex_t  lock;
     DirectHash      *hash;
} x11PoolLocalData;

/**********************************************************************************************************************/

static int
x11PoolDataSize()
{
     return sizeof(x11PoolData);
}

static int
x11PoolLocalDataSize()
{
     return sizeof(x11PoolLocalData);
}

static int
x11AllocationDataSize()
{
     return sizeof(x11AllocationData);
}

static DFBResult
x11InitPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data,
             CoreSurfacePoolDescription *ret_desc )
{
     DFBResult         ret;
     x11PoolLocalData *local = pool_local;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps     = CSPCAPS_NONE;
     ret_desc->access   = CSAF_CPU_READ | CSAF_CPU_WRITE | CSAF_GPU_READ | CSAF_GPU_WRITE | CSAF_SHARED;
     ret_desc->types    = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority = CSPP_PREFERED;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "X11 Shm Images" );

     ret = direct_hash_create( 7, &local->hash );
     if (ret) {
          D_DERROR( ret, "X11/Surfaces: Could not create local hash table!\n" );
          return ret;
     }

     pthread_mutex_init( &local->lock, NULL );

     return DFB_OK;
}

static DFBResult
x11JoinPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data )
{
     DFBResult         ret;
     x11PoolLocalData *local = pool_local;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     ret = direct_hash_create( 7, &local->hash );
     if (ret) {
          D_DERROR( ret, "X11/Surfaces: Could not create local hash table!\n" );
          return ret;
     }

     pthread_mutex_init( &local->lock, NULL );

     return DFB_OK;
}

static DFBResult
x11DestroyPool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     x11PoolLocalData *local = pool_local;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     pthread_mutex_destroy( &local->lock );

     direct_hash_destroy( local->hash );

     return DFB_OK;
}

static DFBResult
x11LeavePool( CoreSurfacePool *pool,
              void            *pool_data,
              void            *pool_local )
{
     x11PoolLocalData *local = pool_local;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     pthread_mutex_destroy( &local->lock );

     direct_hash_destroy( local->hash );

     return DFB_OK;
}

static DFBResult
x11TestConfig( CoreSurfacePool         *pool,
               void                    *pool_data,
               void                    *pool_local,
               CoreSurfaceBuffer       *buffer,
               const CoreSurfaceConfig *config )
{
     return x11ImageInit( NULL, config->size.w, config->size.h, config->format );
}

static DFBResult
x11AllocateBuffer( CoreSurfacePool       *pool,
                   void                  *pool_data,
                   void                  *pool_local,
                   CoreSurfaceBuffer     *buffer,
                   CoreSurfaceAllocation *allocation,
                   void                  *alloc_data )
{
     DFBResult          ret;
     CoreSurface       *surface;
     x11AllocationData *alloc = alloc_data;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     ret = x11ImageInit( &alloc->image, surface->config.size.w, surface->config.size.h, surface->config.format );
     if (ret) {
          D_DERROR( ret, "X11/Surfaces: x11ImageInit() failed!\n" );
          return ret;
     }

     dfb_surface_calc_buffer_size( surface, 4/*?*/, 1, NULL, &allocation->size );

     return DFB_OK;
}

static DFBResult
x11DeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     x11AllocationData *alloc = alloc_data;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     return x11ImageDestroy( &alloc->image );
}

static DFBResult
x11Lock( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         CoreSurfaceBufferLock *lock )
{
     DFBResult          ret;
     void              *addr;
     x11PoolLocalData  *local = pool_local;
     x11AllocationData *alloc = alloc_data;
     CoreSurfaceBuffer *buffer;

     D_DEBUG_AT( X11_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     buffer = allocation->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_ASSERT( local->hash != NULL );

     pthread_mutex_lock( &local->lock );

     addr = direct_hash_lookup( local->hash, alloc->image.seginfo.shmid );
     if (!addr) {
          ret = x11ImageAttach( &alloc->image, &addr );
          if (ret) {
               D_DERROR( ret, "X11/Surfaces: x11ImageAttach() failed!\n" );
               pthread_mutex_unlock( &local->lock );
               return ret;
          }

          direct_hash_insert( local->hash, alloc->image.seginfo.shmid, addr );

          /* FIXME: remove/detach? */
     }

     pthread_mutex_unlock( &local->lock );

     lock->addr   = addr;
     lock->pitch  = DFB_BYTES_PER_LINE( buffer->format, alloc->image.width );
     lock->handle = alloc->image.ximage;

     return DFB_OK;
}

static DFBResult
x11Unlock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     D_DEBUG_AT( X11_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     return DFB_OK;
}

const SurfacePoolFuncs x11SurfacePoolFuncs = {
     PoolDataSize:       x11PoolDataSize,
     PoolLocalDataSize:  x11PoolLocalDataSize,
     AllocationDataSize: x11AllocationDataSize,

     InitPool:           x11InitPool,
     JoinPool:           x11JoinPool,
     DestroyPool:        x11DestroyPool,
     LeavePool:          x11LeavePool,

     TestConfig:         x11TestConfig,

     AllocateBuffer:     x11AllocateBuffer,
     DeallocateBuffer:   x11DeallocateBuffer,

     Lock:               x11Lock,
     Unlock:             x11Unlock
};

