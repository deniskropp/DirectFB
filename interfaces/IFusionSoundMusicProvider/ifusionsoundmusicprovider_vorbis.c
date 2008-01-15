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
#include <unistd.h>
#include <errno.h>

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

#ifdef USE_TREMOR
# include <tremor/ivorbiscodec.h>
# include <tremor/ivorbisfile.h>
#else
# include <vorbis/codec.h>
# include <vorbis/vorbisfile.h>
#endif

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename,
           DirectStream              *stream );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, Vorbis )

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int                           ref;       /* reference counter */

     DirectStream                 *stream;

     OggVorbis_File                vf;
     vorbis_info                  *info;

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
          int                      length;
     } dest;

     FMBufferCallback              callback;
     void                         *ctx;
} IFusionSoundMusicProvider_Vorbis_data;


/* Mixing Routines */

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


#ifdef USE_TREMOR

#define VORBIS_MIX_LOOP() \
 do { \
     int n; \
     if (d_n == s_n) { \
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
     else if (d_n < s_n) { \
          if (d_n == 1 && s_n == 2) { \
               /* Downmix to stereo to mono */ \
               TYPE *d = (TYPE *)dst; \
               for (n = len; n; n--) { \
                    *d++ = CONV((src[0]+src[1])>>1); \
                    src += 2; \
               } \
               break; \
          } \
     } \
     else if (d_n > s_n) { \
          if (d_n == 2 && s_n == 1) { \
               /* Upmix stereo to mono */ \
               TYPE *d = (TYPE *)dst; \
               for (n = len; n; n--) { \
                    d[0] = d[1] = CONV(*src); \
                    d += 2; \
                    src++; \
               } \
               break; \
          } \
          memset( dst, 0, len * d_n * sizeof(TYPE) ); \
     } \
     for (i = 0; i < MIN(s_n,d_n); i++) { \
          s16  *s = &src[i]; \
          TYPE *d = &((TYPE *)dst)[i]; \
          int   n; \
          for (n = len; n; n--) { \
               *d  = CONV(*s); \
                d += d_n; \
                s += s_n; \
          } \
     } \
 } while (0)

static void
vorbis_mix_audio( s16 *src, void *dst, int pos, int len,
                  FSSampleFormat format, int src_channels, FSChannelMode dst_mode )
{
     int s_n = src_channels;
     int d_n = FS_CHANNELS_FOR_MODE(dst_mode);
     int i;
     
     if (pos)
          src += pos * s_n;

     switch (format) {
          case FSSF_U8:
               #define TYPE u8
               #define CONV(s) (((s)>>8)+128)
               VORBIS_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_S16:
               #define TYPE s16
               #define CONV(s) (s)
               VORBIS_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_S24:
               #define TYPE s24
               #define CONV(s) ({ s16 t=(s); (s24){a:0, b:t, c:t>>8}; })
               VORBIS_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_S32:
               #define TYPE s32
               #define CONV(s) ((s)<<8)
               VORBIS_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_FLOAT:
               #define TYPE float
               #define CONV(s) ((float)(s)/32768.f)
               VORBIS_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          default:
               D_BUG( "unexpected sample format" );
               break;
     }
}

#else /* !USE_TREMOR */

static __inline__ u8
FtoU8( float s )
{
     int d;
     d  = s * 128.f + 128.5f;
     return CLAMP( d, 0, 255 );
}

static __inline__ s16
FtoS16( float s )
{
     int d;
     d = s * 32768.f + 0.5f;
     return CLAMP( d, -32768, 32767 );
}

static __inline__ s24
FtoS24( float s )
{
     int d;
     d = s * 8388608.f + 0.5f;
     d = CLAMP( d, -8388608, 8388607 );
     return (s24) { a:d, b:d>>8, c:d>>16 };
}

static __inline__ s32
FtoS32( float s )
{
     s = CLAMP( s, -1.f, 1.f );
     return s * 2147483647.f;
}

static __inline__ float
FtoF32( float s )
{
     return CLAMP( s, -1.f, 1.f );
}

#define VORBIS_MIX_LOOP() \
 do { \
     int i; \
     if (fs_mode_for_channels(s_n) == dst_mode) { \
          for (i = 0; i < s_n; i++) { \
               float *s = src[i] + pos; \
               TYPE  *d = &((TYPE *)dst)[i]; \
               int    n; \
               for (n = len; n; n--) { \
                    *d = CONV(*s); \
                    d += d_n; \
                    s++; \
               } \
          } \
     } \
     else { \
          float c[6] = { /*L*/0, /*C*/0, /*R*/0, /*Rl*/0, /*Rr*/0, /*LFE*/0 }; \
          TYPE *d    = (TYPE *)dst; \
          for (i = pos; i < pos+len; i++) { \
               float s; \
               switch (s_n) { \
                    case 1: \
                         c[0] = c[2] = src[0][i]; \
                         break; \
                    case 4: \
                         c[3] = src[2][i]; \
                         c[4] = src[3][i]; \
                    case 2: \
                         c[0] = src[0][i]; \
                         c[2] = src[1][i]; \
                         break; \
                    case 6: \
                         c[5] = src[5][i]; \
                    case 5: \
                         c[3] = src[3][i]; \
                         c[4] = src[4][i]; \
                    case 3: \
                         c[0] = src[0][i]; \
                         c[1] = src[1][i]; \
                         c[2] = src[2][i]; \
                         break; \
                    default: \
                         break; \
               } \
               switch (dst_mode) { \
                    case FSCM_MONO: \
                         s = c[0] + c[2]; \
                         if (s_n > 2) s += (c[1]*2+c[3]+c[4])*0.7079f; \
                         s *= 0.5f; \
                         *d++ = CONV(s); \
                         break; \
                    case FSCM_STEREO: \
                    case FSCM_STEREO21: \
                         s = c[0]; \
                         if (s_n > 2) s += (c[1]+c[3])*0.7079f; \
                         *d++ = CONV(s); \
                         s = c[2]; \
                         if (s_n > 2) s += (c[1]+c[4])*0.7079f; \
                         *d++ = CONV(s); \
                         if (FS_MODE_HAS_LFE(dst_mode)) \
                              *d++ = CONV(c[5]); \
                         break; \
                    case FSCM_STEREO30: \
                    case FSCM_STEREO31: \
                         s = c[0] + c[3]*0.7079f; \
                         *d++ = CONV(s); \
                         s = (s_n == 2 || s_n == 4) ? ((c[0]+c[2])*0.5f) : c[1]; \
                         *d++ = CONV(s); \
                         s = c[2] + c[4]*0.7079f; \
                         *d++ = CONV(s); \
                         if (FS_MODE_HAS_LFE(dst_mode)) \
                              *d++ = CONV(c[5]); \
                         break; \
                    default: \
                         if (FS_MODE_HAS_CENTER(dst_mode)) { \
                              *d++ = CONV(c[0]); \
                              if (s_n == 2 || s_n == 4) { \
                                   s = (c[0] + c[2]) * 0.5f; \
                                   *d++ = CONV(s); \
                              } else { \
                                   *d++ = CONV(c[1]); \
                              } \
                              *d++ = CONV(c[2]); \
                         } else { \
                              s = c[0] + c[1]*0.7079f; \
                              *d++ = CONV(s); \
                              s = c[2] + c[1]*0.7079f; \
                              *d++ = CONV(s); \
                         } \
                         if (FS_MODE_NUM_REARS(dst_mode) == 1) { \
                              s = (c[3] + c[4]) * 0.5f; \
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
vorbis_mix_audio( float **src, void *dst, int pos, int len,
                  FSSampleFormat format, int src_channels, FSChannelMode dst_mode )
{
     int s_n = src_channels;
     int d_n = FS_CHANNELS_FOR_MODE(dst_mode);

     switch (format) {
          case FSSF_U8:
               #define TYPE u8
               #define CONV FtoU8
               VORBIS_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_S16:
               #define TYPE s16
               #define CONV FtoS16
               VORBIS_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_S24:
               #define TYPE s24
               #define CONV FtoS24
               VORBIS_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_S32:
               #define TYPE s32
               #define CONV FtoS32
               VORBIS_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          case FSSF_FLOAT:
               #define TYPE float
               #define CONV FtoF32
               VORBIS_MIX_LOOP();
               #undef TYPE
               #undef CONV
               break;

          default:
               D_BUG( "unexpected sample format" );
               break;
     }
}

#endif /* USE_TREMOR */

/* I/O callbacks */

static size_t
ov_read_callback( void *dst, size_t size, size_t nmemb, void *ctx )
{
     DirectStream *stream = ctx;
     size_t        total  = size * nmemb;
     size_t        length = 0;
     DirectResult  ret;

     while (length < total) {
          unsigned int read = 0;
          direct_stream_wait( stream, total-length, NULL );
          ret = direct_stream_read( stream, total-length, dst+length, &read );
          if (ret) {
               if (!length)
                    return (ret == DFB_EOF) ? 0 : -1;
               break;
          }
          length += read;
     }

     return length;
}

static int
ov_seek_callback( void *ctx, ogg_int64_t offset, int whence )
{
     DirectStream *stream = ctx;
     unsigned int  pos    = 0;
     DirectResult  ret    = DFB_UNSUPPORTED;

     if (!direct_stream_seekable( stream ) || direct_stream_remote( stream ))
          return -1;

     switch (whence) {
          case SEEK_SET:
               break;
          case SEEK_CUR:
               pos = direct_stream_offset( stream );
               offset += pos;
               break;
          case SEEK_END:
               pos = direct_stream_length( stream );
               if (offset < 0)
                    return pos;
               offset = pos-offset;
               break;
          default:
               offset = -1;
               break;
     }

     if (offset >= 0)
          ret = direct_stream_seek( stream, offset );
     if (ret) {
          errno = -1;
          return -1;
     }

     errno = 0;
     return direct_stream_offset( stream );
}

static long
ov_tell_callback( void *ctx )
{
     DirectStream *stream = ctx;

     return direct_stream_offset( stream );
}

static int
ov_close_callback( void *ctx )
{
     return 0;
}

/* provider methods */

static void
IFusionSoundMusicProvider_Vorbis_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_Vorbis_data *data = thiz->priv;

     thiz->Stop( thiz );

     ov_clear( &data->vf );

     if (data->stream)
          direct_stream_destroy( data->stream );

     pthread_mutex_destroy( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_AddRef( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_Release( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (--data->ref == 0)
          IFusionSoundMusicProvider_Vorbis_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                                  FSMusicProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!caps)
          return DFB_INVARG;

#ifdef USE_TREMOR
     *caps = FMCAPS_BASIC;
#else
     *caps = FMCAPS_BASIC | FMCAPS_HALFRATE;
#endif
     if (direct_stream_seekable( data->stream ))
          *caps |= FMCAPS_SEEK;

     return DFB_OK;
}

static inline float
vorbis_compute_gain( const char *rg_gain, const char *rg_peak )
{
     float gain, peak = 1.0f;

     if (rg_peak)
          peak = atof( rg_peak ) ? : 1.0f;

     gain = pow( 10.0f, atof( rg_gain ) / 20.0f );
     if (gain*peak > 1.0f)
          gain = 1.0f / peak;

     return gain;
}

static void
vorbis_get_metadata( OggVorbis_File     *vf,
                     FSTrackDescription *desc )
{
     vorbis_comment   *vc        = ov_comment( vf, -1 );
     char           **ptr        = vc->user_comments;
     char            *track_gain = NULL;
     char            *track_peak = NULL;
     char            *album_gain = NULL;
     char            *album_peak = NULL;

     memset( desc, 0, sizeof(FSTrackDescription) );

     while (*ptr) {
          char *comment = *ptr;

          if (!strncasecmp( comment, "ARTIST=", sizeof("ARTIST=")-1 )) {
               strncpy( desc->artist,
                        comment + sizeof("ARTIST=") - 1,
                        FS_TRACK_DESC_ARTIST_LENGTH - 1 );
          }
          else if (!strncasecmp( comment, "TITLE=", sizeof("TITLE=")-1 )) {
               strncpy( desc->title,
                        comment + sizeof("TITLE=") - 1,
                        FS_TRACK_DESC_TITLE_LENGTH - 1 );
          }
          else if (!strncasecmp( comment, "ALBUM=", sizeof("ALBUM=")-1 )) {
               strncpy( desc->album,
                        comment + sizeof("ALBUM=") - 1,
                        FS_TRACK_DESC_ALBUM_LENGTH - 1 );
          }
          else if (!strncasecmp( comment, "GENRE=", sizeof("GENRE=")-1 )) {
               strncpy( desc->genre,
                        comment + sizeof("GENRE=") - 1,
                        FS_TRACK_DESC_GENRE_LENGTH - 1 );
          }
          else if (!strncasecmp( comment, "DATE=", sizeof("DATE=")-1 )) {
               desc->year = atoi( comment + sizeof("DATE=") );
          }
          else if (!strncasecmp( comment, "REPLAYGAIN_TRACK_GAIN=",
                                   sizeof("REPLAYGAIN_TRACK_GAIN=")-1 )) {
               track_gain = comment + sizeof("REPLAYGAIN_TRACK_GAIN=") - 1;
          }
          else if (!strncasecmp( comment, "REPLAYGAIN_ALBUM_GAIN=",
                                   sizeof("REPLAYGAIN_ALBUM_GAIN=")-1 )) {
               album_gain = comment + sizeof("REPLAYGAIN_ALBUM_GAIN=") - 1;
          }
          else if (!strncasecmp( comment, "REPLAYGAIN_TRACK_PEAK=",
                                   sizeof("REPLAYGAIN_TRACK_PEAK=")-1 )) {
               track_peak = comment + sizeof("REPLAYGAIN_TRACK_PEAK=") - 1;
          }
          else if (!strncasecmp( comment, "REPLAYGAIN_ALBUM_PEAK=",
                                   sizeof("REPLAYGAIN_ALBUM_PEAK=")-1 )) {
               album_peak = comment + sizeof("REPLAYGAIN_ALBUM_PEAK=") - 1;
          }

          ptr++;
     }

     snprintf( desc->encoding,
               FS_TRACK_DESC_ENCODING_LENGTH, "Vorbis" );

     desc->bitrate = ov_bitrate( vf, -1 ) ? : ov_bitrate_instant( vf );
     if (track_gain)
          desc->replaygain = vorbis_compute_gain( track_gain, track_peak );
     if (album_gain)
          desc->replaygain_album = vorbis_compute_gain( album_gain, album_peak );
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                                      FSTrackDescription        *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!desc)
          return DFB_INVARG;

     vorbis_get_metadata( &data->vf, desc );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                       FSStreamDescription       *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!desc)
          return DFB_INVARG;

     desc->flags        = FSSDF_SAMPLERATE   | FSSDF_CHANNELS  |
                          FSSDF_SAMPLEFORMAT | FSSDF_BUFFERSIZE;
     desc->samplerate   = data->info->rate;
     desc->channels     = data->info->channels;
#ifdef USE_TREMOR
     desc->sampleformat = FSSF_S16;
#else
     desc->sampleformat = FSSF_FLOAT;
#endif
     desc->buffersize   = desc->samplerate/8;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                       FSBufferDescription       *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!desc)
          return DFB_INVARG;

     desc->flags        = FSBDF_SAMPLERATE   | FSBDF_CHANNELS |
                          FSBDF_SAMPLEFORMAT | FSBDF_LENGTH;
     desc->samplerate   = data->info->rate;
     desc->channels     = data->info->channels;
#ifdef USE_TREMOR
     desc->sampleformat = FSSF_S16;
#else
     desc->sampleformat = FSSF_FLOAT;
#endif
     desc->length       = MIN(ov_pcm_total( &data->vf, -1 ), FS_MAX_FRAMES);

     return DFB_OK;
}

static void*
VorbisStreamThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Vorbis_data *data =
          (IFusionSoundMusicProvider_Vorbis_data*) ctx;

#ifdef USE_TREMOR
     s16     src[2048]; // interleaved
#else
     float **src;      // non-interleaved
#endif
     int     section = 0;

     while (data->playing && !data->finished) {
          int length;
          int pos = 0;

          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          if (data->seeked) {
               data->dest.stream->Flush( data->dest.stream );
               data->seeked = false;
          }

#ifdef USE_TREMOR
          length = ov_read( &data->vf, (char*)&src[0], sizeof(src), &section );
          length = (length > 0) ? (length/(data->info->channels*2)) : length;
#else
          length = ov_read_float( &data->vf, &src, data->dest.length, &section );
#endif
          if (length == 0) {
               if (data->flags & FMPLAY_LOOPING) {
                    if (direct_stream_remote( data->stream ))
                         direct_stream_seek( data->stream, 0 );
                    else
                         ov_time_seek( &data->vf, 0 );
               }
               else {
                    data->finished = true;
               }
          }

          pthread_mutex_unlock( &data->lock );

          while (pos < length) {
               void *dst;
               int   len;
                    
               if (data->dest.stream->Access( data->dest.stream, &dst, &len ))
                    break;
                         
               if (len > length-pos)
                    len = length-pos;
                         
               vorbis_mix_audio( src, dst, pos, len, data->dest.format,
                                 data->info->channels, data->dest.mode );

               data->dest.stream->Commit( data->dest.stream, len );
                    
               pos += len;
          }
     }

     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_PlayToStream( IFusionSoundMusicProvider *thiz,
                                               IFusionSoundStream        *destination )
{
     FSStreamDescription desc;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!destination)
          return DFB_INVARG;
          
     if (data->dest.stream == destination)
          return DFB_OK;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
#ifdef USE_TREMOR
     if (desc.samplerate != data->info->rate)
          return DFB_UNSUPPORTED;
#else
     if (desc.samplerate != data->info->rate &&
         desc.samplerate != data->info->rate/2)
          return DFB_UNSUPPORTED;
#endif

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
     
#ifndef USE_TREMOR
     if (desc.samplerate == data->info->rate/2) {
          if (ov_halfrate( &data->vf, 1 )) {
               pthread_mutex_unlock( &data->lock );
               return DFB_UNSUPPORTED;
          }
     } else {
          ov_halfrate( &data->vf, 0 );
     }
#endif

     /* reference destination stream */
     destination->AddRef( destination );
     data->dest.stream = destination;
     data->dest.format = desc.sampleformat;
     data->dest.mode   = desc.channelmode;
     data->dest.length = desc.buffersize;

     if (data->finished) {
          if (direct_stream_remote( data->stream ))
               direct_stream_seek( data->stream, 0 );
          else
               ov_time_seek( &data->vf, 0 );

          data->finished = false;
     }

     /* start thread */
     data->playing = true;
     data->thread  = direct_thread_create( DTT_DEFAULT,
                                           VorbisStreamThread, data, "Vorbis" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static void*
VorbisBufferThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Vorbis_data *data =
          (IFusionSoundMusicProvider_Vorbis_data*) ctx;

     IFusionSoundBuffer *buffer    = data->dest.buffer;
     int                 section   = 0;
     int                 blocksize = FS_CHANNELS_FOR_MODE(data->dest.mode) *
                                     FS_BYTES_PER_SAMPLE(data->dest.format);

     while (data->playing && !data->finished) {
#ifdef USE_TREMOR
          s16     src[2048];
#else
          float **src;
#endif
          char   *dst;
          long    len;
          int     pos = 0;
          int     size;

          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }

          if (buffer->Lock( buffer, (void*)&dst, &size, 0 ) != DFB_OK) {
               D_ERROR( "IFusionSoundMusicProvider_Vorbis: "
                        "Couldn't lock buffer!\n" );
               pthread_mutex_unlock( &data->lock );
               break;
          }

          do {
#ifdef USE_TREMOR
               len = ov_read( &data->vf, (char*)&src[0], sizeof(src), &section );
               len = (len > 0) ? (len/(data->info->channels*2)) : len;
#else
               len = ov_read_float( &data->vf, &src, size-pos, &section );
#endif
               if (len == 0) {
                    if (data->flags & FMPLAY_LOOPING) {
                         if (direct_stream_remote( data->stream ))
                              direct_stream_seek( data->stream, 0 );
                         else
                              ov_time_seek( &data->vf, 0 );
                    }
                    else {
                         data->finished = true;
                    }
                    continue;
               }

               if (len > 0) {
                    int n;
                    do {
                         n = MIN( len, size-pos );
                         vorbis_mix_audio( src, &dst[pos*blocksize], 0, n,
                                           data->dest.format,
                                           data->info->channels,
                                           data->dest.mode );
                         pos += n;
                         len -= n;
                    } while (n > 0);
               }
          } while (pos < size && !data->finished);

          buffer->Unlock( buffer );

          pthread_mutex_unlock( &data->lock );

          if (data->callback) {
               if (data->callback( pos, data->ctx ))
                    data->playing = false;
          }
     }

     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                               IFusionSoundBuffer        *destination,
                                               FMBufferCallback           callback,
                                               void                      *ctx )
{
     FSBufferDescription desc;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!destination)
          return DFB_INVARG;
          
     if (data->dest.buffer == destination)
          return DFB_OK;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
#ifdef USE_TREMOR
     if (desc.samplerate != data->info->rate)
          return DFB_UNSUPPORTED;
#else
     if (desc.samplerate != data->info->rate &&
         desc.samplerate != data->info->rate/2)
          return DFB_UNSUPPORTED;
#endif

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
     
#ifndef USE_TREMOR
     if (desc.samplerate == data->info->rate/2) {
          if (ov_halfrate( &data->vf, 1 )) {
               pthread_mutex_unlock( &data->lock );
               return DFB_UNSUPPORTED;
          }
     } else {
          ov_halfrate( &data->vf, 0 );
     }
#endif

     /* reference destination stream */
     destination->AddRef( destination );
     data->dest.buffer = destination;
     data->dest.format = desc.sampleformat;
     data->dest.mode   = desc.channelmode;
     data->dest.length = desc.length;

     /* register new callback */
     data->callback = callback;
     data->ctx      = ctx;

     if (data->finished) {
          if (direct_stream_remote( data->stream ))
               direct_stream_seek( data->stream, 0 );
          else
               ov_time_seek( &data->vf, 0 );

          data->finished = false;
     }

     /* start thread */
     data->playing = true;
     data->thread  = direct_thread_create( DTT_DEFAULT,
                                           VorbisBufferThread, data, "Vorbis" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_Stop( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

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
IFusionSoundMusicProvider_Vorbis_GetStatus( IFusionSoundMusicProvider *thiz,
                                            FSMusicProviderStatus     *status )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

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
IFusionSoundMusicProvider_Vorbis_SeekTo( IFusionSoundMusicProvider *thiz,
                                         double                     seconds )
{
     DFBResult ret = DFB_OK;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (seconds < 0.0)
          return DFB_INVARG;

     pthread_mutex_lock( &data->lock );

     if (direct_stream_remote( data->stream )) {
          unsigned int off;

          if (!data->info->bitrate_nominal)
               return DFB_UNSUPPORTED;

          off = seconds * (double)(data->info->bitrate_nominal >> 3);
          ret = direct_stream_seek( data->stream, off );
     }
     else {
#ifdef USE_TREMOR
          seconds *= 1000;
#endif
          if (ov_time_seek( &data->vf, seconds ))
               ret = DFB_FAILURE;
     }
     
     if (ret == DFB_OK)
          data->seeked = true;

     pthread_mutex_unlock( &data->lock );

     return ret;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetPos( IFusionSoundMusicProvider *thiz,
                                         double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!seconds)
          return DFB_INVARG;

#ifdef USE_TREMOR
     *seconds = (double)ov_time_tell( &data->vf ) / 1000.0;
#else
     *seconds = ov_time_tell( &data->vf );
#endif

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetLength( IFusionSoundMusicProvider *thiz,
                                            double                    *seconds )
{
     double length;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!seconds)
          return DFB_INVARG;

#ifdef USE_TREMOR
     length = (double)ov_time_total( &data->vf, -1 ) / 1000.0;
#else
     length = ov_time_total( &data->vf, -1 );
#endif
     if (length < 0) {
          if (data->info->bitrate_nominal) {
               length = direct_stream_length( data->stream ) /
                        (double)(data->info->bitrate_nominal >> 3);
          }
     }

     *seconds = length;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_SetPlaybackFlags( IFusionSoundMusicProvider    *thiz,
                                                   FSMusicProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (flags & ~FMPLAY_LOOPING)
          return DFB_UNSUPPORTED;

     if (flags & FMPLAY_LOOPING && !direct_stream_seekable( data->stream ))
          return DFB_UNSUPPORTED;

     data->flags = flags;

     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     if (!memcmp( &ctx->header[0], "OggS", 4 ) &&
         !memcmp( &ctx->header[29], "vorbis", 6 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename,
           DirectStream              *stream )
{
     ov_callbacks cb;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_Vorbis )

     data->ref    = 1;
     data->stream = direct_stream_dup( stream );

     cb.read_func  = ov_read_callback;
     cb.seek_func  = ov_seek_callback;
     cb.tell_func  = ov_tell_callback;
     cb.close_func = ov_close_callback;

     if (ov_open_callbacks( data->stream, &data->vf, NULL, 0, cb ) < 0) {
          D_ERROR( "IFusionSoundMusicProvider_Vorbis: "
                   "Error opening ogg/vorbis stream!\n" );
          IFusionSoundMusicProvider_Vorbis_Destruct( thiz );
          return DFB_FAILURE;
     }

     data->info = ov_info( &data->vf, -1 );
     if (!data->info) {
          D_ERROR( "IFusionSoundMusicProvider_Vorbis: "
                   "Error getting stream informations!\n" );
          IFusionSoundMusicProvider_Vorbis_Destruct( thiz );
          return DFB_FAILURE;
     }

     direct_util_recursive_pthread_mutex_init( &data->lock );

     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_Vorbis_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Vorbis_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_Vorbis_GetCapabilities;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_Vorbis_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_Vorbis_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_Vorbis_GetBufferDescription;
     thiz->PlayToStream         = IFusionSoundMusicProvider_Vorbis_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_Vorbis_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_Vorbis_Stop;
     thiz->GetStatus            = IFusionSoundMusicProvider_Vorbis_GetStatus;
     thiz->SeekTo               = IFusionSoundMusicProvider_Vorbis_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_Vorbis_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_Vorbis_GetLength;
     thiz->SetPlaybackFlags     = IFusionSoundMusicProvider_Vorbis_SetPlaybackFlags;

     return DFB_OK;
}

