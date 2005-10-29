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

#ifndef __CORE_H__
#define __CORE_H__

#include <fusion/types.h>
#include <fusion/lock.h>
#include <fusion/object.h>

#include <directfb.h>

#include "coretypes.h"
#include "coredefs.h"


#define DIRECTFB_CORE_ABI     28


/*
 * Cleanup function, callback of a cleanup stack entry.
 */
typedef void (*CoreCleanupFunc)(void *data, int emergency);



/*
 * Core initialization and deinitialization
 */
DFBResult dfb_core_create ( CoreDFB **ret_core );

DFBResult dfb_core_destroy( CoreDFB  *core,
                            bool      emergency );

/*
 * Object creation
 */
CoreLayerContext *dfb_core_create_layer_context( CoreDFB *core );
CoreLayerRegion  *dfb_core_create_layer_region ( CoreDFB *core );
CorePalette      *dfb_core_create_palette      ( CoreDFB *core );
CoreSurface      *dfb_core_create_surface      ( CoreDFB *core );
CoreWindow       *dfb_core_create_window       ( CoreDFB *core );

/*
 * Debug
 */
DirectResult dfb_core_enum_surfaces      ( CoreDFB               *core,
                                           FusionObjectCallback   callback,
                                           void                  *ctx );
DirectResult dfb_core_enum_layer_contexts( CoreDFB               *core,
                                           FusionObjectCallback   callback,
                                           void                  *ctx );
DirectResult dfb_core_enum_layer_regions ( CoreDFB               *core,
                                           FusionObjectCallback   callback,
                                           void                  *ctx );


/*
 * Returns true if the calling process is the master fusionee,
 * i.e. handles input drivers running their threads.
 */
bool         dfb_core_is_master( CoreDFB *core );

/*
 * Returns the core's fusion world.
 */
FusionWorld *dfb_core_world( CoreDFB *core );

/*
 * Returns the core arena.
 */
FusionArena *dfb_core_arena( CoreDFB *core );

/*
 * Returns the shared memory pool of the core.
 */
FusionSHMPoolShared *dfb_core_shmpool( CoreDFB *core );

/*
 * Suspends all core parts, stopping input threads, closing devices...
 */
DFBResult    dfb_core_suspend( CoreDFB *core );

/*
 * Resumes all core parts, reopening devices, starting input threads...
 */
DFBResult    dfb_core_resume( CoreDFB *core );

/*
 * Adds a function to the cleanup stack that is called during deinitialization.
 * If emergency is true, the cleanup is even called by core_deinit_emergency().
 */
CoreCleanup *dfb_core_cleanup_add( CoreDFB         *core,
                                   CoreCleanupFunc  func,
                                   void            *data,
                                   bool             emergency );

/*
 * Removes a function from the cleanup stack.
 */
void         dfb_core_cleanup_remove( CoreDFB     *core,
                                      CoreCleanup *cleanup );

#endif

