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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <malloc.h>
#include <string.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>

#include <pthread.h>

#include <directfb.h>

#include <misc/util.h>

#include <core/coredefs.h>
#include <core/layers.h>
#include <core/gfxcard.h>
#include <core/surfacemanager.h>

#include <display/idirectfbsurface.h>

/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
     int                      ref;       /* reference counter */

     int                      fd;
     struct video_capability  vcap;
     pthread_t                thread;
     DVFrameCallback          callback;
     void                    *ctx;
} IDirectFBVideoProvider_V4L_data;

static const unsigned int zero = 0;
static const unsigned int one = 1;

static void* FrameThread( void *context );
static SLResult v4l_surface_listener( CoreSurface *surface,
                                      unsigned int flags, void *ctx );
static DFBResult v4l_to_surface( CoreSurface *surface, DFBRectangle *rect,
                                 IDirectFBVideoProvider_V4L_data *data );
static DFBResult v4l_stop( IDirectFBVideoProvider_V4L_data *data );
static void v4l_deinit( IDirectFBVideoProvider_V4L_data *data );


static void IDirectFBVideoProvider_V4L_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_V4L_data *data =
          (IDirectFBVideoProvider_V4L_data*)thiz->priv;
     
     v4l_deinit( data );

     free( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

static DFBResult IDirectFBVideoProvider_V4L_AddRef( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_Release( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBVideoProvider_V4L_Destruct( thiz );
     }

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

     memset( desc, 0, sizeof(DFBSurfaceDescription) );
     desc->flags = (DFBSurfaceDescriptionFlags)
                            (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);
     desc->width = 768;
     desc->height = 576;
     desc->pixelformat = layers->surface->format;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_PlayTo(IDirectFBVideoProvider *thiz,
                                            IDirectFBSurface       *destination,
                                            DFBRectangle           *dstrect,
                                            DVFrameCallback         callback,
                                            void                   *ctx)
{
     DFBRectangle rect;

     IDirectFBVideoProvider_V4L_data *data;
     IDirectFBSurface_data           *dst_data;

     if (!thiz || !destination)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;
     dst_data = (IDirectFBSurface_data*)destination->priv;

     if (!data || !dst_data)
          return DFB_DEAD;

     if (dstrect) {
          if (dstrect->w < 1  ||  dstrect->h < 1)
               return DFB_INVARG;

          rect = *dstrect;

          rect.x += dst_data->req_rect.x;
          rect.y += dst_data->req_rect.y;
     }
     else
          rect = dst_data->req_rect;

     if (!rectangle_intersect( &rect, &dst_data->clip_rect ))
          return DFB_INVARG;

     data->callback = callback;
     data->ctx      = ctx;

     return v4l_to_surface( dst_data->surface, &rect, data );
}

static DFBResult IDirectFBVideoProvider_V4L_Stop( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return v4l_stop( data );
}

static DFBResult IDirectFBVideoProvider_V4L_SeekTo(
                                              IDirectFBVideoProvider *thiz,
                                              double                  seconds )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_V4L_GetPos(
     IDirectFBVideoProvider *thiz,
     double                 *seconds )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     *seconds = 0.0;

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_V4L_GetLength(
     IDirectFBVideoProvider *thiz,
     double                 *seconds )
{
     IDirectFBVideoProvider_V4L_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     *seconds = 0.0;

     return DFB_UNIMPLEMENTED;
}


/* exported symbols */

const char *get_type()
{
     return "IDirectFBVideoProvider";
}

const char *get_implementation()
{
     return "V4L";
}

DFBResult Probe( const char *filename )
{
     if (strncmp( filename, "/dev/video", 10 ) == 0)
          return DFB_OK;

     if (strncmp( filename, "/dev/v4l/video", 14 ) == 0)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

DFBResult Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
     int fd;
     IDirectFBVideoProvider_V4L_data *data;

     fd = open( filename, O_RDWR );
     if (fd < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/v4l: Cannot open `%s'!\n", filename );

          return ret;
     }

     data = (IDirectFBVideoProvider_V4L_data*)
                         malloc( sizeof(IDirectFBVideoProvider_V4L_data) );
     memset( data, 0, sizeof(IDirectFBVideoProvider_V4L_data) );
     thiz->priv = data;

     data->ref = 1;

     ioctl( fd, VIDIOCGCAP, &data->vcap );
     ioctl( fd, VIDIOCCAPTURE, &zero );

     data->fd = fd;
     data->thread = -1;
     
     thiz->AddRef = IDirectFBVideoProvider_V4L_AddRef;
     thiz->Release = IDirectFBVideoProvider_V4L_Release;
     thiz->GetSurfaceDescription =
          IDirectFBVideoProvider_V4L_GetSurfaceDescription;
     thiz->PlayTo = IDirectFBVideoProvider_V4L_PlayTo;
     thiz->Stop = IDirectFBVideoProvider_V4L_Stop;
     thiz->SeekTo    = IDirectFBVideoProvider_V4L_SeekTo;
     thiz->GetPos    = IDirectFBVideoProvider_V4L_GetPos;
     thiz->GetLength = IDirectFBVideoProvider_V4L_GetLength;

     return DFB_OK;
}


/*****************/

/*
 * bogus thread to generate callback,
 * because video4linux does not support syncing in overlay mode
 */
static void* FrameThread( void *ctx )
{
     IDirectFBVideoProvider_V4L_data *data =
          (IDirectFBVideoProvider_V4L_data*)ctx;
     
     struct timeval tv;

     while (1) {
          tv.tv_sec = 0;
          tv.tv_usec = 20000;
          select( 0, 0, 0, 0, &tv );

          pthread_testcancel();

          data->callback( data->ctx );
     }
}


static SLResult v4l_surface_listener( CoreSurface *surface, unsigned int flags,
                                      void *ctx )
{
     v4l_stop( (IDirectFBVideoProvider_V4L_data*)ctx );

     return SL_REMOVE;
}

/************/

static DFBResult v4l_to_surface( CoreSurface *surface, DFBRectangle *rect,
                                 IDirectFBVideoProvider_V4L_data *data )
{
     int bpp, palette;
     SurfaceBuffer *buffer = surface->back_buffer;

     if (surfacemanager_assure_video(buffer)) {
          return DFB_UNSUPPORTED;
     }

     v4l_stop( data );

     switch (surface->format) {
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

          b.base = (void*)(card->fix.smem_start + buffer->video.offset);
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
          //p.contrast = 0x8000;
          //p.colour = 0x6000;

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

     if (ioctl( data->fd, VIDIOCCAPTURE, &one ) < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/v4l: "
                     "Could not start capturing (VIDIOCCAPTURE failed)!\n" );

          return ret;
     }

/*     surface_install_listener( surface, v4l_surface_listener,
                               CSN_DESTROY| CSN_FLIP| CSN_SIZEFORMAT| CSN_VIDEO,
                               NULL );*/
     
     if (data->callback)
          pthread_create( &data->thread, NULL, FrameThread, data );

     return DFB_OK;
}

static DFBResult v4l_stop( IDirectFBVideoProvider_V4L_data *data )
{
     if (data->thread != -1) {
          pthread_cancel( data->thread );
          pthread_join( data->thread, NULL );
          data->thread = -1;
     }

     if (ioctl( data->fd, VIDIOCCAPTURE, &zero ) < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/v4l: "
                     "Could not stop capturing (VIDIOCCAPTURE failed)!\n" );

          return ret;
     }

     return DFB_OK;
}

static void v4l_deinit( IDirectFBVideoProvider_V4L_data *data )
{
     if (data->fd == -1) {
          BUG( "v4l_deinit with 'fd == -1'" );
          return;
     }

     if (data->thread != -1) {
          pthread_cancel( data->thread );
          pthread_join( data->thread, NULL );
          data->thread = -1;
     }

     ioctl( data->fd, VIDIOCCAPTURE, &zero );

     close( data->fd );
     data->fd = -1;
}

