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

#ifndef __CORE_H__
#define __CORE_H__


typedef enum {
     MODULE_LOADED_CONTINUE,
     MODULE_LOADED_STOP,
     MODULE_REJECTED
} CoreModuleLoadResult;

DFBResult core_load_modules( char *module_dir,
                             CoreModuleLoadResult (*handle_func)(void *handle,
                                                                 char *name,
                                                                 void *ctx),
                             void *ctx );


/*
 * is called by DirectFBCreate(), initializes all core parts
 */
DFBResult core_init();

/*
 * is called by IDirectFB_Destruct() or by core_deinit_check() via atexit()
 * processes and clears the cleanup stack
 */
void core_deinit();

/*
 * called by signal handler
 */
void core_deinit_emergency();

/*
 * puts a funtion that is called by core_deinit() on the cleanup stack
 */
void core_cleanup_push( void (*cleanup_func)() );

/*
 * puts a funtion that is called by core_deinit() cleanup stack at last
 */
void core_cleanup_last( void (*cleanup_func)() );

#endif

