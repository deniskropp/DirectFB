/*
 * Copyright (C) 2005-2008 Claudio Ciccani <klan@users.sf.net>
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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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


static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename,
           DirectStream              *stream );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, Wave )

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int                           ref;        /* reference counter */

     DirectStream                 *stream;
     u32                           samplerate; /* frequency */
     int                           channels;   /* number of channels */
     FSSampleFormat                format;     /* sample format */
     int                           framesize;  /* bytes per frame */
     u32                           headsize;   /* size of headers */
     u32                           datasize;   /* size of pcm data */
     double                        length;     /* in seconds */

     FSMusicProviderPlaybackFlags  flags;

     DirectThread                 *thread;
     pthread_mutex_t               lock;
     bool                          playing;
     bool                          finished;
     bool                          seeked;

     void                         *src_buffer;

     struct {
          IFusionSoundStream      *stream;
          IFusionSoundBuffer      *buffer;
          FSSampleFormat           format;
          FSChannelMode            mode;
          int                      framesize;
          int                      length;
     } dest;

     FMBufferCallback              callback;
     void                         *ctx;
} IFusionSoundMusicProvider_Wave_data;


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


static inline int
getsamp( u8 *src, const int i, FSSampleFormat f )
{
     switch (f) {
          case FSSF_U8:
               return (src[i]^0x80) << 22;
          case FSSF_S16:
#ifdef WORDS_BIGENDIAN
               return (s16)BSWAP16(((u16*)src)[i]) << 14;
#else
               return ((s16*)src)[i] << 14;
#endif
          case FSSF_S24:
#ifdef WORDS_BIGENDIAN
               return (src[i*3+0] << 22) |
                      (src[i*3+1] << 14) |
                      (src[i*3+2] <<  6);
#else
               return (src[i*3+2] << 22) |
                      (src[i*3+1] << 14) |
                      (src[i*3+0] <<  6);
#endif
          case FSSF_S32:
#ifdef WORDS_BIGENDIAN
               return (s32)BSWAP32(((u32*)src)[i]) >> 2;
#else
               return ((s32*)src)[i] >> 2;
#endif
          default:
               break;
     }
     
     return 0;
}

static inline u8*
putsamp( u8 *dst, FSSampleFormat f, int s )
{
     switch (f) {
          case FSSF_U8:
               *dst = (s >> 22) ^ 0x80;
               dst++;
               break;
          case FSSF_S16:
               *((s16*)dst) = s >> 14;
               dst += 2;
               break;
          case FSSF_S24:
               ((s24*)dst)->c = s >> 22;
               ((s24*)dst)->b = s >> 14;
               ((s24*)dst)->a = s >>  6;
               dst += 3;
               break;
          case FSSF_S32:
               *((s32*)dst) = s << 2;
               dst += 4;
               break;
          case FSSF_FLOAT:
               *((float*)dst) = (float)s/(float)(1<<29);
               dst += 4;
               break;
          default:
               break;
     }
     
     return dst;
}

static void
wave_mix_audio( u8 *src, u8 *dst, int len,
                FSSampleFormat sf, FSSampleFormat df, int sc, FSChannelMode dm )
{
                 /* L  C  R  Rl Rr LFE */
     int c[6]   = { 0, 0, 0, 0, 0, 0 }; /* 30bit samples */
     int sbytes = FS_BYTES_PER_SAMPLE(sf) * sc;
     
#define clip(s) \
     if ((s) >= (1 << 29)) \
          (s) = (1 << 29) - 1; \
     else if ((s) < -(1 << 29)) \
          (s) = -(1 << 29); \
     
     while (len--) {
          int s;
          
          switch (sc) {
               case 1:
                    c[0] = 
                    c[2] = getsamp( src, 0, sf );
                    break;
               case 2:
                    c[0] = getsamp( src, 0, sf );
                    c[2] = getsamp( src, 1, sf );
                    break;
               case 3:
                    c[0] = getsamp( src, 0, sf );
                    c[1] = getsamp( src, 1, sf );
                    c[2] = getsamp( src, 2, sf );
                    break;
               case 4:
                    c[0] = getsamp( src, 0, sf );
                    c[2] = getsamp( src, 1, sf );
                    c[3] = getsamp( src, 2, sf );
                    c[4] = getsamp( src, 3, sf );
                    break;
               default:
                    c[0] = getsamp( src, 0, sf );
                    c[1] = getsamp( src, 1, sf );
                    c[2] = getsamp( src, 2, sf );
                    c[3] = getsamp( src, 3, sf );
                    c[4] = getsamp( src, 4, sf );
                    if (sc > 5)
                         c[5] = getsamp( src, 5, sf );
                    break;
          }
          src += sbytes;
          
          switch (dm) {
               case FSCM_MONO:
                    s = c[0] + c[2];
                    if (sc > 2) {
                         int sum = (c[1] << 1) + c[3] + c[4];
                         s += sum - (sum >> 2);
                         s >>= 1;
                         clip(s);
                    } else {
                         s >>= 1;
                    }
                    dst = putsamp( dst, df, s );
                    break;
               case FSCM_STEREO:
               case FSCM_STEREO21:
                    s = c[0];
                    if (sc > 2) {
                         int sum = c[1] + c[3];
                         s += sum - (sum >> 2);
                         clip(s);
                    }
                    dst = putsamp( dst, df, s );
                    s = c[2];
                    if (sc > 2) {
                         int sum = c[1] + c[4];
                         s += sum - (sum >> 2);
                         clip(s);
                    }
                    dst = putsamp( dst, df, s );
                    if (FS_MODE_HAS_LFE(dm))
                         dst = putsamp( dst, df, c[5] );
                    break;
               case FSCM_STEREO30:
               case FSCM_STEREO31:
                    s = c[0] + (c[3] - (c[3] >> 2));
                    clip(s);
                    dst = putsamp( dst, df, s );
                    if (sc == 2 || sc == 4)
                         dst = putsamp( dst, df, (c[0]+c[2])>>1 );
                    else
                         dst = putsamp( dst, df, c[1] );
                    s = c[2] + (c[4] - (c[4] >> 2));
                    clip(s);
                    dst = putsamp( dst, df, s );
                    if (FS_MODE_HAS_LFE(dm))
                         dst = putsamp( dst, df, c[5] );               
                    break;
               default:
                    if (FS_MODE_HAS_CENTER(dm)) {
                         dst = putsamp( dst, df, c[0] );
                         if (sc == 2 || sc == 4)
                              dst = putsamp( dst, df, (c[0]+c[2])>>1 );
                         else
                              dst = putsamp( dst, df, c[1] );
                         dst = putsamp( dst, df, c[2] );
                    } else {
                         c[0] += c[1] - (c[1] >> 2);
                         c[2] += c[1] - (c[1] >> 2);
                         clip(c[0]);
                         clip(c[2]);
                         dst = putsamp( dst, df, c[0] );
                         dst = putsamp( dst, df, c[2] );
                    }
                    if (FS_MODE_NUM_REARS(dm) == 1) {
                         s = (c[3] + c[4]) >> 1;
                         dst = putsamp( dst, df, s );
                    } else {     
                         dst = putsamp( dst, df, c[3] );
                         dst = putsamp( dst, df, c[4] );
                    }
                    if (FS_MODE_HAS_LFE(dm))
                         dst = putsamp( dst, df, c[5] );
                    break;
          }
     }
     
#undef clip
}


static void
IFusionSoundMusicProvider_Wave_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_Wave_data *data = thiz->priv;

     thiz->Stop( thiz );

     if (data->stream)
          direct_stream_destroy( data->stream );

     pthread_mutex_destroy( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundMusicProvider_Wave_AddRef( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_Release( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (--data->ref == 0)
          IFusionSoundMusicProvider_Wave_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                                FSMusicProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!caps)
          return DFB_INVARG;

     if (direct_stream_seekable( data->stream ))
          *caps = FMCAPS_BASIC | FMCAPS_SEEK;
     else
          *caps = FMCAPS_BASIC;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                                    FSTrackDescription        *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!desc)
          return DFB_INVARG;

     memset( desc, 0, sizeof(FSTrackDescription) );
     snprintf( desc->encoding,
               FS_TRACK_DESC_ENCODING_LENGTH,
               "PCM %dbit little-endian",
               FS_BITS_PER_SAMPLE(data->format) );
     desc->bitrate = data->samplerate * data->channels *
                     FS_BITS_PER_SAMPLE(data->format);

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                     FSStreamDescription       *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!desc)
          return DFB_INVARG;

     desc->flags        = FSSDF_SAMPLERATE   | FSSDF_CHANNELS  |
                          FSSDF_SAMPLEFORMAT | FSSDF_BUFFERSIZE;
     desc->samplerate   = data->samplerate;
     desc->channels     = data->channels;
     desc->sampleformat = data->format;
     desc->buffersize   = data->samplerate/10;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                     FSBufferDescription       *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!desc)
          return DFB_INVARG;

     desc->flags        = FSBDF_LENGTH       | FSBDF_CHANNELS  |
                          FSBDF_SAMPLEFORMAT | FSBDF_SAMPLERATE;
     desc->samplerate   = data->samplerate;
     desc->channels     = data->channels;
     desc->sampleformat = data->format;
     desc->length       = data->datasize / data->framesize;
     if (desc->length > FS_MAX_FRAMES)
          desc->length = FS_MAX_FRAMES;

     return DFB_OK;
}

static void*
WaveStreamThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Wave_data *data = ctx;

     int   count = data->dest.length * data->framesize;
     void *src   = data->src_buffer;

     while (data->playing && !data->finished) {
          DFBResult      ret;
          unsigned int   len = 0;
          unsigned int   pos = 0;
          struct timeval tv  = { 0, 1000 };
          void          *dst;
          int            num;

          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          if (data->seeked) {
               data->dest.stream->Flush( data->dest.stream );
               data->seeked = false;
          }
          
          if (!data->src_buffer) {
               /* direct copy */
               if (data->dest.stream->Access( data->dest.stream, &dst, &num )) {
                    pthread_mutex_unlock( &data->lock );
                    continue;
               }
               src = dst;
               count = num * data->framesize;
          }       

          ret = direct_stream_wait( data->stream, count, &tv );
          if (ret != DFB_TIMEOUT) {
               ret = direct_stream_read( data->stream, count, src, &len );
               len /= data->framesize;
          }
               
          if (!data->src_buffer) {
               /* direct copy */
               data->dest.stream->Commit( data->dest.stream, len );
          }

          if (ret) {
               if (ret == DFB_EOF) {
                    if (data->flags & FMPLAY_LOOPING)
                         direct_stream_seek( data->stream, data->headsize );
                    else
                         data->finished = true;
               }
               pthread_mutex_unlock( &data->lock );
               continue;
          }

          pthread_mutex_unlock( &data->lock );
          
          if (data->src_buffer) {
               /* convert */
               while (pos < len) {
                    if (data->dest.stream->Access( data->dest.stream, &dst, &num ))
                         break;
                    
                    if (num > len - pos)
                         num = len - pos;
                         
                    wave_mix_audio( src + pos*data->framesize, dst, num,
                                    data->format, data->dest.format,
                                    data->channels, data->dest.mode );        
                    
                    data->dest.stream->Commit( data->dest.stream, num );
                    
                    pos += num;
               }
          }
          else {
               /* Avoid blocking while the mutex is locked. */
               data->dest.stream->Wait( data->dest.stream, 1 );
          }
     }

     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Wave_PlayToStream( IFusionSoundMusicProvider *thiz,
                                             IFusionSoundStream        *destination )
{
     FSStreamDescription desc;
     int                 src_size;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!destination)
          return DFB_INVARG;
          
     if (data->dest.stream == destination)
          return DFB_OK;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
     if (desc.samplerate != data->samplerate)
          return DFB_UNSUPPORTED;

     /* check if destination format is supported */
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
     
     /* check if destination mode is supported */
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

     if (desc.sampleformat != data->format ||
         desc.channelmode  != fs_mode_for_channels(data->channels))
     {
          /* allocate buffer */
          src_size = desc.buffersize * data->channels * FS_BYTES_PER_SAMPLE(data->format);
          data->src_buffer = D_MALLOC( src_size );
          if (!data->src_buffer) {
               pthread_mutex_unlock( &data->lock );
               return D_OOM();
          }
     }

     /* reference destination stream */
     destination->AddRef( destination );
     data->dest.stream    = destination;
     data->dest.format    = desc.sampleformat;
     data->dest.mode      = desc.channelmode;
     data->dest.framesize = desc.channels * FS_BYTES_PER_SAMPLE(desc.sampleformat);
     data->dest.length    = desc.buffersize;

     if (data->finished) {
          direct_stream_seek( data->stream, data->headsize );
          data->finished = false;
     }

     /* start thread */
     data->playing = true;
     data->thread  = direct_thread_create( DTT_DEFAULT,
                                           WaveStreamThread, data, "Wave" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static void*
WaveBufferThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Wave_data *data =
          (IFusionSoundMusicProvider_Wave_data*) ctx;

     IFusionSoundBuffer *buffer = data->dest.buffer;
     size_t              count  = data->dest.length * data->framesize;

     while (data->playing && !data->finished) {
          DFBResult       ret;
          void           *dst;
          int             size;
          unsigned int    len  = 0;
          struct timeval  tv   = { 0, 1000 };
          
          pthread_mutex_lock( &data->lock );
          
          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }

          if (!data->src_buffer) {
               if (buffer->Lock( buffer, &dst, 0, &size ) != DFB_OK) {
                    D_ERROR( "IFusionSoundMusicProvider_Wave: "
                             "Couldn't lock buffer!" );
                    pthread_mutex_unlock( &data->lock );
                    break;
               }
          } else {
               dst  = data->src_buffer;
               size = count;
          }

          ret = direct_stream_wait( data->stream, size, &tv );
          if (ret != DFB_TIMEOUT)
               ret = direct_stream_read( data->stream, size, dst, &len );

          if (!data->src_buffer)
               buffer->Unlock( buffer );

          if (ret) {
               if (ret == DFB_EOF) {
                    if (data->flags & FMPLAY_LOOPING)
                         direct_stream_seek( data->stream, data->headsize );
                    else
                         data->finished = true;
               }
               pthread_mutex_unlock( &data->lock );
               continue;
          }

          pthread_mutex_unlock( &data->lock );

          len /= data->framesize;
          if (len < 1)
               continue;

          if (data->src_buffer) {
               while (len > 0) {
                    if (buffer->Lock( buffer, &dst, &size, 0 ) != DFB_OK) {
                         D_ERROR( "IFusionSoundMusicProvider_Wave: "
                                  "Couldn't lock buffer!" );
                         break;
                    }

                    if (size > len)
                         size = len;

                    wave_mix_audio( data->src_buffer, dst, size,
                                    data->format, data->dest.format,
                                    data->channels, data->dest.mode );

                    buffer->Unlock( buffer );

                    len -= size;

                    if (data->callback) {
                         if (data->callback( size, data->ctx )) {
                              data->playing = false;
                              break;
                         }
                    }
               }
          }
          else {
               if (data->callback) {
                    if (data->callback( len, data->ctx ))
                         data->playing = false;
               }
          }
     }

     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Wave_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                             IFusionSoundBuffer        *destination,
                                             FMBufferCallback           callback,
                                             void                      *ctx )
{
     FSBufferDescription desc;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!destination)
          return DFB_INVARG;
          
     if (data->dest.buffer == destination)
          return DFB_OK;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
     if (desc.samplerate != data->samplerate)
          return DFB_UNSUPPORTED;

     /* check if destination format is supported */
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
     
     /* check if destination mode is supported */
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

     /* allocate buffer */
     if (desc.sampleformat != data->format || 
         desc.channelmode != fs_mode_for_channels(data->channels)) {
          data->src_buffer = D_MALLOC( desc.length * data->channels * 
                                       FS_BYTES_PER_SAMPLE(data->format) );
          if (!data->src_buffer) {
               pthread_mutex_unlock( &data->lock );
               return D_OOM();
          }
     }

     /* reference destination stream */
     destination->AddRef( destination );
     data->dest.buffer    = destination;
     data->dest.format    = desc.sampleformat;
     data->dest.mode      = desc.channelmode;
     data->dest.framesize = desc.channels * FS_BYTES_PER_SAMPLE(desc.sampleformat);
     data->dest.length    = desc.length;

     data->callback    = callback;
     data->ctx         = ctx;

     if (data->finished) {
          direct_stream_seek( data->stream, data->headsize );
          data->finished = false;
     }

     /* start thread */
     data->playing = true;
     data->thread  = direct_thread_create( DTT_DEFAULT,
                                           WaveBufferThread, data, "Wave" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_Stop( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = false;
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }

     /* release buffer(s) */
     if (data->src_buffer) {
          D_FREE( data->src_buffer );
          data->src_buffer = NULL;
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

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_GetStatus( IFusionSoundMusicProvider *thiz,
                                          FSMusicProviderStatus     *status )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!status)
          return DFB_INVARG;

     if (data->finished) {
          *status = FMSTATE_FINISHED;
     }
     else if (data->playing) {
          *status = FMSTATE_PLAY;
     }
     else {
          *status = FMSTATE_STOP;
     }

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_SeekTo( IFusionSoundMusicProvider *thiz,
                                       double                     seconds )
{
     DFBResult ret;
     int       offset;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (seconds < 0.0)
          return DFB_INVARG;

     offset  = (double)data->samplerate * seconds;
     offset  = offset * data->framesize;
     if (data->datasize && offset > data->datasize)
          return DFB_UNSUPPORTED;
     offset += data->headsize;

     pthread_mutex_lock( &data->lock );
     ret = direct_stream_seek( data->stream, offset );
     if (ret == DFB_OK)
          data->seeked = true;
     pthread_mutex_unlock( &data->lock );

     return ret;
}

static DFBResult
IFusionSoundMusicProvider_Wave_GetPos( IFusionSoundMusicProvider *thiz,
                                       double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!seconds)
          return DFB_INVARG;

     *seconds = (double) direct_stream_offset( data->stream ) /
                (double)(data->samplerate * data->framesize);

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_GetLength( IFusionSoundMusicProvider *thiz,
                                          double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!seconds)
          return DFB_INVARG;

     *seconds = data->length;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_SetPlaybackFlags( IFusionSoundMusicProvider    *thiz,
                                                 FSMusicProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (flags & ~FMPLAY_LOOPING)
          return DFB_UNSUPPORTED;

     if (flags & FMPLAY_LOOPING && !direct_stream_seekable( data->stream ))
          return DFB_UNSUPPORTED;

     data->flags = flags;

     return DFB_OK;
}


static DFBResult
parse_headers( DirectStream *stream, u32 *ret_samplerate,
               int *ret_channels,    int *ret_format,
               u32 *ret_headsize,    u32 *ret_datasize )
{
     char buf[4];
     u32  fmt_len;
     u32  data_size;
     struct {
          u16 encoding;
          u16 channels;
          u32 frequency;
          u32 byterate;
          u16 blockalign;
          u16 bitspersample;
     } fmt;

#define wave_read( buf, count ) {\
     unsigned int len = 0; \
     direct_stream_wait( stream, count, NULL ); \
     if (direct_stream_read( stream, count, buf, &len ) || len < (count)) \
          return DFB_UNSUPPORTED;\
}

     wave_read( buf, 4 );
     if (buf[0] != 'R' || buf[1] != 'I' || buf[2] != 'F' || buf[3] != 'F') {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: No RIFF header found.\n" );
          return DFB_UNSUPPORTED;
     }

     /* actually ignore chunksize */
     wave_read( buf, 4 );

     wave_read( buf, 4 );
     if (buf[0] != 'W' || buf[1] != 'A' || buf[2] != 'V' || buf[3] != 'E') {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: No WAVE header found.\n" );
          return DFB_UNSUPPORTED;
     }

     wave_read( buf, 4 );
     if (buf[0] != 'f' || buf[1] != 'm' || buf[2] != 't' || buf[3] != ' ') {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: Expected 'fmt ' header.\n" );
          return DFB_UNSUPPORTED;
     }

     wave_read( &fmt_len, 4 );
#ifdef WORDS_BIGENDIAN
     fmt_len = BSWAP32(fmt_len);
#endif
     if (fmt_len < sizeof(fmt)) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "fmt chunk expected to be at least %zu bytes (got %d).\n",
                   sizeof(fmt), fmt_len );
          return DFB_UNSUPPORTED;
     }

     wave_read( &fmt, sizeof(fmt) );
#ifdef WORDS_BIGENDIAN
     fmt.encoding      = BSWAP16(fmt.encoding);
     fmt.channels      = BSWAP16(fmt.channels);
     fmt.frequency     = BSWAP32(fmt.frequency);
     fmt.byterate      = BSWAP32(fmt.byterate);
     fmt.blockalign    = BSWAP16(fmt.blockalign);
     fmt.bitspersample = BSWAP16(fmt.bitspersample);
#endif

     if (fmt.encoding != 1) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Unsupported encoding (%d).\n", fmt.encoding );
          return DFB_UNSUPPORTED;
     }
     if (fmt.channels < 1) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Invalid number of channels (%d).\n", fmt.channels );
          return DFB_UNSUPPORTED;
     }
     if (fmt.frequency < 1000) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Unsupported frequency (%dHz).\n", fmt.frequency );
          return DFB_UNSUPPORTED;
     }
     if (fmt.bitspersample !=  8 && fmt.bitspersample != 16 &&
         fmt.bitspersample != 24 && fmt.bitspersample != 32) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Unsupported sample format (%d bits).\n", fmt.bitspersample );
          return DFB_UNSUPPORTED;
     }
     if (fmt.byterate != (fmt.frequency * fmt.channels * fmt.bitspersample >> 3)) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Invalid byterate (%d).\n", fmt.byterate );
          return DFB_UNSUPPORTED;
     }
     if (fmt.blockalign != (fmt.channels * fmt.bitspersample >> 3)) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Invalid blockalign (%d).\n", fmt.blockalign );
          return DFB_UNSUPPORTED;
     }

     if (fmt_len > sizeof(fmt)) {
          char tmp[fmt_len - sizeof(fmt)];
          wave_read( tmp, fmt_len - sizeof(fmt) );
     }

     while (true) {
          wave_read( buf, 4 );

          wave_read( &data_size, 4 );
#ifdef WORDS_BIGENDIAN
          data_size = BSWAP32(data_size);
#endif

          if (buf[0] != 'd' || buf[1] != 'a' || buf[2] != 't' || buf[3] != 'a') {
               D_DEBUG( "IFusionSoundMusicProvider_Wave: Expected 'data' header, got '%c%c%c%c'.\n", buf[0], buf[1], buf[2], buf[3] );

               if (data_size) {
                    char tmp[data_size];
                    wave_read( tmp, data_size );
               }
          }
          else
               break;
     }

     if (ret_samplerate)
          *ret_samplerate = fmt.frequency;
     if (ret_channels)
          *ret_channels = fmt.channels;
     if (ret_format)
          *ret_format = fmt.bitspersample;
     if (ret_headsize)
          *ret_headsize = fmt_len + 28;
     if (ret_datasize)
          *ret_datasize = data_size;

#undef wave_read

     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     if (!memcmp( ctx->header+0, "RIFF", 4 ) &&
         !memcmp( ctx->header+8, "WAVEfmt ", 8 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename,
           DirectStream              *stream )
{
     DFBResult    ret;
     unsigned int size;
     int          format;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_Wave )

     data->ref    = 1;
     data->stream = direct_stream_dup( stream );

     ret = parse_headers( data->stream,    &data->samplerate,
                          &data->channels, &format,
                          &data->headsize, &data->datasize );
     if (ret) {
          IFusionSoundMusicProvider_Wave_Destruct( thiz );
          return ret;
     }
     
     switch (format) {
          case 8:
               data->format = FSSF_U8;
               break;
          case 16:
               data->format = FSSF_S16;
               break;
          case 24:
               data->format = FSSF_S24;
               break;
          case 32:
               data->format = FSSF_S32;
               break;
          default:
               D_BUG( "unexpected sample format" );
               IFusionSoundMusicProvider_Wave_Destruct( thiz );
               return DFB_BUG;
     }
     
     data->framesize = data->channels * FS_BYTES_PER_SAMPLE(data->format);

     size = direct_stream_length( data->stream );
     if (size) {
          if (data->datasize)
               data->datasize = MIN( data->datasize, size - data->headsize );
          else
               data->datasize = size - data->headsize;
     }

     data->length = (double) data->datasize /
                    (double)(data->samplerate * data->framesize);

     direct_util_recursive_pthread_mutex_init( &data->lock );

     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_Wave_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Wave_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_Wave_GetCapabilities;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_Wave_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_Wave_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_Wave_GetBufferDescription;
     thiz->PlayToStream         = IFusionSoundMusicProvider_Wave_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_Wave_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_Wave_Stop;
     thiz->GetStatus            = IFusionSoundMusicProvider_Wave_GetStatus;
     thiz->SeekTo               = IFusionSoundMusicProvider_Wave_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_Wave_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_Wave_GetLength;
     thiz->SetPlaybackFlags     = IFusionSoundMusicProvider_Wave_SetPlaybackFlags;

     return DFB_OK;
}

