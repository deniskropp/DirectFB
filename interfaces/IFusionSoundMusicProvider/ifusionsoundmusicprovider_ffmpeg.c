/*
 * Copyright (C) 2007-2008 Claudio Ciccani <klan@users.sf.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <math.h>

#include <fusionsound.h>
#include <fusionsound_limits.h>

#include <direct/types.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/stream.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <media/ifusionsoundmusicprovider.h>

#include <misc/sound_util.h>

#include <avcodec.h>
#include <avformat.h>

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename,
           DirectStream              *stream );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, FFmpeg )

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int                           ref;       /* reference counter */

     DirectStream                 *stream;

     ByteIOContext                 pb;
     AVFormatContext              *ctx;
     AVStream                     *st;
     void                         *iobuf;
     
     AVCodecContext               *codec;
     u8                           *buf;
     s64                           pts;

     FSMusicProviderPlaybackFlags  flags;

     DirectThread                 *thread;
     pthread_mutex_t               lock;
     bool                          playing;
     bool                          finished;
     bool                          seeked;

     struct {
          IFusionSoundStream      *stream;
          IFusionSoundBuffer      *buffer;
          FSSampleFormat           format;
          FSChannelMode            mode;
          int                      framesize;
     } dest;

     FMBufferCallback              callback;
     void                         *callback_data;
} IFusionSoundMusicProvider_FFmpeg_data;

/*****************************************************************************/

static int
av_read_callback( void *opaque, uint8_t *buf, int size )
{
     IFusionSoundMusicProvider_FFmpeg_data *data = opaque;
     unsigned int                           len  = 0;
     DirectResult                           ret;
     
     if (!buf || size < 0)
          return -1;
     
     while (size) {
          unsigned int num = 0;
          direct_stream_wait( data->stream, size, NULL );
          ret = direct_stream_read( data->stream, size, buf, &num );
          if (ret) {
               if (!len)
                    return -1;
               break;
          }
          len += num;
          size -= num;
     }
          
     return len;
}

static offset_t
av_seek_callback( void *opaque, offset_t offset, int whence )
{
     IFusionSoundMusicProvider_FFmpeg_data *data = opaque;
     unsigned int                           pos  = 0;
     DirectResult                           ret;
    
     switch (whence) {
          case SEEK_SET:
               ret = direct_stream_seek( data->stream, offset );
               break;
          case SEEK_CUR:
               pos = direct_stream_offset( data->stream );
               if (!offset)
                    return pos;
               ret = direct_stream_seek( data->stream, pos+offset );
               break;
          case SEEK_END:
               pos = direct_stream_length( data->stream );
               if (offset == -1)
                    return pos;
               ret = direct_stream_seek( data->stream, pos-offset );
               break;
          default:
               ret = DFB_UNSUPPORTED;
               break;
     }
     
     if (ret != DFB_OK)
          return -1;
          
     return direct_stream_offset( data->stream );
}     

/*****************************************************************************/

typedef struct {
#ifdef WORDS_BIGENDIAN
     s8 c;
     u8 b;
     u8 a;
#else
     u8 a;
     u8 b;
     s8 c;
#endif
} __attribute__((packed)) s24;


#define FFMPEG_MIX_LOOP() \
 do { \
     int n; \
     if (fs_mode_for_channels(s_n) == dst_mode) { \
          if (sizeof(TYPE) == sizeof(s16)) { \
               direct_memcpy( dst, src, len*s_n*sizeof(s16) ); \
          } \
          else { \
               TYPE *d = (TYPE *)dst; \
               for (n = len*s_n; n; n--) { \
                    *d++ = CONV(*src); \
                    src++; \
               } \
          } \
          break; \
     } \
     else { \
          s16 c[6] = { 0, 0, 0, 0, 0, 0 }; \
          TYPE *d  = (TYPE *)dst; \
          for (n = len; n; n--) { \
               int s; \
               switch (s_n) { \
                    case 1: \
                         c[0] = c[2] = src[0]; \
                         break; \
                    case 2: \
                         c[0] = src[0]; \
                         c[2] = src[1]; \
                         break; \
                    case 3: \
                         c[0] = src[0]; \
                         c[1] = src[1]; \
                         c[2] = src[2]; \
                         break; \
                    case 4: \
                         c[0] = src[0]; \
                         c[2] = src[1]; \
                         c[3] = src[2]; \
                         c[4] = src[4]; \
                         break; \
                    default: \
                         c[0] = src[0]; \
                         c[1] = src[1]; \
                         c[2] = src[2]; \
                         c[3] = src[3]; \
                         c[4] = src[4]; \
                         if (s_n > 5) \
                              c[5] = src[5]; \
                         break; \
               } \
               src += s_n; \
               switch (dst_mode) { \
                    case FSCM_MONO: \
                         s = c[0] + c[2]; \
                         if (s_n > 2) { \
                              int sum = (c[1] << 1) + c[3] + c[4]; \
                              s += sum - (sum >> 2); \
                              s >>= 1; \
                              s = CLAMP(s, -32768, 32767); \
                         } else { \
                              s >>= 1; \
                         } \
                         *d++ = CONV(s); \
                         break; \
                    case FSCM_STEREO: \
                    case FSCM_STEREO21: \
                         s = c[0]; \
                         if (s_n > 2) { \
                              int sum = c[1] + c[3]; \
                              s += sum - (sum >> 2); \
                              s = CLAMP(s, -32768, 32767); \
                         } \
                         *d++ = CONV(s); \
                         s = c[2]; \
                         if (s_n > 2) { \
                              int sum = c[1] + c[4]; \
                              s += sum - (sum >> 2); \
                              s = CLAMP(s, -32768, 32767); \
                         } \
                         *d++ = CONV(s); \
                         if (FS_MODE_HAS_LFE(dst_mode)) \
                              *d++ = CONV(c[5]); \
                         break; \
                    case FSCM_STEREO30: \
                    case FSCM_STEREO31: \
                         s = c[0] + c[3] - (c[3] >> 2); \
                         s = CLAMP(s, -32768, 32767); \
                         *d++ = CONV(s); \
                         if (s_n == 2 || s_n == 4) \
                              c[1] = (c[0] + c[2]) >> 1; \
                         *d++ = CONV(c[1]); \
                         s = c[2] + c[4] - (c[4] >> 2); \
                         s = CLAMP(s, -32768, 32767); \
                         *d++ = CONV(s); \
                         if (FS_MODE_HAS_LFE(dst_mode)) \
                              *d++ = CONV(c[5]); \
                         break; \
                    default: \
                         if (FS_MODE_HAS_CENTER(dst_mode)) { \
                              *d++ = CONV(c[0]); \
                              if (s_n == 2 || s_n == 4) { \
                                   s = (c[0] + c[2]) >> 1; \
                                   *d++ = CONV(s); \
                              } else { \
                                   *d++ = CONV(c[1]); \
                              } \
                              *d++ = CONV(c[2]); \
                         } else { \
                              s = c[0] + c[1] - (c[1] >> 2); \
                              s = CLAMP(s, -32768, 32767); \
                              *d++ = CONV(s); \
                              s = c[2] + c[1] - (c[1] >> 2); \
                              s = CLAMP(s, -32768, 32767); \
                              *d++ = CONV(s); \
                         } \
                         if (FS_MODE_NUM_REARS(dst_mode) == 1) { \
                              s = (c[3] + c[4]) >> 1; \
                              *d++ = CONV(s); \
                         } else { \
                              *d++ = CONV(c[3]); \
                              *d++ = CONV(c[4]); \
                         } \
                         if (FS_MODE_HAS_LFE(dst_mode)) \
                              *d++ = CONV(c[5]); \
                         break; \
               } \
          } \
     } \
 } while (0)


static void
ffmpeg_mix_audio( s16 *src, void *dst, int len,
                  FSSampleFormat format, int src_channels, FSChannelMode dst_mode )
{
     int s_n = src_channels;

     switch (format) {
          case FSSF_U8:
               #define TYPE u8
               #define CONV(s) (((s)>>8)+128)
               FFMPEG_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_S16:
               #define TYPE s16
               #define CONV(s) (s)
               FFMPEG_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_S24:
               #define TYPE s24
               #define CONV(s) ((s24){a:0, b:(s), c:(s)>>8})
               FFMPEG_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_S32:
               #define TYPE s32
               #define CONV(s) ((s)<<8)
               FFMPEG_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_FLOAT:
               #define TYPE float
               #define CONV(s) ((float)(s)/32768.f)
               FFMPEG_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          default:
               D_BUG( "unexpected sample format" );
               break;
     }
}

/*****************************************************************************/

static void
IFusionSoundMusicProvider_FFmpeg_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_FFmpeg_data *data = thiz->priv;

     thiz->Stop( thiz );

     if (data->codec)
          avcodec_close( data->codec );
          
     if (data->ctx) { 
          AVInputFormat *iformat = data->ctx->iformat;
          /* Ugly hack to fix a bug (segfault) in url_fclose() */
          if (!(iformat->flags & AVFMT_NOFILE)) {
               iformat->flags |= AVFMT_NOFILE;
               av_close_input_file( data->ctx );
               iformat->flags ^= AVFMT_NOFILE;
          }
          else {
               av_close_input_file( data->ctx );
          }
     }
     
     if (data->iobuf)
          D_FREE( data->iobuf );

     if (data->stream)
          direct_stream_destroy( data->stream );

     pthread_mutex_destroy( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_AddRef( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     data->ref++;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_Release( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (--data->ref == 0)
          IFusionSoundMusicProvider_FFmpeg_Destruct( thiz );
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                                  FSMusicProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (!caps)
          return DFB_INVARG;
          
     *caps = FMCAPS_BASIC;
     if (direct_stream_seekable( data->stream ))
          *caps |= FMCAPS_SEEK;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                                      FSTrackDescription        *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (!desc)
          return DFB_INVARG;

     direct_snputs( desc->artist, data->ctx->author, FS_TRACK_DESC_ARTIST_LENGTH );
     direct_snputs( desc->title, data->ctx->title, FS_TRACK_DESC_TITLE_LENGTH );
     direct_snputs( desc->album, data->ctx->album, FS_TRACK_DESC_ALBUM_LENGTH );
     direct_snputs( desc->genre, data->ctx->genre, FS_TRACK_DESC_GENRE_LENGTH );
     direct_snputs( desc->encoding, data->codec->codec->name, FS_TRACK_DESC_ENCODING_LENGTH );
     desc->year = data->ctx->year;
     desc->bitrate = data->codec->bit_rate;
     desc->replaygain = desc->replaygain_album = 0;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                       FSStreamDescription       *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (!desc)
          return DFB_INVARG;
          
     desc->flags        = FSSDF_SAMPLERATE   | FSSDF_CHANNELS  |
                          FSSDF_SAMPLEFORMAT | FSSDF_BUFFERSIZE;
     desc->samplerate   = data->codec->sample_rate;
     desc->channels     = data->codec->channels;
     desc->sampleformat = FSSF_S16;
     desc->buffersize   = desc->samplerate/10;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                FSBufferDescription       *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (!desc)
          return DFB_INVARG;
          
     desc->flags        = FSBDF_SAMPLERATE   | FSBDF_CHANNELS |
                          FSBDF_SAMPLEFORMAT | FSBDF_LENGTH;
     desc->samplerate   = data->codec->sample_rate;
     desc->channels     = data->codec->channels;
     desc->sampleformat = FSSF_S16;
     if (data->st->nb_frames)
          desc->length = MIN(data->st->nb_frames, FS_MAX_FRAMES);
     else
          desc->length = MIN((s64)data->ctx->duration*desc->samplerate/AV_TIME_BASE, FS_MAX_FRAMES);
     
     return DFB_OK;
}

static void*
FFmpegStreamThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_FFmpeg_data *data = ctx;
     
     while (data->playing && !data->finished) {
          AVPacket  pkt;
          u8       *pkt_data;
          int       pkt_size;
          int       len = 0, size = 0;
          
          pthread_mutex_lock( &data->lock );
          
          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          if (data->seeked) {
               data->dest.stream->Flush( data->dest.stream );
               avcodec_flush_buffers( data->codec );
               data->seeked = false;
          }
          
          if (av_read_frame( data->ctx, &pkt ) < 0) {
               //if (url_feof( data->ctx->pb )) {
                    if (!(data->flags & FMPLAY_LOOPING) || av_seek_frame( data->ctx, -1, 0, 0 ) < 0)
                         data->finished = true;
               //}
               pthread_mutex_unlock( &data->lock );
               continue;
          }
          
          if (pkt.stream_index != data->st->index) {
               pthread_mutex_unlock( &data->lock );
               continue;
          }
          
          for (pkt_data = pkt.data, pkt_size = pkt.size; pkt_size > 0;) {
               int decoded;

               len = AVCODEC_MAX_AUDIO_FRAME_SIZE - size;

               decoded = avcodec_decode_audio2( data->codec,
                                                (s16*)&data->buf[size], &len, 
                                                pkt_data, pkt_size );
               if (decoded < 0)
                    break;
                       
               pkt_data += decoded;
               pkt_size -= decoded;
               if (len > 0)
                    size += len;
          }
          
          size /= data->codec->channels * 2;
          
          if (pkt.pts != AV_NOPTS_VALUE)
               data->pts = av_rescale_q( pkt.pts-data->st->start_time, data->st->time_base, AV_TIME_BASE_Q );
          else if (size)
               data->pts += (s64)size * AV_TIME_BASE / data->codec->sample_rate;
               
          av_free_packet( &pkt );
          
          pthread_mutex_unlock( &data->lock );
          
          if (data->dest.format != FSSF_S16 ||
              FS_CHANNELS_FOR_MODE(data->dest.mode) != data->codec->channels) {
               int  num = FS_BYTES_PER_SAMPLE(data->dest.format)*FS_CHANNELS_FOR_MODE(data->dest.mode);
               u8   dst[1024*num];
               s16 *buf = (s16*)data->buf;
               
               while (size) {
                    len = MIN( size, 1024 );
                    
                    ffmpeg_mix_audio( buf, dst, len, data->dest.format, 
                                      data->codec->channels, data->dest.mode );
                    
                    data->dest.stream->Write( data->dest.stream, dst, len );
                    
                    size -= len;
                    buf  += len * data->codec->channels;
               }
          }
          else {
               data->dest.stream->Write( data->dest.stream, data->buf, size );
          }
     }
     
     return (void*)0;
}                             

static DFBResult
IFusionSoundMusicProvider_FFmpeg_PlayToStream( IFusionSoundMusicProvider *thiz,
                                               IFusionSoundStream        *destination )
{
     FSStreamDescription desc;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )

     if (!destination)
          return DFB_INVARG;
          
     if (data->dest.stream == destination)
          return DFB_OK;
          
     destination->GetDescription( destination, &desc );
     
     /* check whether destination samplerate is supported */
     if (desc.samplerate != data->codec->sample_rate)
          return DFB_UNSUPPORTED;
          
     /* check whether destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_U8:
          case FSSF_S16:
          case FSSF_S24:
          case FSSF_S32:
          case FSSF_FLOAT:
               break;
          default:
               return DFB_UNSUPPORTED;
     }
     
     /* check whether destination mode is supported */
     switch (desc.channelmode) {
          case FSCM_MONO:
          case FSCM_STEREO:
          case FSCM_STEREO21:
          case FSCM_STEREO30:
          case FSCM_STEREO31:
          case FSCM_SURROUND30:
          case FSCM_SURROUND31:
          case FSCM_SURROUND40_2F2R:
          case FSCM_SURROUND41_2F2R:
          case FSCM_SURROUND40_3F1R:
          case FSCM_SURROUND41_3F1R:
          case FSCM_SURROUND50:
          case FSCM_SURROUND51:
               break;
          default:
               return DFB_UNSUPPORTED;
     }
     
     thiz->Stop( thiz );
     
     pthread_mutex_lock( &data->lock );
     
     if (data->finished) {
          if (av_seek_frame( data->ctx, -1, 0, AVSEEK_FLAG_BACKWARD ) < 0) {
               pthread_mutex_unlock( &data->lock );
               return DFB_UNSUPPORTED;
          }
          data->finished = false;
     }
     
     if (!data->buf) {
          data->buf = D_MALLOC( AVCODEC_MAX_AUDIO_FRAME_SIZE );
          if (!data->buf) {
               pthread_mutex_unlock( &data->lock );
               return DFB_NOSYSTEMMEMORY;
          }
     }
     
     destination->AddRef( destination );
     data->dest.stream    = destination;
     data->dest.format    = desc.sampleformat;
     data->dest.mode      = desc.channelmode;
     data->dest.framesize = desc.channels * FS_BYTES_PER_SAMPLE(desc.sampleformat);
     
     /* start thread */
     data->playing = true;
     data->thread  = direct_thread_create( DTT_DEFAULT,
                                           FFmpegStreamThread, data, "FFmpeg" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static void*
FFmpegBufferThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_FFmpeg_data *data = ctx;
     
     int pos = 0;
     
     while (data->playing && !data->finished) {
          AVPacket  pkt;
          u8       *pkt_data;
          int       pkt_size;
          s16      *buf;
          int       len = 0, size = 0;
          
          pthread_mutex_lock( &data->lock );
          
          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          if (data->seeked) {
               data->dest.stream->Flush( data->dest.stream );
               avcodec_flush_buffers( data->codec );
               data->seeked = false;
          }
          
          if (av_read_frame( data->ctx, &pkt ) < 0) {
               //if (url_feof( data->ctx->pb )) {
                    if (!(data->flags & FMPLAY_LOOPING) || av_seek_frame( data->ctx, -1, 0, 0 ) < 0) {
                         data->finished = true;
                         if (pos && data->callback) {
                              if (data->callback( pos, data->callback_data ))
                                   data->playing = false;
                         }
                    }
               //}
               pthread_mutex_unlock( &data->lock );
               continue;
          }
          
          if (pkt.stream_index != data->st->index) {
               pthread_mutex_unlock( &data->lock );
               continue;
          }
          
          for (pkt_data = pkt.data, pkt_size = pkt.size; pkt_size > 0;) {
               int decoded;

               len = AVCODEC_MAX_AUDIO_FRAME_SIZE - size;

               decoded = avcodec_decode_audio2( data->codec,
                                                (s16*)&data->buf[size], &len, 
                                                pkt_data, pkt_size );
               if (decoded < 0)
                    break;
                       
               pkt_data += decoded;
               pkt_size -= decoded;
               if (len > 0)
                    size += len;
          }
          
          size /= data->codec->channels * 2;
          
          if (pkt.pts != AV_NOPTS_VALUE)
               data->pts = av_rescale_q( pkt.pts-data->st->start_time, data->st->time_base, AV_TIME_BASE_Q );
          else if (size)
               data->pts += (s64)size * AV_TIME_BASE / data->codec->sample_rate;
               
          av_free_packet( &pkt );
          
          buf = (s16*)data->buf;
          while (size) {
               void *dst;
               int   num;
               
               if (data->dest.buffer->Lock( data->dest.buffer, &dst, &len, NULL ))
                    break;
               dst += pos * data->dest.framesize;
               len -= pos;
                
               num = MIN( size, len );  
               ffmpeg_mix_audio( buf, dst, num, data->dest.format, 
                                 data->codec->channels, data->dest.mode );
               size -= num;
               buf  += num * data->codec->channels;
               
               pos += num;
               len -= num;           
               
               data->dest.buffer->Unlock( data->dest.buffer );
               
               if (!len) {
                    if (data->callback) {
                         if (data->callback( pos, data->callback_data )) {
                              data->playing = false;
                              break;
                         }
                    }
                    pos = 0;
               }
          }                                   
          
          pthread_mutex_unlock( &data->lock );
     }
     
     return (void*)0;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                               IFusionSoundBuffer        *destination,
                                               FMBufferCallback           callback,
                                               void                      *ctx )
{
     FSBufferDescription desc;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )

     if (!destination)
          return DFB_INVARG;
          
     if (data->dest.buffer == destination)
          return DFB_OK;
          
     destination->GetDescription( destination, &desc );
     
     /* check whether destination samplerate is supported */
     if (desc.samplerate != data->codec->sample_rate)
          return DFB_UNSUPPORTED;
          
     /* check whether destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_U8:
          case FSSF_S16:
          case FSSF_S24:
          case FSSF_S32:
          case FSSF_FLOAT:
               break;
          default:
               return DFB_UNSUPPORTED;
     }
     
     /* check whether destination mode is supported */
     switch (desc.channelmode) {
          case FSCM_MONO:
          case FSCM_STEREO:
          case FSCM_STEREO21:
          case FSCM_STEREO30:
          case FSCM_STEREO31:
          case FSCM_SURROUND30:
          case FSCM_SURROUND31:
          case FSCM_SURROUND40_2F2R:
          case FSCM_SURROUND41_2F2R:
          case FSCM_SURROUND40_3F1R:
          case FSCM_SURROUND41_3F1R:
          case FSCM_SURROUND50:
          case FSCM_SURROUND51:
               break;
          default:
               return DFB_UNSUPPORTED;
     }
     
     thiz->Stop( thiz );
     
     pthread_mutex_lock( &data->lock );
     
     if (data->finished) {
          if (av_seek_frame( data->ctx, -1, 0, AVSEEK_FLAG_BACKWARD ) < 0) {
               pthread_mutex_unlock( &data->lock );
               return DFB_UNSUPPORTED;
          }
          data->finished = false;
     }
     
     if (!data->buf) {
          data->buf = D_MALLOC( AVCODEC_MAX_AUDIO_FRAME_SIZE );
          if (!data->buf) {
               pthread_mutex_unlock( &data->lock );
               return DFB_NOSYSTEMMEMORY;
          }
     }
     
     destination->AddRef( destination );
     data->dest.buffer    = destination;
     data->dest.format    = desc.sampleformat;
     data->dest.mode      = desc.channelmode;
     data->dest.framesize = desc.channels * FS_BYTES_PER_SAMPLE(desc.sampleformat);
     
     data->callback      = callback;
     data->callback_data = ctx;
     
     /* start thread */
     data->playing = true;
     data->thread  = direct_thread_create( DTT_DEFAULT,
                                           FFmpegBufferThread, data, "FFmpeg" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_Stop( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (!data->playing)
          return DFB_OK;
          
     pthread_mutex_lock( &data->lock );
     
     data->playing = false;
     
     /* stop thread */
     if (data->thread) {
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }
     
     /* release previous destination stream */
     if (data->dest.stream) {
          data->dest.stream->Release( data->dest.stream );
          data->dest.stream = NULL;
     }

     /* release previous destination buffer */
     if (data->dest.buffer) {
          data->dest.buffer->Release( data->dest.buffer );
          data->dest.buffer = NULL;
     }
   
     /* release output buffer */  
     if (data->buf) {
          D_FREE( data->buf );
          data->buf = NULL;
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_GetStatus( IFusionSoundMusicProvider *thiz,
                                            FSMusicProviderStatus     *status )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (!status)
          return DFB_INVARG;
          
     if (data->finished)
          *status = FMSTATE_FINISHED;
     else if (data->playing)
          *status = FMSTATE_PLAY;
     else
          *status = FMSTATE_STOP;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_SeekTo( IFusionSoundMusicProvider *thiz,
                                         double                     seconds )
{
     DFBResult ret;
     s64       time;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (seconds < 0.0)
          return DFB_INVARG;
     
     if (!direct_stream_seekable( data->stream ))
          return DFB_UNSUPPORTED;
          
     time = seconds * AV_TIME_BASE;
     if (data->ctx->duration != AV_NOPTS_VALUE && time > data->ctx->duration)
          return DFB_OK;
     
     pthread_mutex_lock( &data->lock );
     
     if (av_seek_frame( data->ctx, -1, time, 
                          (time < data->pts) ? AVSEEK_FLAG_BACKWARD : 0 ) >= 0) {
          data->seeked = true;
          ret = DFB_OK;
     }
     else {
          ret = DFB_FAILURE;
     }
     
     pthread_mutex_unlock( &data->lock );
 
     return ret;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_GetPos( IFusionSoundMusicProvider *thiz,
                                         double                    *seconds )
{
     s64 pos;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (!seconds)
          return DFB_INVARG;
      
     pos = data->pts;      
     if (data->dest.stream) {
          int delay = 0;
          data->dest.stream->GetPresentationDelay( data->dest.stream, &delay );
          pos -= delay * 1000ll;
     }
     pos = MAX( pos, 0 );
     
     *seconds = (double)pos / AV_TIME_BASE; 
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_GetLength( IFusionSoundMusicProvider *thiz,
                                            double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (!seconds)
          return DFB_INVARG;
          
     if (data->ctx->duration != AV_NOPTS_VALUE) {      
          *seconds = (double)data->ctx->duration / AV_TIME_BASE;
          return DFB_OK;
     }
     
     *seconds = 0.0;
     
     return DFB_UNSUPPORTED;
}

static DFBResult
IFusionSoundMusicProvider_FFmpeg_SetPlaybackFlags( IFusionSoundMusicProvider    *thiz,
                                                   FSMusicProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_FFmpeg )
     
     if (flags & ~FMPLAY_LOOPING)
          return DFB_UNSUPPORTED;
          
     data->flags = flags;
     
     return DFB_OK;
}

/*****************************************************************************/

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     AVProbeData    pd;
     AVInputFormat *format;
          
     av_register_all();
          
     pd.filename = ctx->filename;
     pd.buf      = ctx->header;
     pd.buf_size = sizeof(ctx->header);
     
     format = av_probe_input_format( &pd, 1 );
     if (format && format->name) {
          if (!strcmp( format->name, "asf" ) || // wma
              !strcmp( format->name, "rm" )  || // real audio
              !strcmp( format->name, "flac" ))
               return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename,
           DirectStream              *stream )
{
     AVProbeData    pd;
     AVInputFormat *fmt;
     AVCodec       *c;
     unsigned char  buf[64];
     unsigned int   i, len = 0;
      
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_FFmpeg )
     
     data->ref    = 1;
     data->stream = direct_stream_dup( stream );
     
     direct_stream_peek( stream, sizeof(buf), 0, &buf[0], &len );
     
     pd.filename = filename;
     pd.buf      = &buf[0];
     pd.buf_size = len;
     
     fmt = av_probe_input_format( &pd, 1 );
     if (!fmt) {
          IFusionSoundMusicProvider_FFmpeg_Destruct( thiz );
          return DFB_INIT;
     }
     
     data->iobuf = D_MALLOC( 4096 );
     if (!data->iobuf) {
          IFusionSoundMusicProvider_FFmpeg_Destruct( thiz );
          return D_OOM();
     }
     
     if (init_put_byte( &data->pb, data->iobuf, 4096, 0, 
                        (void*)data, av_read_callback, NULL,
                        direct_stream_seekable( stream ) ? av_seek_callback : NULL ) < 0) {
          D_ERROR( "IFusionSoundMusicProvider_FFmpeg: init_put_byte() failed!\n" );
          IFusionSoundMusicProvider_FFmpeg_Destruct( thiz );
          return DFB_INIT;
     }
     
     if (av_open_input_stream( &data->ctx, &data->pb, filename, fmt, NULL ) < 0) {
          D_ERROR( "IFusionSoundMusicProvider_FFmpeg: av_open_input_stream() failed!\n" );
          IFusionSoundMusicProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     if (av_find_stream_info( data->ctx ) < 0) {
          D_ERROR( "IFusionSoundMusicProvider_FFmpeg: couldn't find stream info!\n" );
          IFusionSoundMusicProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     for (i = 0; i < data->ctx->nb_streams; i++) {
          if (data->ctx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
               if (!data->st || data->st->codec->bit_rate < data->ctx->streams[i]->codec->bit_rate)
                    data->st = data->ctx->streams[i];
          }
     }
     
     if (!data->st) {
          D_ERROR( "IFusionSoundMusicProvider_FFmpeg: couldn't find audio stream!\n" );
          IFusionSoundMusicProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     data->codec = data->st->codec;
     c = avcodec_find_decoder( data->codec->codec_id );
     if (!c || avcodec_open( data->codec, c ) < 0) {
          D_ERROR( "IFusionSoundMusicProvider_FFmpeg: couldn't find audio decoder!\n" );
          IFusionSoundMusicProvider_FFmpeg_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     direct_util_recursive_pthread_mutex_init( &data->lock );
     
     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_FFmpeg_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_FFmpeg_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_FFmpeg_GetCapabilities;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_FFmpeg_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_FFmpeg_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_FFmpeg_GetBufferDescription;
     thiz->PlayToStream         = IFusionSoundMusicProvider_FFmpeg_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_FFmpeg_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_FFmpeg_Stop;
     thiz->GetStatus            = IFusionSoundMusicProvider_FFmpeg_GetStatus;
     thiz->SeekTo               = IFusionSoundMusicProvider_FFmpeg_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_FFmpeg_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_FFmpeg_GetLength;
     thiz->SetPlaybackFlags     = IFusionSoundMusicProvider_FFmpeg_SetPlaybackFlags;

     return DFB_OK;
}
     
     
