/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __GFX__CONVERT_H__
#define __GFX__CONVERT_H__

#include <string.h>
#include <directfb.h>
#include <asm/types.h>

#include "misc/memcpy.h"


#define PIXEL_RGB332(r,g,b)    ( (((r)&0xE0)     ) | \
                                 (((g)&0xE0) >> 3) | \
                                 (((b)&0xC0) >> 6) )

#define PIXEL_RGB15(r,g,b)     ( (((r)&0xF8) << 7) | \
                                 (((g)&0xF8) << 2) | \
                                 (((b)&0xF8) >> 3) )

#define PIXEL_RGB16(r,g,b)     ( (((r)&0xF8) << 8) | \
                                 (((g)&0xFC) << 3) | \
                                 (((b)&0xF8) >> 3) )

#define PIXEL_RGB24(r,g,b)     ( ((r) << 16) | \
                                 ((g) <<  8) | \
                                  (b) )

#define PIXEL_RGB32(r,g,b)     ( ((r) << 16) | \
                                 ((g) <<  8) | \
                                  (b) )

#define PIXEL_ARGB(a,r,g,b)    ( ((a) << 24) | \
                                 ((r) << 16) | \
                                 ((g) << 8)  | \
                                  (b) )


#define RGB15_TO_RGB332(pixel) ( (((pixel) & 0x7000) >> 7) | \
                                 (((pixel) & 0x0380) >> 5) | \
                                 (((pixel) & 0x0018) >> 3) )

#define RGB15_TO_RGB16(pixel)  ( (((pixel) & 0x7C00) << 1) | \
                                 (((pixel) & 0x03E0) << 1) | \
                                 (((pixel) & 0x001F)) )

#define RGB15_TO_RGB24(pixel)  ( (((pixel) & 0x7C00) << 9) | \
                                 (((pixel) & 0x03E0) << 6) | \
                                 (((pixel) & 0x001F) << 3) )

#define RGB15_TO_RGB32(pixel)  ( (((pixel) & 0x7C00) << 9) | \
                                 (((pixel) & 0x03E0) << 6) | \
                                 (((pixel) & 0x001F) << 3) )

#define RGB15_TO_ARGB(pixel)   ( 0xFF000000 |                \
                                 (((pixel) & 0x7C00) << 9) | \
                                 (((pixel) & 0x03E0) << 6) | \
                                 (((pixel) & 0x001F) << 3) )


#define RGB16_TO_RGB332(pixel) ( (((pixel) & 0xE000) >> 8) | \
                                 (((pixel) & 0x0700) >> 6) | \
                                 (((pixel) & 0x0018) >> 3) )

#define RGB16_TO_RGB15(pixel)  ( (((pixel) & 0xF800) >> 1) | \
                                 (((pixel) & 0x07C0) >> 1) | \
                                 (((pixel) & 0x001F)) )

#define RGB16_TO_RGB24(pixel)  ( (((pixel) & 0xF800) << 8) | \
                                 (((pixel) & 0x07E0) << 5) | \
                                 (((pixel) & 0x001F) << 3) )

#define RGB16_TO_RGB32(pixel)  ( (((pixel) & 0xF800) << 8) | \
                                 (((pixel) & 0x07E0) << 5) | \
                                 (((pixel) & 0x001F) << 3) )

#define RGB16_TO_ARGB(pixel)   ( 0xFF000000 |                \
                                 (((pixel) & 0xF800) << 8) | \
                                 (((pixel) & 0x07E0) << 5) | \
                                 (((pixel) & 0x001F) << 3) )


#define RGB32_TO_RGB332(pixel) ( (((pixel) & 0xE00000) >> 16) | \
                                 (((pixel) & 0x00E000) >> 11) | \
                                 (((pixel) & 0x0000C0) >> 6) )

#define RGB32_TO_RGB15(pixel)  ( (((pixel) & 0xF80000) >> 9) | \
                                 (((pixel) & 0x00F800) >> 6) | \
                                 (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_RGB16(pixel)  ( (((pixel) & 0xF80000) >> 8) | \
                                 (((pixel) & 0x00FC00) >> 5) | \
                                 (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_RGB24(pixel)  ( (pixel) & 0x00FFFFFF )

#define RGB32_TO_ARGB(pixel)   ( 0xFF000000 | (pixel) )


static inline DFBSurfacePixelFormat dfb_pixelformat_for_depth( int depth )
{
     switch (depth) {
          case 8:
#ifdef SUPPORT_RGB332
               return DSPF_RGB332;
#else
               return DSPF_LUT8;
#endif
          case 15:
               return DSPF_RGB15;
          case 16:
               return DSPF_RGB16;
          case 24:
               return DSPF_RGB24;
          case 32:
               return DSPF_RGB32;
     }

     return DSPF_UNKNOWN;
}

static inline __u32 color_to_pixel( DFBSurfacePixelFormat format,
                                    __u8 r, __u8 g, __u8 b )
{
     __u32 pixel;

     switch (format) {
#ifdef SUPPORT_RGB332
          case DSPF_RGB332:
               pixel = PIXEL_RGB332( r, g, b );
               break;
#endif
          case DSPF_RGB15:
               pixel = PIXEL_RGB15( r, g, b );
               break;
          case DSPF_RGB16:
               pixel = PIXEL_RGB16( r, g, b );
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
               pixel = PIXEL_RGB24( r, g, b );
               break;
          default:
               pixel = 0;
     }

     return pixel;
}

static inline void span_a1_to_argb( __u8 *src, __u32 *dst, int width )
{
     int i;
     for (i = 0; i < width; i++)
          *dst++ =  PIXEL_ARGB( (src[i>>3] & (1<<(7-(i%8)))) ? 0xFF : 0x0,
                                0xFF, 0xFF, 0xFF );
}

static inline void span_a1_to_a8( __u8 *src, __u8 *dst, int width )
{
     int i;
     for (i = 0; i < width; i++)
          *dst++ = (src[i>>3] & (1<<(7-(i%8)))) ? 0xFF : 0x0;
}


static inline void span_a8_to_argb( __u8 *src, __u32 *dst, int width )
{
     while (width--) *dst++ = PIXEL_ARGB( *src++, 0xFF, 0xFF, 0xFF );
}


static inline void span_rgb15_to_rgb16( __u16 *src, __u16 *dst, int width )
{
     while (width--) *dst++ = RGB15_TO_RGB16( *src ), src++;
}

static inline void span_rgb15_to_rgb32( __u16 *src, __u32 *dst, int width )
{
     while (width--) *dst++ = RGB15_TO_RGB32( *src ), src++;
}

static inline void span_rgb15_to_argb( __u16 *src, __u32 *dst, int width )
{
     while (width--) *dst++ = RGB15_TO_ARGB( *src ), src++;
}


static inline void span_rgb16_to_rgb15( __u16 *src, __u16 *dst, int width )
{
     while (width--) *dst++ = RGB16_TO_RGB15( *src ), src++;
}

static inline void span_rgb16_to_rgb32( __u16 *src, __u32 *dst, int width )
{
     while (width--) *dst++ = RGB16_TO_RGB32( *src ), src++;
}

static inline void span_rgb16_to_argb( __u16 *src, __u32 *dst, int width )
{
     while (width--) *dst++ = RGB16_TO_ARGB( *src ), src++;
}


static inline void span_rgb32_to_rgb332( __u32 *src, __u8 *dst, int width )
{
     while (width--) *dst++ = RGB32_TO_RGB332( *src ), src++;
}

static inline void span_rgb32_to_rgb15( __u32 *src, __u16 *dst, int width )
{
     while (width--) *dst++ = RGB32_TO_RGB15( *src ), src++;
}

static inline void span_rgb32_to_rgb16( __u32 *src, __u16 *dst, int width )
{
     while (width--) *dst++ = RGB32_TO_RGB16( *src ), src++;
}

static inline void span_rgb32_to_argb( __u32 *src, __u32 *dst, int width )
{
     while (width--) *dst++ = RGB32_TO_ARGB( *src ), src++;
}


static inline void span_argb_to_a8( __u32 *src, __u8 *dst, int width )
{
     while (width--) *dst++ = *src >> 24, src++;
}

static inline void span_argb_to_rgb15( __u32 *src, __u16 *dst, int width )
{
     while (width--) *dst++ = RGB32_TO_RGB15( *src ), src++;
}

static inline void span_argb_to_rgb16( __u32 *src, __u16 *dst, int width )
{
     while (width--) *dst++ = RGB32_TO_RGB16( *src ), src++;
}

static inline void span_argb_to_rgb32( __u32 *src, __u32 *dst, int width )
{
     dfb_memcpy( dst, src, width*4 );
}

#endif
