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

#include "directfb.h"

#include "coredefs.h"
#include "coretypes.h"

#include "core.h"
#include "vt.h"
#include "sig.h"
#include "input.h"
#include "fbdev.h"
#include "gfxcard.h"
#include "layers.h"
#include "surfaces.h"
#include "surfacemanager.h"

#include "misc/mem.h"
#include "misc/memcpy.h"
#include "misc/util.h"
#include "misc/fbdebug.h"

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
 * macro for error handling in init functions
 */
#define INITCHECK(a...)                                                     \
     if ((ret = a) != DFB_OK) {                                             \
          ERRORMSG("DirectFB/Core: Error during initialization: " #a "\n"); \
          dfb_core_deinit_emergency();                                      \
          return ret;                                                       \
     }

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
#ifdef DFB_DYNAMIC_LINKING
     if (!dfb_lib_handle)
          dfb_lib_handle = dlopen(SOPATH, RTLD_GLOBAL|RTLD_LAZY);
#endif
    
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

     /* check for multiple calls, do reference counting */
     if (dfb_core && dfb_core->refs++)
          return DFB_OK;

     if (dfb_config->sighandler)
          dfb_sig_install_handlers();

     if (dfb_config->deinit_check)
          atexit( dfb_core_deinit_check );

#ifdef FUSION_FAKE
     INITMSG( "Single Application Core.%s\n", mmx_string );
#else
     INITMSG( "Multi Application Core.%s\n", mmx_string );
#endif

     if (dfb_config->sync) {
          INITMSG( "DirectFB/core: doing sync()...\n" );
          sync();
     }

     dfb_find_best_memcpy();

     fid = fusion_init();
     if (fid < 0)
          return DFB_INIT;

     DEBUGMSG( "DirectFB/Core: fusion id %d\n", fid );

     /* allocate local data */
     dfb_core = DFBCALLOC( 1, sizeof(CoreData) );

     dfb_core->refs  = 1;
     dfb_core->fid   = fid;

#ifndef FUSION_FAKE
     arena_enter ("DirectFB/Core",
                  dfb_core_initialize, dfb_core_join, NULL);
     if (!dfb_core)
          return DFB_INIT;
#else
     if (dfb_core_initialize( NULL, NULL ))
          return DFB_INIT;
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
     arena_exit( dfb_core->arena,
                 dfb_core_shutdown, dfb_core_leave, false );
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
#ifndef FUSION_FAKE
     return DFB_UNSUPPORTED;
#else
     DFBResult ret;

     ret = dfb_input_suspend();
     if (ret)
          return ret;

     ret = dfb_layers_suspend();
     if (ret)
          return ret;

     ret = dfb_gfxcard_suspend();
     if (ret)
          return ret;

     return DFB_OK;
#endif
}

DFBResult
dfb_core_resume()
{
#ifndef FUSION_FAKE
     return DFB_UNSUPPORTED;
#else
     DFBResult ret;

     ret = dfb_gfxcard_resume();
     if (ret)
          return ret;

     ret = dfb_layers_resume();
     if (ret)
          return ret;

     ret = dfb_input_resume();
     if (ret)
          return ret;

     return DFB_OK;
#endif
}

void
dfb_core_deinit_emergency()
{
     if (!dfb_core || !dfb_core->refs)
          return;

     dfb_core->refs = 0;

#ifndef FUSION_FAKE
     arena_exit( dfb_core->arena,
                 dfb_core_shutdown, dfb_core_leave, true );
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

/*
 * module loading functions
 */
#ifdef DFB_DYNAMIC_LINKING
DFBResult
dfb_core_load_modules( char *module_dir,
                       CoreModuleLoadResult (*handle_func)(void *handle,
                                                           char *name,
                                                           void *ctx),
                       void *ctx )
{
     DFBResult      ret = DFB_UNSUPPORTED;
     DIR           *dir;
     int            dir_len = strlen( module_dir );
     struct dirent *entry;

     dir = opendir( module_dir );

     if (!dir) {
          PERRORMSG( "DirectFB/core: "
                     "Could not open module directory `%s'!\n", module_dir );
          return errno2dfb( errno );
     }

     while ( (entry = readdir(dir) ) != NULL ) {
          void *handle;
          int   entry_len = strlen(entry->d_name);
          int   buf_len   = dir_len + entry_len + 2;
          char  buf[buf_len];

          if (entry_len < 4 ||
              entry->d_name[strlen(entry->d_name)-1] != 'o' ||
              entry->d_name[strlen(entry->d_name)-2] != 's')
               continue;

          snprintf( buf, buf_len, "%s/%s", module_dir, entry->d_name );

          handle = dlopen( buf, RTLD_LAZY );
          if (handle) {
               switch (handle_func( handle, buf, ctx )) {
                    case MODULE_REJECTED:
                         dlclose( handle );
                         break;

                    case MODULE_LOADED_STOP:
                         closedir( dir );
                         return DFB_OK;

                    case MODULE_LOADED_CONTINUE:
                         ret = DFB_OK;
                         break;
               }
          }
          else
               DLERRORMSG( "DirectFB/core: Unable to dlopen `%s'!\n", buf );

     }

     closedir( dir );

     return ret;
}
#endif

/****************************/

static int
dfb_core_initialize( FusionArena *arena, void *ctx )
{
     DFBResult ret;

     DEBUGMSG( "DirectFB/Core: we are the master, initializing...\n" );

     dfb_core->arena  = arena;
     dfb_core->master = true;

#ifdef DFB_DEBUG
     fbdebug_init();
#endif

     INITCHECK( dfb_vt_initialize() );
     INITCHECK( dfb_input_initialize() );
     INITCHECK( dfb_layers_initialize() );
     INITCHECK( dfb_fbdev_initialize() );
     INITCHECK( dfb_gfxcard_initialize() );
     
     INITCHECK( dfb_layers_init_all() );

     return 0;
}

#ifndef FUSION_FAKE
static int
dfb_core_join( FusionArena *arena, void *ctx )
{
     DFBResult ret;

     DEBUGMSG( "DirectFB/Core: we are a slave, joining...\n" );

     dfb_core->arena  = arena;

     INITCHECK( dfb_vt_join() );
     INITCHECK( dfb_input_join() );
     INITCHECK( dfb_layers_join() );
     INITCHECK( dfb_fbdev_join() );
     INITCHECK( dfb_gfxcard_join() );

     INITCHECK( dfb_layers_join_all() );
     
     return 0;
}
#endif

static int
dfb_core_shutdown( FusionArena *arena, void *ctx, bool emergency )
{
     DEBUGMSG( "DirectFB/Core: shutting down!\n" );

     while (core_cleanups) {
          CoreCleanup *cleanup = (CoreCleanup *)core_cleanups;

          core_cleanups = core_cleanups->next;

          if (cleanup->emergency || !emergency)
               cleanup->func( cleanup->data, emergency );

          DFBFREE( cleanup );
     }

     dfb_layers_shutdown( emergency );
     dfb_gfxcard_shutdown( emergency );
     dfb_fbdev_shutdown( emergency );
     dfb_input_shutdown( emergency );
     dfb_vt_shutdown( emergency );

#ifdef DFB_DEBUG
     fbdebug_exit();
#endif

     return 0;
}

#ifndef FUSION_FAKE
static int
dfb_core_leave( FusionArena *arena, void *ctx, bool emergency )
{
     DEBUGMSG( "DirectFB/Core: leaving!\n" );

     while (core_cleanups) {
          CoreCleanup *cleanup = (CoreCleanup *)core_cleanups;

          core_cleanups = core_cleanups->next;

          if (cleanup->emergency || !emergency)
               cleanup->func( cleanup->data, emergency );

          DFBFREE( cleanup );
     }
     
     dfb_gfxcard_leave( emergency );
     dfb_fbdev_leave( emergency );
     dfb_layers_leave( emergency );
     dfb_input_leave( emergency );
     dfb_vt_leave( emergency );

     return 0;
}
#endif

