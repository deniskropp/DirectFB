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

#ifndef __CORE_H__
#define __CORE_H__

#include <core/fusion/fusion_types.h>
#include <core/fusion/lock.h>

#include <directfb.h>

#include "coretypes.h"
#include "coredefs.h"

/*
 * Return type of a module loading callback.
 */
typedef enum {
     MODULE_LOADED_CONTINUE,  /* Keep module loaded, continue with next. */
     MODULE_LOADED_STOP,      /* Keep module loaded, but don't continue. */
     MODULE_REJECTED          /* Module has been rejected, continue. */
} CoreModuleLoadResult;

/*
 * Cleanup function, callback of a cleanup stack entry.
 */
typedef void (*CoreCleanupFunc)(void *data, int emergency);

/*
 * Opens a directory and starts opening one module after another (*.so).
 * Return value of handle_func decides if loading continues or stops.
 */
DFBResult
dfb_core_load_modules( char *module_dir,
                       CoreModuleLoadResult (*handle_func)(void *handle,
                                                           char *name,
                                                           void *ctx),
                       void *ctx );

/*
 * Process local core data. Shared between threads.
 */
typedef struct {
     int                    refs;       /* references to the core */
     int                    fid;        /* fusion id */
     bool                   master;     /* if we are the master fusionee */
     FusionArena           *arena;      /* DirectFB Core arena */
} CoreData;

extern CoreData *dfb_core;


/*
 * called by DirectFBInit
 */
DFBResult
dfb_core_init( int *argc, char **argv[] );

/*
 * Called by DirectFBCreate(), initializes all core parts if needed and
 * increases the core reference counter.
 */
DFBResult
dfb_core_ref();

/*
 * Called by IDirectFB::Destruct() or by core_deinit_check() via atexit(),
 * decreases the core reference counter and deinitializes all core parts
 * if reference counter reaches zero.
 */
void
dfb_core_unref();

/*
 * Returns true if the calling process is the master fusionee,
 * i.e. handles input drivers running their threads.
 */
bool
dfb_core_is_master();

/*
 * Suspends all core parts, stopping input threads, closing devices...
 */
DFBResult
dfb_core_suspend();

/*
 * Resumes all core parts, reopening devices, starting input threads...
 */
DFBResult
dfb_core_resume();

/*
 * Called by signal handler, does all important shutdowns.
 */
void
dfb_core_deinit_emergency();

/*
 * Adds a function to the cleanup stack that is called during deinitialization.
 * If emergency is true, the cleanup is even called by core_deinit_emergency().
 */
CoreCleanup *
dfb_core_cleanup_add( CoreCleanupFunc func, void *data, bool emergency );

/*
 * Removes a function from the cleanup stack.
 */
void
dfb_core_cleanup_remove( CoreCleanup *cleanup );

#endif

