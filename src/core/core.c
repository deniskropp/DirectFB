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

#include "directfb.h"

#include "coredefs.h"
#include "coretypes.h"

#include "core.h"
#include "vt.h"
#include "sig.h"
#include "input.h"
#include "fbdev.h"
#include "gfxcard.h"

#include "misc/util.h"

/*
 * one entry on the cleanup stack
 */
typedef struct _Cleanup {
     void (*cleanup)();

     struct _Cleanup     *prev;
} Cleanup;

/*
 * the cleanup stack
 */
static Cleanup *cleanup_stack = NULL;

/*
 * macro for error handling in init functions
 */
#define INITCHECK(a...)                                                     \
     if ((ret = a) != DFB_OK) {                                       \
          ERRORMSG("DirectFB/Core: Error during initialization: " #a "\n"); \
          if (cleanup_stack) core_deinit();                                \
          return ret;                                                       \
     }

/*
 * ckecks if stack is clean, otherwise prints warning, then calls core_deinit()
 */
void core_deinit_check()
{
     if (cleanup_stack) {
          DEBUGMSG( "DirectFB/core: WARNING - Application exitted without deinitialization of DirectFB!\n" );
          core_deinit();
     }
}

DFBResult core_init()
{
     DFBResult ret;
#ifdef USE_MMX
     char *mmx_string = " (with MMX support)";
#else
     char *mmx_string = "";
#endif

     if (cleanup_stack) {
          DEBUGMSG( "core_init() called with something on the cleanup stack" );
          return DFB_OK;
     }

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
     if (!cleanup_stack)
          return;

     /* FIXME: cleanup the cleanup code, many deadlock problems... */
     core_deinit_emergency();
     return;

     while (cleanup_stack) {
          Cleanup *cleanup = cleanup_stack;
          void (*cleanup_func)() = cleanup_stack->cleanup;

          cleanup_stack = cleanup_stack->prev;
          
          free( cleanup );

          cleanup_func();
     }

     vt_close();
}

void core_deinit_emergency()
{
//     keyboard_deinit(NULL);
     vt_close();
     
     cleanup_stack = NULL;
}

void core_cleanup_push( void (*cleanup_func)() )
{
     Cleanup *cleanup = (Cleanup*)malloc( sizeof(Cleanup) );

     cleanup->cleanup = cleanup_func;

     if (cleanup_stack) {
          cleanup->prev = cleanup_stack;
     }
     else {
          cleanup->prev = NULL;
     }

     cleanup_stack = cleanup;
}

void core_cleanup_last( void (*cleanup_func)() )
{
     Cleanup *cleanup = (Cleanup*)malloc( sizeof(Cleanup) );

     cleanup->cleanup = cleanup_func;

     if (cleanup_stack) {
          Cleanup *stack = cleanup_stack;

          while (stack->prev)
               stack = stack->prev;

          stack->prev = cleanup;
     }
     else
          cleanup_stack = cleanup;
          
     cleanup->prev = NULL;
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

