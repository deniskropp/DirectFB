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

#ifndef __SURFACES_H__
#define __SURFACES_H__

#include <directfb.h>

#include <core/fusion/list.h>
#include <core/fusion/lock.h>
#include <core/fusion/reactor.h>

#include <core/coretypes.h>


struct _Chunk;

typedef enum {
     CSNF_SIZEFORMAT     = 0x00000001,  /* width, height, format */
     CSNF_SYSTEM         = 0x00000002,  /* system instance information */
     CSNF_VIDEO          = 0x00000004,  /* video instance information */
     CSNF_DESTROY        = 0x00000008,  /* surface is about to be destroyed */
     CSNF_FLIP           = 0x00000010,  /* surface buffer pointer swapped */
     CSNF_SET_EVEN       = 0x00000020,  /* set the even field of an interlaced
                                           surface buffer active */
     CSNF_SET_ODD        = 0x00000040   /* set the odd field of an interlaced
                                           surface buffer active */
} CoreSurfaceNotificationFlags;

typedef struct {
     CoreSurfaceNotificationFlags  flags;
     CoreSurface                  *surface;
} CoreSurfaceNotification;

typedef enum {
     CSH_INVALID         = 0x00000000,  /* surface isn't stored */
     CSH_STORED          = 0x00000001,  /* surface is stored,
                                           well and kicking */
     CSH_RESTORE         = 0x00000002   /* surface needs to be
                                           reloaded into area */
} CoreSurfaceHealth;

typedef enum {
     CSP_SYSTEMONLY      = 0x00000000,  /* never try to swap
                                           into video memory */
     CSP_VIDEOONLY       = 0x00000001,  /* always and only
                                           store in video memory */
     CSP_VIDEOLOW        = 0x00000002,  /* try to store in video memory,
                                           low priority */
     CSP_VIDEOHIGH       = 0x00000003   /* try to store in video memory,
                                           high priority */
} CoreSurfacePolicy;

typedef enum {
     SBF_NONE            = 0x00000000,
     SBF_FOREIGN_SYSTEM  = 0x00000001   /* system memory is preallocated by
                                           application, won't be freed */
} SurfaceBufferFlags;

typedef enum {
     VAF_NONE            = 0x00000000,  /* no write access happened since last
                                           clearing of all bits */
     VAF_SOFTWARE_WRITE  = 0x00000001,  /* software wrote to buffer */
     VAF_HARDWARE_WRITE  = 0x00000002,  /* hardware wrote to buffer */
     VAF_SOFTWARE_READ   = 0x00000004,  /* software read from buffer */
     VAF_HARDWARE_READ   = 0x00000008   /* hardware read from buffer */
} VideoAccessFlags;


struct _SurfaceBuffer
{
     SurfaceBufferFlags      flags;     /* additional information */
     CoreSurfacePolicy       policy;    /* swapping policy for surfacemanager */

     struct {
          CoreSurfaceHealth  health;    /* currently stored in system memory? */
          int                locked;    /* system instance is locked,
                                           stick to it */

          int                pitch;     /* number of bytes til next line */
          void              *addr;      /* address pointing to surface data */
     } system;

     struct {
          CoreSurfaceHealth  health;    /* currently stored in video memory? */
          int                locked;    /* video instance is locked, don't
                                           try to kick out, could deadlock */

          VideoAccessFlags   access;    /* information about recent read/write
                                           accesses to video buffer memory */

          int                pitch;     /* number of bytes til next line */
          int                offset;    /* byte offset from the beginning
                                           of the framebuffer */
          struct _Chunk     *chunk;     /* points to the allocated chunk */
     } video;

     CoreSurface            *surface;   /* always pointing to the surface this
                                           buffer belongs to, surfacemanger
                                           always depends on this! */
};

struct _CoreSurface
{
     FusionLink             link;

     /* size/format and instances */
     unsigned int           width;         /* pixel width of the surface */
     unsigned int           height;        /* pixel height of the surface */
     DFBSurfacePixelFormat  format;        /* pixel format of the surface */
     DFBSurfaceCapabilities caps;

     SurfaceBuffer         *front_buffer;  /* buffer for reading
                                              (blit from or display buffer) */
     FusionSkirmish         front_lock;    /* skirmish lock for front buffer */

     SurfaceBuffer         *back_buffer;   /* buffer for (reading&)writing
                                              (drawing/blitting destination) */
     FusionSkirmish         back_lock;     /* skirmish lock for back buffer,
                                              mutexes are outside of
                                              SurfaceBuffer because of flipping
                                              that just swaps the pointers */

     FusionReactor         *reactor;       /* event dispatcher */
     
     SurfaceManager        *manager;
};

/*
 * creates a surface with specified width and height in the specified
 * pixelformat using the specified swapping policy
 */
DFBResult dfb_surface_create( int                      width,
                              int                      height,
                              DFBSurfacePixelFormat    format,
                              CoreSurfacePolicy        policy,
                              DFBSurfaceCapabilities   caps,
                              CoreSurface            **surface );

/*
 * like surface_create, but with preallocated system memory that won't be
 * freed on surface destruction
 */
DFBResult dfb_surface_create_preallocated( int                      width,
                                           int                      height,
                                           DFBSurfacePixelFormat    format,
                                           CoreSurfacePolicy        policy,
                                           DFBSurfaceCapabilities   caps,
                                           void                    *front_data,
                                           void                    *back_data,
                                           int                      front_pitch,
                                           int                      back_pitch,
                                           CoreSurface            **surface );

/*
 * initialize surface structure, not required for surface_create_*
 */
DFBResult dfb_surface_init ( CoreSurface           *surface,
                             int                    width,
                             int                    height,
                             DFBSurfacePixelFormat  format,
                             DFBSurfaceCapabilities caps );

/*
 * reallocates data for the specified surface
 */
DFBResult dfb_surface_reformat( CoreSurface           *surface,
                                int                    width,
                                int                    height,
                                DFBSurfacePixelFormat  format );

/*
 * Change policies of buffers.
 */
DFBResult dfb_surface_reconfig( CoreSurface       *surface,
                                CoreSurfacePolicy  front_policy,
                                CoreSurfacePolicy  back_policy );

/*
 * helper function
 */
static inline void dfb_surface_notify_listeners( CoreSurface *surface,
                                                 CoreSurfaceNotificationFlags flags)
{
     CoreSurfaceNotification notification;

     notification.flags   = flags;
     notification.surface = surface;

     reactor_dispatch( surface->reactor, &notification, true );
}

/*
 * really swaps front_buffer and back_buffer if they have the same policy,
 * otherwise it does a back_to_front_copy, notifies listeners
 */
void dfb_surface_flip_buffers( CoreSurface *surface );

/*
 * This is a utility function for easier usage.
 * It locks the surface maneger, does a surface_software_lock, and unlocks
 * the surface manager.
 */
DFBResult dfb_surface_soft_lock( CoreSurface          *surface,
                                 DFBSurfaceLockFlags   flags,
                                 void                **data,
                                 int                  *pitch,
                                 int                   front );

/*
 * unlocks a previously locked surface
 * note that the other instance's health is CSH_RESTORE now, if it has
 * been CSH_STORED before
 */
void dfb_surface_unlock( CoreSurface *surface, int front );

/*
 * destroy the surface and free its instances
 */
void dfb_surface_destroy( CoreSurface *surface );


#endif

