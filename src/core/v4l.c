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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>

#include <pthread.h>

#include <directfb.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <misc/util.h>

#include "coredefs.h"
#include "v4l.h"
#include "gfxcard.h"
#include "surfacemanager.h"


static const unsigned int zero = 0;
static const unsigned int one = 1;

/*
 * TODO: move them to a struct, dynamically allocated and associated
 */
static int fd = -1;
static struct video_capability vcap;
static pthread_t thread = -1;
void* FrameThread( void *context );

static SLResult v4l_surface_listener( CoreSurface *surface, unsigned int flags,
                                      void *ctx );

static void *tmp_ctx; //HACK!!!


DFBResult v4l_probe()
{
     if (fd != -1)
          return DFB_LOCKED;

     fd = open( "/dev/video0", O_RDWR );
     if (fd < 0)
          return errno2dfb( errno );

     close( fd );
     fd = -1;

     return DFB_OK;
}

static void v4l_deinit()
{
     if (fd == -1) {
          BUG( "v4l_deinit with 'fd == -1'" );
          return;
     }

     if (thread != -1) {
          pthread_cancel( thread );
          thread = -1;
     }

     ioctl( fd, VIDIOCCAPTURE, &zero );

     close( fd );
     fd = -1;
}

DFBResult v4l_open()
{
     if (fd != -1) {
          BUG( "v4l_init called twice" );
          return DFB_BUG;
     }

     fd = open( "/dev/video0", O_RDWR );
     if (fd < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/core/v4l: Cannot open `/dev/video'!\n" );

          return ret;
     }

     ioctl( fd, VIDIOCGCAP, &vcap );

     if (ioctl( fd, VIDIOCCAPTURE, &zero ) < 0);

     core_cleanup_push( v4l_deinit );

     return DFB_OK;
}

DFBResult v4l_to_surface( CoreSurface *surface, DFBRectangle *rect,
                          DVFrameCallback callback, void *ctx )
{
     int bpp, palette;
     SurfaceBuffer *buffer = surface->back_buffer;

     if (surfacemanager_assure_video(buffer)) {
          return DFB_UNSUPPORTED;
     }

     v4l_stop();

     switch (surface->format) {
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

          if (ioctl( fd, VIDIOCSFBUF, &b ) < 0) {
               DFBResult ret = errno2dfb( errno );

               PERRORMSG( "DirectFB/core/v4l: "
                          "VIDIOCSFBUF failed, must run being root!\n" );

               return ret;
          }
     }

     {
          struct video_picture p;

          if (ioctl( fd, VIDIOCGPICT, &p ) < 0) {
               DFBResult ret = errno2dfb( errno );

               PERRORMSG( "DirectFB/core/v4l: VIDIOCGPICT failed!\n" );

               return ret;
          }

          p.depth = bpp;
          p.palette = palette;
          //p.contrast = 0x8000;
          //p.colour = 0x6000;

          if (ioctl( fd, VIDIOCSPICT, &p ) < 0) {
               DFBResult ret = errno2dfb( errno );

               PERRORMSG( "DirectFB/core/v4l: VIDIOCSPICT failed!\n" );

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

          if (ioctl( fd, VIDIOCSWIN, &win ) < 0) {
               DFBResult ret = errno2dfb( errno );

               PERRORMSG( "DirectFB/core/v4l: VIDIOCSWIN failed!\n" );

               return ret;
          }
     }

     if (ioctl( fd, VIDIOCCAPTURE, &one ) < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/core/v4l: "
                     "Could not start capturing (VIDIOCCAPTURE failed)!\n" );

          return ret;
     }

     surface_install_listener( surface, v4l_surface_listener,
                               CSN_DESTROY| CSN_FLIP| CSN_SIZEFORMAT| CSN_VIDEO,
                               NULL );
     
     tmp_ctx = ctx; //HACK!!!

     if (callback)
          pthread_create( &thread, NULL, FrameThread, callback );

     return DFB_OK;
}

DFBResult v4l_stop()
{
     if (thread != -1) {
          pthread_cancel( thread );
          pthread_join( thread, NULL );
          thread = -1;
     }

     if (ioctl( fd, VIDIOCCAPTURE, &zero ) < 0) {
          DFBResult ret = errno2dfb( errno );

          PERRORMSG( "DirectFB/core/v4l: "
                     "Could not stop capturing (VIDIOCCAPTURE failed)!\n" );

          return ret;
     }

     return DFB_OK;
}


/*
 * bogus thread to generate callback,
 * because video4linux does not support syncing in overlay mode
 */
void* FrameThread( void *context )
{
     void *ctx = tmp_ctx; //HACK!!!
     DVFrameCallback callback = (DVFrameCallback)context;
     struct timeval tv;

     while (1) {
          tv.tv_sec = 0;
          tv.tv_usec = 20000;
          select( 0, 0, 0, 0, &tv );

          pthread_testcancel();

          callback( ctx );
     }
}


static SLResult v4l_surface_listener( CoreSurface *surface, unsigned int flags,
                                      void *ctx )
{
     v4l_stop();
     return SL_REMOVE;
}

