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
#include <endian.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/soundcard.h>

#include <pthread.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <media/idirectfbvideoprovider.h>

#include <misc/util.h>
#include <misc/mem.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/state.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>

#include <gfx/convert.h>
#include <misc/memcpy.h>

#include <display/idirectfbsurface.h>

#include <libmpeg3.h>

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           const char             *filename );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, Libmpeg3 )

/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
    int                    ref;       /* reference counter */
    char                  *filename;
    mpeg3_t               *file;

    struct {
         int               width;
         int               height;
         float             rate;
         long              length;

         unsigned char    *buffer;

         int               yuv;
         int               playing;
         int               seeked;

         pthread_t         thread;
         pthread_mutex_t   lock;
    } video;

    struct {
         int               available;

         int               bits;
         long              rate;
         int               channels;
         long              length;

         int               fd;
         int               block_size;
         int               samples_per_block;

         int               playing;
         int               seeked;

         pthread_t         thread;
         pthread_mutex_t   lock;
    } audio;

    struct {
         int               supported;
         unsigned char   **lines;
    } rgb;

    struct {
         int               supported;
         unsigned char    *lines[3];
    } yuv;

    IDirectFBSurface      *destination;
    DFBRectangle           dest_rect;
    DFBRectangle           dest_clip;

    DVFrameCallback        callback;
    void                  *ctx;
} IDirectFBVideoProvider_Libmpeg3_data;

/* OSS sound playback */
static DFBResult OpenSound ( IDirectFBVideoProvider_Libmpeg3_data *data );
static DFBResult CloseSound( IDirectFBVideoProvider_Libmpeg3_data *data );


/* interface implementation */

static void
IDirectFBVideoProvider_Libmpeg3_Destruct( IDirectFBVideoProvider *thiz )
{
    IDirectFBVideoProvider_Libmpeg3_data *data;

    data = (IDirectFBVideoProvider_Libmpeg3_data*)thiz->priv;

    thiz->Stop( thiz );

    mpeg3_close( data->file );

    DFBFREE( data->video.buffer );
    DFBFREE( data->rgb.lines );

    pthread_mutex_destroy( &data->video.lock );
    pthread_mutex_destroy( &data->audio.lock );

    DFBFREE( data->filename );

    DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBVideoProvider_Libmpeg3_AddRef( IDirectFBVideoProvider *thiz )
{
    INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

    data->ref++;

    return DFB_OK;
}


static DFBResult
IDirectFBVideoProvider_Libmpeg3_Release( IDirectFBVideoProvider *thiz )
{
    INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

    if (--data->ref == 0) {
        IDirectFBVideoProvider_Libmpeg3_Destruct( thiz );
    }

    return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Libmpeg3_GetCapabilities(
                                           IDirectFBVideoProvider       *thiz,
                                           DFBVideoProviderCapabilities *caps )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

     if (!caps)
          return DFB_INVARG;

     *caps = DVCAPS_BASIC | DVCAPS_SEEK;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Libmpeg3_GetSurfaceDescription(
    IDirectFBVideoProvider *thiz, DFBSurfaceDescription  *desc )
{
    INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

    if (!desc)
        return DFB_INVARG;

    desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
    desc->width       = mpeg3_video_width( data->file, 0 );
    desc->height      = mpeg3_video_height( data->file, 0 );
    desc->pixelformat = DSPF_YUY2;

    return DFB_OK;
}

static void
RGB888_to_RGB332( void *d, void *s, int len )
{
     int    i;
     __u8 *dst = (__u8*) d;
     __u8 *src = (__u8*) s;

     for (i=0; i<len; i++) {
          dst[i] = PIXEL_RGB332( src[0], src[1], src[2] );

          src += 3;
     }
}

static void
RGB888_to_RGB15( void *d, void *s, int len )
{
     int    i;
     __u16 *dst = (__u16*) d;
     __u8  *src = (__u8*) s;

     for (i=0; i<len; i++) {
          dst[i] = PIXEL_RGB15( src[0], src[1], src[2] );

          src += 3;
     }
}

static void
RGB888_to_RGB16( void *d, void *s, int len )
{
     int    i;
     __u16 *dst = (__u16*) d;
     __u8  *src = (__u8*) s;

     for (i=0; i<len; i++) {
          dst[i] = PIXEL_RGB16( src[0], src[1], src[2] );

          src += 3;
     }
}

static void
RGB888_to_RGB24( void *d, void *s, int len )
{
     int   i;
     __u8 *dst = (__u8*) d;
     __u8 *src = (__u8*) s;

     for (i=0; i<len*3; i+=3) {
          dst[i+0] = src[i+2];
          dst[i+1] = src[i+1];
          dst[i+2] = src[i+0];
     }
}

static void
RGB888_to_RGB32( void *d, void *s, int len )
{
     int    i;
     __u32 *dst = (__u32*) d;
     __u8  *src = (__u8*) s;

     for (i=0; i<len; i++) {
          dst[i] = PIXEL_RGB32( src[0], src[1], src[2] );

          src += 3;
     }
}

static void
RGB888_to_ARGB( void *d, void *s, int len )
{
     int    i;
     __u32 *dst = (__u32*) d;
     __u8  *src = (__u8*) s;

     for (i=0; i<len; i++) {
          dst[i] = PIXEL_ARGB( 0xff, src[0], src[1], src[2] );

          src += 3;
     }
}

static void
WriteRGBFrame( IDirectFBVideoProvider_Libmpeg3_data *data )
{
     unsigned char         *ptr;
     unsigned int           pitch;
     CoreSurface           *surface;
     IDirectFBSurface_data *dst_data;
     int                    i, off_x, off_y;

     dst_data = (IDirectFBSurface_data*) data->destination->priv;
     surface  = dst_data->surface;

     dfb_surface_soft_lock( surface, DSLF_WRITE, (void**)&ptr, &pitch, 0 );

     ptr += data->dest_clip.y * pitch +
            data->dest_clip.x * DFB_BYTES_PER_PIXEL (surface->format);

     off_x = data->dest_clip.x - data->dest_rect.x;
     off_y = data->dest_clip.y - data->dest_rect.y;

     switch (surface->format) {
          case DSPF_RGB332:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGB888_to_RGB332( ptr, data->rgb.lines[i] + off_x * 4,
                                      data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          case DSPF_RGB15:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGB888_to_RGB15( ptr, data->rgb.lines[i] + off_x * 4,
                                     data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          case DSPF_RGB16:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGB888_to_RGB16( ptr, data->rgb.lines[i] + off_x * 4,
                                     data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          case DSPF_RGB24:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGB888_to_RGB24( ptr, data->rgb.lines[i] + off_x * 4,
                                     data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          case DSPF_RGB32:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGB888_to_RGB32( ptr, data->rgb.lines[i] + off_x * 4,
                                     data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          case DSPF_ARGB:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGB888_to_ARGB( ptr, data->rgb.lines[i] + off_x * 4,
                                    data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          default:
               break;
     }

     dfb_surface_unlock( dst_data->surface, 0 );
}

static void
WriteYUVFrame( IDirectFBVideoProvider_Libmpeg3_data *data )
{
     __u16                 *dst;
     unsigned int           pitch;
     CoreSurface           *surface;
     IDirectFBSurface_data *dst_data;
     __u8                  *src_y, *src_u, *src_v;
     __u8                  *dst_y, *dst_u, *dst_v;
     int                    x, y, off_x, off_y;

     dst_data = (IDirectFBSurface_data*) data->destination->priv;
     surface  = dst_data->surface;

     dfb_surface_soft_lock( surface, DSLF_WRITE, (void**)&dst, &pitch, 0 );

     src_y  = data->yuv.lines[0];
     src_u  = data->yuv.lines[1];
     src_v  = data->yuv.lines[2];

     off_x  = data->dest_clip.x - data->dest_rect.x;
     off_y  = data->dest_clip.y - data->dest_rect.y;

     src_y +=  off_y * data->video.width + off_x;
     src_u += (off_y / 2) * (data->video.width / 2) + off_x / 2;
     src_v += (off_y / 2) * (data->video.width / 2) + off_x / 2;

     /* this code might not work with offsets not being a multiple of 2 */

     switch (surface->format) {
          case DSPF_I420:
          case DSPF_YV12:
               dst_y  = (__u8*) dst;
               dst_y += data->dest_clip.y * pitch + data->dest_clip.x;

               if (surface->format == DSPF_I420) {
                    dst_u = (__u8*) dst + pitch * data->video.height;
                    dst_v = dst_u + pitch/2 * data->video.height / 2;

                    dst_u += data->dest_clip.y/2 * pitch/2 + data->dest_clip.x/2;
                    dst_v += data->dest_clip.y/2 * pitch/2 + data->dest_clip.x/2;
               }
               else {
                    dst_v = (__u8*) dst + pitch * data->video.height;
                    dst_u = dst_v + pitch/2 * data->video.height / 2;

                    dst_u += data->dest_clip.y/2 * pitch/2 + data->dest_clip.x/2;
                    dst_v += data->dest_clip.y/2 * pitch/2 + data->dest_clip.x/2;
               }

               for (y=0; y<data->dest_clip.h; y++) {
                    dfb_memcpy( dst_y, src_y, data->dest_clip.w );

                    src_y += data->video.width;
                    dst_y += pitch;

                    if (y & 1) {
                         src_u += data->video.width/2;
                         src_v += data->video.width/2;
                         dst_u += pitch/2;
                         dst_v += pitch/2;
                    }
                    else {
                         dfb_memcpy( dst_u, src_u, data->dest_clip.w/2 );
                         dfb_memcpy( dst_v, src_v, data->dest_clip.w/2 );
                    }
               }
               break;

          case DSPF_YUY2:
               dst += data->dest_clip.y * pitch + data->dest_clip.x;

               for (y=0; y<data->dest_clip.h; y++) {
                    for (x=0; x<data->dest_clip.w; x++) {
                         if (x & 1)
                              dst[x] = (src_v[x/2] << 8) | src_y[x];
                         else
                              dst[x] = (src_u[x/2] << 8) | src_y[x];
                    }

                    src_y += data->video.width;

                    if (y & 1) {
                         src_u += data->video.width/2;
                         src_v += data->video.width/2;
                    }

                    dst += pitch/2;
               }
               break;

          case DSPF_UYVY:
               dst += data->dest_clip.y * pitch + data->dest_clip.x;

               for (y=0; y<data->dest_clip.h; y++) {
                    for (x=0; x<data->dest_clip.w; x++) {
                         if (x & 1)
                              dst[x] = (src_y[x] << 8) | src_v[x/2];
                         else
                              dst[x] = (src_y[x] << 8) | src_u[x/2];
                    }

                    src_y += data->video.width;

                    if (y & 1) {
                         src_u += data->video.width/2;
                         src_v += data->video.width/2;
                    }

                    dst += pitch/2;
               }
               break;

          default:
               break;
     }

     dfb_surface_unlock( dst_data->surface, 0 );
}

static void*
VideoThread( void *ctx )
{
     IDirectFBVideoProvider_Libmpeg3_data *data =
          (IDirectFBVideoProvider_Libmpeg3_data*) ctx;

     struct timeval start, after;

     long   frame_delay;
     long   delay = -1;
     double rate;
     int    drop = 0;
     long   frame, start_frame = 0;
     long   frames;

     rate = data->video.rate / 1000.0;
     frame_delay = (long) (1000 / data->video.rate);

     frames = data->video.length;
     if (frames == 1)
          frames = -1;

     data->video.seeked = 1;

     while (data->video.playing) {
          pthread_mutex_lock( &data->video.lock );

          if (!data->video.playing) {
               pthread_mutex_unlock( &data->video.lock );
               break;
          }

          if (data->video.seeked) {
               drop = 0;
               gettimeofday(&start, 0);
               start_frame = mpeg3_get_frame( data->file, 0 );
               data->video.seeked = 0;
          }

          if (drop) {
               mpeg3_drop_frames( data->file, drop, 0 );
               drop = 0;
          }
          else {
               int ret;

               if (data->video.yuv)
                    ret = mpeg3_read_yuvframe( data->file,
                                               data->yuv.lines[0],
                                               data->yuv.lines[1],
                                               data->yuv.lines[2],
                                               0, 0,
                                               data->video.width,
                                               data->video.height, 0 );
               else
                    ret = mpeg3_read_frame( data->file,
                                            data->rgb.lines, 0, 0,
                                            data->video.width,
                                            data->video.height,
                                            data->video.width,
                                            data->video.height,
                                            MPEG3_RGB888, 0 );

               if (ret) {
                    pthread_mutex_unlock( &data->video.lock );
                    break;
               }

               if (data->video.yuv)
                    WriteYUVFrame( data );
               else
                    WriteRGBFrame( data );

               if (data->callback)
                    data->callback (data->ctx);
          }

          frame = mpeg3_get_frame( data->file, 0 );

          gettimeofday (&after, 0);
          delay = (after.tv_sec - start.tv_sec) * 1000 +
                  (after.tv_usec - start.tv_usec) / 1000;
          {
               long cframe = (long) (delay * rate) + start_frame;

               if ( frame < cframe ) {
                    drop = cframe - frame;
                    pthread_mutex_unlock( &data->video.lock );
                    continue;
               }
               else if ( frame == cframe )
                    delay = ((long) ((frame - start_frame + 1) / rate)) - delay;
               else
                    delay = frame_delay;
          }
          after.tv_sec = 0;
          after.tv_usec = delay * 1000;

          pthread_mutex_unlock( &data->video.lock );

          select( 0, 0, 0, 0, &after );

          /* jump to start if arrived at last frame  */
          if (frame == frames) {
               pthread_mutex_lock( &data->video.lock );
               pthread_mutex_lock( &data->audio.lock );
               mpeg3_set_frame( data->file, 0, 0 );
               mpeg3_set_sample( data->file, 0, 0 );
               data->video.seeked = 1;
               data->audio.seeked = 1;
               pthread_mutex_unlock( &data->audio.lock );
               pthread_mutex_unlock( &data->video.lock );
          }
     }

     return NULL;
}

static void*
AudioThread( void *ctx )
{
     IDirectFBVideoProvider_Libmpeg3_data *data =
          (IDirectFBVideoProvider_Libmpeg3_data*) ctx;

     __s16 buffer[data->audio.samples_per_block * data->audio.channels];
     __s16 left[data->audio.samples_per_block];
     __s16 right[data->audio.samples_per_block];

     /* calculate audio position */
     long audio_pos = (long) ((double)mpeg3_get_frame (data->file, 0) *
                              (double)data->audio.rate /
                              (double)data->video.rate);

     /* seek audio */
     mpeg3_set_sample( data->file, audio_pos, 0 );

     data->audio.seeked = 1;

     while (data->audio.playing) {
          pthread_mutex_lock( &data->audio.lock );

          if (!data->audio.playing) {
               pthread_mutex_unlock( &data->audio.lock );
               break;
          }

          if (data->audio.seeked) {
               /* flush buffered audio data */
               ioctl( data->audio.fd, SNDCTL_DSP_RESET, 0 );
               data->audio.seeked = 0;
          }

          if (data->audio.channels == 1) {
               long pos = mpeg3_get_sample( data->file, 0 );

               /* mono */
               if ((pos < 0) ||
                   (pos + data->audio.samples_per_block) >= data->audio.length ||
                   mpeg3_read_audio( data->file, NULL, buffer, 0,
                                     data->audio.samples_per_block, 0 ))
                    memset( buffer, 0, data->audio.block_size );

               write( data->audio.fd, buffer, data->audio.block_size );
          }
          else {
               long pos = mpeg3_get_sample( data->file, 0 );

               /* stereo */
               if ((pos < 0) ||
                   (pos + data->audio.samples_per_block) >= data->audio.length ||
                   mpeg3_read_audio( data->file, NULL, left, 0,
                                     data->audio.samples_per_block, 0 ))
                    memset( buffer, 0, data->audio.block_size );
               else {
                    int i;

                    mpeg3_reread_audio( data->file, NULL, right, 1,
                                        data->audio.samples_per_block, 0 );

                    /* produce interleaved buffer */
                    for (i=0; i<data->audio.samples_per_block; i++) {
                         buffer[i*2+0] = left[i];
                         buffer[i*2+1] = right[i];
                    }
               }

               write( data->audio.fd, buffer, data->audio.block_size );
          }

          pthread_mutex_unlock( &data->audio.lock );
     }

     /* tell sound driver that we stopped playback */
     ioctl( data->audio.fd, SNDCTL_DSP_POST, 0 );

     return NULL;
}

static DFBResult
IDirectFBVideoProvider_Libmpeg3_PlayTo( IDirectFBVideoProvider *thiz,
                                        IDirectFBSurface       *destination,
                                        const DFBRectangle     *dstrect,
                                        DVFrameCallback         callback,
                                        void                   *ctx )
{
     int                    yuv_mode = 0;
     DFBRectangle           rect, dest_rect, dest_clip;
     IDirectFBSurface_data *dst_data;

     INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

     if (!destination)
          return DFB_INVARG;

     dst_data = (IDirectFBSurface_data*)destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     /* check if destination format is supported */
     switch (dst_data->surface->format) {
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_YUY2:
          case DSPF_UYVY:
               if (!data->yuv.supported)
                    return DFB_UNSUPPORTED;

               yuv_mode = 1;
               break;

          case DSPF_RGB332:
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
               if (data->rgb.supported)
                    break;

          default:
               return DFB_UNSUPPORTED;
     }

     /* build the destination rectangle */
     if (dstrect) {
          if (dstrect->w < 1  ||  dstrect->h < 1)
               return DFB_INVARG;

          rect = *dstrect;

          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;
     }
     else
          rect = dst_data->area.wanted;

     /* check destination rect and save it */
     if (rect.w != data->video.width || rect.h != data->video.height)
          return DFB_UNSUPPORTED;

     dest_rect = rect;

     /* build the clip rectangle */
     if (!dfb_rectangle_intersect( &rect, &dst_data->area.current ))
          return DFB_INVARG;

     dest_clip = rect;

     pthread_mutex_lock( &data->video.lock );

     /* release previous destination surface */
     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;     /* FIXME: remove listener */
     }

     /* reference destination surface */
     destination->AddRef( destination );
     data->destination = destination;   /* FIXME: install listener */

     /* store video playback information */
     data->dest_rect      = dest_rect;
     data->dest_clip      = dest_clip;
     data->callback       = callback;
     data->ctx            = ctx;
     data->video.yuv      = yuv_mode;
     data->video.playing  = 1;

     /* start audio playback thread */
     if (data->audio.thread == -1 && data->audio.available) {
          if (OpenSound( data ) == DFB_OK) {
               pthread_mutex_lock( &data->audio.lock );

               data->audio.playing = 1;

               pthread_create( &data->audio.thread, NULL, AudioThread, data );

               pthread_mutex_unlock( &data->audio.lock );
          }
     }

     /* start video playback thread */
     if (data->video.thread == -1)
          pthread_create( &data->video.thread, NULL, VideoThread, data );

     pthread_mutex_unlock( &data->video.lock );

     return DFB_OK;
}


static DFBResult
IDirectFBVideoProvider_Libmpeg3_Stop( IDirectFBVideoProvider *thiz )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );

     /* stop audio thread and close device */
     if (data->audio.thread != -1) {
          data->audio.playing = 0;

          pthread_mutex_unlock( &data->audio.lock );
          pthread_join( data->audio.thread, NULL );
          pthread_mutex_lock( &data->audio.lock );

          data->audio.thread = -1;
          CloseSound( data );
     }

     /* stop video thread */
     if (data->video.thread != -1) {
          data->video.playing = 0;

          pthread_mutex_unlock( &data->video.lock );
          pthread_join( data->video.thread, NULL );
          pthread_mutex_lock( &data->video.lock );

          data->video.thread = -1;
     }

     /* release destination surface */
     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;     /* FIXME: remove listener */
     }

     pthread_mutex_unlock( &data->audio.lock );
     pthread_mutex_unlock( &data->video.lock );

     return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_Libmpeg3_SeekTo(
                                              IDirectFBVideoProvider *thiz,
                                              double                  seconds )
{
     long new_pos, old_pos, audio_pos;

     INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );

     /* calculate new and old video position */
     new_pos = (long) (data->video.rate * seconds);
     old_pos = mpeg3_get_frame( data->file, 0 );

     /* nothing to do? */
     if (new_pos == old_pos) {
          pthread_mutex_unlock( &data->audio.lock );
          pthread_mutex_unlock( &data->video.lock );
          return DFB_OK;
     }

     /* check end of stream */
     if (new_pos >= data->video.length)
          new_pos = 0;

     /* calculate new audio position */
     audio_pos = (long) ((double)new_pos *
                         (double)data->audio.rate /
                         (double)data->video.rate);

     /* seek audio */
     mpeg3_set_sample( data->file, audio_pos, 0 );

     data->audio.seeked = 1;

     pthread_mutex_unlock( &data->audio.lock );

     /* seek video */
     mpeg3_set_frame( data->file, new_pos, 0 );

     data->video.seeked = 1;

     pthread_mutex_unlock( &data->video.lock );

     return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_Libmpeg3_GetPos(
                                              IDirectFBVideoProvider *thiz,
                                              double                 *seconds )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

     pthread_mutex_lock( &data->video.lock );

     *seconds = mpeg3_get_frame( data->file, 0 ) / data->video.rate;

     pthread_mutex_unlock( &data->video.lock );

     return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_Libmpeg3_GetLength(
                                              IDirectFBVideoProvider *thiz,
                                              double                 *seconds )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

     *seconds = data->video.length / data->video.rate;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_Libmpeg3_GetColorAdjustment(
                                                  IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

     return DFB_UNSUPPORTED;
}

static DFBResult IDirectFBVideoProvider_Libmpeg3_SetColorAdjustment(
                                                  IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_Libmpeg3)

     return DFB_UNSUPPORTED;
}


/* exported symbols */

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     mpeg3_t *q;

     if (!mpeg3_check_sig( (char *) ctx->filename ))
          return DFB_UNSUPPORTED;

     q = mpeg3_open( (char*) ctx->filename );
     if (!q)
          return DFB_UNSUPPORTED;

     if (!mpeg3_has_video( q )) {
          ERRORMSG( "Libmpeg3 Provider: "
                    "File doesn't contain a video track!\n" );
          mpeg3_close( q );
          return DFB_UNSUPPORTED;
     }

     mpeg3_close( q );

     return DFB_OK;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
     int i;
     
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBVideoProvider_Libmpeg3)

     /* initialize private data */
     data->ref           = 1;
     data->filename      = DFBSTRDUP( filename );

     data->video.thread  = -1;
     data->audio.thread  = -1;

     pthread_mutex_init( &data->video.lock, NULL );
     pthread_mutex_init( &data->audio.lock, NULL );


     /* open mpeg3 file */
     data->file          = mpeg3_open( data->filename );

     /* fetch information about video */
     data->video.width   = mpeg3_video_width( data->file, 0 );
     data->video.height  = mpeg3_video_height( data->file, 0 );

     data->video.rate    = mpeg3_frame_rate( data->file, 0 );
     data->video.length  = mpeg3_video_frames( data->file, 0 );

     /* check codec format support */
     data->yuv.supported = 1;
     data->rgb.supported = 1;

     /* allocate video decoding buffer */
     data->video.buffer  = DFBMALLOC( data->video.height *
                                      data->video.width * 3 );

     data->rgb.lines     = DFBMALLOC( data->video.height *
                                      sizeof(unsigned char*) );

     for (i=0; i<data->video.height; i++)
          data->rgb.lines[i] = data->video.buffer + data->video.width * 3 * i;

     data->yuv.lines[0] = data->video.buffer;
     data->yuv.lines[1] = data->video.buffer +
                          data->video.width * data->video.height;
     data->yuv.lines[2] = data->video.buffer +
                          data->video.width * data->video.height * 5 / 4;

     /* setup audio playback */
     if (mpeg3_has_audio( data->file )) {
          data->audio.available = 1;
          data->audio.bits      = 16;
          data->audio.channels  = mpeg3_audio_channels( data->file, 0 );
          data->audio.rate      = mpeg3_sample_rate( data->file, 0 );
          data->audio.length    = mpeg3_audio_samples( data->file, 0 );
     }



     /* initialize function pointers */
     thiz->AddRef                = IDirectFBVideoProvider_Libmpeg3_AddRef;
     thiz->Release               = IDirectFBVideoProvider_Libmpeg3_Release;
     thiz->GetCapabilities       =
          IDirectFBVideoProvider_Libmpeg3_GetCapabilities;

     thiz->GetSurfaceDescription =
          IDirectFBVideoProvider_Libmpeg3_GetSurfaceDescription;

     thiz->PlayTo                = IDirectFBVideoProvider_Libmpeg3_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_Libmpeg3_Stop;
     thiz->SeekTo                = IDirectFBVideoProvider_Libmpeg3_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_Libmpeg3_GetPos;
     thiz->GetLength             =
          IDirectFBVideoProvider_Libmpeg3_GetLength;

     thiz->GetColorAdjustment    =
          IDirectFBVideoProvider_Libmpeg3_GetColorAdjustment;

     thiz->SetColorAdjustment    =
          IDirectFBVideoProvider_Libmpeg3_SetColorAdjustment;

     return DFB_OK;
}


/* OSS sound support */

static DFBResult
OpenSound( IDirectFBVideoProvider_Libmpeg3_data *data )
{
     int fd;
     int prof   = APF_NORMAL;
     int format;
     int bytes  = (data->audio.bits + 7) / 8;
     int stereo = (data->audio.channels > 1) ? 1 : 0;
     int rate   = data->audio.rate;

     /* open audio device */
     fd = open( "/dev/dsp", O_WRONLY );
     if (fd < 0) {
          fd = open( "/dev/sound/dsp", O_WRONLY );
          if (fd < 0) {
               PERRORMSG( "Libmpeg3 Provider: Opening both '/dev/dsp' and "
                          "'/dev/sound/dsp' failed!\n" );
               return DFB_IO;
          }
     }

     /* set application profile */
     ioctl( fd, SNDCTL_DSP_PROFILE, &prof );

     /* set format */
     if (data->audio.bits == 8)
          format = AFMT_U8;
     else if (data->audio.bits == 16) {
#if __BYTE_ORDER == __BIG_ENDIAN
          format = AFMT_S16_BE;
#else
          format = AFMT_S16_LE;
#endif
     }
     else {
          ERRORMSG( "Libmpeg3 Provider: "
                    "unexpected sample format (%d bit)!\n", data->audio.bits );
          close( fd );
          return DFB_UNSUPPORTED;                    
     }
     
     if (ioctl( fd, SNDCTL_DSP_SETFMT, &format )) {
          ERRORMSG( "Libmpeg3 Provider: "
                    "failed to set sample format to %d bit!\n", 
                    data->audio.bits );
          close( fd );
          return DFB_UNSUPPORTED;
     }

     /* set mono/stereo */
     if (ioctl( fd, SNDCTL_DSP_STEREO, &stereo ) == -1) {
          ERRORMSG( "Libmpeg3 Provider: Unable to set '%s' mode!\n",
                    (data->audio.channels > 1) ? "stereo" : "mono");
          close( fd );
          return DFB_UNSUPPORTED;
     }

     /* set sample rate */
     if (ioctl( fd, SNDCTL_DSP_SPEED, &rate ) == -1) {
          ERRORMSG( "Libmpeg3 Provider: "
                    "Unable to set sample rate to '%ld'!\n", data->audio.rate );
          close( fd );
          return DFB_UNSUPPORTED;
     }

     /* query block size */
     ioctl( fd, SNDCTL_DSP_GETBLKSIZE, &data->audio.block_size );
     if (data->audio.block_size < 1) {
          ERRORMSG( "Libmpeg3 Provider: "
                    "Unable to query block size of '/dev/dsp'!\n" );
          close( fd );
          return DFB_UNSUPPORTED;
     }

     /* calculate number of samples fitting into block for each channel */
     data->audio.samples_per_block = data->audio.block_size / bytes /
                                     data->audio.channels;

     /* store file descriptor */
     data->audio.fd = fd;

     return DFB_OK;
}

static DFBResult
CloseSound( IDirectFBVideoProvider_Libmpeg3_data *data )
{
     /* flush pending sound data so we don't have to wait */
     ioctl( data->audio.fd, SNDCTL_DSP_RESET, 0 );

     /* close audio device */
     close( data->audio.fd );

     return DFB_OK;
}

