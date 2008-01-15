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

#define GEN_FUNC_NAME( action, format )  dpack_##action##_##format
#define FUNC_NAME( action, format )      GEN_FUNC_NAME( action, format )


static int
FUNC_NAME(encode, TYPE)( const TYPE *source, int channels, int length, u8 *dest )
{
     u8  *dst = dest;
     int  c;
     
     for (c = 0; c < channels; c++) {
          TYPE *src;
          int   bits = 0;
          int   sign = 0;
          int   mind = 0;
          int   prev;
          int   n;
          
          src = (TYPE*) &source[c];
          prev = GET_SAMP(src);
          src += channels;
          
          if (length > 1) {
               int max = 0;
               int min = 0x7fffffff;
               int an, bn;
               int as, bs;
               
               for (n = length-1; n; n--) {
                    int delta = (GET_SAMP(src) - prev) >> DPACK_SCALE;
                    if (delta > max)
                         max = delta;
                    if (delta < min)
                         min = delta;
                    prev += delta << DPACK_SCALE;
                    src += channels;
               }
               
               max = CLAMP( max, -SAMP_MAX, SAMP_MAX );
               min = CLAMP( min, -SAMP_MAX, SAMP_MAX );
               
               if (ABS(max) > ABS(min))
                    an = bitcount( ABS(max) );
               else
                    an = bitcount( ABS(min) );
               as = (max < 0 || min < 0);
               
               bn = bitcount( ABS(max-min) );
               bs = (max-min < 0);
               
               if (an+as > bn+bs && length > sizeof(TYPE)*8) {
                    bits = bn;
                    sign = bs;
                    mind = min;
               }
               else {
                    bits = an;
                    sign = as;
               }
          }
         
          src = (TYPE*) &source[c];
          prev = GET_SAMP(src);
          src += channels;
          
          *dst++ = (bits & 31) | (sign << 5) | ((mind != 0) << 6);
          PUT_CODED_SAMP(dst, prev);
          dst += sizeof(TYPE);
          if (mind) {
               PUT_CODED_SAMP(dst, mind);
               dst += sizeof(TYPE);
          }
          
          bits += sign;
          if (bits) {
               struct bitio bio;
               
               bitio_init( &bio, dst );
               
               for (n = length-1; n; n--) {
                    int delta = (GET_SAMP(src) - prev) >> DPACK_SCALE;
                    bitio_put( &bio, delta-mind, bits );
                    prev += delta << DPACK_SCALE;
                    src += channels;
               }
               
               dst = bitio_tell( &bio );
          }
     }
     
     return (int)(dst - dest);
}

static int
FUNC_NAME(decode, TYPE)( const u8 *source, int channels, int length, TYPE *dest )
{
     u8  *src = (u8*) source;
     int  c;
     
     for (c = 0; c < channels; c++) {
          TYPE *dst = &dest[c];
          int   bits;
          int   sign;
          int   mind;
          int   prev;
          int   n;
          
          bits = *src++;
          prev = GET_CODED_SAMP(src);
          src += sizeof(TYPE);
          if (bits & (1 << 6)) {
               mind = GET_CODED_SAMP(src);
               src += sizeof(TYPE);
          } else {
               mind = 0;
          }
          
          PUT_SAMP(dst, prev);
          dst += channels;
          
          sign = (bits >> 5) & 1;
          bits = (bits & 31) + sign;
          if (bits) {
               struct bitio bio;
               
               bitio_init( &bio, src );
               
               if (sign) {
                    for (n = length-1; n; n--) {
                         prev += (bitio_gets( &bio, bits ) + mind) << DPACK_SCALE;
                         PUT_SAMP(dst, prev);
                         dst += channels;
                    }
               }
               else {
                    for (n = length-1; n; n--) {
                         prev += (bitio_get( &bio, bits ) + mind) << DPACK_SCALE;
                         PUT_SAMP(dst, prev);
                         dst += channels;
                    }
               }
               
               src = bitio_tell( &bio );
          }
          else {
               mind <<= DPACK_SCALE;
               for (n = length-1; n; n--) {
                    prev += mind;
                    PUT_SAMP(dst, prev);
                    dst += channels;
               }
          }
     }
     
     return (int)(src - source);
}


#undef FUNC_NAME
#undef GEN_FUNC_NAME

#undef TYPE
#undef SAMP_MAX
#undef GET_SAMP
#undef PUT_SAMP
#undef GET_CODED_SAMP
#undef PUT_CODED_SAMP

               
                         
               
               
          
          
          
