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

#ifndef __SURFACES_H__
#define __SURFACES_H__

#include "core/reactor.h"

struct _Chunk;

typedef enum {
     CSNF_SIZEFORMAT  = 0x00000001,     /* width, height, format */
     CSNF_SYSTEM      = 0x00000002,     /* system instance information */
     CSNF_VIDEO       = 0x00000004,     /* video instance information */
     CSNF_DESTROY     = 0x00000008,     /* surface is about to be destroyed */
     CSNF_FLIP        = 0x00000010      /* surface buffer pointer swapped */
} CoreSurfaceNotificationFlags;

typedef struct {
     CoreSurfaceNotificationFlags  flags;
     CoreSurface                  *surface;
} CoreSurfaceNotification;


struct _SurfaceBuffer
{
     int                 policy;        /* swapping policy for surfacemanager */
     
     struct {
          int            health;        /* currently stored in system memory? */

          int            pitch;         /* number of bytes til next line */
          void           *addr;         /* address pointing to surface data */
     } system;

     struct {
          int            health;        /* currently stored in video memory? */

          int            pitch;         /* number of bytes til next line */
          int            offset;        /* byte offset from the beginning
                                           of the framebuffer */
          struct _Chunk *chunk;         /* points to the allocated chunk */
     } video;

     CoreSurface        *surface;       /* always pointing to the surface this
                                           buffer belongs to, surfacemanger
                                           always depends on this! */
};

struct _CoreSurface
{
     DFBSurfaceCapabilities caps;

     /* size/format and instances */
     unsigned int           width;         /* pixel width of the surface */
     unsigned int           height;        /* pixel height of the surface */
     DFBSurfacePixelFormat  format;        /* pixel format of the surface */

     SurfaceBuffer         *front_buffer;  /* buffer for reading
                                              (blit from or display buffer) */
     pthread_mutex_t        front_lock;    /* mutex lock for front buffer */
     
     SurfaceBuffer         *back_buffer;   /* buffer for (reading&)writing
                                              (drawing/blitting destination) */
     pthread_mutex_t        back_lock;     /* mutex lock for back buffer,
                                              mutexes are outside of
                                              SurfaceBuffer because of flipping
                                              that just swaps the pointers */

     Reactor               *reactor;       /* event dispatcher */

     /* doubly linked list */
     CoreSurface           *next;
     CoreSurface           *prev;
};

/*
 * values for CoreSurface.(system|video).health
 */
#define CSH_INVALID      0x00 /* surface isn't stored and isn't ought to be */
#define CSH_STORED       0x01 /* surface is stored, well and kicking */
#define CSH_RESTORE      0x02 /* surface needs to be reloaded into area */

/*
 * values for CoreSurface.policy
 */
#define CSP_SYSTEMONLY   0x00 /* never try to swap into video memory */
#define CSP_VIDEOONLY    0x01 /* always and only store in video memory */
#define CSP_VIDEOLOW     0x02 /* try to store in video memory, low priority */
#define CSP_VIDEOHIGH    0x03 /* try to store in video memory, high priority */


/*
 * creates a surface with specified width and height in the specified
 * pixelformat using the specified swapping policy
 */
DFBResult surface_create( int width, int height, int format, int policy,
                          DFBSurfaceCapabilities caps, CoreSurface **surface );

/*
 * reallocates data for the specified surface
 */
DFBResult surface_reformat( CoreSurface *surface, int width, int height,
                            DFBSurfacePixelFormat format );

/*
 * helper function
 */
static inline void surface_notify_listeners( CoreSurface *surface,
                                             CoreSurfaceNotificationFlags flags)
{
     CoreSurfaceNotification notification;

     notification.flags   = flags;
     notification.surface = surface;

     reactor_dispatch( surface->reactor, &notification );
}

/*
 * really swaps front_buffer and back_buffer if they have the same policy,
 * otherwise it does a back_to_front_copy, notifies listeners
 */
void surface_flip_buffers( CoreSurface *surface );

/*
 * lock a surface for access by software, returns a pointer to the
 * surface data and the line pitch a.k.a. rowstride
 */
DFBResult surface_soft_lock( CoreSurface *surface, unsigned int flags,
                             void **data, unsigned int *pitch, int front );

/*
 * lock a surface for access by hardware that enforces a video instance
 * an therefore the data and pitch can be looked up in the surface struct's
 * video struct, however this function will fail if the surfacemanager could
 * not assure a video memory instance
 */
DFBResult surface_hard_lock( CoreSurface *surface,
                             unsigned int flags, int front );

/*
 * unlocks a previously locked surface
 * note that the other instance's health is CSH_RESTORE now, if it has
 * been CSH_STORED before
 */
void surface_unlock( CoreSurface *surface, int front );

/*
 * destroy the surface and free its instances
 */
void surface_destroy( CoreSurface *surface );

#endif
