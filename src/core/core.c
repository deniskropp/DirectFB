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
#include <core/system.h>
#include <core/sig.h>

#include <misc/mem.h>
#include <misc/memcpy.h>
#include <misc/util.h>

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

/*
 * list of cleanup functions
 */
static FusionLink *core_cleanups = NULL;

CoreData *dfb_core = NULL;

#ifdef DFB_DYNAMIC_LINKING
/*
 * the library handle for dlopen'ing ourselves
 */
static void* dfb_lib_handle = NULL;
#endif


static CorePart *core_parts[] = {
     &dfb_core_clipboard,
     &dfb_core_colorhash,
     &dfb_core_system,
     &dfb_core_input,
     &dfb_core_gfxcard,
     &dfb_core_layers
};

#define NUM_CORE_PARTS ((int)(sizeof(core_parts)/sizeof(CorePart*)))

static int
dfb_core_initialize( FusionArena *arena, void *ctx );

#ifndef FUSION_FAKE
static int
dfb_core_join( FusionArena *arena, void *ctx );
#endif

static int
dfb_core_shutdown( FusionArena *arena, void *ctx, bool emergency );

#ifndef FUSION_FAKE
static int
dfb_core_leave( FusionArena *arena, void *ctx, bool emergency );
#endif

/*
 * ckecks if stack is clean, otherwise prints warning, then calls core_deinit()
 */
static void
dfb_core_deinit_check()
{
     if (dfb_core && dfb_core->refs) {
          DEBUGMSG( "DirectFB/core: WARNING - Application "
                    "exitted without deinitialization of DirectFB!\n" );
          if (dfb_core->master) {
               dfb_core->refs = 1;
               dfb_core_unref();
          }
     }

#ifdef DFB_DEBUG
     dfb_dbg_print_memleaks();
#endif
}

DFBResult
dfb_core_init( int *argc, char **argv[] )
{
     DFBResult ret;

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
     if (ret)
          return ret;
     
     return DFB_OK;
}

DFBResult
dfb_core_ref()
{
#ifdef USE_MMX
     char *mmx_string = " (with MMX support)";
#else
     char *mmx_string = "";
#endif
     int   fid;
     int   ret;
     int   world;
#ifndef FUSION_FAKE
     char  buf[16];
#endif

     /* check for multiple calls, do reference counting */
     if (dfb_core && dfb_core->refs++)
          return DFB_OK;

     if (dfb_config->deinit_check)
          atexit( dfb_core_deinit_check );

#ifdef FUSION_FAKE
     INITMSG( "Single Application Core.%s ("BUILDTIME")\n", mmx_string );
#else
     INITMSG( "Multi Application Core.%s ("BUILDTIME")\n", mmx_string );
#endif

     if (dfb_config->sync) {
          INITMSG( "DirectFB/core: doing sync()...\n" );
          sync();
     }

     if (dfb_config->block_all_signals)
          dfb_sig_block_all();

#ifndef FUSION_FAKE
     dfb_system_thread_init();
#endif

     dfb_find_best_memcpy();

     fid = fusion_init( dfb_config->session, &world );
     if (fid < 0)
          return DFB_INIT;

#ifndef FUSION_FAKE
     DEBUGMSG( "DirectFB/core: world %d, fusion id %d\n", world, fid );

     snprintf( buf, sizeof(buf), "%d", world );

     setenv( "DIRECTFB_SESSION", buf, true );
#endif

     /* allocate local data */
     dfb_core = DFBCALLOC( 1, sizeof(CoreData) );

     dfb_core->refs = 1;
     dfb_core->fid  = fid;

#ifndef FUSION_FAKE
     if (fusion_arena_enter ("DirectFB/Core",
                      dfb_core_initialize, dfb_core_join, NULL,
                      &dfb_core->arena, &ret))
     {
          fusion_exit();

          DFBFREE( dfb_core );
          dfb_core = NULL;

          return DFB_FUSION;
     }
     
     if (ret) {
          ERRORMSG("DirectFB/Core: Error during initialization (%s)\n",
                   DirectFBErrorString( ret ));
          
          fusion_exit();

          DFBFREE( dfb_core );
          dfb_core = NULL;

          dfb_sig_remove_handlers();
          
          return ret;
     }
#else
     ret = dfb_core_initialize( NULL, NULL );
     if (ret) {
          ERRORMSG("DirectFB/Core: Error during initialization (%s)\n",
                   DirectFBErrorString( ret ));
          
          fusion_exit();

          DFBFREE( dfb_core );
          dfb_core = NULL;

          dfb_sig_remove_handlers();
          
          return ret;
     }
#endif

     return DFB_OK;
}

bool
dfb_core_is_master()
{
     return dfb_core->master;
}

void
dfb_core_unref()
{
     if (!dfb_core)
          return;

     if (!dfb_core->refs)
          return;

     if (--dfb_core->refs)
          return;

#ifndef FUSION_FAKE
     if (dfb_core->master) {
          fusion_kill( 0, SIGTERM, 5000 );
          fusion_kill( 0, SIGKILL, 2000 );
     }

     fusion_arena_exit( dfb_core->arena,
                        dfb_core_shutdown, dfb_core_leave, NULL, false, NULL );
#else
     dfb_core_shutdown( NULL, NULL, false );
#endif
    
     fusion_exit();

     DFBFREE( dfb_core );
     dfb_core = NULL;

#ifdef DFB_DYNAMIC_LINKING
     if (dfb_lib_handle) {
          dlclose(dfb_lib_handle);
          dfb_lib_handle = NULL;
     }
#endif

     dfb_sig_remove_handlers();
}

DFBResult
dfb_core_suspend()
{
     DFBResult ret;

     if (!dfb_core->master)
          return DFB_ACCESSDENIED;
     
     ret = dfb_core_layers.Suspend();
     if (ret)
          return ret;

     ret = dfb_core_input.Suspend();
     if (ret)
          return ret;

     ret = dfb_core_gfxcard.Suspend();
     if (ret)
          return ret;

     return DFB_OK;
}

DFBResult
dfb_core_resume()
{
     DFBResult ret;

     if (!dfb_core->master)
          return DFB_ACCESSDENIED;

     ret = dfb_core_gfxcard.Resume();
     if (ret)
          return ret;

     ret = dfb_core_input.Resume();
     if (ret)
          return ret;

     ret = dfb_core_layers.Resume();
     if (ret)
          return ret;

     return DFB_OK;
}

void
dfb_core_deinit_emergency()
{
     if (!dfb_core || !dfb_core->refs)
          return;

     dfb_core->refs = 0;

#ifndef FUSION_FAKE
     fusion_arena_exit( dfb_core->arena,
                        dfb_core_shutdown, dfb_core_leave, NULL, true, NULL );
#else
     dfb_core_shutdown( NULL, NULL, true );
#endif

     fusion_exit();

     DFBFREE( dfb_core );
     dfb_core = NULL;

     dfb_sig_remove_handlers();
}

CoreCleanup *
dfb_core_cleanup_add( CoreCleanupFunc func, void *data, bool emergency )
{
     CoreCleanup *cleanup = DFBCALLOC( 1, sizeof(CoreCleanup) );

     cleanup->func      = func;
     cleanup->data      = data;
     cleanup->emergency = emergency;

     fusion_list_prepend( &core_cleanups, &cleanup->link );

     return cleanup;
}

void
dfb_core_cleanup_remove( CoreCleanup *cleanup )
{
     fusion_list_remove( &core_cleanups, &cleanup->link );

     DFBFREE( cleanup );
}

/****************************/

static int
dfb_core_initialize( FusionArena *arena, void *ctx )
{
     int       i;
     DFBResult ret;

     DEBUGMSG( "DirectFB/Core: we are the master, initializing...\n" );

     dfb_core->master = true;

     dfb_sig_install_handlers();
     
     for (i=0; i<NUM_CORE_PARTS; i++) {
          if ((ret = dfb_core_part_initialize( core_parts[i] ))) {
               dfb_core_shutdown( arena, ctx, true );
               return ret;
          }
     }

     return 0;
}

#ifndef FUSION_FAKE
static int
dfb_core_join( FusionArena *arena, void *ctx )
{
     int       i;
     DFBResult ret;

     DEBUGMSG( "DirectFB/Core: we are a slave, joining...\n" );

     dfb_config->sighandler = false;
     dfb_sig_install_handlers();
     
     for (i=0; i<NUM_CORE_PARTS; i++) {
          if ((ret = dfb_core_part_join( core_parts[i] ))) {
               dfb_core_leave( arena, ctx, true );
               return ret;
          }
     }
     
     return 0;
}
#endif

static void
process_cleanups( bool emergency )
{
     while (core_cleanups) {
          CoreCleanup *cleanup = (CoreCleanup *)core_cleanups;

          core_cleanups = core_cleanups->next;

          if (cleanup->emergency || !emergency)
               cleanup->func( cleanup->data, emergency );

          DFBFREE( cleanup );
     }
}

static int
dfb_core_shutdown( FusionArena *arena, void *ctx, bool emergency )
{
     int i;

     DEBUGMSG( "DirectFB/Core: shutting down!\n" );

     process_cleanups( emergency );

     for (i=NUM_CORE_PARTS-1; i>=0; i--)
          dfb_core_part_shutdown( core_parts[i], emergency );

     return 0;
}

#ifndef FUSION_FAKE
static int
dfb_core_leave( FusionArena *arena, void *ctx, bool emergency )
{
     int i;

     DEBUGMSG( "DirectFB/Core: leaving!\n" );

     process_cleanups( emergency );
     
     for (i=NUM_CORE_PARTS-1; i>=0; i--)
          dfb_core_part_leave( core_parts[i], emergency );

     return 0;
}
#endif

