/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <directfb.h>


/* pixel packing */

#define PIXEL_RGB332(r,g,b)    ( (((r)&0xE0)     ) | \
                                 (((g)&0xE0) >> 3) | \
                                 (((b)&0xC0) >> 6) )

#define PIXEL_ARGB1555(a,r,g,b)( (((a)&0x80) << 8) | \
                                 (((r)&0xF8) << 7) | \
                                 (((g)&0xF8) << 2) | \
                                 (((b)&0xF8) >> 3) )

#define PIXEL_RGB16(r,g,b)     ( (((r)&0xF8) << 8) | \
                                 (((g)&0xFC) << 3) | \
                                 (((b)&0xF8) >> 3) )

#define PIXEL_RGB32(r,g,b)     ( ((r) << 16) | \
                                 ((g) <<  8) | \
                                  (b) )

#define PIXEL_ARGB(a,r,g,b)    ( ((a) << 24) | \
                                 ((r) << 16) | \
                                 ((g) << 8)  | \
                                  (b) )

#define PIXEL_AiRGB(a,r,g,b)   ( (((a) ^ 0xff) << 24) | \
                                 ((r) << 16) | \
                                 ((g) << 8)  | \
                                  (b) )

#define PIXEL_YUY2(y,u,v)      ( ((v) << 24) | \
                                 ((y) << 16) | \
                                 ((u) << 8)  | \
                                  (y) )

#define PIXEL_UYVY(y,u,v)      ( ((y) << 24) | \
                                 ((v) << 16) | \
                                 ((y) << 8)  | \
                                  (u) )


/* packed pixel conversions */

#define ARGB1555_TO_RGB332(pixel) ( (((pixel) & 0x7000) >> 7) | \
                                    (((pixel) & 0x0380) >> 5) | \
                                    (((pixel) & 0x0018) >> 3) )

#define ARGB1555_TO_RGB16(pixel)  ( (((pixel) & 0x7C00) << 1) | \
                                    (((pixel) & 0x03E0) << 1) | \
                                    (((pixel) & 0x001F)) )

#define ARGB1555_TO_RGB32(pixel)  ( (((pixel) & 0x7C00) << 9) | \
                                    (((pixel) & 0x03E0) << 6) | \
                                    (((pixel) & 0x001F) << 3) )

#define ARGB1555_TO_ARGB(pixel)   ( (((pixel) & 0x8000) ? 0xFF000000 : 0) | \
                                    (((pixel) & 0x7C00) << 9) | \
                                    (((pixel) & 0x03E0) << 6) | \
                                    (((pixel) & 0x001F) << 3) )


#define RGB16_TO_RGB332(pixel) ( (((pixel) & 0xE000) >> 8) | \
                                 (((pixel) & 0x0700) >> 6) | \
                                 (((pixel) & 0x0018) >> 3) )

#define RGB16_TO_ARGB1555(pixel)  ( 0x8000 | \
                                    (((pixel) & 0xF800) >> 1) | \
                                    (((pixel) & 0x07C0) >> 1) | \
                                    (((pixel) & 0x001F)) )

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

#define RGB32_TO_ARGB1555(pixel)  ( 0x8000 | \
                                    (((pixel) & 0xF80000) >> 9) | \
                                    (((pixel) & 0x00F800) >> 6) | \
                                    (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_RGB16(pixel)  ( (((pixel) & 0xF80000) >> 8) | \
                                 (((pixel) & 0x00FC00) >> 5) | \
                                 (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_ARGB1555(pixel) ( 0x8000 | \
                                   (((pixel) & 0xF80000) >> 9) | \
                                   (((pixel) & 0x00F800) >> 6) | \
                                   (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_ARGB(pixel)   ( 0xFF000000 | (pixel) )


#define ARGB_TO_ARGB1555(pixel)  ( (((pixel) & 0x80000000) >> 16) | \
                                   (((pixel) & 0x00F80000) >>  9) | \
                                   (((pixel) & 0x0000F800) >>  6) | \
                                   (((pixel) & 0x000000F8) >>  3) )


/* RGB to YUV */

#define Y_FROM_RGB(r,g,b)      ( ( 0.2290 * r + 0.5670 * g + 0.1440 * b) * \
                                   219 / 255 + 16 )

#define CB_FROM_RGB(r,g,b)     ( (-0.1687 * r - 0.3313 * g + 0.5000 * b) * \
                                   112 / 127 + 128)

#define CR_FROM_RGB(r,g,b)     ( ( 0.5000 * r - 0.4187 * g - 0.0813 * b) * \
                                   112 / 127 + 128)


DFBSurfacePixelFormat dfb_pixelformat_for_depth( int depth );

__u32 dfb_color_to_pixel( DFBSurfacePixelFormat format,
                          __u8 r, __u8 g, __u8 b );

static inline void
dfb_argb_to_rgb332( __u32 *src, __u8 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register __u32 argb = src[i];

          dst[i] = RGB32_TO_RGB332( argb );
     }
}

static inline void
dfb_argb_to_argb1555( __u32 *src, __u16 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register __u32 argb = src[i];

          dst[i] = ARGB_TO_ARGB1555( argb );
     }
}

static inline void
dfb_argb_to_rgb16( __u32 *src, __u16 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register __u32 argb = src[i];

          dst[i] = RGB32_TO_RGB16( argb );
     }
}

static inline void
dfb_argb_to_a8( __u32 *src, __u8 *dst, int len )
{
     int i;

     for (i=0; i<len; i++)
          dst[i] = src[i] >> 24;
}

#endif
