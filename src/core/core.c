/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include "misc/mem.h"
#include "misc/util.h"

/*
 * one entry in the cleanup list
 */
struct _CoreCleanup {
     CoreCleanupFunc  cleanup;
     void            *data;
     int              emergency;

     CoreCleanup     *prev;
     CoreCleanup     *next;
};

/*
 * list of cleanup functions
 */
static CoreCleanup *core_cleanups = NULL;

static int core_refs = 0;

/*
 * macro for error handling in init functions
 */
#define INITCHECK(a...)                                                     \
     if ((ret = a) != DFB_OK) {                                             \
          ERRORMSG("DirectFB/Core: Error during initialization: " #a "\n"); \
          core_deinit_emergency();                                          \
          return ret;                                                       \
     }

/*
 * ckecks if stack is clean, otherwise prints warning, then calls core_deinit()
 */
void core_deinit_check()
{
     if (core_refs) {
          DEBUGMSG( "DirectFB/core: WARNING - Application "
                    "exitted without deinitialization of DirectFB!\n" );
          core_deinit();
     }

#ifdef DFB_DEBUG
     dbg_print_memleaks();
#endif
}

DFBResult core_init()
{
     DFBResult ret;
#ifdef USE_MMX
     char *mmx_string = " (with MMX support)";
#else
     char *mmx_string = "";
#endif

     if (core_refs++)
          return DFB_OK;

     if (!dfb_config->no_sighandler)
          sig_install_handlers();

     if (!dfb_config->no_deinit_check)
          atexit( core_deinit_check );

     INITMSG( "Single Application Core.%s\n", mmx_string );

     if (dfb_config->sync) {
          INITMSG( "DirectFB/core: doing sync()...\n" );
          sync();
     }

     INITCHECK( vt_open() );
     INITCHECK( input_init_devices() );
     INITCHECK( fbdev_open() );
     INITCHECK( gfxcard_init() );

     INITCHECK( primarylayer_init() );
     INITCHECK( gfxcard_init_layers() );

     return DFB_OK;
}

void core_deinit()
{
     if (--core_refs)
          return;

     while (core_cleanups) {
          CoreCleanup *cleanup = core_cleanups;

          core_cleanups = cleanup->next;

          cleanup->cleanup( cleanup->data, 0 );

          DFBFREE( cleanup );
     }

     layers_deinit();
     surfaces_deinit();
     gfxcard_deinit();
     fbdev_deinit();
     input_deinit();
     vt_close();

     sig_remove_handlers();
}

void core_deinit_emergency()
{
     core_refs = 0;

     while (core_cleanups) {
          CoreCleanup *cleanup = core_cleanups;

          core_cleanups = core_cleanups->prev;

          if (cleanup->emergency)
               cleanup->cleanup( cleanup->data, 1 );

          DFBFREE( cleanup );
     }

     if (card) {
          int i;

          /* try to prohibit graphics hardware access,
             this may fail if the current thread locked it */
          for (i=0; i<100; i++) {
               gfxcard_sync();

               if (pthread_mutex_trylock( &card->lock ) != EBUSY)
                    break;

               sched_yield();
          }

          if (card->info.driver && card->info.driver->DeInit)
               card->info.driver->DeInit();
     }

     input_deinit();
     if (vt)
          vt_close();

     sig_remove_handlers();
}

CoreCleanup *core_cleanup_add( CoreCleanupFunc cleanup,
                               void *data, int emergency )
{
     CoreCleanup *c = (CoreCleanup*)DFBCALLOC( 1, sizeof(CoreCleanup) );

     c->cleanup   = cleanup;
     c->data      = data;
     c->emergency = emergency;

     if (core_cleanups) {
          CoreCleanup *cc = core_cleanups;

          while (cc->next)
               cc = cc->next;

          c->prev = cc;
          cc->next = c;
     }
     else
          core_cleanups = c;

     return c;
}

void core_cleanup_remove( CoreCleanup *cleanup )
{
     if (cleanup->next)
          cleanup->next->prev = cleanup->prev;

     if (cleanup->prev)
          cleanup->prev->next = cleanup->next;
     else
          core_cleanups = cleanup->next;

     DFBFREE( cleanup );
}

/*
 * module loading functions
 */

DFBResult core_load_modules( char *module_dir,
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
          char  buf[dir_len + entry_len + 2];

          if (entry_len < 4 ||
              entry->d_name[strlen(entry->d_name)-1] != 'o' ||
              entry->d_name[strlen(entry->d_name)-2] != 's')
               continue;

          sprintf( buf, "%s/%s", module_dir, entry->d_name );

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

