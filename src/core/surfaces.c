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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <pthread.h>

#include <core/fusion/shmalloc.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>
#include <core/system.h>

#include <gfx/util.h>
#include <misc/util.h>
#include <misc/mem.h>
#include <misc/memcpy.h>

static DFBResult dfb_surface_allocate_buffer  ( CoreSurface        *surface,
                                                CoreSurfacePolicy   policy,
                                                SurfaceBuffer     **buffer );
static DFBResult dfb_surface_reallocate_buffer( CoreSurface        *surface,
                                                SurfaceBuffer      *buffer );
static void dfb_surface_deallocate_buffer     ( CoreSurface        *surface,
                                                SurfaceBuffer      *buffer );

static void video_access_by_hardware( SurfaceBuffer       *buffer,
                                      DFBSurfaceLockFlags  flags );
static void video_access_by_software( SurfaceBuffer       *buffer,
                                      DFBSurfaceLockFlags  flags );

static const React dfb_surface_globals[] = {
/* 0 */   _dfb_layer_surface_listener,
/* 1 */   _dfb_layer_background_image_listener,
          NULL
};

static void surface_destructor( FusionObject *object, bool zombie )
{
     CoreSurface *surface = (CoreSurface*) object;

     DEBUGMSG("DirectFB/core/surfaces: destroying %p (%dx%d%s)\n", surface,
              surface->width, surface->height, zombie ? " ZOMBIE" : "");

     /* announce surface destruction */
     dfb_surface_notify_listeners( surface, CSNF_DESTROY );

     /* deallocate first buffer */
     dfb_surface_deallocate_buffer( surface, surface->front_buffer );

     /* deallocate second buffer if it's another one */
     if (surface->back_buffer != surface->front_buffer)
          dfb_surface_deallocate_buffer( surface, surface->back_buffer );

     /* deallocate third buffer if it's another one */
     if (surface->idle_buffer != surface->front_buffer)
          dfb_surface_deallocate_buffer( surface, surface->idle_buffer );

     /* unlink palette */
     if (surface->palette) {
          dfb_palette_detach_global( surface->palette,
                                     &surface->palette_reaction );
          dfb_palette_unlink( surface->palette );
     }

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

     DFB_ASSERT( width > 0 );
     DFB_ASSERT( height > 0 );
     
     if (width * (long long) height > 4096*4096)
          return DFB_BUFFERTOOLARGE;

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

     if (caps & (DSCAPS_FLIPPING | DSCAPS_TRIPLE)) {
          ret = dfb_surface_allocate_buffer( s, policy, &s->back_buffer );
          if (ret) {
               dfb_surface_deallocate_buffer( s, s->front_buffer );

               fusion_object_destroy( &s->object );
               return ret;
          }
     }
     else
          s->back_buffer = s->front_buffer;

     if (caps & DSCAPS_TRIPLE) {
          ret = dfb_surface_allocate_buffer( s, policy, &s->idle_buffer );
          if (ret) {
               dfb_surface_deallocate_buffer( s, s->back_buffer );
               dfb_surface_deallocate_buffer( s, s->front_buffer );

               fusion_object_destroy( &s->object );
               return ret;
          }
     }
     else
          s->idle_buffer = s->front_buffer;

     fusion_object_activate( &s->object );
     
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

     DFB_ASSERT( width > 0 );
     DFB_ASSERT( height > 0 );
     
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


     s->front_buffer = (SurfaceBuffer *) SHCALLOC( 1, sizeof(SurfaceBuffer) );

     s->front_buffer->flags   = SBF_FOREIGN_SYSTEM | SBF_WRITTEN;
     s->front_buffer->policy  = policy;
     s->front_buffer->surface = s;

     s->front_buffer->system.health = CSH_STORED;
     s->front_buffer->system.pitch  = front_pitch;
     s->front_buffer->system.addr   = front_buffer;

     if (caps & DSCAPS_FLIPPING) {
          s->back_buffer = (SurfaceBuffer *) SHMALLOC( sizeof(SurfaceBuffer) );
          dfb_memcpy( s->back_buffer, s->front_buffer, sizeof(SurfaceBuffer) );

          s->back_buffer->system.pitch  = back_pitch;
          s->back_buffer->system.addr   = back_buffer;
     }
     else
          s->back_buffer = s->front_buffer;

     /* No triple buffering */
     s->idle_buffer = s->front_buffer;

     fusion_object_activate( &s->object );
     
     *surface = s;

     return DFB_OK;
}

FusionResult
dfb_surface_notify_listeners( CoreSurface                  *surface,
                              CoreSurfaceNotificationFlags  flags)
{
     CoreSurfaceNotification notification;

     notification.flags   = flags;
     notification.surface = surface;

     return dfb_surface_dispatch( surface, &notification, dfb_surface_globals );
}

DFBResult dfb_surface_reformat( CoreSurface *surface, int width, int height,
                                DFBSurfacePixelFormat format )
{
     int old_width, old_height;
     DFBSurfacePixelFormat old_format;
     DFBResult ret;

     DFB_ASSERT( width > 0 );
     DFB_ASSERT( height > 0 );

     if (width * (long long) height > 4096*4096)
          return DFB_BUFFERTOOLARGE;
     
     if (surface->front_buffer->flags & SBF_FOREIGN_SYSTEM ||
         surface->back_buffer->flags  & SBF_FOREIGN_SYSTEM)
     {
          return DFB_UNSUPPORTED;
     }

     old_width  = surface->width;
     old_height = surface->height;
     old_format = surface->format;

     surface->width  = width;
     surface->height = height;
     surface->format = format;
     
     if (width      <= surface->min_width &&
         old_width  <= surface->min_width &&
         height     <= surface->min_height &&
         old_height <= surface->min_height &&
         old_format == surface->format)
     {
          dfb_surface_notify_listeners( surface, CSNF_SIZEFORMAT );
          return DFB_OK;
     }

     dfb_surfacemanager_lock( surface->manager );

     ret = dfb_surface_reallocate_buffer( surface, surface->front_buffer );
     if (ret) {
          surface->width  = old_width;
          surface->height = old_height;
          surface->format = old_format;

          dfb_surfacemanager_unlock( surface->manager );
          return ret;
     }

     if (surface->caps & (DSCAPS_FLIPPING | DSCAPS_TRIPLE)) {
          ret = dfb_surface_reallocate_buffer( surface, surface->back_buffer );
          if (ret) {
               surface->width  = old_width;
               surface->height = old_height;
               surface->format = old_format;

               dfb_surface_reallocate_buffer( surface, surface->front_buffer );

               dfb_surfacemanager_unlock( surface->manager );
               return ret;
          }
     }

     if (surface->caps & DSCAPS_TRIPLE) {
          ret = dfb_surface_reallocate_buffer( surface, surface->idle_buffer );
          if (ret) {
               surface->width  = old_width;
               surface->height = old_height;
               surface->format = old_format;

               dfb_surface_reallocate_buffer( surface, surface->back_buffer );
               dfb_surface_reallocate_buffer( surface, surface->front_buffer );

               dfb_surfacemanager_unlock( surface->manager );
               return ret;
          }
     }

     if (DFB_PIXELFORMAT_IS_INDEXED( format ) && !surface->palette) {
          DFBResult    ret;
          CorePalette *palette;

          ret = dfb_palette_create( 1 << DFB_BITS_PER_PIXEL( format ),
                                    &palette );
          if (ret)
               return ret;

          dfb_surface_set_palette( surface, palette );

          dfb_palette_unref( palette );
     }

     dfb_surfacemanager_unlock( surface->manager );
     
     dfb_surface_notify_listeners( surface, CSNF_SIZEFORMAT |
                                   CSNF_SYSTEM | CSNF_VIDEO );

     return DFB_OK;
}

DFBResult dfb_surface_reconfig( CoreSurface       *surface,
                                CoreSurfacePolicy  front_policy,
                                CoreSurfacePolicy  back_policy ) 
{
     DFBResult      ret;
     SurfaceBuffer *old_front;
     SurfaceBuffer *old_back;
     SurfaceBuffer *old_idle;
     bool           new_front = surface->front_buffer->policy != front_policy;

     if (surface->front_buffer->flags & SBF_FOREIGN_SYSTEM ||
         surface->back_buffer->flags  & SBF_FOREIGN_SYSTEM)
     {
          return DFB_UNSUPPORTED;
     }

     dfb_surfacemanager_lock( surface->manager );

     old_front = surface->front_buffer;
     old_back = surface->back_buffer;
     old_idle = surface->idle_buffer;

     if (new_front) {
          ret = dfb_surface_allocate_buffer( surface, front_policy, &surface->front_buffer );
          if (ret) {
               dfb_surfacemanager_unlock( surface->manager );
               return ret;
          }
     }

     if (surface->caps & (DSCAPS_FLIPPING | DSCAPS_TRIPLE)) {
          ret = dfb_surface_allocate_buffer( surface, back_policy, &surface->back_buffer );
          if (ret) {
               if (new_front) {
                    dfb_surface_deallocate_buffer( surface, surface->front_buffer );
                    surface->front_buffer = old_front;
               }

               dfb_surfacemanager_unlock( surface->manager );
               return ret;
          }
     }
     else
          surface->back_buffer = surface->front_buffer;

     if (surface->caps & DSCAPS_TRIPLE) {
          ret = dfb_surface_allocate_buffer( surface, back_policy, &surface->idle_buffer );
          if (ret) {
               dfb_surface_deallocate_buffer( surface, surface->back_buffer );
               surface->back_buffer = old_back;

               if (new_front) {
                    dfb_surface_deallocate_buffer( surface, surface->front_buffer );
                    surface->front_buffer = old_front;
               }

               dfb_surfacemanager_unlock( surface->manager );
               return ret;
          }
     }
     else
          surface->idle_buffer = surface->front_buffer;

     if (new_front)
          dfb_surface_deallocate_buffer( surface, old_front );

     if (old_front != old_back)
          dfb_surface_deallocate_buffer ( surface, old_back );

     if (old_front != old_idle)
          dfb_surface_deallocate_buffer ( surface, old_idle );

     dfb_surfacemanager_unlock( surface->manager );
     
     dfb_surface_notify_listeners( surface, CSNF_SIZEFORMAT |
                                   CSNF_SYSTEM | CSNF_VIDEO );

     return DFB_OK;
}

DFBResult
dfb_surface_set_palette( CoreSurface *surface,
                         CorePalette *palette )
{
     DFB_ASSERT( surface != NULL );

     if (surface->palette == palette)
          return DFB_OK;

     if (surface->palette) {
          dfb_palette_detach_global( surface->palette, &surface->palette_reaction );
          dfb_palette_unlink( surface->palette );

          surface->palette = NULL;
     }

     if (palette) {
          dfb_palette_link( &surface->palette, palette );
          dfb_palette_attach_global( palette, DFB_SURFACE_PALETTE_LISTENER,
                                     surface, &surface->palette_reaction );
     }

     dfb_surface_notify_listeners( surface, CSNF_PALETTE_CHANGE );

     return DFB_OK;
}

void dfb_surface_flip_buffers( CoreSurface *surface )
{
     SurfaceBuffer *tmp;

     DFB_ASSERT( surface != NULL );
     
     DFB_ASSERT(surface->back_buffer->policy == surface->front_buffer->policy);

     dfb_surfacemanager_lock( surface->manager );

     if (surface->caps & DSCAPS_TRIPLE) {
          tmp = surface->front_buffer;
          surface->front_buffer = surface->back_buffer;
          surface->back_buffer = surface->idle_buffer;
          surface->idle_buffer = tmp;
     } else {
          tmp = surface->front_buffer;
          surface->front_buffer = surface->back_buffer;
          surface->back_buffer = tmp;

          /* To avoid problems with buffer deallocation */
          surface->idle_buffer = surface->front_buffer;
     }

     dfb_surfacemanager_unlock( surface->manager );

     dfb_surface_notify_listeners( surface, CSNF_FLIP );
}

void dfb_surface_set_field( CoreSurface *surface, int field )
{
     DFB_ASSERT( surface != NULL );

     surface->field = field;

     dfb_surface_notify_listeners( surface, CSNF_FIELD );
}

DFBResult dfb_surface_soft_lock( CoreSurface *surface, DFBSurfaceLockFlags flags,
                                 void **data, int *pitch, bool front )
{
     DFBResult ret;

     DFB_ASSERT( surface != NULL );
     DFB_ASSERT( data != NULL );
     DFB_ASSERT( pitch != NULL );
     
     dfb_surfacemanager_lock( surface->manager );
     ret = dfb_surface_software_lock( surface, flags, data, pitch, front );
     dfb_surfacemanager_unlock( surface->manager );

     return ret;
}

DFBResult dfb_surface_software_lock( CoreSurface *surface, DFBSurfaceLockFlags flags,
                                     void **data, int *pitch, bool front )
{
     SurfaceBuffer *buffer;

     DFB_ASSERT( surface != NULL );
     DFB_ASSERT( flags != 0 );
     DFB_ASSERT( data != NULL );
     DFB_ASSERT( pitch != NULL );

     buffer = front ? surface->front_buffer : surface->back_buffer;

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
                    *data = dfb_system_video_memory_virtual( buffer->video.offset );
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
               if (dfb_surfacemanager_assure_video( surface->manager, buffer ))
                    ONCE( "accessing video memory during suspend" );
               buffer->video.locked++;
               *data = dfb_system_video_memory_virtual( buffer->video.offset );
               *pitch = buffer->video.pitch;
               video_access_by_software( buffer, flags );
               break;
          default:
               BUG( "invalid surface policy" );
               return DFB_BUG;
     }

     if (flags & DSLF_WRITE)
          buffer->flags |= SBF_WRITTEN;

     return DFB_OK;
}

DFBResult dfb_surface_hardware_lock( CoreSurface *surface,
                                     unsigned int flags, bool front )
{
     SurfaceBuffer *buffer;

     DFB_ASSERT( surface != NULL );
     DFB_ASSERT( flags != 0 );

     buffer = front ? surface->front_buffer : surface->back_buffer;

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
               if (dfb_surfacemanager_assure_video( surface->manager, buffer ))
                    break;
               
               buffer->video.locked++;
               
               video_access_by_hardware( buffer, flags );
               
               if (flags & DSLF_WRITE)
                    buffer->flags |= SBF_WRITTEN;
               
               return DFB_OK;

          default:
               BUG( "invalid surface policy" );
               return DFB_BUG;
     }

     return DFB_FAILURE;
}

void dfb_surface_unlock( CoreSurface *surface, int front )
{
     SurfaceBuffer *buffer;
     
     DFB_ASSERT( surface != NULL );
     
     buffer = front ? surface->front_buffer : surface->back_buffer;
     
     DFB_ASSERT( buffer != NULL );
     
     if (buffer->system.locked)
          buffer->system.locked--;

     if (buffer->video.locked)
          buffer->video.locked--;
}

DFBResult dfb_surface_init ( CoreSurface            *surface,
                             int                     width,
                             int                     height,
                             DFBSurfacePixelFormat   format,
                             DFBSurfaceCapabilities  caps,
                             CorePalette            *palette )
{
     DFB_ASSERT( surface != NULL );
     
     switch (format) {
          case DSPF_A8:
          case DSPF_ARGB:
          case DSPF_ARGB1555:
          case DSPF_I420:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_RGB332:
          case DSPF_UYVY:
          case DSPF_YUY2:
          case DSPF_YV12:
          case DSPF_LUT8:
          case DSPF_ALUT44:
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
          DFBResult    ret;
          CorePalette *palette;

          ret = dfb_palette_create( 1 << DFB_BITS_PER_PIXEL( format ),
                                    &palette );
          if (ret)
               return ret;

          if (format == DSPF_LUT8)
               dfb_palette_generate_rgb332_map( palette );
          else if (format == DSPF_ALUT44)
               dfb_palette_generate_rgb121_map( palette );

          dfb_surface_set_palette( surface, palette );

          dfb_palette_unref( palette );
     }
     
     surface->manager = dfb_gfxcard_surface_manager();
     
     return DFB_OK;
}

DFBResult dfb_surface_dump( CoreSurface *surface,
                            const char  *directory,
                            const char  *prefix )
{
     DFBResult  ret;
     int        num = -1;
     int        fd_p, fd_g = -1, i, n;
     int        len = strlen(directory) + strlen(prefix) + 11;
     char       filename[len];
     char       head[30];
     void      *data;
     int        pitch;
     bool       alpha = false;

     DFB_ASSERT( surface != NULL );
     DFB_ASSERT( directory != NULL );
     DFB_ASSERT( prefix != NULL );

     /* Check pixel format. */
     switch (surface->format) {
          case DSPF_ARGB:
          case DSPF_ARGB1555:
               alpha = true;

               /* fall through */

          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
               break;

          default:
               ERRORMSG( "DirectFB/core/surfaces: surface dump for format "
                         "0x%08x is not implemented!\n", surface->format );
               return DFB_UNSUPPORTED;
     }
     
     /* Lock the surface, get the data pointer and pitch. */
     ret = dfb_surface_soft_lock( surface, DSLF_READ, &data, &pitch, true );
     if (ret)
          return ret;

     /* Create a file with the lowest unused pixmap index. */
     do {
          snprintf( filename, len, "%s/%s_%04d.ppm", directory, prefix, ++num );

          errno = 0;

          fd_p = open( filename, O_EXCL | O_CREAT | O_WRONLY, 0644 );
          if (fd_p < 0 && errno != EEXIST) {
               PERRORMSG("DirectFB/core/input: "
                         "could not open %s!\n", filename);

               dfb_surface_unlock( surface, true );
               
               return DFB_IO;
          }
     } while (errno == EEXIST);

     /* Create a graymap for the alpha channel using the same index. */
     if (alpha) {
          snprintf( filename, len, "%s/%s_%04d.pgm", directory, prefix, num );
          
          fd_g = open( filename, O_EXCL | O_CREAT | O_WRONLY, 0644 );
          if (fd_g < 0) {
               PERRORMSG("DirectFB/core/input: "
                         "could not open %s!\n", filename);

               dfb_surface_unlock( surface, true );
               
               close( fd_p );

               snprintf( filename, len, "%s/%s_%04d.ppm",
                         directory, prefix, num );
               unlink( filename );

               return DFB_IO;
          }
     }
     
     /* Write the pixmap header. */
     snprintf( head, 30,
               "P6\n%d %d\n255\n", surface->width, surface->height );
     write( fd_p, head, strlen(head) );

     /* Write the graymap header. */
     if (alpha) {
          snprintf( head, 30,
                    "P5\n%d %d\n255\n", surface->width, surface->height );
          write( fd_g, head, strlen(head) );
     }

     /* Write the pixmap (and graymap) data. */
     for (i=0; i<surface->height; i++) {
          int    n3;
          __u8  *data8  = data;
          __u16 *data16 = data;
          __u32 *data32 = data;

          __u8 buf_p[surface->width * 3];
          __u8 buf_g[surface->width];
          
          /* Prepare one row. */
          switch (surface->format) {
               case DSPF_ARGB:
                    for (n=0, n3=0; n<surface->width; n++, n3+=3) {
                         buf_p[n3+0] = (data32[n] & 0xFF0000) >> 16;
                         buf_p[n3+1] = (data32[n] & 0x00FF00) >>  8;
                         buf_p[n3+2] = (data32[n] & 0x0000FF);
                         
                         buf_g[n] = data32[n] >> 24;
                    }
                    break;
               case DSPF_ARGB1555:
                    for (n=0, n3=0; n<surface->width; n++, n3+=3) {
                         buf_p[n3+0] = (data16[n] & 0x7C00) >> 7;
                         buf_p[n3+1] = (data16[n] & 0x03E0) >> 2;
                         buf_p[n3+2] = (data16[n] & 0x001F) << 3;
                         
                         buf_g[n] = (data16[n] & 0x8000) ? 0xff : 0x00;
                    }
                    break;
               case DSPF_RGB16:
                    for (n=0, n3=0; n<surface->width; n++, n3+=3) {
                         buf_p[n3+0] = (data16[n] & 0xF800) >> 8;
                         buf_p[n3+1] = (data16[n] & 0x07E0) >> 3;
                         buf_p[n3+2] = (data16[n] & 0x001F) << 3;
                    }
                    break;
               case DSPF_RGB24:
                    for (n=0, n3=0; n<surface->width; n++, n3+=3) {
                         buf_p[n3+0] = data8[n3+2];
                         buf_p[n3+1] = data8[n3+1];
                         buf_p[n3+2] = data8[n3+0];
                    }
                    break;
               case DSPF_RGB32:
                    for (n=0, n3=0; n<surface->width; n++, n3+=3) {
                         buf_p[n3+0] = (data32[n] & 0xFF0000) >> 16;
                         buf_p[n3+1] = (data32[n] & 0x00FF00) >>  8;
                         buf_p[n3+2] = (data32[n] & 0x0000FF);
                    }
                    break;
               default:
                    BUG( "unexpected pixelformat" );
                    break;
          }

          /* Write color buffer to pixmap file. */
          write( fd_p, buf_p, surface->width * 3 );

          /* Write alpha buffer to graymap file. */
          if (alpha)
               write( fd_g, buf_g, surface->width );

          data += pitch;
     }

     /* Unlock the surface. */
     dfb_surface_unlock( surface, true );

     /* Close pixmap file. */
     close( fd_p );

     /* Close graymap file. */
     if (alpha)
          close( fd_g );

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
     buffer = SHCALLOC( 1, sizeof(SurfaceBuffer) );

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
               data = SHMALLOC( size );
               if (!data) {
                    SHFREE( buffer );
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
                    SHFREE( buffer );
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
          data = SHMALLOC( size );
          if (!data)
               return DFB_NOSYSTEMMEMORY;

          /* Free old memory. */
          SHFREE( buffer->system.addr );
          
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
          SHFREE( buffer->system.addr );

     dfb_surfacemanager_lock( surface->manager );

     if (buffer->video.health)
          dfb_surfacemanager_deallocate( surface->manager, buffer );

     dfb_surfacemanager_unlock( surface->manager );

     SHFREE( buffer );
}

ReactionResult
_dfb_surface_palette_listener( const void *msg_data,
                               void       *ctx )
{
     const CorePaletteNotification *notification = msg_data;
     CoreSurface                   *surface      = ctx;

     if (notification->flags & CPNF_DESTROY)
          return RS_REMOVE;
     
     if (notification->flags & CPNF_ENTRIES)
          dfb_surface_notify_listeners( surface, CSNF_PALETTE_UPDATE );

     return RS_OK;
}

/* internal functions needed to avoid side effects */

static void video_access_by_hardware( SurfaceBuffer       *buffer,
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

static void video_access_by_software( SurfaceBuffer       *buffer,
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

