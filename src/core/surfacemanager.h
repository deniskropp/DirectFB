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

#ifndef __SURFACEMANAGER_H__
#define __SURFACEMANAGER_H__

#include <core/fusion/lock.h>

#include <directfb.h>
#include <core/core.h>

#define CSLF_FORCE 0x80000000


SurfaceManager *dfb_surfacemanager_create( unsigned int length,
                                           unsigned int byteoffset_align,
                                           unsigned int pixelpitch_align );

void dfb_surfacemanager_destroy( SurfaceManager *manager );

#ifdef FUSION_FAKE
DFBResult dfb_surfacemanager_suspend( SurfaceManager *manager );
DFBResult dfb_surfacemanager_resume( SurfaceManager *manager );
#endif

/*
 * adjust the offset within the framebuffer for surface storage,
 * needs to be called after a resolution switch
 */
DFBResult dfb_surfacemanager_adjust_heap_offset( SurfaceManager *manager,
                                                 unsigned int    offset );

/*
 * Lock/unlock the surfacemanager for usage of the functions below.
 */
void dfb_surfacemanager_lock( SurfaceManager *manager );
void dfb_surfacemanager_unlock( SurfaceManager *manager );

/*
 * lock a surface for access by software, returns a pointer to the
 * surface data and the line pitch a.k.a. rowstride
 */
DFBResult dfb_surface_software_lock( CoreSurface *surface, unsigned int flags,
                                     void **data, unsigned int *pitch, int front );

/*
 * lock a surface for access by hardware that enforces a video instance
 * an therefore the data and pitch can be looked up in the surface struct's
 * video struct, however this function will fail if the surfacemanager could
 * not assure a video memory instance
 */
DFBResult dfb_surface_hardware_lock( CoreSurface *surface,
                                     unsigned int flags, int front );


/*
 * finds and allocates one for the surface or fails,
 * after success the video health is CSH_RESTORE.
 * NOTE: this does not notify the listeners
 */
DFBResult dfb_surfacemanager_allocate( SurfaceManager *manager,
                                       SurfaceBuffer  *buffer );

/*
 * sets the video health to CSH_INVALID frees the chunk and
 * notifies the listeners
 */
DFBResult dfb_surfacemanager_deallocate( SurfaceManager *manager,
                                         SurfaceBuffer  *buffer );

/*
 * puts the surface into the video memory,
 * i.e. it initializees the video instance or fails
 */
DFBResult dfb_surfacemanager_assure_video( SurfaceManager *manager,
                                           SurfaceBuffer  *buffer );

/*
 * makes sure the system instance is not outdated,
 * it fails if the policy is CSP_VIDEOONLY
 */
DFBResult dfb_surfacemanager_assure_system( SurfaceManager *manager,
                                            SurfaceBuffer  *buffer );

#endif
