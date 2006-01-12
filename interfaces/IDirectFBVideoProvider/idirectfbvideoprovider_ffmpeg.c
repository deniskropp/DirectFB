/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
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

#include <core/surfaces.h>
#include <core/layers.h>

#include <display/idirectfbsurface.h>

#ifdef HAVE_FUSIONSOUND
# include <fusionsound.h>
#endif

#include <avcodec.h>
#include <avformat.h>


static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, FFmpeg )

typedef struct {
     DirectLink  link;
     AVPacket    packet;
} PacketLink;

typedef struct {
     PacketLink *list;
     int         size;
} PacketQueue;
     
typedef struct {
     int                         ref;

     IDirectFBDataBuffer        *buffer;
     bool                        seekable;
     void                       *iobuf;
     
     AVFormatContext            *context;
     
     bool                        paused;
     bool                        finished;
     
     __s64                       start_time;
     
     struct {
          DirectThread          *thread;
          pthread_mutex_t        lock;
          
          bool                   seeked;
          __s64                  seek_time;
          int                    seek_flag;
     } input;
     
     struct {  
          DirectThread          *thread;
          pthread_mutex_t        lock;
          pthread_cond_t         cond;
          
          AVStream              *st;
          AVCodecContext        *ctx;
          AVCodec               *codec;
          
          PacketQueue            queue;
          
          double                 pts;
                   
          long                   interval;
          
          bool                   seeked;
     
          IDirectFBSurface      *dest;
          IDirectFBSurface_data *dest_data;
          DFBRectangle           rect;
          
          AVFrame               *src_frame;
          AVFrame               *dst_frame;
          enum PixelFormat       dst_format;
     } video;
     
     struct {
          DirectThread          *thread;
          pthread_mutex_t        lock;

          AVStream              *st;
          AVCodecContext        *ctx;
          AVCodec               *codec;
          
          PacketQueue            queue;
          
          double                 pts;
          
          bool                   seeked;
     
#ifdef HAVE_FUSIONSOUND
          IFusionSound          *sound;
          IFusionSoundStream    *stream;
#endif
          int                    sample_size;
          int                    sample_rate;
          int                    buffer_size;
     } audio;
     
     DVFrameCallback             callback;
     void                       *ctx;
} IDirectFBVideoProvider_FFmpeg_data;


#define IO_BUFFER_SIZE  8192

#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
#define MAX_AUDIOQ_SIZE (5 *  16 * 1024)

#define SEEK_ON_DELAY    1

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

static offset_t
av_seek_callback( void *opaque, offset_t offset, int whence )
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
add_packet( PacketQueue *queue, AVPacket *packet )
{
     PacketLink *p;
     
     p = D_CALLOC( 1, sizeof(PacketLink) );
     if (!p) {
          D_OOM();
          return false;
     }
     
     av_dup_packet( packet );
     p->packet = *packet;
     
     direct_list_append( (DirectLink**)&queue->list, &p->link );
     queue->size += packet->size;
     
     return true;
}

static bool
get_packet( PacketQueue *queue, AVPacket *packet )
{
     PacketLink *p = queue->list;
     
     if (p) {
          direct_list_remove( (DirectLink**)&queue->list, &p->link );
          queue->size -= p->packet.size;
          
          *packet = p->packet;
          D_FREE( p );
          
          return true;
     }
     
     return false;
}    
     
static void
flush_packets( PacketQueue *queue )
{
     PacketLink *p = queue->list;

     while (p) {
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

static void*
FFmpegInput( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_FFmpeg_data *data = arg;
     
     while (!direct_thread_is_canceled( self )) {
          AVPacket packet;
          
          pthread_mutex_lock( &data->input.lock );
          
          if (data->input.seeked) {
#if SEEK_ON_DELAY
               if (av_seek_frame( data->context, -1, data->input.seek_time,
                                                data->input.seek_flag ) >= 0) {
                    flush_packets( &data->video.queue );
                    flush_packets( &data->audio.queue );
                    data->finished     = false;
                    data->video.pts    =
                    data->audio.pts    = (double)data->input.seek_time/AV_TIME_BASE;
                    data->video.seeked = true;
                    data->audio.seeked = true;
                    pthread_cond_signal( &data->video.cond );
               }
#else
               flush_packets( &data->video.queue );
               flush_packets( &data->audio.queue );
#endif
               data->input.seeked = false;
          }
          
          if (data->video.queue.size >= MAX_VIDEOQ_SIZE ||
              data->audio.queue.size >= MAX_AUDIOQ_SIZE) {
               pthread_mutex_unlock( &data->input.lock );
               usleep( 0 );
               continue;
          }
          
          if (av_read_frame( data->context, &packet ) < 0) {
               if (url_feof( &data->context->pb )) {
                    data->finished = (data->video.queue.size == 0 &&
                                      data->audio.queue.size == 0);
               }
                    
               pthread_mutex_unlock( &data->input.lock );
               usleep( 100 );
               continue;
          }
          
          if (packet.stream_index == data->video.st->index)      
               add_packet( &data->video.queue, &packet );
          else if (data->audio.thread &&
                   packet.stream_index == data->audio.st->index)
               add_packet( &data->audio.queue, &packet );
          else
               av_free_packet( &packet );
          
          pthread_mutex_unlock( &data->input.lock );
     }
     
     return (void*)0;
}      

static void*
FFmpegVideo( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_FFmpeg_data *data = arg; 
     long                                drop = 0;

     while (!direct_thread_is_canceled( self )) {
          AVPacket pkt;
          __s64    time;
          int      done = 0;
          
          time = av_gettime();
          
          if (data->paused) {
               pthread_mutex_lock( &data->video.lock );
               pthread_cond_wait( &data->video.cond, &data->video.lock );
               pthread_mutex_unlock( &data->video.lock );
               time = av_gettime();
               drop = 0;
          }

          direct_thread_testcancel( self );
          
          pthread_mutex_lock( &data->video.lock );
    
          pthread_mutex_lock( &data->input.lock );
          if (!get_packet( &data->video.queue, &pkt )) {
               pthread_mutex_unlock( &data->input.lock );
               pthread_mutex_unlock( &data->video.lock );
               usleep( 0 );
               continue;
          }
          pthread_mutex_unlock( &data->input.lock );
          
          if (data->video.seeked) {
               avcodec_flush_buffers( data->video.ctx );
               data->video.seeked = false;
               drop = 0;
          }
               
          avcodec_decode_video( data->video.ctx, 
                                data->video.src_frame,
                                &done, pkt.data, pkt.size );
          
          if (done && !drop) {
               CoreSurface  *surface = data->video.dest_data->surface;
               AVFrame      *frame   = data->video.dst_frame;
               DFBRectangle  rect;
               
               if (data->video.rect.w == 0) {
                    rect = data->video.dest_data->area.wanted;
               } else {
                    rect = data->video.rect;
                    dfb_rectangle_intersect( &rect,
                                             &data->video.dest_data->area.wanted );
               }
               
               if (rect.w > data->video.ctx->width)
                    rect.w = data->video.ctx->width;
               if (rect.h > data->video.ctx->height)
                    rect.h = data->video.ctx->height;      
               
               dfb_surface_soft_lock( surface, DSLF_WRITE, 
                                      (void*)&frame->data[0], 
                                      &frame->linesize[0], 0 );
                                 
               switch (surface->format) {
                    case DSPF_YV12:
                    case DSPF_I420:
                         frame->linesize[1] =
                         frame->linesize[2] = frame->linesize[0]/2;
                         frame->data[1] = frame->data[0] + 
                                          surface->height * frame->linesize[0];
                         frame->data[2] = frame->data[1] +
                                          surface->height/2 * frame->linesize[1];
                         frame->data[1] += rect.y/2 * frame->linesize[1] +
                                           rect.x/2;
                         frame->data[2] += rect.y/2 * frame->linesize[2] +
                                           rect.x/2;
                         if (surface->format == DSPF_YV12) {
                              void *tmp = frame->data[1];
                              frame->data[1] = frame->data[2];
                              frame->data[2] = tmp;
                         }
                    default:
                         frame->data[0] += rect.y * frame->linesize[0] +
                                           DFB_BYTES_PER_LINE( surface->format, rect.x );
                         break;
               }
               
               img_convert( (AVPicture*)data->video.dst_frame,
                             data->video.dst_format,
                            (AVPicture*)data->video.src_frame,
                             data->video.ctx->pix_fmt,
                             rect.w, rect.h );
                             
               dfb_surface_unlock( surface, 0 );
          
               if (data->callback)
                    data->callback( data->ctx );
          }
          
          if (pkt.dts != AV_NOPTS_VALUE)
               data->video.pts = av_q2d(data->video.st->time_base) * pkt.dts;
          else
               data->video.pts += (double)data->video.interval/AV_TIME_BASE;
               
          av_free_packet( &pkt );
          
          if (!data->paused) {
               time += data->video.interval;
               if (data->audio.thread)
                    time += (data->video.pts - data->audio.pts) * AV_TIME_BASE;
               
               drop = time - av_gettime();
               if (drop > 0) {
                    struct timespec stop;
                      
                    stop.tv_sec  =  time / AV_TIME_BASE;
                    stop.tv_nsec = (time % AV_TIME_BASE) * 1000;
             
                    pthread_cond_timedwait( &data->video.cond,
                                            &data->video.lock, &stop );
                    drop = 0;
               } else {
                    drop /= data->video.interval;
               }
          }

          pthread_mutex_unlock( &data->video.lock );
     }
  
     return (void*)0;
}

#ifdef HAVE_FUSIONSOUND
static void*
FFmpegAudio( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_FFmpeg_data *data = arg;
     
     __u8 buf[AVCODEC_MAX_AUDIO_FRAME_SIZE];

     while (!direct_thread_is_canceled( self )) {
          AVPacket  pkt;
          __u8     *pkt_data;
          int       pkt_size;
          int       decoded;
          int       len   = 0;
          int       size  = 0;
          int       delay = 0;
          
          if (data->paused) {
               usleep( 0 );
               continue;
          }
          
          pthread_mutex_lock( &data->audio.lock );
          
          pthread_mutex_lock( &data->input.lock );
          if (!get_packet( &data->audio.queue, &pkt )) {
               pthread_mutex_unlock( &data->input.lock );
               pthread_mutex_unlock( &data->audio.lock );
               usleep( 0 );
               continue;
          }
          pthread_mutex_unlock( &data->input.lock );
          
          if (data->audio.seeked) {
               //data->audio.stream->Wait( data->audio.stream, 0 );
               avcodec_flush_buffers( data->audio.ctx );
               data->audio.seeked = false;
          }
          
          for (pkt_data = pkt.data, pkt_size = pkt.size; pkt_size > 0;) {
               decoded = avcodec_decode_audio( data->audio.ctx, 
                                               (__s16*)&buf[size], &len, 
                                               pkt_data, pkt_size );
               if (decoded < 0)
                    break;
                         
               pkt_data += decoded;
               pkt_size -= decoded;
               if (len > 0)
                    size += len;
          }
          
          size /= data->audio.sample_size;
          
          if (pkt.pts != AV_NOPTS_VALUE) {
               data->audio.pts = av_q2d(data->audio.st->time_base) * pkt.pts;
          }
          else if (size) {
               data->audio.pts += (double)size / 
                                  (double)data->audio.sample_rate;
          }
          
          data->audio.stream->GetPresentationDelay( data->audio.stream, &delay );
          data->audio.pts -= (double)delay / 1000.0;
               
          av_free_packet( &pkt );
 
          pthread_mutex_unlock( &data->audio.lock );
          
          if (size)
               data->audio.stream->Write( data->audio.stream, buf, size ); 
          else
               usleep( 0 );
     }

     return (void*)0;
}
#endif

/*****************************************************************************/

static void
IDirectFBVideoProvider_FFmpeg_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_FFmpeg_data *data = thiz->priv;
     
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
     
#ifdef HAVE_FUSIONSOUND
     if (data->audio.thread) {
          direct_thread_cancel( data->audio.thread );
          direct_thread_join( data->audio.thread );
          direct_thread_destroy( data->audio.thread );
     }
     
     if (data->audio.ctx)
          avcodec_close( data->audio.ctx );
        
     if (data->audio.stream)
          data->audio.stream->Release( data->audio.stream );

     if (data->audio.sound)
          data->audio.sound->Release( data->audio.sound );
#endif

     if (data->video.ctx)
          avcodec_close( data->video.ctx );
          
     if (data->video.src_frame)
          av_free( data->video.src_frame );
          
     if (data->video.dst_frame)
          av_free( data->video.dst_frame );

     if (data->video.dest)
          data->video.dest->Release( data->video.dest );
          
     if (data->context) {
          data->context->iformat->flags |= AVFMT_NOFILE;
          av_close_input_file( data->context );
     }
          
     if (data->buffer)
          data->buffer->Release( data->buffer );

     if (data->iobuf)
          D_FREE( data->iobuf );
          
     flush_packets( &data->video.queue );
     flush_packets( &data->audio.queue );

     pthread_cond_destroy( &data->video.cond );
     pthread_mutex_destroy( &data->video.lock );
     pthread_mutex_destroy( &data->audio.lock );
     pthread_mutex_destroy( &data->input.lock );
     
     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (--data->ref == 0)
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetCapabilities( IDirectFBVideoProvider       *thiz,
                                               DFBVideoProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (!caps)
          return DFB_INVARG;
          
     *caps = DVCAPS_BASIC;
     if (data->seekable)
          *caps |= DVCAPS_SEEK;
     if (data->video.src_frame->interlaced_frame)
          *caps |= DVCAPS_INTERLACED;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetSurfaceDescription( IDirectFBVideoProvider *thiz,
                                                     DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!desc)
          return DFB_INVARG;
          
     desc->flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     
     desc->width  = data->video.ctx->width;
     desc->height = data->video.ctx->height;
     if (desc->width < 1 || desc->height < 1) {
          desc->width  = 320;
          desc->height = 240;
     }

     switch (data->video.ctx->pix_fmt) {
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
          case PIX_FMT_RGBA32:
               desc->pixelformat = DSPF_ARGB;
               break;
          case PIX_FMT_YUV422:
               desc->pixelformat = DSPF_YUY2;
               break;
          case PIX_FMT_UYVY422:
          case PIX_FMT_UYVY411:
               desc->pixelformat = DSPF_UYVY;
               break;
          case PIX_FMT_YUV420P:
          case PIX_FMT_YUV422P:
          case PIX_FMT_YUV444P:
          case PIX_FMT_YUV410P:
          case PIX_FMT_YUV411P:
          case PIX_FMT_YUVJ420P:
          case PIX_FMT_YUVJ422P:
          case PIX_FMT_YUVJ444P:
               desc->pixelformat = DSPF_YV12;
               break;
          default:
               desc->pixelformat = dfb_primary_layer_pixelformat();
               break;
     }

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
     enum PixelFormat       pix_fmt = 0;
     DFBRectangle           rect    = { 0, 0, 0, 0 };
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!dest)
          return DFB_INVARG;

     if (!dest->priv)
          return DFB_DESTROYED;
          
     dest_data = dest->priv;
     
     switch (dest_data->surface->format) {
          case DSPF_ARGB1555:
               pix_fmt = PIX_FMT_RGB555;
               break;
          case DSPF_RGB16:
               pix_fmt = PIX_FMT_RGB565;
               break;
          case DSPF_RGB24:
               pix_fmt = PIX_FMT_RGB24;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               pix_fmt = PIX_FMT_RGBA32;
               break;
          case DSPF_YUY2:
               pix_fmt = PIX_FMT_YUV422;
               break;
          case DSPF_UYVY:
               pix_fmt = PIX_FMT_UYVY422;
               break;
          case DSPF_YV12:
          case DSPF_I420:
               pix_fmt = PIX_FMT_YUV420P;
               break;
          default:
               return DFB_UNSUPPORTED;
     }
           
     if (dest_rect) {
          if (dest_rect->x < 1 || dest_rect->y < 1)
               return DFB_INVARG;
         
          rect    = *dest_rect;
          rect.x += dest_data->area.wanted.x;
          rect.y += dest_data->area.wanted.y;
          
          if (!dfb_rectangle_intersect( &rect,
                                        &dest_data->area.wanted ))
               return DFB_INVARG;
     }

     pthread_mutex_lock( &data->input.lock );
     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );
     
     if (data->video.dest)
          data->video.dest->Release( data->video.dest );
     dest->AddRef( dest );
     
     data->video.dst_format = pix_fmt;
     data->video.dest       = dest;
     data->video.dest_data  = dest_data;
     data->video.rect       = rect;
     data->callback         = callback;
     data->ctx              = ctx;
     
     if (data->finished) {
#if SEEK_ON_DELAY
          data->input.seek_time = (double)data->start_time / AV_TIME_BASE;
          data->input.seek_flag = 0;
          data->input.seeked    = true;
#else
          if (av_seek_frame( data->context, -1, 
                             data->start_time, 0 ) < 0) {
               pthread_mutex_unlock( &data->input.lock );
               pthread_mutex_unlock( &data->video.lock );
               pthread_mutex_unlock( &data->audio.lock );
               return DFB_UNSUPPORTED;
          }
          
          data->finished     = false;
          data->video.pts    =
          data->audio.pts    = (double)data->start_time / AV_TIME_BASE;
          data->input.seeked = true;
          data->video.seeked = true;
          data->audio.seeked = true;
#endif
     }

     if (data->paused) {
          data->paused = false;
          pthread_cond_signal( &data->video.cond );
     }
     
     if (!data->input.thread) {
          data->input.thread = direct_thread_create( DTT_DEFAULT, FFmpegInput,
                                                    (void*)data, "FFmpeg Input" );
     }
     
     if (!data->video.thread) {
          data->video.thread = direct_thread_create( DTT_DEFAULT, FFmpegVideo,
                                                    (void*)data, "FFmpeg Video" );
     }

#ifdef HAVE_FUSIONSOUND
     if (!data->audio.thread && data->audio.stream) {
          data->audio.thread = direct_thread_create( DTT_DEFAULT, FFmpegAudio,
                                                     (void*)data, "FFmpeg Audio" );
     }
#endif
 
     pthread_mutex_unlock( &data->input.lock );
     pthread_mutex_unlock( &data->video.lock );
     pthread_mutex_unlock( &data->audio.lock );
          
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
 
     if (!data->paused) {
          pthread_mutex_lock( &data->video.lock );
          pthread_mutex_lock( &data->audio.lock );
          
          data->paused = true;
          pthread_cond_signal( &data->video.cond );
          
          pthread_mutex_unlock( &data->video.lock );
          pthread_mutex_unlock( &data->audio.lock );
     }
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_SeekTo( IDirectFBVideoProvider *thiz,
                                      double                  seconds )
{
     __s64  time;
     double pos = 0.0;
     int    ret = 0;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (seconds < 0.0)
          return DFB_INVARG;
          
     if (!data->seekable)
          return DFB_UNSUPPORTED;
          
     thiz->GetPos( thiz, &pos );
  
     time = (seconds * AV_TIME_BASE) + data->start_time;
      
     if (data->context->duration != AV_NOPTS_VALUE &&
         time > data->context->duration)
          return DFB_OK;

#if SEEK_ON_DELAY
     data->input.seek_time = time;
     data->input.seek_flag = (seconds < pos) ? AVSEEK_FLAG_BACKWARD : 0;
     data->input.seeked    = true;
#else
     pthread_mutex_lock( &data->input.lock );
     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );

     ret = av_seek_frame( data->context, -1, time, 
                         (seconds < pos) ? AVSEEK_FLAG_BACKWARD : 0 );
     if (ret >= 0) {
          data->finished     = false;
          data->video.pts    = (double)time / AV_TIME_BASE;
          data->audio.pts    = (double)time / AV_TIME_BASE;
          data->input.seeked = true;
          data->video.seeked = true;
          data->audio.seeked = true;
          pthread_cond_signal( &data->video.cond );
     }
     
     pthread_mutex_unlock( &data->input.lock );
     pthread_mutex_unlock( &data->video.lock );
     pthread_mutex_unlock( &data->audio.lock );
#endif
     
     return (ret < 0) ? DFB_FAILURE : DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetPos( IDirectFBVideoProvider *thiz,
                                      double                 *seconds )
{
     double pos = 0.0;
                 
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (!seconds)
          return DFB_INVARG;
          
     if (data->audio.pts != -1)
          pos += data->audio.pts;
     else
          pos += data->video.pts;
          
     pos -= (double)data->start_time / AV_TIME_BASE;
          
     *seconds = (pos < 0.0) ? 0.0 : pos;
     
     return data->finished ? DFB_EOF : DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetLength( IDirectFBVideoProvider *thiz,
                                         double                 *seconds )
{    
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
          
     if (!seconds)
          return DFB_INVARG;
     
     if (data->context->duration != AV_NOPTS_VALUE) {      
          *seconds = (double)data->context->duration / AV_TIME_BASE;
          return DFB_OK;
     }
    
     *seconds = 0.0;
     
     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetColorAdjustment( IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!adj)
          return DFB_INVARG;
          
     adj->flags = DCAF_NONE;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_SetColorAdjustment( IDirectFBVideoProvider   *thiz,
                                                  const DFBColorAdjustment *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!adj)
          return DFB_INVARG;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_SendEvent( IDirectFBVideoProvider *thiz,
                                         const DFBEvent         *evt )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (!evt)
          return DFB_INVARG;
          
     return DFB_UNSUPPORTED;
} 

/* exported symbols */

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     IDirectFBDataBuffer *buffer = ctx->buffer;
     AVProbeData          pd;
     AVInputFormat       *format;
     unsigned char        buf[2048];
     int                  len = 0;
     DFBResult            ret;
     
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
     
     return (format) ? DFB_OK : DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     AVProbeData    pd;
     AVInputFormat *fmt;
     ByteIOContext  pb;
     unsigned char  buf[2048];
     double         rate;
     int            i, len = 0;
      
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_FFmpeg )
     
     data->ref    = 1;
     data->buffer = buffer;
     
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
     
     data->iobuf = D_MALLOC( IO_BUFFER_SIZE );
     if (!data->iobuf) {
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return D_OOM();
     }
          
     if (init_put_byte( &pb, data->iobuf, IO_BUFFER_SIZE, 0, 
                        (void*)data, av_read_callback, NULL,
                        data->seekable ? av_seek_callback : NULL ) < 0) {
          D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                   "init_put_byte() failed!\n" );
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return DFB_INIT;
     }
     
     if (av_open_input_stream( &data->context, 
                               &pb, pd.filename, fmt, NULL ) < 0) {
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
               case CODEC_TYPE_VIDEO:
                    data->video.st = data->context->streams[i];
                    break;
               case CODEC_TYPE_AUDIO:
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
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     data->video.src_frame = avcodec_alloc_frame();
     data->video.dst_frame = avcodec_alloc_frame();
     if (!data->video.src_frame || !data->video.dst_frame) {
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return D_OOM();
     }
     
     rate = av_q2d(data->video.st->r_frame_rate);
     if (!rate || !finite(rate)) {
          D_DEBUG( "IDirectFBVideoProvider_FFmpeg: "
                   "assuming framerate=30fps.\n" );
          rate = 30.0;
     }
     data->video.interval = AV_TIME_BASE / rate;
     
#ifdef HAVE_FUSIONSOUND      
     if (data->audio.st && 
         idirectfb_singleton->GetInterface( idirectfb_singleton,
                    "IFusionSound", 0, 0, (void**)&data->audio.sound ) == DFB_OK)
     {          
          data->audio.ctx   = data->audio.st->codec;
          data->audio.codec = avcodec_find_decoder( data->audio.ctx->codec_id );
          
          if (data->audio.codec &&
              avcodec_open( data->audio.ctx, data->audio.codec ) == 0) 
          {
               FSStreamDescription dsc;
               
               if (data->audio.ctx->channels > 2)
                    data->audio.ctx->channels = 2;

               dsc.flags        = FSSDF_BUFFERSIZE   | FSSDF_CHANNELS  |
                                  FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;
               dsc.channels     = data->audio.ctx->channels;
               dsc.samplerate   = data->audio.ctx->sample_rate;
               dsc.buffersize   = dsc.samplerate*120/1000; /* 120(ms) */
               dsc.sampleformat = FSSF_S16;

               if (data->audio.sound->CreateStream( data->audio.sound, 
                                           &dsc, &data->audio.stream ) != DFB_OK) {
                    D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                             "IFusionSound::CreateStream() failed!\n" );
                    avcodec_close( data->audio.ctx );
                    data->audio.ctx = NULL;
               }

               data->audio.sample_size = 2 * dsc.channels;
               data->audio.sample_rate = dsc.samplerate;
               data->audio.buffer_size = dsc.buffersize;
          }
          
          if (!data->audio.stream) {
               data->audio.sound->Release( data->audio.sound );
               data->audio.sound = NULL;
          }
     }
     else {
          D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                   "failed to get FusionSound interface!\n" );
     }
#endif

     if (data->context->start_time != AV_NOPTS_VALUE)
          data->start_time = data->context->start_time;

     data->video.pts = 
     data->audio.pts = (double)data->start_time / AV_TIME_BASE;
 
     direct_util_recursive_pthread_mutex_init( &data->input.lock );
     direct_util_recursive_pthread_mutex_init( &data->audio.lock );
     direct_util_recursive_pthread_mutex_init( &data->video.lock );
     pthread_cond_init( &data->video.cond, NULL );
     
     thiz->AddRef                = IDirectFBVideoProvider_FFmpeg_AddRef;
     thiz->Release               = IDirectFBVideoProvider_FFmpeg_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_FFmpeg_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_FFmpeg_GetSurfaceDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_FFmpeg_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_FFmpeg_Stop;
     thiz->SeekTo                = IDirectFBVideoProvider_FFmpeg_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_FFmpeg_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_FFmpeg_GetLength;
     thiz->GetColorAdjustment    = IDirectFBVideoProvider_FFmpeg_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBVideoProvider_FFmpeg_SetColorAdjustment;
     thiz->SendEvent             = IDirectFBVideoProvider_FFmpeg_SendEvent;
     
     return DFB_OK;
}

