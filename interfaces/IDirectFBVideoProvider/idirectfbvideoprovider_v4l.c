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

#if defined(__dietlibc__) && !defined(_BSD_SOURCE)
#define _BSD_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <malloc.h>
#include <string.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>

#include <pthread.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <media/idirectfbvideoprovider.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>

#include <display/idirectfbsurface.h>

#include <misc/util.h>
#include <misc/mem.h>
#include <misc/memcpy.h>

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           const char             *filename );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, V4L )

/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
     int                      ref;       /* reference counter */

     char                    *filename;
     int                      fd;
     struct video_capability  vcap;
     struct video_mmap        vmmap;
     struct video_mbuf        vmbuf;
     void                    *buffer;
     pthread_t                thread;
     CoreSurface             *destination;
     DVFrameCallback          callback;
     void                    *ctx;

     CoreCleanup             *cleanup;

     int                      grab_mode;
} IDirectFBVideoProvider_V4L_data;

static const unsigned int zero = 0;
static const unsigned int one = 1;

static void* OverlayThread( void *context );
static void* GrabThread( void *context );
static ReactionResult v4l_videosurface_listener( const void *msg_data, void *ctx );
static ReactionResult v4l_systemsurface_listener( const void *msg_data, void *ctx );
static DFBResult v4l_to_surface_overlay( CoreSurface *surface, DFBRectangle *rect,
                                 IDirectFBVideoProvider_V4L_data *data );
static DFBResult v4l_to_surface_grab( CoreSurface *surface, DFBRectangle *rect,
                                 IDirectFBVideoProvider_V4L_data *data );
static DFBResult v4l_stop( IDirectFBVideoProvider_V4L_data *data );
static void v4l_deinit( IDirectFBVideoProvider_V4L_data *data );
static void v4l_cleanup( void *data, int emergency );


static void IDirectFBVideoProvider_V4L_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_V4L_data *data =
          (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     v4l_deinit( data );

     if (data->cleanup)
          dfb_core_cleanup_remove( data->cleanup );

     DFBFREE( data->filename );

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult IDirectFBVideoProvider_V4L_AddRef( IDirectFBVideoProvider *thiz )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_Release( IDirectFBVideoProvider *thiz )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (--data->ref == 0) {
          IDirectFBVideoProvider_V4L_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_GetCapabilities (
                                           IDirectFBVideoProvider       *thiz,
                                           DFBVideoProviderCapabilities *caps )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!caps)
          return DFB_INVARG;

     *caps = ( DVCAPS_BASIC      |
               DVCAPS_BRIGHTNESS |
               DVCAPS_CONTRAST   |
               DVCAPS_HUE        |
               DVCAPS_SATURATION |
               DVCAPS_INTERLACED );

     if (data->vcap.type & VID_TYPE_SCALES)
          *caps |= DVCAPS_SCALE;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_GetSurfaceDescription(
                                                 IDirectFBVideoProvider *thiz,
                                                 DFBSurfaceDescription  *desc )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz || !desc)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     desc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->width  = 768;
     desc->height = 576;
     desc->pixelformat = dfb_primary_layer_pixelformat();

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_PlayTo(
                                            IDirectFBVideoProvider *thiz,
                                            IDirectFBSurface       *destination,
                                            const DFBRectangle     *dstrect,
                                            DVFrameCallback         callback,
                                            void                   *ctx )
{
     DFBRectangle           rect;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *surface = 0;
     DFBResult              ret;

     INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!destination)
          return DFB_INVARG;

     dst_data = (IDirectFBSurface_data*)destination->priv;

     if (!dst_data)
          return DFB_DEAD;

     if (!dst_data->area.current.w || !dst_data->area.current.h)
          return DFB_INVAREA;

     if (dstrect) {
          if (dstrect->w < 1  ||  dstrect->h < 1)
               return DFB_INVARG;

          rect = *dstrect;

          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;
     }
     else
          rect = dst_data->area.wanted;

     if (!dfb_rectangle_intersect( &rect, &dst_data->area.current ))
          return DFB_INVAREA;

     v4l_stop( data );
     
     data->callback = callback;
     data->ctx      = ctx;

     surface = dst_data->surface;

     data->grab_mode = 0;
     if (dst_data->caps & DSCAPS_SYSTEMONLY
	 || surface->caps & DSCAPS_FLIPPING
	 || !(VID_TYPE_OVERLAY & data->vcap.type))
          data->grab_mode = 1;

     if (!(dst_data->caps & DSCAPS_SYSTEMONLY)) {
          dfb_surfacemanager_lock( surface->manager );

	  ret = dfb_surfacemanager_assure_video( surface->manager, 
						 surface->back_buffer );

	  /*
	   * Because we're constantly writing to the surface we
	   * permanently lock it.
	   */
	  if (DFB_OK == ret && !data->grab_mode)
	       surface->back_buffer->video.locked++;

	  dfb_surfacemanager_unlock( surface->manager );

	  if (ret)
	       return ret;
     }

     if (data->grab_mode)
          ret = v4l_to_surface_grab( surface, &rect, data );
     else
          ret = v4l_to_surface_overlay( surface, &rect, data );

     if (DFB_OK != ret && !data->grab_mode)
          surface->back_buffer->video.locked--;

     return ret;
}

static DFBResult IDirectFBVideoProvider_V4L_Stop(
                                                 IDirectFBVideoProvider *thiz )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     return v4l_stop( data );
}

static DFBResult IDirectFBVideoProvider_V4L_SeekTo(
                                              IDirectFBVideoProvider *thiz,
                                              double                  seconds )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_V4L_GetPos(
                                              IDirectFBVideoProvider *thiz,
                                              double                 *seconds )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!seconds)
        return DFB_INVARG;

     *seconds = 0.0;

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_V4L_GetLength(
                                              IDirectFBVideoProvider *thiz,
                                              double                 *seconds )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!seconds)
        return DFB_INVARG;

     *seconds = 0.0;

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_V4L_GetColorAdjustment(
                                                  IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     struct video_picture pic;

     INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!adj)
        return DFB_INVARG;

     ioctl( data->fd, VIDIOCGPICT, &pic );

     adj->flags =
          DCAF_BRIGHTNESS | DCAF_CONTRAST | DCAF_HUE | DCAF_SATURATION;
     adj->brightness = pic.brightness;
     adj->contrast   = pic.contrast;
     adj->hue        = pic.hue;
     adj->saturation = pic.colour;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_SetColorAdjustment(
     IDirectFBVideoProvider *thiz,
     DFBColorAdjustment     *adj )
{
     struct video_picture pic;

     INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!adj)
        return DFB_INVARG;

     if (adj->flags == DCAF_NONE)
          return DFB_OK;

     if (ioctl( data->fd, VIDIOCGPICT, &pic ) < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/v4l: VIDIOCGPICT failed!\n" );

          return ret;
     }

     if (adj->flags & DCAF_BRIGHTNESS) pic.brightness = adj->brightness;
     if (adj->flags & DCAF_CONTRAST)   pic.contrast   = adj->contrast;
     if (adj->flags & DCAF_HUE)        pic.hue        = adj->hue;
     if (adj->flags & DCAF_SATURATION) pic.colour     = adj->saturation;

     if (ioctl( data->fd, VIDIOCSPICT, &pic ) < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/v4l: VIDIOCSPICT failed!\n" );

          return ret;
     }

     return DFB_OK;
}


/* exported symbols */

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     if (strncmp( ctx->filename, "/dev/video", 10 ) == 0)
          return DFB_OK;

     if (strncmp( ctx->filename, "/dev/v4l/video", 14 ) == 0)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
     int fd;
     
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBVideoProvider_V4L)

     data->ref = 1;
     
     fd = open( filename, O_RDWR );
     if (fd < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/v4l: Cannot open `%s'!\n", filename );

          DFB_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ioctl( fd, VIDIOCGCAP, &data->vcap );
     ioctl( fd, VIDIOCCAPTURE, &zero );

     ioctl( fd, VIDIOCGMBUF, &data->vmbuf );
     data->buffer = mmap( NULL, data->vmbuf.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );

     data->filename = DFBSTRDUP( filename );
     data->fd = fd;
     data->thread = -1;

     thiz->AddRef    = IDirectFBVideoProvider_V4L_AddRef;
     thiz->Release   = IDirectFBVideoProvider_V4L_Release;
     thiz->GetCapabilities = IDirectFBVideoProvider_V4L_GetCapabilities;
     thiz->GetSurfaceDescription =
          IDirectFBVideoProvider_V4L_GetSurfaceDescription;
     thiz->PlayTo    = IDirectFBVideoProvider_V4L_PlayTo;
     thiz->Stop      = IDirectFBVideoProvider_V4L_Stop;
     thiz->SeekTo    = IDirectFBVideoProvider_V4L_SeekTo;
     thiz->GetPos    = IDirectFBVideoProvider_V4L_GetPos;
     thiz->GetLength = IDirectFBVideoProvider_V4L_GetLength;
     thiz->GetColorAdjustment = IDirectFBVideoProvider_V4L_GetColorAdjustment;
     thiz->SetColorAdjustment = IDirectFBVideoProvider_V4L_SetColorAdjustment;

     return DFB_OK;
}


/*****************/

/*
 * bogus thread to generate callback,
 * because video4linux does not support syncing in overlay mode
 */
static void* OverlayThread( void *ctx )
{
     IDirectFBVideoProvider_V4L_data *data =
          (IDirectFBVideoProvider_V4L_data*)ctx;

     int field = 0;
     struct timeval tv;

     while (1) {
          tv.tv_sec = 0;
          tv.tv_usec = 20000;
          select( 0, 0, 0, 0, &tv );

          pthread_testcancel();

          if (data->destination->caps & DSCAPS_INTERLACED) {
              dfb_surface_notify_listeners( data->destination,
                                            field ? CSNF_SET_ODD : CSNF_SET_EVEN );
              field = !field;
          }

          if (data->callback)
              data->callback( data->ctx );
     }

     return NULL;
}

/*
 * thread to capture data from v4l buffers and generate callback
 */
static void* GrabThread( void *ctx )
{
     IDirectFBVideoProvider_V4L_data *data =
          (IDirectFBVideoProvider_V4L_data*)ctx;
     CoreSurface *surface = data->destination;
     __u8 *src, *dst;
     int dst_pitch, src_pitch, h;
     int frame = 0;

     src_pitch = DFB_BYTES_PER_LINE( surface->format, surface->width );

     while (frame < data->vmbuf.frames) {
          data->vmmap.frame = frame;
          ioctl( data->fd, VIDIOCMCAPTURE, &data->vmmap );
          frame++;
     }

     frame = 0;
     while (1) {
          ioctl( data->fd, VIDIOCSYNC, &frame );

          pthread_testcancel();

          h = surface->height;
          src = (__u8 *) data->buffer + data->vmbuf.offsets[frame];
          dfb_surface_soft_lock( surface, DSLF_WRITE, (void**)&dst, &dst_pitch, 0 );
          while (h--) {
               dfb_memcpy( dst, src, src_pitch );
               dst += dst_pitch;
               src += src_pitch;
          }
          if (surface->format == DSPF_I420) {
               h = surface->height / 2;
               while (h--) {
                    dfb_memcpy( dst, src, src_pitch );
                    dst += dst_pitch;
                    src += src_pitch;
               }
          } else if (surface->format == DSPF_YV12) {
               h = surface->height / 4;
               src += h * src_pitch;
               while (h--) {
                    dfb_memcpy( dst, src, src_pitch );
                    dst += dst_pitch;
                    src += src_pitch;
               }
               h = surface->height / 4;
               src -=  2 * h * src_pitch;
               while (h--) {
                    dfb_memcpy( dst, src, src_pitch );
                    dst += dst_pitch;
                    src += src_pitch;
               }
          }
          dfb_surface_unlock( surface, 0 );

          data->vmmap.frame = frame;
          ioctl( data->fd, VIDIOCMCAPTURE, &data->vmmap );

          if (data->callback)
               data->callback(data->ctx);

          if (++frame == data->vmbuf.frames)
               frame = 0;
     }

     return NULL;
}

static ReactionResult v4l_videosurface_listener( const void *msg_data, void *ctx )
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*) msg_data;
     IDirectFBVideoProvider_V4L_data *data = (IDirectFBVideoProvider_V4L_data*) ctx;
     
/*     if ((notification->flags & (CSNF_DESTROY | CSNF_SIZEFORMAT))) {
          v4l_stop( data );
          return RS_REMOVE;
     }*/
     CoreSurface *surface = data->destination;

     if (notification->flags & CSNF_VIDEO) {
          if (surface && surface->back_buffer->video.health == CSH_INVALID) {
              v4l_stop( data );
              return RS_REMOVE;
          }
     }

     return RS_OK;
}

static ReactionResult v4l_systemsurface_listener( const void *msg_data, void *ctx )
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*) msg_data;
     IDirectFBVideoProvider_V4L_data *data = (IDirectFBVideoProvider_V4L_data*) ctx;
     
/*     if ((notification->flags & (CSNF_DESTROY | CSNF_SIZEFORMAT))) {
          v4l_stop( data );
          return RS_REMOVE;
     }*/
     CoreSurface *surface = data->destination;

     if (notification->flags & CSNF_SYSTEM) {
          if (surface && surface->back_buffer->system.health == CSH_INVALID) {
               v4l_stop( data );
               return RS_REMOVE;
          }
     }

     return RS_OK;
}


/************/

static DFBResult v4l_to_surface_overlay( CoreSurface *surface, DFBRectangle *rect,
					 IDirectFBVideoProvider_V4L_data *data )
{
     int bpp, palette;
     SurfaceBuffer *buffer = surface->back_buffer;

     /*
      * Sanity check. Overlay to system surface isn't possible.
      */
     if (surface->caps & DSCAPS_SYSTEMONLY)
          return DFB_UNSUPPORTED;
     
     switch (surface->format) {
          case DSPF_YUY2:
               bpp = 16;
               palette = VIDEO_PALETTE_YUYV;
               break;
          case DSPF_UYVY:
               bpp = 16;
               palette = VIDEO_PALETTE_UYVY;
               break;
          case DSPF_I420:
               bpp = 8;
               palette = VIDEO_PALETTE_YUV420P;
               break;
          case DSPF_RGB15:
               bpp = 15;
               palette = VIDEO_PALETTE_RGB555;
               break;
          case DSPF_RGB16:
               bpp = 16;
               palette = VIDEO_PALETTE_RGB565;
               break;
          case DSPF_RGB24:
               bpp = 24;
               palette = VIDEO_PALETTE_RGB24;
               break;
          case DSPF_ARGB:
          case DSPF_RGB32:
               bpp = 32;
               palette = VIDEO_PALETTE_RGB32;
               break;
          default:
               BUG( "unknown pixel format" );
               return DFB_BUG;
     }

     {
          struct video_buffer b;

          b.base = (void*)dfb_gfxcard_memory_physical( buffer->video.offset );
          b.width = surface->width;
          b.height = surface->height;
          b.depth = bpp;
          b.bytesperline = buffer->video.pitch;

          if (ioctl( data->fd, VIDIOCSFBUF, &b ) < 0) {
               DFBResult ret = errno2dfb( errno );

               PERRORMSG( "DirectFB/v4l: "
                          "VIDIOCSFBUF failed, must run being root!\n" );

               return ret;
          }
     }
     {
          struct video_picture p;

          if (ioctl( data->fd, VIDIOCGPICT, &p ) < 0) {
               DFBResult ret = errno2dfb( errno );

               PERRORMSG( "DirectFB/v4l: VIDIOCGPICT failed!\n" );

               return ret;
          }

          p.depth = bpp;
          p.palette = palette;

          if (ioctl( data->fd, VIDIOCSPICT, &p ) < 0) {
               DFBResult ret = errno2dfb( errno );

               PERRORMSG( "DirectFB/v4l: VIDIOCSPICT failed!\n" );

               return ret;
          }
     }
     {
          struct video_window win;

          win.width = rect->w;
          win.height = rect->h;
          win.x = rect->x;
          win.y = rect->y;
          win.flags = 0;
          win.clips = NULL;
          win.clipcount = 0;
          win.chromakey = 0;

          if (ioctl( data->fd, VIDIOCSWIN, &win ) < 0) {
               DFBResult ret = errno2dfb( errno );

               PERRORMSG( "DirectFB/v4l: VIDIOCSWIN failed!\n" );

               return ret;
          }
     }

     if (!data->cleanup)
          data->cleanup = dfb_core_cleanup_add( v4l_cleanup, data, true );

     if (ioctl( data->fd, VIDIOCCAPTURE, &one ) < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/v4l: "
                     "Could not start capturing (VIDIOCCAPTURE failed)!\n" );

          return ret;
     }

     data->destination = surface;

     dfb_surface_attach( surface, v4l_videosurface_listener, data );

     if (data->callback || surface->caps & DSCAPS_INTERLACED)
          pthread_create( &data->thread, NULL, OverlayThread, data );

     return DFB_OK;
}

static DFBResult v4l_to_surface_grab( CoreSurface *surface, DFBRectangle *rect,
                                       IDirectFBVideoProvider_V4L_data *data )
{
     int bpp, palette;

     if (!data->vmbuf.frames)
          return DFB_UNSUPPORTED;

     switch (surface->format) {
          case DSPF_YUY2:
               bpp = 16;
               palette = VIDEO_PALETTE_YUYV;
               break;
          case DSPF_UYVY:
               bpp = 16;
               palette = VIDEO_PALETTE_UYVY;
               break;
          case DSPF_I420:
               bpp = 8;
               palette = VIDEO_PALETTE_YUV420P;
               break;
          case DSPF_YV12:
               bpp = 8;
               palette = VIDEO_PALETTE_YUV420P;
               break;
          case DSPF_RGB15:
               bpp = 15;
               palette = VIDEO_PALETTE_RGB555;
               break;
          case DSPF_RGB16:
               bpp = 16;
               palette = VIDEO_PALETTE_RGB565;
               break;
          case DSPF_RGB24:
               bpp = 24;
               palette = VIDEO_PALETTE_RGB24;
               break;
          case DSPF_ARGB:
          case DSPF_RGB32:
               bpp = 32;
               palette = VIDEO_PALETTE_RGB32;
               break;
          default:
               BUG( "unknown pixel format" );
               return DFB_BUG;
     }

     data->vmmap.width = surface->width;
     data->vmmap.height = surface->height;
     data->vmmap.format = palette;
     data->vmmap.frame = 0;
     if (ioctl(data->fd, VIDIOCMCAPTURE, &data->vmmap) < 0) {
          DFBResult ret = errno2dfb(errno);

          PERRORMSG("DirectFB/v4l: "
                    "Could not start capturing (VIDIOCMCAPTURE failed)!\n");
	  
          return ret;
     }

     if (!data->cleanup)
          data->cleanup = dfb_core_cleanup_add( v4l_cleanup, data, true );

     data->destination = surface;

     dfb_surface_attach( surface, v4l_systemsurface_listener, data );

     pthread_create( &data->thread, NULL, GrabThread, data );

     return DFB_OK;
}

static DFBResult v4l_stop( IDirectFBVideoProvider_V4L_data *data )
{
     if (data->thread != -1) {
          pthread_cancel( data->thread );
          pthread_join( data->thread, NULL );
          data->thread = -1;
     }

     if (VID_TYPE_OVERLAY & data->vcap.type) {
          if (ioctl( data->fd, VIDIOCCAPTURE, &zero ) < 0) {
	       DFBResult ret = errno2dfb( errno );
 
	       PERRORMSG( "DirectFB/v4l: "
			  "Could not stop capturing (VIDIOCCAPTURE failed)!\n" );
 
	       return ret;
	  }
     }

     if (!data->destination)
          return DFB_OK;

     if (!data->grab_mode)
          data->destination->back_buffer->video.locked--;

     data->destination = NULL;

     return DFB_OK;
}

static void v4l_deinit( IDirectFBVideoProvider_V4L_data *data )
{
     if (data->fd == -1) {
          BUG( "v4l_deinit with 'fd == -1'" );
          return;
     }

     v4l_stop( data );

     munmap( data->buffer, data->vmbuf.size );
     close( data->fd );
     data->fd = -1;
}

static void v4l_cleanup( void *ctx, int emergency )
{
     IDirectFBVideoProvider_V4L_data *data =
          (IDirectFBVideoProvider_V4L_data*)ctx;

     if (emergency)
          v4l_stop( data );
     else
          v4l_deinit( data );
}
