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

#include <core/fusion/lock.h>

#include <directfb.h>

#include "coretypes.h"
#include "coredefs.h"

typedef enum {
     MODULE_LOADED_CONTINUE,
     MODULE_LOADED_STOP,
     MODULE_REJECTED
} CoreModuleLoadResult;

typedef void (*CoreCleanupFunc)(void *data, int emergency);

DFBResult dfb_core_load_modules( char *module_dir,
                                 CoreModuleLoadResult (*handle_func)(void *handle,
                                                                     char *name,
                                                                     void *ctx),
                                 void *ctx );


typedef struct {
     int                    refs;       /* local references to DirectFB */
     int                    fid;        /* fusion id */
     int                    master;     /* if we are the master fusionee */
     FusionArena           *arena;      /* DirectFB Core arena */
} CoreData;

extern CoreData *dfb_core;


/*
 * called by DirectFBInit
 */
DFBResult dfb_core_init( int *argc, char **argv[] );

/*
 * is called by DirectFBCreate(), initializes all core parts
 */
DFBResult dfb_core_ref();

/*
 * is called by IDirectFB_Destruct() or by core_deinit_check() via atexit()
 * processes and clears the cleanup stack
 */
void dfb_core_unref();

int dfb_core_is_master();

DFBResult dfb_core_suspend();
DFBResult dfb_core_resume();

/*
 * called by signal handler
 */
void dfb_core_deinit_emergency();

/*
 * adds a function that is called by core_deinit() to the cleanup stack,
 * if emergency is not 0, the cleanup is even called by core_deinit_emergency()
 */
CoreCleanup *dfb_core_cleanup_add( CoreCleanupFunc cleanup,
                                   void *data, int emergency );

void dfb_core_cleanup_remove( CoreCleanup *cleanup );

#endif

