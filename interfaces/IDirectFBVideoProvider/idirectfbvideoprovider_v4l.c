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
#include <directfb_internals.h>

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


/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
     int                      ref;       /* reference counter */

     char                    *filename;
     int                      fd;
     struct video_capability  vcap;
     pthread_t                thread;
     CoreSurface             *destination;
     DVFrameCallback          callback;
     void                    *ctx;

     CoreCleanup             *cleanup;
} IDirectFBVideoProvider_V4L_data;

static const unsigned int zero = 0;
static const unsigned int one = 1;

static void* FrameThread( void *context );
static ReactionResult v4l_surface_listener( const void *msg_data, void *ctx );
static DFBResult v4l_to_surface( CoreSurface *surface, DFBRectangle *rect,
                                 IDirectFBVideoProvider_V4L_data *data );
static DFBResult v4l_stop( IDirectFBVideoProvider_V4L_data *data );
static void v4l_deinit( IDirectFBVideoProvider_V4L_data *data );
static void v4l_suspend_resume( int suspend, void *ctx );
static void v4l_cleanup( void *data, int emergency );


static void IDirectFBVideoProvider_V4L_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_V4L_data *data =
          (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     DFBRemoveSuspendResumeFunc( v4l_suspend_resume, (void*)data );

     v4l_deinit( data );

     if (data->cleanup)
          core_cleanup_remove( data->cleanup );

     DFBFREE( data->filename );

     DFBFREE( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     DFBFREE( thiz );
#endif
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
     desc->pixelformat = layers->surface->format;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_PlayTo(
                                            IDirectFBVideoProvider *thiz,
                                            IDirectFBSurface       *destination,
                                            DFBRectangle           *dstrect,
                                            DVFrameCallback         callback,
                                            void                   *ctx )
{
     DFBRectangle           rect;
     IDirectFBSurface_data *dst_data;

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

     if (!rectangle_intersect( &rect, &dst_data->area.current ))
          return DFB_INVAREA;

     data->callback = callback;
     data->ctx      = ctx;

     return v4l_to_surface( dst_data->surface, &rect, data );
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
          DFBCALLOC( 1, sizeof(IDirectFBVideoProvider_V4L_data) );

     thiz->priv = data;

     data->ref = 1;

     ioctl( fd, VIDIOCGCAP, &data->vcap );
     ioctl( fd, VIDIOCCAPTURE, &zero );

     data->filename = DFBSTRDUP( filename );
     data->fd = fd;
     data->thread = -1;

     DFBAddSuspendResumeFunc( v4l_suspend_resume, (void*)data );

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
static void* FrameThread( void *ctx )
{
     IDirectFBVideoProvider_V4L_data *data =
          (IDirectFBVideoProvider_V4L_data*)ctx;

     int            even = 0;
     struct timeval tv;

     while (1) {
          tv.tv_sec = 0;
          tv.tv_usec = 20000;
          select( 0, 0, 0, 0, &tv );

          pthread_testcancel();

          if (data->destination->caps & DSCAPS_INTERLACED) {
               surface_notify_listeners( data->destination,
                                         even ? CSNF_SET_EVEN : CSNF_SET_ODD );
               even = !even;
          }

          if (data->callback)
               data->callback( data->ctx );
     }
}


static ReactionResult v4l_surface_listener( const void *msg_data, void *ctx )
{
     v4l_stop( (IDirectFBVideoProvider_V4L_data*)ctx );

     return RS_REMOVE;
}

/************/

static DFBResult v4l_to_surface( CoreSurface *surface, DFBRectangle *rect,
                                 IDirectFBVideoProvider_V4L_data *data )
{
     DFBResult ret;
     int bpp, palette;
     SurfaceBuffer *buffer = surface->back_buffer;

     surfacemanager_lock();

     ret = surfacemanager_assure_video(buffer);

     surfacemanager_unlock();

     if (ret)
          return ret;

     v4l_stop( data );

     switch (surface->format) {
          case DSPF_YUY2:
               bpp = 16;
               palette = VIDEO_PALETTE_YUYV;
               break;
          case DSPF_UYVY:
               bpp = 16;
               palette = VIDEO_PALETTE_UYVY;
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
          data->cleanup = core_cleanup_add( v4l_cleanup, data, 1 );

     if (ioctl( data->fd, VIDIOCCAPTURE, &one ) < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/v4l: "
                     "Could not start capturing (VIDIOCCAPTURE failed)!\n" );

          return ret;
     }

/*     surface_install_listener( surface, v4l_surface_listener,
                               CSN_DESTROY| CSN_FLIP| CSN_SIZEFORMAT| CSN_VIDEO,
                               NULL );*/

     data->destination = surface;

     if (data->callback || surface->caps & DSCAPS_INTERLACED)
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

     close( data->fd );
     data->fd = -1;
}

static void v4l_suspend_resume( int suspend, void *ctx )
{
     IDirectFBVideoProvider_V4L_data *data =
          (IDirectFBVideoProvider_V4L_data*)ctx;

     if (suspend) {
          v4l_stop( data );
          close( data->fd );
     }
     else {
          data->fd = open( data->filename, O_RDWR );
          if (data->fd < 0)
               PERRORMSG( "DirectFB/v4l: Cannot reopen `%s'!\n",
                          data->filename );
     }
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
