/*
 * Copyright (C) 2005-2006 Claudio Ciccani <klan@users.sf.net>
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

#include <media/ifusionsoundmusicprovider.h>

#include <direct/types.h>
#include <direct/mem.h>
#include <direct/stream.h>
#include <direct/thread.h>
#include <direct/util.h>

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
     u32                           byteorder;  /* 0=little-endian 1=big-endian */
     u32                           samplerate; /* frequency */
     u16                           channels;   /* number of channels */
     u16                           format;     /* bits per sample */
     u32                           headsize;   /* size of headers */
     u32                           datasize;   /* size of pcm data */
     double                        length;     /* in seconds */

     FSMusicProviderPlaybackFlags  flags;

     DirectThread                 *thread;
     pthread_mutex_t               lock;
     bool                          playing;
     bool                          finished;

     void                         *src_buffer;
     void                         *dst_buffer;

     struct {
          IFusionSoundStream      *stream;
          IFusionSoundBuffer      *buffer;
          FSSampleFormat           format;
          int                      channels;
          int                      length;
     } dest;

     FMBufferCallback              callback;
     void                         *ctx;
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
     u8  *s = src;
     u8  *d = dst;
     int  i;

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
     s16 *s = src;
     u8  *d = dst;
     int  i;

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
     s8  *s = src;
     u8  *d = dst;
     int  i;

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
     s32 *s = src;
     u8  *d = dst;
     int  i;

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
     u8  *s = src;
     s16 *d = dst;
     int  i;

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
     s16 *s = src;
     s16 *d = dst;
     int  i;

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
     u8  *s = src;
     s16 *d = dst;
     int  i;

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
               s16 s0, s1;
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
     s32 *s = src;
     s16 *d = dst;
     int  i;

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
     u8  *s = src;
     u8  *d = dst;
     int  i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               int c = (s[i] - 128) << 16;
#ifdef WORDS_BIGENDIAN
               d[0] = d[3] = c >> 16;
               d[1] = d[4] = c >> 8;
               d[2] = d[5] = c;
#else
               d[0] = d[3] = c;
               d[1] = d[4] = c >> 8;
               d[2] = d[5] = c >> 16;
#endif
               d += 6;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len; i += 2) {
               int c = (s[i+0] + s[i+1] - 256) << 15;
#ifdef WORDS_BIGENDIAN
               d[0] = c >> 16;
               d[1] = c >> 8;
               d[2] = c;
#else
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
#endif
               d += 3;
          }
     }
     /* Convert unsigned 8 to signed 24 */
     else {
          for (i = 0; i < len; i++) {
               int c = (s[i] - 128) << 16;
#ifdef WORDS_BIGENDIAN
               d[0] = c >> 16;
               d[1] = c >> 8;
               d[2] = c;
#else
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
#endif
               d += 3;
          }
     }
}

static inline void
mix16to24( void *src, void *dst, int len, int delta )
{
     s16 *s = src;
     u8  *d = dst;
     int  i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               int c = s[i] << 8;
#ifdef WORDS_BIGENDIAN
               d[0] = d[3] = c >> 16;
               d[1] = d[4] = c >> 8;
               d[2] = d[5] = c;
#else
               d[0] = d[3] = c;
               d[1] = d[4] = c >> 8;
               d[2] = d[5] = c >> 16;
#endif
               d += 6;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len; i += 2) {
               int c = (s[i+0] + s[i+1]) << 7;
#ifdef WORDS_BIGENDIAN
               d[0] = c >> 16;
               d[1] = c >> 8;
               d[2] = c;
#else
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
#endif
               d += 3;
          }
     }
     /* Convert signed 16 to signed 24 */
     else {
          for (i = 0; i < len; i++) {
               int c = s[i] << 8;
#ifdef WORDS_BIGENDIAN
               d[0] = c >> 16;
               d[1] = c >> 8;
               d[2] = c;
#else
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
#endif
               d += 3;
          }
     }
}

static inline void
mix24to24( void *src, void *dst, int len, int delta )
{
     u8  *s = src;
     u8  *d = dst;
     int  i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = len; i--;) {
               d[0] = d[3] = s[0];
               d[1] = d[4] = s[1];
               d[2] = d[5] = s[2];
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
#ifdef WORDS_BIGENDIAN
               d[0] = c >> 16;
               d[1] = c >> 8;
               d[2] = c;
#else
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
#endif
               s += 6;
               d += 3;
          }
     }
}

static inline void
mix32to24( void *src, void *dst, int len, int delta )
{
     s32 *s = src;
     u8  *d = dst;
     int  i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               int c = s[i] >> 8;
#ifdef WORDS_BIGENDIAN
               d[0] = d[3] = c >> 16;
               d[1] = d[4] = c >> 8;
               d[2] = d[5] = c;
#else
               d[0] = d[3] = c;
               d[1] = d[4] = c >> 8;
               d[2] = d[5] = c >> 16;
#endif
               d += 6;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len; i += 2) {
               int c = ((s[i+0]>>8) + (s[i+1]>>8)) >> 1;
#ifdef WORDS_BIGENDIAN
               d[0] = c >> 16;
               d[1] = c >> 8;
               d[2] = c;
#else
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
#endif
               d += 3;
          }
     }
     /* Convert signed 32 to signed 24 */
     else {
          for (i = 0; i < len; i++) {
               int c = s[i] >> 8;
#ifdef WORDS_BIGENDIAN
               d[0] = c >> 16;
               d[1] = c >> 8;
               d[2] = c;
#else
               d[0] = c;
               d[1] = c >> 8;
               d[2] = c >> 16;
#endif
               d += 3;
          }
     }
}

static inline void
mix8to32( void *src, void *dst, int len, int delta )
{
     u8  *s = src;
     s32 *d = dst;
     int  i;

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
     s16 *s = src;
     s32 *d = dst;
     int  i;

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
     u8  *s = src;
     s32 *d = dst;
     int  i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = len; i--;) {
#ifdef WORDS_BIGENDIAN
               d[0] = d[1] = (s[2]<<8) | (s[1]<<16) | (s[0]<<24);
#else
               d[0] = d[1] = (s[0]<<8) | (s[1]<<16) | (s[2]<<24);
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
               c = ((int)((s[2]<<8) | (s[1]<<16) | (s[0]<<24)) >> 8) +
                   ((int)((s[5]<<8) | (s[4]<<16) | (s[3]<<24)) >> 8);
#else
               c = ((int)((s[0]<<8) | (s[1]<<16) | (s[2]<<24)) >> 8) +
                   ((int)((s[3]<<8) | (s[4]<<16) | (s[5]<<24)) >> 8);
#endif
               d[i] = c << 7;
               s += 6;
          }
     }
     /* Convert signed 24 to signed 32 */
     else {
          for (i = 0; i < len; i++) {
#ifdef WORDS_BIGENDIAN
               d[i] = (s[2]<<8) | (s[1]<<16) | (s[0]<<24);
#else
               d[i] = (s[0]<<8) | (s[1]<<16) | (s[2]<<24);
#endif
               s += 3;
          }
     }
}

static inline void
mix32to32( void *src, void *dst, int len, int delta )
{
     s32 *s = src;
     s32 *d = dst;
     int  i;

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

static inline void
mix8toF32( void *src, void *dst, int len, int delta )
{
     u8    *s = src;
     float *d = dst;
     int    i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = (float)(s[i]-128)/128.0f;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = (float)((s[0]+s[1]-256)>>1)/128.0f;
               s += 2;
          }
     }
     /* Convert unsigned 8 to float 32 */
     else {
          for (i = 0; i < len; i++)
               d[i] = (float)(s[i]-128)/128.0f;
     }
}

static inline void
mix16toF32( void *src, void *dst, int len, int delta )
{
     s16   *s = src;
     float *d = dst;
     int    i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = (float)s[i]/32768.0f;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = (float)((s[0]+s[1])>>1)/32768.0f;
               s += 2;
          }
     }
     /* Convert signed 16 to float 32 */
     else {
          for (i = 0; i < len; i++)
               d[i] = (float)s[i]/32768.0f;
     }
}

static inline void
mix24toF32( void *src, void *dst, int len, int delta )
{
     u8    *s = src;
     float *d = dst;
     int    i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = len; i--;) {
               int c;
#ifdef WORDS_BIGENDIAN
               c = (int)((s[2]<<8) | (s[1]<<16) | (s[0]<<24)) >> 8;
#else
               c = (int)((s[0]<<8) | (s[1]<<16) | (s[2]<<24)) >> 8;
#endif
               d[0] = d[1] = (float)c/8388608.0f;
               s += 3;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               int c;
#ifdef WORDS_BIGENDIAN
               c = (((int)((s[2]<<8) | (s[1]<<16) | (s[0]<<24)) >> 8) +
                    ((int)((s[5]<<8) | (s[4]<<16) | (s[3]<<24)) >> 8)) >> 1;
#else
               c = (((int)((s[0]<<8) | (s[1]<<16) | (s[2]<<24)) >> 8) +
                    ((int)((s[3]<<8) | (s[4]<<16) | (s[5]<<24)) >> 8)) >> 1;
#endif
               d[i] = (float)c/8388608.0f;
               s += 6;
          }
     }
     /* Convert signed 24 to signed 32 */
     else {
          for (i = 0; i < len; i++) {
               int c;
#ifdef WORDS_BIGENDIAN
               c = (int)((s[2]<<8) | (s[1]<<16) | (s[0]<<24)) >> 8;
#else
               c = (int)((s[0]<<8) | (s[1]<<16) | (s[2]<<24)) >> 8;
#endif
               d[i] = (float)c/8388608.0f;
               s += 3;
          }
     }
}

static inline void
mix32toF32( void *src, void *dst, int len, int delta )
{
     s32   *s = src;
     float *d = dst;
     int    i;

     /* Upmix mono to stereo */
     if (delta < 0) {
          for (i = 0; i < len; i++) {
               d[0] = d[1] = (float)s[i]/2147483648.0f;
               d += 2;
          }
     }
     /* Downmix stereo to mono */
     else if (delta > 0) {
          for (i = 0; i < len/2; i++) {
               d[i] = ((float)s[0]/2147483648.0f + (float)s[1]/2147483648.0f)/2.0f;
               s += 2;
          }
     }
}

static void
wave_mix_audio( u8 *src, u8 *dst, int len,
                int src_format, int dst_format, int byteorder, int delta )
{
     if (byteorder != HOST_BYTEORDER) {
         int i;

         switch (src_format) {
               case 16:
                    for (i = 0; i < len; i++)
                         SWAP16( ((u16*)src)[i] );
                    break;
               case 24:
                    for (i = 0; i < len*3; i += 3) {
                         u8 tmp = src[i+0];
                         src[i+0] = src[i+2];
                         src[i+2] = tmp;
                    }
                    break;
               case 32:
                    for (i = 0; i < len; i++)
                         SWAP32( ((u32*)src)[i] );
                    break;
               default:
                    break;
          }
     }

     switch (dst_format) {
          case FSSF_U8:
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

          case FSSF_S16:
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

          case FSSF_S24:
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

          case FSSF_S32:
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

          case FSSF_FLOAT:
               switch (src_format) {
                    case 8:
                         mix8toF32( src, dst, len, delta );
                         break;
                    case 16:
                         mix16toF32( src, dst, len, delta );
                         break;
                    case 24:
                         mix24toF32( src, dst, len, delta );
                         break;
                    case 32:
                         mix32toF32( src, dst, len, delta );
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
     desc->length       = data->samplerate/10;

     return DFB_OK;
}

static void*
WaveStreamThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Wave_data *data =
          (IFusionSoundMusicProvider_Wave_data*) ctx;

     int     delta = data->channels - data->dest.channels;
     size_t  count = data->dest.length * data->channels * data->format >> 3;
     u8     *src   = data->src_buffer;
     u8     *dst   = data->dst_buffer;

     direct_stream_wait( data->stream, count, NULL );

     while (data->playing && !data->finished) {
          DFBResult      ret;
          int            len = 0;
          struct timeval tv  = { 0, 500 };

          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }

          ret = direct_stream_wait( data->stream, count, &tv );
          if (ret != DFB_TIMEOUT)
               ret = direct_stream_read( data->stream, count, src, &len );

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

          len /= data->format >> 3;
          if (len < 1)
               continue;

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
     int                  dst_format = 0;
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
          case FSSF_FLOAT:
               dst_format = -32;
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     thiz->Stop( thiz );

     pthread_mutex_lock( &data->lock );

     /* allocate buffer(s) */
     src_size = desc.buffersize * data->channels * data->format >> 3;
     if (dst_format != data->format || desc.channels != data->channels) {
          dst_size = desc.buffersize * desc.channels *
                     FS_BITS_PER_SAMPLE(desc.sampleformat) >> 3;
     }

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
     data->dest.format   = desc.sampleformat;
     data->dest.channels = desc.channels;
     data->dest.length   = desc.buffersize;

     if (data->finished) {
          direct_stream_seek( data->stream, data->headsize );
          data->finished = false;
     }

     /* start thread */
     data->playing  = true;
     data->thread   = direct_thread_create( DTT_DEFAULT,
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

     direct_stream_wait( data->stream, count, NULL );

     while (data->playing && !data->finished) {
          DFBResult       ret;
          void           *dst;
          int             size;
          int             len = 0;
          struct timeval  tv  = { 0, 500 };

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

               ret = direct_stream_wait( data->stream, size, &tv );
               if (ret != DFB_TIMEOUT)
                    ret = direct_stream_read( data->stream, size, dst, &len );

               buffer->Unlock( buffer );
          }
          else {
               ret = direct_stream_wait( data->stream, count, &tv );
               if (ret != DFB_TIMEOUT) {
                    ret = direct_stream_read( data->stream, count,
                                              data->src_buffer, &len );
               }
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

          len /= data->format >> 3;
          if (len < 1)
               continue;

          if (data->src_buffer) {
               while (len > 0) {
                    if (buffer->Lock( buffer, &dst, 0, &size ) != DFB_OK) {
                         D_ERROR( "IFusionSoundMusicProvider_Wave: "
                                  "Couldn't lock buffer!" );
                         break;
                    }

                    size /= FS_BYTES_PER_SAMPLE(data->dest.format);
                    if (size > len)
                         size = len;

                    wave_mix_audio( data->src_buffer, dst, size,
                                    data->format, data->dest.format,
                                    data->byteorder, delta );

                    buffer->Unlock( buffer );

                    len -= size;

                    if (data->callback) {
                         if (data->callback( size/data->channels, data->ctx )) {
                              data->playing = false;
                              break;
                         }
                    }
               }
          }
          else {
               if (data->callback) {
                    if (data->callback( len/data->channels, data->ctx ))
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
     int                 dst_format = 0;

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
          case FSSF_FLOAT:
               dst_format = -32;
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     thiz->Stop( thiz );

     pthread_mutex_lock( &data->lock );

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
     data->dest.format   = desc.sampleformat;
     data->dest.channels = desc.channels;
     data->dest.length   = desc.length;

     data->callback      = callback;
     data->ctx           = ctx;

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
     offset  = offset * data->channels * data->format >> 3;
     if (data->datasize && offset > data->datasize)
          return DFB_UNSUPPORTED;
     offset += data->headsize;

     pthread_mutex_lock( &data->lock );
     ret = direct_stream_seek( data->stream, offset );
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
                (double)(data->samplerate * data->channels * data->format >> 3);

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
parse_headers( DirectStream *stream,
               u32 *ret_byteorder, u32 *ret_samplerate,
               u16 *ret_channels,  u16 *ret_format,
               u32 *ret_headsize,  u32 *ret_datasize )
{
     char buf[4];
     u32  byteorder = 0;
     u32  fmt_len;
     struct {
          u16 encoding;
          u16 channels;
          u32 frequency;
          u32 byterate;
          u16 blockalign;
          u16 bitspersample;
     } fmt;
     u32 data_size;

#define wave_read( buf, count ) {\
     int len = 0;\
     if (direct_stream_read( stream, count, buf, &len ) || len < (count))\
          return DFB_UNSUPPORTED;\
}

     direct_stream_wait( stream, 44, NULL );

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
     if (byteorder != HOST_BYTEORDER)
          SWAP32( fmt_len );

     if (fmt_len < sizeof(fmt)) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "fmt chunk expected to be at least %d bytes (got %d).\n",
                   sizeof(fmt), fmt_len );
          return DFB_UNSUPPORTED;
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
          return DFB_UNSUPPORTED;
     }
     if (fmt.channels < 1 || fmt.channels > 2) {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: "
                   "Unsupported number of channels (%d).\n", fmt.channels );
          return DFB_UNSUPPORTED;
     }
     if (fmt.frequency < 4000 || fmt.frequency > 48000) {
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

     wave_read( buf, 4 );
     if (buf[0] != 'd' || buf[1] != 'a' || buf[2] != 't' || buf[3] != 'a') {
          D_DEBUG( "IFusionSoundMusicProvider_Wave: Expected 'data' header.\n" );
          return DFB_UNSUPPORTED;
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

     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     if (!memcmp( ctx->header, "RIFF", 4 ) ||
         !memcmp( ctx->header, "RIFX", 4 ))
     {
          if (!memcmp( ctx->header+8, "WAVEfmt ", 8 ))
               return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename,
           DirectStream              *stream )
{
     DFBResult    ret;
     unsigned int size;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_Wave )

     data->ref    = 1;
     data->stream = direct_stream_dup( stream );

     ret = parse_headers( data->stream,
                          &data->byteorder, &data->samplerate,
                          &data->channels,  &data->format,
                          &data->headsize,  &data->datasize );
     if (ret) {
          IFusionSoundMusicProvider_Wave_Destruct( thiz );
          return ret;
     }

     size = direct_stream_length( data->stream );
     if (size) {
          if (data->datasize)
               data->datasize = MIN( data->datasize, size - data->headsize );
          else
               data->datasize = size - data->headsize;
     }

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

