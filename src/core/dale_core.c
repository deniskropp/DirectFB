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
#include <fusion/conf.h>
#include <fusion/fusion.h>
#include <fusion/shmalloc.h>
#include <fusion/object.h>

#include <core/dale_core.h>
#include <core/messenger.h>
#include <core/messenger_port.h>

#include <misc/dale_config.h>


#define FUSIONDALE_CORE_ABI   1

D_DEBUG_DOMAIN( Dale_Core, "FusionDale/Core", "FusionDale Core" );

/**********************************************************************************************************************/

struct __FD_CoreDaleShared {
     int                  magic;

     FusionObjectPool    *messenger_pool;
     FusionObjectPool    *messenger_port_pool;

     FusionSHMPoolShared *shmpool;
};

struct __FD_CoreDale {
     int                  magic;

     int                  refs;

     int                  fusion_id;

     FusionWorld         *world;
     FusionArena         *arena;

     CoreDaleShared      *shared;

     bool                 master;

     DirectSignalHandler *signal_handler;
};

/**********************************************************************************************************************/

static DirectSignalHandlerResult fd_core_signal_handler( int   num,
                                                         void *addr,
                                                         void *ctx );

/**********************************************************************************************************************/

static int fd_core_arena_initialize( FusionArena *arena,
                                     void        *ctx );
static int fd_core_arena_join      ( FusionArena *arena,
                                     void        *ctx );
static int fd_core_arena_leave     ( FusionArena *arena,
                                     void        *ctx,
                                     bool         emergency);
static int fd_core_arena_shutdown  ( FusionArena *arena,
                                     void        *ctx,
                                     bool         emergency);

/**********************************************************************************************************************/

static CoreDale        *core_dale      = NULL;
static pthread_mutex_t  core_dale_lock = PTHREAD_MUTEX_INITIALIZER;

DFBResult
fd_core_create( CoreDale **ret_core )
{
     int        ret;
     CoreDale *core = NULL;

     D_ASSERT( ret_core != NULL );

     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     /* Lock the core singleton mutex. */
     pthread_mutex_lock( &core_dale_lock );

     /* Core already created? */
     if (core_dale) {
          /* Increase its references. */
          core_dale->refs++;

          /* Return the core. */
          *ret_core = core_dale;

          /* Unlock the core singleton mutex. */
          pthread_mutex_unlock( &core_dale_lock );

          return DFB_OK;
     }

     /* Allocate local core structure. */
     core = D_CALLOC( 1, sizeof(CoreDale) );
     if (!core) {
          ret = D_OOM();
          goto error;
     }

     ret = fusion_enter( fusiondale_config->session, FUSIONDALE_CORE_ABI,
                         fusiondale_config->force_slave ? FER_SLAVE : FER_ANY, &core->world );
     if (ret)
          goto error;

     core->fusion_id = fusion_id( core->world );
     
     fusiondale_config->session = fusion_world_index( core->world );

#if FUSION_BUILD_MULTI
     D_DEBUG_AT( Dale_Core, "  -> world %d, fusion id %d\n", fusiondale_config->session, core->fusion_id );
#endif

     /* Initialize the references. */
     core->refs = 1;

     direct_signal_handler_add( -1, fd_core_signal_handler, core, &core->signal_handler );

     D_MAGIC_SET( core, CoreDale );

     /* Enter the FusionDale core arena. */
     if (fusion_arena_enter( core->world, "FusionDale/Core",
                             fd_core_arena_initialize, fd_core_arena_join,
                             core, &core->arena, &ret ) || ret)
     {
          D_MAGIC_CLEAR( core );
          ret = ret ? : DFB_FUSION;
          goto error;
     }

     /* Return the core and store the singleton. */
     *ret_core = core_dale = core;

     /* Unlock the core singleton mutex. */
     pthread_mutex_unlock( &core_dale_lock );

     return DFB_OK;


error:
     if (core) {
          if (core->world) {
               direct_signal_handler_remove( core->signal_handler );
               fusion_exit( core->world, false );
          }

          D_FREE( core );
     }

     pthread_mutex_unlock( &core_dale_lock );

     return ret;
}

DFBResult
fd_core_destroy( CoreDale *core, bool emergency )
{
     D_MAGIC_ASSERT( core, CoreDale );
     D_ASSERT( core == core_dale );

     D_DEBUG_AT( Dale_Core, "%s( %p, %semergency )\n", __FUNCTION__, core, emergency ? "" : "no " );

     /* Lock the core singleton mutex. */
     if (!emergency)
          pthread_mutex_lock( &core_dale_lock );

     /* Decrement and check references. */
     if (!emergency && --core->refs) {
          /* Unlock the core singleton mutex. */
          pthread_mutex_unlock( &core_dale_lock );

          return DFB_OK;
     }

     direct_signal_handler_remove( core->signal_handler );
     
     /* Exit the FusionDale core arena. */
     if (fusion_arena_exit( core->arena, fd_core_arena_shutdown,
                            core->master ? NULL : fd_core_arena_leave,
                            core, emergency, NULL ) == DFB_BUSY)
     {
          if (core->master) {
               if (emergency) {
                    fusion_kill( core->world, 0, SIGKILL, 1000 );
               }
               else {
                    fusion_kill( core->world, 0, SIGTERM, 5000 );
                    fusion_kill( core->world, 0, SIGKILL, 2000 );
               }
          }
          
          while (fusion_arena_exit( core->arena, fd_core_arena_shutdown,
                                    core->master ? NULL : fd_core_arena_leave,
                                    core, emergency, NULL ) == DFB_BUSY)
          {
               D_ONCE( "waiting for FusionDale slaves to terminate" );
               usleep( 100000 );
          }
     }

     fusion_exit( core->world, emergency );

     D_MAGIC_CLEAR( core );

     /* Deallocate local core structure. */
     D_FREE( core );

     /* Clear the singleton. */
     core_dale = NULL;

     /* Unlock the core singleton mutex. */
     if (!emergency)
          pthread_mutex_unlock( &core_dale_lock );

     return DFB_OK;
}

/**********************************************************************************************************************/

CoreMessenger *
fd_core_create_messenger( CoreDale *core )
{
     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->messenger_pool != NULL );

     /* Create a new object in the messenger pool. */
     return (CoreMessenger*) fusion_object_create( core->shared->messenger_pool, core->world );
}

CoreMessengerPort *
fd_core_create_messenger_port( CoreDale *core )
{
     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->messenger_port_pool != NULL );

     /* Create a new object in the messenger port pool. */
     return (CoreMessengerPort*) fusion_object_create( core->shared->messenger_port_pool, core->world );
}

/**********************************************************************************************************************/

DirectResult
fd_core_enum_messengers( CoreDale             *core,
                         FusionObjectCallback  callback,
                         void                 *ctx )
{
     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->messenger_pool != NULL );

     /* Enumerate objects in the messenger pool. */
     return fusion_object_pool_enum( core->shared->messenger_pool, callback, ctx );
}

DirectResult
fd_core_enum_messenger_ports( CoreDale             *core,
                              FusionObjectCallback  callback,
                              void                 *ctx )
{
     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->messenger_port_pool != NULL );

     /* Enumerate objects in the messenger port pool. */
     return fusion_object_pool_enum( core->shared->messenger_port_pool, callback, ctx );
}

DirectResult
fd_core_get_messenger( CoreDale        *core,
                       FusionObjectID   object_id,
                       CoreMessenger  **ret_messenger )
{
     DirectResult  ret;
     FusionObject *object;

     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->messenger_port_pool != NULL );

     D_ASSERT( ret_messenger != NULL );

     /* Enumerate objects in the messenger port pool. */
     ret = fusion_object_get( core->shared->messenger_pool, object_id, &object );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( (CoreMessenger*) object, CoreMessenger );

     *ret_messenger = (CoreMessenger*) object;

     return DFB_OK;
}

/**********************************************************************************************************************/

FusionWorld *
fd_core_world( CoreDale *core )
{
     D_MAGIC_ASSERT( core, CoreDale );
     D_ASSERT( core->world != NULL );

     return core->world;
}

FusionSHMPoolShared *
fd_core_shmpool( CoreDale *core )
{
     D_MAGIC_ASSERT( core, CoreDale );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->shmpool != NULL );

     return core->shared->shmpool;
}

/**********************************************************************************************************************/

static DirectSignalHandlerResult
fd_core_signal_handler( int num, void *addr, void *ctx )
{
     CoreDale *core = (CoreDale*) ctx;

     D_DEBUG_AT( Dale_Core, "%s( %d, %p, %p )\n", __FUNCTION__, num, addr, ctx );

     D_MAGIC_ASSERT( core, CoreDale );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core == core_dale );

     fd_core_destroy( core, true );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
fd_core_initialize( CoreDale *core )
{
     CoreDaleShared *shared = core->shared;
     
     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );

     /* create a pool for messenger (port) objects */
     shared->messenger_pool      = fd_messenger_pool_create( core->world );
     shared->messenger_port_pool = fd_messenger_port_pool_create( core->world );

     return DFB_OK;
}

static DFBResult
fd_core_join( CoreDale *core )
{
     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );

     /* really nothing to be done here, yet ;) */

     return DFB_OK;
}

static DFBResult
fd_core_leave( CoreDale *core )
{
     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );

     /* really nothing to be done here, yet ;) */

     return DFB_OK;
}

static DFBResult
fd_core_shutdown( CoreDale *core )
{
     CoreDaleShared *shared;

     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );
     D_ASSERT( core->shared != NULL );

     shared = core->shared;

     D_ASSERT( shared->messenger_pool != NULL );

     /* destroy messenger port object pool */
     fusion_object_pool_destroy( shared->messenger_port_pool, core->world );

     /* destroy messenger object pool */
     fusion_object_pool_destroy( shared->messenger_pool, core->world );

     return DFB_OK;
}

/**********************************************************************************************************************/

static int
fd_core_arena_initialize( FusionArena *arena,
                          void        *ctx )
{
     DFBResult            ret;
     CoreDale            *core   = ctx;
     CoreDaleShared      *shared;
     FusionSHMPoolShared *pool;

     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );

     /* Create the shared memory pool first! */
     ret = fusion_shm_pool_create( core->world, "FusionDale Main Pool", 0x1000000,
                                   fusion_config->debugshm, &pool );
     if (ret)
          return ret;

     /* Allocate shared structure in the new pool. */
     shared = SHCALLOC( pool, 1, sizeof(CoreDaleShared) );
     if (!shared) {
          fusion_shm_pool_destroy( core->world, pool );
          return D_OOSHM();
     }

     core->shared = shared;
     core->master = true;

     shared->shmpool = pool;

     /* Initialize. */
     ret = fd_core_initialize( core );
     if (ret) {
          SHFREE( pool, shared );
          fusion_shm_pool_destroy( core->world, pool );
          return ret;
     }

     /* Register shared data. */
     fusion_arena_add_shared_field( arena, "Core/Shared", shared );

     return DFB_OK;
}

static int
fd_core_arena_shutdown( FusionArena *arena,
                        void        *ctx,
                        bool         emergency)
{
     DFBResult            ret;
     CoreDale           *core = ctx;
     CoreDaleShared     *shared;
     FusionSHMPoolShared *pool;

     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );

     shared = core->shared;

     pool = shared->shmpool;

     if (!core->master) {
          D_DEBUG( "FusionDale/Core: Refusing shutdown in slave.\n" );
          return fd_core_leave( core );
     }

     /* Shutdown. */
     ret = fd_core_shutdown( core );
     if (ret)
          return ret;

     SHFREE( pool, shared );

     fusion_dbg_print_memleaks( pool );

     fusion_shm_pool_destroy( core->world, pool );

     return DFB_OK;
}

static int
fd_core_arena_join( FusionArena *arena,
                    void        *ctx )
{
     DFBResult        ret;
     CoreDale       *core   = ctx;
     CoreDaleShared *shared;

     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );

     /* Get shared data. */
     if (fusion_arena_get_shared_field( arena, "Core/Shared", (void*)&shared ))
          return DFB_FUSION;

     core->shared = shared;

     /* Join. */
     ret = fd_core_join( core );
     if (ret)
          return ret;

     return DFB_OK;
}

static int
fd_core_arena_leave( FusionArena *arena,
                     void        *ctx,
                     bool         emergency)
{
     DFBResult  ret;
     CoreDale *core = ctx;

     D_DEBUG_AT( Dale_Core, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, CoreDale );

     /* Leave. */
     ret = fd_core_leave( core );
     if (ret)
          return ret;

     return DFB_OK;
}

