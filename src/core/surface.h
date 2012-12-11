/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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
     CSNF_DISPLAY        = 0x00000200,  /* surface buffer displayed */
     CSNF_FRAME          = 0x00000400,  /* flip count ack */
     CSNF_BUFFER_ALLOCATION_DESTROY
                         = 0x00000800,  /* Buffer allocation about to be destroyed */

     CSNF_ALL            = 0x00000FFF
} CoreSurfaceNotificationFlags;

typedef struct {
     CoreSurfaceNotificationFlags  flags;
     CoreSurface                  *surface;

     /* The following field is used only by the CSNF_DISPLAY message. */
     int                           index;

     /* The following fields are used only by the CSNF_BUFFER_ALLOCATION_DESTROY message. */
     CoreSurfaceBuffer *buffer_no_access;  /* Pointer to associated CoreSurfaceBuffer being 
                                              destroyed. Do not dereference. */
     void              *surface_data;      /* CoreSurface's shared driver specific data. */
     int                surface_object_id; /* CoreSurface's Fusion ID. */

     unsigned int                  flip_count;     
} CoreSurfaceNotification;


typedef enum {
     CSCH_NOTIFICATION,
     CSCH_EVENT
} CoreSurfaceChannel;


typedef enum {
     CSCONF_NONE         = 0x00000000,

     CSCONF_SIZE         = 0x00000001,
     CSCONF_FORMAT       = 0x00000002,
     CSCONF_CAPS         = 0x00000004,
     CSCONF_COLORSPACE   = 0x00000008,

     CSCONF_PREALLOCATED = 0x00000010,

     CSCONF_ALL          = 0x0000001F
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
     DFBSurfaceColorSpace     colorspace;
     DFBSurfaceCapabilities   caps;

     struct {
          void                    *addr;               /* " */
          unsigned long            phys;               /* " */
          unsigned long            offset;             /* " */
          unsigned int             pitch;              /* " */

          void                    *handle;             /* " */
     }                        preallocated[MAX_SURFACE_BUFFERS];

     CoreSurfacePoolID        preallocated_pool_id;

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

     CSAF_READ           = 0x00000001,  /* accessor may read */
     CSAF_WRITE          = 0x00000002,  /* accessor may write */

     CSAF_SHARED         = 0x00000010,  /* other processes can read/write at the same time (shared mapping) */

     CSAF_ALL            = 0x00000013
} CoreSurfaceAccessFlags;

typedef enum {
     CSAID_NONE          = 0x00000000,  /* none or unknown accessor */

     CSAID_CPU           = 0x00000001,  /* local processor, where DirectFB is running on, could be app or sw fallback */

     CSAID_GPU           = 0x00000002,  /* primary accelerator, as in traditional 'gfxcard' core (ACCEL0) */

     CSAID_ACCEL0        = 0x00000002,  /* accelerators, decoders etc. (CSAID_ACCEL0 + accel_id<0-5>) */
     CSAID_ACCEL1        = 0x00000003,
     CSAID_ACCEL2        = 0x00000004,
     CSAID_ACCEL3        = 0x00000005,
     CSAID_ACCEL4        = 0x00000006,
     CSAID_ACCEL5        = 0x00000007,

     CSAID_LAYER0        = 0x00000008,  /* display layers, registered by layer core (CSAID_LAYER0 + layer_id<0-MAX_LAYERS>) */
     CSAID_LAYER1        = 0x00000009,
     CSAID_LAYER2        = 0x0000000a,
     CSAID_LAYER3        = 0x0000000b,
     CSAID_LAYER4        = 0x0000000c,
     CSAID_LAYER5        = 0x0000000d,
     CSAID_LAYER6        = 0x0000000e,
     CSAID_LAYER7        = 0x0000000f,
     CSAID_LAYER8        = 0x00000010,
     CSAID_LAYER9        = 0x00000011,
     CSAID_LAYER10       = 0x00000012,
     CSAID_LAYER11       = 0x00000013,
     CSAID_LAYER12       = 0x00000014,
     CSAID_LAYER13       = 0x00000015,
     CSAID_LAYER14       = 0x00000016,
     CSAID_LAYER15       = 0x00000017,

     _CSAID_NUM          = 0x00000018,  /* number of statically assigned IDs for usage in static arrays */

     CSAID_ANY           = 0x00000100   /* any other accessor needs to be registered using IDs starting from here */
} CoreSurfaceAccessorID;

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
     unsigned long            resource_id;   /* layer id, window id, or user specified */

     int                      rotation;

     CoreSurfaceNotificationFlags notifications;

     DirectSerial             serial;

     int                      field;
     u8                       alpha_ramp[4];

     CoreSurfaceBuffer      **buffers;
     CoreSurfaceBuffer       *left_buffers[MAX_SURFACE_BUFFERS];
     CoreSurfaceBuffer       *right_buffers[MAX_SURFACE_BUFFERS];
     int                      num_buffers;
     int                      buffer_indices[MAX_SURFACE_BUFFERS];

     u32                      flips;

     CorePalette             *palette;
     GlobalReaction           palette_reaction;

     FusionSHMPoolShared     *shmpool;

     void                    *data;         /* Shared system driver-specific data for this surface. */

     FusionCall               call;

     FusionVector             clients;
     u32                      flips_acked;
};

#define CORE_SURFACE_ASSERT(surface)                                                           \
     do {                                                                                      \
          D_MAGIC_ASSERT( surface, CoreSurface );                                              \
     } while (0)


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
                                      unsigned long                 resource_id,
                                      CorePalette                  *palette,
                                      CoreSurface                 **ret_surface );

DFBResult dfb_surface_create_simple ( CoreDFB                      *core,
                                      int                           width,
                                      int                           height,
                                      DFBSurfacePixelFormat         format,
                                      DFBSurfaceColorSpace          colorspace,
                                      DFBSurfaceCapabilities        caps,
                                      CoreSurfaceTypeFlags          type,
                                      unsigned long                 resource_id,
                                      CorePalette                  *palette,
                                      CoreSurface                 **ret_surface );

DFBResult dfb_surface_init_palette  ( CoreDFB                      *core,
                                      CoreSurface                  *surface );

DFBResult dfb_surface_notify        ( CoreSurface                  *surface,
                                      CoreSurfaceNotificationFlags  flags);

DFBResult dfb_surface_notify_display( CoreSurface                  *surface,
                                      CoreSurfaceBuffer            *buffer);

DFBResult dfb_surface_notify_display2( CoreSurface                  *surface,
                                       int                           index );

DFBResult dfb_surface_notify_frame  ( CoreSurface                  *surface,
                                      unsigned int                  flip_count );

/*
     Prepares and sends a notification message that a change is about to happen to the specified
     surface buffer pool allocation.  The notification message will be received by all pocesses
     that have listeners attached to the associated CoreSurface's reactor.

     At present, only THE CSNF_BUFFER_ALLOCATION_DESTROY message is handled.
*/
DFBResult dfb_surface_pool_notify   ( CoreSurface                  *surface,
                                      CoreSurfaceBuffer            *buffer,
                                      CoreSurfaceAllocation        *allocation,
                                      CoreSurfaceNotificationFlags  flags );

DFBResult dfb_surface_flip          ( CoreSurface                  *surface,
                                      bool                          swap );

DFBResult dfb_surface_dispatch_event( CoreSurface                  *surface,
                                      DFBSurfaceEventType           type );

DFBResult dfb_surface_dispatch_update( CoreSurface                  *surface,
                                       const DFBRegion              *update,
                                       const DFBRegion              *update_right );

DFBResult dfb_surface_reconfig      ( CoreSurface                  *surface,
                                      const CoreSurfaceConfig      *config );

DFBResult dfb_surface_destroy_buffers( CoreSurface                 *surface );

DFBResult dfb_surface_deallocate_buffers( CoreSurface              *surface );

DFBResult dfb_surface_destroy       ( CoreSurface                  *surface );

DFBResult dfb_surface_lock_buffer   ( CoreSurface                  *surface,
                                      CoreSurfaceBufferRole         role,
                                      CoreSurfaceAccessorID         accessor,
                                      CoreSurfaceAccessFlags        access,
                                      CoreSurfaceBufferLock        *ret_lock );

DFBResult dfb_surface_lock_buffer2  ( CoreSurface                  *surface,
                                      CoreSurfaceBufferRole         role,
                                      u32                           flip_count,
                                      DFBSurfaceStereoEye           eye,
                                      CoreSurfaceAccessorID         accessor,
                                      CoreSurfaceAccessFlags        access,
                                      CoreSurfaceBufferLock        *ret_lock );

DFBResult dfb_surface_unlock_buffer ( CoreSurface                  *surface,
                                      CoreSurfaceBufferLock        *lock );

DFBResult dfb_surface_read_buffer   ( CoreSurface                  *surface,
                                      CoreSurfaceBufferRole         role,
                                      void                         *destination,
                                      int                           pitch,
                                      const DFBRectangle           *rect );

DFBResult dfb_surface_write_buffer  ( CoreSurface                  *surface,
                                      CoreSurfaceBufferRole         role,
                                      const void                   *source,
                                      int                           pitch,
                                      const DFBRectangle           *rect );

DFBResult dfb_surface_dump_buffer   ( CoreSurface                  *surface,
                                      CoreSurfaceBufferRole         role,
                                      const char                   *path,
                                      const char                   *prefix );

DFBResult dfb_surface_dump_buffer2  ( CoreSurface                  *surface,
                                      CoreSurfaceBufferRole         role,
                                      DFBSurfaceStereoEye           eye,
                                      const char                   *path,
                                      const char                   *prefix );

DFBResult dfb_surface_set_palette   ( CoreSurface                  *surface,
                                      CorePalette                  *palette );

DFBResult dfb_surface_set_field     ( CoreSurface                  *surface,
                                      int                           field );

DFBResult dfb_surface_set_alpha_ramp( CoreSurface                  *surface,
                                      u8                            a0,
                                      u8                            a1,
                                      u8                            a2,
                                      u8                            a3 );

DFBResult dfb_surface_clear_buffers  ( CoreSurface                  *surface );


static __inline__ DirectResult
dfb_surface_lock( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     return fusion_skirmish_prevail( &surface->lock );
}

static __inline__ DirectResult
dfb_surface_trylock( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     return fusion_skirmish_swoop( &surface->lock );
}

static __inline__ DirectResult
dfb_surface_unlock( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     return fusion_skirmish_dismiss( &surface->lock );
}

static __inline__ CoreSurfaceBuffer *
dfb_surface_get_buffer( CoreSurface           *surface,
                        CoreSurfaceBufferRole  role )
{
     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( role == CSBR_FRONT || role == CSBR_BACK || role == CSBR_IDLE );

     D_ASSERT( surface->num_buffers > 0 );

     return surface->buffers[ surface->buffer_indices[(surface->flips + role) % surface->num_buffers] ];
}

static __inline__ CoreSurfaceBuffer *
dfb_surface_get_buffer2( CoreSurface           *surface,
                         CoreSurfaceBufferRole  role,
                         DFBSurfaceStereoEye    eye )
{
     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( role == CSBR_FRONT || role == CSBR_BACK || role == CSBR_IDLE );
     D_ASSERT( eye == DSSE_LEFT || eye == DSSE_RIGHT );

     D_ASSERT( surface->num_buffers > 0 );

     if (eye == DSSE_LEFT)
          return surface->left_buffers[ surface->buffer_indices[(surface->flips + role) % surface->num_buffers] ];

     return surface->right_buffers[ surface->buffer_indices[(surface->flips + role) % surface->num_buffers] ];
}

static __inline__ CoreSurfaceBuffer *
dfb_surface_get_buffer3( CoreSurface           *surface,
                         CoreSurfaceBufferRole  role,
                         DFBSurfaceStereoEye    eye,
                         u32                    flip_count )
{
     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( role == CSBR_FRONT || role == CSBR_BACK || role == CSBR_IDLE );
     D_ASSERT( eye == DSSE_LEFT || eye == DSSE_RIGHT );

     D_ASSERT( surface->num_buffers > 0 );

     if (eye == DSSE_LEFT)
          return surface->left_buffers[ surface->buffer_indices[(flip_count + role) % surface->num_buffers] ];

     return surface->right_buffers[ surface->buffer_indices[(flip_count + role) % surface->num_buffers] ];
}

static __inline__ void *
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

     return (u8*)data + pitch * y + DFB_BYTES_PER_LINE( surface->config.format, x );
}

static __inline__ void
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

static __inline__ void
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

static __inline__ DFBResult
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

static __inline__ DFBResult
dfb_surface_reformat( CoreSurface           *surface,
                      int                    width,
                      int                    height,
                      DFBSurfacePixelFormat  format )
{
     CoreSurfaceConfig config;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     config.flags  = (CoreSurfaceConfigFlags)(CSCONF_SIZE | CSCONF_FORMAT);
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

