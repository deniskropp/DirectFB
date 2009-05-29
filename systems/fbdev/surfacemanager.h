/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <directfb.h>

#include <core/coretypes.h>

typedef struct _SurfaceManager SurfaceManager;
typedef struct _Chunk          Chunk;

/*
 * initially there is one big free chunk,
 * chunks are splitted into a free and an occupied chunk if memory is allocated,
 * two chunks are merged to one free chunk if memory is deallocated
 */
struct _Chunk {
     int                  magic;

     int                  offset;      /* offset in memory,
                                          is greater or equal to the heap offset */
     int                  length;      /* length of this chunk in bytes */

     int                  pitch;
     
     CoreSurfaceBuffer   *buffer;      /* pointer to surface buffer occupying
                                          this chunk, or NULL if chunk is free */
     CoreSurfaceAllocation *allocation;

     int                  tolerations; /* number of times this chunk was scanned
                                          occupied, resetted in assure_video */

     Chunk               *prev;
     Chunk               *next;
};

struct _SurfaceManager {
     int                  magic;

     FusionSHMPoolShared *shmpool;

     Chunk               *chunks;

     int                  offset;
     int                  length;         /* length of the heap in bytes */
     int                  avail;          /* amount of available memory in bytes */

     int                  min_toleration;
     
     bool                 suspended;
};


DFBResult dfb_surfacemanager_create ( CoreDFB             *core,
                                      unsigned int         length,
                                      SurfaceManager     **ret_manager );

void      dfb_surfacemanager_destroy( SurfaceManager      *manager );

/*
 * adjust the offset within the framebuffer for surface storage,
 * needs to be called after a resolution switch
 */
DFBResult dfb_surfacemanager_adjust_heap_offset( SurfaceManager *manager,
                                                 int             offset );

/*
 * finds and allocates one for the surface or fails,
 * after success the video health is CSH_RESTORE.
 * NOTE: this does not notify the listeners
 */
DFBResult dfb_surfacemanager_allocate( CoreDFB                *core,
                                       SurfaceManager         *manager,
                                       CoreSurfaceBuffer      *buffer,
                                       CoreSurfaceAllocation  *allocation,
                                       Chunk                 **ret_chunk );

DFBResult dfb_surfacemanager_displace( CoreDFB           *core,
                                       SurfaceManager    *manager,
                                       CoreSurfaceBuffer *buffer );

/*
 * sets the video health to CSH_INVALID frees the chunk and
 * notifies the listeners
 */
DFBResult dfb_surfacemanager_deallocate( SurfaceManager *manager,
                                         Chunk          *chunk );

#endif

