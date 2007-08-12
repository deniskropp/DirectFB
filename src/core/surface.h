/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __CORE__SURFACE_H__
#define __CORE__SURFACE_H__

#include <directfb.h>

#include <direct/list.h>
#include <direct/serial.h>
#include <direct/util.h>

#include <fusion/object.h>
#include <fusion/reactor.h>

#include <core/coredefs.h>
#include <core/coretypes.h>


typedef enum {
     CSNF_NONE           = 0x00000000,

     CSNF_SIZEFORMAT     = 0x00000001,  /* width, height, format */
     CSNF_SYSTEM         = 0x00000002,  /* system instance information */
     CSNF_VIDEO          = 0x00000004,  /* video instance information */
     CSNF_DESTROY        = 0x00000008,  /* surface is about to be destroyed */
     CSNF_FLIP           = 0x00000010,  /* surface buffer pointer swapped */
     CSNF_FIELD          = 0x00000020,  /* active (displayed) field switched */
     CSNF_PALETTE_CHANGE = 0x00000040,  /* another palette has been set */
     CSNF_PALETTE_UPDATE = 0x00000080,  /* current palette has been altered */
     CSNF_ALPHA_RAMP     = 0x00000100,  /* alpha ramp was modified */

     CSNF_ALL            = 0x000001FF
} CoreSurfaceNotificationFlags;

typedef struct {
     CoreSurfaceNotificationFlags  flags;
     CoreSurface                  *surface;
} CoreSurfaceNotification;


typedef enum {
     CSCONF_NONE         = 0x00000000,

     CSCONF_SIZE         = 0x00000001,
     CSCONF_FORMAT       = 0x00000002,
     CSCONF_CAPS         = 0x00000004,

     CSCONF_PREALLOCATED = 0x00000010,

     CSCONF_ALL          = 0x00000017
} CoreSurfaceConfigFlags;

typedef enum {
     CSTF_NONE           = 0x00000000,

     CSTF_LAYER          = 0x00000001,  /* surface for layer */
     CSTF_WINDOW         = 0x00000002,  /* surface for window */
     CSTF_CURSOR         = 0x00000004,  /* surface for cursor */
     CSTF_FONT           = 0x00000008,  /* surface for font */

     CSTF_SHARED         = 0x00000010,  /* accessable by other processes */

     CSTF_INTERNAL       = 0x00000100,  /* system memory */
     CSTF_EXTERNAL       = 0x00000200,  /* video memory */

     CSTF_PREALLOCATED   = 0x00000400,  /* preallocated memory */

     CSTF_ALL            = 0x0000071F
} CoreSurfaceTypeFlags;

typedef struct {
     CoreSurfaceConfigFlags   flags;

     DFBDimension             size;
     DFBSurfacePixelFormat    format;
     DFBSurfaceCapabilities   caps;

     struct {
          void *addr;
          int   pitch;
     }                        preallocated[MAX_SURFACE_BUFFERS];

     DFBDimension             min_size;
} CoreSurfaceConfig;

typedef enum {
     CSP_SYSTEMONLY      = 0x00000000,  /* never try to swap
                                           into video memory */
     CSP_VIDEOLOW        = 0x00000001,  /* try to store in video memory,
                                           low priority */
     CSP_VIDEOHIGH       = 0x00000002,  /* try to store in video memory,
                                           high priority */
     CSP_VIDEOONLY       = 0x00000003   /* always and only
                                           store in video memory */
} CoreSurfacePolicy;

typedef enum {
     CSAF_NONE           = 0x00000000,

     CSAF_GPU_READ       = 0x00000001,  /* accelerator can read */
     CSAF_GPU_WRITE      = 0x00000002,  /* accelerator can write */
     CSAF_CPU_READ       = 0x00000004,  /* software can read */
     CSAF_CPU_WRITE      = 0x00000008,  /* software can write */

     CSAF_SHARED         = 0x00000010,  /* other processes can read/write as well */

     CSAF_ALL            = 0x0000001F
} CoreSurfaceAccessFlags;

typedef enum {
     CSBR_FRONT          = 0,
     CSBR_BACK           = 1,
     CSBR_IDLE           = 2
} CoreSurfaceBufferRole;

typedef enum {
     CSSF_NONE           = 0x00000000,

     CSSF_DESTROYED      = 0x00000001,  /* surface is being or has been destroyed */

     CSSF_ALL            = 0x00000001
} CoreSurfaceStateFlags;

struct __DFB_CoreSurface
{
     FusionObject             object;
     int                      magic;

     FusionSkirmish           lock;

     CoreSurfaceStateFlags    state;

     CoreSurfaceConfig        config;
     CoreSurfaceTypeFlags     type;

     CoreSurfaceNotificationFlags notifications;

     DirectSerial             serial;

     int                      field;
     __u8                     alpha_ramp[4];

     CoreSurfaceBuffer       *buffers[MAX_SURFACE_BUFFERS];
     int                      num_buffers;

     int                      buffer_indices[MAX_SURFACE_BUFFERS];

     unsigned int             flips;

     CorePalette             *palette;
     GlobalReaction           palette_reaction;

     FusionSHMPoolShared     *shmpool;
};


/*
 * Creates a pool of surface objects.
 */
FusionObjectPool *dfb_surface_pool_create( const FusionWorld *world );

/*
 * Generates dfb_surface_ref(), dfb_surface_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreSurface, dfb_surface )


DFBResult dfb_surface_create        ( CoreDFB                      *core,
                                      const CoreSurfaceConfig      *config,
                                      CoreSurfaceTypeFlags          type,
                                      CorePalette                  *palette,
                                      CoreSurface                 **ret_surface );

DFBResult dfb_surface_create_simple ( CoreDFB                      *core,
                                      int                           width,
                                      int                           height,
                                      DFBSurfacePixelFormat         format,
                                      DFBSurfaceCapabilities        caps,
                                      CoreSurfaceTypeFlags          type,
                                      CorePalette                  *palette,
                                      CoreSurface                 **ret_surface );

DFBResult dfb_surface_notify        ( CoreSurface                  *surface,
                                      CoreSurfaceNotificationFlags  flags);

DFBResult dfb_surface_flip          ( CoreSurface                  *surface,
                                      bool                          swap );

DFBResult dfb_surface_reconfig      ( CoreSurface                  *surface,
                                      const CoreSurfaceConfig      *config );

DFBResult dfb_surface_lock_buffer   ( CoreSurface                  *surface,
                                      CoreSurfaceBufferRole         role,
                                      CoreSurfaceAccessFlags        access,
                                      CoreSurfaceBufferLock        *ret_lock );

DFBResult dfb_surface_unlock_buffer ( CoreSurface                  *surface,
                                      CoreSurfaceBufferLock        *lock );

DFBResult dfb_surface_set_palette   ( CoreSurface                  *surface,
                                      CorePalette                  *palette );

DFBResult dfb_surface_set_field     ( CoreSurface                  *surface,
                                      int                           field );

DFBResult dfb_surface_set_alpha_ramp( CoreSurface                  *surface,
                                      __u8                          a0,
                                      __u8                          a1,
                                      __u8                          a2,
                                      __u8                          a3 );


static inline DFBResult
dfb_surface_lock( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     return fusion_skirmish_prevail( &surface->lock );
}

static inline DFBResult
dfb_surface_unlock( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     return fusion_skirmish_dismiss( &surface->lock );
}

static inline CoreSurfaceBuffer *
dfb_surface_get_buffer( CoreSurface           *surface,
                        CoreSurfaceBufferRole  role )
{
     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( role == CSBR_FRONT || role == CSBR_BACK || role == CSBR_IDLE );

     D_ASSERT( surface->num_buffers > 0 );

     return surface->buffers[ surface->buffer_indices[(surface->flips + role) % surface->num_buffers] ];
}

static inline void *
dfb_surface_data_offset( const CoreSurface *surface,
                         void              *data,
                         int                pitch,
                         int                x,
                         int                y )
{
     D_ASSERT( surface != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( pitch > 0 );
     D_ASSERT( x >= 0 );
     D_ASSERT( x < surface->config.size.w );
     D_ASSERT( y >= 0 );
     D_ASSERT( y < surface->config.size.h );

     if (surface->config.caps & DSCAPS_SEPARATED) {
          if (y & 1)
               y += surface->config.size.h;

          y >>= 1;
     }

     return data + pitch * y + DFB_BYTES_PER_LINE( surface->config.format, x );
}

static inline void
dfb_surface_calc_buffer_size( CoreSurface *surface,
                              int          byte_align,
                              int          pixel_align,
                              int         *ret_pitch,
                              int         *ret_size )
{
     DFBSurfacePixelFormat format;
     int                   width;
     int                   pitch;

     D_MAGIC_ASSERT( surface, CoreSurface );

     format = surface->config.format;

     width = direct_util_align( surface->config.size.w, pixel_align );
     pitch = direct_util_align( DFB_BYTES_PER_LINE( format, width ), byte_align );

     if (ret_pitch)
          *ret_pitch = pitch;

     if (ret_size)
          *ret_size = pitch * DFB_PLANE_MULTIPLY( format, surface->config.size.h );
}

static inline void
dfb_surface_caps_apply_policy( CoreSurfacePolicy       policy,
                               DFBSurfaceCapabilities *caps )
{
     switch (policy) {
          case CSP_SYSTEMONLY:
               *caps = (DFBSurfaceCapabilities)((*caps & ~DSCAPS_VIDEOONLY) | DSCAPS_SYSTEMONLY);
               break;

          case CSP_VIDEOONLY:
               *caps = (DFBSurfaceCapabilities)((*caps & ~DSCAPS_SYSTEMONLY) | DSCAPS_VIDEOONLY);
               break;

          default:
               *caps = (DFBSurfaceCapabilities)(*caps & ~(DSCAPS_SYSTEMONLY | DSCAPS_VIDEOONLY));
               break;
     }
}

static inline DFBResult
dfb_surface_resize( CoreSurface *surface,
                    int          width,
                    int          height )
{
     CoreSurfaceConfig config;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     config.flags  = CSCONF_SIZE;
     config.size.w = width;
     config.size.h = height;

     return dfb_surface_reconfig( surface, &config );
}

static inline DFBResult
dfb_surface_reformat( CoreSurface           *surface,
                      int                    width,
                      int                    height,
                      DFBSurfacePixelFormat  format )
{
     CoreSurfaceConfig config;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     config.flags  = CSCONF_SIZE | CSCONF_FORMAT;
     config.size.w = width;
     config.size.h = height;
     config.format = format;

     return dfb_surface_reconfig( surface, &config );
}

/* global reactions */
ReactionResult _dfb_surface_palette_listener( const void *msg_data,
                                              void       *ctx );

typedef enum {
     DFB_LAYER_REGION_SURFACE_LISTENER,
     DFB_WINDOWSTACK_BACKGROUND_IMAGE_LISTENER
} DFB_SURFACE_GLOBALS;

#endif

