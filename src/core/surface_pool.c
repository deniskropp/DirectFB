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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/mem.h>

#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>

#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <core/system.h>

#include <gfx/convert.h>


D_DEBUG_DOMAIN( Core_SurfacePool, "Core/SurfacePool", "DirectFB Core Surface Pool" );

/**********************************************************************************************************************/

static const SurfacePoolFuncs *pool_funcs[MAX_SURFACE_POOLS];
static void                   *pool_locals[MAX_SURFACE_POOLS];
static int                     pool_count;
static CoreSurfacePool        *pools[MAX_SURFACE_POOLS];

/**********************************************************************************************************************/

static inline const SurfacePoolFuncs *
get_funcs( const CoreSurfacePool *pool )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     D_ASSERT( pool->pool_id >= 0 );
     D_ASSERT( pool->pool_id < MAX_SURFACE_POOLS );
     D_ASSERT( pool_funcs[pool->pool_id] != NULL );

     /* Return function table of the pool. */
     return pool_funcs[pool->pool_id];
}

static inline void *
get_local( const CoreSurfacePool *pool )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     D_ASSERT( pool->pool_id >= 0 );
     D_ASSERT( pool->pool_id < MAX_SURFACE_POOLS );

     /* Return local data of the pool. */
     return pool_locals[pool->pool_id];
}

/**********************************************************************************************************************/

static DFBResult init_pool( CoreDFB                *core,
                            CoreSurfacePool        *pool,
                            const SurfacePoolFuncs *funcs );

/**********************************************************************************************************************/

DFBResult
dfb_surface_pool_initialize( CoreDFB                 *core,
                             const SurfacePoolFuncs  *funcs,
                             CoreSurfacePool        **ret_pool )
{
     DFBResult            ret;
     CoreSurfacePool     *pool;
     FusionSHMPoolShared *shmpool;

     D_DEBUG_AT( Core_SurfacePool, "dfb_surface_pool_initialize( %p )\n", funcs );

     D_ASSERT( core != NULL );
     D_ASSERT( funcs != NULL );
     D_ASSERT( ret_pool != NULL );

     /* Check against pool limit. */
     if (pool_count == MAX_SURFACE_POOLS) {
          D_ERROR( "Core/SurfacePool: Maximum number of pools (%d) reached!\n", MAX_SURFACE_POOLS );
          return DFB_LIMITEXCEEDED;
     }

     D_ASSERT( pool_funcs[pool_count] == NULL );

     shmpool = dfb_core_shmpool( core );

     /* Allocate pool structure. */
     pool = SHCALLOC( shmpool, 1, sizeof(CoreSurfacePool) );
     if (!pool)
          return D_OOSHM();

     /* Assign a pool ID. */
     pool->pool_id = pool_count++;

     /* Remember shared memory pool. */
     pool->shmpool = shmpool;

     /* Set function table of the pool. */
     pool_funcs[pool->pool_id] = funcs;

     /* Add to global pool list. */
     pools[pool->pool_id] = pool;

     D_MAGIC_SET( pool, CoreSurfacePool );

     ret = init_pool( core, pool, funcs );
     if (ret) {
          pool_count--;
          D_MAGIC_CLEAR( pool );
          SHFREE( shmpool, pool );
          return ret;
     }

     D_DEBUG_AT( Core_SurfacePool, "  -> %p - '%s' [%d] (%d), %p\n",
                 pool, pool->desc.name, pool->pool_id, pool->desc.priority, funcs );

     /* Return the new pool. */
     *ret_pool = pool;

     return DFB_OK;
}

DFBResult
dfb_surface_pool_join( CoreDFB                *core,
                       CoreSurfacePool        *pool,
                       const SurfacePoolFuncs *funcs )
{
     DFBResult ret;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     D_DEBUG_AT( Core_SurfacePool, "dfb_surface_pool_join( %p [%d], %p )\n", pool, pool->pool_id, funcs );

     D_ASSERT( core != NULL );
     D_ASSERT( funcs != NULL );

     D_ASSERT( pool->pool_id >= 0 );
     D_ASSERT( pool->pool_id < MAX_SURFACE_POOLS );
     D_ASSERT( pool_funcs[pool->pool_id] == NULL );

     /* Allocate local pool data. */
     if (pool->pool_local_data_size &&
         !(pool_locals[pool->pool_id] = D_CALLOC( 1, pool->pool_local_data_size )))
         return D_OOM();

     /* Set function table of the pool. */
     pool_funcs[pool->pool_id] = funcs;

     /* Add to global pool list. */
     pools[pool->pool_id] = pool;

     /* Adjust pool count. */
     if (pool_count < pool->pool_id + 1)
          pool_count = pool->pool_id + 1;

     funcs = get_funcs( pool );

     if (funcs->JoinPool) {
          ret = funcs->JoinPool( core, pool, pool->data, get_local(pool), dfb_system_data() );
          if (ret) {
               D_DERROR( ret, "Core/SurfacePool: Joining '%s' failed!\n", pool->desc.name );

               if (pool_locals[pool->pool_id]) {
                    D_FREE( pool_locals[pool->pool_id] );
                    pool_locals[pool->pool_id] = NULL;
               }

               pool_count--;

               return ret;
          }
     }

     return DFB_OK;
}

DFBResult
dfb_surface_pool_destroy( CoreSurfacePool *pool )
{
     CoreSurfacePoolID       pool_id;
     const SurfacePoolFuncs *funcs;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     pool_id = pool->pool_id;

     D_DEBUG_AT( Core_SurfacePool, "%s( %p, '%s' [%d] )\n", __FUNCTION__, pool, pool->desc.name, pool_id );

     D_ASSERT( pool->pool_id >= 0 );
     D_ASSERT( pool_id < MAX_SURFACE_POOLS );
     D_ASSERT( pools[pool_id] == pool );

     funcs = get_funcs( pool );

     if (funcs->DestroyPool)
          funcs->DestroyPool( pool, pool->data, get_local(pool) );

     /* Free local pool data. */
     if (pool_locals[pool_id]) {
          D_FREE( pool_locals[pool_id] );
          pool_locals[pool_id] = NULL;
     }

     /* Free shared pool data. */
     if (pool->data)
          SHFREE( pool->shmpool, pool->data );

     /* Erase entries of the pool. */
     pools[pool_id]      = NULL;
     pool_funcs[pool_id] = NULL;

     while (pool_count > 0 && !pools[pool_count-1])
          pool_count--;

     fusion_skirmish_destroy( &pool->lock );

     fusion_vector_destroy( &pool->allocs );

     D_MAGIC_CLEAR( pool );

     SHFREE( pool->shmpool, pool );

     return DFB_OK;
}

DFBResult
dfb_surface_pool_leave( CoreSurfacePool *pool )
{
     CoreSurfacePoolID       pool_id;
     const SurfacePoolFuncs *funcs;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     pool_id = pool->pool_id;

     D_DEBUG_AT( Core_SurfacePool, "%s( %p, '%s' [%d] )\n", __FUNCTION__, pool, pool->desc.name, pool_id );

     D_ASSERT( pool->pool_id >= 0 );
     D_ASSERT( pool_id < MAX_SURFACE_POOLS );
     D_ASSERT( pools[pool_id] == pool );

     funcs = get_funcs( pool );

     if (funcs->LeavePool)
          funcs->LeavePool( pool, pool->data, get_local(pool) );

     /* Free local pool data. */
     if (pool_locals[pool_id]) {
          D_FREE( pool_locals[pool_id] );
          pool_locals[pool_id] = NULL;
     }

     /* Erase entries of the pool. */
     pools[pool_id]      = NULL;
     pool_funcs[pool_id] = NULL;

     while (pool_count > 0 && !pools[pool_count-1])
          pool_count--;

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_pools_negotiate( CoreSurfaceBuffer       *buffer,
                             CoreSurfaceAccessFlags   access,
                             CoreSurfacePool        **ret_pool )
{
     DFBResult             ret;
     int                   i;
     int                   best = -1;
     bool                  oovm = false;
     CoreSurface          *surface;
     CoreSurfaceTypeFlags  type;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_ASSERT( ret_pool != NULL );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p [%s], 0x%02x )\n", __FUNCTION__,
                 buffer, dfb_pixelformat_name( buffer->format ), access );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     type = surface->type & ~(CSTF_INTERNAL | CSTF_EXTERNAL);

     switch (buffer->policy) {
          case CSP_SYSTEMONLY:
               type |= CSTF_INTERNAL;
               break;

          case CSP_VIDEOONLY:
               type |= CSTF_EXTERNAL;
               break;

          default:
               break;
     }

     D_DEBUG_AT( Core_SurfacePool, "  -> 0x%02x 0x%02x required\n", access, type );

     for (i=0; i<pool_count; i++) {
          CoreSurfacePool *pool = pools[i];

          D_DEBUG_AT( Core_SurfacePool, "  -> 0x%02x 0x%02x [%s]\n",
                      pool->desc.access, pool->desc.types, pool->desc.name );

          if (D_FLAGS_ARE_SET( pool->desc.access, access ) &&
              D_FLAGS_ARE_SET( pool->desc.types,  type ))
          {
               const SurfacePoolFuncs *funcs;

               D_DEBUG_AT( Core_SurfacePool, "     %d / %d\n", pool->desc.priority, best );

               funcs = get_funcs( pool );

               if (funcs->TestConfig) {
                    ret = funcs->TestConfig( pool, pool->data, get_local(pool), buffer, &surface->config );
                    switch (ret) {
                         case DFB_OK:
                              break;

                         case DFB_NOVIDEOMEMORY:
                              oovm = true;
                              /* fall through */

                         default:
                              continue;
                    }
               }

               if (best <= (int)pool->desc.priority) {
                    best = pool->desc.priority;

                    *ret_pool = pool;
               }
          }
     }

     if (best != -1) {
          D_DEBUG_AT( Core_SurfacePool, "  => %s\n", (*ret_pool)->desc.name );

          return DFB_OK;
     }

     D_DEBUG_AT( Core_SurfacePool, "  => %s\n", oovm ? "OUT OF MEMORY!" : "UNSUPPORTED!" );

     return oovm ? DFB_NOVIDEOMEMORY : DFB_UNSUPPORTED;
}

DFBResult
dfb_surface_pools_enumerate( CoreSurfacePoolCallback  callback,
                             void                    *ctx )
{
     int i;

     D_ASSERT( callback != NULL );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p, %p )\n", __FUNCTION__, callback, ctx );

     for (i=0; i<pool_count; i++) {
          CoreSurfacePool *pool = pools[i];

          if (callback( pool, ctx ) == DFENUM_CANCEL)
               break;
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_pool_allocate( CoreSurfacePool        *pool,
                           CoreSurfaceBuffer      *buffer,
                           CoreSurfaceAllocation **ret_allocation )
{
     DFBResult               ret;
     int                     i;
     CoreSurfaceAllocation  *allocation = NULL;
     const SurfacePoolFuncs *funcs;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( Core_SurfacePool, "dfb_surface_pool_allocate( %p [%d], %p )\n", pool, pool->pool_id, buffer );

     D_ASSERT( ret_allocation != NULL );

     funcs = get_funcs( pool );

     D_ASSERT( funcs->AllocateBuffer != NULL );

     allocation = SHCALLOC( pool->shmpool, 1, sizeof(CoreSurfaceAllocation) );
     if (!allocation)
          return D_OOSHM();

     allocation->buffer = buffer;
     allocation->pool   = pool;
     allocation->access = pool->desc.access;

     if (pool->alloc_data_size) {
          allocation->data = SHCALLOC( pool->shmpool, 1, pool->alloc_data_size );
          if (!allocation->data) {
               ret = D_OOSHM();
               goto error;
          }
     }

     D_MAGIC_SET( allocation, CoreSurfaceAllocation );

     if (fusion_skirmish_prevail( &pool->lock )) {
          ret = DFB_FUSION;
          goto error;
     }

     ret = funcs->AllocateBuffer( pool, pool->data, get_local(pool), buffer, allocation, allocation->data );
     if (ret) {
          D_DEBUG_AT( Core_SurfacePool, "  -> %s\n", DirectFBErrorString( ret ) );
          D_MAGIC_CLEAR( allocation );
          fusion_skirmish_dismiss( &pool->lock );
          goto error;
     }

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     if (allocation->flags & CSALF_ONEFORALL) {
          CoreSurface *surface = buffer->surface;

          for (i=0; i<surface->num_buffers; i++) {
               buffer = surface->buffers[i];

               D_ASSUME( buffer->allocs.elements == 0 );

               D_DEBUG_AT( Core_SurfacePool, "  -> %p (%d)\n", allocation, i );
               fusion_vector_add( &buffer->allocs, allocation );
               fusion_vector_add( &pool->allocs, allocation );
          }
     }
     else {
          D_DEBUG_AT( Core_SurfacePool, "  -> %p\n", allocation );
          fusion_vector_add( &buffer->allocs, allocation );
          fusion_vector_add( &pool->allocs, allocation );
     }

     fusion_skirmish_dismiss( &pool->lock );

     direct_serial_init( &allocation->serial );

     *ret_allocation = allocation;

     return DFB_OK;

error:
     if (allocation->data)
          SHFREE( pool->shmpool, allocation->data );

     SHFREE( pool->shmpool, allocation );

     return ret;
}

DFBResult
dfb_surface_pool_deallocate( CoreSurfacePool       *pool,
                             CoreSurfaceAllocation *allocation )
{
     DFBResult               ret;
     int                     i;
     const SurfacePoolFuncs *funcs;
     CoreSurfaceBuffer      *buffer;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p [%d], %p )\n", __FUNCTION__, pool, pool->pool_id, allocation );

     D_ASSERT( pool == allocation->pool );

     buffer = allocation->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     funcs = get_funcs( pool );

     D_ASSERT( funcs->DeallocateBuffer != NULL );

     if (fusion_skirmish_prevail( &pool->lock ))
          return DFB_FUSION;

     ret = funcs->DeallocateBuffer( pool, pool->data, get_local(pool), allocation->buffer, allocation, allocation->data );
     if (ret) {
          D_DERROR( ret, "Core/SurfacePool: Could not deallocate buffer!\n" );
          fusion_skirmish_dismiss( &pool->lock );
          return ret;
     }

     if (allocation->flags & CSALF_ONEFORALL) {
          CoreSurface *surface = buffer->surface;

          for (i=0; i<surface->num_buffers; i++) {
               buffer = surface->buffers[i];

               fusion_vector_remove( &buffer->allocs, fusion_vector_index_of( &buffer->allocs, allocation ) );
               fusion_vector_remove( &pool->allocs, fusion_vector_index_of( &pool->allocs, allocation ) );
          }
     }
     else {
          fusion_vector_remove( &buffer->allocs, fusion_vector_index_of( &buffer->allocs, allocation ) );
          fusion_vector_remove( &pool->allocs, fusion_vector_index_of( &pool->allocs, allocation ) );
     }

     fusion_skirmish_dismiss( &pool->lock );

     if (allocation->data)
          SHFREE( pool->shmpool, allocation->data );

     direct_serial_deinit( &allocation->serial );

     D_MAGIC_CLEAR( allocation );

     SHFREE( pool->shmpool, allocation );

     return DFB_OK;
}

DFBResult
dfb_surface_pool_lock( CoreSurfacePool       *pool,
                       CoreSurfaceAllocation *allocation,
                       CoreSurfaceBufferLock *lock )
{
     DFBResult               ret;
     const SurfacePoolFuncs *funcs;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Core_SurfacePool, "dfb_surface_pool_lock( %p [%d], %p )\n", pool, pool->pool_id, allocation );

     D_ASSERT( pool == allocation->pool );

     funcs = get_funcs( pool );

     D_ASSERT( funcs->Lock != NULL );

     ret = funcs->Lock( pool, pool->data, get_local(pool), allocation, allocation->data, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfacePool: Could not lock allocation!\n" );
          return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_surface_pool_unlock( CoreSurfacePool       *pool,
                         CoreSurfaceAllocation *allocation,
                         CoreSurfaceBufferLock *lock )
{
     DFBResult               ret;
     const SurfacePoolFuncs *funcs;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Core_SurfacePool, "dfb_surface_pool_unlock( %p [%d], %p )\n", pool, pool->pool_id, allocation );

     D_ASSERT( pool == allocation->pool );

     funcs = get_funcs( pool );

     D_ASSERT( funcs->Unlock != NULL );

     ret = funcs->Unlock( pool, pool->data, get_local(pool), allocation, allocation->data, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfacePool: Could not unlock allocation!\n" );
          return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_surface_pool_enumerate ( CoreSurfacePool          *pool,
                             CoreSurfaceAllocCallback  callback,
                             void                     *ctx )
{
     int                    i;
     CoreSurfaceAllocation *allocation;

     D_DEBUG_AT( Core_SurfacePool, "%s( %p, %p, %p )\n", __FUNCTION__, pool, callback, ctx );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( callback != NULL );

     fusion_vector_foreach (allocation, i, pool->allocs) {
          if (callback( allocation, ctx ) == DFENUM_CANCEL)
               break;
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
init_pool( CoreDFB                *core,
           CoreSurfacePool        *pool,
           const SurfacePoolFuncs *funcs )
{
     DFBResult ret;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( funcs != NULL );
     D_ASSERT( funcs->InitPool != NULL );

     D_DEBUG_AT( Core_SurfacePool, "init_pool( %p, %p )\n", pool, funcs );

     if (funcs->PoolDataSize)
          pool->pool_data_size = funcs->PoolDataSize();

     if (funcs->PoolLocalDataSize)
          pool->pool_local_data_size = funcs->PoolLocalDataSize();

     if (funcs->AllocationDataSize)
          pool->alloc_data_size = funcs->AllocationDataSize();

     /* Allocate shared pool data. */
     if (pool->pool_data_size) {
          pool->data = SHCALLOC( pool->shmpool, 1, pool->pool_data_size );
          if (!pool->data)
               return D_OOSHM();
     }

     /* Allocate local pool data. */
     if (pool->pool_local_data_size &&
         !(pool_locals[pool->pool_id] = D_CALLOC( 1, pool->pool_local_data_size )))
     {
          SHFREE( pool->shmpool, pool->data );
          return D_OOM();
     }

     fusion_vector_init( &pool->allocs, 4, pool->shmpool );

     ret = funcs->InitPool( core, pool, pool->data, get_local(pool), dfb_system_data(), &pool->desc );
     if (ret) {
          D_DERROR( ret, "Core/SurfacePool: Initializing '%s' failed!\n", pool->desc.name );

          if (pool_locals[pool->pool_id]) {
               D_FREE( pool_locals[pool->pool_id] );
               pool_locals[pool->pool_id] = NULL;
          }
          if (pool->data) {
               SHFREE( pool->shmpool, pool->data );
               pool->data = NULL;
          }
          return ret;
     }

     fusion_skirmish_init( &pool->lock, pool->desc.name, dfb_core_world(core) );

     return DFB_OK;
}

