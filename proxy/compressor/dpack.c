/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Claudio Ciccani <klan@users.sf.net>.

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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include "dpack.h"

/*
 * ABSTRACT:
 *    DPack (Delta Pack) is a loseless (or better, minimum-loss) compression method
 *    based on delta coding and bit packing. 
 *
 * BITSTREAM SYNTAX:
 *    The stream is encoded as a sequence of non-interleaved channels with the 1st
 *    channel staring at offset 0.
 *    Each channel if encoded as following:
 *      1 BYTE    - Header:
 *                       Bit 0-4: number of bits per absolute delta.
 *                       Bit 5  : 0 if unsigned deltas, 1 if signed;
 *                                add this bit to the previous value to
 *                                obtain the effective amount of bits per delta.
 *                       Bit 6  : deltas are encoded as the difference between
 *                                the effective delta and the minimum delta (MD).
 *                       Bit 7  : channels are coupled
 *
 *
 *      N BYTES   - First Sample:
 *                       N can be 1, 2 or 4, depending on the sample format.
 *
 *      IF [Header:6]
 *        N BYTES - Minimum Delta:
 *                       this is the minimum delta value for the whole packet;
 *                       N can be 1, 2 or 4, depending on the sample format.
 *      ENDIF
 *
 *      N BITS    - Deltas:
 *                       deltas are encoded using the following formalas
 *                        
 *                         ENCODING: IF [Header:7 && c > 0]
 *                                      I(t)[c] -= I(t)[c-1]
 *                                   ENDIF
 *
 *                                   D(t)[c] = (I(t)[c] - O(t-1)[c]) / 2
 *
 *                         DECODING: IF [Header:6]
 *                                      O(t)[c] = O(t-1)[c] + (D(t)[c] + MD) * 2
 *                                   ELSE
 *                                      O(t)[c] = O(t-1)[c] + D(t)[c] * 2
 *                                   ENDIF
 *
 *                                   IF [Header:7 && c > 0]
 *                                      O(t)[c] += O(t)[c-1]
 *                                   ENDIF
 *
 *                       where t is the frame index, c is the channel index,
 *                       D(t) is the delta at t, I(t) is the input sample
 *                       at t, O(t) is the output sample at t and MD is the
 *                       minimum delta;
 *                       the amount of bits used to store the delta is given by
 *                       the Header, while the effective number of deltas is an
 *                       input parameter for the decoder (i.e. you must know the
 *                       original length in frames of the packet to decode it).
 *
 *      N BITS    - Align:
 *                       empty bits used to align next channel to the byte boundary.
 *
 */

/**************************************************************************************************/

struct bitio {
     u8  *ptr;
     int  left;
};

static inline void
bitio_init( struct bitio *bio, void *ptr )
{
     bio->ptr  = ptr;
     bio->left = 8;
}

static inline void
bitio_put( struct bitio *bio, u32 val, int n )
{

     if (n >= 8) {
          if (!(bio->left & 7)) {
               do {
                    n -= 8;
                    *bio->ptr++ = val >> n;
               } while (n >= 8);
          }
          else {
               do {
                    u8 v;
                    n -= 8;
                    v = val >> n;
                    *(bio->ptr+0) |= v >> (8-bio->left);
                    *(bio->ptr+1)  = v << bio->left;
                    bio->ptr++;
               } while (n >= 8);
          }
     }

     while (n--) {
          if (!(bio->left & 7)) {
               *bio->ptr = ((val >> n) & 1) << 7;
               bio->left--;
          }
          else {
               *bio->ptr |= ((val >> n) & 1) << --bio->left;
               if (!bio->left) {
                    bio->left = 8;
                    bio->ptr++;
               }
          }
     }
}

static inline u32
bitio_get( struct bitio *bio, int n )
{
     u32 ret = 0;

     if (n >= 8) {
          if (!(bio->left & 7)) {
               do {
                    n -= 8;
                    ret |= *bio->ptr << n;
                    bio->ptr++;
               } while (n >= 8);
          }
          else {
               do {
                    n -= 8;
                    ret |= (u32)((u8)(*(bio->ptr+0) << (8-bio->left)) |
                                 (u8)(*(bio->ptr+1) >> bio->left)) << n;
                    bio->ptr++;
               } while (n >= 8);
          }
     }

     while (n--) {
          ret |= ((*bio->ptr >> --bio->left) & 1) << n;
          if (!bio->left) {
               bio->left = 8;
               bio->ptr++;
          }
     }
     
     return ret;
}

static inline s32
bitio_gets( struct bitio *bio, int n )
{
     s32 ret;

     if (n >= 8) {
          n -= 8;
          if (!(bio->left & 7)) {
               ret = *((s8*)bio->ptr) << n;
               bio->ptr++;
               while (n >= 8) {
                    n -= 8;
                    ret |= *bio->ptr << n;
                    bio->ptr++;
               } 
          }
          else {
               ret = (s32)((s8)(*(bio->ptr+0) << (8-bio->left)) |
                           (u8)(*(bio->ptr+1) >> bio->left)) << n;
               bio->ptr++;
               while (n >= 8) {
                    n -= 8;
                    ret |= (u32)((u8)(*(bio->ptr+0) << (8-bio->left)) |
                                 (u8)(*(bio->ptr+1) >> bio->left)) << n;
                    bio->ptr++;
               } 
          }
     }
     else {
          ret = -((*bio->ptr >> --bio->left) & 1) << --n;
          if (!bio->left) {
               bio->left = 8;
               bio->ptr++;
          }
     }

     while (n--) {
          ret |= ((*bio->ptr >> --bio->left) & 1) << n;
          if (!bio->left) {
               bio->left = 8;
               bio->ptr++;
          }
     }
     
     return ret;
}               

static inline void*
bitio_tell( struct bitio *bio )
{
     return (bio->left & 7) ? (bio->ptr+1) : bio->ptr;
}

/**************************************************************************************************/

static inline int 
bitcount( unsigned int val )
{
     int ret = 0;
     
     while (val & ~0xff) {
          val >>= 8;
          ret += 8;
     }
     
     while (val) {
          val >>= 1;
          ret++;
     }
     
     return ret;
}

/**************************************************************************************************/

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


/* Converted to S8 */
#define TYPE u8
#define SAMP_MAX 127
#define GET_SAMP(src) ((s8)(*((u8*)src) ^ 0x80))
#define PUT_SAMP(dst, s) *((u8*)dst) = (s) ^ 0x80
#define GET_CODED_SAMP(src) GET_SAMP(src)
#define PUT_CODED_SAMP(dst, s) PUT_SAMP(dst, s)
#include "dpack_proto.h"


#define TYPE s16
#define SAMP_MAX 32767
#define GET_SAMP(src) (*((s16*)src))
#define PUT_SAMP(dst, s) *((s16*)dst) = (s)
#define GET_CODED_SAMP(src) ((((s8*)src)[0] << 8) | ((u8*)src)[1])
#define PUT_CODED_SAMP(dst, s) ((s8*)dst)[0] = (s) >> 8; ((u8*)dst)[1] = (s)
#include "dpack_proto.h"


#define TYPE s24
#define SAMP_MAX 8388607
#define GET_SAMP(src) (((s24*)src)->a | (((s24*)src)->b << 8) | (((s24*)src)->c << 16))
#define PUT_SAMP(dst, s) ((s24*)dst)->a = (s); ((s24*)dst)->b = (s) >> 8; ((s24*)dst)->c = (s) >> 16
#define GET_CODED_SAMP(src) ((((s8*)src)[0] << 16) | (((u8*)src)[1] << 8) | ((u8*)src)[2])
#define PUT_CODED_SAMP(dst, s) ((s8*)dst)[0] = (s) >> 16; ((u8*)dst)[1] = (s) >> 8; ((u8*)dst)[2] = (s)
#include "dpack_proto.h"


/* Scaled down by 1 to prevent overflow */
#define TYPE s32
#define SAMP_MAX 1073741823
#define GET_SAMP(src) (*((s32*)src) >> 1)
#define PUT_SAMP(dst, s) *((s32*)dst) = (s) << 1
#define GET_CODED_SAMP(src) ((((s8*)src)[0]<<24) | (((u8*)src)[1]<<16) | (((u8*)src)[2]<<8) | ((u8*)src)[3])
#define PUT_CODED_SAMP(dst, s) ((s8*)dst)[0] = (s)>>24; ((u8*)dst)[1] = (s)>>16; ((u8*)dst)[2] = (s)>>8; ((u8*)dst)[3] = (s)
#include "dpack_proto.h"


/* Converted to S31 */
#define TYPE float
#define SAMP_MAX 1073741823
#define GET_SAMP(src) ((s32)(*((float*)src) * 1073741823.f))
#define PUT_SAMP(dst, s) *((float*)dst) = (float)(s) / 1073741823.f
#define GET_CODED_SAMP(src) ((((s8*)src)[0]<<24) | (((u8*)src)[1]<<16) | (((u8*)src)[2]<<8) | ((u8*)src)[3])
#define PUT_CODED_SAMP(dst, s) ((s8*)dst)[0] = (s)>>24; ((u8*)dst)[1] = (s)>>16; ((u8*)dst)[2] = (s)>>8; ((u8*)dst)[3] = (s)
#include "dpack_proto.h"



int 
dpack_encode( const void *source, FSSampleFormat format, int channels, int length, u8 *dest )
{
     int frames = length;
     int bytes  = channels * FS_BYTES_PER_SAMPLE(format);
     int size   = 0;
#if D_DEBUG_ENABLED
     static unsigned long ctotal = 0;
     static unsigned long rtotal = 0;
#endif
     
     D_ASSERT( source != NULL );
     D_ASSERT( channels > 0 );
     D_ASSERT( length > 0 );
     D_ASSERT( dest != NULL );
     
     switch (format) {
          case FSSF_U8:
               while (frames) {
                    int num = MIN(frames, DPACK_FRAMES);
                    size   += dpack_encode_u8( source, channels, num, dest+size );
                    source += num * bytes;
                    frames -= num;
               }                    
               break;
               
          case FSSF_S16:
               while (frames) {
                    int num = MIN(frames, DPACK_FRAMES);
                    size   += dpack_encode_s16( source, channels, num, dest+size );
                    source += num * bytes;
                    frames -= num;
               }
               break;
               
          case FSSF_S24:
               while (frames) {
                    int num = MIN(frames, DPACK_FRAMES);
                    size   += dpack_encode_s24( source, channels, num, dest+size );
                    source += num * bytes;
                    frames -= num;
               }
               break;
               
          case FSSF_S32:
               while (frames) {
                    int num = MIN(frames, DPACK_FRAMES);
                    size   += dpack_encode_s32( source, channels, num, dest+size );
                    source += num * bytes;
                    frames -= num;
               }
               break;
               
          case FSSF_FLOAT:
               while (frames) {
                    int num = MIN(frames, DPACK_FRAMES);
                    size   += dpack_encode_float( source, channels, num, dest+size );
                    source += num * bytes;
                    frames -= num;
               }
               break;
               
          default:
               D_WARN( "unsupported sample format" );
               return 0;
     }
     
     D_DEBUG( "DPACK: raw=%6d encoded=%6d ratio=%02d%% avg=%02d%%\n",
              length * bytes, size, size * 100 / (length * bytes),
              (int)({ctotal += size; rtotal += length * bytes; ctotal*100/rtotal;}) );
              
     return size;
}

int 
dpack_decode( const u8 *source, FSSampleFormat format, int channels, int length, void *dest )
{
     int frames = length;
     int bytes  = channels * FS_BYTES_PER_SAMPLE(format);
     int size   = 0;
     
     D_ASSERT( source != NULL );
     D_ASSERT( channels > 0 );
     D_ASSERT( length > 0 );
     D_ASSERT( dest != NULL );  
     
     switch (format) {
          case FSSF_U8:
               while (frames) {
                    int num = MIN(frames, DPACK_FRAMES);
                    size   += dpack_decode_u8( source+size, channels, num, dest );
                    dest   += num * bytes;
                    frames -= num;
               }
               break;
               
          case FSSF_S16:
               while (frames) {
                    int num = MIN(frames, DPACK_FRAMES);
                    size   += dpack_decode_s16( source+size, channels, num, dest );
                    dest   += num * bytes;
                    frames -= num;
               }
               break;
               
          case FSSF_S24:
               while (frames) {
                    int num = MIN(frames, DPACK_FRAMES);
                    size   += dpack_decode_s24( source+size, channels, num, dest );
                    dest   += num * bytes;
                    frames -= num;
               }
               break;
               
          case FSSF_S32:
               while (frames) {
                    int num = MIN(frames, DPACK_FRAMES);
                    size   += dpack_decode_s32( source+size, channels, num, dest );
                    dest   += num * bytes;
                    frames -= num;
               }
               break;
               
          case FSSF_FLOAT:
               while (frames) {
                    int num = MIN(frames, DPACK_FRAMES);
                    size   += dpack_decode_float( source+size, channels, num, dest );
                    dest   += num * bytes;
                    frames -= num;
               }
               break;
               
          default:
               D_WARN( "unsupported sample format" );
               return 0;
     }
      
     return size;
}

