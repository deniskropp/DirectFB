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
     
     bool                        stopped;
     bool                        finished;
     
     __u64                       delay;
     
     struct {
          DirectThread          *thread;
          pthread_mutex_t        lock;
          
          bool                   seeked;
     } input;
     
     struct {  
          DirectThread          *thread;
          pthread_mutex_t        lock;
          pthread_cond_t         cond;
          
          int                    idx;
          AVCodecContext        *context;
          AVCodec               *codec;
          
          PacketQueue            queue;
          
          AVFrame               *src_frame;
          AVFrame               *dst_frame;
          enum PixelFormat       dst_format;
          
          long                   interval;
          
          bool                   seeked;
     
          IDirectFBSurface      *dest;
          IDirectFBSurface_data *dest_data;
          DFBRectangle           rect;
     } video;
     
     struct {
          DirectThread          *thread;
          pthread_mutex_t        lock;

          int                    idx;
          AVCodecContext        *context;
          AVCodec               *codec;
          
          PacketQueue            queue;
          
          bool                   seeked;
     
#ifdef HAVE_FUSIONSOUND
          IFusionSound          *sound;
          IFusionSoundStream    *stream;
#endif
          int                    sample_size;
          int                    buffer_size;
     } audio;
     
     DVFrameCallback             callback;
     void                       *ctx;
} IDirectFBVideoProvider_FFmpeg_data;


#define IO_BUFFER_SIZE  8192

#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
#define MAX_AUDIOQ_SIZE (5 *  16 * 1024)


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

static void*
FFmpegInput( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_FFmpeg_data *data = arg;
     
     while (!direct_thread_is_canceled( self )) {
          AVPacket packet;
          
          pthread_mutex_lock( &data->input.lock );
          
          if (data->input.seeked) {
               flush_packets( &data->video.queue );
               flush_packets( &data->audio.queue );
               data->input.seeked = false;
          }
          
          if (data->video.queue.size >= MAX_VIDEOQ_SIZE ||
              data->audio.queue.size >= MAX_AUDIOQ_SIZE) {
               pthread_mutex_unlock( &data->input.lock );
               usleep( 0 );
               continue;
          }
          
          if (av_read_frame( data->context, &packet ) < 0) {
               if (url_feof( &data->context->pb ))
                    data->finished = true;
                    
               pthread_mutex_unlock( &data->input.lock );
               usleep( 100 );
               continue;
          }
          
          if (packet.stream_index == data->video.idx)      
               add_packet( &data->video.queue, &packet );
          else if (packet.stream_index == data->audio.idx && data->audio.thread)
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
     IDirectFBVideoProvider_FFmpeg_data *data  = arg;
     long                                delay = 0;

     while (!direct_thread_is_canceled( self )) {
          struct timeval start;
          AVPacket       packet = { .data = NULL };
          int            done   = 0;
          
          gettimeofday( &start, NULL );
          
          if (data->stopped) {
               pthread_mutex_lock( &data->video.lock );
               pthread_cond_wait( &data->video.cond, &data->video.lock );
               pthread_mutex_unlock( &data->video.lock );
               gettimeofday( &start, NULL );
               delay = 0;
          }

          direct_thread_testcancel( self );
          
          pthread_mutex_lock( &data->video.lock );
          
          if (data->video.seeked) {
               //avcodec_flush_buffers( data->video.context );
               data->video.seeked = false;
          }
    
          pthread_mutex_lock( &data->input.lock );
          get_packet( &data->video.queue, &packet );
          pthread_mutex_unlock( &data->input.lock );
          
          if (!packet.data) {
               pthread_mutex_unlock( &data->video.lock );
               usleep( 0 );
               continue;
          }
               
          if (delay < data->video.interval) {
               __u8 *pkt_data = packet.data;
               int   pkt_size = packet.size;
               int   decoded;
                    
               while (pkt_size > 0 && !done) {
                    decoded = avcodec_decode_video( data->video.context, 
                                                    data->video.src_frame,
                                                    &done, pkt_data, pkt_size );
                    if (decoded < 0)
                         break;
                              
                    pkt_data += decoded;
                    pkt_size -= decoded;
               }
          }
               
          av_free_packet( &packet );
          
          if (done) {
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
               
               if (rect.w > data->video.context->width)
                    rect.w = data->video.context->width;
               if (rect.h > data->video.context->height)
                    rect.h = data->video.context->height;      
               
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
                             data->video.context->pix_fmt,
                             rect.w, rect.h );
                             
               dfb_surface_unlock( surface, 0 );
          
               if (data->callback)
                    data->callback( data->ctx );
          }

          if (!data->stopped) {
               struct timeval now;

               gettimeofday( &now, NULL );
               delay += (now.tv_sec  - start.tv_sec) * 1000000 + 
                        (now.tv_usec - start.tv_usec);
               
               if (delay < data->video.interval) {
                    struct timespec t;

                    t.tv_sec  =  start.tv_sec;
                    t.tv_nsec = (start.tv_usec + data->video.interval) * 1000;
               
                    pthread_cond_timedwait( &data->video.cond,
                                            &data->video.lock, &t );
                    delay = 0;
               } else {
                    delay -= data->video.interval;
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
     int  samples = 0;

     while (!direct_thread_is_canceled( self )) {
          AVPacket packet = { .data = NULL };
          int      size   = 0;
          
          if (data->stopped) {
               usleep( 0 );
               continue;
          }
          
          pthread_mutex_lock( &data->audio.lock );
          
          pthread_mutex_unlock( &data->input.lock );
          get_packet( &data->audio.queue, &packet );
          pthread_mutex_unlock( &data->input.lock );
          
          if (data->audio.seeked) {
               if (samples)
                    data->audio.stream->Wait( data->audio.stream, samples );
               data->audio.seeked = false;
          }
          
          if (packet.data) {
               __u8 *pkt_data = packet.data;
               int   pkt_size = packet.size;
               int   decoded;
               int   len;
               
               for (len = 0; pkt_size > 0;) {
                    decoded = avcodec_decode_audio( data->audio.context, 
                                                   (__s16*)&buf[size], &len, 
                                                   pkt_data, pkt_size );
                    if (decoded < 0)
                         break;
                         
                    pkt_data += decoded;
                    pkt_size -= decoded;
                    if (len > 0)
                         size += len;
               }
               
               av_free_packet( &packet );
          }
 
          pthread_mutex_unlock( &data->audio.lock );
          
          if (size < 1) {
               usleep( 0 );
               continue;
          }
          
          size /= data->audio.sample_size;
          
          data->audio.stream->Write( data->audio.stream, buf, size );
          samples += size;
          samples %= data->audio.buffer_size+1;
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
     
     if (data->audio.context)
          avcodec_close( data->audio.context );
        
     if (data->audio.stream)
          data->audio.stream->Release( data->audio.stream );

     if (data->audio.sound)
          data->audio.sound->Release( data->audio.sound );
#endif

     if (data->video.context)
          avcodec_close( data->video.context );
          
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
     
     desc->width  = data->video.context->width;
     desc->height = data->video.context->height;
     if (desc->width < 1 || desc->height < 1) {
          desc->width  = 320;
          desc->height = 240;
     }

     switch (data->video.context->pix_fmt) {
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
          if (av_seek_frame( data->context, -1, 
                             data->context->start_time, 0 ) < 0) {
               pthread_mutex_unlock( &data->input.lock );
               pthread_mutex_unlock( &data->video.lock );
               pthread_mutex_unlock( &data->audio.lock );
               return DFB_UNSUPPORTED;
          }
          
          data->finished     = false;
          data->input.seeked = true;
          data->video.seeked = true;
          data->audio.seeked = true;
     }

     if (data->stopped) {
          data->stopped = false;
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
 
     if (!data->stopped) {
          pthread_mutex_lock( &data->video.lock );
          pthread_mutex_lock( &data->audio.lock );
          
          data->stopped = true;
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
     __s64 pos;
     int   ret;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )

     if (seconds < 0.0)
          return DFB_INVARG;
          
     if (!data->seekable)
          return DFB_UNSUPPORTED;
  
     pos = (seconds * AV_TIME_BASE);
     if (data->context->start_time != AV_NOPTS_VALUE)
          pos += data->context->start_time;
     
     if (data->context->duration != AV_NOPTS_VALUE &&
         pos > data->context->duration)
          return DFB_OK;
     
     pthread_mutex_lock( &data->input.lock );
     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );

     ret = av_seek_frame( data->context, -1, pos, 0 );
     if (ret >= 0) {
          data->finished     = false;
          data->input.seeked = true;
          data->video.seeked = true;
          data->audio.seeked = true;
          pthread_cond_signal( &data->video.cond );
     }
     
     pthread_mutex_unlock( &data->input.lock );
     pthread_mutex_unlock( &data->video.lock );
     pthread_mutex_unlock( &data->audio.lock );

     return (ret < 0) ? DFB_FAILURE : DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_FFmpeg_GetPos( IDirectFBVideoProvider *thiz,
                                      double                 *seconds )
{            
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_FFmpeg )
     
     if (!seconds)
          return DFB_INVARG;        
     
     if (data->context->bit_rate > 0) {
          offset_t pos;
          
          pos = url_ftell( &data->context->pb );
          if (pos != (offset_t)-1) {
               pos -= data->video.queue.size + data->audio.queue.size;
               *seconds = (double)pos / (double)(data->context->bit_rate/8);
               return data->finished ? DFB_EOF : DFB_OK;
          }
     }
     
     *seconds = 0.0;
     
     return DFB_UNSUPPORTED;
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
          
     pd.filename = ((IDirectFBDataBuffer_data*)buffer->priv)->filename ? : "";
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
     int            video_stream = -1;
     int            audio_stream = -1;
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
                    video_stream = i;
                    break;
               case CODEC_TYPE_AUDIO:
                    audio_stream = i;
                    break;
               default:
                    break;
          }
     }
     
     if (video_stream == -1) {
          D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                   "couldn't find video stream!\n" );
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     data->video.idx     = video_stream;
     data->video.context = data->context->streams[video_stream]->codec;
     data->video.codec   = avcodec_find_decoder( data->video.context->codec_id );
     if (!data->video.codec || 
          avcodec_open( data->video.context, data->video.codec ) < 0) 
     {
          D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                   "error opening video codec!\n" );
          IDirectFBVideoProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     data->video.src_frame = avcodec_alloc_frame();
     data->video.dst_frame = avcodec_alloc_frame();
     
     rate = av_q2d(data->context->streams[video_stream]->r_frame_rate);
     if (!rate || !finite(rate)) {
          D_DEBUG( "IDirectFBVideoProvider_FFmpeg: "
                   "assuming framerate=30fps.\n" );   
          data->video.interval = 1000000.0 / 30.0;
     } 
     else {
          data->video.interval = 1000000.0 / rate;
     }
     
#ifdef HAVE_FUSIONSOUND      
     if (audio_stream != -1 && 
         idirectfb_singleton->GetInterface( idirectfb_singleton,
                    "IFusionSound", 0, 0, (void**)&data->audio.sound ) == DFB_OK)
     {          
          data->audio.idx     = audio_stream;
          data->audio.context = data->context->streams[audio_stream]->codec;
          data->audio.codec   = avcodec_find_decoder( data->audio.context->codec_id );
          
          if (data->audio.codec &&
              avcodec_open( data->audio.context, data->audio.codec ) == 0) 
          {
               FSStreamDescription dsc;
               
               if (data->audio.context->channels > 2)
                    data->audio.context->channels = 2;

               dsc.flags        = FSSDF_BUFFERSIZE   | FSSDF_CHANNELS  |
                                  FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;
               dsc.channels     = data->audio.context->channels;
               dsc.samplerate   = data->audio.context->sample_rate;
               dsc.buffersize   = dsc.samplerate/4;
               dsc.sampleformat = FSSF_S16;

               if (data->audio.sound->CreateStream( data->audio.sound, 
                                        &dsc, &data->audio.stream ) != DFB_OK) {
                    D_ERROR( "IDirectFBVideoProvider_FFmpeg: "
                             "IFusionSound::CreateStream() failed!\n" );
                    avcodec_close( data->audio.context );
                    data->audio.context = NULL;
               }

               data->audio.sample_size = 2 * dsc.channels;
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

