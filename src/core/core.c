/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#define _GNU_SOURCE

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>

#include <pthread.h>

#include <core/fusion/fusion.h>
#include <core/fusion/arena.h>
#include <core/fusion/list.h>
#include <core/fusion/shmalloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core.h>
#include <core/core_parts.h>
#include <core/layer_region.h>
#include <core/palette.h>
#include <core/sig.h>
#include <core/surfaces.h>
#include <core/system.h>
#include <core/windows.h>

#include <misc/mem.h>
#include <misc/memcpy.h>
#include <misc/util.h>


#define DIRECTFB_CORE_ABI     1

/******************************************************************************/

/*
 * one entry in the cleanup stack
 */
struct _CoreCleanup {
     FusionLink       link;

     CoreCleanupFunc  func;        /* the cleanup function to be called */
     void            *data;        /* context of the cleanup function */
     bool             emergency;   /* if true, cleanup is also done during
                                      emergency shutdown (from signal hadler) */
};

struct __DFB_CoreDFBShared {
     FusionObjectPool *layer_region_pool;
     FusionObjectPool *palette_pool;
     FusionObjectPool *surface_pool;
     FusionObjectPool *window_pool;
};

struct __DFB_CoreDFB {
     int               refs;

     int               fusion_id;

     FusionArena      *arena;

     CoreDFBShared    *shared;

     bool              master;

     FusionLink       *cleanups;
};

/******************************************************************************/

/*
 * ckecks if stack is clean, otherwise prints warning, then calls core_deinit()
 */
static void dfb_core_deinit_check();

static void dfb_core_process_cleanups( CoreDFB *core, bool emergency );

/******************************************************************************/

static int dfb_core_arena_initialize( FusionArena *arena,
                                      void        *ctx );
static int dfb_core_arena_join      ( FusionArena *arena,
                                      void        *ctx );
static int dfb_core_arena_leave     ( FusionArena *arena,
                                      void        *ctx,
                                      bool         emergency );
static int dfb_core_arena_shutdown  ( FusionArena *arena,
                                      void        *ctx,
                                      bool         emergency );

/******************************************************************************/

static CorePart *core_parts[] = {
     &dfb_core_clipboard,
     &dfb_core_colorhash,
     &dfb_core_system,
     &dfb_core_input,
     &dfb_core_gfxcard,
     &dfb_core_layers
};

#define NUM_CORE_PARTS ((int)(sizeof(core_parts)/sizeof(CorePart*)))

#ifdef DFB_DYNAMIC_LINKING
/*
 * the library handle for dlopen'ing ourselves
 */
static void* dfb_lib_handle = NULL;
#endif

/******************************************************************************/

static CoreDFB         *core_dfb      = NULL;
static pthread_mutex_t  core_dfb_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/******************************************************************************/

DFBResult
dfb_core_create( CoreDFB **ret_core )
{
#ifdef USE_MMX
     char *mmx_string = " (with MMX support)";
#else
     char *mmx_string = "";
#endif
     int      ret;
     int      world;
#ifndef FUSION_FAKE
     char     buf[16];
#endif
     CoreDFB *core;

     DFB_ASSERT( ret_core != NULL );
     DFB_ASSERT( dfb_config != NULL );

     DEBUGMSG( "DirectFB/Core: %s...\n", __FUNCTION__ );

     pthread_mutex_lock( &core_dfb_lock );

     DFB_ASSERT( core_dfb == NULL || core_dfb->refs > 0 );

     if (core_dfb) {
          core_dfb->refs++;

          *ret_core = core_dfb;

          pthread_mutex_unlock( &core_dfb_lock );

          return DFB_OK;
     }


#ifdef FUSION_FAKE
     INITMSG( "Single Application Core.%s ("BUILDTIME")\n", mmx_string );
#else
     INITMSG( "Multi Application Core.%s ("BUILDTIME")\n", mmx_string );
#endif

#ifdef DFB_DYNAMIC_LINKING
     if (!dfb_lib_handle)
#ifdef RTLD_GLOBAL
          dfb_lib_handle = dlopen(SOPATH, RTLD_GLOBAL|RTLD_LAZY);
#else
          /* RTLD_GLOBAL is not defined on OpenBSD */
          dfb_lib_handle = dlopen(SOPATH, RTLD_LAZY);
#endif
#endif

     ret = dfb_system_lookup();
     if (ret) {
          pthread_mutex_unlock( &core_dfb_lock );

          return ret;
     }


     /* Allocate local core structure. */
     core = DFBCALLOC( 1, sizeof(CoreDFB) );
     if (!core) {
          pthread_mutex_unlock( &core_dfb_lock );

          return DFB_NOSYSTEMMEMORY;
     }

     core->refs = 1;


#ifndef FUSION_FAKE
     dfb_system_thread_init();
#endif

     dfb_find_best_memcpy();

     setpgid( 0, 0 );

     core->fusion_id = fusion_init( dfb_config->session,
                                    DIRECTFB_CORE_ABI, &world );
     if (core->fusion_id < 0) {
          DFBFREE( core );

          pthread_mutex_unlock( &core_dfb_lock );

          return DFB_FUSION;
     }

#ifndef FUSION_FAKE
     DEBUGMSG( "DirectFB/core: world %d, fusion id %d\n",
               world, core->fusion_id );

     snprintf( buf, sizeof(buf), "%d", world );

     setenv( "DIRECTFB_SESSION", buf, true );
#endif

     if (dfb_config->sync) {
          INITMSG( "DirectFB/core: doing sync()...\n" );
          sync();
     }

     core_dfb = core;

     dfb_sig_install_handlers( core );


     if (fusion_arena_enter( "DirectFB/Core",
                             dfb_core_arena_initialize, dfb_core_arena_join,
                             core, &core->arena, &ret ) || ret)
     {
          DFBFREE( core );
          core_dfb = NULL;

          dfb_sig_remove_handlers( core );

          pthread_mutex_unlock( &core_dfb_lock );

          return ret ? ret : DFB_FUSION;
     }


     if (dfb_config->block_all_signals)
          dfb_sig_block_all();

     if (dfb_config->deinit_check)
          atexit( dfb_core_deinit_check );


     *ret_core = core;

     pthread_mutex_unlock( &core_dfb_lock );

     return DFB_OK;
}

DFBResult
dfb_core_destroy( CoreDFB *core, bool emergency )
{
     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->refs > 0 );
     DFB_ASSERT( core == core_dfb );

     DEBUGMSG( "DirectFB/Core: %s...\n", __FUNCTION__ );

     pthread_mutex_lock( &core_dfb_lock );

     if (!emergency && --core->refs) {
          pthread_mutex_unlock( &core_dfb_lock );
          return DFB_OK;
     }

     dfb_sig_remove_handlers( core );

     if (core->master) {
          if (emergency) {
               fusion_kill( 0, SIGKILL, 1000 );
          }
          else {
               fusion_kill( 0, SIGTERM, 5000 );
               fusion_kill( 0, SIGKILL, 2000 );
          }
     }

     dfb_core_process_cleanups( core, emergency );

     while (fusion_arena_exit( core->arena, dfb_core_arena_shutdown,
                               core->master ? NULL : dfb_core_arena_leave,
                               core, emergency, NULL ) == FUSION_INUSE)
     {
          ONCE( "waiting for DirectFB slaves to terminate" );
          usleep( 100000 );
     }

     fusion_exit();

     DFBFREE( core );
     core_dfb = NULL;

     pthread_mutex_unlock( &core_dfb_lock );

     return DFB_OK;
}

CoreLayerRegion *
dfb_core_create_layer_region( CoreDFB *core )
{
     DFB_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     DFB_ASSERT( core->shared->layer_region_pool != NULL );

     return (CoreLayerRegion*) fusion_object_create( core->shared->layer_region_pool );
}

CorePalette *
dfb_core_create_palette( CoreDFB *core )
{
     DFB_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     DFB_ASSERT( core->shared->palette_pool != NULL );

     return (CorePalette*) fusion_object_create( core->shared->palette_pool );
}

CoreSurface *
dfb_core_create_surface( CoreDFB *core )
{
     DFB_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     DFB_ASSERT( core->shared->surface_pool != NULL );

     return (CoreSurface*) fusion_object_create( core->shared->surface_pool );
}

CoreWindow *
dfb_core_create_window( CoreDFB *core )
{
     DFB_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );
     DFB_ASSERT( core->shared->window_pool != NULL );

     return (CoreWindow*) fusion_object_create( core->shared->window_pool );
}

FusionResult
dfb_core_enum_surfaces( CoreDFB               *core,
                        FusionObjectCallback   callback,
                        void                  *ctx )
{
     DFB_ASSERT( core != NULL || core_dfb != NULL );

     if (!core)
          core = core_dfb;

     return fusion_object_pool_enum( core->shared->surface_pool,
                                     callback, ctx );
}

bool
dfb_core_is_master( CoreDFB *core )
{
     DFB_ASSUME( core != NULL );
     DFB_ASSERT( core_dfb != NULL );

     if (!core)
          core = core_dfb;

     return core->master;
}

FusionArena *
dfb_core_arena( CoreDFB *core )
{
     DFB_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     DFB_ASSERT( core != NULL );

     return core->arena;
}

DFBResult
dfb_core_suspend( CoreDFB *core )
{
     DFBResult ret;

     DFB_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     DFB_ASSERT( core != NULL );

     if (!core->master)
          return DFB_ACCESSDENIED;

     ret = dfb_core_layers.Suspend( core );
     if (ret)
          return ret;

     ret = dfb_core_input.Suspend( core );
     if (ret)
          return ret;

     ret = dfb_core_gfxcard.Suspend( core );
     if (ret)
          return ret;

     return DFB_OK;
}

DFBResult
dfb_core_resume( CoreDFB *core )
{
     DFBResult ret;

     DFB_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     DFB_ASSERT( core != NULL );

     if (!core->master)
          return DFB_ACCESSDENIED;

     ret = dfb_core_gfxcard.Resume( core );
     if (ret)
          return ret;

     ret = dfb_core_input.Resume( core );
     if (ret)
          return ret;

     ret = dfb_core_layers.Resume( core );
     if (ret)
          return ret;

     return DFB_OK;
}

CoreCleanup *
dfb_core_cleanup_add( CoreDFB         *core,
                      CoreCleanupFunc  func,
                      void            *data,
                      bool             emergency )
{
     CoreCleanup *cleanup;

     DFB_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     DFB_ASSERT( core != NULL );

     cleanup = DFBCALLOC( 1, sizeof(CoreCleanup) );

     cleanup->func      = func;
     cleanup->data      = data;
     cleanup->emergency = emergency;

     fusion_list_prepend( &core->cleanups, &cleanup->link );

     return cleanup;
}

void
dfb_core_cleanup_remove( CoreDFB     *core,
                         CoreCleanup *cleanup )
{
     DFB_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     DFB_ASSERT( core != NULL );

     fusion_list_remove( &core->cleanups, &cleanup->link );

     DFBFREE( cleanup );
}

/******************************************************************************/

static void
dfb_core_deinit_check()
{
     if (core_dfb && core_dfb->refs) {
          DEBUGMSG( "DirectFB/core: WARNING - Application "
                    "exitted without deinitialization of DirectFB!\n" );
          dfb_core_destroy( core_dfb, true );
     }

#ifdef DFB_DEBUG
     dfb_dbg_print_memleaks();
#endif
}

static void
dfb_core_process_cleanups( CoreDFB *core, bool emergency )
{
     while (core->cleanups) {
          CoreCleanup *cleanup = (CoreCleanup*) core->cleanups;

          core->cleanups = core->cleanups->next;

          if (cleanup->emergency || !emergency)
               cleanup->func( cleanup->data, emergency );

          DFBFREE( cleanup );
     }
}

/******************************************************************************/

static int
dfb_core_shutdown( CoreDFB *core, bool emergency )
{
     int            i;
     CoreDFBShared *shared;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );

     shared = core->shared;

     for (i=NUM_CORE_PARTS-1; i>=0; i--)
          dfb_core_part_shutdown( core, core_parts[i], emergency );

     return 0;
}

static DFBResult
dfb_core_initialize( CoreDFB *core )
{
     int            i;
     CoreDFBShared *shared;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );

     shared = core->shared;

     shared->layer_region_pool = dfb_layer_region_pool_create();
     shared->palette_pool      = dfb_palette_pool_create();
     shared->surface_pool      = dfb_surface_pool_create();
     shared->window_pool       = dfb_window_pool_create();

     for (i=0; i<NUM_CORE_PARTS; i++) {
          DFBResult ret;

          if ((ret = dfb_core_part_initialize( core, core_parts[i] ))) {
               dfb_core_shutdown( core, true );
               return ret;
          }
     }

     return 0;
}

static int
dfb_core_leave( CoreDFB *core, bool emergency )
{
     int i;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );

     for (i=NUM_CORE_PARTS-1; i>=0; i--)
          dfb_core_part_leave( core, core_parts[i], emergency );

     return 0;
}

static int
dfb_core_join( CoreDFB *core )
{
     int i;

     DFB_ASSERT( core != NULL );
     DFB_ASSERT( core->shared != NULL );

     for (i=0; i<NUM_CORE_PARTS; i++) {
          DFBResult ret;

          if ((ret = dfb_core_part_join( core, core_parts[i] ))) {
               dfb_core_leave( core, true );
               return ret;
          }
     }

     return 0;
}

/******************************************************************************/

static int
dfb_core_arena_initialize( FusionArena *arena,
                           void        *ctx )
{
     DFBResult      ret;
     CoreDFB       *core = ctx;
     CoreDFBShared *shared;

     DEBUGMSG( "DirectFB/Core: Initializing...\n" );

     /* Allocate shared structure. */
     shared = SHCALLOC( 1, sizeof(CoreDFBShared) );
     if (!shared) {
          ERRORMSG( "DirectFB/Core: Could not allocate (shared) memory!\n" );
          return DFB_NOSYSTEMMEMORY;
     }

     core->shared = shared;
     core->master = true;

     /* Initialize. */
     ret = dfb_core_initialize( core );
     if (ret) {
          SHFREE( shared );
          return ret;
     }

     /* Register shared data. */
     fusion_arena_add_shared_field( arena, "Core/Shared", shared );

     return DFB_OK;
}

static int
dfb_core_arena_join( FusionArena *arena,
                     void        *ctx )
{
     DFBResult      ret;
     CoreDFB       *core = ctx;
     CoreDFBShared *shared;

     DEBUGMSG( "DirectFB/Core: Joining...\n" );

     /* Get shared data. */
     if (fusion_arena_get_shared_field( arena, "Core/Shared", (void**)&shared ))
          return DFB_FUSION;

     core->shared = shared;

     /* Join. */
     ret = dfb_core_join( core );
     if (ret)
          return ret;

     return DFB_OK;
}

static int
dfb_core_arena_leave( FusionArena *arena,
                      void        *ctx,
                      bool         emergency)
{
     DFBResult  ret;
     CoreDFB   *core = ctx;

     DEBUGMSG( "DirectFB/Core: Leaving...\n" );

     /* Leave. */
     ret = dfb_core_leave( core, emergency );
     if (ret)
          return ret;

     return DFB_OK;
}

static int
dfb_core_arena_shutdown( FusionArena *arena,
                         void        *ctx,
                         bool         emergency)
{
     DFBResult  ret;
     CoreDFB   *core = ctx;

     DEBUGMSG( "DirectFB/Core: Shutting down...\n" );

     if (!core->master) {
          CAUTION( "refusing shutdown in slave" );
          return dfb_core_leave( core, emergency );
     }

     /* Shutdown. */
     ret = dfb_core_shutdown( core, emergency );
     if (ret)
          return ret;

     return DFB_OK;
}

