/*
   (c) Copyright 2001  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,
              Andreas Hundt <andi@convergence.de> and
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

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

#include <pthread.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <misc/util.h>
#include <misc/mem.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/state.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>

#include <gfx/convert.h>

#include <display/idirectfbsurface.h>

#include <openquicktime/openquicktime.h>
#include <openquicktime/colormodels.h>


/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
    int                    ref;       /* reference counter */
    char                  *filename;
    quicktime_t           *file;

    struct {
         int               width;
         int               height;
         float             rate;
         long              length;

         unsigned char    *buffer;
         unsigned char   **lines;

         pthread_t         thread;
         pthread_mutex_t   lock;

         int               playing;
    } video;

    IDirectFBSurface      *destination;
    DFBRectangle           dest_rect;
    DFBRectangle           dest_clip;

    DVFrameCallback        callback;
    void                  *ctx;
} IDirectFBVideoProvider_OpenQuicktime_data;


static void
IDirectFBVideoProvider_OpenQuicktime_Destruct( IDirectFBVideoProvider *thiz )
{
    IDirectFBVideoProvider_OpenQuicktime_data *data;

    data = (IDirectFBVideoProvider_OpenQuicktime_data*)thiz->priv;

    thiz->Stop( thiz );

#ifdef SEGFAULTS_IN_OPENQUICKTIME
    quicktime_close( data->file );
#endif

    DFBFREE( data->video.buffer );
    DFBFREE( data->video.lines );

    pthread_mutex_destroy( &data->video.lock );

    DFBFREE( data->filename );

    DFBFREE( thiz->priv );
    thiz->priv = NULL;

#ifndef DFB_DEBUG
    DFBFREE( thiz );
#endif
}

static DFBResult
IDirectFBVideoProvider_OpenQuicktime_AddRef( IDirectFBVideoProvider *thiz )
{
    INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

    data->ref++;

    return DFB_OK;
}


static DFBResult
IDirectFBVideoProvider_OpenQuicktime_Release( IDirectFBVideoProvider *thiz )
{
    INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

    if (--data->ref == 0) {
        IDirectFBVideoProvider_OpenQuicktime_Destruct( thiz );
    }

    return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_OpenQuicktime_GetCapabilities(
                                           IDirectFBVideoProvider       *thiz,
                                           DFBVideoProviderCapabilities *caps )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

     if (!caps)
          return DFB_INVARG;

     *caps = DVCAPS_BASIC | DVCAPS_SEEK;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_OpenQuicktime_GetSurfaceDescription(
    IDirectFBVideoProvider *thiz, DFBSurfaceDescription  *desc )
{
    INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

    if (!desc)
        return DFB_INVARG;

    desc->flags  = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
    desc->width  = quicktime_video_width( data->file, 0 );
    desc->height = quicktime_video_height( data->file, 0 );
    desc->pixelformat = layers->shared->surface->format;

    return DFB_OK;
}

static void
RGBA_to_RGB332( void *d, void *s, int len )
{
     int    i;
     __u8 *dst = (__u8*) d;
     __u8 *src = (__u8*) s;

     for (i=0; i<len; i++) {
          dst[i] = PIXEL_RGB332( src[0], src[1], src[2] );

          src += 4;
     }
}

static void
RGBA_to_RGB15( void *d, void *s, int len )
{
     int    i;
     __u16 *dst = (__u16*) d;
     __u8  *src = (__u8*) s;

     for (i=0; i<len; i++) {
          dst[i] = PIXEL_RGB15( src[0], src[1], src[2] );

          src += 4;
     }
}

static void
RGBA_to_RGB16( void *d, void *s, int len )
{
     int    i;
     __u16 *dst = (__u16*) d;
     __u8  *src = (__u8*) s;

     for (i=0; i<len; i++) {
          dst[i] = PIXEL_RGB16( src[0], src[1], src[2] );

          src += 4;
     }
}

static void
RGBA_to_RGB24( void *d, void *s, int len )
{
     int   i;
     __u8 *dst = (__u8*) d;
     __u8 *src = (__u8*) s;

     for (i=0; i<len*3; i+=3) {
          dst[i+0] = src[2];
          dst[i+1] = src[1];
          dst[i+2] = src[0];

          src += 4;
     }
}

static void
RGBA_to_RGB32( void *d, void *s, int len )
{
     int    i;
     __u32 *dst = (__u32*) d;
     __u8  *src = (__u8*) s;

     for (i=0; i<len; i++) {
          dst[i] = PIXEL_RGB32( src[0], src[1], src[2] );

          src += 4;
     }
}

static void
RGBA_to_ARGB( void *d, void *s, int len )
{
     int    i;
     __u32 *dst = (__u32*) d;
     __u8  *src = (__u8*) s;

     for (i=0; i<len; i++) {
          dst[i] = PIXEL_ARGB( src[3], src[0], src[1], src[2] );

          src += 4;
     }
}

static void
WriteFrame( IDirectFBVideoProvider_OpenQuicktime_data *data )
{
     unsigned char         *ptr;
     unsigned int           pitch;
     CoreSurface           *surface;
     IDirectFBSurface_data *dst_data;
     int                    i, off_x, off_y;

     dst_data = (IDirectFBSurface_data*) data->destination->priv;
     surface  = dst_data->surface;

     surface_soft_lock( surface, DSLF_WRITE, (void**)&ptr, &pitch, 0 );

     ptr += data->dest_clip.y * pitch +
            data->dest_clip.x * DFB_BYTES_PER_PIXEL (surface->format);

     off_x = data->dest_clip.x - data->dest_rect.x;
     off_y = data->dest_clip.y - data->dest_rect.y;

     switch (surface->format) {
          case DSPF_RGB332:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGBA_to_RGB332( ptr, data->video.lines[i] + off_x * 4,
                                    data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          case DSPF_RGB15:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGBA_to_RGB15( ptr, data->video.lines[i] + off_x * 4,
                                   data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          case DSPF_RGB16:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGBA_to_RGB16( ptr, data->video.lines[i] + off_x * 4,
                                   data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          case DSPF_RGB24:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGBA_to_RGB24( ptr, data->video.lines[i] + off_x * 4,
                                   data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          case DSPF_RGB32:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGBA_to_RGB32( ptr, data->video.lines[i] + off_x * 4,
                                   data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          case DSPF_ARGB:
               for (i=off_y; i<data->dest_clip.h; i++) {
                    RGBA_to_ARGB( ptr, data->video.lines[i] + off_x * 4,
                                  data->dest_clip.w );

                    ptr += pitch;
               }
               break;

          default:
               break;
     }

     surface_unlock( dst_data->surface, 0 );
}

static void*
VideoThread( void *ctx )
{
     IDirectFBVideoProvider_OpenQuicktime_data *data =
          (IDirectFBVideoProvider_OpenQuicktime_data*) ctx;
     struct timeval start, after;
     long  frame_delay;
     long  delay = -1;
     float rate;
     int   drop = 0;
     long  frame;
     long  frames;

     rate = quicktime_frame_rate( data->file, 0 ) / 1000.0;
     frame_delay = (long) (1000 / quicktime_frame_rate( data->file, 0 ));

     frames = quicktime_video_length( data->file, 0 );
     if (frames == 1)
          frames = -1;

     gettimeofday(&start, 0);

     while (data->video.playing) {
          pthread_mutex_lock( &data->video.lock );

          while (drop) {
               quicktime_decode_video( data->file, BC_NONE, NULL, 0 );
               drop--;
          }

          if (quicktime_decode_video( data->file,
                                      BC_RGBA8888, data->video.lines, 0 ))
          {
               pthread_mutex_unlock( &data->video.lock );
               break;
          }

          WriteFrame( data );

          if (data->callback)
               data->callback (data->ctx);

          frame = quicktime_video_position( data->file, 0 );

          gettimeofday (&after, 0);
          delay = (after.tv_sec - start.tv_sec) * 1000 +
                  (after.tv_usec - start.tv_usec) / 1000;
          {
               long cframe = (long) (delay * rate);

               if ( frame < cframe ) {
                    drop = cframe - frame;
                    pthread_mutex_unlock( &data->video.lock );
                    continue;
               }
               else if ( frame == cframe )
                    delay = ((long) ((frame + 1) / rate)) - delay;
               else
                    delay = frame_delay;
          }
          after.tv_sec = 0;
          after.tv_usec = delay * 1000;
          
          pthread_mutex_unlock( &data->video.lock );
          
          select( 0, 0, 0, 0, &after );

          /*  jump to start if arrived at last frame  */
          if (frame == frames) {
               pthread_mutex_lock( &data->video.lock );
               quicktime_seek_start( data->file );
               gettimeofday(&start, 0);
               pthread_mutex_unlock( &data->video.lock );
          }
     }

     return NULL;
}

static DFBResult
IDirectFBVideoProvider_OpenQuicktime_PlayTo( IDirectFBVideoProvider *thiz,
                                        IDirectFBSurface       *destination,
                                        DFBRectangle           *dstrect,
                                        DVFrameCallback         callback,
                                        void                   *ctx )
{
    DFBRectangle            rect;
    IDirectFBSurface_data  *dst_data;

    INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

    if (!destination)
        return DFB_INVARG;

    dst_data = (IDirectFBSurface_data*)destination->priv;
    if (!dst_data)
        return DFB_DEAD;

    /* check if destination format is supported */
    switch (dst_data->surface->format) {
         case DSPF_RGB332:
         case DSPF_RGB15:
         case DSPF_RGB16:
         case DSPF_RGB24:
         case DSPF_RGB32:
         case DSPF_ARGB:
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

    pthread_mutex_lock( &data->video.lock );

    data->dest_rect = rect;

    /* build the clip rectangle */
    if (!rectangle_intersect( &rect, &dst_data->area.current )) {
         pthread_mutex_unlock( &data->video.lock );
         return DFB_INVARG;
    }

    data->dest_clip = rect;

    if (data->destination) {
        data->destination->Release( data->destination );
        data->destination = NULL;     /* FIXME: remove listener */
    }

    destination->AddRef( destination );
    data->destination = destination;   /* FIXME: install listener */

    data->callback       = callback;
    data->ctx            = ctx;
    data->video.playing  = 1;

    if (data->video.thread == -1)
        pthread_create( &data->video.thread, NULL, VideoThread, data );

    pthread_mutex_unlock( &data->video.lock );

    return DFB_OK;
}


static DFBResult
IDirectFBVideoProvider_OpenQuicktime_Stop( IDirectFBVideoProvider *thiz )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

     if (data->video.thread != -1) {
          data->video.playing = 0;
          pthread_join( data->video.thread, NULL );
          data->video.thread = -1;
     }

     pthread_mutex_lock( &data->video.lock );

     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;     /* FIXME: remove listener */
     }

     pthread_mutex_unlock( &data->video.lock );

     return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_OpenQuicktime_SeekTo(
                                              IDirectFBVideoProvider *thiz,
                                              double                  seconds )
{
     long new_pos, old_pos;

     INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

     pthread_mutex_lock( &data->video.lock );

     new_pos = (long) (data->video.rate * seconds);
     old_pos = quicktime_video_position( data->file, 0 );

     if (new_pos == old_pos) {
          pthread_mutex_unlock( &data->video.lock );
          return DFB_OK;
     }

     if (new_pos < old_pos)
          new_pos = quicktime_get_keyframe_before( data->file, new_pos, 0 );
     else
          new_pos = quicktime_get_keyframe_after( data->file, new_pos, 0 );

     quicktime_set_video_position( data->file, new_pos, 0 );

     pthread_mutex_unlock( &data->video.lock );

     return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_OpenQuicktime_GetPos(
                                              IDirectFBVideoProvider *thiz,
                                              double                 *seconds )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

     pthread_mutex_lock( &data->video.lock );

     *seconds = quicktime_video_position( data->file, 0 ) / data->video.rate;

     pthread_mutex_unlock( &data->video.lock );

     return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_OpenQuicktime_GetLength(
                                              IDirectFBVideoProvider *thiz,
                                              double                 *seconds )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

     *seconds = data->video.length / data->video.rate;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_OpenQuicktime_GetColorAdjustment(
                                                  IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

     return DFB_UNSUPPORTED;
}

static DFBResult IDirectFBVideoProvider_OpenQuicktime_SetColorAdjustment(
                                                  IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     INTERFACE_GET_DATA (IDirectFBVideoProvider_OpenQuicktime)

     return DFB_UNSUPPORTED;
}


/* exported symbols */

const char *get_type()
{
     return "IDirectFBVideoProvider";
}

const char *get_implementation()
{
     return "OpenQuicktime";
}

DFBResult Probe( const char *filename )
{
     quicktime_t *q;

#ifdef SEGFAULTS_IN_OPENQUICKTIME
     if (!quicktime_check_sig( (char *) filename ))
          return DFB_UNSUPPORTED;
#endif

     q = quicktime_open( (char*) filename, 1, 0 );
     if (!q)
          return DFB_UNSUPPORTED;

     if (!quicktime_has_video( q )) {
          ERRORMSG( "OpenQuicktime Provider: "
                    "File doesn't contain a video track!\n" );
          quicktime_close( q );
          return DFB_UNSUPPORTED;
     }

     if (!quicktime_supported_video( q, 0 )) {
          ERRORMSG( "OpenQuicktime Provider: "
                    "Video Codec not supported by OpenQuicktime!\n" );
          quicktime_close( q );
          return DFB_UNSUPPORTED;
     }

     if (!quicktime_reads_cmodel( q, BC_RGBA8888, 0 )) {
          ERRORMSG( "OpenQuicktime Provider: "
                    "Only codecs reading RGBA8888 are supported yet!\n" );
          quicktime_close( q );
          return DFB_UNSUPPORTED;
     }

     quicktime_close( q );

     return DFB_OK;
}

DFBResult Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
     int i;
     IDirectFBVideoProvider_OpenQuicktime_data *data;

     /* allocate private data */
     data = (IDirectFBVideoProvider_OpenQuicktime_data *)
          DFBCALLOC( 1, sizeof(IDirectFBVideoProvider_OpenQuicktime_data) );

     thiz->priv = data;


     /* initialize private data */
     data->ref          = 1;
     data->filename     = DFBSTRDUP( filename );

     data->video.thread = -1;

     pthread_mutex_init( &data->video.lock, NULL );


     /* open quicktime file */
     data->file         = quicktime_open( data->filename, 1, 0 );


     /* fetch information about video */
     data->video.width  = quicktime_video_width( data->file, 0 );
     data->video.height = quicktime_video_height( data->file, 0 );

     data->video.rate   = quicktime_frame_rate( data->file, 0 );
     data->video.length = quicktime_video_length( data->file, 0 );


     /* allocate video decoding buffer */
     data->video.buffer = DFBMALLOC( data->video.height *
                                     data->video.width * 4 );

     data->video.lines  = DFBMALLOC( data->video.height *
                                     sizeof(unsigned char*) );

     for (i=0; i<data->video.height; i++)
          data->video.lines[i] = data->video.buffer + data->video.width * 4 * i;


     /* initialize function pointers */
     thiz->AddRef                = IDirectFBVideoProvider_OpenQuicktime_AddRef;
     thiz->Release               = IDirectFBVideoProvider_OpenQuicktime_Release;
     thiz->GetCapabilities       =
          IDirectFBVideoProvider_OpenQuicktime_GetCapabilities;

     thiz->GetSurfaceDescription =
          IDirectFBVideoProvider_OpenQuicktime_GetSurfaceDescription;

     thiz->PlayTo                = IDirectFBVideoProvider_OpenQuicktime_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_OpenQuicktime_Stop;
     thiz->SeekTo                = IDirectFBVideoProvider_OpenQuicktime_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_OpenQuicktime_GetPos;
     thiz->GetLength             =
          IDirectFBVideoProvider_OpenQuicktime_GetLength;

     thiz->GetColorAdjustment    =
          IDirectFBVideoProvider_OpenQuicktime_GetColorAdjustment;

     thiz->SetColorAdjustment    =
          IDirectFBVideoProvider_OpenQuicktime_SetColorAdjustment;

     return DFB_OK;
}

