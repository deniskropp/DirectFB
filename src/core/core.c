/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>

#include <pthread.h>

#include <fusion/fusion.h>
#include <fusion/arena.h>
#include <direct/list.h>
#include <fusion/shmalloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core.h>
#include <core/core_parts.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/system.h>
#include <core/windows.h>

#include <direct/build.h>
#include <direct/direct.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <fusion/build.h>

#include <misc/conf.h>
#include <misc/util.h>


#define DIRECTFB_CORE_ABI     22

extern CorePart dfb_core_clipboard;
extern CorePart dfb_core_colorhash;
extern CorePart dfb_core_gfxcard;
extern CorePart dfb_core_input;
extern CorePart dfb_core_layers;
extern CorePart dfb_core_screens;
extern CorePart dfb_core_system;
extern CorePart dfb_core_wm;

/******************************************************************************/

/*
 * one entry in the cleanup stack
 */
struct _CoreCleanup {
     DirectLink       link;

     CoreCleanupFunc  func;        /* the cleanup function to be called */
     void            *data;        /* context of the cleanup function */
     bool             emergency;   /* if true, cleanup is also done during
                                      emergency shutdown (from signal hadler) */
};

struct __DFB_CoreDFBShared {
     FusionObjectPool *layer_context_pool;
     FusionObjectPool *layer_region_pool;
     FusionObjectPool *palette_pool;
     FusionObjectPool *surface_pool;
     FusionObjectPool *window_pool;
};

struct __DFB_CoreDFB {
     int                      refs;

     int                      fusion_id;

     FusionArena             *arena;

     CoreDFBShared           *shared;

     bool                     master;

     DirectLink              *cleanups;

     DirectThreadInitHandler *init_handler;

     DirectSignalHandler     *signal_handler;
};

/******************************************************************************/

/*
 * ckecks if stack is clean, otherwise prints warning, then calls core_deinit()
 */
static void dfb_core_deinit_check();

static void dfb_core_thread_init_handler( DirectThread *thread, void *arg );

static void dfb_core_process_cleanups( CoreDFB *core, bool emergency );

static DirectSignalHandlerResult dfb_core_signal_handler( int   num,
                                                          void *addr,
                                                          void *ctx );

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
     &dfb_core_screens,
     &dfb_core_layers,
     &dfb_core_wm
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
static pthread_mutex_t  core_dfb_lock = DIRECT_UTIL_RECURSIVE_PTHREAD_MUTEX_INITIALIZER;

/******************************************************************************/

DFBResult
dfb_core_create( CoreDFB **ret_core )
{
     int      ret;
     int      world;
#if FUSION_BUILD_MULTI
     char     buf[16];
#endif
     CoreDFB *core;

     D_ASSERT( ret_core != NULL );
     D_ASSERT( dfb_config != NULL );

     D_DEBUG( "DirectFB/Core: %s...\n", __FUNCTION__ );

     pthread_mutex_lock( &core_dfb_lock );

     D_ASSERT( core_dfb == NULL || core_dfb->refs > 0 );

     if (core_dfb) {
          core_dfb->refs++;

          *ret_core = core_dfb;

          pthread_mutex_unlock( &core_dfb_lock );

          return DFB_OK;
     }

     direct_initialize();


     D_INFO( "DirectFB/Core: %s Application Core. ("BUILDTIME") %s%s\n",
             FUSION_BUILD_MULTI ? "Multi" : "Single",
             DIRECT_BUILD_DEBUG ? "[ DEBUG ]" : "",
             DIRECT_BUILD_TRACE ? "[ TRACE ]" : "" );


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
          direct_shutdown();
          return ret;
     }


     /* Allocate local core structure. */
     core = D_CALLOC( 1, sizeof(CoreDFB) );
     if (!core) {
          pthread_mutex_unlock( &core_dfb_lock );
          direct_shutdown();
          return DFB_NOSYSTEMMEMORY;
     }

     core->refs = 1;

     core->init_handler = direct_thread_add_init_handler( dfb_core_thread_init_handler, core );

#if FUSION_BUILD_MULTI
     dfb_system_thread_init();
#endif

     direct_find_best_memcpy();

     setpgid( 0, 0 );

     core_dfb = core;

     core->fusion_id = fusion_init( dfb_config->session,
#if DIRECT_BUILD_DEBUG
                                    -DIRECTFB_CORE_ABI,
#else
                                    DIRECTFB_CORE_ABI,
#endif
                                    &world );
     if (core->fusion_id < 0) {
          direct_thread_remove_init_handler( core->init_handler );
          D_FREE( core );
          core_dfb = NULL;
          pthread_mutex_unlock( &core_dfb_lock );
          direct_shutdown();
          return DFB_FUSION;
     }

#if FUSION_BUILD_MULTI
     D_DEBUG( "DirectFB/Core: world %d, fusion id %d\n", world, core->fusion_id );

     snprintf( buf, sizeof(buf), "%d", world );

     setenv( "DIRECTFB_SESSION", buf, true );
#endif

     if (dfb_config->sync) {
          D_INFO( "DirectFB/Core: doing sync()...\n" );
          sync();
     }

     direct_signal_handler_add( -1, dfb_core_signal_handler, core, &core->signal_handler );

     if (fusion_arena_enter( "DirectFB/Core",
                             dfb_core_arena_initialize, dfb_core_arena_join,
                             core, &core->arena, &ret ) || ret)
     {
          direct_thread_remove_init_handler( core->init_handler );

          direct_signal_handler_remove( core->signal_handler );

          D_FREE( core );
          core_dfb = NULL;

          pthread_mutex_unlock( &core_dfb_lock );

          direct_shutdown();
          return ret ? ret : DFB_FUSION;
     }


     if (dfb_config->block_all_signals)
          direct_signals_block_all();

     if (dfb_config->deinit_check)
          atexit( dfb_core_deinit_check );


     *ret_core = core;

     pthread_mutex_unlock( &core_dfb_lock );

     D_DEBUG( "DirectFB/Core: Core successfully created.\n" );

     return DFB_OK;
}

DFBResult
dfb_core_destroy( CoreDFB *core, bool emergency )
{
     D_ASSERT( core != NULL );
     D_ASSERT( core->refs > 0 );
     D_ASSERT( core == core_dfb );

     D_DEBUG( "DirectFB/Core: %s...\n", __FUNCTION__ );

     pthread_mutex_lock( &core_dfb_lock );

     if (!emergency && --core->refs) {
          pthread_mutex_unlock( &core_dfb_lock );
          return DFB_OK;
     }

     direct_signal_handler_remove( core->signal_handler );

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
                               core, emergency, NULL ) == DFB_BUSY)
     {
          D_ONCE( "waiting for DirectFB slaves to terminate" );
          usleep( 100000 );
     }

     fusion_exit( emergency );

     direct_thread_remove_init_handler( core->init_handler );

     D_FREE( core );
     core_dfb = NULL;

     pthread_mutex_unlock( &core_dfb_lock );

     direct_shutdown();

     return DFB_OK;
}

CoreLayerContext *
dfb_core_create_layer_context( CoreDFB *core )
{
     D_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->layer_context_pool != NULL );

     return (CoreLayerContext*) fusion_object_create( core->shared->layer_context_pool );
}

CoreLayerRegion *
dfb_core_create_layer_region( CoreDFB *core )
{
     D_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->layer_region_pool != NULL );

     return (CoreLayerRegion*) fusion_object_create( core->shared->layer_region_pool );
}

CorePalette *
dfb_core_create_palette( CoreDFB *core )
{
     D_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->palette_pool != NULL );

     return (CorePalette*) fusion_object_create( core->shared->palette_pool );
}

CoreSurface *
dfb_core_create_surface( CoreDFB *core )
{
     D_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->surface_pool != NULL );

     return (CoreSurface*) fusion_object_create( core->shared->surface_pool );
}

CoreWindow *
dfb_core_create_window( CoreDFB *core )
{
     D_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );
     D_ASSERT( core->shared->window_pool != NULL );

     return (CoreWindow*) fusion_object_create( core->shared->window_pool );
}

DirectResult
dfb_core_enum_surfaces( CoreDFB               *core,
                        FusionObjectCallback   callback,
                        void                  *ctx )
{
     D_ASSERT( core != NULL || core_dfb != NULL );

     if (!core)
          core = core_dfb;

     return fusion_object_pool_enum( core->shared->surface_pool,
                                     callback, ctx );
}

DirectResult
dfb_core_enum_layer_contexts( CoreDFB               *core,
                              FusionObjectCallback   callback,
                              void                  *ctx )
{
     D_ASSERT( core != NULL || core_dfb != NULL );

     if (!core)
          core = core_dfb;

     return fusion_object_pool_enum( core->shared->layer_context_pool,
                                     callback, ctx );
}

DirectResult
dfb_core_enum_layer_regions( CoreDFB               *core,
                             FusionObjectCallback   callback,
                             void                  *ctx )
{
     D_ASSERT( core != NULL || core_dfb != NULL );

     if (!core)
          core = core_dfb;

     return fusion_object_pool_enum( core->shared->layer_region_pool,
                                     callback, ctx );
}

bool
dfb_core_is_master( CoreDFB *core )
{
     D_ASSERT( core != NULL );

     return core->master;
}

FusionArena *
dfb_core_arena( CoreDFB *core )
{
     D_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     D_ASSERT( core != NULL );

     return core->arena;
}

DFBResult
dfb_core_suspend( CoreDFB *core )
{
     DFBResult ret;

     D_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     D_ASSERT( core != NULL );

     if (!core->master)
          return DFB_ACCESSDENIED;

     ret = dfb_core_input.Suspend( core );
     if (ret)
          return ret;

     ret = dfb_core_layers.Suspend( core );
     if (ret)
          return ret;

     ret = dfb_core_screens.Suspend( core );
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

     D_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     D_ASSERT( core != NULL );

     if (!core->master)
          return DFB_ACCESSDENIED;

     ret = dfb_core_input.Resume( core );
     if (ret)
          return ret;

     ret = dfb_core_gfxcard.Resume( core );
     if (ret)
          return ret;

     ret = dfb_core_screens.Resume( core );
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

     D_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     D_ASSERT( core != NULL );

     cleanup = D_CALLOC( 1, sizeof(CoreCleanup) );

     cleanup->func      = func;
     cleanup->data      = data;
     cleanup->emergency = emergency;

     direct_list_prepend( &core->cleanups, &cleanup->link );

     return cleanup;
}

void
dfb_core_cleanup_remove( CoreDFB     *core,
                         CoreCleanup *cleanup )
{
     D_ASSUME( core != NULL );

     if (!core)
          core = core_dfb;

     D_ASSERT( core != NULL );

     direct_list_remove( &core->cleanups, &cleanup->link );

     D_FREE( cleanup );
}

/******************************************************************************/

static void
dfb_core_deinit_check()
{
     if (core_dfb && core_dfb->refs) {
          D_WARN( "Application exited without deinitialization of DirectFB!" );
          dfb_core_destroy( core_dfb, true );
     }

     direct_print_memleaks();

     direct_print_interface_leaks();
}

static void
dfb_core_thread_init_handler( DirectThread *thread, void *arg )
{
     dfb_system_thread_init();
}

static void
dfb_core_process_cleanups( CoreDFB *core, bool emergency )
{
     while (core->cleanups) {
          CoreCleanup *cleanup = (CoreCleanup*) core->cleanups;

          core->cleanups = core->cleanups->next;

          if (cleanup->emergency || !emergency)
               cleanup->func( cleanup->data, emergency );

          D_FREE( cleanup );
     }
}

static DirectSignalHandlerResult
dfb_core_signal_handler( int   num,
                         void *addr,
                         void *ctx )
{
     CoreDFB *core = ctx;

     D_ASSERT( core == core_dfb );

     dfb_core_destroy( core, true );

     return DSHR_OK;
}

/******************************************************************************/

static int
dfb_core_shutdown( CoreDFB *core, bool emergency )
{
     int            i;
     CoreDFBShared *shared;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );

     shared = core->shared;

     if (!emergency) {
          fusion_object_pool_destroy( shared->window_pool );
          fusion_object_pool_destroy( shared->layer_region_pool );
          fusion_object_pool_destroy( shared->layer_context_pool );
          fusion_object_pool_destroy( shared->surface_pool );
          fusion_object_pool_destroy( shared->palette_pool );
     }

     for (i=NUM_CORE_PARTS-1; i>=0; i--)
          dfb_core_part_shutdown( core, core_parts[i], emergency );

     return 0;
}

static DFBResult
dfb_core_initialize( CoreDFB *core )
{
     int            i;
     CoreDFBShared *shared;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );

     shared = core->shared;

     shared->layer_context_pool = dfb_layer_context_pool_create();
     shared->layer_region_pool  = dfb_layer_region_pool_create();
     shared->palette_pool       = dfb_palette_pool_create();
     shared->surface_pool       = dfb_surface_pool_create();
     shared->window_pool        = dfb_window_pool_create();

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

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );

     for (i=NUM_CORE_PARTS-1; i>=0; i--)
          dfb_core_part_leave( core, core_parts[i], emergency );

     return 0;
}

static int
dfb_core_join( CoreDFB *core )
{
     int i;

     D_ASSERT( core != NULL );
     D_ASSERT( core->shared != NULL );

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

     D_DEBUG( "DirectFB/Core: Initializing...\n" );

     /* Allocate shared structure. */
     shared = SHCALLOC( 1, sizeof(CoreDFBShared) );
     if (!shared) {
          D_ERROR( "DirectFB/Core: Could not allocate (shared) memory!\n" );
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
     DFBResult  ret;
     CoreDFB   *core = ctx;
     void      *field;

     D_DEBUG( "DirectFB/Core: Joining...\n" );

     /* Get shared data. */
     if (fusion_arena_get_shared_field( arena, "Core/Shared", &field ))
          return DFB_FUSION;

     core->shared = field;

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

     D_DEBUG( "DirectFB/Core: Leaving...\n" );

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

     D_DEBUG( "DirectFB/Core: Shutting down...\n" );

     if (!core->master) {
          D_WARN( "refusing shutdown in slave" );
          return dfb_core_leave( core, emergency );
     }

     /* Shutdown. */
     ret = dfb_core_shutdown( core, emergency );
     if (ret)
          return ret;

     return DFB_OK;
}

