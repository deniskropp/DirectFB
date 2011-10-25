/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>

#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>

#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>


D_DEBUG_DOMAIN( Core_SurfacePool,  "Core/SurfacePool",  "DirectFB Core Surface Pool" );
D_DEBUG_DOMAIN( Core_SurfPoolLock, "Core/SurfPoolLock", "DirectFB Core Surface Pool Lock" );

/**********************************************************************************************************************/

static const SurfacePoolFuncs *pool_funcs[MAX_SURFACE_POOLS];
static void                   *pool_locals[MAX_SURFACE_POOLS];
static int                     pool_count;
static CoreSurfacePool        *pool_array[MAX_SURFACE_POOLS];
static unsigned int            pool_order[MAX_SURFACE_POOLS];

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
                            const SurfacePoolFuncs *funcs,
                            void                   *ctx );

/**********************************************************************************************************************/

static void insert_pool_local( CoreSurfacePool   *pool );
static void remove_pool_local( CoreSurfacePoolID  pool_id );

/**********************************************************************************************************************/

static void      remove_allocation( CoreSurfacePool       *pool,
                                    CoreSurfaceBuffer     *buffer,
                                    CoreSurfaceAllocation *allocation );

static DFBResult backup_allocation( CoreSurfaceAllocation *allocation );

/**********************************************************************************************************************/

/*
 * Enable a surface pool to obtain its own local data without having to
 * explicitly store a static local pointer to it during init/join.
 */
void *
dfb_surface_pool_get_local( const CoreSurfacePool *pool )
{
     return get_local( pool );
}

DFBResult
dfb_surface_pool_initialize( CoreDFB                 *core,
                             const SurfacePoolFuncs  *funcs,
                             CoreSurfacePool        **ret_pool )
{
     return dfb_surface_pool_initialize2( core, funcs, dfb_system_data(), ret_pool );
}

DFBResult
dfb_surface_pool_initialize2( CoreDFB                 *core,
                              const SurfacePoolFuncs  *funcs,
                              void                    *ctx,
                              CoreSurfacePool        **ret_pool )
{
     DFBResult            ret;
     CoreSurfacePool     *pool;
     FusionSHMPoolShared *shmpool;

     D_DEBUG_AT( Core_SurfacePool, "%s( %p )\n", __FUNCTION__, funcs );

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
     pool_array[pool->pool_id] = pool;

     D_MAGIC_SET( pool, CoreSurfacePool );

     ret = init_pool( core, pool, funcs, ctx );
     if (ret) {
          pool_funcs[pool->pool_id] = NULL;
          pool_array[pool->pool_id] = NULL;
          pool_count--;
          D_MAGIC_CLEAR( pool );
          SHFREE( shmpool, pool );
          return ret;
     }

     /* Set default backup pool being the shared memory surface pool */
     if (!pool->backup && pool_count > 1)
          pool->backup = pool_array[0];

     /* Insert new pool into priority order */
     insert_pool_local( pool );

     /* Return the new pool. */
     *ret_pool = pool;

     return DFB_OK;
}

DFBResult
dfb_surface_pool_join( CoreDFB                *core,
                       CoreSurfacePool        *pool,
                       const SurfacePoolFuncs *funcs )
{
     return dfb_surface_pool_join2( core, pool, funcs, dfb_system_data() );
}

DFBResult
dfb_surface_pool_join2( CoreDFB                *core,
                        CoreSurfacePool        *pool,
                        const SurfacePoolFuncs *funcs,
                        void                   *ctx )
{
     DFBResult ret;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p [%d], %p )\n", __FUNCTION__, pool, pool->pool_id, funcs );

     D_ASSERT( core != NULL );
     D_ASSERT( funcs != NULL );

     D_ASSERT( pool->pool_id < MAX_SURFACE_POOLS );
     D_ASSERT( pool->pool_id == pool_count );
     D_ASSERT( pool_funcs[pool->pool_id] == NULL );

     /* Enforce same order as initialization to be used during join. */
     if (pool->pool_id != pool_count) {
          D_ERROR( "Core/SurfacePool: Wrong order of joining pools, got %d, should be %d!\n",
                   pool->pool_id, pool_count );
          return DFB_BUG;
     }

     /* Allocate local pool data. */
     if (pool->pool_local_data_size &&
         !(pool_locals[pool->pool_id] = D_CALLOC( 1, pool->pool_local_data_size )))
         return D_OOM();

     /* Set function table of the pool. */
     pool_funcs[pool->pool_id] = funcs;

     /* Add to global pool list. */
     pool_array[pool->pool_id] = pool;

     /* Adjust pool count. */
     if (pool_count < pool->pool_id + 1)
          pool_count = pool->pool_id + 1;

     funcs = get_funcs( pool );

     if (funcs->JoinPool) {
          ret = funcs->JoinPool( core, pool, pool->data, get_local(pool), ctx );
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

     /* Insert new pool into priority order */
     insert_pool_local( pool );

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
     D_ASSERT( pool_array[pool_id] == pool );

     funcs = get_funcs( pool );

     if (funcs->DestroyPool)
          funcs->DestroyPool( pool, pool->data, get_local(pool) );

     /* Free shared pool data. */
     if (pool->data)
          SHFREE( pool->shmpool, pool->data );

     /* Free local pool data and remove from lists */
     remove_pool_local( pool_id );

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
     D_ASSERT( pool_array[pool_id] == pool );

     funcs = get_funcs( pool );

     if (funcs->LeavePool)
          funcs->LeavePool( pool, pool->data, get_local(pool) );

     /* Free local pool data and remove from lists */
     remove_pool_local( pool_id );

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_pools_prealloc( const DFBSurfaceDescription *description,
                            CoreSurfaceConfig           *config )
{
     DFBResult            ret;
     int                  i;
     CoreSurfaceTypeFlags type;

     D_DEBUG_AT( Core_SurfacePool, "%s( %p, %p )\n", __FUNCTION__, description, config );

     D_ASSERT( description != NULL );
     D_ASSERT( config != NULL );

     type = CSTF_PREALLOCATED;

     if (description->flags & DSDESC_CAPS) {
          if (description->caps & DSCAPS_SYSTEMONLY)
               type |= CSTF_INTERNAL;

          if (description->caps & DSCAPS_VIDEOONLY)
               type |= CSTF_EXTERNAL;
     }

     D_DEBUG_AT( Core_SurfacePool, "  ->     type 0x%03x required\n", type );

     for (i=0; i<pool_count; i++) {
          CoreSurfacePool *pool;

          D_ASSERT( pool_order[i] >= 0 );
          D_ASSERT( pool_order[i] < pool_count );

          pool = pool_array[pool_order[i]];
          D_MAGIC_ASSERT( pool, CoreSurfacePool );

          if (D_FLAGS_ARE_SET( pool->desc.types, type )) {
               const SurfacePoolFuncs *funcs;

               D_DEBUG_AT( Core_SurfacePool, "  -> [%d] 0x%02x 0x%03x (%d) [%s]\n", pool->pool_id,
                           pool->desc.caps, pool->desc.types, pool->desc.priority, pool->desc.name );

               funcs = get_funcs( pool );

               if (funcs->PreAlloc) {
                    ret = funcs->PreAlloc( pool, pool->data, get_local(pool), description, config );
                    if (ret == DFB_OK) {
                         config->preallocated_pool_id = pool->pool_id;
                         return DFB_OK;
                    }
               }
          }
     }

     return DFB_UNSUPPORTED;
}

DFBResult
dfb_surface_pools_negotiate( CoreSurfaceBuffer       *buffer,
                             CoreSurfaceAccessorID    accessor,
                             CoreSurfaceAccessFlags   access,
                             CoreSurfacePool        **ret_pools,
                             unsigned int             max_pools,
                             unsigned int            *ret_num )
{
     DFBResult             ret;
     int                   i;
     unsigned int          num = 0;
     CoreSurface          *surface;
     CoreSurfaceTypeFlags  type;
     unsigned int          free_count = 0;
     CoreSurfacePool      *free_pools[pool_count];
     unsigned int          oom_count = 0;
     CoreSurfacePool      *oom_pools[pool_count];

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p [%s], 0x%02x, 0x%02x, max %d )\n", __FUNCTION__,
                 buffer, dfb_pixelformat_name( buffer->format ), accessor, access, max_pools );

     D_ASSERT( ret_pools != NULL );
     D_ASSERT( max_pools > 0 );
     D_ASSERT( ret_num != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );

     D_ASSERT( accessor >= CSAID_CPU );
     D_ASSUME( accessor < _CSAID_NUM );
     if (accessor >= CSAID_ANY) {
          D_UNIMPLEMENTED();
          return DFB_UNIMPLEMENTED;
     }

     if (accessor < 0 || accessor >= _CSAID_NUM)
          return DFB_INVARG;

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

#if D_DEBUG_ENABLED
     D_DEBUG_AT( Core_SurfacePool, "  ->     0x%02x 0x%03x required\n", access, type );

     if (access & CSAF_READ)
          D_DEBUG_AT( Core_SurfacePool, "  ->     READ\n" );

     if (access & CSAF_WRITE)
          D_DEBUG_AT( Core_SurfacePool, "  ->     WRITE\n" );

     if (access & CSAF_SHARED)
          D_DEBUG_AT( Core_SurfacePool, "  ->     SHARED\n" );

     if (type & CSTF_LAYER)
          D_DEBUG_AT( Core_SurfacePool, "  ->     LAYER\n" );

     if (type & CSTF_WINDOW)
          D_DEBUG_AT( Core_SurfacePool, "  ->     WINDOW\n" );

     if (type & CSTF_CURSOR)
          D_DEBUG_AT( Core_SurfacePool, "  ->     CURSOR\n" );

     if (type & CSTF_FONT)
          D_DEBUG_AT( Core_SurfacePool, "  ->     FONT\n" );

     if (type & CSTF_SHARED)
          D_DEBUG_AT( Core_SurfacePool, "  ->     SHARED\n" );

     if (type & CSTF_INTERNAL)
          D_DEBUG_AT( Core_SurfacePool, "  ->     INTERNAL\n" );

     if (type & CSTF_EXTERNAL)
          D_DEBUG_AT( Core_SurfacePool, "  ->     EXTERNAL\n" );

     if (type & CSTF_PREALLOCATED)
          D_DEBUG_AT( Core_SurfacePool, "  ->     PREALLOCATED\n" );
#endif

     for (i=0; i<pool_count; i++) {
          CoreSurfacePool *pool;

          D_ASSERT( pool_order[i] >= 0 );
          D_ASSERT( pool_order[i] < pool_count );

          pool = pool_array[pool_order[i]];
          D_MAGIC_ASSERT( pool, CoreSurfacePool );

          if (D_FLAGS_ARE_SET( pool->desc.access[accessor], access ) &&
              D_FLAGS_ARE_SET( pool->desc.types, type & ~CSTF_PREALLOCATED ))
          {
               const SurfacePoolFuncs *funcs;

               D_DEBUG_AT( Core_SurfacePool, "  -> [%d] 0x%02x 0x%03x (%d) [%s]\n", pool->pool_id,
                           pool->desc.caps, pool->desc.types, pool->desc.priority, pool->desc.name );

               funcs = get_funcs( pool );

               ret = funcs->TestConfig ? funcs->TestConfig( pool, pool->data, get_local(pool),
                                                            buffer, &surface->config ) : DFB_OK;
               switch (ret) {
                    case DFB_OK:
                         D_DEBUG_AT( Core_SurfacePool, "    => OK\n" );
                         free_pools[free_count++] = pool;
                         break;

                    case DFB_NOVIDEOMEMORY:
                         D_DEBUG_AT( Core_SurfacePool, "    => OUT OF MEMORY\n" );
                         oom_pools[oom_count++] = pool;
                         break;

                    default:
                         continue;
               }
          }
     }

     D_DEBUG_AT( Core_SurfacePool, "  => %d pools available\n", free_count );
     D_DEBUG_AT( Core_SurfacePool, "  => %d pools out of memory\n", oom_count );

     for (i=0; i<free_count && num<max_pools; i++)
          ret_pools[num++] = free_pools[i];

     for (i=0; i<oom_count && num<max_pools; i++)
          ret_pools[num++] = oom_pools[i];

     *ret_num = num;

     return free_count ? DFB_OK : oom_count ? DFB_NOVIDEOMEMORY : DFB_UNSUPPORTED;
}

DFBResult
dfb_surface_pools_enumerate( CoreSurfacePoolCallback  callback,
                             void                    *ctx )
{
     int i;

     D_ASSERT( callback != NULL );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p, %p )\n", __FUNCTION__, callback, ctx );

     for (i=0; i<pool_count; i++) {
          CoreSurfacePool *pool = pool_array[i];

          D_MAGIC_ASSERT( pool, CoreSurfacePool );

          if (callback( pool, ctx ) == DFENUM_CANCEL)
               break;
     }

     return DFB_OK;
}

DFBResult
dfb_surface_pools_lookup( CoreSurfacePoolID   pool_id,
                          CoreSurfacePool   **ret_pool )
{
     int i;

     D_DEBUG_AT( Core_SurfacePool, "%s( pool id %u, %p )\n", __FUNCTION__, pool_id, ret_pool );

     D_ASSERT( ret_pool != NULL );

     for (i=0; i<pool_count; i++) {
          CoreSurfacePool *pool = pool_array[i];

          D_MAGIC_ASSERT( pool, CoreSurfacePool );

          if (pool->pool_id == pool_id) {
               *ret_pool = pool;
               return DFB_OK;
          }
     }

     return DFB_IDNOTFOUND;
}

DFBResult
dfb_surface_pools_allocate( CoreSurfaceBuffer       *buffer,
                            CoreSurfaceAccessorID    accessor,
                            CoreSurfaceAccessFlags   access,
                            CoreSurfaceAllocation  **ret_allocation )
{
     DFBResult              ret;
     int                    i;
     CoreSurface           *surface;
     CoreSurfaceAllocation *allocation = NULL;
     CoreSurfacePool       *pools[pool_count];
     unsigned int           num_pools;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_FLAGS_ASSERT( access, CSAF_ALL );
     D_ASSERT( ret_allocation != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p, 0x%x )\n", __FUNCTION__, buffer, access );

     D_DEBUG_AT( Core_SurfacePool, " -> %dx%d %s - %s%s%s%s%s%s%s%s\n",
                 surface->config.size.w, surface->config.size.h,
                 dfb_pixelformat_name( surface->config.format ),
                 (surface->type & CSTF_SHARED)       ? "SHARED"        : "PRIVATE",
                 (surface->type & CSTF_LAYER)        ? " LAYER"        : "",
                 (surface->type & CSTF_WINDOW)       ? " WINDOW"       : "",
                 (surface->type & CSTF_CURSOR)       ? " CURSOR"       : "",
                 (surface->type & CSTF_FONT)         ? " FONT"         : "",
                 (surface->type & CSTF_INTERNAL)     ? " INTERNAL"     : "",
                 (surface->type & CSTF_EXTERNAL)     ? " EXTERNAL"     : "",
                 (surface->type & CSTF_PREALLOCATED) ? " PREALLOCATED" : "" );

     D_ASSERT( accessor >= CSAID_CPU );
     D_ASSUME( accessor < _CSAID_NUM );
     if (accessor >= CSAID_ANY) {
          D_UNIMPLEMENTED();
          return DFB_UNIMPLEMENTED;
     }

     if (accessor < 0 || accessor >= _CSAID_NUM)
          return DFB_INVARG;

     /* Build a list of possible pools being free or out of memory */
     ret = dfb_surface_pools_negotiate( buffer, accessor, access, pools, pool_count, &num_pools );
     if (ret && ret != DFB_NOVIDEOMEMORY) {
          D_DEBUG_AT( Core_SurfacePool, "  -> NEGOTIATION FAILED! (%s)\n", DirectFBErrorString( ret ) );
          return ret;
     }

     /* Try to do the allocation in one of the pools */
     for (i=0; i<num_pools; i++) {
          CoreSurfacePool *pool = pools[i];

          D_MAGIC_ASSERT( pool, CoreSurfacePool );

          ret = dfb_surface_pool_allocate( pool, buffer, &allocation );

          if (ret == DFB_OK)
               break;

          /* When an error other than out of memory happens... */
          if (ret != DFB_NOVIDEOMEMORY) {
               D_DEBUG_AT( Core_SurfacePool, "  -> Allocation in '%s' failed!\n", pool->desc.name );

               /* ...forget about the pool for now */
               pools[i] = NULL;
          }
     }

     /* Check if none of the pools could do the allocation */
     if (!allocation) {
          /* Try to find a pool with "older" allocations to muck out */
          for (i=0; i<num_pools; i++) {
               CoreSurfacePool *pool = pools[i];

               /* Pools with non-oom errors were sorted out above */
               if (!pool)
                    continue;

               D_MAGIC_ASSERT( pool, CoreSurfacePool );

               ret = dfb_surface_pool_displace( pool, buffer, &allocation );

               if (ret == DFB_OK)
                    break;
          }
     }

     /* Still no luck? */
     if (!allocation) {
          D_DEBUG_AT( Core_SurfacePool, "  -> FAILED!\n" );
          return DFB_FAILURE;
     }

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     D_DEBUG_AT( Core_SurfacePool, "  -> %p\n", allocation );

     *ret_allocation = allocation;

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
     CoreSurface            *surface;
     CoreSurfaceAllocation  *allocation = NULL;
     const SurfacePoolFuncs *funcs;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p [%d - %s], %p )\n", __FUNCTION__, pool, pool->pool_id, pool->desc.name, buffer );

     D_ASSERT( ret_allocation != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );

     funcs = get_funcs( pool );

     D_ASSERT( funcs->AllocateBuffer != NULL );

     allocation = SHCALLOC( pool->shmpool, 1, sizeof(CoreSurfaceAllocation) );
     if (!allocation)
          return D_OOSHM();

     allocation->buffer  = buffer;
     allocation->surface = surface;
     allocation->pool    = pool;
     allocation->access  = pool->desc.access;

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

     if (dfb_config->warn.flags & DCWF_ALLOCATE_BUFFER &&
         dfb_config->warn.allocate_buffer.min_size.w <= surface->config.size.w &&
         dfb_config->warn.allocate_buffer.min_size.h <= surface->config.size.h)
          D_WARN( "allocate-buffer %4dx%4d %6s, surface-caps 0x%08x",
                  surface->config.size.w, surface->config.size.h, dfb_pixelformat_name(buffer->format),
                  surface->config.caps );

     ret = funcs->AllocateBuffer( pool, pool->data, get_local(pool), buffer, allocation, allocation->data );
     if (ret) {
          D_DEBUG_AT( Core_SurfacePool, "  -> %s\n", DirectFBErrorString( ret ) );
          D_MAGIC_CLEAR( allocation );
          fusion_skirmish_dismiss( &pool->lock );
          goto error;
     }

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     if (allocation->flags & CSALF_ONEFORALL) {
          for (i=0; i<surface->num_buffers; i++) {
               buffer = surface->buffers[i];

               D_ASSUME( fusion_vector_is_empty( &buffer->allocs ) );

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

     direct_serial_init( &allocation->serial );

     fusion_skirmish_dismiss( &pool->lock );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

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
     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p [%d - %s], %p )\n", __FUNCTION__, pool, pool->pool_id, pool->desc.name, allocation );

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
          CoreSurface *surface;

          surface = buffer->surface;
          D_MAGIC_ASSERT( surface, CoreSurface );
          FUSION_SKIRMISH_ASSERT( &surface->lock );

          for (i=0; i<surface->num_buffers; i++)
               remove_allocation( pool, surface->buffers[i], allocation );
     }
     else
          remove_allocation( pool, buffer, allocation );

     fusion_skirmish_dismiss( &pool->lock );

     if (allocation->data)
          SHFREE( pool->shmpool, allocation->data );

     direct_serial_deinit( &allocation->serial );

     D_MAGIC_CLEAR( allocation );

     SHFREE( pool->shmpool, allocation );

     return DFB_OK;
}

DFBResult
dfb_surface_pool_displace( CoreSurfacePool        *pool,
                           CoreSurfaceBuffer      *buffer,
                           CoreSurfaceAllocation **ret_allocation )
{
     DFBResult               ret, ret_lock = DFB_OK;
     int                     i, retries = 3;
     CoreSurface            *surface;
     CoreSurfaceAllocation  *allocation;
     const SurfacePoolFuncs *funcs;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p [%d - %s], %p )\n", __FUNCTION__, pool, pool->pool_id, pool->desc.name, buffer );

     D_ASSERT( ret_allocation != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );

     funcs = get_funcs( pool );

     if (fusion_skirmish_prevail( &pool->lock ))
          return DFB_FUSION;

     /* Check for integrated method to muck out "older" allocations for a new one */
     if (funcs->MuckOut) {
          ret = funcs->MuckOut( pool, pool->data, get_local(pool), buffer );
          if (ret) {
               fusion_skirmish_dismiss( &pool->lock );
               return ret;
          }
     }
     else {
          /* Or take the generic approach via allocation list */
          D_UNIMPLEMENTED();
     }

     /* FIXME: Solve potential dead lock, until then do a few retries... */
fixme_retry:
     fusion_vector_foreach (allocation, i, pool->allocs) {
          CORE_SURFACE_ALLOCATION_ASSERT( allocation );

          if (allocation->flags & CSALF_MUCKOUT) {
               CoreSurface       *alloc_surface;
               CoreSurfaceBuffer *alloc_buffer;

               alloc_buffer = allocation->buffer;
               D_MAGIC_ASSERT( alloc_buffer, CoreSurfaceBuffer );

               alloc_surface = alloc_buffer->surface;
               D_MAGIC_ASSERT( alloc_surface, CoreSurface );

               D_DEBUG_AT( Core_SurfacePool, "  <= %p %5dk, %lu\n",
                           allocation, allocation->size / 1024, allocation->offset );

               /* FIXME: Solve potential dead lock, until then only try to lock... */
               ret = dfb_surface_trylock( alloc_surface );
               if (ret) {
                    D_WARN( "could not lock surface (%s)", DirectFBErrorString(ret) );
                    ret_lock = ret;
                    continue;
               }

               /* Ensure mucked out allocation is backed up in another pool */
               ret = backup_allocation( allocation );
               if (ret) {
                    D_WARN( "could not backup allocation (%s)", DirectFBErrorString(ret) );
                    dfb_surface_unlock( alloc_surface );
                    goto error_cleanup;
               }

               /* Deallocate mucked out allocation */
               dfb_surface_pool_deallocate( pool, allocation );
               i--;

               dfb_surface_unlock( alloc_surface );
          }
     }

     /* FIXME: Solve potential dead lock, until then do a few retries... */
     if (ret_lock) {
          if (retries--)
               goto fixme_retry;

          ret = DFB_LOCKED;

          goto error_cleanup;
     }
     else
          ret = dfb_surface_pool_allocate( pool, buffer, ret_allocation );

     fusion_skirmish_dismiss( &pool->lock );

     return ret;


error_cleanup:
     fusion_vector_foreach (allocation, i, pool->allocs) {
          CORE_SURFACE_ALLOCATION_ASSERT( allocation );

          if (allocation->flags & CSALF_MUCKOUT)
               allocation->flags &= ~CSALF_MUCKOUT;
     }

     fusion_skirmish_dismiss( &pool->lock );

     return ret;
}

DFBResult
dfb_surface_pool_prelock( CoreSurfacePool        *pool,
                          CoreSurfaceAllocation  *allocation,
                          CoreSurfaceAccessorID   accessor,
                          CoreSurfaceAccessFlags  access )
{
     DFBResult               ret;
     const SurfacePoolFuncs *funcs;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     D_DEBUG_AT( Core_SurfPoolLock, "%s( %p [%d - %s], %p )\n", __FUNCTION__, pool, pool->pool_id, pool->desc.name, allocation );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );
     D_ASSERT( pool == allocation->pool );

     funcs = get_funcs( pool );

     if (funcs->PreLock) {
          ret = funcs->PreLock( pool, pool->data, get_local(pool), allocation, allocation->data, accessor, access );
          if (ret) {
               D_DERROR( ret, "Core/SurfacePool: Could not prelock allocation!\n" );
               return ret;
          }
     }

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

     D_DEBUG_AT( Core_SurfPoolLock, "%s( %p [%d - %s], %p )\n", __FUNCTION__, pool, pool->pool_id, pool->desc.name, allocation );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );
     CORE_SURFACE_BUFFER_LOCK_ASSERT( lock );
     D_ASSERT( lock->buffer == NULL );

     D_ASSERT( pool == allocation->pool );

     funcs = get_funcs( pool );

     D_ASSERT( funcs->Lock != NULL );

     lock->allocation = allocation;
     lock->buffer     = allocation->buffer;

     ret = funcs->Lock( pool, pool->data, get_local(pool), allocation, allocation->data, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfacePool: Could not lock allocation!\n" );
          dfb_surface_buffer_lock_reset( lock );
          return ret;
     }

     CORE_SURFACE_BUFFER_LOCK_ASSERT( lock );
     D_ASSERT( lock->buffer != NULL );

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

     D_DEBUG_AT( Core_SurfPoolLock, "%s( %p [%d - %s], %p )\n", __FUNCTION__, pool, pool->pool_id, pool->desc.name, allocation );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );
     CORE_SURFACE_BUFFER_LOCK_ASSERT( lock );
     D_ASSERT( lock->buffer != NULL );

     D_ASSERT( pool == allocation->pool );

     funcs = get_funcs( pool );

     D_ASSERT( funcs->Unlock != NULL );

     ret = funcs->Unlock( pool, pool->data, get_local(pool), allocation, allocation->data, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfacePool: Could not unlock allocation!\n" );
          return ret;
     }

     CORE_SURFACE_BUFFER_LOCK_ASSERT( lock );
     D_ASSERT( lock->buffer != NULL );

     dfb_surface_buffer_lock_reset( lock );

     return DFB_OK;
}

DFBResult
dfb_surface_pool_read( CoreSurfacePool       *pool,
                       CoreSurfaceAllocation *allocation,
                       void                  *data,
                       int                    pitch,
                       const DFBRectangle    *rect )
{
     DFBResult               ret;
     const SurfacePoolFuncs *funcs;
     CoreSurface            *surface;
     DFBRectangle            area;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     D_DEBUG_AT( Core_SurfPoolLock, "%s( %p [%d - %s], %p )\n", __FUNCTION__, pool, pool->pool_id, pool->desc.name, allocation );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );
     D_ASSERT( data != NULL );
     D_ASSERT( pitch >= 0 );
     DFB_RECTANGLE_ASSERT_IF( rect );

     D_ASSERT( pool == allocation->pool );

     funcs = get_funcs( pool );
     D_ASSERT( funcs != NULL );

     if (!funcs->Read)
          return DFB_UNSUPPORTED;

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     area.x = 0;
     area.y = 0;
     area.w = surface->config.size.w;
     area.h = surface->config.size.h;

     if (rect && !dfb_rectangle_intersect( &area, rect ))
          return DFB_INVAREA;

     ret = funcs->Read( pool, pool->data, get_local(pool), allocation, allocation->data, data, pitch, &area );
     if (ret)
          D_DERROR( ret, "Core/SurfacePool: Could not read from allocation!\n" );

     return ret;
}

DFBResult
dfb_surface_pool_write( CoreSurfacePool       *pool,
                        CoreSurfaceAllocation *allocation,
                        const void            *data,
                        int                    pitch,
                        const DFBRectangle    *rect )
{
     DFBResult               ret;
     const SurfacePoolFuncs *funcs;
     CoreSurface            *surface;
     DFBRectangle            area;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     D_DEBUG_AT( Core_SurfPoolLock, "%s( %p [%d - %s], %p )\n", __FUNCTION__, pool, pool->pool_id, pool->desc.name, allocation );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );
     D_ASSERT( data != NULL );
     D_ASSERT( pitch >= 0 );
     DFB_RECTANGLE_ASSERT_IF( rect );

     D_ASSERT( pool == allocation->pool );

     funcs = get_funcs( pool );
     D_ASSERT( funcs != NULL );

     if (!funcs->Write)
          return DFB_UNSUPPORTED;

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     area.x = 0;
     area.y = 0;
     area.w = surface->config.size.w;
     area.h = surface->config.size.h;

     if (rect && !dfb_rectangle_intersect( &area, rect ))
          return DFB_INVAREA;

     ret = funcs->Write( pool, pool->data, get_local(pool), allocation, allocation->data, data, pitch, &area );
     if (ret)
          D_DERROR( ret, "Core/SurfacePool: Could not write to allocation!\n" );

     return ret;
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
           const SurfacePoolFuncs *funcs,
           void                   *ctx )
{
     DFBResult ret;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( funcs != NULL );
     D_ASSERT( funcs->InitPool != NULL );

     D_DEBUG_AT( Core_SurfacePool, "%s( %p, %p )\n", __FUNCTION__, pool, funcs );

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

     ret = funcs->InitPool( core, pool, pool->data, get_local(pool), ctx, &pool->desc );
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

     pool->desc.caps &= ~(CSPCAPS_READ | CSPCAPS_WRITE);

     if (funcs->Read)
          pool->desc.caps |= CSPCAPS_READ;

     if (funcs->Write)
          pool->desc.caps |= CSPCAPS_WRITE;

     fusion_skirmish_init( &pool->lock, pool->desc.name, dfb_core_world(core) );

     return DFB_OK;
}

static void
insert_pool_local( CoreSurfacePool *pool )
{
     int i, n;

     for (i=0; i<pool_count-1; i++) {
          D_ASSERT( pool_order[i] >= 0 );
          D_ASSERT( pool_order[i] < pool_count-1 );

          D_MAGIC_ASSERT( pool_array[pool_order[i]], CoreSurfacePool );

          if (pool_array[pool_order[i]]->desc.priority < pool->desc.priority)
               break;
     }

     for (n=pool_count-1; n>i; n--) {
          D_ASSERT( pool_order[n-1] >= 0 );
          D_ASSERT( pool_order[n-1] < pool_count-1 );

          D_MAGIC_ASSERT( pool_array[pool_order[n-1]], CoreSurfacePool );

          pool_order[n] = pool_order[n-1];
     }

     pool_order[n] = pool_count - 1;

#if D_DEBUG_ENABLED
     for (i=0; i<pool_count; i++) {
          D_DEBUG_AT( Core_SurfacePool, "  %c> [%d] %p - '%s' [%d] (%d), %p\n",
                      (i == n) ? '=' : '-', i, pool_array[pool_order[i]], pool_array[pool_order[i]]->desc.name,
                      pool_array[pool_order[i]]->pool_id, pool_array[pool_order[i]]->desc.priority,
                      pool_funcs[pool_order[i]] );
          D_ASSERT( pool_order[i] == pool_array[pool_order[i]]->pool_id );
     }
#endif
}

static void
remove_pool_local( CoreSurfacePoolID pool_id )
{
     int i;

     /* Free local pool data. */
     if (pool_locals[pool_id]) {
          D_FREE( pool_locals[pool_id] );
          pool_locals[pool_id] = NULL;
     }

     /* Erase entries of the pool. */
     pool_array[pool_id] = NULL;
     pool_funcs[pool_id] = NULL;

     while (pool_count > 0 && !pool_array[pool_count-1]) {
          pool_count--;

          for (i=0; i<pool_count; i++) {
               if (pool_order[i] == pool_count) {
                    direct_memmove( &pool_order[i], &pool_order[i+1], sizeof(pool_order[0]) * (pool_count - i) );
                    break;
               }
          }
     }
}

/**********************************************************************************************************************/

static void
remove_allocation( CoreSurfacePool       *pool,
                   CoreSurfaceBuffer     *buffer,
                   CoreSurfaceAllocation *allocation )
{
     int index_buffer;
     int index_pool;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     CORE_SURFACE_ALLOCATION_ASSERT( allocation );
     if (buffer->surface) {
          D_MAGIC_ASSERT( buffer->surface, CoreSurface );
          FUSION_SKIRMISH_ASSERT( &buffer->surface->lock );
     }
     FUSION_SKIRMISH_ASSERT( &pool->lock );
     D_ASSERT( pool == allocation->pool );

     /* Lookup indices within vectors */
     index_buffer = fusion_vector_index_of( &buffer->allocs, allocation );
     index_pool   = fusion_vector_index_of( &pool->allocs,   allocation );

     D_ASSERT( index_buffer >= 0 );
     D_ASSERT( index_pool >= 0 );

     /* Remove allocation from buffer and pool */
     fusion_vector_remove( &buffer->allocs, index_buffer );
     fusion_vector_remove( &pool->allocs,   index_pool );

     /* Reset 'read' allocation pointer of buffer */
     if (buffer->read == allocation)
          buffer->read = NULL;

     /* Update 'written' allocation pointer of buffer */
     if (buffer->written == allocation) {
          /* Reset pointer first */
          buffer->written = NULL;

          /* Iterate through remaining allocations */
          fusion_vector_foreach (allocation, index_buffer, buffer->allocs) {
               CORE_SURFACE_ALLOCATION_ASSERT( allocation );

               /* Check if allocation is up to date and set it as 'written' allocation */
               if (direct_serial_check( &allocation->serial, &buffer->serial )) {
                    buffer->written = allocation;
                    break;
               }
          }
     }
}

static DFBResult
backup_allocation( CoreSurfaceAllocation *allocation )
{
     DFBResult              ret = DFB_OK;
     int                    i;
     CoreSurfaceAllocation *backup = NULL;
     CoreSurfacePool       *pool;
     CoreSurfaceBuffer     *buffer;

     D_DEBUG_AT( Core_SurfacePool, "%s( %p )\n", __FUNCTION__, allocation );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     pool = allocation->pool;
     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     buffer = allocation->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_MAGIC_ASSERT( buffer->surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &buffer->surface->lock );
     FUSION_SKIRMISH_ASSERT( &pool->lock );

     /* Check if allocation is the only up to date (requiring a backup) */
     if (direct_serial_check( &allocation->serial, &buffer->serial )) {
          CoreSurfacePool *backup_pool = pool->backup;

          /* First check if any of the existing allocations is up to date */
          fusion_vector_foreach (backup, i, buffer->allocs) {
               D_MAGIC_ASSERT( backup, CoreSurfaceAllocation );
               D_MAGIC_ASSERT( backup->pool, CoreSurfacePool );

               if (backup->pool != pool && direct_serial_check( &backup->serial, &buffer->serial )) {
                    D_DEBUG_AT( Core_SurfacePool, "  -> up to date in '%s'\n", backup->pool->desc.name );
                    return DFB_OK;
               }
          }

          /* Try to update one of the existing allocations */
          fusion_vector_foreach (backup, i, buffer->allocs) {
               D_MAGIC_ASSERT( backup, CoreSurfaceAllocation );
               D_MAGIC_ASSERT( backup->pool, CoreSurfacePool );

               if (backup->pool != pool && dfb_surface_allocation_update( backup, CSAF_NONE ) == DFB_OK) {
                    D_DEBUG_AT( Core_SurfacePool, "  -> updated in '%s'\n", backup->pool->desc.name );
                    return DFB_OK;
               }
          }

          /* Try the designated backup pool and theirs if failing */
          while (backup_pool) {
               D_MAGIC_ASSERT( backup_pool, CoreSurfacePool );

               D_DEBUG_AT( Core_SurfacePool, "  -> allocating in '%s'\n", backup_pool->desc.name );

               /* Allocate in backup pool */
               ret = dfb_surface_pool_allocate( backup_pool, buffer, &backup );
               if (ret == DFB_OK) {
                    /* Update new allocation */
                    ret = dfb_surface_allocation_update( backup, CSAF_NONE );
                    if (ret) {
                         D_DEBUG_AT( Core_SurfacePool, "  -> update failed! (%s)\n", DirectFBErrorString(ret) );
                         dfb_surface_pool_deallocate( backup_pool, backup );
                         backup = NULL;
                    }
                    else
                         return DFB_OK;
               }
               else
                    D_DEBUG_AT( Core_SurfacePool, "  -> allocation failed! (%s)\n", DirectFBErrorString(ret) );

               backup_pool = backup_pool->backup;
          }
     }
     else
          D_DEBUG_AT( Core_SurfacePool, "  -> not up to date anyhow\n" );

     return ret;
}

