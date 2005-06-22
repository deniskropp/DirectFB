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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fusionsound.h>

#include <media/ifusionsoundmusicprovider.h>

#include <direct/types.h>
#include <direct/mem.h>
#include <direct/thread.h>
#include <direct/util.h>

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, Wave )

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int                      ref;        /* reference counter */
     
     int                      fd;
     __u32                    byteorder;  /* 0=little-endian 1=big-endian */
     __u32                    samplerate; /* frequency */
     __u16                    channels;   /* number of channels */
     __u16                    format;     /* bits per sample */
     __u32                    headsize;   /* size of headers */
     __u32                    datasize;   /* size of pcm data */
     double                   length;     /* in seconds */
     
     DirectThread            *thread;
     pthread_mutex_t          lock;
     bool                     playing;
     bool                     finished;

     void                    *src_buffer;
     void                    *dst_buffer;

     struct {
          IFusionSoundStream *stream;
          IFusionSoundBuffer *buffer;
          int                 format;
          int                 channels;
          int                 length;
     } dest;

     FMBufferCallback         callback;
     void                    *ctx;
} IFusionSoundMusicProvider_Wave_data;


#ifdef WORDS_BIGENDIAN
# define HOST_BYTEORDER  1
#else
# define HOST_BYTEORDER  0
#endif

#define SWAP16( n )  (n) = ((n) >> 8) | ((n) << 8)

#define SWAP32( n )  (n) = ((n) >> 24) | (((n) & 0x00ff0000) >> 8) | \
                           (((n) & 0x0000ff00) << 8) | (((n) << 24))


static inline void
mix8to8( void *src, void *dst, int len, int delta )
{
     __u8 *s = src;
     __u8 *d = dst;
     int   i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = s[i];
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = (s[0] + s[1]) >> 1;
               s += 2;
          }
     }
}

static inline void
mix16to8( void *src, void *dst, int len, int delta )
{
     __s16 *s = src;
     __u8  *d = dst;
     int    i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = (s[i] >> 8) + 128;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = ((s[0] + s[1]) >> 9) + 128;
               s += 2;
          }
     }
     /* Convert signed 16 to unsigned 8 */
     else {
          for (i = 0; i < len; i++)
               d[i] = (s[i] >> 8) + 128;
     }
}

static inline void
mix24to8( void *src, void *dst, int len, int delta )
{
     __s8 *s = src;
     __u8 *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = len; i--;) {
#ifdef WORDS_BIGENDIAN
               d[0] = d[1] = s[0] + 128;
#else
               d[0] = d[1] = s[2] + 128;
#endif
               s += 3;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
#ifdef WORDS_BIGENDIAN
               d[i] = (s[0] + s[3] + 256) >> 1;
#else
               d[i] = (s[2] + s[5] + 256) >> 1;
#endif
               s += 6;
          }
     }
     /* Convert signed 24 to unsigned 8 */
     else {
          for (i = 0; i < len; i++) {
#ifdef WORDS_BIGENDIAN
               d[i] = s[0] + 128;
#else
               d[i] = s[2] + 128;
#endif
               s += 3;
          }
     }
}

static inline void 
mix32to8( void *src, void *dst, int len, int delta )
{
     __s32 *s = src;
     __u8  *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = (s[i] >> 24) + 128;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = ((s[0] + s[1]) >> 25) + 128;
               s += 2;
          }
     }
     /* Convert signed 32 to unsigned 8 */
     else {
          for (i = 0; i < len; i++)
               d[i] = (s[i] >> 24) + 128;
     }
}

static inline void
mix8to16( void *src, void *dst, int len, int delta )
{
     __u8  *s = src;
     __s16 *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = (s[i] - 128) << 8;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = (s[0] + s[1] - 256) << 7;
               s += 2;
          }
     }
     /* Convert unsigned 8 to signed 16 */
     else {
          for (i = 0; i < len; i++)
               d[i] = (s[i] - 128) << 8;
     }
}

static inline void
mix16to16( void *src, void *dst, int len, int delta )
{
     __s16 *s = src;
     __s16 *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = s[i];
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = (s[0] + s[1]) >> 1;
               s += 2;
          }
     }
}

static inline void
mix24to16( void *src, void *dst, int len, int delta )
{
     __u8  *s = src;
     __s16 *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = len; i--;) {
#ifdef WORDS_BIGENDIAN
               d[0] = d[1] = s[1] | (s[0] << 8);
#else
               d[0] = d[1] = s[1] | (s[2] << 8);
#endif
               s += 3;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               __s16 s0, s1;
#ifdef WORDS_BIGENDIAN
               s0 = s[1] | (s[0] << 8);
               s1 = s[4] | (s[3] << 8);
#else
               s0 = s[1] | (s[2] << 8);
               s1 = s[4] | (s[5] << 8);
#endif
               d[i] = (s0 + s1) >> 1;
               s += 6;
          }
     }
     /* Convert signed 24 to signed 16 */
     else {
          for (i = 0; i < len; i++) {
#ifdef WORDS_BIGENDIAN
               d[i] = s[1] | (s[0] << 8);
#else
               d[i] = s[1] | (s[2] << 8);
#endif
               s += 3;
          }
     }
}

static inline void
mix32to16( void *src, void *dst, int len, int delta )
{
     __s32 *s = src;
     __s16 *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = s[i] >> 16;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = (s[0] + s[1]) >> 17;
               s += 2;
          }
     }
     /* Convert signed 32 to signed 16 */
     else {
          for (i = 0; i < len; i++)
               d[i] = s[i] >> 16;
     }
}

static inline void
mix8to24( void *src, void *dst, int len, int delta )
{
     __u8 *s = src;
     __u8 *d = dst;
     int   i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               int c = (s[i] - 128) << 16;
               d[0] = d[3] = c;
               d[1] = d[4] = c >> 8;
               d[2] = d[5] = c >> 16;
               d += 6;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len; i += 2) {
               int c = (s[i+0] + s[i+1] - 256) << 15;
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
               d += 3;
          }
     }
     /* Convert unsigned 8 to signed 24 */
     else {
          for (i = 0; i < len; i++) {
               int c = (s[i] - 128) << 16;
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
               d += 3;
          }
     }
}

static inline void
mix16to24( void *src, void *dst, int len, int delta )
{
     __s16 *s = src;
     __u8  *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               int c = s[i] << 8;
               d[0] = d[3] = c;
               d[1] = d[4] = c >> 8;
               d[2] = d[5] = c >> 16;
               d += 6;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len; i += 2) {
               int c = (s[i+0] + s[i+1]) << 7;
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
               d += 3;
          }
     }
     /* Convert signed 16 to signed 24 */
     else {
          for (i = 0; i < len; i++) {
               int c = s[i] << 8;
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
               d += 3;
          }
     } 
}

static inline void
mix24to24( void *src, void *dst, int len, int delta )
{
     __u8 *s = src;
     __u8 *d = dst;
     int   i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = len; i--;) {
               d[0] = d[3] = s[0];
               d[1] = d[4] = s[1];
               d[2] = d[5] = s[3];
               s += 3;
               d += 3;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = len/2; i--;) {
               int c;
#ifdef WORDS_BIGENDIAN
               c = (((s[2] << 8) | (s[1] << 16) | (s[0] << 24)) >> 8) +
                   (((s[5] << 8) | (s[4] << 16) | (s[3] << 24)) >> 8);
#else
               c = (((s[0] << 8) | (s[1] << 16) | (s[2] << 24)) >> 8) +
                   (((s[3] << 8) | (s[4] << 16) | (s[5] << 24)) >> 8);
#endif
               c >>= 1;
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
               s += 6;
               d += 3;
          }
     }
}

static inline void
mix32to24( void *src, void *dst, int len, int delta )
{
     __s32 *s = src;
     __u8  *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               int c = s[i] >> 8;
               d[0] = d[3] = c;
               d[1] = d[4] = c >> 8;
               d[2] = d[5] = c >> 16;
               d += 6;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len; i += 2) {
               int c = ((s[i+0]>>8) + (s[i+1]>>8)) >> 1;
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;               
               d += 3;
          }
     }
     /* Convert signed 32 to signed 24 */
     else {
          for (i = 0; i < len; i++) {
               int c = s[i] >> 8;
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
               d += 3;
          }
     }
}

static inline void
mix8to32( void *src, void *dst, int len, int delta )
{
     __u8  *s = src;
     __s32 *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = (s[i] - 128) << 24;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = (s[0] + s[1] - 256) << 23;
               s += 2;
          }
     }
     /* Convert unsigned 8 to signed 32 */
     else {
          for (i = 0; i < len; i++)
               d[i] = (s[i] - 128) << 24;
     }
}

static inline void
mix16to32( void *src, void *dst, int len, int delta )
{
     __s16 *s = src;
     __s32 *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = s[i] << 16;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = (s[0] + s[1]) << 15;
               s += 2;
          }
     }
     /* Convert signed 16 to signed 32 */
     else {
          for (i = 0; i < len; i++)
               d[i] = s[i] << 16;
     }
}

static inline void
mix24to32( void *src, void *dst, int len, int delta )
{
     __u8  *s = src;
     __s32 *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = len; i--;) {
#ifdef WORDS_BIGENDIAN
               d[0] = d[1] = (s[2] << 8) | (s[1] << 16) | (s[0] << 24);
#else
               d[0] = d[1] = (s[0] << 8) | (s[1] << 16) | (s[2] << 24);
#endif
               s += 3;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               int c;
#ifdef WORDS_BIGENDIAN
               c = (((s[2] << 8) | (s[1] << 16) | (s[0] << 24)) >> 8) +
                   (((s[5] << 8) | (s[4] << 16) | (s[3] << 24)) >> 8);
#else
               c = (((s[0] << 8) | (s[1] << 16) | (s[2] << 24)) >> 8) +
                   (((s[3] << 8) | (s[4] << 16) | (s[5] << 24)) >> 8);
#endif
               d[i] = c << 7;
               s += 6;
          }
     }
     /* Convert signed 24 to signed 32 */
     else {
          for (i = 0; i < len; i++) {
#ifdef WORDS_BIGENDIAN
               d[i] = (s[2] << 8) | (s[1] << 16) | (s[0] << 24);
#else
               d[i] = (s[0] << 8) | (s[1] << 16) | (s[2] << 24);
#endif
               s += 3;
          }
     }
}

static inline void
mix32to32( void *src, void *dst, int len, int delta )
{
     __s32 *s = src;
     __s32 *d = dst;
     int    i;
 
     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = s[i];
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = s[0]/2 + s[1]/2;
               s += 2;
          }
     }
}


static void
wave_mix_audio( __u8 *src, __u8 *dst, int len, 
                int src_format, int dst_format, int byteorder, int delta )
{
     if (byteorder != HOST_BYTEORDER) {
         int i;
               
         switch (src_format) {
               case 16:
                    for (i = 0; i < len; i++)
                         SWAP16( ((__u16*)src)[i] );
                    break;
               case 24:
                    for (i = 0; i < len*3; i += 3) {
                         __u8 tmp = src[i+0];
                         src[i+0] = src[i+2];
                         src[i+2] = tmp;
                    }
                    break;
               case 32:
                    for (i = 0; i < len; i++)
                         SWAP32( ((__u32*)src)[i] );
                    break;
               default:
                    break;
          }
     }

     switch (dst_format) {
          case 8:
               switch (src_format) {
                    case 8:
                         mix8to8( src, dst, len, delta );
                         break;
                    case 16:
                         mix16to8( src, dst, len, delta );
                         break;
                    case 24:
                         mix24to8( src, dst, len, delta );
                         break;
                    case 32:
                         mix32to8( src, dst, len, delta );
                         break;
                    default:
                         break;
               }
               break;

          case 16:
               switch (src_format) {
                    case 8:
                         mix8to16( src, dst, len, delta );
                         break;
                    case 16:
                         mix16to16( src, dst, len, delta );
                         break;
                    case 24:
                         mix24to16( src, dst, len, delta );
                         break;
                    case 32:
                         mix32to16( src, dst, len, delta );
                         break;
                    default:
                         break;
               }
               break;

          case 24:
               switch (src_format) {
                    case 8:
                         mix8to24( src, dst, len, delta );
                         break;
                    case 16:
                         mix16to24( src, dst, len, delta );
                         break;
                    case 24:
                         mix24to24( src, dst, len, delta );
                         break;
                    case 32:
                         mix32to24( src, dst, len, delta );
                         break;
                    default:
                         break;
               }
               break;
               
          case 32:
               switch (src_format) {
                    case 8:
                         mix8to32( src, dst, len, delta );
                         break;
                    case 16:
                         mix16to32( src, dst, len, delta );
                         break;
                    case 24:
                         mix24to32( src, dst, len, delta );
                         break;
                    case 32:
                         mix32to32( src, dst, len, delta );
                         break;
                    default:
                         break;
               }
               break;
               
          default:
               D_BUG( "unexpected sampleformat" );
               break;
     }
}


static void
IFusionSoundMusicProvider_Wave_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_Wave_data *data =
         (IFusionSoundMusicProvider_Wave_data*)thiz->priv;

     thiz->Stop( thiz );
    
     close( data->fd );

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

     *caps = FMCAPS_BASIC | FMCAPS_SEEK;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_EnumTracks( IFusionSoundMusicProvider *thiz,
                                           FSTrackCallback            callback,
                                           void                      *callbackdata )
{
     FSTrackDescription desc;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!callback)
          return DFB_INVARG;
     
     memset( &desc, 0, sizeof(FSTrackDescription) );
     snprintf( desc.encoding,
               FS_TRACK_DESC_ENCODING_LENGTH,
               "PCM %dbit %s-endian", 
               data->format, data->byteorder ? "big" : "little" );
     desc.bitrate = data->samplerate * data->channels * data->format;

     callback( 0, desc, callbackdata );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_GetTrackID( IFusionSoundMusicProvider *thiz,
                                           FSTrackID                 *ret_track_id )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!ret_track_id)
          return DFB_INVARG;

     *ret_track_id = 0;

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
               "PCM %dbit %s-endian", 
               data->format, data->byteorder ? "big" : "little" );
     desc->bitrate = data->samplerate * data->channels * data->format;

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
     switch (data->format) {
          case 8:
               desc->sampleformat = FSSF_U8;
               break;
          case 16:
               desc->sampleformat = FSSF_S16;
               break;
          case 24:
               desc->sampleformat = FSSF_S24;
               break;
          case 32:
               desc->sampleformat = FSSF_S32;
               break;
     }
     desc->buffersize   = data->samplerate / 
                         (data->channels * data->format >> 3);

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
     switch (data->format) {
          case 8:
               desc->sampleformat = FSSF_U8;
               break;
          case 16:
               desc->sampleformat = FSSF_S16;
               break;
          case 24:
               desc->sampleformat = FSSF_S24;
               break;
          case 32:
               desc->sampleformat = FSSF_S32;
               break;
     }
     desc->length       = data->samplerate / 
                         (data->channels * data->format >> 3);

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Wave_SelectTrack( IFusionSoundMusicProvider *thiz,
                                            FSTrackID                  track_id )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (track_id != 0)
          return DFB_INVARG;

     return DFB_OK;
}

static void*
WaveStreamThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Wave_data *data = 
          (IFusionSoundMusicProvider_Wave_data*) ctx;
     
     int     delta = data->channels - data->dest.channels;
     size_t  count = data->dest.length * data->channels * data->format >> 3;
     __u8   *src   = data->src_buffer;
     __u8   *dst   = data->dst_buffer;

     data->finished = false;
     
     while (data->playing) {
          ssize_t len;
          
          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          len  = read( data->fd, src, count );
          len /= data->format >> 3;

          pthread_mutex_unlock( &data->lock );
          
          if (len <= 0) {
               data->finished = true;
               break;
          }

          wave_mix_audio( src, dst, len, 
                          data->format, data->dest.format,
                          data->byteorder, delta );
                          
          data->dest.stream->Write( data->dest.stream, 
                                    dst, len / data->channels );
     }
     
     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Wave_PlayToStream( IFusionSoundMusicProvider *thiz,
                                             IFusionSoundStream        *destination )
{
     FSStreamDescription  desc;
     __u32                dst_format = 0;
     int                  src_size   = 0;
     int                  dst_size   = 0;
     void                *buffer;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
     if (desc.samplerate != data->samplerate)
          return DFB_UNSUPPORTED;
     
     /* check if number of channels is supported */
     if (desc.channels > 2)
          return DFB_UNSUPPORTED;

     /* check if destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_U8:
          case FSSF_S16:
          case FSSF_S24:
          case FSSF_S32:
               dst_format = FS_BITS_PER_SAMPLE(desc.sampleformat);
               break;
          default:
               return DFB_UNSUPPORTED;
     }

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
          data->dst_buffer = NULL;
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
     
     /* allocate buffer(s) */
     src_size = desc.buffersize * data->channels * data->format >> 3;
     if (dst_format != data->format || desc.channels != data->channels)
          dst_size = desc.buffersize * desc.channels * dst_format >> 3;
     
     buffer = D_MALLOC( src_size + dst_size );
     if (!buffer) {
          pthread_mutex_unlock( &data->lock );
          return D_OOM();
     }
     
     data->src_buffer = buffer;
     data->dst_buffer = (dst_size) ? (buffer + src_size) : buffer;
 
     /* reference destination stream */
     destination->AddRef( destination );
     data->dest.stream   = destination;
     data->dest.format   = dst_format;
     data->dest.channels = desc.channels;
     data->dest.length   = desc.buffersize;
     
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
     int                 delta  = data->channels - data->dest.channels;
     size_t              count  = data->dest.length * data->channels *
                                  data->format >> 3;
     
     data->finished = false;
     
     while (data->playing) {
          void    *dst;
          ssize_t  len;
          
          pthread_mutex_lock( &data->lock );
          
          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          if (!data->src_buffer) {
               if (buffer->Lock( buffer, &dst ) != DFB_OK) {
                    D_ERROR( "IFusionSoundMusicProvider_Wave: "
                             "Couldn't lock buffer!" );
                    pthread_mutex_unlock( &data->lock );
                    break;
               }
               
               len = read( data->fd, dst, count );
               
               buffer->Unlock( buffer );
          } else
               len = read( data->fd, data->src_buffer, count );

          pthread_mutex_unlock( &data->lock );
          
          len /= data->format >> 3;
          if (len <= 0) {
               data->finished = true;
               break;
          }

          if (data->src_buffer) {
               if (buffer->Lock( buffer, &dst ) != DFB_OK) {
                    D_ERROR( "IFusionSoundMusicProvider_Wave: "
                             "Couldn't lock buffer!" );
                    break;
               }

               wave_mix_audio( data->src_buffer, dst, len,
                               data->format, data->dest.format,
                               data->byteorder, delta );
          
               buffer->Unlock( buffer );
          }

          if (data->callback)
               data->callback( len / data->channels, data->ctx );
     }
     
     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Wave_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                             IFusionSoundBuffer        *destination,
                                             FMBufferCallback           callback,
                                             void                      *ctx )
{
     FSBufferDescription  desc;
     __u32                dst_format = 0;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
     if (desc.samplerate != data->samplerate)
          return DFB_UNSUPPORTED;
     
     /* check if number of channels is supported */
     if (desc.channels > 2)
          return DFB_UNSUPPORTED;

     /* check if destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_U8:
          case FSSF_S16:
          case FSSF_S24:
          case FSSF_S32:
               dst_format = FS_BITS_PER_SAMPLE(desc.sampleformat);
               break;
          default:
               return DFB_UNSUPPORTED;
     }

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
          data->dst_buffer = NULL;
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
     
     /* allocate buffer */
     if (dst_format != data->format || desc.channels != data->channels) {
          data->src_buffer = D_MALLOC( desc.length *
                                       data->channels * data->format >> 3 );
          if (!data->src_buffer) {
               pthread_mutex_unlock( &data->lock );
               return D_OOM();
          }
     }
 
     /* reference destination stream */
     destination->AddRef( destination );
     data->dest.buffer   = destination;
     data->dest.format   = dst_format;
     data->dest.channels = desc.channels;
     data->dest.length   = desc.length;

     data->callback      = callback;
     data->ctx           = ctx;
     
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
          data->dst_buffer = NULL;
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
IFusionSoundMusicProvider_Wave_SeekTo( IFusionSoundMusicProvider *thiz,
                                       double                     seconds )
{
     off_t offset;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (seconds < 0.0)
          return DFB_INVARG;

     offset = (double)data->samplerate * seconds;
     offset = offset * data->channels * data->format >> 3;
     offset = MIN( offset, data->datasize ) + data->headsize;

     pthread_mutex_lock( &data->lock );
     lseek( data->fd, offset, SEEK_SET );
     pthread_mutex_unlock( &data->lock );
     
     return DFB_OK;
}

static DFBResult 
IFusionSoundMusicProvider_Wave_GetPos( IFusionSoundMusicProvider *thiz,
                                       double                    *seconds )
{
     off_t offset;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Wave )

     if (!seconds)
          return DFB_INVARG;

     if (data->finished) {
          *seconds = data->length;
          return DFB_EOF;
     }

     offset  = lseek( data->fd, 0, SEEK_CUR );
     offset -= data->headsize;
     
     if (offset > 0) {
          *seconds = (double) offset /
                     (double)(data->samplerate * data->channels * data->format >> 3);
     } else
          *seconds = 0.0;
          
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


static bool
parse_headers( int fd, __u32 *ret_byteorder, __u32 *ret_samplerate,
                       __u16 *ret_channels,  __u16 *ret_format,
                       __u32 *ret_headsize,  __u32 *ret_datasize )
{
     char  buf[4];
     __u32 byteorder = 0;
     __u32 fmt_len;
     struct {
          __u16 encoding;
          __u16 channels;
          __u32 frequency;
          __u32 byterate;
          __u16 blockalign;
          __u16 bitspersample;
     } fmt;
     __u32 data_size;

#define wave_read( buf, count ) {\
     if (read( fd, buf, count) < (count))\
          return false;\
}
     
     wave_read( buf, 4 );
     if (buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F') {
          switch (buf[3]) {
               case 'F':
                    byteorder = 0; // little-endian
                    break;
               case 'X':
                    byteorder = 1; // big-endian
                    break;
               default:
                    D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                             "No RIFF header found.\n" );
                    break;
          }
     } else
          D_DEBUG( "IFusionSoundMusicProvider_Wave: No RIFF header found.\n" );

     /* actually ignore chunksize */
     if (lseek( fd, 4, SEEK_CUR ) < 8)
          return false;

     wave_read( buf, 4 );
     if (buf[0] != 'W' || buf[1] != 'A' || buf[2] != 'V' || buf[3] != 'E') {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: No WAVE header found.\n" );
          return false;
     }

     wave_read( buf, 4 );
     if (buf[0] != 'f' || buf[1] != 'm' || buf[2] != 't' || buf[3] != ' ') {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: Expected 'fmt ' header.\n" );
          return false;
     }

     wave_read( &fmt_len, 4 );
     if (byteorder != HOST_BYTEORDER)
          SWAP32( fmt_len );

     if (fmt_len < sizeof(fmt)) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "fmt chunk expected to be at least %d bytes (got %d).\n",
                   sizeof(fmt), fmt_len );
          return false;
     }

     wave_read( &fmt, sizeof(fmt) );
     if (byteorder != HOST_BYTEORDER) {
          SWAP16( fmt.encoding );
          SWAP16( fmt.channels );
          SWAP32( fmt.frequency );
          SWAP32( fmt.byterate );
          SWAP16( fmt.blockalign );
          SWAP16( fmt.bitspersample );
     }

     if (fmt.encoding != 1) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Unsupported encoding (%d).\n", fmt.encoding );
          return false;
     }
     if (fmt.channels < 1 || fmt.channels > 2) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Unsupported number of channels (%d).\n", fmt.channels );
          return false;
     }
     if (fmt.frequency < 4000 || fmt.frequency > 48000) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Unsupported frequency (%dHz).\n", fmt.frequency );
          return false;
     }
     if (fmt.bitspersample !=  8 && fmt.bitspersample != 16 && 
         fmt.bitspersample != 24 && fmt.bitspersample != 32) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Unsupported sample format (%d bits).\n", fmt.bitspersample );
          return false;
     }
     if (fmt.byterate != (fmt.frequency * fmt.channels * fmt.bitspersample >> 3)) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Invalid byterate (%d).\n", fmt.byterate );
          return false;
     }
     if (fmt.blockalign != (fmt.channels * fmt.bitspersample >> 3)) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Invalid blockalign (%d).\n", fmt.blockalign );
          return false;
     }

     if (fmt_len > sizeof(fmt)) {
          if (lseek( fd, fmt_len - sizeof(fmt), SEEK_CUR ) < (fmt_len + 20))
               return false;
     }

     wave_read( buf, 4 );
     if (buf[0] != 'd' || buf[1] != 'a' || buf[2] != 't' || buf[3] != 'a') {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: Expected 'data' header.\n" );
          return false;
     }

     wave_read( &data_size, 4 );
     if (byteorder != HOST_BYTEORDER)
          SWAP32( data_size );

     if (ret_byteorder)
          *ret_byteorder = byteorder;
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
          
     return true;
}   

/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     int  fd;
     bool ret;
     
     fd = open( ctx->filename, O_RDONLY );
     if (fd < 0)
          return DFB_UNSUPPORTED;

     ret = parse_headers( fd, NULL, NULL, NULL, NULL, NULL, NULL );
     close( fd );

     return (ret) ? DFB_OK : DFB_UNSUPPORTED;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz, const char *filename )
{
     struct stat buf;
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_Wave )

     data->ref = 1;
     
     data->fd = open( filename, O_RDONLY );
     if (data->fd < 0)
          return DFB_IO;
    
     if (!parse_headers( data->fd, &data->byteorder, &data->samplerate,
                                   &data->channels,  &data->format,
                                   &data->headsize,  &data->datasize )) {
          close( data->fd );
          return DFB_FAILURE;
     }

     if (fstat( data->fd, &buf) < 0 || buf.st_size < data->headsize) {
          close( data->fd );
          return DFB_FAILURE;
     }

     if (data->datasize)
          data->datasize = MIN( data->datasize, buf.st_size - data->headsize );
     else
          data->datasize = buf.st_size - data->headsize;

     data->length = (double) data->datasize /
                    (double)(data->samplerate * data->channels * data->format >> 3);

     D_DEBUG( "IFusionSoundMusicProvider_Wave: "
              "%s [%dHz - %d channel(s) - %dbits %c.E.].\n",
              filename, data->samplerate, data->channels,
              data->format, data->byteorder ? 'B' : 'L' );
     
     direct_util_recursive_pthread_mutex_init( &data->lock );

     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_Wave_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Wave_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_Wave_GetCapabilities;
     thiz->EnumTracks           = IFusionSoundMusicProvider_Wave_EnumTracks;
     thiz->GetTrackID           = IFusionSoundMusicProvider_Wave_GetTrackID;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_Wave_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_Wave_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_Wave_GetBufferDescription;
     thiz->SelectTrack          = IFusionSoundMusicProvider_Wave_SelectTrack;
     thiz->PlayToStream         = IFusionSoundMusicProvider_Wave_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_Wave_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_Wave_Stop;
     thiz->SeekTo               = IFusionSoundMusicProvider_Wave_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_Wave_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_Wave_GetLength;

     return DFB_OK;
}

