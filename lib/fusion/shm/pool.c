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

#include <unistd.h>
#include <sys/mman.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <fusion/shmalloc.h>
#include <fusion/fusion_internal.h>

#include <fusion/shm/pool.h>
#include <fusion/shm/shm_internal.h>


D_DEBUG_DOMAIN( Fusion_SHMPool, "Fusion/SHMPool", "Fusion Shared Memory Pool" );

/**********************************************************************************************************************/

static DirectResult init_pool    ( FusionSHM           *shm,
                                   FusionSHMPool       *pool,
                                   FusionSHMPoolShared *shared,
                                   const char          *name,
                                   unsigned int         max_size,
                                   bool                 debug );

static DirectResult join_pool    ( FusionSHM           *shm,
                                   FusionSHMPool       *pool,
                                   FusionSHMPoolShared *shared );

static void         leave_pool   ( FusionSHM           *shm,
                                   FusionSHMPool       *pool,
                                   FusionSHMPoolShared *shared );

static void         shutdown_pool( FusionSHM           *shm,
                                   FusionSHMPool       *pool,
                                   FusionSHMPoolShared *shared );

/**********************************************************************************************************************/

DirectResult
fusion_shm_pool_create( FusionWorld          *world,
                        const char           *name,
                        unsigned int          max_size,
                        bool                  debug,
                        FusionSHMPoolShared **ret_pool )
{
     int              i;
     DirectResult     ret;
     FusionSHM       *shm;
     FusionSHMShared *shared;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( name != NULL );
     D_ASSERT( max_size > 0 );
     D_ASSERT( ret_pool != NULL );

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p [%d], '%s', %d, %p, %sdebug )\n", __FUNCTION__,
                 world, world->shared->world_index, name, max_size, ret_pool, debug ? "" : "non-" );

#if !DIRECT_BUILD_DEBUGS
     debug = false;
#endif

     shm = &world->shm;

     D_MAGIC_ASSERT( shm, FusionSHM );

     shared = shm->shared;

     D_MAGIC_ASSERT( shared, FusionSHMShared );

     if (max_size < 8192) {
          D_ERROR( "Fusion/SHMPool: Maximum size (%d) should be 8192 at least!\n", max_size );
          return DFB_INVARG;
     }

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret)
          goto error;

     if (shared->num_pools == FUSION_SHM_MAX_POOLS) {
          D_ERROR( "Fusion/SHMPool: Maximum number of pools (%d) already reached!\n", FUSION_SHM_MAX_POOLS );
          ret = DFB_LIMITEXCEEDED;
          goto error;
     }

     for (i=0; i<FUSION_SHM_MAX_POOLS; i++) {
          if (!shared->pools[i].active)
               break;

          D_MAGIC_ASSERT( &shared->pools[i], FusionSHMPoolShared );
          D_MAGIC_ASSUME( &shm->pools[i], FusionSHMPool );
     }

     D_ASSERT( i < FUSION_SHM_MAX_POOLS );

     D_DEBUG_AT( Fusion_SHMPool, "  -> index %d\n", i );

     memset( &shm->pools[i], 0, sizeof(FusionSHMPool) );
     memset( &shared->pools[i], 0, sizeof(FusionSHMPoolShared) );

     shared->pools[i].index = i;

     ret = init_pool( shm, &shm->pools[i], &shared->pools[i], name, max_size, debug );
     if (ret)
          goto error;

     shared->num_pools++;

     fusion_skirmish_dismiss( &shared->lock );

     *ret_pool = &shared->pools[i];

     D_DEBUG_AT( Fusion_SHMPool, "  -> %p\n", *ret_pool );

     return DFB_OK;


error:
     fusion_skirmish_dismiss( &shared->lock );

     return ret;
}

DirectResult
fusion_shm_pool_destroy( FusionWorld         *world,
                         FusionSHMPoolShared *pool )
{
     DirectResult     ret;
     FusionSHM       *shm;
     FusionSHMShared *shared;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p )\n", __FUNCTION__, world, pool );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     shm = &world->shm;

     D_MAGIC_ASSERT( shm, FusionSHM );

     shared = shm->shared;

     D_MAGIC_ASSERT( shared, FusionSHMShared );

     D_ASSERT( shared == pool->shm );

     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret)
          return ret;

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret) {
          fusion_skirmish_dismiss( &pool->lock );
          return ret;
     }

     D_ASSERT( pool->active );
     D_ASSERT( pool->index >= 0 );
     D_ASSERT( pool->index < FUSION_SHM_MAX_POOLS );
     D_ASSERT( pool->pool_id == shm->pools[pool->index].pool_id );
     D_ASSERT( pool == &shared->pools[pool->index] );

     D_MAGIC_ASSERT( &shm->pools[pool->index], FusionSHMPool );

     shutdown_pool( shm, &shm->pools[pool->index], pool );

     shared->num_pools--;

     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

DirectResult
fusion_shm_pool_attach( FusionSHM           *shm,
                        FusionSHMPoolShared *pool )
{
     DirectResult     ret;
     FusionSHMShared *shared;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p )\n", __FUNCTION__, shm, pool );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     shared = shm->shared;

     D_MAGIC_ASSERT( shared, FusionSHMShared );

     D_ASSERT( shared == pool->shm );

     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          return ret;
     }

     D_ASSERT( pool->active );
     D_ASSERT( pool->index >= 0 );
     D_ASSERT( pool->index < FUSION_SHM_MAX_POOLS );
     D_ASSERT( pool == &shared->pools[pool->index] );
     D_ASSERT( !shm->pools[pool->index].attached );

     ret = join_pool( shm, &shm->pools[pool->index], pool );

     fusion_skirmish_dismiss( &pool->lock );

     return ret;
}

DirectResult
fusion_shm_pool_detach( FusionSHM           *shm,
                        FusionSHMPoolShared *pool )
{
     DirectResult     ret;
     FusionSHMShared *shared;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p )\n", __FUNCTION__, shm, pool );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     shared = shm->shared;

     D_MAGIC_ASSERT( shared, FusionSHMShared );

     D_ASSERT( shared == pool->shm );

     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          fusion_skirmish_dismiss( &shared->lock );
          return ret;
     }

     D_ASSERT( pool->active );
     D_ASSERT( pool->index >= 0 );
     D_ASSERT( pool->index < FUSION_SHM_MAX_POOLS );
     D_ASSERT( pool->pool_id == shm->pools[pool->index].pool_id );
     D_ASSERT( pool == &shared->pools[pool->index] );
     D_ASSERT( shm->pools[pool->index].attached );

     D_MAGIC_ASSERT( &shm->pools[pool->index], FusionSHMPool );

     leave_pool( shm, &shm->pools[pool->index], pool );

     fusion_skirmish_dismiss( &pool->lock );

     return DFB_OK;
}

DirectResult
fusion_shm_pool_allocate( FusionSHMPoolShared  *pool,
                          int                   size,
                          bool                  clear,
                          bool                  lock,
                          void                **ret_data )
{
     DirectResult  ret;
     void         *data;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %d, %sclear, %p )\n", __FUNCTION__,
                 pool, size, clear ? "" : "un", ret_data );

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     D_ASSERT( size > 0 );
     D_ASSERT( ret_data != NULL );

     if (lock) {
          ret = fusion_skirmish_prevail( &pool->lock );
          if (ret)
               return ret;
     }

     __shmalloc_brk( pool->heap, 0 );

     data = _fusion_shmalloc( pool->heap, size );
     if (!data) {
          if (lock)
               fusion_skirmish_dismiss( &pool->lock );
          return DFB_NOSHAREDMEMORY;
     }

     if (clear)
          memset( data, 0, size );

     *ret_data = data;

     if (lock)
          fusion_skirmish_dismiss( &pool->lock );

     return DFB_OK;
}

DirectResult
fusion_shm_pool_reallocate( FusionSHMPoolShared  *pool,
                            void                 *data,
                            int                   size,
                            bool                  lock,
                            void                **ret_data )
{
     DirectResult  ret;
     void         *new_data;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %d, %p )\n",
                 __FUNCTION__, pool, data, size, ret_data );

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     D_ASSERT( data != NULL );
     D_ASSERT( data >= pool->addr_base );
     D_ASSERT( data < pool->addr_base + pool->max_size );
     D_ASSERT( size > 0 );
     D_ASSERT( ret_data != NULL );

     if (lock) {
          ret = fusion_skirmish_prevail( &pool->lock );
          if (ret)
               return ret;
     }

     __shmalloc_brk( pool->heap, 0 );

     new_data = _fusion_shrealloc( pool->heap, data, size );
     if (!new_data) {
          if (lock)
               fusion_skirmish_dismiss( &pool->lock );
          return DFB_NOSHAREDMEMORY;
     }

     *ret_data = new_data;

     if (lock)
          fusion_skirmish_dismiss( &pool->lock );

     return DFB_OK;
}

DirectResult
fusion_shm_pool_deallocate( FusionSHMPoolShared *pool,
                            void                *data,
                            bool                 lock )
{
     DirectResult ret;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p )\n", __FUNCTION__, pool, data );

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     D_ASSERT( data != NULL );
     D_ASSERT( data >= pool->addr_base );
     D_ASSERT( data < pool->addr_base + pool->max_size );

     if (lock) {
          ret = fusion_skirmish_prevail( &pool->lock );
          if (ret)
               return ret;
     }

     __shmalloc_brk( pool->heap, 0 );

     _fusion_shfree( pool->heap, data );

     if (lock)
          fusion_skirmish_dismiss( &pool->lock );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DirectResult
init_pool( FusionSHM           *shm,
           FusionSHMPool       *pool,
           FusionSHMPoolShared *shared,
           const char          *name,
           unsigned int         max_size,
           bool                 debug )
{
     DirectResult         ret;
     int                  fd;
     int                  size;
     FusionWorld         *world;
     FusionSHMPoolNew     pool_new    = {0};
     FusionSHMPoolAttach  pool_attach = {0};
     FusionEntryInfo      info;
     char                 buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 32];

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p, '%s', %d, %sdebug )\n",
                 __FUNCTION__, shm, pool, shared, name, max_size, debug ? "" : "non-" );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_ASSERT( name != NULL );
     D_ASSERT( max_size > sizeof(shmalloc_heap) );

     world = shm->world;

     D_MAGIC_ASSERT( world, FusionWorld );

     D_ASSERT( pool != NULL );
     D_ASSERT( shared != NULL );

     /* Fill out information for new pool. */
     pool_new.max_size = max_size;

     pool_new.max_size += BLOCKALIGN(sizeof(shmalloc_heap)) +
                          BLOCKALIGN( (max_size + BLOCKSIZE-1) / BLOCKSIZE * sizeof(shmalloc_info) );

     /* Create the new pool. */
     while (ioctl( world->fusion_fd, FUSION_SHMPOOL_NEW, &pool_new )) {
          if (errno == EINTR)
               continue;

          D_PERROR( "Fusion/SHM: FUSION_SHMPOOL_NEW failed!\n" );
          return DFB_FUSION;
     }

     /* Set the pool info. */
     info.type = FT_SHMPOOL;
     info.id   = pool_new.pool_id;

     snprintf( info.name, sizeof(info.name), "%s", name );

     ioctl( world->fusion_fd, FUSION_ENTRY_SET_INFO, &info );


     /* Set pool to attach to. */
     pool_attach.pool_id = pool_new.pool_id;

     /* Attach to the pool. */
     while (ioctl( world->fusion_fd, FUSION_SHMPOOL_ATTACH, &pool_attach )) {
          if (errno == EINTR)
               continue;

          D_PERROR( "Fusion/SHM: FUSION_SHMPOOL_ATTACH failed!\n" );

          while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DESTROY, &shared->pool_id )) {
               if (errno != EINTR) {
                    D_PERROR( "Fusion/SHM: FUSION_SHMPOOL_DESTROY failed!\n" );
                    break;
               }
          }

          return DFB_FUSION;
     }


     /* Generate filename. */
     snprintf( buf, sizeof(buf), "%s/fusion.%d.%d", shm->shared->tmpfs,
               fusion_world_index( shm->world ), pool_new.pool_id );

     /* Initialize the heap. */
     ret = __shmalloc_init_heap( shm, buf, pool_new.addr_base, max_size, &fd, &size );
     if (ret) {
          while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DESTROY, &shared->pool_id )) {
               if (errno != EINTR) {
                    D_PERROR( "Fusion/SHM: FUSION_SHMPOOL_DESTROY failed!\n" );
                    break;
               }
          }

          return ret;
     }


     /* Initialize local data. */
     pool->attached = true;
     pool->shm      = shm;
     pool->shared   = shared;
     pool->pool_id  = pool_new.pool_id;
     pool->fd       = fd;
     pool->filename = D_STRDUP( buf );

     /* Initialize shared data. */
     shared->active     = true;
     shared->debug      = debug;
     shared->shm        = shm->shared;
     shared->max_size   = pool_new.max_size;
     shared->pool_id    = pool_new.pool_id;
     shared->addr_base  = pool_new.addr_base;
     shared->heap       = pool_new.addr_base;
     shared->heap->pool = shared;

     fusion_skirmish_init( &shared->lock, name, world );


     D_MAGIC_SET( pool, FusionSHMPool );
     D_MAGIC_SET( shared, FusionSHMPoolShared );


     shared->name = SHSTRDUP( shared, name );

     return DFB_OK;
}

static DirectResult
join_pool( FusionSHM           *shm,
           FusionSHMPool       *pool,
           FusionSHMPoolShared *shared )
{
     DirectResult         ret;
     int                  fd;
     FusionWorld         *world;
     FusionSHMPoolAttach  pool_attach = {0};
     char                 buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 32];

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p )\n", __FUNCTION__, shm, pool, shared );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_MAGIC_ASSERT( shared, FusionSHMPoolShared );

#if !DIRECT_BUILD_DEBUGS
     if (shared->debug) {
          D_ERROR( "Fusion/SHM: Can't join debug enabled pool with pure-release library!\n" );
          return DFB_UNSUPPORTED;
     }
#endif

     world = shm->world;

     D_MAGIC_ASSERT( world, FusionWorld );


     /* Set pool to attach to. */
     pool_attach.pool_id = shared->pool_id;

     /* Attach to the pool. */
     while (ioctl( world->fusion_fd, FUSION_SHMPOOL_ATTACH, &pool_attach )) {
          if (errno == EINTR)
               continue;

          D_PERROR( "Fusion/SHM: FUSION_SHMPOOL_ATTACH failed!\n" );
          return DFB_FUSION;
     }


     /* Generate filename. */
     snprintf( buf, sizeof(buf), "%s/fusion.%d.%d", shm->shared->tmpfs,
               fusion_world_index( shm->world ), shared->pool_id );

     /* Join the heap. */
     ret = __shmalloc_join_heap( shm, buf, pool_attach.addr_base, shared->max_size, &fd );
     if (ret) {
          while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DETACH, &shared->pool_id )) {
               if (errno != EINTR) {
                    D_PERROR( "Fusion/SHM: FUSION_SHMPOOL_DETACH failed!\n" );
                    break;
               }
          }

          return ret;
     }


     /* Initialize local data. */
     pool->attached = true;
     pool->shm      = shm;
     pool->shared   = shared;
     pool->pool_id  = shared->pool_id;
     pool->fd       = fd;
     pool->filename = D_STRDUP( buf );


     D_MAGIC_SET( pool, FusionSHMPool );

     return DFB_OK;
}

static void
leave_pool( FusionSHM           *shm,
            FusionSHMPool       *pool,
            FusionSHMPoolShared *shared )
{
     FusionWorld *world;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p )\n", __FUNCTION__, shm, pool, shared );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( pool, FusionSHMPool );
     D_MAGIC_ASSERT( shared, FusionSHMPoolShared );

     world = shm->world;

     D_MAGIC_ASSERT( world, FusionWorld );

     while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DETACH, &shared->pool_id )) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/SHM: FUSION_SHMPOOL_DETACH failed!\n" );
               break;
          }
     }

     if (munmap( shared->addr_base, shared->max_size ))
          D_PERROR( "Fusion/SHM: Could not munmap shared memory file '%s'!\n", pool->filename );

     if (pool->fd != -1 && close( pool->fd ))
          D_PERROR( "Fusion/SHM: Could not close shared memory file '%s'!\n", pool->filename );

     pool->attached = false;

     D_FREE( pool->filename );

     D_MAGIC_CLEAR( pool );
}

static void
shutdown_pool( FusionSHM           *shm,
               FusionSHMPool       *pool,
               FusionSHMPoolShared *shared )
{
     FusionWorld *world;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p )\n", __FUNCTION__, shm, pool, shared );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( pool, FusionSHMPool );
     D_MAGIC_ASSERT( shared, FusionSHMPoolShared );

     world = shm->world;

     D_MAGIC_ASSERT( world, FusionWorld );

     SHFREE( shared, shared->name );

     fusion_dbg_print_memleaks( shared );

     while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DESTROY, &shared->pool_id )) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/SHM: FUSION_SHMPOOL_DESTROY failed!\n" );
               break;
          }
     }

     if (munmap( shared->addr_base, shared->max_size ))
          D_PERROR( "Fusion/SHM: Could not munmap shared memory file '%s'!\n", pool->filename );

     if (pool->fd != -1 && close( pool->fd ))
          D_PERROR( "Fusion/SHM: Could not close shared memory file '%s'!\n", pool->filename );

     if (unlink( pool->filename ))
          D_PERROR( "Fusion/SHM: Could not unlink shared memory file '%s'!\n", pool->filename );

     shared->active = false;

     pool->attached = false;

     D_FREE( pool->filename );

     D_MAGIC_CLEAR( pool );

     fusion_skirmish_destroy( &shared->lock );

     D_MAGIC_CLEAR( shared );
}

/**********************************************************************************************************************/

void
_fusion_shmpool_process( FusionWorld          *world,
                         int                   pool_id,
                         FusionSHMPoolMessage *msg )
{
     int              i;
     DirectResult     ret;
     FusionSHM       *shm;
     FusionSHMShared *shared;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %d, %p )\n", __FUNCTION__, world, pool_id, msg );

     D_MAGIC_ASSERT( world, FusionWorld );

     shm = &world->shm;

     D_MAGIC_ASSERT( shm, FusionSHM );

     shared = shm->shared;

     D_MAGIC_ASSERT( shared, FusionSHMShared );

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret)
          return;

     for (i=0; i<FUSION_SHM_MAX_POOLS; i++) {
          if (shm->pools[i].attached) {
               D_MAGIC_ASSERT( &shm->pools[i], FusionSHMPool );

               if (shm->pools[i].pool_id == pool_id) {
                    switch (msg->type) {
                         case FSMT_REMAP:
                              break;

                         case FSMT_UNMAP:
                              D_UNIMPLEMENTED();
                              break;
                    }

                    break;
               }
          }
     }

     fusion_skirmish_dismiss( &shared->lock );
}

