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

#include <directfb.h>

#include <stdlib.h>

#include "core.h"
#include "coredefs.h"
#include "vt.h"
#include "sig.h"
#include "input.h"
#include "fbdev.h"
#include "gfxcard.h"

#include "inputdevices/keyboard.h"

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
          BUG( "core_init() called with something on the cleanup stack" );
          return DFB_OK;
     }

     if (!config->no_sighandler)
          sig_install_handlers();

     if (!config->no_deinit_check)
          atexit( core_deinit_check );

     INITMSG( "Single Application Core.%s\n", mmx_string );

     INITCHECK( vt_open() );
     INITCHECK( input_init_devices() );
     INITCHECK( fbdev_open() );
     INITCHECK( gfxcard_init() );
     
     INITCHECK( primarylayer_init() );
     INITCHECK( gfxcard_init_layers() );
     
     INITCHECK( fonts_load_default() );

     return DFB_OK;
}

void core_deinit()
{
     if (!cleanup_stack)
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
     keyboard_deinit(NULL);
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

