/*
 * Copyright (C) 2005 Claudio Ciccani <klan@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>

#include <direct/types.h>
#include <direct/list.h>
#include <direct/messages.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <directfb.h>

#include <idirectfb.h>

#include <media/idirectfbvideoprovider.h>

#include <core/surfaces.h>

#include <display/idirectfbsurface.h>

#include <gfx/clip.h>
#include <gfx/convert.h>

#ifdef HAVE_FUSIONSOUND
# include <fusionsound.h>
#endif

#include <swfdec.h>
#include <swfdec_buffer.h>
#include <swfdec_render.h>


static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer  );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, Swfdec )


typedef struct {
     DirectLink       link;
     SwfdecBuffer    *buffer;
     __s64            pts;
} FrameLink;

typedef struct {
     FrameLink       *list;
     pthread_mutex_t  lock;
     int              count;
} FrameQueue;

typedef struct {
     int                    ref;

     IDirectFBDataBuffer   *buffer;
     
     SwfdecDecoder         *decoder;

     int                    width;
     int                    height;
     double                 rate;
     unsigned int           interval;
     double                 speed;
     float                  volume;
     
     IDirectFBSurface      *dest;
     IDirectFBSurface_data *dest_data;
     DFBRectangle           rect;
     
#ifdef HAVE_FUSIONSOUND
     IFusionSound          *sound;
     IFusionSoundStream    *stream;
     IFusionSoundPlayback  *playback;
#endif  
    
     DFBVideoProviderStatus status;
     
     struct {
          DirectThread     *thread;
          pthread_mutex_t   lock;
          
          double            seek;
     } input;
     
     struct {
          DirectThread     *thread;
          pthread_mutex_t   lock;
          pthread_cond_t    cond;
                   
          __s64             pts;
          
          bool              seeked;

          FrameQueue        queue;
     } video;
     
     struct {
          DirectThread     *thread;
          pthread_mutex_t   lock;
          
          __s64             pts;
          
          bool              seeked;

          FrameQueue        queue;
     } audio;

     int                    mouse_x;
     int                    mouse_y;
     int                    mouse_btn;
     
     DVFrameCallback        callback;
     void                  *ctx;
} IDirectFBVideoProvider_Swfdec_data;


/*****************************************************************************/

static inline long long usec( void )
{
     struct timeval t;
     gettimeofday( &t, NULL );
     return t.tv_sec * 1000000ll + t.tv_usec;
}

/*****************************************************************************/

static void
add_frame( FrameQueue *queue, SwfdecBuffer *buffer, __s64 pts )
{
     FrameLink *f;
     
     f = D_CALLOC( 1, sizeof(FrameLink) );
     if (!f) {
          D_OOM();
          return;
     }
     
     f->buffer = buffer;
     f->pts    = pts;
     
     pthread_mutex_lock( &queue->lock );
     
     direct_list_append( (DirectLink**)&queue->list, &f->link );
     queue->count++;
     
     pthread_mutex_unlock( &queue->lock );
}

static bool
get_frame( FrameQueue *queue, SwfdecBuffer **ret_buffer, __s64 *ret_pts )
{
     FrameLink *f;
     
     pthread_mutex_lock( &queue->lock );
     
     f = queue->list;
     if (f) {
          direct_list_remove( (DirectLink**)&queue->list, &f->link );
          queue->count--;
          
          if (ret_buffer)
               *ret_buffer = f->buffer;
          if (ret_pts)
               *ret_pts = f->pts;
          
          D_FREE( f );
     }
     
     pthread_mutex_unlock( &queue->lock );
     
     return (f != NULL);
}    
     
static void
flush_frames( FrameQueue *queue )
{
     FrameLink *f;
     
     pthread_mutex_lock( &queue->lock );

     for (f = queue->list; f;) {
          FrameLink *next = (FrameLink*) f->link.next;

          direct_list_remove( (DirectLink**)&queue->list, &f->link );
          swfdec_buffer_unref( f->buffer );
          D_FREE( f );

          f = next;
     }
     
     queue->list = NULL;
     queue->count = 0;
     
     pthread_mutex_unlock( &queue->lock );
}

/*****************************************************************************/

static void*
SwfInput( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_Swfdec_data *data   = arg;
     IDirectFBDataBuffer                *buffer = data->buffer;
     __s64                               pts    = 0;
     unsigned int                        q_max;
     
     while (!direct_thread_is_canceled( self )) {
          DFBResult     ret;
          unsigned char tmp[4096];
          unsigned int  len = 0;
          
          ret = buffer->WaitForDataWithTimeout( buffer, sizeof(tmp), 0, 1 );
          if (ret == DFB_TIMEOUT)
               continue;
          
          while ((ret = buffer->GetData( buffer,
                                         sizeof(tmp), tmp, &len )) == DFB_OK) {
               void *buf;

               buf = malloc( len );
               if (!buf) {
                    D_OOM();
                    return (void*)1;
               }

               direct_memcpy( buf, tmp, len );

               pthread_mutex_lock( &data->input.lock );
               
               swfdec_decoder_add_data( data->decoder, buf, len );
               if (swfdec_decoder_parse( data->decoder ) == SWF_EOF) {
                    pthread_mutex_unlock( &data->input.lock ); 
                    ret = DFB_EOF;
                    break;
               }
               
               pthread_mutex_unlock( &data->input.lock );

               direct_thread_testcancel( self );
          }

          if (ret == DFB_EOF) {
               pthread_mutex_lock( &data->input.lock );
               swfdec_decoder_eof( data->decoder );
               swfdec_decoder_parse( data->decoder );
               pthread_mutex_unlock( &data->input.lock );
               break;
          }
     }

     q_max = MAX( (int)data->rate/3, 2 );
     
     while (!direct_thread_is_canceled( self )) {
          SwfdecBuffer *buffer;
          DFBRectangle  rect;
          int           w, h;
                   
          pthread_mutex_lock( &data->input.lock );
          
          if (data->input.seek != -1) {
#ifdef SWFDEC_HAS_SEEK
               swfdec_render_seek( data->decoder, data->input.seek*data->rate );
               pthread_mutex_lock( &data->video.lock );
               pthread_mutex_lock( &data->audio.lock );
               flush_frames( &data->video.queue );
               flush_frames( &data->audio.queue );
               data->video.seeked = true;
               data->audio.seeked = true;
               pts = data->input.seek * 1000000ll;
               if (data->video.thread)
                    pthread_cond_signal( &data->video.cond );
               pthread_mutex_unlock( &data->audio.lock );
               pthread_mutex_unlock( &data->video.lock );
#endif
               data->input.seek = -1;
          }
          else if (data->video.queue.count >= q_max ||
                   data->audio.queue.count >= q_max) 
          {
               pthread_mutex_unlock( &data->input.lock );
               usleep( 100 );
               continue;
          }
          
          swfdec_render_iterate( data->decoder );
           
#ifdef HAVE_FUSIONSOUND
          if (data->stream) {
               buffer = swfdec_render_get_audio( data->decoder );
               add_frame( &data->audio.queue, buffer, pts );
          }
#endif
          swfdec_decoder_get_image_size( data->decoder, &w, &h );
          rect = (data->rect.w == 0)
                 ? data->dest_data->area.wanted : data->rect;
          
          if (w != rect.w || h != rect.h) {
               swfdec_decoder_set_image_size( data->decoder, rect.w, rect.h );
               w = rect.w;
               h = rect.h;
          }
          
          buffer = swfdec_render_get_image( data->decoder );
          if (buffer)
               buffer->length = (h << 16) | (w & 0xffff);
          add_frame( &data->video.queue, buffer, pts );
          
          pts += data->interval;
          
          pthread_mutex_unlock( &data->input.lock );
     }

     return (void*)0;
}

static void
SwfPutImage( CoreSurface  *surface,
             DFBRectangle *current,
             DFBRectangle *rect,
             SwfdecBuffer *source )
{
     DFBRegion     clip;
     DFBRectangle  srect;
     __u8         *D;
     __u32        *S;
     __u8         *dst, *src;
     int           pitch;
     int           sw  = source->length & 0xffff;
     int           sh  = source->length >> 16;
     int           w, h, n;

     dfb_region_from_rectangle( &clip, current );
     
     srect = (DFBRectangle) {0, 0, MIN(rect->w,sw), MIN(rect->h,sh)};
     dfb_clip_blit( &clip, &srect, &rect->x, &rect->y );
     if (srect.w < 1 || srect.h < 1)
          return;
     
     if (dfb_surface_soft_lock( surface, DSLF_WRITE,
                                (void*)&dst, &pitch, 0 ) != DFB_OK)
          return;
   
     dst += pitch * rect->y +
            DFB_BYTES_PER_LINE( surface->format, rect->x );
     src  = source->data + srect.y * sw * 4 + srect.x * 4;
     
     for (h = srect.h; h; h--) {
          D = (__u8 *) dst;
          S = (__u32*) src;
          w = srect.w;
          
          switch (surface->format) {
               case DSPF_RGB332:
                    while (w--) {
                         *D++ = RGB32_TO_RGB332( *S );
                         S++;
                    }
                    break;
               case DSPF_ARGB4444:
                    if ((__u32)D & 2) {
                         *((__u16*)D) = RGB32_TO_ARGB4444( *S );
                         D += 2; S++;
                         w--;
                    }

                    for (n = w/2; n--;) {
                         __u32 p0, p1;
                         p0 = RGB32_TO_ARGB4444( *(S+0) );
                         p1 = RGB32_TO_ARGB4444( *(S+1) );
#ifdef WORDS_BIGENDIAN
                         *((__u32*)D) = (p0 << 16) | p1;
#else
                         *((__u32*)D) = p0 | (p1 << 16);
#endif
                         D += 4; S += 2;
                    }

                    if (w & 1)
                         *((__u16*)D) = RGB32_TO_ARGB4444( *S );
                    break;
               case DSPF_ARGB2554: 
                    if ((__u32)D & 2) {
                         *((__u16*)D) = RGB32_TO_ARGB2554( *S );
                         D += 2; S++;
                         w--;
                    }

                    for (n = w/2; n--;) {
                         __u32 p0, p1;
                         p0 = RGB32_TO_ARGB2554( *(S+0) );
                         p1 = RGB32_TO_ARGB2554( *(S+1) );
#ifdef WORDS_BIGENDIAN
                         *((__u32*)D) = (p0 << 16) | p1;
#else
                         *((__u32*)D) = p0 | (p1 << 16);
#endif
                         D += 4; S += 2;
                    }

                    if (w & 1)
                         *((__u16*)D) = RGB32_TO_ARGB2554( *S );
                    break;
               case DSPF_ARGB1555:
                    if ((__u32)D & 2) {
                         *((__u16*)D) = RGB32_TO_ARGB1555( *S );
                         D += 2; S++;
                         w--;
                    }

                    for (n = w/2; n--;) {
                         __u32 p0, p1;
                         p0 = RGB32_TO_ARGB1555( *(S+0) );
                         p1 = RGB32_TO_ARGB1555( *(S+1) );
#ifdef WORDS_BIGENDIAN
                         *((__u32*)D) = (p0 << 16) | p1;
#else
                         *((__u32*)D) = p0 | (p1 << 16);
#endif
                         D += 4; S += 2;
                    }

                    if (w & 1)
                         *((__u16*)D) = RGB32_TO_ARGB1555( *S );
                    break;
               case DSPF_RGB16:
                    if ((__u32)D & 2) {
                         *((__u16*)D) = RGB32_TO_RGB16( *S );
                         D += 2; S++;
                         w--;
                    }

                    for (n = w/2; n--;) {
                         __u32 p0, p1;
                         p0 = RGB32_TO_RGB16( *(S+0) );
                         p1 = RGB32_TO_RGB16( *(S+1) );
#ifdef WORDS_BIGENDIAN
                         *((__u32*)D) = (p0 << 16) | p1;
#else
                         *((__u32*)D) = p0 | (p1 << 16);
#endif
                         D += 4; S += 2;
                    }

                    if (w & 1)
                         *((__u16*)D) = RGB32_TO_RGB16( *S );
                    break;
               case DSPF_RGB24:
                    while (w--) {
                         *(D+0) = (*S      ) & 0xff;
                         *(D+1) = (*S >>  8) & 0xff;
                         *(D+2) = (*S >> 16) & 0xff;
                         D += 3; S++;
                    }
                    break;
               case DSPF_RGB32:
               case DSPF_ARGB:
                    direct_memcpy( dst, src, w*4 );
                    break;
               case DSPF_AiRGB:
                    while (w--) {
                         *((__u32*)D) = *S++ ^ 0xff000000;
                         D += 4;
                    }
                    break;
               default:
                    break;
          }
          
          dst += pitch;
          src += sw << 2;
     }
     
     dfb_surface_unlock( surface, 0 );
}

static void*
SwfVideo( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_Swfdec_data *data = arg;
     
     while (data->status == DVSTATE_PLAY) {
          SwfdecBuffer *buffer;
          long long     time;
          
          time = usec();
          
          direct_thread_testcancel( self );
          
          pthread_mutex_lock( &data->video.lock );
               
          if (!get_frame( &data->video.queue, &buffer, &data->video.pts )) {
               pthread_mutex_unlock( &data->video.lock );
               usleep( 0 );
               continue;
          }

          if (data->video.seeked) {
               /* nothing to do */
               data->video.seeked = false;
          }
          
          if (buffer) {
               DFBRectangle rect;
              
               rect = (data->rect.w == 0)
                      ? data->dest_data->area.wanted : data->rect;
               
               SwfPutImage( data->dest_data->surface, 
                           &data->dest_data->area.current, &rect, buffer );
                                 
               swfdec_buffer_unref( buffer );
                    
               if (data->callback)
                    data->callback( data->ctx );
          }

          if (!data->speed) {
               /* paused */
               pthread_cond_wait( &data->video.cond, &data->video.lock );
          }
          else {
               struct timespec t;
               
               time += data->interval;
               if (data->audio.thread)
                    time += data->video.pts - data->audio.pts;
               
               t.tv_sec  = (time / 1000000ll);
               t.tv_nsec = (time % 1000000ll) * 1000;
               
               pthread_cond_timedwait( &data->video.cond,
                                       &data->video.lock, &t );
          }

          pthread_mutex_unlock( &data->video.lock );
     }
  
     return (void*)0;
}

#ifdef HAVE_FUSIONSOUND
static void*
SwfAudio( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_Swfdec_data *data = arg;

     while (data->status == DVSTATE_PLAY) {
          SwfdecBuffer *buffer;
          int           delay = 0;
          __s64         pts;
          
          direct_thread_testcancel( self );
          
          pthread_mutex_lock( &data->audio.lock );
          
          if (!data->speed || !get_frame( &data->audio.queue, &buffer, &pts )) {
               pthread_mutex_unlock( &data->audio.lock );
               usleep( 0 );
               continue;
          }

          if (data->audio.seeked) {
               data->stream->Flush( data->stream );
               data->audio.seeked = false;
          }

          data->stream->GetPresentationDelay( data->stream, &delay );
          data->audio.pts = pts - delay * 1000ll;
          
          pthread_mutex_unlock( &data->audio.lock );
         
          if (data->speed) {
               if (buffer) {
                    data->stream->Write( data->stream, 
                                         buffer->data, buffer->length/4 );
               }
               else {
                    int   len = 44100/data->rate;
                    __s16 buf[len*2];
                    memset( buf, 0, sizeof(buf) );
                    data->stream->Write( data->stream, buf, len );
               }
          }

          if (buffer)
               swfdec_buffer_unref( buffer );
                    
     }

     return (void*)0;
}
#endif

/*****************************************************************************/

static void
IDirectFBVideoProvider_Swfdec_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Swfdec_data *data = thiz->priv;
     
     if (data->input.thread) {
          direct_thread_cancel( data->input.thread );
          direct_thread_join( data->input.thread );
          direct_thread_destroy( data->input.thread );
     }
     
     if (data->video.thread) { 
          direct_thread_cancel( data->video.thread );
          pthread_cond_signal( &data->video.cond );
          direct_thread_join( data->video.thread );
          direct_thread_destroy( data->video.thread );
     }
     
     if (data->audio.thread) {
          direct_thread_cancel( data->audio.thread );
          direct_thread_join( data->audio.thread );
          direct_thread_destroy( data->audio.thread );
     }
       
#ifdef HAVE_FUSIONSOUND
     if (data->playback)
          data->playback->Release( data->playback );
          
     if (data->stream)
          data->stream->Release( data->stream );
     
     if (data->sound)
          data->sound->Release( data->sound );
#endif
     
     if (data->dest)
          data->dest->Release( data->dest );

     if (data->buffer)
          data->buffer->Release( data->buffer );

     if (data->decoder)
          swfdec_decoder_free( data->decoder );
   
     flush_frames( &data->video.queue );
     flush_frames( &data->audio.queue );
     pthread_mutex_destroy( &data->video.queue.lock );
     pthread_mutex_destroy( &data->audio.queue.lock );            

     pthread_cond_destroy( &data->video.cond );
     pthread_mutex_destroy( &data->video.lock );
     pthread_mutex_destroy( &data->audio.lock );
     pthread_mutex_destroy( &data->input.lock );
     
     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBVideoProvider_Swfdec_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (--data->ref == 0)
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetCapabilities( IDirectFBVideoProvider       *thiz,
                                               DFBVideoProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!caps)
          return DFB_INVARG;
          
     *caps = DVCAPS_BASIC | DVCAPS_SCALE | DVCAPS_SPEED | DVCAPS_INTERACTIVE;
#ifdef SWFDEC_HAS_SEEK
     *caps |= DVCAPS_SEEK;
#endif
#ifdef HAVE_FUSIONSOUND
     if (data->playback)
          *caps |= DVCAPS_VOLUME;
#endif
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetSurfaceDescription( IDirectFBVideoProvider *thiz,
                                                     DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!desc)
          return DFB_INVARG;
          
     desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->width       = data->width;
     desc->height      = data->height;
     desc->pixelformat = DSPF_RGB32;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetStreamDescription( IDirectFBVideoProvider *thiz,
                                                    DFBStreamDescription   *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!desc)
          return DFB_INVARG;
     
     desc->caps = DVSCAPS_VIDEO | DVSCAPS_AUDIO; /* assume we have audio */

     snprintf( desc->video.encoding,
               DFB_STREAM_DESC_ENCODING_LENGTH, "Shockwave Flash" );
     desc->video.framerate = data->rate;
     desc->video.aspect    = (double)data->width / (double) data->height;
     desc->video.bitrate   = 0;

     snprintf( desc->audio.encoding,
               DFB_STREAM_DESC_ENCODING_LENGTH, "MP3" );
     desc->audio.samplerate = 44100;
     desc->audio.channels   = 2;
     desc->audio.bitrate    = 0;

     desc->title[0] = desc->author[0] = desc->album[0]   =
     desc->year     = desc->genre[0]  = desc->comment[0] = 0;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_PlayTo( IDirectFBVideoProvider *thiz,
                                      IDirectFBSurface       *dest,
                                      const DFBRectangle     *dest_rect,
                                      DVFrameCallback         callback,
                                      void                   *ctx )
{
     IDirectFBSurface_data *dest_data;
     DFBRectangle           rect      = { 0, 0, 0, 0 };
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!dest)
          return DFB_INVARG;

     if (!dest->priv)
          return DFB_DESTROYED;
          
     dest_data = dest->priv;
     
     switch (dest_data->surface->format) {
          case DSPF_RGB332:
          case DSPF_ARGB4444:
          case DSPF_ARGB2554:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_AiRGB:
               break;
          default:
               D_DEBUG( "IDirectFBVideoProvider_Swf: "
                        "unsupported destination pixelformat.\n" );
               return DFB_UNSUPPORTED;
     }
     
     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
         
          rect    = *dest_rect;
          rect.x += dest_data->area.wanted.x;
          rect.y += dest_data->area.wanted.y;
     }

     pthread_mutex_lock( &data->input.lock );
     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );
     
     if (data->dest)
          data->dest->Release( data->dest );
     dest->AddRef( dest );
     
     data->dest      = dest;
     data->dest_data = dest_data;
     data->rect      = rect;
     data->callback  = callback;
     data->ctx       = ctx;
     
     data->status = DVSTATE_PLAY;
     
     if (!data->input.thread) {
          data->input.thread = direct_thread_create( DTT_DEFAULT, SwfInput,
                                                    (void*)data, "Swf Input" );
     }
     
     if (!data->video.thread) {
          data->video.thread = direct_thread_create( DTT_DEFAULT, SwfVideo,
                                                    (void*)data, "Swf Video" );
     }
 
#ifdef HAVE_FUSIONSOUND
     if (!data->audio.thread && data->stream) {
          data->audio.thread = direct_thread_create( DTT_DEFAULT, SwfAudio,
                                                     (void*)data, "Swf Audio" );
     }
#endif
 
     pthread_mutex_unlock( &data->video.lock );
     pthread_mutex_unlock( &data->audio.lock );
     pthread_mutex_unlock( &data->input.lock );
          
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (data->status == DVSTATE_STOP)
          return DFB_OK;
     
     pthread_mutex_lock( &data->input.lock );
 
     data->status = DVSTATE_STOP;
     
     if (data->video.thread) {
          if (!data->speed) {
               pthread_mutex_lock( &data->video.lock );
               pthread_cond_signal( &data->video.cond );
               pthread_mutex_unlock( &data->video.lock );
          }
          direct_thread_join( data->video.thread );
          direct_thread_destroy( data->video.thread );
          data->video.thread = NULL;
     }
     
     if (data->audio.thread) {
          direct_thread_join( data->audio.thread );
          direct_thread_destroy( data->audio.thread );
          data->audio.thread = NULL;
     }
     
     pthread_mutex_unlock( &data->input.lock );
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetStatus( IDirectFBVideoProvider *thiz,
                                         DFBVideoProviderStatus *status )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!status)
          return DFB_INVARG;

     *status = data->status;

     return DFB_OK;
}

#ifdef SWFDEC_HAS_SEEK
static DFBResult
IDirectFBVideoProvider_Swfdec_SeekTo( IDirectFBVideoProvider *thiz,
                                      double                  seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (seconds < 0.0)
          return DFB_INVARG;
    
     pthread_mutex_lock( &data->input.lock );
     data->input.seek = seconds;
     pthread_mutex_unlock( &data->input.lock );

     return DFB_OK;
}
#endif

static DFBResult
IDirectFBVideoProvider_Swfdec_GetPos( IDirectFBVideoProvider *thiz,
                                      double                 *seconds )
{
     int frame = 0;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!seconds)
          return DFB_INVARG;

     frame = swfdec_render_get_frame_index( data->decoder ) + 1;
     *seconds = (double)frame / data->rate;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetLength( IDirectFBVideoProvider *thiz,
                                         double                 *seconds )
{
     int frames = 0;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
          
     if (!seconds)
          return DFB_INVARG;
     
     swfdec_decoder_get_n_frames( data->decoder, &frames );
     *seconds = (double)frames / data->rate;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetColorAdjustment( IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!adj)
          return DFB_INVARG;
          
     adj->flags = DCAF_NONE;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SetColorAdjustment( IDirectFBVideoProvider   *thiz,
                                                  const DFBColorAdjustment *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!adj)
          return DFB_INVARG;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SendEvent( IDirectFBVideoProvider *thiz,
                                         const DFBEvent         *evt )
{
     bool mouse_event = false;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!evt)
          return DFB_INVARG;

     switch (evt->clazz) {
          case DFEC_INPUT:
               switch (evt->input.type) {
                    case DIET_BUTTONPRESS:
                         switch (evt->input.button) {
                              case DIBI_LEFT:
                                   data->mouse_btn = 1;
                                   mouse_event = true;
                                   break;
                              case DIBI_MIDDLE:
                                   data->mouse_btn = 2;
                                   mouse_event = true;
                                   break;
                              case DIBI_RIGHT:
                                   data->mouse_btn = 3;
                                   mouse_event = true;
                                   break;
                              default:
                                   break;
                         }
                         break; 
                    case DIET_BUTTONRELEASE:
                         switch (evt->input.button) {
                              case DIBI_LEFT:
                              case DIBI_MIDDLE:
                              case DIBI_RIGHT:
                                   data->mouse_btn = 0;
                                   mouse_event = true;
                                   break;
                              default:
                                   break;
                         }
                         break;
                    case DIET_AXISMOTION:
                         switch (evt->input.axis) {
                              case DIAI_X:
                                   if (evt->input.flags & DIEF_AXISREL)
                                        data->mouse_x += evt->input.axisrel;
                                   if (evt->input.flags & DIEF_AXISABS)
                                        data->mouse_x = evt->input.axisabs;
                                   mouse_event = true;
                                   break;
                              case DIAI_Y:
                                   if (evt->input.flags & DIEF_AXISREL)
                                        data->mouse_y += evt->input.axisrel;
                                   if (evt->input.flags & DIEF_AXISABS)
                                        data->mouse_y = evt->input.axisabs;
                                   mouse_event = true;
                                   break;
                              default:
                                   break;
                         }
                         break;
                    default:
                         break;
               }
               break;

          case DFEC_WINDOW:
               switch (evt->window.type) {
                    case DWET_BUTTONDOWN:
                         switch (evt->window.button) {
                              case DIBI_LEFT:
                                   data->mouse_btn = 1;
                                   mouse_event = true;
                                   break;
                              case DIBI_MIDDLE:
                                   data->mouse_btn = 2;
                                   mouse_event = true;
                                   break;
                              case DIBI_RIGHT:
                                   data->mouse_btn = 3;
                                   mouse_event = true;
                                   break;
                              default:
                                   break;
                         }
                         if (mouse_event) {
                              data->mouse_x = evt->window.x;
                              data->mouse_y = evt->window.y;
                         }
                         break;
                    case DWET_BUTTONUP:
                         switch (evt->window.button) {
                              case DIBI_LEFT:
                              case DIBI_MIDDLE:
                              case DIBI_RIGHT:
                                   data->mouse_x = evt->window.x;
                                   data->mouse_y = evt->window.y;
                                   data->mouse_btn = 0;
                                   mouse_event = true;
                                   break;
                              default:
                                   break;
                         }
                         break;
                    case DWET_ENTER:
                    case DWET_MOTION:
                         data->mouse_x = evt->window.x;
                         data->mouse_y = evt->window.y;
                         mouse_event = true;
                         break;
                    case DWET_LEAVE:
                         data->mouse_x = -1;
                         data->mouse_y = -1;
                         data->mouse_btn = 0;
                         mouse_event = true;
                         break;
                    default:
                         break;
               }
               break;

          default:
               break;
     }

     if (mouse_event) {
          swfdec_decoder_set_mouse( data->decoder,
                                    data->mouse_x,
                                    data->mouse_y, 
                                    data->mouse_btn );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SetSpeed( IDirectFBVideoProvider *thiz,
                                        double                  multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (multiplier < 0.0)
          return DFB_INVARG;
          
     if (multiplier > 64.0)
          return DFB_UNSUPPORTED;
          
     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );
     
     if (multiplier)
          data->interval = 1000000.0/(data->rate*multiplier);
     pthread_cond_signal( &data->video.cond );
     
#ifdef HAVE_FUSIONSOUND
     if (data->playback && multiplier >= 0.01)
          data->playback->SetPitch( data->playback, multiplier );
#endif

     data->speed = multiplier;
     
     pthread_mutex_unlock( &data->audio.lock );
     pthread_mutex_unlock( &data->video.lock );    
     
     return DFB_OK;
}

static DFBResult 
IDirectFBVideoProvider_Swfdec_GetSpeed( IDirectFBVideoProvider *thiz,
                                        double                 *ret_multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!ret_multiplier)
          return DFB_INVARG;
          
     *ret_multiplier = data->speed;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SetVolume( IDirectFBVideoProvider *thiz,
                                         float                   level )
{
     DFBResult ret = DFB_UNSUPPORTED;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (level < 0.0)
          return DFB_INVARG;
          
#ifdef HAVE_FUSIONSOUND
     if (data->playback) {
          ret = data->playback->SetVolume( data->playback, level );
          if (ret == DFB_OK)
               data->volume = level;
     }
#endif

     return ret;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetVolume( IDirectFBVideoProvider *thiz,
                                         float                  *ret_level )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!ret_level)
          return DFB_INVARG;

     *ret_level = data->volume;

     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     if ((ctx->header[0] == 'F' || ctx->header[0] == 'C') &&
         (ctx->header[1] == 'W' && ctx->header[2] == 'S'))
          return DFB_OK;
    
     return DFB_UNSUPPORTED;
}

#define PRELOAD_SIZE  512

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     DFBResult  ret;
     void      *buf;
      
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_Swfdec )
     
     data->ref    = 1;
     data->status = DVSTATE_STOP;
     data->buffer = buffer;
     data->speed  = 1.0;
     data->volume = 1.0;
     
     buffer->AddRef( buffer );
     
     buf = malloc( PRELOAD_SIZE );
     if (!buf) {
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );
          return D_OOM();
     }

     buffer->WaitForData( buffer, PRELOAD_SIZE );
     ret = buffer->GetData( buffer, PRELOAD_SIZE, buf, NULL );
     if (ret) {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "error fetching %d bytes from buffer!", PRELOAD_SIZE );
          free( buf );
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );
          return ret;
     }

     swfdec_init();

     data->decoder = swfdec_decoder_new();
     if (!data->decoder) {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "couldn't create swf decoder.\n" );
          free( buf );
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );
          return DFB_INIT;
     }

     swfdec_decoder_set_colorspace( data->decoder, SWF_COLORSPACE_RGB888 );
    
     swfdec_decoder_add_data( data->decoder, buf, PRELOAD_SIZE );
     if (swfdec_decoder_parse( data->decoder ) == SWF_ERROR) {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "swfdec_decoder_parse() failed!\n" );
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     swfdec_decoder_get_image_size( data->decoder,
                                    &data->width, &data->height );
     swfdec_decoder_get_rate( data->decoder, &data->rate );
     
     data->width    = data->width  ? : 1;
     data->height   = data->height ? : 1;
     data->rate     = data->rate   ? : 1;
     data->interval = 1000000.0/data->rate;

     D_DEBUG( "IDirectFBVideoProvider_Swfdec: "
              "width:%d height:%d rate:%.4f.\n",
              data->width, data->height, data->rate );

#ifdef HAVE_FUSIONSOUND
     if (idirectfb_singleton->GetInterface( idirectfb_singleton,
                                            "IFusionSound", 0, 0,
                                            (void**)&data->sound ) == DFB_OK)
     {
          FSStreamDescription dsc;

          dsc.flags        = FSSDF_BUFFERSIZE   | FSSDF_CHANNELS  |
                             FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;
          dsc.buffersize   = 44100.0*120/1000;
          dsc.channels     = 2;
          dsc.samplerate   = 44100;
          dsc.sampleformat = FSSF_S16;

          if (data->sound->CreateStream( data->sound, &dsc,
                                         &data->stream ) != DFB_OK) {
               D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                        "IFusionSound::CreateStream() failed!\n" );
               data->sound->Release( data->sound );
               data->sound = NULL;
          }
          
          data->stream->GetPlayback( data->stream, &data->playback );
     }
     else {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "failed to get FusionSound interface!\n" );
     }
#endif     
 
     pthread_mutex_init( &data->input.lock, NULL );
     pthread_mutex_init( &data->video.lock, NULL );
     pthread_mutex_init( &data->audio.lock, NULL );
     pthread_mutex_init( &data->video.queue.lock, NULL );
     pthread_mutex_init( &data->audio.queue.lock, NULL );
     pthread_cond_init ( &data->video.cond, NULL );
     
     thiz->AddRef                = IDirectFBVideoProvider_Swfdec_AddRef;
     thiz->Release               = IDirectFBVideoProvider_Swfdec_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_Swfdec_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_Swfdec_GetSurfaceDescription;
     thiz->GetStreamDescription  = IDirectFBVideoProvider_Swfdec_GetStreamDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_Swfdec_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_Swfdec_Stop;
     thiz->GetStatus             = IDirectFBVideoProvider_Swfdec_GetStatus;
#ifdef SWFDEC_HAS_SEEK
     thiz->SeekTo                = IDirectFBVideoProvider_Swfdec_SeekTo;
#endif
     thiz->GetPos                = IDirectFBVideoProvider_Swfdec_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_Swfdec_GetLength;
     thiz->GetColorAdjustment    = IDirectFBVideoProvider_Swfdec_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBVideoProvider_Swfdec_SetColorAdjustment;
     thiz->SendEvent             = IDirectFBVideoProvider_Swfdec_SendEvent;
     thiz->SetSpeed              = IDirectFBVideoProvider_Swfdec_SetSpeed;
     thiz->GetSpeed              = IDirectFBVideoProvider_Swfdec_GetSpeed;
     thiz->SetVolume             = IDirectFBVideoProvider_Swfdec_SetVolume;
     thiz->GetVolume             = IDirectFBVideoProvider_Swfdec_GetVolume;
     
     return DFB_OK;
}

