/*
   Copyright (C) 2006-2008 Claudio Ciccani <klan@users.sf.net>

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include <math.h>

#include <pthread.h>

#include <direct/types.h>
#include <direct/list.h>
#include <direct/messages.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <directfb.h>

#include <idirectfb.h>

#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbvideoprovider.h>

#include <core/surface.h>
#include <core/layers.h>

#include <display/idirectfbsurface.h>

#include <gfx/clip.h>

#ifdef HAVE_FUSIONSOUND
# include <fusionsound.h>
# include <fusionsound_limits.h>
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#if (LIBAVFORMAT_VERSION_MAJOR >= 53)
#include <libavutil/mathematics.h>
struct AVDictionary {
    int count;
    AVDictionaryEntry *elems;
};
#endif

#include <libavutil/avutil.h>

#include "dvc.h"


static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, FFmpeg )

D_DEBUG_DOMAIN( FFMPEG, "VideoProvider/FFMPEG", "DirectFB VideoProvider FFmpeg" );

typedef struct {
     DirectLink           link;
     AVPacket             packet;
} PacketLink;

typedef struct {
     PacketLink          *list;
     int                  size;
     s64                  max_len;
     int                  max_size;
     pthread_mutex_t      lock;
} PacketQueue;

typedef struct {
     DirectLink            link;
     
     IDirectFBEventBuffer *buffer;
} EventLink;
     
typedef struct {
     int                            ref;

     DFBVideoProviderStatus         status;
     DFBVideoProviderPlaybackFlags  flags;
     double                         speed;
     float                          volume;
     
     u16                            brightness;
     u16                            contrast;
     u16                            saturation;
     
     IDirectFBDataBuffer           *buffer;
     bool                           seekable;
     void                          *iobuf;
#if (LIBAVFORMAT_VERSION_MAJOR >= 53)
     AVIOContext                    pb;
#else
     ByteIOContext                  pb;
#endif
     AVFormatContext               *context;
     
     s64                            start_time;
 
     struct {
          DirectThread             *thread;
          pthread_mutex_t           lock;
          
          bool                      buffering;
          
          bool                      seeked;
          s64                       seek_time;
          int                       seek_flag;
     } input;
     
     struct {  
          DirectThread             *thread;
          pthread_mutex_t           lock;
          pthread_cond_t            cond;
          
          AVStream                 *st;
          AVCodecContext           *ctx;
          AVCodec                  *codec;
          
          PacketQueue               queue;
          
          s64                       pts;
          
          double                    rate;
          
          bool                      seeked;
     
          IDirectFBSurface         *dest;
          DFBRectangle              rect;
          
          AVFrame                  *src_frame;
          
          DVCColormap              *colormap;
     } video;
     
     struct {
          DirectThread             *thread;
          pthread_mutex_t           lock;
          pthread_cond_t            cond;

          AVStream                 *st;
          AVCodecContext           *ctx;
          AVCodec                  *codec;
          
          PacketQueue               queue;
          
          s64                       pts;
          
          bool                      seeked;
     
#ifdef HAVE_FUSIONSOUND
          IFusionSound             *sound;
          IFusionSoundStream       *stream;
          IFusionSoundPlayback     *playback;
#endif
          int                       sample_size;
          int                       sample_rate;
          int                       buffer_size;
     } audio;
     
     DVFrameCallback                callback;
     void                          *ctx;
     
     EventLink                     *events;
     DFBVideoProviderEventType      events_mask;
     pthread_mutex_t                events_lock;
} IDirectFBVideoProvider_FFmpeg_data;


#define IO_BUFFER_SIZE       8 /* in kylobytes */

#define MAX_QUEUE_LEN        3 /* in seconds */

#define GAP_TOLERANCE    15000 /* in microseconds */

#define GAP_THRESHOLD   250000 /* in microseconds */

/*****************************************************************************/

static int
av_read_callback( void *opaque, uint8_t *buf, int size )
{
     IDirectFBVideoProvider_FFmpeg_data *data = opaque;
     unsigned int                        len  = 0;
     DFBResult                           ret;
     
     if (!buf || size < 0)
          return -1;
     
     if (size) {
          data->buffer->WaitForData( data->buffer, size );
          ret = data->buffer->GetData( data->buffer, size, buf, &len );
          if (ret && ret != DFB_EOF)
               return -1;
     }
          
     return len;
}

static int64_t
av_seek_callback( void *opaque, int64_t offset, int whence )
{
     IDirectFBVideoProvider_FFmpeg_data *data = opaque;
     unsigned int                        pos  = 0;
     DFBResult                           ret;
    
     switch (whence) {
          case SEEK_SET:
               ret = data->buffer->SeekTo( data->buffer, offset );
               break;
          case SEEK_CUR:
               ret = data->buffer->GetPosition( data->buffer, &pos );
               if (ret == DFB_OK) {
                    if (!offset)
                         return pos;
                    ret = data->buffer->SeekTo( data->buffer, pos+offset );
               }
               break;
          case SEEK_END:
               ret = data->buffer->GetLength( data->buffer, &pos );
               if (ret == DFB_OK) {
                    if (offset == -1)
                         return pos;
                    ret = data->buffer->SeekTo( data->buffer, pos-offset );
               }
               break;
          default:
               ret = DFB_UNSUPPORTED;
               break;
     }
     
     if (ret != DFB_OK)
          return -1;
          
     data->buffer->GetPosition( data->buffer, &pos );
          
     return pos;
}     

/*****************************************************************************/

static bool
put_packet( PacketQueue *queue, AVPacket *packet )
{
     PacketLink *p;
     
     p = D_MALLOC( sizeof(PacketLink) );
     if (!p) {
          D_OOM();
          return false;
     }
     
     av_dup_packet( packet );
     p->packet = *packet;
     
     pthread_mutex_lock( &queue->lock );
     direct_list_append( (DirectLink**)&queue->list, &p->link );
     queue->size += packet->size;
     pthread_mutex_unlock( &queue->lock );
     
     return true;
}

static bool
get_packet( PacketQueue *queue, AVPacket *packet )
{
     PacketLink *p = NULL;
         
     pthread_mutex_lock( &queue->lock );
     p = queue->list;
     if (p) {
          direct_list_remove( (DirectLink**)&queue->list, &p->link );
          queue->size -= p->packet.size; 
          *packet = p->packet;
          D_FREE( p );
     }
     pthread_mutex_unlock( &queue->lock );
     
     return (p != NULL);
}    
     
static void
flush_packets( PacketQueue *queue )
{
     PacketLink *p;

     for (p = queue->list; p;) {
          PacketLink *next = (PacketLink*)p->link.next;

          direct_list_remove( (DirectLink**)&queue->list, &p->link );
          av_free_packet( &p->packet );
          D_FREE( p );

          p = next;
     }
     
     queue->list = NULL;
     queue->size = 0;
}

/*****************************************************************************/

static void
dispatch_event( IDirectFBVideoProvider_FFmpeg_data *data,
                DFBVideoProviderEventType           type )
{
     EventLink             *link;
     DFBVideoProviderEvent  event;
     
     if (!data->events || !(data->events_mask & type))
          return;
         
     event.clazz = DFEC_VIDEOPROVIDER;
     event.type  = type;
     
     pthread_mutex_lock( &data->events_lock );
     
     direct_list_foreach (link, data->events)
          link->buffer->PostEvent (link->buffer, DFB_EVENT(&event));
     
     pthread_mutex_unlock( &data->events_lock );
}

static void
release_events( IDirectFBVideoProvider_FFmpeg_data *data )
{
     EventLink *link, *tmp;
     
     direct_list_foreach_safe (link, tmp, data->events) {
          direct_list_remove( (DirectLink**)&data->events, &link->link );
          link->buffer->Release( link->buffer );
          D_FREE( link );
     }
}

/*****************************************************************************/

static inline void
getclock( struct timespec *ret )
{
#ifdef _POSIX_TIMERS
     clock_gettime( CLOCK_REALTIME, ret );
#else
     struct timeval t;
     gettimeofday( &t, NULL );
     ret->tv_sec  = t.tv_sec;
     ret->tv_nsec = t.tv_usec * 1000;
#endif
}

static inline s64
get_stream_clock( IDirectFBVideoProvider_FFmpeg_data *data )
{
#ifdef HAVE_FUSIONSOUND
     if (data->audio.stream && data->audio.pts != -1) {
          int delay = 0;
          data->audio.stream->GetPresentationDelay( data->audio.stream, &delay );
          return data->audio.pts - delay*1000;
     }
#endif          
     return data->video.pts;
}

/******************************************************************************/

static bool
queue_is_full( PacketQueue *queue )
{
     PacketLink *first, *last;
     
     if (!queue->list)
          return false;
          
     first = queue->list;
     last  = (PacketLink*)first->link.prev;
          
     if (last->packet.dts  != AV_NOPTS_VALUE &&
         first->packet.dts != AV_NOPTS_VALUE) {
          if ((last->packet.dts - first->packet.dts) >= queue->max_len)
               return true;
     }
     
     return (queue->size >= queue->max_size);
}

static void*
FFmpegInput( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_FFmpeg_data *data = arg;

     if (url_is_streamed( data->context->pb )) {
          data->input.buffering = true;
          pthread_mutex_lock( &data->video.queue.lock );
          pthread_mutex_lock( &data->audio.queue.lock );
     }
     
     data->audio.pts = -1;
     
     dispatch_event( data, DVPET_STARTED );

     while (data->status != DVSTATE_STOP) {
          AVPacket packet;
          
          direct_thread_testcancel( self );
          
          pthread_mutex_lock( &data->input.lock );
          
          if (data->input.seeked) {
               if (av_seek_frame( data->context, -1, data->input.seek_time,
                                                data->input.seek_flag ) >= 0) {
                    pthread_mutex_lock( &data->video.lock );
                    pthread_mutex_lock( &data->audio.lock );
                    
                    flush_packets( &data->video.queue );
                    flush_packets( &data->audio.queue );
                    if (!data->input.buffering &&
                        url_is_streamed( data->context->pb )) {
                         data->input.buffering = true;
                         pthread_mutex_lock( &data->video.queue.lock );
                         pthread_mutex_lock( &data->audio.queue.lock );
                    }
                  
                    if (data->status == DVSTATE_FINISHED)
                         data->status = DVSTATE_PLAY;
                    data->audio.pts = -1;
                    data->video.seeked = true;
                    data->audio.seeked = true;
                    
                    if (data->video.thread)
                         pthread_cond_signal( &data->video.cond );
                    
                    pthread_mutex_unlock( &data->audio.lock );
                    pthread_mutex_unlock( &data->video.lock );
               }
               data->input.seeked = false;
          }
          
          if (queue_is_full( &data->video.queue ) ||
              queue_is_full( &data->audio.queue )) {
               if (data->input.buffering) {
                    pthread_mutex_unlock( &data->audio.queue.lock ); 
                    pthread_mutex_unlock( &data->video.queue.lock );
                    data->input.buffering = false;
               }
               pthread_mutex_unlock( &data->input.lock );
               usleep( 20000 );
               continue;
          }
          else if (data->video.queue.size == 0 || 
                   data->audio.queue.size == 0) {
               if (!data->input.buffering &&
                   url_is_streamed( data->context->pb )) {
                    data->input.buffering = true;
                    pthread_mutex_lock( &data->video.queue.lock );
                    pthread_mutex_lock( &data->audio.queue.lock );
               }
          }
          
          if (av_read_frame( data->context, &packet ) < 0) {
               if (url_feof( data->context->pb )) {
                    if (data->input.buffering) {
                         pthread_mutex_unlock( &data->audio.queue.lock ); 
                         pthread_mutex_unlock( &data->video.queue.lock );
                         data->input.buffering = false;
                    }
                    if (data->video.queue.size == 0 &&
                        data->audio.queue.size == 0) {
                         if (data->flags & DVPLAY_LOOPING) {
                              data->input.seek_time = 0;
                              data->input.seek_flag = 0;
                              data->input.seeked    = true;
                         } else {
                              data->status = DVSTATE_FINISHED;
                              dispatch_event( data, DVPET_FINISHED );
                         }
                    }
               }     
               pthread_mutex_unlock( &data->input.lock );
               usleep( 100 );
               continue;
          }
          
          if (packet.stream_index == data->video.st->index) {
               put_packet( &data->video.queue, &packet );
          }
#ifdef HAVE_FUSIONSOUND
          else if (data->audio.stream &&
                   packet.stream_index == data->audio.st->index) {
               put_packet( &data->audio.queue, &packet );
          }
#endif
          else {
               av_free_packet( &packet );
          }
          
          pthread_mutex_unlock( &data->input.lock );
     }
     
     if (data->input.buffering) {
          pthread_mutex_unlock( &data->audio.queue.lock ); 
          pthread_mutex_unlock( &data->video.queue.lock );
          data->input.buffering = false;
     }
     
     return (void*)0;
}

static inline DVCPixelFormat
ff2dvc_pixelformat( int pix_fmt )
{
     switch (pix_fmt) {
          case PIX_FMT_YUV420P:
          case PIX_FMT_YUVJ420P:
               return DVCPF_YUV420;
          case PIX_FMT_YUV422P:
          case PIX_FMT_YUVJ422P:
               return DVCPF_YUV422;
          case PIX_FMT_YUV444P:
          case PIX_FMT_YUVJ444P:
               return DVCPF_YUV444;
          case PIX_FMT_YUV411P:
               return DVCPF_YUV411;
          case PIX_FMT_YUV410P:
               return DVCPF_YUV410;
          case PIX_FMT_YUYV422:
               return DVCPF_YUYV_LE;
          case PIX_FMT_UYVY422:
               return DVCPF_YUYV_BE;
          case PIX_FMT_NV12:
               return DVCPF_NV12_LE;
          case PIX_FMT_NV21:
               return DVCPF_NV12_BE;
          case PIX_FMT_GRAY8:
               return DVCPF_Y8;
          case PIX_FMT_RGB8:
               return DVCPF_RGB8;
          case PIX_FMT_RGB555:
               return DVCPF_RGB15;
          case PIX_FMT_RGB565:
               return DVCPF_RGB16;
          case PIX_FMT_RGB24:
               return DVCPF_RGB24;
          case PIX_FMT_BGR24:
               return DVCPF_BGR24;
          case PIX_FMT_RGB32:
               return DVCPF_RGB32;
          case PIX_FMT_BGR32:
               return DVCPF_BGR32;
         default:
               D_ONCE("unsupported picture format");
               break;
     }
     
     return DVCPF_UNKNOWN;
}

static void
FFmpegPutFrame( IDirectFBVideoProvider_FFmpeg_data *data )
{
     AVFrame    *src_frame = data->video.src_frame;
     DVCPicture  picture;
     
     picture.format = ff2dvc_pixelformat( data->video.ctx->pix_fmt );
     picture.width = data->video.ctx->width;
     picture.height = data->video.ctx->height;
     
     picture.base[0] = src_frame->data[0];
     picture.base[1] = src_frame->data[1];
     picture.base[2] = src_frame->data[2];
     picture.pitch[0] = src_frame->linesize[0];
     picture.pitch[1] = src_frame->linesize[1];
     picture.pitch[2] = src_frame->linesize[2];
     
     picture.palette = NULL;
     picture.palette_size = 0;
     
     picture.separated = false;
     picture.premultiplied = false;
     
     dvc_scale_to_surface( &picture, data->video.dest, 
                           data->video.rect.w ? &data->video.rect : NULL,
                           data->video.colormap );
}

static void*
FFmpegVideo( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_FFmpeg_data *data = arg;
     
     AVStream    *st       = data->video.st;
     s64          firtspts = 0;
     unsigned int framecnt = 0;
     long         duration = 1000000.0/data->video.rate;
     int          drop     = 0;

     while (data->status != DVSTATE_STOP) {
          AVPacket        pkt;
          struct timespec time, now;
          int             done = 0;

          getclock( &time );
          
          direct_thread_testcancel( self );
          
          pthread_mutex_lock( &data->video.lock );

          if (data->input.buffering || !get_packet( &data->video.queue, &pkt )) {
               pthread_mutex_unlock( &data->video.lock );
               usleep( 100 );
               continue;
          }

          if (data->video.seeked) {
               avcodec_flush_buffers( data->video.ctx );
               data->video.seeked = false;
               framecnt = 0;
          }
#if (LIBAVFORMAT_VERSION_MAJOR >= 53)
          avcodec_decode_video2( data->video.ctx,
                                 data->video.src_frame,
                                 &done, &pkt );
#else
          avcodec_decode_video( data->video.ctx,
                                data->video.src_frame,
                                &done, pkt.data, pkt.size );
#endif

          if (done && !drop) {
               FFmpegPutFrame( data );

               if (data->callback)
                    data->callback( data->ctx );
          }
          
          if (pkt.dts != AV_NOPTS_VALUE)
               data->video.pts = av_rescale_q( pkt.dts, st->time_base, AV_TIME_BASE_Q );  
          else
               data->video.pts += duration;

          av_free_packet( &pkt );
          
          if (!data->speed) {
               /* paused */
               pthread_cond_wait( &data->video.cond, &data->video.lock );
          }
          else {
               long length, delay;
               
               if (framecnt)
                    duration = (data->video.pts - firtspts) / framecnt;
               length = duration;
               if (data->speed != 1.0)
                    length = (double)length / data->speed;
               
               delay = data->video.pts - get_stream_clock(data);
               if (delay > -GAP_THRESHOLD && delay < +GAP_THRESHOLD)
                    delay = CLAMP( delay, -GAP_TOLERANCE, +GAP_TOLERANCE );
               length += delay;
               
               time.tv_nsec += (length%1000000) * 1000;
               time.tv_sec  += (length/1000000);
               time.tv_sec  += (time.tv_nsec/1000000000);
               time.tv_nsec %= 1000000000;
               
               getclock( &now );
               if (time.tv_sec > now.tv_sec ||
                  (time.tv_sec == now.tv_sec && time.tv_nsec > now.tv_nsec)) {

                    pthread_cond_timedwait( &data->video.cond,
                                            &data->video.lock, &time );
                    drop = false;
               }
               else {
                    delay = (now.tv_sec  - time.tv_sec ) * 1000000 +
                            (now.tv_nsec - time.tv_nsec) / 1000;
                    drop = (delay >= duration);
               }
          }

          pthread_mutex_unlock( &data->video.lock );
          
          if (framecnt++ == 0)
               firtspts = data->video.pts;
     }

     return (void*)0;
}

#ifdef HAVE_FUSIONSOUND
static void*
FFmpegAudio( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_FFmpeg_data *data = arg;

     AVStream *st = data->audio.st;   
     u8        buf[AVCODEC_MAX_AUDIO_FRAME_SIZE]; 

     while (data->status != DVSTATE_STOP) {
          AVPacket  pkt;
          u8       *pkt_data;
          int       pkt_size;
          int       decoded = 0;
          int       len     = AVCODEC_MAX_AUDIO_FRAME_SIZE;
          int       size    = 0;
          
          direct_thread_testcancel( self );
         
          pthread_mutex_lock( &data->audio.lock );
          
          if (!data->speed) {
               pthread_cond_wait( &data->audio.cond, &data->audio.lock );
               pthread_mutex_unlock( &data->audio.lock );
               continue;
          }
          
          if (data->input.buffering ||
              !get_packet( &data->audio.queue, &pkt )) {
               pthread_mutex_unlock( &data->audio.lock );
               usleep( 100 );
               continue;
          }
          
          if (data->audio.seeked) {
               data->audio.stream->Flush( data->audio.stream );
               avcodec_flush_buffers( data->audio.ctx );
               data->audio.seeked = false;
          }
          
          for (pkt_data = pkt.data, pkt_size = pkt.size; pkt_size > 0;) {
#if (LIBAVFORMAT_VERSION_MAJOR >= 53)
               decoded = avcodec_decode_audio3( data->audio.ctx,
                                                (s16*)&buf[size], &len, &pkt );
#else
               decoded = avcodec_decode_audio2( data->audio.ctx,
                                                (s16*)&buf[size], &len,
                                                pkt_data, pkt_size );
#endif
               if (decoded < 0)
                    break;
                       
               pkt_data += decoded;
               pkt_size -= decoded;
               if (len > 0)
                    size += len;
          }
          
          size /= data->audio.sample_size;
          
          if (pkt.pts != AV_NOPTS_VALUE) {
               data->audio.pts = av_rescale_q( pkt.pts,
                                               st->time_base, AV_TIME_BASE_Q ); 
          }
          else if (size && data->audio.pts != -1) {
               data->audio.pts += (s64)size * AV_TIME_BASE / data->audio.sample_rate;
          }
               
          av_free_packet( &pkt );
 
          pthread_mutex_unlock( &data->audio.lock );
          
          if (size)
               data->audio.stream->Write( data->audio.stream, buf, size ); 
          else
               usleep( 1000 );
     }

     return (void*)0;
}
#endif

/*****************************************************************************/

static void
IDirectFBVideoProvider_FFmpeg_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_FFmpeg_data *data = thiz->priv;

     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     if (data->input.thread) {
          direct_thread_cancel( data->input.thread );
          direct_thread_join( data->input.thread );
          direct_thread_destroy( data->input.thread );
     }    
     
     if (data->video.thread) { 
          direct_thread_cancel( data->video.thread );
          direct_thread_join( data->video.thread );
          direct_thread_destroy( data->video.thread );
     }
    
     if (data->audio.thread) {
          direct_thread_cancel( data->audio.thread );
          direct_thread_join( data->audio.thread );
          direct_thread_destroy( data->audio.thread );
     }

#ifdef HAVE_FUSIONSOUND
     if (data->audio.playback)
          data->audio.playback->Release( data->audio.playback );
         
     if (data->audio.stream)
          data->audio.stream->Release( data->audio.stream );

     if (data->audio.sound)
          data->audio.sound->Release( data->audio.sound );
#endif

     if (data->audio.ctx)
          avcodec_close( data->audio.ctx );

     if (data->video.ctx)
          avcodec_close( data->video.ctx );

     if (data->video.src_frame)
          av_free( data->video.src_frame );

     if (data->video.dest)
          data->video.dest->Release( data->video.dest );

     if (data->video.colormap)
          D_FREE( data->video.colormap );

     if (data->context) { 
          AVInputFormat *iformat = data->context->iformat;
          /* Ugly hack to fix a bug (segfault) in url_fclose() */
          if (!(iformat->flags & AVFMT_NOFILE)) {
               iformat->flags |= AVFMT_NOFILE;
               av_close_input_file( data->context );
               iformat->flags ^= AVFMT_NOFILE;
          }
          else {
               av_close_input_file( data->context );
          }
     }

     if (data->buffer)
          data->buffer->Release( data->buffer );

     if (data->iobuf)
          D_FREE( data->iobuf );

     flush_packets( &data->video.queue );
     flush_packets( &data->audio.queue );

     pthread_cond_destroy ( &data->audio.cond );
     pthread_cond_destroy ( &data->video.cond );
     pthread_mutex_destroy( &data->audio.queue.lock );
     pthread_mutex_destroy( &data->video.queue.lock );
     pthread_mutex_destroy( &data->audio.lock );
     pthread_mutex_destroy( &data->video.lock );
     pthread_mutex_destroy( &data->input.lock );

     release_events( data );
     pthread_mutex_destroy( &data->events_lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBVideoProvider_FFmpeg_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     data->ref++;

     return DR_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_Stop( IDirectFBVideoProvider *thiz );

static DirectResult
IDirectFBVideoProvider_FFmpeg_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     IDirectFBVideoProvider_FFmpeg_Stop( thiz );

     if (--data->ref == 0)
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );

     return DR_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetCapabilities( IDirectFBVideoProvider       *thiz,
                                               DFBVideoProviderCapabilities *caps )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (!caps)
          return DFB_INVARG;

     *caps = DVCAPS_BASIC      | DVCAPS_SCALE    | DVCAPS_SPEED     |
             DVCAPS_BRIGHTNESS | DVCAPS_CONTRAST | DVCAPS_SATURATION;
     if (data->seekable)
          *caps |= DVCAPS_SEEK;
     if (data->video.src_frame->interlaced_frame)
          *caps |= DVCAPS_INTERLACED;     
#ifdef HAVE_FUSIONSOUND
     if (data->audio.playback)
          *caps |= DVCAPS_VOLUME;
#endif
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetSurfaceDescription( IDirectFBVideoProvider *thiz,
                                                     DFBSurfaceDescription  *desc )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!desc)
          return DFB_INVARG;
          
     desc->flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     
     if (data->video.src_frame->interlaced_frame) {
          desc->flags |= DSDESC_CAPS;
          desc->caps = DSCAPS_INTERLACED;
     }
     
     desc->width  = data->video.ctx->width;
     desc->height = data->video.ctx->height;
     if (desc->width < 1 || desc->height < 1) {
          desc->width  = 320;
          desc->height = 240;
     }

     switch (data->video.ctx->pix_fmt) {
          case PIX_FMT_RGB8:
               desc->pixelformat = DSPF_RGB332;
               break;
          case PIX_FMT_RGB555:
               desc->pixelformat = DSPF_ARGB1555;
               break;
          case PIX_FMT_RGB565:
               desc->pixelformat = DSPF_RGB16;
               break;
          case PIX_FMT_RGB24:
          case PIX_FMT_BGR24:
               desc->pixelformat = DSPF_RGB24;
               break;
          case PIX_FMT_RGB32:
          case PIX_FMT_BGR32:
               desc->pixelformat = DSPF_RGB32;
               break;
          case PIX_FMT_YUYV422:
               desc->pixelformat = DSPF_YUY2;
               break;
          case PIX_FMT_UYVY422:
               desc->pixelformat = DSPF_UYVY;
               break;
          case PIX_FMT_YUV444P:
          case PIX_FMT_YUV422P:
          case PIX_FMT_YUV420P:
          case PIX_FMT_YUV411P:
          case PIX_FMT_YUV410P:
          case PIX_FMT_YUVJ420P:
          case PIX_FMT_YUVJ422P:
          case PIX_FMT_YUVJ444P:
               desc->pixelformat = DSPF_I420;
               break;
          default:
               desc->pixelformat = dfb_primary_layer_pixelformat();
               break;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetStreamDescription( IDirectFBVideoProvider *thiz,
                                                    DFBStreamDescription   *desc )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!desc)
          return DFB_INVARG;

     desc->caps = DVSCAPS_VIDEO;

     direct_snputs( desc->video.encoding, data->video.codec->name,
                    DFB_STREAM_DESC_ENCODING_LENGTH );
     desc->video.framerate = av_q2d( data->video.st->r_frame_rate );
     desc->video.aspect    = av_q2d( data->video.ctx->sample_aspect_ratio );
     if (!finite( desc->video.aspect ))
          desc->video.aspect = 0.0;
     if (desc->video.aspect)
          desc->video.aspect *= (double)data->video.ctx->width/(double)data->video.ctx->height;
     desc->video.bitrate   = data->video.ctx->bit_rate;

     if (data->audio.st) {
          desc->caps |= DVSCAPS_AUDIO;

          direct_snputs( desc->audio.encoding, data->audio.codec->name,
                         DFB_STREAM_DESC_ENCODING_LENGTH );
          desc->audio.samplerate = data->audio.ctx->sample_rate;
          desc->audio.channels   = data->audio.ctx->channels;
          desc->audio.bitrate    = data->audio.ctx->bit_rate;
     }
               
//     direct_snputs( desc->title, data->context->title, DFB_STREAM_DESC_TITLE_LENGTH );
//     direct_snputs( desc->author, data->context->author, DFB_STREAM_DESC_AUTHOR_LENGTH );
//     direct_snputs( desc->album, data->context->album, DFB_STREAM_DESC_ALBUM_LENGTH );
//     direct_snputs( desc->genre, data->context->genre, DFB_STREAM_DESC_GENRE_LENGTH );
//     direct_snputs( desc->comment, data->context->comment, DFB_STREAM_DESC_COMMENT_LENGTH );
//     desc->year = data->context->year;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_SetDestination( IDirectFBVideoProvider *thiz,
                                              IDirectFBSurface       *dest,
                                              const DFBRectangle     *dest_rect )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_PlayTo( IDirectFBVideoProvider *thiz,
                                      IDirectFBSurface       *dest,
                                      const DFBRectangle     *dest_rect,
                                      DVFrameCallback         callback,
                                      void                   *ctx )
{
     IDirectFBSurface_data *dest_data;
     DFBRectangle           rect = { 0, 0, 0, 0 };

     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!dest)
          return DFB_INVARG;

     if (!dest->priv)
          return DFB_DESTROYED;

     if (data->status == DVSTATE_FINISHED && !data->seekable)
          return DFB_UNSUPPORTED;

     dest_data = dest->priv;

     if (!dfb2dvc_pixelformat( dest_data->surface->config.format ))
          return DFB_UNSUPPORTED;

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;

          rect = *dest_rect;
     }

     pthread_mutex_lock( &data->input.lock );
     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );

     if (data->video.dest)
          data->video.dest->Release( data->video.dest );

     dest->AddRef( dest );

     data->video.dest = dest;
     data->video.rect = rect;
     data->callback   = callback;
     data->ctx        = ctx;

     if (data->status == DVSTATE_FINISHED) {
          data->input.seek_time = 0;
          data->input.seek_flag = 0;
          data->input.seeked    = true;
     }

     data->status = DVSTATE_PLAY;
     data->speed  = 1;

     if (!data->input.thread) {
          data->input.thread = direct_thread_create( DTT_DEFAULT, FFmpegInput,
                                                    (void*)data, "FFmpeg Input" );
     }

#ifdef HAVE_FUSIONSOUND
     if (!data->audio.thread && data->audio.stream) {
          data->audio.thread = direct_thread_create( DTT_DEFAULT, FFmpegAudio,
                                                     (void*)data, "FFmpeg Audio" );
     }
#endif

     if (!data->video.thread) {
          data->video.thread = direct_thread_create( DTT_DEFAULT, FFmpegVideo,
                                                    (void*)data, "FFmpeg Video" );
     }

     pthread_mutex_unlock( &data->audio.lock );
     pthread_mutex_unlock( &data->video.lock );
     pthread_mutex_unlock( &data->input.lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_Stop( IDirectFBVideoProvider *thiz )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (data->status == DVSTATE_STOP)
          return DFB_OK;

     pthread_mutex_lock( &data->input.lock );

     data->status = DVSTATE_STOP;
     
     if (data->input.thread) {
          direct_thread_join( data->input.thread );
          direct_thread_destroy( data->input.thread );
          data->input.thread = NULL;
     }
     
     if (data->video.thread) {
          pthread_mutex_lock( &data->video.lock );
          pthread_cond_signal( &data->video.cond );
          pthread_mutex_unlock( &data->video.lock );
          direct_thread_join( data->video.thread );
          direct_thread_destroy( data->video.thread );
          data->video.thread = NULL;
     }
     
     if (data->audio.thread) {
          pthread_mutex_lock( &data->audio.lock );
          pthread_cond_signal( &data->audio.cond );
          pthread_mutex_unlock( &data->audio.lock );
          direct_thread_join( data->audio.thread );
          direct_thread_destroy( data->audio.thread );
          data->audio.thread = NULL;
     }
     
     if (data->video.dest) {
          data->video.dest->Release( data->video.dest );
          data->video.dest = NULL;
     }

     dispatch_event( data, DVPET_STOPPED );
     
     pthread_mutex_unlock( &data->input.lock );
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetStatus( IDirectFBVideoProvider *thiz,
                                         DFBVideoProviderStatus *status )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!status)
          return DFB_INVARG;

     if (data->status == DVSTATE_PLAY && data->input.buffering)
          *status = DVSTATE_BUFFERING;
     else
          *status = data->status;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_SeekTo( IDirectFBVideoProvider *thiz,
                                      double                  seconds )
{
     s64    time;
     double pos = 0.0;

     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (seconds < 0.0)
          return DFB_INVARG;
          
     if (!data->seekable)
          return DFB_UNSUPPORTED;
          
     thiz->GetPos( thiz, &pos );
  
     time = seconds * AV_TIME_BASE; 
     if (data->context->duration != AV_NOPTS_VALUE &&
         time > data->context->duration)
          return DFB_OK;

     data->input.seek_time = time;
     data->input.seek_flag = (seconds < pos) ? AVSEEK_FLAG_BACKWARD : 0;
     data->input.seeked    = true;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetPos( IDirectFBVideoProvider *thiz,
                                      double                 *seconds )
{
     s64 position;

     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (!seconds)
          return DFB_INVARG;
     
     position = get_stream_clock( data ) - data->start_time;
     *seconds = (position < 0) ? 0.0 : ((double)position/AV_TIME_BASE);
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetLength( IDirectFBVideoProvider *thiz,
                                         double                 *seconds )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!seconds)
          return DFB_INVARG;
     
     if (data->context->duration != AV_NOPTS_VALUE) {      
          *seconds = (double)data->context->duration/AV_TIME_BASE;
          return DFB_OK;
     }
    
     *seconds = 0.0;
     
     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetColorAdjustment( IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!adj)
          return DFB_INVARG;
          
     adj->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST | DCAF_SATURATION;
     adj->brightness = data->brightness;
     adj->contrast   = data->contrast;
     adj->saturation = data->saturation;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_SetColorAdjustment( IDirectFBVideoProvider   *thiz,
                                                  const DFBColorAdjustment *adj )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!adj)
          return DFB_INVARG;
          
     if (adj->flags & DCAF_BRIGHTNESS)
          data->brightness = adj->brightness;
     if (adj->flags & DCAF_CONTRAST)
          data->contrast = adj->contrast;
     if (adj->flags & DCAF_SATURATION)
          data->saturation = adj->saturation;
          
     pthread_mutex_lock( &data->video.lock );
     
     if (data->brightness != 0x8000 ||
         data->contrast   != 0x8000 ||
         data->saturation != 0x8000)
     {
          if (!data->video.colormap) {
               data->video.colormap = D_MALLOC( sizeof(DVCColormap) );
               if (!data->video.colormap) {
                    pthread_mutex_unlock( &data->video.lock );
                    return D_OOM();
               }
          }
          
          dvc_colormap_gen( data->video.colormap,
                            ff2dvc_pixelformat(data->video.ctx->pix_fmt),
                            data->brightness,
                            data->contrast,
                            data->saturation );
     }
     else {
          if (data->video.colormap) {
               D_FREE( data->video.colormap );
               data->video.colormap = NULL;
          }
     }
     
     pthread_mutex_unlock( &data->video.lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_SetPlaybackFlags( IDirectFBVideoProvider        *thiz,
                                                DFBVideoProviderPlaybackFlags  flags )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (flags & ~DVPLAY_LOOPING)
          return DFB_UNSUPPORTED;
          
     if (flags & DVPLAY_LOOPING && !data->seekable)
          return DFB_UNSUPPORTED;
          
     data->flags = flags;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_SetSpeed( IDirectFBVideoProvider *thiz,
                                        double                  multiplier )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (multiplier < 0.0)
          return DFB_INVARG;
          
     if (multiplier > 32.0)
          return DFB_UNSUPPORTED;
          
     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );
     
     if (multiplier) {
          multiplier = MAX( multiplier, 0.01 );
#ifdef HAVE_FUSIONSOUND
          if (data->audio.playback)
               data->audio.playback->SetPitch( data->audio.playback, multiplier );
#endif
     }

     if (multiplier > data->speed) {
          pthread_cond_signal( &data->video.cond );
          pthread_cond_signal( &data->audio.cond );
     }
     
     data->speed = multiplier;
     
     dispatch_event( data, DVPET_SPEEDCHANGE );
     
     pthread_mutex_unlock( &data->audio.lock );
     pthread_mutex_unlock( &data->video.lock );    
     
     return DFB_OK;
}

static DFBResult 
IDirectFBVideoProvider_FFmpeg_GetSpeed( IDirectFBVideoProvider *thiz,
                                        double                 *ret_multiplier )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (!ret_multiplier)
          return DFB_INVARG;
          
     *ret_multiplier = data->speed;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_SetVolume( IDirectFBVideoProvider *thiz,
                                         float                   level )
{
     DFBResult ret = DFB_UNSUPPORTED;

     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (level < 0.0)
          return DFB_INVARG;
          
#ifdef HAVE_FUSIONSOUND
     if (data->audio.playback) {
          ret = data->audio.playback->SetVolume( data->audio.playback, level );
          if (ret == DFB_OK)
               data->volume = level;
     }
#endif

     return ret;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetVolume( IDirectFBVideoProvider *thiz,
                                         float                  *ret_level )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (!ret_level)
          return DFB_INVARG;

     *ret_level = data->volume;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_CreateEventBuffer( IDirectFBVideoProvider  *thiz,
                                                 IDirectFBEventBuffer   **ret_buffer )
{
     IDirectFBEventBuffer *buffer;
     DFBResult             ret;

     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (!ret_buffer)
          return DFB_INVARG;
          
     ret = idirectfb_singleton->CreateEventBuffer( idirectfb_singleton, &buffer );
     if (ret)
          return ret;
          
     ret = thiz->AttachEventBuffer( thiz, buffer );
     
     buffer->Release( buffer );
     
     *ret_buffer = (ret == DFB_OK) ? buffer : NULL;
     
     return ret;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_AttachEventBuffer( IDirectFBVideoProvider *thiz,
                                                 IDirectFBEventBuffer   *buffer )
{
     DFBResult  ret;
     EventLink *link;

     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (!buffer)
          return DFB_INVARG;
     
     ret = buffer->AddRef( buffer );
     if (ret)
          return ret;
     
     link = D_MALLOC( sizeof(EventLink) );
     if (!link) {
          buffer->Release( buffer );
          return D_OOM();
     }
     
     link->buffer = buffer;
     
     pthread_mutex_lock( &data->events_lock );
     direct_list_append( (DirectLink**)&data->events, &link->link );
     pthread_mutex_unlock( &data->events_lock );
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_DetachEventBuffer( IDirectFBVideoProvider *thiz,
                                                 IDirectFBEventBuffer   *buffer )
{
     DFBResult  ret = DFB_ITEMNOTFOUND;
     EventLink *link;

     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (!buffer)
          return DFB_INVARG;
     
     pthread_mutex_lock( &data->events_lock );
     
     direct_list_foreach (link, data->events) {
          if (link->buffer == buffer) {
               direct_list_remove( (DirectLink**)&data->events, &link->link );
               link->buffer->Release( link->buffer );
               D_FREE( link );
               ret = DFB_OK;
               break;
          }
     }

     pthread_mutex_unlock( &data->events_lock );
     
     return ret;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_EnableEvents( IDirectFBVideoProvider    *thiz,
                                            DFBVideoProviderEventType  mask )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (mask & ~DVPET_ALL)
          return DFB_INVARG;
          
     data->events_mask |= mask;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_DisableEvents( IDirectFBVideoProvider    *thiz,
                                             DFBVideoProviderEventType  mask )
{
     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (mask & ~DVPET_ALL)
          return DFB_INVARG;
          
     data->events_mask &= ~mask;
     
     return DFB_OK;
}
       

/* exported symbols */

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     IDirectFBDataBuffer *buffer = ctx->buffer;
     AVProbeData          pd;
     AVInputFormat       *format;
     unsigned char        buf[2048];
     unsigned int         len = 0;
     DFBResult            ret;

     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     ret = buffer->WaitForData( buffer, sizeof(buf) );
     if (ret == DFB_OK)
          ret = buffer->PeekData( buffer, sizeof(buf), 0, &buf[0], &len );
                                       
     if (ret != DFB_OK)
          return ret;    
          
     av_register_all();
          
     pd.filename = ctx->filename ? : "";
     pd.buf      = &buf[0];
     pd.buf_size = len;
     
     format = av_probe_input_format( &pd, 1 );
     if (format) {
          if (format->name) {
               /* ignore formats that are known
                * to not contain video stream. */
               if (!strcmp( format->name, "wav" ) ||
                   !strcmp( format->name, "au"  ) ||
                   !strcmp( format->name, "snd" ) ||
                   !strcmp( format->name, "mp2" ) ||
                   !strcmp( format->name, "mp3" ) ||
                   !strcmp( format->name, "m2a" ) ||
                   !strcmp( format->name, "aac" ) ||
                   !strcmp( format->name, "m4a" ) ||
                   !strcmp( format->name, "ra"  ) ||
                   !strcmp( format->name, "wma" ) ||
                   !strcmp( format->name, "swf" ) ||
                   !strcmp( format->name, "gif" ) ||
                   !strcmp( format->name, "rm"  ))
                    return DFB_UNSUPPORTED;
          }

          return DFB_OK;
     }
          
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     AVProbeData    pd;
     AVInputFormat *fmt;
     unsigned char  buf[2048];
     unsigned int   len = 0;
     int            i;

     D_DEBUG_AT( FFMPEG, "%s:\n", __FUNCTION__ );

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_FFmpeg )
     
     data->ref    = 1;
     data->status = DVSTATE_STOP;
     data->buffer = buffer;
     data->speed  = 1.0;
     data->volume = 1.0;
     
     data->brightness =
     data->contrast   =
     data->saturation = 0x8000;
     
     data->events_mask = DVPET_ALL;
     
     buffer->AddRef( buffer ); 
     buffer->PeekData( buffer, sizeof(buf), 0, &buf[0], &len );
          
     pd.filename = ((IDirectFBDataBuffer_data*)buffer->priv)->filename ? : "";
     pd.buf      = &buf[0];
     pd.buf_size = len;
     
     fmt = av_probe_input_format( &pd, 1 );
     if (!fmt) {
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return DFB_INIT;
     }
     
     data->seekable = (buffer->SeekTo( buffer, 0 ) == DFB_OK);
     
     data->iobuf = D_MALLOC( IO_BUFFER_SIZE * 1024 );
     if (!data->iobuf) {
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return D_OOM();
     }
          
     if (init_put_byte( &data->pb, data->iobuf, IO_BUFFER_SIZE * 1024, 0, 
                        (void*)data, av_read_callback, NULL,
                        data->seekable ? av_seek_callback : NULL ) < 0) {
          D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                   "init_put_byte() failed!\n" );
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return DFB_INIT;
     }
     
     data->pb.is_streamed = (!data->seekable                       ||
                             !strncmp( pd.filename, "http://", 7 ) || 
                             !strncmp( pd.filename, "unsv://", 7 ) ||
                             !strncmp( pd.filename, "ftp://",  6 ) || 
                             !strncmp( pd.filename, "rtsp://", 7 ));
     
     if (av_open_input_stream( &data->context, 
                               &data->pb, pd.filename, fmt, NULL ) < 0) {
          D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                   "av_open_input_stream() failed!\n" );
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
       
     if (av_find_stream_info( data->context ) < 0) {
          D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                   "couldn't find stream info!\n" );
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     for (i = 0; i < data->context->nb_streams; i++) {
          switch (data->context->streams[i]->codec->codec_type) {
#if (LIBAVFORMAT_VERSION_MAJOR >= 53)
               case AVMEDIA_TYPE_VIDEO:
#else
               case CODEC_TYPE_VIDEO:
#endif
                    if (!data->video.st || 
                         data->video.st->codec->bit_rate < 
                         data->context->streams[i]->codec->bit_rate)
                         data->video.st = data->context->streams[i];
                    break;
#if (LIBAVFORMAT_VERSION_MAJOR >= 53)
               case AVMEDIA_TYPE_AUDIO:
#else
               case CODEC_TYPE_AUDIO:
#endif
                    if (!data->audio.st ||
                         data->audio.st->codec->bit_rate <
                         data->context->streams[i]->codec->bit_rate)
                         data->audio.st = data->context->streams[i];
                    break;
               default:
                    break;
          }
     }
     
     if (!data->video.st) {
          D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                   "couldn't find video stream!\n" );
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     data->video.ctx   = data->video.st->codec;
     data->video.codec = avcodec_find_decoder( data->video.ctx->codec_id );
     if (!data->video.codec || 
          avcodec_open( data->video.ctx, data->video.codec ) < 0) 
     {
          D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                   "error opening video codec!\n" );
          data->video.ctx = NULL;
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     data->video.src_frame = avcodec_alloc_frame();
     if (!data->video.src_frame) {
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return D_OOM();
     }
     
     data->video.rate = av_q2d(data->video.st->r_frame_rate);
     if (!data->video.rate || !finite(data->video.rate)) {
          D_INFO( "IDirectFBVideoProvider_FFmpeg: assuming 25 framesXsecond.\n" );
          data->video.rate = 25.0;
     }

     if (data->audio.st) {
          data->audio.ctx   = data->audio.st->codec;
          data->audio.codec = avcodec_find_decoder( data->audio.ctx->codec_id );
          if (!data->audio.codec ||
              avcodec_open( data->audio.ctx, data->audio.codec ) < 0) {
               data->audio.st    = NULL;
               data->audio.ctx   = NULL;
               data->audio.codec = NULL;
          }
     }
     
#ifdef HAVE_FUSIONSOUND      
     if (data->audio.st && 
         idirectfb_singleton->GetInterface( idirectfb_singleton,
                    "IFusionSound", 0, 0, (void**)&data->audio.sound ) == DFB_OK)
     {          
          FSStreamDescription dsc;
          DFBResult           ret;

          if (data->audio.ctx->channels > FS_MAX_CHANNELS)
               data->audio.ctx->channels = FS_MAX_CHANNELS;
               
          dsc.flags        = FSSDF_BUFFERSIZE   | FSSDF_CHANNELS  |
                             FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;
          dsc.channels     = data->audio.ctx->channels;
          dsc.samplerate   = data->audio.ctx->sample_rate;
          dsc.buffersize   = dsc.samplerate/10; /* 100(ms) */
          dsc.sampleformat = FSSF_S16;

          ret = data->audio.sound->CreateStream( data->audio.sound, 
                                                 &dsc, &data->audio.stream );
          if (ret != DFB_OK) {
               D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                        "IFusionSound::CreateStream() failed!\n"
                        "\t-> %s\n", DirectFBErrorString( ret ) );
               data->audio.sound->Release( data->audio.sound );
               data->audio.sound = NULL;
          }
          else {
               data->audio.stream->GetPlayback( data->audio.stream,
                                                &data->audio.playback );

               data->audio.sample_size = 2 * dsc.channels;
               data->audio.sample_rate = dsc.samplerate;
               data->audio.buffer_size = dsc.buffersize;
          }
     }
     else if (data->audio.st) {
          D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                   "couldn't get FusionSound interface!\n" );
     }
#endif

     data->video.queue.max_len = av_rescale_q( MAX_QUEUE_LEN*AV_TIME_BASE,
                                               AV_TIME_BASE_Q,
                                               data->video.st->time_base );
     if (data->video.ctx->bit_rate > 0)
          data->video.queue.max_size = MAX_QUEUE_LEN * data->video.ctx->bit_rate/8;
     else
          data->video.queue.max_size = MAX_QUEUE_LEN * 256 * 1024;

     if (data->audio.st) {
          data->audio.queue.max_len = av_rescale_q( MAX_QUEUE_LEN*AV_TIME_BASE,
                                                    AV_TIME_BASE_Q,
                                                    data->audio.st->time_base );
          if (data->audio.ctx->bit_rate > 0)
               data->audio.queue.max_size = MAX_QUEUE_LEN * data->audio.ctx->bit_rate/8;
          else
               data->audio.queue.max_size = MAX_QUEUE_LEN * 64 * 1024;
     }

     if (data->context->start_time != AV_NOPTS_VALUE)
          data->start_time = data->context->start_time;
 
     direct_util_recursive_pthread_mutex_init( &data->input.lock );
     direct_util_recursive_pthread_mutex_init( &data->video.lock );
     direct_util_recursive_pthread_mutex_init( &data->audio.lock );
     direct_util_recursive_pthread_mutex_init( &data->video.queue.lock );
     direct_util_recursive_pthread_mutex_init( &data->audio.queue.lock );  
     direct_util_recursive_pthread_mutex_init( &data->events_lock );
     pthread_cond_init( &data->video.cond, NULL );
     pthread_cond_init( &data->audio.cond, NULL );
     
     
     thiz->AddRef                = IDirectFBVideoProvider_FFmpeg_AddRef;
     thiz->Release               = IDirectFBVideoProvider_FFmpeg_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_FFmpeg_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_FFmpeg_GetSurfaceDescription;
     thiz->GetStreamDescription  = IDirectFBVideoProvider_FFmpeg_GetStreamDescription;
     thiz->SetDestination        = IDirectFBVideoProvider_FFmpeg_SetDestination;
     thiz->PlayTo                = IDirectFBVideoProvider_FFmpeg_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_FFmpeg_Stop;
     thiz->GetStatus             = IDirectFBVideoProvider_FFmpeg_GetStatus;
     thiz->SeekTo                = IDirectFBVideoProvider_FFmpeg_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_FFmpeg_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_FFmpeg_GetLength;
     thiz->GetColorAdjustment    = IDirectFBVideoProvider_FFmpeg_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBVideoProvider_FFmpeg_SetColorAdjustment;
     thiz->SetPlaybackFlags      = IDirectFBVideoProvider_FFmpeg_SetPlaybackFlags;
     thiz->SetSpeed              = IDirectFBVideoProvider_FFmpeg_SetSpeed;
     thiz->GetSpeed              = IDirectFBVideoProvider_FFmpeg_GetSpeed;
     thiz->SetVolume             = IDirectFBVideoProvider_FFmpeg_SetVolume;
     thiz->GetVolume             = IDirectFBVideoProvider_FFmpeg_GetVolume;
     thiz->CreateEventBuffer     = IDirectFBVideoProvider_FFmpeg_CreateEventBuffer;
     thiz->AttachEventBuffer     = IDirectFBVideoProvider_FFmpeg_AttachEventBuffer;
     thiz->DetachEventBuffer     = IDirectFBVideoProvider_FFmpeg_DetachEventBuffer;
     thiz->EnableEvents          = IDirectFBVideoProvider_FFmpeg_EnableEvents;
     thiz->DisableEvents         = IDirectFBVideoProvider_FFmpeg_DisableEvents;
     
     return DFB_OK;
}

