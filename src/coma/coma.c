/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#include <pthread.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/util.h>

#include <fusion/arena.h>
#include <fusion/build.h>
#include <fusion/call.h>
#include <fusion/conf.h>
#include <fusion/fusion.h>
#include <fusion/hash.h>
#include <fusion/lock.h>
#include <fusion/object.h>
#include <fusion/reactor.h>
#include <fusion/shmalloc.h>

#include <coma/coma.h>
#include <coma/component.h>


D_DEBUG_DOMAIN( Coma_Core, "Coma/Core", "Coma Core" );

/**********************************************************************************************************************/

typedef struct {
     int                  magic;

     void                *ptr;
     unsigned int         size;

     FusionSHMPoolShared *pool;
} ThreadLocalSHM;

/**********************************************************************************************************************/

struct __COMA_ComaShared {
     int                  magic;

     FusionSkirmish       lock;

     FusionSHMPoolShared *shmpool;

     FusionObjectPool    *component_pool;
     FusionHash          *components;
};

struct __COMA_Coma {
     int                  magic;

     char                *name;

     int                  fusion_id;

     FusionWorld         *world;
     FusionArena         *arena;

     ComaShared          *shared;

     pthread_key_t        tlshm_key;
};

/**********************************************************************************************************************/

static int coma_arena_initialize( FusionArena *arena,
                                  void        *ctx );
static int coma_arena_join      ( FusionArena *arena,
                                  void        *ctx );
static int coma_arena_leave     ( FusionArena *arena,
                                  void        *ctx,
                                  bool         emergency );
static int coma_arena_shutdown  ( FusionArena *arena,
                                  void        *ctx,
                                  bool         emergency );

/**********************************************************************************************************************/

static void tlshm_destroy( void *arg );

/**********************************************************************************************************************/

DirectResult
coma_enter( FusionWorld *world, const char *name, Coma **ret_coma )
{
     Coma *coma;
     char  buf[128];
     int   ret = DFB_OK;

     D_ASSERT( world != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( ret_coma != NULL );

     D_DEBUG_AT( Coma_Core, "%s()\n", __FUNCTION__ );

     /* Allocate local coma structure. */
     coma = D_CALLOC( 1, sizeof(Coma) );
     if (!coma)
          return D_OOM();

     coma->world     = world;
     coma->fusion_id = fusion_id( world );
     coma->name      = D_STRDUP( name );

     pthread_key_create( &coma->tlshm_key, tlshm_destroy );

     D_MAGIC_SET( coma, Coma );

     snprintf( buf, sizeof(buf), "Coma/%s", name );

     /* Enter the Coma arena. */
     if (fusion_arena_enter( world, buf, coma_arena_initialize, coma_arena_join,
                             coma, &coma->arena, &ret ) || ret)
     {
          D_MAGIC_CLEAR( coma );
          D_FREE( coma->name );
          D_FREE( coma );
          return ret ? : DFB_FUSION;
     }

     /* Return the coma. */
     *ret_coma = coma;

     return DFB_OK;
}

DirectResult
coma_exit( Coma *coma, bool emergency )
{
     D_MAGIC_ASSERT( coma, Coma );

     D_DEBUG_AT( Coma_Core, "%s( %p, %semergency )\n", __FUNCTION__, coma, emergency ? "" : "no " );

     /* Exit the Coma arena. */
     fusion_arena_exit( coma->arena, coma_arena_shutdown, coma_arena_leave, coma, emergency, NULL );

     D_FREE( coma->name );

     D_MAGIC_CLEAR( coma );

     /* Deallocate local coma structure. */
     D_FREE( coma );

     return DFB_OK;
}

/**********************************************************************************************************************/

DirectResult
coma_create_component( Coma           *coma,
                       const char     *name,
                       ComaMethodFunc  func,
                       int             num_notifications,
                       void           *ctx,
                       ComaComponent **ret_component )
{
     DirectResult   ret;
     ComaShared    *shared;
     ComaComponent *component;

     D_MAGIC_ASSERT( coma, Coma );

     D_DEBUG_AT( Coma_Core, "%s( %p, '%s' )\n", __FUNCTION__, coma, name );

     shared = coma->shared;
     D_MAGIC_ASSERT( shared, ComaShared );

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret)
          return ret;

     /* Check for existence first. */
     component = fusion_hash_lookup( shared->components, name );
     if (component) {
          D_MAGIC_ASSERT( component, ComaComponent );
          D_WARN( "component '%s' already exists", name );
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_BUSY;
     }

     /* Create component object. */
     component = (ComaComponent*) fusion_object_create( shared->component_pool, coma->world );
     if (!component) {
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_FUSION;
     }

     /* Initialize component object. */
     ret = coma_component_init( component, coma, name, func, num_notifications, ctx );
     if (ret) {
          fusion_skirmish_dismiss( &shared->lock );
          fusion_object_destroy( &component->object );
          return ret;
     }

     /* Activate component object. */
     fusion_object_activate( &component->object );

     /* Insert new component into hash table. */
     ret = fusion_hash_insert( shared->components, component->name, component );
     if (ret) {
          D_DERROR( ret, "Coma/Core: fusion_hash_insert( '%s', %p ) failed!\n", name, component );
          fusion_skirmish_dismiss( &shared->lock );
          coma_component_unref( component );
          return ret;
     }

     ret = fusion_skirmish_notify( &shared->lock );
     if (ret)
          D_DERROR( ret, "Coma/Core: fusion_skirmish_notify() failed!\n" );

     fusion_skirmish_dismiss( &shared->lock );

     *ret_component = component;

     return DFB_OK;
}

DirectResult
coma_get_component( Coma           *coma,
                    const char     *name,
                    unsigned int    timeout,
                    ComaComponent **ret_component )
{
     DirectResult   ret;
     ComaShared    *shared;
     ComaComponent *component;

     D_MAGIC_ASSERT( coma, Coma );
     D_ASSERT( name != NULL );
     D_ASSERT( ret_component != NULL );

     D_DEBUG_AT( Coma_Core, "%s( %p, '%s' )\n", __FUNCTION__, coma, name );

     shared = coma->shared;
     D_MAGIC_ASSERT( shared, ComaShared );

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret)
          return ret;

     while ((component = fusion_hash_lookup( shared->components, name )) == NULL) {
          ret = fusion_skirmish_wait( &shared->lock, timeout );
          if (ret)
               return ret;
     }

     D_MAGIC_ASSERT( component, ComaComponent );

     ret = coma_component_ref( component );
     if (ret) {
          fusion_skirmish_dismiss( &shared->lock );
          return ret;
     }

     *ret_component = component;

     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
tlshm_destroy( void *arg )
{
     ThreadLocalSHM *tlshm = arg;

     D_MAGIC_ASSERT( tlshm, ThreadLocalSHM );

     if (tlshm->ptr)
          SHFREE( tlshm->pool, tlshm->ptr );

     D_MAGIC_CLEAR( tlshm );

     SHFREE( tlshm->pool, tlshm );
}

/**********************************************************************************************************************/

DirectResult
coma_get_local( Coma          *coma,
                unsigned int   bytes,
                void         **ret_ptr )
{
     ComaShared     *shared;
     ThreadLocalSHM *tlshm;

     D_MAGIC_ASSERT( coma, Coma );
     D_ASSERT( bytes > 0 );
     D_ASSERT( ret_ptr != NULL );

     shared = coma->shared;
     D_MAGIC_ASSERT( shared, ComaShared );

     tlshm = pthread_getspecific( coma->tlshm_key );
     if (!tlshm) {
          tlshm = SHCALLOC( shared->shmpool, 1, sizeof(ThreadLocalSHM) );
          if (!tlshm)
               return D_OOSHM();

          tlshm->pool = shared->shmpool;

          D_MAGIC_SET( tlshm, ThreadLocalSHM );

          pthread_setspecific( coma->tlshm_key, tlshm );
     }

     D_MAGIC_ASSERT( tlshm, ThreadLocalSHM );

     if (tlshm->size < bytes) {
          void *ptr = SHCALLOC( tlshm->pool, 1, bytes );

          if (!ptr)
               return D_OOSHM();

          if (tlshm->ptr)
               SHFREE( tlshm->pool, tlshm->ptr );

          tlshm->ptr  = ptr;
          tlshm->size = bytes;
     }

     *ret_ptr = tlshm->ptr;

     return DFB_OK;
}

DirectResult
coma_free_local( Coma *coma )
{
     ComaShared     *shared;
     ThreadLocalSHM *tlshm;

     D_MAGIC_ASSERT( coma, Coma );

     shared = coma->shared;
     D_MAGIC_ASSERT( shared, ComaShared );

     tlshm = pthread_getspecific( coma->tlshm_key );
     if (!tlshm)
          return DFB_ITEMNOTFOUND;

     D_MAGIC_ASSERT( tlshm, ThreadLocalSHM );

     if (!tlshm->ptr)
          return DFB_BUFFEREMPTY;

     SHFREE( tlshm->pool, tlshm->ptr );

     tlshm->ptr  = NULL;
     tlshm->size = 0;

     return DFB_OK;
}

/**********************************************************************************************************************/

FusionSHMPoolShared *
coma_shmpool( const Coma *coma )
{
     const ComaShared *shared;

     D_MAGIC_ASSERT( coma, Coma );

     shared = coma->shared;
     D_MAGIC_ASSERT( shared, ComaShared );

     return shared->shmpool;
}

FusionWorld *
coma_world( const Coma *coma )
{
     D_MAGIC_ASSERT( coma, Coma );

     return coma->world;
}

/**********************************************************************************************************************/

void
_coma_internal_remove_component( Coma          *coma,
                                 ComaComponent *component )
{
     DirectResult  ret;
     ComaShared   *shared;

     D_MAGIC_ASSERT( coma, Coma );
     D_MAGIC_ASSERT( component, ComaComponent );

     D_DEBUG_AT( Coma_Core, "%s( %p, '%s' )\n", __FUNCTION__, coma, component->name );

     shared = coma->shared;
     D_MAGIC_ASSERT( shared, ComaShared );

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret) {
          D_DERROR( ret, "Coma/Core: Could not lock core to remove component!\n" );
          return;
     }

     fusion_hash_remove( shared->components, component->name, NULL, NULL );

     fusion_skirmish_dismiss( &shared->lock );
}

/**********************************************************************************************************************/

static DFBResult
coma_initialize( Coma *coma )
{
     DFBResult     ret;
     ComaShared *shared;

     D_DEBUG_AT( Coma_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( coma, Coma );

     shared = coma->shared;
     D_MAGIC_ASSERT( shared, ComaShared );

     ret = fusion_hash_create( shared->shmpool, HASH_STRING, HASH_PTR, 7, &shared->components );
     if (ret)
          return ret;

     fusion_hash_set_autofree( shared->components, false, false );

     fusion_skirmish_init( &shared->lock, coma->name, coma->world );

     shared->component_pool = coma_component_pool_create( coma );

     return DFB_OK;
}

static DFBResult
coma_join( Coma *coma )
{
     D_DEBUG_AT( Coma_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( coma, Coma );

     /* really nothing to be done here, yet ;) */

     return DFB_OK;
}

static DFBResult
coma_leave( Coma *coma )
{
     D_DEBUG_AT( Coma_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( coma, Coma );

     /* really nothing to be done here, yet ;) */

     return DFB_OK;
}

static DFBResult
coma_shutdown( Coma *coma )
{
     ComaShared *shared;

     D_DEBUG_AT( Coma_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( coma, Coma );

     shared = coma->shared;
     D_MAGIC_ASSERT( shared, ComaShared );

     fusion_object_pool_destroy( shared->component_pool, coma->world );

     fusion_skirmish_destroy( &shared->lock );

     fusion_hash_destroy( shared->components );

     return DFB_OK;
}

/**********************************************************************************************************************/

static int
coma_arena_initialize( FusionArena *arena,
                          void        *ctx )
{
     DFBResult            ret;
     Coma              *coma = ctx;
     ComaShared        *shared;
     FusionSHMPoolShared *pool;

     D_DEBUG_AT( Coma_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( coma, Coma );

     /* Create the shared memory pool first! */
     ret = fusion_shm_pool_create( coma->world, "Coma Core", 0x1000000,
                                   fusion_config->debugshm, &pool );
     if (ret)
          return ret;

     /* Allocate shared structure in the new pool. */
     shared = SHCALLOC( pool, 1, sizeof(ComaShared) );
     if (!shared) {
          fusion_shm_pool_destroy( coma->world, pool );
          return D_OOSHM();
     }

     D_MAGIC_SET( shared, ComaShared );

     coma->shared = shared;

     shared->shmpool = pool;

     /* Initialize. */
     ret = coma_initialize( coma );
     if (ret) {
          SHFREE( pool, shared );
          fusion_shm_pool_destroy( coma->world, pool );
          return ret;
     }

     /* Register shared data. */
     fusion_arena_add_shared_field( arena, "Core/Shared", shared );

     return DFB_OK;
}

static int
coma_arena_shutdown( FusionArena *arena,
                        void        *ctx,
                        bool         emergency)
{
     DFBResult            ret;
     Coma              *coma = ctx;
     ComaShared        *shared;
     FusionSHMPoolShared *pool;

     D_DEBUG_AT( Coma_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( coma, Coma );

     shared = coma->shared;
     D_MAGIC_ASSERT( shared, ComaShared );

     pool = shared->shmpool;

     /* Shutdown. */
     ret = coma_shutdown( coma );
     if (ret)
          return ret;

     D_MAGIC_CLEAR( shared );

     SHFREE( pool, shared );

     fusion_dbg_print_memleaks( pool );

     fusion_shm_pool_destroy( coma->world, pool );

     return DFB_OK;
}

static int
coma_arena_join( FusionArena *arena,
                    void        *ctx )
{
     DFBResult     ret;
     Coma       *coma = ctx;
     ComaShared *shared;

     D_DEBUG_AT( Coma_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( coma, Coma );

     /* Get shared data. */
     if (fusion_arena_get_shared_field( arena, "Core/Shared", (void*)&shared ))
          return DFB_FUSION;

     coma->shared = shared;

     /* Join. */
     ret = coma_join( coma );
     if (ret)
          return ret;

     return DFB_OK;
}

static int
coma_arena_leave( FusionArena *arena,
                     void        *ctx,
                     bool         emergency)
{
     DFBResult  ret;
     Coma    *coma = ctx;

     D_DEBUG_AT( Coma_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( coma, Coma );

     /* Leave. */
     ret = coma_leave( coma );
     if (ret)
          return ret;

     return DFB_OK;
}

