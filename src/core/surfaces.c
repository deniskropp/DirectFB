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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <malloc.h>

#include <pthread.h>

#include <core/fusion/shmalloc.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>

#include <gfx/util.h>
#include <misc/util.h>
#include <misc/mem.h>

static DFBResult dfb_surface_allocate_buffer  ( CoreSurface        *surface,
                                                CoreSurfacePolicy   policy,
                                                SurfaceBuffer     **buffer );
static DFBResult dfb_surface_reallocate_buffer( CoreSurface        *surface,
                                                SurfaceBuffer      *buffer );
static void dfb_surface_deallocate_buffer     ( CoreSurface        *surface,
                                                SurfaceBuffer      *buffer );

static ReactionResult palette_listener( const void *msg_data,
                                        void       *ctx );

/* internal functions needed to avoid side effects */

static inline void video_access_by_hardware( SurfaceBuffer       *buffer,
                                             DFBSurfaceLockFlags  flags )
{
     if (flags & DSLF_READ) {
          if (buffer->video.access & VAF_SOFTWARE_WRITE) {
               dfb_gfxcard_flush_texture_cache();
               buffer->video.access &= ~VAF_SOFTWARE_WRITE;
          }
          buffer->video.access |= VAF_HARDWARE_READ;
     }
     if (flags & DSLF_WRITE)
          buffer->video.access |= VAF_HARDWARE_WRITE;
}

static inline void video_access_by_software( SurfaceBuffer       *buffer,
                                             DFBSurfaceLockFlags  flags )
{
     if (flags & DSLF_WRITE) {
          if (buffer->video.access & VAF_HARDWARE_READ) {
               dfb_gfxcard_sync();
               buffer->video.access &= ~VAF_HARDWARE_READ;
          }
          buffer->video.access |= VAF_SOFTWARE_WRITE;
     }
     if (flags & (DSLF_READ | DSLF_WRITE)) {
          if (buffer->video.access & VAF_HARDWARE_WRITE) {
               dfb_gfxcard_sync();
               buffer->video.access &= ~VAF_HARDWARE_WRITE;
          }
          buffer->video.access |= VAF_SOFTWARE_READ;
     }
}

static void surface_destructor( FusionObject *object, bool zombie )
{
     CoreSurface *surface = (CoreSurface*) object;

     DEBUGMSG("DirectFB/core/surfaces: destroying %p (%dx%d)%s\n", surface,
              surface->width, surface->height, zombie ? " (ZOMBIE)" : "");

     dfb_surface_destroy( surface, false );

     skirmish_destroy( &surface->lock );

     fusion_object_destroy( object );
}

/** public **/

FusionObjectPool *dfb_surface_pool_create()
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Surface Pool",
                                       sizeof(CoreSurface),
                                       sizeof(CoreSurfaceNotification),
                                       surface_destructor );

     return pool;
}

DFBResult dfb_surface_create( int width, int height, DFBSurfacePixelFormat format,
                              CoreSurfacePolicy policy,
                              DFBSurfaceCapabilities caps, CorePalette *palette,
                              CoreSurface **surface )
{
     DFBResult    ret;
     CoreSurface *s;

     s = (CoreSurface*) fusion_object_create( dfb_gfxcard_surface_pool() );

     ret = dfb_surface_init( s, width, height, format, caps, palette );
     if (ret) {
          fusion_object_destroy( &s->object );
          return ret;
     }

     switch (policy) {
          case CSP_SYSTEMONLY:
               s->caps |= DSCAPS_SYSTEMONLY;
               break;
          case CSP_VIDEOONLY:
               s->caps |= DSCAPS_VIDEOONLY;
               break;
          default:
               ;
     }


     ret = dfb_surface_allocate_buffer( s, policy, &s->front_buffer );
     if (ret) {
          fusion_object_destroy( &s->object );
          return ret;
     }

     if (caps & DSCAPS_FLIPPING) {
          ret = dfb_surface_allocate_buffer( s, policy, &s->back_buffer );
          if (ret) {
               dfb_surface_deallocate_buffer( s, s->front_buffer );

               fusion_object_destroy( &s->object );
               return ret;
          }
     }
     else
          s->back_buffer = s->front_buffer;


     *surface = s;

     return DFB_OK;
}

DFBResult dfb_surface_create_preallocated( int width, int height,
                                           DFBSurfacePixelFormat format,
                                           CoreSurfacePolicy policy, DFBSurfaceCapabilities caps,
                                           CorePalette *palette,
                                           void *front_buffer, void *back_buffer,
                                           int front_pitch, int back_pitch,
                                           CoreSurface **surface )
{
     DFBResult    ret;
     CoreSurface *s;

     if (policy == CSP_VIDEOONLY)
          return DFB_UNSUPPORTED;

     s = (CoreSurface*) fusion_object_create( dfb_gfxcard_surface_pool() );

     ret = dfb_surface_init( s, width, height, format, caps, palette );
     if (ret) {
          fusion_object_destroy( &s->object );
          return ret;
     }

     if (policy == CSP_SYSTEMONLY)
          s->caps |= DSCAPS_SYSTEMONLY;


     s->front_buffer = (SurfaceBuffer *) shcalloc( 1, sizeof(SurfaceBuffer) );

     s->front_buffer->flags   = SBF_FOREIGN_SYSTEM;
     s->front_buffer->policy  = policy;
     s->front_buffer->surface = s;

     s->front_buffer->system.health = CSH_STORED;
     s->front_buffer->system.pitch  = front_pitch;
     s->front_buffer->system.addr   = front_buffer;

     if (caps & DSCAPS_FLIPPING) {
          s->back_buffer = (SurfaceBuffer *) shcalloc( 1, sizeof(SurfaceBuffer) );
          memcpy( s->back_buffer, s->front_buffer, sizeof(SurfaceBuffer) );

          s->back_buffer->system.pitch  = back_pitch;
          s->back_buffer->system.addr   = back_buffer;
     }
     else
          s->back_buffer = s->front_buffer;


     *surface = s;

     return DFB_OK;
}

DFBResult dfb_surface_reformat( CoreSurface *surface, int width, int height,
                                DFBSurfacePixelFormat format )
{
     int old_width, old_height;
     DFBSurfacePixelFormat old_format;
     DFBResult ret;

     if (surface->front_buffer->flags & SBF_FOREIGN_SYSTEM ||
         surface->back_buffer->flags  & SBF_FOREIGN_SYSTEM)
     {
          return DFB_UNSUPPORTED;
     }

     old_width  = surface->width;
     old_height = surface->height;
     old_format = surface->format;

     if (width      <= surface->min_width &&
         old_width  <= surface->min_width &&
         height     <= surface->min_height &&
         old_height <= surface->min_height &&
         old_format == surface->format)
          return DFB_OK;

     surface->width  = width;
     surface->height = height;
     surface->format = format;
     
     dfb_surfacemanager_lock( surface->manager );

     skirmish_prevail( &surface->front_lock );
     skirmish_prevail( &surface->back_lock );

     ret = dfb_surface_reallocate_buffer( surface, surface->front_buffer );
     if (ret) {
          surface->width  = old_width;
          surface->height = old_height;
          surface->format = old_format;

          skirmish_dismiss( &surface->front_lock );
          skirmish_dismiss( &surface->back_lock );

          dfb_surfacemanager_unlock( surface->manager );
          return ret;
     }

     if (surface->caps & DSCAPS_FLIPPING) {
          ret = dfb_surface_reallocate_buffer( surface, surface->back_buffer );
          if (ret) {
               surface->width  = old_width;
               surface->height = old_height;
               surface->format = old_format;

               dfb_surface_reallocate_buffer( surface, surface->front_buffer );

               skirmish_dismiss( &surface->front_lock );
               skirmish_dismiss( &surface->back_lock );

               dfb_surfacemanager_unlock( surface->manager );
               return ret;
          }
     }

     dfb_surfacemanager_unlock( surface->manager );

     if (DFB_PIXELFORMAT_IS_INDEXED( format ) && !surface->palette) {
          CorePalette *palette = dfb_palette_create( 256 );
          if (!palette)
               return DFB_FAILURE;

          dfb_surface_set_palette( surface, palette );

          dfb_palette_unref( palette );
     }

     dfb_surface_notify_listeners( surface, CSNF_SIZEFORMAT |
                                   CSNF_SYSTEM | CSNF_VIDEO );

     skirmish_dismiss( &surface->front_lock );
     skirmish_dismiss( &surface->back_lock );

     return DFB_OK;
}

DFBResult dfb_surface_reconfig( CoreSurface       *surface,
                                CoreSurfacePolicy  front_policy,
                                CoreSurfacePolicy  back_policy ) 
{
     DFBResult      ret;
     SurfaceBuffer *old_front;
     SurfaceBuffer *old_back;
     bool           new_front = surface->front_buffer->policy != front_policy;

     if (surface->front_buffer->flags & SBF_FOREIGN_SYSTEM ||
         surface->back_buffer->flags  & SBF_FOREIGN_SYSTEM)
     {
          return DFB_UNSUPPORTED;
     }

     dfb_surfacemanager_lock( surface->manager );
     skirmish_prevail( &surface->front_lock );
     skirmish_prevail( &surface->back_lock );
     dfb_surfacemanager_unlock( surface->manager );

     old_front = surface->front_buffer;
     old_back = surface->back_buffer;

     if (new_front) {
          ret = dfb_surface_allocate_buffer( surface, front_policy, &surface->front_buffer );
          if (ret) {
               skirmish_dismiss( &surface->front_lock );
               skirmish_dismiss( &surface->back_lock );
               return ret;
          }
     }

     if (surface->caps & DSCAPS_FLIPPING) {
          ret = dfb_surface_allocate_buffer( surface, back_policy, &surface->back_buffer );
          if (ret) {
               if (new_front) {
                    dfb_surface_deallocate_buffer( surface, surface->front_buffer );
                    surface->front_buffer = old_front;
               }

               skirmish_dismiss( &surface->front_lock );
               skirmish_dismiss( &surface->back_lock );
               return ret;
          }
     }
     else {
          surface->back_buffer = surface->front_buffer;
     }

     if (new_front)
          dfb_surface_deallocate_buffer( surface, old_front );

     if (old_front != old_back)
          dfb_surface_deallocate_buffer ( surface, old_back );

     dfb_surface_notify_listeners( surface, CSNF_SIZEFORMAT |
                                   CSNF_SYSTEM | CSNF_VIDEO );

     skirmish_dismiss( &surface->front_lock );
     skirmish_dismiss( &surface->back_lock );

     return DFB_OK;
}

DFBResult
dfb_surface_set_palette( CoreSurface *surface,
                         CorePalette *palette )
{
     DFB_ASSERT( surface != NULL );

     if (surface->palette) {
          dfb_palette_detach( surface->palette, palette_listener, surface );
          dfb_palette_unlink( surface->palette );

          surface->palette = NULL;
     }

     if (palette) {
          dfb_palette_link( &surface->palette, palette );
          dfb_palette_attach( palette, palette_listener, surface );
     }

     return DFB_OK;
}

void dfb_surface_flip_buffers( CoreSurface *surface )
{
     SurfaceBuffer *tmp;

     DFB_ASSERT(surface->back_buffer->policy == surface->front_buffer->policy);

     dfb_surfacemanager_lock( surface->manager );

     skirmish_prevail( &surface->front_lock );
     skirmish_prevail( &surface->back_lock );

     tmp = surface->front_buffer;
     surface->front_buffer = surface->back_buffer;
     surface->back_buffer = tmp;

     dfb_surfacemanager_unlock( surface->manager );

     dfb_surface_notify_listeners( surface, CSNF_FLIP );

     skirmish_dismiss( &surface->front_lock );
     skirmish_dismiss( &surface->back_lock );
}

DFBResult dfb_surface_soft_lock( CoreSurface *surface, DFBSurfaceLockFlags flags,
                                 void **data, int *pitch, int front )
{
     DFBResult ret;

     dfb_surfacemanager_lock( surface->manager );
     ret = dfb_surface_software_lock( surface, flags, data, pitch, front );
     dfb_surfacemanager_unlock( surface->manager );

     return ret;
}

DFBResult dfb_surface_software_lock( CoreSurface *surface, DFBSurfaceLockFlags flags,
                                     void **data, unsigned int *pitch, int front )
{
     SurfaceBuffer *buffer;

     DFB_ASSERT( flags != 0 );

     if (front) {
          skirmish_prevail( &surface->front_lock );
          buffer = surface->front_buffer;
     }
     else {
          skirmish_prevail( &surface->back_lock );
          buffer = surface->back_buffer;
     }

     switch (buffer->policy) {
          case CSP_SYSTEMONLY:
               buffer->system.locked++;
               *data = buffer->system.addr;
               *pitch = buffer->system.pitch;
               break;
          case CSP_VIDEOLOW:
               /* no valid video instance
                  or read access and valid system? system lock! */
               if ((buffer->video.health != CSH_STORED ||
                    (flags & DSLF_READ && buffer->system.health == CSH_STORED))
                   && !buffer->video.locked)
               {
                    dfb_surfacemanager_assure_system( surface->manager, buffer );
                    buffer->system.locked++;
                    *data = buffer->system.addr;
                    *pitch = buffer->system.pitch;
                    if (flags & DSLF_WRITE &&
                        buffer->video.health == CSH_STORED)
                         buffer->video.health = CSH_RESTORE;
               }
               else {
                    /* ok, write only goes into video directly */
                    buffer->video.locked++;
                    *data = dfb_gfxcard_memory_virtual( buffer->video.offset );
                    *pitch = buffer->video.pitch;
                    if (flags & DSLF_WRITE)
                         buffer->system.health = CSH_RESTORE;
                    video_access_by_software( buffer, flags );
               }
               break;
          case CSP_VIDEOHIGH:
               /* no video instance yet? system lock! */
               if (buffer->video.health != CSH_STORED) {
                    /* no video health, no fetch */
                    buffer->system.locked++;
                    *data = buffer->system.addr;
                    *pitch = buffer->system.pitch;
                    break;
               }
               /* video lock! write access? restore system! */
               if (flags & DSLF_WRITE)
                    buffer->system.health = CSH_RESTORE;
               /* FALL THROUGH, for the rest we have to do a video lock
                  as if it had the policy CSP_VIDEOONLY */
          case CSP_VIDEOONLY:
               buffer->video.locked++;
               *data = dfb_gfxcard_memory_virtual( buffer->video.offset );
               *pitch = buffer->video.pitch;
               video_access_by_software( buffer, flags );
               break;
          default:
               BUG( "invalid surface policy" );

               if (front)
                    skirmish_dismiss( &surface->front_lock );
               else
                    skirmish_dismiss( &surface->back_lock );

               return DFB_BUG;
     }

     return DFB_OK;
}

DFBResult dfb_surface_hardware_lock( CoreSurface *surface,
                                     unsigned int flags, int front )
{
     SurfaceBuffer *buffer;

     if (!flags) {
          BUG( "lock without flags" );
          return DFB_INVARG;
     }

     if (front) {
          skirmish_prevail( &surface->front_lock );
          buffer = surface->front_buffer;
     }
     else {
          skirmish_prevail( &surface->back_lock );
          buffer = surface->back_buffer;
     }

     switch (buffer->policy) {
          case CSP_SYSTEMONLY:
               /* never ever! */
               break;

          case CSP_VIDEOHIGH:
          case CSP_VIDEOLOW:
               /* avoid inconsistency, could be optimized (read/write) */
               if (buffer->system.locked)
                    break;
               /* no reading? no force? no video instance? no success! ;-) */
               if (!(flags & (DSLF_READ|CSLF_FORCE)) && buffer->video.health != CSH_STORED)
                    break;
               if (dfb_surfacemanager_assure_video( surface->manager, buffer ))
                    break;
               if (flags & DSLF_WRITE)
                    buffer->system.health = CSH_RESTORE;
               /* fall through */

          case CSP_VIDEOONLY:
               buffer->video.locked++;
               video_access_by_hardware( buffer, flags );
               return DFB_OK;

          default:
               BUG( "invalid surface policy" );

               if (front)
                    skirmish_dismiss( &surface->front_lock );
               else
                    skirmish_dismiss( &surface->back_lock );

               return DFB_BUG;
     }

     if (front)
          skirmish_dismiss( &surface->front_lock );
     else
          skirmish_dismiss( &surface->back_lock );

     return DFB_FAILURE;
}

void dfb_surface_unlock( CoreSurface *surface, int front )
{
     if (front) {
          SurfaceBuffer *buffer = surface->front_buffer;

          if (buffer->system.locked)
               buffer->system.locked--;

          if (buffer->video.locked)
               buffer->video.locked--;

          skirmish_dismiss( &surface->front_lock );
     }
     else {
          SurfaceBuffer *buffer = surface->back_buffer;

          if (buffer->system.locked)
               buffer->system.locked--;

          if (buffer->video.locked)
               buffer->video.locked--;

          skirmish_dismiss( &surface->back_lock );
     }
}

void dfb_surface_destroy( CoreSurface *surface, bool unref )
{
     DEBUGMSG("DirectFB/core/surfaces: dfb_surface_destroy (%p) entered\n", surface);

     skirmish_prevail( &surface->lock );

     if (surface->destroyed) {
          skirmish_dismiss( &surface->lock );
          return;
     }

     surface->destroyed = true;
     
     /* anounce surface destruction */
     dfb_surface_notify_listeners( surface, CSNF_DESTROY );

     /* deallocate first buffer */
     dfb_surface_deallocate_buffer( surface, surface->front_buffer );

     /* deallocate second buffer if it's another one */
     if (surface->back_buffer != surface->front_buffer)
          dfb_surface_deallocate_buffer( surface, surface->back_buffer );

     /* destroy the locks */
     skirmish_destroy( &surface->front_lock );
     skirmish_destroy( &surface->back_lock );

     /* unlink palette */
     if (surface->palette) {
          dfb_palette_detach( surface->palette, palette_listener, surface );
          dfb_palette_unlink( surface->palette );
     }

     skirmish_dismiss( &surface->lock );
     
     /* unref surface object */
     if (unref)
          dfb_surface_unref( surface );
     
     DEBUGMSG("DirectFB/core/surfaces: dfb_surface_destroy (%p) exitting\n", surface);
}

DFBResult dfb_surface_init ( CoreSurface            *surface,
                             int                     width,
                             int                     height,
                             DFBSurfacePixelFormat   format,
                             DFBSurfaceCapabilities  caps,
                             CorePalette            *palette )
{
     switch (format) {
          case DSPF_A8:
          case DSPF_ARGB:
          case DSPF_I420:
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_RGB332:
          case DSPF_UYVY:
          case DSPF_YUY2:
          case DSPF_YV12:
          case DSPF_LUT8:
               break;

          default:
               BUG( "unknown pixel format" );
               return DFB_BUG;
     }

     surface->width  = width;
     surface->height = height;
     surface->format = format;
     surface->caps   = caps;

     if (caps & DSCAPS_STATIC_ALLOC) {
          surface->min_width  = width;
          surface->min_height = height;
     }

     if (palette) {
          dfb_surface_set_palette( surface, palette );
     }
     else if (DFB_PIXELFORMAT_IS_INDEXED( format )) {
          palette = dfb_palette_create( 256 );
          if (!palette)
               return DFB_FAILURE;

          dfb_surface_set_palette( surface, palette );

          dfb_palette_unref( palette );
     }
     
     skirmish_init( &surface->lock );
     
     skirmish_init( &surface->front_lock );
     skirmish_init( &surface->back_lock );

     if (format == DSPF_LUT8)
          dfb_palette_generate_rgb332_map( surface->palette );

     surface->manager = dfb_gfxcard_surface_manager();
     
     return DFB_OK;
}


/** internal **/

static DFBResult dfb_surface_allocate_buffer( CoreSurface        *surface,
                                              CoreSurfacePolicy   policy,
                                              SurfaceBuffer     **ret_buffer )
{
     SurfaceBuffer *buffer;

     DFB_ASSERT( surface != NULL );
     DFB_ASSERT( ret_buffer != NULL );

     /* Allocate buffer structure. */
     buffer = shcalloc( 1, sizeof(SurfaceBuffer) );

     buffer->policy  = policy;
     buffer->surface = surface;

     switch (policy) {
          case CSP_SYSTEMONLY:
          case CSP_VIDEOLOW:
          case CSP_VIDEOHIGH: {
               int   pitch;
               int   size;
               void *data;

               /* Calculate pitch. */
               pitch = DFB_BYTES_PER_LINE( surface->format,
                                           MAX( surface->width,
                                                surface->min_width ) );
               if (pitch & 3)
                    pitch += 4 - (pitch & 3);

               /* Calculate amount of data to allocate. */
               size = DFB_PLANE_MULTIPLY( surface->format,
                                          MAX( surface->height,
                                               surface->min_height ) * pitch );

               /* Allocate shared memory. */
               data = shmalloc( size );
               if (!data) {
                    shfree( buffer );
                    return DFB_NOSYSTEMMEMORY;
               }

               /* Write back values. */
               buffer->system.health = CSH_STORED;
               buffer->system.pitch  = pitch;
               buffer->system.addr   = data;

               break;
          }

          case CSP_VIDEOONLY: {
               DFBResult ret;

               /* Lock surface manager. */
               dfb_surfacemanager_lock( surface->manager );

               /* Allocate buffer in video memory. */
               ret = dfb_surfacemanager_allocate( surface->manager, buffer );

               /* Unlock surface manager. */
               dfb_surfacemanager_unlock( surface->manager );

               /* Check for successful allocation. */
               if (ret) {
                    shfree( buffer );
                    return ret;
               }

               /* Set from 'to be restored' to 'is stored'. */
               buffer->video.health = CSH_STORED;

               break;
          }
     }

     /* Return the new buffer. */
     *ret_buffer = buffer;

     return DFB_OK;
}

static DFBResult dfb_surface_reallocate_buffer( CoreSurface   *surface,
                                                SurfaceBuffer *buffer )
{
     DFBResult    ret;

     if (buffer->flags & SBF_FOREIGN_SYSTEM)
          return DFB_UNSUPPORTED;

     if (buffer->system.health) {
          int   pitch;
          int   size;
          void *data;

          /* Calculate pitch. */
          pitch = DFB_BYTES_PER_LINE( surface->format,
                                      MAX( surface->width,
                                           surface->min_width ) );
          if (pitch & 3)
               pitch += 4 - (pitch & 3);

          /* Calculate amount of data to allocate. */
          size = DFB_PLANE_MULTIPLY( surface->format,
                                     MAX( surface->height,
                                          surface->min_height ) * pitch );

          /* Allocate shared memory. */
          data = shmalloc( size );
          if (!data)
               return DFB_NOSYSTEMMEMORY;

          /* Free old memory. */
          shfree( buffer->system.addr );
          
          /* Write back new values. */
          buffer->system.health = CSH_STORED;
          buffer->system.pitch  = pitch;
          buffer->system.addr   = data;
     }

     if (buffer->video.health) {
          /* FIXME: better support video instance reallocation */
          dfb_surfacemanager_deallocate( surface->manager, buffer );
          ret = dfb_surfacemanager_allocate( surface->manager, buffer );

          if (ret) {
               if (!buffer->system.health)
                    CAUTION( "reallocation of video instance failed" );
               else {
                    buffer->system.health = CSH_STORED;
                    return DFB_OK;
               }

               return ret;
          }

          buffer->video.health = CSH_STORED;
     }

     return DFB_OK;
}

static void dfb_surface_deallocate_buffer( CoreSurface   *surface,
                                           SurfaceBuffer *buffer )
{
     if (buffer->system.health && !(buffer->flags & SBF_FOREIGN_SYSTEM))
          shfree( buffer->system.addr );

     dfb_surfacemanager_lock( surface->manager );

     if (buffer->video.health)
          dfb_surfacemanager_deallocate( surface->manager, buffer );

     dfb_surfacemanager_unlock( surface->manager );

     shfree( buffer );
}

static ReactionResult
palette_listener( const void *msg_data,
                  void       *ctx )
{
     CorePaletteNotification *notification = (CorePaletteNotification*)msg_data;
     CoreSurface             *surface      = (CoreSurface*) ctx;

     if (notification->flags & CPNF_DESTROY)
          surface->palette = NULL;

     dfb_surface_notify_listeners( surface, CSNF_PALETTE );

     if (notification->flags & CPNF_DESTROY)
          return RS_REMOVE;

     return RS_OK;
}

