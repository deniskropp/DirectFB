/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <asm/types.h>


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


#define RGB32_TO_RGB15(pixel)  ( (((pixel) & 0xF80000) >> 9) | \
                                 (((pixel) & 0x00F800) >> 6) | \
                                 (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_RGB16(pixel)  ( (((pixel) & 0xF80000) >> 8) | \
                                 (((pixel) & 0x00FC00) >> 5) | \
                                 (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_RGB24(pixel)  ( (pixel) & 0x00FFFFFF )

#define RGB32_TO_ARGB(pixel)   ( 0xFF000000 | (pixel) )


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
     memcpy( dst, src, width*4 );
}

#endif
