/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#include <string.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#ifdef HAVE_LINUX_COMPILER_H
#include <linux/compiler.h>
#endif
#include "videodev.h"

#include <directfb.h>

#include <media/idirectfbvideoprovider.h>
#include <media/idirectfbdatabuffer.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/layer_control.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>

#include <display/idirectfbsurface.h>

#include <misc/util.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#ifdef DFB_HAVE_V4L2
#include "videodev2.h"
#endif

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, V4L )

/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
     int                      ref;       /* reference counter */

     char                    *filename;
     int                      fd;
#ifdef DFB_HAVE_V4L2
#define NUMBER_OF_BUFFERS 2
     bool is_v4l2;

     struct v4l2_format fmt;
     struct v4l2_capability caps;

     struct v4l2_queryctrl brightness;
     struct v4l2_queryctrl contrast;
     struct v4l2_queryctrl saturation;
     struct v4l2_queryctrl hue;

     struct v4l2_requestbuffers req;
     struct v4l2_buffer vidbuf[NUMBER_OF_BUFFERS];
     char *ptr[NUMBER_OF_BUFFERS]; /* only used for capture to system memory */
     bool framebuffer_or_system;
#endif
     struct video_capability  vcap;
     struct video_mmap        vmmap;
     struct video_mbuf        vmbuf;
     void                    *buffer;
     bool                     grab_mode;

     DirectThread            *thread;
     CoreSurface             *destination;
     DVFrameCallback          callback;
     void                    *ctx;

     CoreCleanup             *cleanup;

     bool                     running;
     pthread_mutex_t          lock;

     Reaction                 reaction; /* for the destination listener */

     CoreDFB                 *core;
} IDirectFBVideoProvider_V4L_data;

static const unsigned int zero = 0;
static const unsigned int one = 1;

static void* OverlayThread( DirectThread *thread, void *context );
static void* GrabThread( DirectThread *thread, void *context );
static ReactionResult v4l_videosurface_listener( const void *msg_data, void *ctx );
static ReactionResult v4l_systemsurface_listener( const void *msg_data, void *ctx );
static DFBResult v4l_to_surface_overlay( CoreSurface *surface, DFBRectangle *rect,
                                         IDirectFBVideoProvider_V4L_data *data );
static DFBResult v4l_to_surface_grab( CoreSurface *surface, DFBRectangle *rect,
                                      IDirectFBVideoProvider_V4L_data *data );
static DFBResult v4l_stop( IDirectFBVideoProvider_V4L_data *data, bool detach );
static void v4l_deinit( IDirectFBVideoProvider_V4L_data *data );
static void v4l_cleanup( void *data, int emergency );

#ifdef DFB_HAVE_V4L2
static DFBResult v4l2_playto(CoreSurface * surface, DFBRectangle * rect, IDirectFBVideoProvider_V4L_data * data);
#endif

static void IDirectFBVideoProvider_V4L_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_V4L_data *data =
     (IDirectFBVideoProvider_V4L_data*)thiz->priv;

     if (data->cleanup)
          dfb_core_cleanup_remove( NULL, data->cleanup );

     v4l_deinit( data );

     D_FREE( data->filename );

     pthread_mutex_destroy( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult IDirectFBVideoProvider_V4L_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (--data->ref == 0) {
          IDirectFBVideoProvider_V4L_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_GetCapabilities (
                                                            IDirectFBVideoProvider       *thiz,
                                                            DFBVideoProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!caps)
          return DFB_INVARG;

#ifdef DFB_HAVE_V4L2
     if ( 0 != data->is_v4l2 ) {

          *caps = 0;

          data->saturation.id = V4L2_CID_SATURATION;
          if ( 0 != ioctl(data->fd, VIDIOC_G_CTRL, &data->saturation)) {
               *caps |= DVCAPS_SATURATION;
          }
          else {
               data->saturation.id = 0;
          }
          data->brightness.id = V4L2_CID_BRIGHTNESS;
          if ( 0 != ioctl(data->fd, VIDIOC_G_CTRL, &data->brightness)) {
               *caps |= DVCAPS_BRIGHTNESS;
          }
          else {
               data->brightness.id = 0;
          }
          data->contrast.id = V4L2_CID_CONTRAST;
          if ( 0 != ioctl(data->fd, VIDIOC_G_CTRL, &data->contrast)) {
               *caps |= DVCAPS_CONTRAST;
          }
          else {
               data->contrast.id = 0;
          }
          data->hue.id = V4L2_CID_HUE;
          if ( 0 != ioctl(data->fd, VIDIOC_G_CTRL, &data->hue)) {
               *caps |= DVCAPS_HUE;
          }
          else {
               data->hue.id = 0;
          }
          /* fixme: interlaced might not be true for field capture */
          *caps |= DVCAPS_BASIC | DVCAPS_SCALE | DVCAPS_INTERLACED;
     }
     else
#endif
     {
          *caps = ( DVCAPS_BASIC      |
                    DVCAPS_BRIGHTNESS |
                    DVCAPS_CONTRAST   |
                    DVCAPS_HUE        |
                    DVCAPS_SATURATION |
                    DVCAPS_INTERLACED );

          if (data->vcap.type & VID_TYPE_SCALES)
               *caps |= DVCAPS_SCALE;
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

     desc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_CAPS;
#ifdef DFB_HAVE_V4L2
     if ( 0 != data->is_v4l2 ) {
          desc->width = 720;  /* fimxe: depends on the selected standard: query standard and set accordingly */
          desc->height = 576;
     }
     else
#endif
     {
          desc->width  = data->vcap.maxwidth;
          desc->height = data->vcap.maxheight;
     }
     desc->pixelformat = dfb_primary_layer_pixelformat();
     desc->caps = DSCAPS_INTERLACED;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_GetStreamDescription(
                                                                 IDirectFBVideoProvider *thiz,
                                                                 DFBStreamDescription   *desc )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!desc)
          return DFB_INVARG;

     desc->caps = DVSCAPS_VIDEO;
     
     desc->video.encoding[0] = 0;
     desc->video.framerate   = 10; // assume 10fps
#ifdef DFB_HAVE_V4L2
     desc->video.aspect      = 720.0/576.0;
#else
     desc->video.aspect      = (double)data->vcap.maxwidth /
                               (double)data->vcap.maxheight;
#endif
     desc->video.bitrate     = 0;

     desc->title[0] = desc->author[0] = desc->album[0]   =
     desc->year     = desc->genre[0]  = desc->comment[0] = 0;

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

     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

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

     v4l_stop( data, true );

     pthread_mutex_lock( &data->lock );

     data->callback = callback;
     data->ctx      = ctx;

     surface = dst_data->surface;

#ifdef DFB_HAVE_V4L2
     if ( 0 != data->is_v4l2 ) {
          ret = v4l2_playto(surface, &rect, data);
     }
     else
#endif
     {
          data->grab_mode = 0;
          if ( getenv("DFB_V4L_GRAB") || surface->caps & DSCAPS_SYSTEMONLY
               || surface->caps & DSCAPS_FLIPPING
               || !(VID_TYPE_OVERLAY & data->vcap.type))
               data->grab_mode = 1;
          else {
               dfb_surfacemanager_lock( surface->manager );

               /*
                * Because we're constantly writing to the surface we
                * permanently lock it.
                */
               ret = dfb_surface_hardware_lock( data->core, surface,
                                                DSLF_WRITE | CSLF_FORCE, false );

               dfb_surfacemanager_unlock( surface->manager );

               if (ret) {
                    pthread_mutex_unlock( &data->lock );
                    return ret;
               }
          }

          if (data->grab_mode)
               ret = v4l_to_surface_grab( surface, &rect, data );
          else
               ret = v4l_to_surface_overlay( surface, &rect, data );
     }
     if (DFB_OK != ret && !data->grab_mode)
          dfb_surface_unlock( surface, false );

     pthread_mutex_unlock( &data->lock );

     return ret;
}

static DFBResult IDirectFBVideoProvider_V4L_Stop(
                                                IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     return v4l_stop( data, true );
}

static DFBResult IDirectFBVideoProvider_V4L_GetStatus( 
                                                     IDirectFBVideoProvider *thiz,
                                                     DFBVideoProviderStatus *status )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!status)
          return DFB_INVARG;

     *status = data->running ? DVSTATE_PLAY : DVSTATE_STOP;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_SeekTo(
                                                  IDirectFBVideoProvider *thiz,
                                                  double                  seconds )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_V4L_GetPos(
                                                  IDirectFBVideoProvider *thiz,
                                                  double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     return DFB_UNSUPPORTED;
}

static DFBResult IDirectFBVideoProvider_V4L_GetLength(
                                                     IDirectFBVideoProvider *thiz,
                                                     double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     return DFB_UNSUPPORTED;
}

static DFBResult IDirectFBVideoProvider_V4L_GetColorAdjustment(
                                                              IDirectFBVideoProvider *thiz,
                                                              DFBColorAdjustment     *adj )
{
     struct video_picture pic;

     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!adj)
          return DFB_INVARG;

#ifdef DFB_HAVE_V4L2
     if ( 0 != data->is_v4l2 ) {
          struct v4l2_control ctrl;

          if (data->brightness.id != 0) {
               ctrl.id = data->brightness.id;
               if ( 0 == ioctl(data->fd, VIDIOC_G_CTRL, &ctrl)) {
                    adj->flags |= DCAF_BRIGHTNESS;
                    adj->brightness = 0xffff * ctrl.value / (data->brightness.maximum - data->brightness.minimum);
               }
          }
          if (data->contrast.id != 0) {
               ctrl.id = data->contrast.id;
               if ( 0 == ioctl(data->fd, VIDIOC_G_CTRL, &ctrl)) {
                    adj->flags |= DCAF_CONTRAST;
                    adj->contrast = 0xffff * ctrl.value / (data->contrast.maximum - data->contrast.minimum);
               }
          }
          if (data->hue.id != 0) {
               ctrl.id = data->hue.id;
               if ( 0 == ioctl(data->fd, VIDIOC_G_CTRL, &ctrl)) {
                    adj->flags |= DCAF_HUE;
                    adj->hue = 0xffff * ctrl.value / (data->hue.maximum - data->hue.minimum);
               }
          }
          if (data->saturation.id != 0) {
               ctrl.id = data->saturation.id;
               if ( 0 == ioctl(data->fd, VIDIOC_G_CTRL, &ctrl)) {
                    adj->flags |= DCAF_SATURATION;
                    adj->saturation = 0xffff * ctrl.value / (data->saturation.maximum - data->saturation.minimum);
               }
          }
     }
     else
#endif
     {
          ioctl( data->fd, VIDIOCGPICT, &pic );

          adj->flags = DCAF_BRIGHTNESS | DCAF_CONTRAST | DCAF_HUE | DCAF_SATURATION;

          adj->brightness = pic.brightness;
          adj->contrast   = pic.contrast;
          adj->hue        = pic.hue;
          adj->saturation = pic.colour;
     }
     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_SetColorAdjustment( IDirectFBVideoProvider   *thiz,
                                                                const DFBColorAdjustment *adj )
{
     struct video_picture pic;

     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     if (!adj)
          return DFB_INVARG;

     if (adj->flags == DCAF_NONE)
          return DFB_OK;

#ifdef DFB_HAVE_V4L2
     if ( 0 != data->is_v4l2 ) {
          struct v4l2_control ctrl;
          if (adj->flags & DCAF_BRIGHTNESS && data->brightness.id != 0) {
               ctrl.id = data->brightness.id;
               ctrl.value = (adj->brightness * (data->brightness.maximum - data->brightness.minimum)) / 0xfff;
               ioctl(data->fd, VIDIOC_S_CTRL, &ctrl);
          }
          if (adj->flags & DCAF_CONTRAST && data->contrast.id != 0) {
               ctrl.id = data->contrast.id;
               ctrl.value = (adj->contrast * (data->contrast.maximum - data->contrast.minimum)) / 0xfff;
               ioctl(data->fd, VIDIOC_S_CTRL, &ctrl);
          }
          if (adj->flags & DCAF_HUE && data->hue.id != 0) {
               ctrl.id = data->hue.id;
               ctrl.value = (adj->hue * (data->hue.maximum - data->hue.minimum)) / 0xfff;
               ioctl(data->fd, VIDIOC_S_CTRL, &ctrl);
          }
          if (adj->flags & DCAF_SATURATION && data->saturation.id != 0) {
               ctrl.id = data->saturation.id;
               ctrl.value = (adj->saturation * (data->saturation.maximum - data->saturation.minimum)) / 0xfff;
               ioctl(data->fd, VIDIOC_S_CTRL, &ctrl);
          }
     }
     else
#endif
     {
          if (ioctl( data->fd, VIDIOCGPICT, &pic ) < 0) {
               DFBResult ret = errno2result( errno );

               D_PERROR( "DirectFB/Video4Linux: VIDIOCGPICT failed!\n" );

               return ret;
          }

          if (adj->flags & DCAF_BRIGHTNESS) pic.brightness = adj->brightness;
          if (adj->flags & DCAF_CONTRAST)   pic.contrast   = adj->contrast;
          if (adj->flags & DCAF_HUE)        pic.hue        = adj->hue;
          if (adj->flags & DCAF_SATURATION) pic.colour     = adj->saturation;

          if (ioctl( data->fd, VIDIOCSPICT, &pic ) < 0) {
               DFBResult ret = errno2result( errno );

               D_PERROR( "DirectFB/Video4Linux: VIDIOCSPICT failed!\n" );

               return ret;
          }
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_V4L_SendEvent( IDirectFBVideoProvider *thiz,
                                                       const DFBEvent         *evt )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBVideoProvider_V4L)

     return DFB_UNSUPPORTED;
}


/* exported symbols */

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     if (ctx->filename) {
          if (strncmp( ctx->filename, "/dev/video", 10 ) == 0)
               return DFB_OK;

          if (strncmp( ctx->filename, "/dev/v4l/video", 14 ) == 0)
               return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz, ... )
{
     int fd;
     IDirectFBDataBuffer *buffer;
     IDirectFBDataBuffer_data *buffer_data;
     CoreDFB             *core;
     va_list              tag;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBVideoProvider_V4L)

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );


     data->ref  = 1;
     data->core = core;

     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;

     fd = open( buffer_data->filename, O_RDWR );     
     if (fd < 0) {
          DFBResult ret = errno2result( errno );

          D_PERROR( "DirectFB/Video4Linux: Cannot open `%s'!\n",
                     buffer_data->filename );

          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     direct_util_recursive_pthread_mutex_init( &data->lock );

#ifdef DFB_HAVE_V4L2
     data->is_v4l2 = 0;

     /* look if the device is a v4l2 device */
     if (0 == ioctl(fd, VIDIOC_QUERYCAP, &data->caps)) {
          D_INFO("DirectFB/Video4Linux: This is a Video4Linux-2 device.\n");
          data->is_v4l2 = 1;
     }

     if ( 0 != data->is_v4l2 ) {
          /* hmm, anything to do here? */
     }
     else
#endif
     {
          D_INFO("DirectFB/Video4Linux: This is a Video4Linux-1 device.\n");

          ioctl( fd, VIDIOCGCAP, &data->vcap );
          ioctl( fd, VIDIOCCAPTURE, &zero );

          ioctl( fd, VIDIOCGMBUF, &data->vmbuf );

          data->buffer = mmap( NULL, data->vmbuf.size,
                               PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
     }

     data->filename = D_STRDUP( buffer_data->filename );
     data->fd       = fd;

     thiz->AddRef    = IDirectFBVideoProvider_V4L_AddRef;
     thiz->Release   = IDirectFBVideoProvider_V4L_Release;
     thiz->GetCapabilities = IDirectFBVideoProvider_V4L_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_V4L_GetSurfaceDescription;
     thiz->GetStreamDescription = IDirectFBVideoProvider_V4L_GetStreamDescription;
     thiz->PlayTo    = IDirectFBVideoProvider_V4L_PlayTo;
     thiz->Stop      = IDirectFBVideoProvider_V4L_Stop;
     thiz->GetStatus = IDirectFBVideoProvider_V4L_GetStatus;
     thiz->SeekTo    = IDirectFBVideoProvider_V4L_SeekTo;
     thiz->GetPos    = IDirectFBVideoProvider_V4L_GetPos;
     thiz->GetLength = IDirectFBVideoProvider_V4L_GetLength;
     thiz->GetColorAdjustment = IDirectFBVideoProvider_V4L_GetColorAdjustment;
     thiz->SetColorAdjustment = IDirectFBVideoProvider_V4L_SetColorAdjustment;
     thiz->SendEvent = IDirectFBVideoProvider_V4L_SendEvent;

     return DFB_OK;
}


/*****************/

/*
 * bogus thread to generate callback,
 * because video4linux does not support syncing in overlay mode
 */
static void* OverlayThread( DirectThread *thread, void *ctx )
{
     IDirectFBVideoProvider_V4L_data *data =
     (IDirectFBVideoProvider_V4L_data*)ctx;

     int field = 0;
     struct timeval tv;

     while (data->running) {
          tv.tv_sec = 0;
          tv.tv_usec = 20000;
          select( 0, 0, 0, 0, &tv );

          if (!data->running)
               break;

          if (data->destination &&
              data->destination->caps & DSCAPS_INTERLACED) {
               dfb_surface_set_field( data->destination, field );

               field = !field;
          }

          if (!data->running)
               break;

          if (data->callback)
               data->callback( data->ctx );
     }

     return NULL;
}

/*
 * thread to capture data from v4l buffers and generate callback
 */
static void* GrabThread( DirectThread *thread, void *ctx )
{
     IDirectFBVideoProvider_V4L_data *data =
     (IDirectFBVideoProvider_V4L_data*)ctx;
     CoreSurface *surface = data->destination;
     void *src, *dst;
     int dst_pitch, src_pitch, h;
     int frame = 0;

     D_DEBUG( "DirectFB/Video4Linux: %s started.\n", __FUNCTION__ );

     src_pitch = DFB_BYTES_PER_LINE( surface->format, surface->width );

     while (frame < data->vmbuf.frames) {
          data->vmmap.frame = frame;
          ioctl( data->fd, VIDIOCMCAPTURE, &data->vmmap );
          frame++;
     }

     if (dfb_surface_ref( surface )) {
          D_ERROR( "DirectFB/Video4Linux: dfb_surface_ref() failed!\n" );
          return NULL;
     }

     frame = 0;
     while (data->running) {
          ioctl( data->fd, VIDIOCSYNC, &frame );

          if (!data->running)
               break;

          h = surface->height;
          src = data->buffer + data->vmbuf.offsets[frame];
          dfb_surface_soft_lock( data->core, surface, DSLF_WRITE, &dst, &dst_pitch, 0 );
          while (h--) {
               direct_memcpy( dst, src, src_pitch );
               dst += dst_pitch;
               src += src_pitch;
          }
          if (surface->format == DSPF_I420) {
               h = surface->height;
               while (h--) {
                    direct_memcpy( dst, src, src_pitch >> 1 );
                    dst += dst_pitch >> 1;
                    src += src_pitch >> 1;
               }
          }
          else if (surface->format == DSPF_YV12) {
               h = surface->height >> 1;
               src += h * (src_pitch >> 1);
               while (h--) {
                    direct_memcpy( dst, src, src_pitch >> 1 );
                    dst += dst_pitch >> 1;
                    src += src_pitch >> 1;
               }
               h = surface->height >> 1;
               src -=  2 * h * (src_pitch >> 1);
               while (h--) {
                    direct_memcpy( dst, src, src_pitch >> 1 );
                    dst += dst_pitch >> 1;
                    src += src_pitch >> 1;
               }
          }
          dfb_surface_unlock( surface, 0 );

          data->vmmap.frame = frame;
          ioctl( data->fd, VIDIOCMCAPTURE, &data->vmmap );

          if (!data->running)
               break;

          if (surface->caps & DSCAPS_INTERLACED)
               dfb_surface_set_field( surface, 0 );

          if (data->callback)
               data->callback(data->ctx);

          if (!data->running)
               break;

          sched_yield();

          if (surface->caps & DSCAPS_INTERLACED) {
               if (!data->running)
                    break;

               dfb_surface_set_field( surface, 1 );

               if (data->callback)
                    data->callback(data->ctx);

               if (!data->running)
                    break;

               sched_yield();
          }

          if (++frame == data->vmbuf.frames)
               frame = 0;
     }

     dfb_surface_unref( surface );

     return NULL;
}

static ReactionResult v4l_videosurface_listener( const void *msg_data, void *ctx )
{
     const CoreSurfaceNotification   *notification = msg_data;
     IDirectFBVideoProvider_V4L_data *data         = ctx;
     CoreSurface                     *surface      = notification->surface;

     if ((notification->flags & CSNF_SIZEFORMAT) ||
         (surface->back_buffer->video.health != CSH_STORED)) {
          v4l_stop( data, false );
          return RS_REMOVE;
     }

     return RS_OK;
}

static ReactionResult v4l_systemsurface_listener( const void *msg_data, void *ctx )
{
     const CoreSurfaceNotification   *notification = msg_data;
     IDirectFBVideoProvider_V4L_data *data         = ctx;
     CoreSurface                     *surface      = notification->surface;

     if ((notification->flags & CSNF_SIZEFORMAT) ||
         (surface->back_buffer->system.health != CSH_STORED &&
          surface->back_buffer->video.health != CSH_STORED)) {
          v4l_stop( data, false );
          return RS_REMOVE;
     }

     return RS_OK;
}


/************/

static DFBResult v4l_to_surface_overlay( CoreSurface *surface, DFBRectangle *rect,
                                         IDirectFBVideoProvider_V4L_data *data )
{
     int bpp, palette;
     SurfaceBuffer *buffer = surface->back_buffer;

     D_DEBUG( "DirectFB/Video4Linux: %s (%p, %d,%d - %dx%d)\n", __FUNCTION__,
              surface, rect->x, rect->y, rect->w, rect->h );

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
          case DSPF_ARGB1555:
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
          case DSPF_AiRGB:
          case DSPF_RGB32:
               bpp = 32;
               palette = VIDEO_PALETTE_RGB32;
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     {
          struct video_buffer b;

          b.base = (void*)dfb_gfxcard_memory_physical( NULL, buffer->video.offset );
          b.width = buffer->video.pitch / ((bpp + 7) / 8);
          b.height = surface->height;
          b.depth = bpp;
          b.bytesperline = buffer->video.pitch;

          if (ioctl( data->fd, VIDIOCSFBUF, &b ) < 0) {
               DFBResult ret = errno2result( errno );

               D_PERROR( "DirectFB/Video4Linux: VIDIOCSFBUF failed, must run being root!\n" );

               return ret;
          }
     }
     {
          struct video_picture p;

          if (ioctl( data->fd, VIDIOCGPICT, &p ) < 0) {
               DFBResult ret = errno2result( errno );

               D_PERROR( "DirectFB/Video4Linux: VIDIOCGPICT failed!\n" );

               return ret;
          }

          p.depth = bpp;
          p.palette = palette;

          if (ioctl( data->fd, VIDIOCSPICT, &p ) < 0) {
               DFBResult ret = errno2result( errno );

               D_PERROR( "DirectFB/Video4Linux: VIDIOCSPICT failed!\n" );

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
               DFBResult ret = errno2result( errno );

               D_PERROR( "DirectFB/Video4Linux: VIDIOCSWIN failed!\n" );

               return ret;
          }
     }

     if (!data->cleanup)
          data->cleanup = dfb_core_cleanup_add( NULL, v4l_cleanup, data, true );

     if (ioctl( data->fd, VIDIOCCAPTURE, &one ) < 0) {
          DFBResult ret = errno2result( errno );

          D_PERROR( "DirectFB/Video4Linux: Could not start capturing (VIDIOCCAPTURE failed)!\n" );

          return ret;
     }

     data->destination = surface;

     dfb_surface_attach( surface, v4l_videosurface_listener,
                         data, &data->reaction );

     data->running = true;

     if (data->callback || surface->caps & DSCAPS_INTERLACED)
          data->thread = direct_thread_create( DTT_CRITICAL, OverlayThread, data, "V4L Overlay" );

     return DFB_OK;
}

static DFBResult v4l_to_surface_grab( CoreSurface *surface, DFBRectangle *rect,
                                      IDirectFBVideoProvider_V4L_data *data )
{
     int palette;

     D_DEBUG( "DirectFB/Video4Linux: %s...\n", __FUNCTION__ );

     if (!data->vmbuf.frames)
          return DFB_UNSUPPORTED;

     switch (surface->format) {
          case DSPF_YUY2:
               palette = VIDEO_PALETTE_YUYV;
               break;
          case DSPF_UYVY:
               palette = VIDEO_PALETTE_UYVY;
               break;
          case DSPF_I420:
          case DSPF_YV12:
               palette = VIDEO_PALETTE_YUV420P;
               break;
          case DSPF_ARGB1555:
               palette = VIDEO_PALETTE_RGB555;
               break;
          case DSPF_RGB16:
               palette = VIDEO_PALETTE_RGB565;
               break;
          case DSPF_RGB24:
               palette = VIDEO_PALETTE_RGB24;
               break;
          case DSPF_ARGB:
          case DSPF_AiRGB:
          case DSPF_RGB32:
               palette = VIDEO_PALETTE_RGB32;
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     data->vmmap.width = surface->width;
     data->vmmap.height = surface->height;
     data->vmmap.format = palette;
     data->vmmap.frame = 0;
     if (ioctl(data->fd, VIDIOCMCAPTURE, &data->vmmap) < 0) {
          DFBResult ret = errno2result(errno);

          D_PERROR("DirectFB/Video4Linux: Could not start capturing (VIDIOCMCAPTURE failed)!\n");

          return ret;
     }

     if (!data->cleanup)
          data->cleanup = dfb_core_cleanup_add( NULL, v4l_cleanup, data, true );

     data->destination = surface;

     dfb_surface_attach( surface, v4l_systemsurface_listener,
                         data, &data->reaction );

     data->running = true;

     data->thread = direct_thread_create( DTT_INPUT, GrabThread, data, "V4L Grabber" );

     return DFB_OK;
}

static DFBResult v4l_stop( IDirectFBVideoProvider_V4L_data *data, bool detach )
{
     CoreSurface *destination;

     D_DEBUG( "DirectFB/Video4Linux: %s...\n", __FUNCTION__ );

     pthread_mutex_lock( &data->lock );

     if (!data->running) {
          pthread_mutex_unlock( &data->lock );
          return DFB_OK;
     }

     if (data->thread) {
          data->running = false;
          direct_thread_join( data->thread );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }

#ifdef DFB_HAVE_V4L2
     if ( 0 != data->is_v4l2 ) {
          /* turn off streaming */
          int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          int err = ioctl(data->fd, VIDIOC_STREAMOFF, &type);
          if (err) {
               D_PERROR("DirectFB/Video4Linux2: VIDIOC_STREAMOFF.\n");
               /* don't quit here */
          }
     }
     else
#endif
     {
          if (!data->grab_mode) {
               if (ioctl( data->fd, VIDIOCCAPTURE, &zero ) < 0)
                    D_PERROR( "DirectFB/Video4Linux: "
                              "Could not stop capturing (VIDIOCCAPTURE failed)!\n" );
          }
     }

     destination = data->destination;

     if (!destination) {
          pthread_mutex_unlock( &data->lock );
          return DFB_OK;
     }

#ifdef DFB_HAVE_V4L2
     if ( 0 != data->is_v4l2 ) {
          /* unmap all buffers, if necessary */
          if (0 != data->framebuffer_or_system) {
               int i;
               for (i = 0; i < data->req.count; i++) {
                    struct v4l2_buffer *vidbuf = &data->vidbuf[i];
                    D_DEBUG("DirectFB/Video4Linux2: %d => 0x%08x, len:%d\n", i, (u32) data->ptr[i], vidbuf->length);
                    if (0 != munmap(data->ptr[i], vidbuf->length)) {
                         D_PERROR("DirectFB/Video4Linux2: munmap().\n");
                    }
               }
          }
     }
     else
#endif
     {
          if (!data->grab_mode)
               dfb_surface_unlock( destination, false );
     }

     data->destination = NULL;

     pthread_mutex_unlock( &data->lock );

     if (detach)
          dfb_surface_detach( destination, &data->reaction );

     return DFB_OK;
}

static void v4l_deinit( IDirectFBVideoProvider_V4L_data *data )
{
     if (data->fd == -1) {
          D_BUG( "v4l_deinit with 'fd == -1'" );
          return;
     }

     v4l_stop( data, true );

     munmap( data->buffer, data->vmbuf.size );
     close( data->fd );
     data->fd = -1;
}

static void v4l_cleanup( void *ctx, int emergency )
{
     IDirectFBVideoProvider_V4L_data *data =
     (IDirectFBVideoProvider_V4L_data*)ctx;

     if (emergency)
          v4l_stop( data, false );
     else
          v4l_deinit( data );
}

/* v4l2 specific stuff */
#ifdef DFB_HAVE_V4L2
static int wait_for_buffer(int vid, struct v4l2_buffer *cur)
{
     fd_set rdset;
     struct timeval timeout;
     int n, err;

//	D_DEBUG("DirectFB/Video4Linux2: %s...\n", __FUNCTION__);

     cur->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

     FD_ZERO(&rdset);
     FD_SET(vid, &rdset);

     timeout.tv_sec = 5;
     timeout.tv_usec = 0;

     n = select(vid + 1, &rdset, NULL, NULL, &timeout);
     if (n == -1) {
          D_PERROR("DirectFB/Video4Linux2: select().\n");
          return -1;     /* fixme */
     }
     else if (n == 0) {
          D_PERROR("DirectFB/Video4Linux2: select(), timeout.\n");
          return -1;     /* fixme */
     }
     else if (FD_ISSET(vid, &rdset)) {
          err = ioctl(vid, VIDIOC_DQBUF, cur);
          if (err) {
               D_PERROR("DirectFB/Video4Linux2: VIDIOC_DQBUF.\n");
               return -1;     /* fixme */
          }
     }
     return 0;
}

static void *V4L2_Thread(DirectThread * thread, void *ctx)
{
     int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     int i, err;

     IDirectFBVideoProvider_V4L_data *data = (IDirectFBVideoProvider_V4L_data *) ctx;
     CoreSurface *surface = data->destination;
     SurfaceBuffer *buffer = surface->back_buffer;
     void *src, *dst;
     int dst_pitch, src_pitch, h;

     D_DEBUG("DirectFB/Video4Linux2: %s started.\n", __FUNCTION__);

     src_pitch = DFB_BYTES_PER_LINE(surface->format, surface->width);

     /* Queue all buffers */
     for (i = 0; i < data->req.count; i++) {
          struct v4l2_buffer *vidbuf = &data->vidbuf[i];

          if (0 == data->framebuffer_or_system) {
               vidbuf->m.offset = buffer->video.offset;
          }

          err = ioctl(data->fd, VIDIOC_QBUF, vidbuf);
          if (err) {
               D_PERROR("DirectFB/Video4Linux2: VIDIOC_QBUF.\n");
               return NULL;
          }
     }

     /* start streaming */
     if (ioctl(data->fd, VIDIOC_STREAMON, &type)) {
          D_PERROR("DirectFB/Video4Linux2: VIDIOC_STREAMON.\n");
          return NULL;   /* fixme */
     }

     while (data->running) {

          struct v4l2_buffer cur;

          if (0 != wait_for_buffer(data->fd, &cur)) {
               return NULL;
          }

          if (0 != data->framebuffer_or_system) {

               D_HEAVYDEBUG("DirectFB/Video4Linux2: index:%d, to system memory.\n", cur.index);

               h = surface->height;
               src = data->ptr[cur.index];
               dfb_surface_soft_lock(data->core, surface, DSLF_WRITE, &dst, &dst_pitch, 0);
               while (h--) {
                    direct_memcpy(dst, src, src_pitch);
                    dst += dst_pitch;
                    src += src_pitch;
               }
               if (surface->format == DSPF_I420) {
                    h = surface->height;
                    while (h--) {
                         direct_memcpy(dst, src, src_pitch >> 1);
                         dst += dst_pitch >> 1;
                         src += src_pitch >> 1;
                    }
               }
               else if (surface->format == DSPF_YV12) {
                    h = surface->height >> 1;
                    src += h * (src_pitch >> 1);
                    while (h--) {
                         direct_memcpy(dst, src, src_pitch >> 1);
                         dst += dst_pitch >> 1;
                         src += src_pitch >> 1;
                    }
                    h = surface->height >> 1;
                    src -= 2 * h * (src_pitch >> 1);
                    while (h--) {
                         direct_memcpy(dst, src, src_pitch >> 1);
                         dst += dst_pitch >> 1;
                         src += src_pitch >> 1;
                    }
               }
               else if (surface->format == DSPF_NV12 ||
                        surface->format == DSPF_NV21) {
                    h = surface->height >> 1;
                    while (h--) {
                         direct_memcpy(dst, src, src_pitch);
                         dst += dst_pitch;
                         src += src_pitch;
                    }
               }
               dfb_surface_unlock(surface, 0);
          }
          else {
               D_HEAVYDEBUG("DirectFB/Video4Linux2: index:%d, to overlay surface\n", cur.index);
          }

          if (data->callback)
               data->callback(data->ctx);

          if (0 != ioctl(data->fd, VIDIOC_QBUF, &cur)) {
               D_PERROR("DirectFB/Video4Linux2: VIDIOC_QBUF.\n");
               return NULL;
          }
     }

     return NULL;
}

static DFBResult v4l2_playto(CoreSurface * surface, DFBRectangle * rect, IDirectFBVideoProvider_V4L_data * data)
{
     SurfaceBuffer *buffer = surface->back_buffer;
     int palette;

     int err;
     int i;

     D_DEBUG("DirectFB/Video4Linux2: %s...\n", __FUNCTION__);

     switch (surface->format) {
          case DSPF_YUY2:
               palette = V4L2_PIX_FMT_YUYV;
               break;
          case DSPF_UYVY:
               palette = V4L2_PIX_FMT_UYVY;
               break;
          case DSPF_I420:
               palette = V4L2_PIX_FMT_YUV420;
               break;
          case DSPF_YV12:
               palette = V4L2_PIX_FMT_YVU420;
               break;
          case DSPF_NV12:
               palette = V4L2_PIX_FMT_NV12;
               break;
          case DSPF_NV21:
               palette = V4L2_PIX_FMT_NV21;
               break;
          case DSPF_RGB332:
               palette = V4L2_PIX_FMT_RGB332;
               break;
          case DSPF_ARGB1555:
               palette = V4L2_PIX_FMT_RGB555;
               break;
          case DSPF_RGB16:
               palette = V4L2_PIX_FMT_RGB565;
               break;
          case DSPF_RGB24:
               palette = V4L2_PIX_FMT_BGR24;
               break;
          case DSPF_ARGB:
          case DSPF_AiRGB:
          case DSPF_RGB32:
               palette = V4L2_PIX_FMT_BGR32;
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     data->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     data->fmt.fmt.pix.width = surface->width;
     data->fmt.fmt.pix.height = surface->height;
     data->fmt.fmt.pix.pixelformat = palette;
     data->fmt.fmt.pix.bytesperline = buffer->video.pitch;
     data->fmt.fmt.pix.field = V4L2_FIELD_INTERLACED; /* fixme: we can do field based capture, too */

     D_DEBUG("DirectFB/Video4Linux2: surface->width:%d, surface->height:%d.\n", surface->width, surface->height);

     err = ioctl(data->fd, VIDIOC_S_FMT, &data->fmt);
     if (err) {
          D_PERROR("DirectFB/Video4Linux2: VIDIOC_S_FMT.\n");
          return err;
     }

     if (data->fmt.fmt.pix.width != surface->width || data->fmt.fmt.pix.height != surface->height) {
          D_PERROR("DirectFB/Video4Linux2: driver cannot fulfill application request.\n");
          return DFB_UNSUPPORTED;  /* fixme */
     }

     if ( 0 != data->brightness.id ) {
          ioctl(data->fd, VIDIOC_G_CTRL, &data->brightness);
     }
     if ( 0 != data->contrast.id ) {
          ioctl(data->fd, VIDIOC_G_CTRL, &data->contrast);
     }
     if ( 0 != data->saturation.id ) {
          ioctl(data->fd, VIDIOC_G_CTRL, &data->saturation);
     }
     if ( 0 != data->hue.id ) {
          ioctl(data->fd, VIDIOC_G_CTRL, &data->hue);
     }

     if (surface->caps & DSCAPS_SYSTEMONLY) {
          data->framebuffer_or_system = 1;
          data->req.memory = V4L2_MEMORY_MMAP;
     }
     else {
          struct v4l2_framebuffer fb;

          data->framebuffer_or_system = 0;
          data->req.memory = V4L2_MEMORY_OVERLAY;

          fb.base = (void *) dfb_gfxcard_memory_physical(NULL, 0);
          fb.fmt.width = surface->width;
          fb.fmt.height = surface->height;
          fb.fmt.pixelformat = palette;

          D_DEBUG("w:%d, h:%d, bpl:%d, base:0x%08lx\n",fb.fmt.width, fb.fmt.height, fb.fmt.bytesperline, (unsigned long)fb.base);

          if (ioctl(data->fd, VIDIOC_S_FBUF, &fb) < 0) {
               DFBResult ret = errno2result(errno);

               D_PERROR("DirectFB/Video4Linux2: VIDIOC_S_FBUF failed, must run being root!\n");

               return ret;
          }
     }

     /* Ask Video Device for Buffers */
     data->req.count = NUMBER_OF_BUFFERS;
     data->req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     err = ioctl(data->fd, VIDIOC_REQBUFS, &data->req);
     if (err < 0 || data->req.count < NUMBER_OF_BUFFERS) {
          D_PERROR("DirectFB/Video4Linux2: VIDIOC_REQBUFS: %d, %d.\n", err, data->req.count);
          return err;
     }

     /* Query each buffer and map it to the video device if necessary */
     for (i = 0; i < data->req.count; i++) {
          struct v4l2_buffer *vidbuf = &data->vidbuf[i];

          vidbuf->index = i;
          vidbuf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

          err = ioctl(data->fd, VIDIOC_QUERYBUF, vidbuf);
          if (err < 0) {
               D_PERROR("DirectFB/Video4Linux2: VIDIOC_QUERYBUF.\n");
               return err;
          }

/*
          if (vidbuf->length == 0) {
               D_PERROR("DirectFB/Video4Linux2: length is zero!\n");
               return -EINVAL;
          }
*/
          if (0 != data->framebuffer_or_system) {
               data->ptr[i] = mmap(0, vidbuf->length, PROT_READ | PROT_WRITE, MAP_SHARED, data->fd, vidbuf->m.offset);
               if (data->ptr[i] == MAP_FAILED) {
                    D_PERROR("DirectFB/Video4Linux2: mmap().\n");
                    return err;
               }
          }
          D_DEBUG("DirectFB/Video4Linux2: len:0x%08x, %d => 0x%08x\n", vidbuf->length, i, (u32) data->ptr[i]);
     }

     if (!data->cleanup)
          data->cleanup = dfb_core_cleanup_add( NULL, v4l_cleanup, data, true );

     data->destination = surface;

     dfb_surface_attach(surface, v4l_systemsurface_listener, data, &data->reaction);

     data->running = true;

     data->thread = direct_thread_create(DTT_DEFAULT, V4L2_Thread, data, "Video4Linux 2");

     return DFB_OK;
}
#endif
