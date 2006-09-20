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

#include <direct/util.h>


/* pixel packing */

#define PIXEL_RGB332(r,g,b)    ( (((r)&0xE0)     ) | \
                                 (((g)&0xE0) >> 3) | \
                                 (((b)&0xC0) >> 6) )

#define PIXEL_ARGB1555(a,r,g,b)( (((a)&0x80) << 8) | \
                                 (((r)&0xF8) << 7) | \
                                 (((g)&0xF8) << 2) | \
                                 (((b)&0xF8) >> 3) )

#define PIXEL_ARGB2554(a,r,g,b)( (((a)&0xC0) << 8) | \
                                 (((r)&0xF8) << 6) | \
                                 (((g)&0xF8) << 1) | \
                                 (((b)&0xF0) >> 4) )

#define PIXEL_ARGB4444(a,r,g,b)( (((a)&0xF0) << 8) | \
                                 (((r)&0xF0) << 4) | \
                                 (((g)&0xF0)     ) | \
                                 (((b)&0xF0) >> 4) )

#define PIXEL_RGB16(r,g,b)     ( (((r)&0xF8) << 8) | \
                                 (((g)&0xFC) << 3) | \
                                 (((b)&0xF8) >> 3) )

#define PIXEL_RGB18(r,g,b)     ( (((r)&0xFC) << 10) | \
                                 (((g)&0xFC) << 4) | \
                                 (((b)&0xFC) >> 2) )

#define PIXEL_RGB32(r,g,b)     ( ((r) << 16) | \
                                 ((g) <<  8) | \
                                  (b) )

#define PIXEL_ARGB(a,r,g,b)    ( ((a) << 24) | \
                                 ((r) << 16) | \
                                 ((g) << 8)  | \
                                  (b) )

#define PIXEL_ARGB1666(a,r,g,b) ( ( ((a)&0x80) << 11) | \
                                 (  ((r)&0xFC) << 10) | \
                                 (  ((g)&0xFC) << 4 )  | \
                                    ((b)&0xFC) >>2 )

#define PIXEL_ARGB6666(a,r,g,b) ( ( ((a)&0xFC) << 16) | \
                                 (  ((r)&0xFC) << 10) | \
                                 (  ((g)&0xFC) << 4 )  | \
                                    ((b)&0xFC) >>2 )

#define PIXEL_AYUV(a,y,u,v)    ( ((a) << 24) | \
                                 ((y) << 16) | \
                                 ((u) << 8)  | \
                                  (v) )

#define PIXEL_AiRGB(a,r,g,b)   ( (((a) ^ 0xff) << 24) | \
                                 ((r) << 16) | \
                                 ((g) << 8)  | \
                                  (b) )

#ifdef WORDS_BIGENDIAN

#define PIXEL_YUY2(y,u,v)      ( ((u) << 24) | \
                                 ((y) << 16) | \
                                 ((v) << 8)  | \
                                  (y) )

#define PIXEL_UYVY(y,u,v)      ( ((y) << 24) | \
                                 ((u) << 16) | \
                                 ((y) << 8)  | \
                                  (v) )
#else /* little endian */
     
#define PIXEL_YUY2(y,u,v)      ( ((v) << 24) | \
                                 ((y) << 16) | \
                                 ((u) << 8)  | \
                                  (y) )

#define PIXEL_UYVY(y,u,v)      ( ((y) << 24) | \
                                 ((v) << 16) | \
                                 ((y) << 8)  | \
                                  (u) )

#endif


/* packed pixel conversions */

#define ARGB1555_TO_RGB332(pixel) ( (((pixel) & 0x7000) >> 7) | \
                                    (((pixel) & 0x0380) >> 5) | \
                                    (((pixel) & 0x0018) >> 3) )

#define ARGB1555_TO_ARGB2554(pixel) ( (((pixel) & 0x8000)     ) | \
                                      (((pixel) & 0x7FFF) >> 1) )

#define ARGB1555_TO_ARGB4444(pixel) ( (((pixel) & 0x8000)     ) | \
                                      (((pixel) & 0x7800) >> 3) | \
                                      (((pixel) & 0x03C0) >> 2) | \
                                      (((pixel) & 0x001E) >> 1) )

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

#define RGB16_TO_ARGB2554(pixel)  ( 0xC000 |                    \
                                    (((pixel) & 0xF800) >> 2) | \
                                    (((pixel) & 0x07C0) >> 2) | \
                                    (((pixel) & 0x001F) >> 1) )

#define RGB16_TO_ARGB4444(pixel)  ( 0xF000 |                    \
                                    (((pixel) & 0xF000) >> 4) | \
                                    (((pixel) & 0x0780) >> 3) | \
                                    (((pixel) & 0x001F) >> 1) )

#define RGB16_TO_RGB32(pixel)  ( (((pixel) & 0xF800) << 8) | \
                                 (((pixel) & 0x07E0) << 5) | \
                                 (((pixel) & 0x001F) << 3) )

#define RGB16_TO_ARGB(pixel)   ( 0xFF000000 |                \
                                 (((pixel) & 0xF800) << 8) | \
                                 (((pixel) & 0x07E0) << 5) | \
                                 (((pixel) & 0x001F) << 3) )

#define RGB18_TO_ARGB(pixel)   ( 0xFF000000 |                \
                                 (((pixel) & 0xFC00) << 10) | \
                                 (((pixel) & 0x3F00) << 4) | \
                                 (((pixel) & 0x00FC) << 2) )

#define RGB32_TO_RGB332(pixel) ( (((pixel) & 0xE00000) >> 16) | \
                                 (((pixel) & 0x00E000) >> 11) | \
                                 (((pixel) & 0x0000C0) >> 6) )

#define RGB32_TO_ARGB1555(pixel)  ( 0x8000 | \
                                    (((pixel) & 0xF80000) >> 9) | \
                                    (((pixel) & 0x00F800) >> 6) | \
                                    (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_ARGB2554(pixel)  ( 0xC000 |                       \
                                    (((pixel) & 0xF80000) >> 10) | \
                                    (((pixel) & 0x00F800) >>  7) | \
                                    (((pixel) & 0x0000F0) >>  4) )

#define RGB32_TO_ARGB4444(pixel)  ( 0xF000 |                       \
                                    (((pixel) & 0xF00000) >> 12) | \
                                    (((pixel) & 0x00F000) >>  8) | \
                                    (((pixel) & 0x0000F0) >>  4) )

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

#define ARGB_TO_ARGB2554(pixel)  ( (((pixel) & 0xC0000000) >> 16) | \
                                   (((pixel) & 0x00F80000) >> 10) | \
                                   (((pixel) & 0x0000F800) >>  7) | \
                                   (((pixel) & 0x000000F0) >>  4) )

#define ARGB_TO_ARGB4444(pixel)  ( (((pixel) & 0xF0000000) >> 16) | \
                                   (((pixel) & 0x00F00000) >> 12) | \
                                   (((pixel) & 0x0000F000) >>  8) | \
                                   (((pixel) & 0x000000F0) >>  4) )


/* RGB <-> YCbCr conversion */

extern const __u16 y_for_rgb[256];
extern const __s16 cr_for_r[256];
extern const __s16 cr_for_g[256];
extern const __s16 cb_for_g[256];
extern const __s16 cb_for_b[256];

#define YCBCR_TO_RGB( y, cb, cr, r, g, b ) do { \
     __u16 _y, _cb, _cr;\
     __s16 _r, _g, _b;\
     _y  = y_for_rgb[(y)]; _cb = (cb); _cr = (cr);\
     _r  = _y + cr_for_r[_cr]; \
     _g  = _y + cr_for_g[_cr] + cb_for_g[_cb]; \
     _b  = _y + cb_for_b[_cb]; \
     (r) = CLAMP( _r, 0, 255 ); \
     (g) = CLAMP( _g, 0, 255 ); \
     (b) = CLAMP( _b, 0, 255 ); \
} while (0)

extern const __u16 y_from_ey[256];
extern const __u16 cb_from_bey[512];
extern const __u16 cr_from_rey[512];

#define RGB_TO_YCBCR( r, g, b, y, cb, cr ) do { \
     __u32 _ey, _r, _g, _b;\
     _r = (r); _g = (g); _b = (b);\
     _ey = (19595 * _r + 38469 * _g + 7471 * _b) >> 16;\
     (y)  = y_from_ey[_ey]; \
     (cb) = cb_from_bey[_b-_ey+255]; \
     (cr) = cr_from_rey[_r-_ey+255]; \
} while (0)



DFBSurfacePixelFormat dfb_pixelformat_for_depth( int depth );

__u32 dfb_color_to_pixel( DFBSurfacePixelFormat format,
                          __u8 r, __u8 g, __u8 b );

const char *dfb_pixelformat_name( DFBSurfacePixelFormat format );

static inline __u32
dfb_color_to_argb( const DFBColor *color )
{
     return (color->a << 24) | (color->r << 16) | (color->g << 8) | color->b;
}

static inline __u32
dfb_color_to_aycbcr( const DFBColor *color )
{
     unsigned int red   = color->r;
     unsigned int green = color->g;
     unsigned int blue  = color->b;

     __u8 y  = (__u8)(((66 * red + 129 * green + 25 * blue) / 256) + 16);

     __u8 cb = (__u8)((128 * 256 -  38 * red   - 74 * green + 112 * blue) / 256);
     __u8 cr = (__u8)((128 * 256 + 112 * red   - 94 * green -  18 * blue) / 256);

     return (color->a << 24) | (y << 16) | (cb << 8) | cr;
}

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
dfb_argb_to_argb2554( __u32 *src, __u16 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register __u32 argb = src[i];

          dst[i] = ARGB_TO_ARGB2554( argb );
     }
}

static inline void
dfb_argb_to_argb4444( __u32 *src, __u16 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register __u32 argb = src[i];

          dst[i] = ARGB_TO_ARGB4444( argb );
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
