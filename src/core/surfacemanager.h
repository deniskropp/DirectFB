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

#ifndef __SURFACEMANAGER_H__
#define __SURFACEMANAGER_H__

#include <directfb.h>
#include <core/coretypes.h>

/*
 * initially there is one big free chunk,
 * chunks are splitted into a free and an occupied chunk if memory is allocated,
 * two chunks are merged to one free chunk if memory is deallocated
 */
typedef struct _Chunk
{
     int                 offset;   /* offset in video memory,
                                      is greater or equal to the heap offset */
     int                 length;   /* length of this chunk in bytes */

     SurfaceBuffer      *buffer;   /* pointer to surface buffer occupying
                                      this chunk, or NULL if chunk is free */

     int                 tolerations; /* number of times this chunk was scanned
                                         occupied, resetted in assure_video */

     struct _Chunk      *prev;
     struct _Chunk      *next;
} Chunk;

/*
 * initializes the surface manager's heap management,
 * creates one big chunk with the size of the framebuffer
 */
DFBResult surfacemanager_init_heap();

/*
 * frees all data allocated by the surfacemanager,
 * it does not deallocate surfaces from video memory
 */
void surfacemanager_deinit();

/*
 * marks video instances for restoration,
 * TODO: backup video only surfaces in system memory
 */
DFBResult surfacemanager_suspend();

/*
 * adjust the offset within the framebuffer for surface storage,
 * needs to be called after a resolution switch
 */
DFBResult surfacemanager_adjust_heap_offset( int offset );



extern pthread_mutex_t surfacemanager_mutex_lock;

/*
 * Lock/unlock the surfacemanager for usage of the functions below.
 */
#ifdef DFB_DEBUG
#define surfacemanager_lock() { \
     DEBUGMSG("[%d] Locking in "__FUNCTION__" at "__FILE__":%d\n", getpid(), __LINE__); \
     pthread_mutex_lock( &surfacemanager_mutex_lock ); \
}

#define surfacemanager_unlock() { \
     DEBUGMSG("[%d] Unlocking in "__FUNCTION__" in "__FILE__":%d\n", getpid(), __LINE__); \
     pthread_mutex_unlock( &surfacemanager_mutex_lock ); \
}
#else
static inline int surfacemanager_lock() {
     return pthread_mutex_lock( &surfacemanager_mutex_lock );
}

static inline int surfacemanager_unlock() {
     return pthread_mutex_unlock( &surfacemanager_mutex_lock );
}
#endif

/*
 * finds and allocates one for the surface or fails,
 * after success the video health is CSH_RESTORE.
 * NOTE: this does not notify the listeners
 */
DFBResult surfacemanager_allocate( SurfaceBuffer *buffer );

/*
 * sets the video health to CSH_INVALID frees the chunk and
 * notifies the listeners
 */
DFBResult surfacemanager_deallocate( SurfaceBuffer *buffer );

/*
 * puts the surface into the video memory,
 * i.e. it establishes the video instance or fails
 */
DFBResult surfacemanager_assure_video( SurfaceBuffer *buffer );

/*
 * makes sure the system instance is not outdated,
 * it fails if the policy is CSP_VIDEOONLY
 */
DFBResult surfacemanager_assure_system( SurfaceBuffer *buffer );

#endif
