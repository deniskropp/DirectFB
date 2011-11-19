/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <direct/memcpy.h>
#include <direct/util.h>


/* pixel packing */

#define PIXEL_RGB332(r,g,b)    ( (((r)&0xE0)     ) | \
                                 (((g)&0xE0) >> 3) | \
                                 (((b)&0xC0) >> 6) )

#define PIXEL_ARGB1555(a,r,g,b)( (((a)&0x80) << 8) | \
                                 (((r)&0xF8) << 7) | \
                                 (((g)&0xF8) << 2) | \
                                 (((b)&0xF8) >> 3) )

#define PIXEL_RGBA5551(a,r,g,b)( (((a)&0x80) >> 7) | \
                                 (((r)&0xF8) << 8) | \
                                 (((g)&0xF8) << 3) | \
                                 (((b)&0xF8) >> 2) )

#define PIXEL_RGB555(r,g,b)  ( (((r)&0xF8) << 7) | \
                                 (((g)&0xF8) << 2) | \
                                 (((b)&0xF8) >> 3) )

#define PIXEL_BGR555(r,g,b)  ( (((b)&0xF8) << 7) | \
                                 (((g)&0xF8) << 2) | \
                                 (((r)&0xF8) >> 3) )

#define PIXEL_ARGB2554(a,r,g,b)( (((a)&0xC0) << 8) | \
                                 (((r)&0xF8) << 6) | \
                                 (((g)&0xF8) << 1) | \
                                 (((b)&0xF0) >> 4) )

#define PIXEL_ARGB4444(a,r,g,b)( (((a)&0xF0) << 8) | \
                                 (((r)&0xF0) << 4) | \
                                 (((g)&0xF0)     ) | \
                                 (((b)&0xF0) >> 4) )

#define PIXEL_RGBA4444(a,r,g,b)( (((r)&0xF0) << 8) | \
                                 (((g)&0xF0) << 4) | \
                                 (((b)&0xF0)     ) | \
                                 (((a)&0xF0) >> 4) )

#define PIXEL_RGB444(r,g,b)  ( (((r)&0xF0) << 4) | \
                                 (((g)&0xF0)     ) | \
                                 (((b)&0xF0) >> 4) )

#define PIXEL_RGB16(r,g,b)     ( (((r)&0xF8) << 8) | \
                                 (((g)&0xFC) << 3) | \
                                 (((b)&0xF8) >> 3) )

#define PIXEL_RGB18(r,g,b)     ( (((r)&0xFC) << 10) | \
                                 (((g)&0xFC) <<  4) | \
                                 (((b)&0xFC) >>  2) )

#define PIXEL_RGB32(r,g,b)     ( (0xff << 24) | \
                                 ((r)  << 16) | \
                                 ((g)  <<  8) | \
                                  (b) )

#define PIXEL_ARGB(a,r,g,b)    ( ((a) << 24) | \
                                 ((r) << 16) | \
                                 ((g) << 8)  | \
                                  (b) )

#define PIXEL_ABGR(a,r,g,b)    ( ((a) << 24) | \
                                 ((b) << 16) | \
                                 ((g) << 8)  | \
                                  (r) )

#define PIXEL_ARGB8565(a,r,g,b)( ((a) << 16) | \
                                 PIXEL_RGB16 (r, g, b) )

#define PIXEL_ARGB1666(a,r,g,b) ( (((a)&0x80) << 11) | \
                                  (((r)&0xFC) << 10) | \
                                  (((g)&0xFC) <<  4) | \
                                  (((b)&0xFC) >>  2) )

#define PIXEL_ARGB6666(a,r,g,b) ( (((a)&0xFC) << 16) | \
                                  (((r)&0xFC) << 10) | \
                                  (((g)&0xFC) <<  4) | \
                                  (((b)&0xFC) >>  2) )

#define PIXEL_AYUV(a,y,u,v)    ( ((a) << 24) | \
                                 ((y) << 16) | \
                                 ((u) << 8)  | \
                                  (v) )

#define PIXEL_AVYU(a,y,u,v)    ( ((a) << 24) | \
                                 ((v) << 16) | \
                                 ((y) << 8)  | \
                                  (u) )

#define PIXEL_AiRGB(a,r,g,b)   ( (((a) ^ 0xff) << 24) | \
                                 ((r) << 16) | \
                                 ((g) << 8)  | \
                                  (b) )

#define PIXEL_RGBAF88871(a,r,g,b)  ( ((r) << 24) | \
                                     ((g) << 16) | \
                                     ((b) << 8)  | \
                                     ((a) &~1 ) )

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

#define PIXEL_VYU(y,u,v)       ( ((v) << 16) | \
                                 ((y) << 8)  | \
                                  (u) )


/* packed pixel conversions */

#define ARGB1555_TO_RGB332(pixel) ( (((pixel) & 0x7000) >> 7) | \
                                    (((pixel) & 0x0380) >> 5) | \
                                    (((pixel) & 0x0018) >> 3) )

#define ARGB1555_TO_ARGB2554(pixel) ( (((pixel) & 0x8000)     ) | \
                                      (((pixel) & 0x7FFF) >> 1) )

#define ARGB1555_TO_ARGB4444(pixel) ( (((pixel) & 0x8000) ? 0xf000 : 0 ) | \
                                      (((pixel) & 0x7800) >> 3) | \
                                      (((pixel) & 0x03C0) >> 2) | \
                                      (((pixel) & 0x0018) >> 1) )

#define ARGB1555_TO_RGBA4444(pixel) ( (((pixel) & 0x8000) ? 0x000f : 0 ) | \
                                      (((pixel) & 0x7800) << 1) | \
                                      (((pixel) & 0x03C0) << 2) | \
                                      (((pixel) & 0x0018) << 3) )

#define ARGB1555_TO_RGB16(pixel)  ( (((pixel) & 0x7C00) << 1) | \
                                    (((pixel) & 0x03E0) << 1) | \
                                    (((pixel) & 0x001F)) )

#define ARGB1555_TO_ARGB8565(pixel) ( (((pixel) & 0x8000) ? 0x00FF0000 : 0) | \
                                      ARGB1555_TO_RGB16 (pixel) )

#define ARGB1555_TO_RGB32(pixel)  ( (((pixel) & 0x7C00) << 9) | \
                                    (((pixel) & 0x03E0) << 6) | \
                                    (((pixel) & 0x001F) << 3) )

#define ARGB1555_TO_ARGB(pixel)   ( (((pixel) & 0x8000) ? 0xFF000000 : 0) | \
                                    (((pixel) & 0x7C00) << 9) | \
                                    (((pixel) & 0x03E0) << 6) | \
                                    (((pixel) & 0x001F) << 3) )

#define ARGB1555_TO_RGB555(pixel) ( (((pixel) & 0x7C00) << 9) | \
                                    (((pixel) & 0x03E0) << 6) | \
                                    (((pixel) & 0x001F) << 3) )

#define ARGB1555_TO_RGB444(pixel) ( (((pixel) & 0x7800) >> 3) | \
                                    (((pixel) & 0x03C0) >> 2) | \
                                    (((pixel) & 0x001E) >> 1) )

/* xRGB to xxRRGGBB, so xRxx left 3, xRGx left 2, xxGB left 1, xxxB */
#define ARGB4444_TO_RGB32(pixel)  ( (((pixel) & 0x0F00) << 12) | \
                                    (((pixel) & 0x0FF0) <<  8) | \
                                    (((pixel) & 0x00FF) <<  4) | \
                                    (((pixel) & 0x000F)      ) )

/* RGBx to xxRRGGBB, so Rxxx left 2, RGxx left 1, xGBx, xxBx right 1 */
#define RGBA4444_TO_RGB32(pixel)  ( (((pixel) & 0xF000) <<  8) | \
                                    (((pixel) & 0xFF00) <<  4) | \
                                    (((pixel) & 0x0FF0)      ) | \
                                    (((pixel) & 0x00F0) >>  4) )

/* ARGB to AARRGGBB, so Axxx left 4, ARxx left 3, xRGx left 2, xxGB left 1, xxxB */
#define ARGB4444_TO_ARGB(pixel)  ( (((pixel) & 0xF000) << 16) | \
                                   (((pixel) & 0xFF00) << 12) | \
                                   (((pixel) & 0x0FF0) <<  8) | \
                                   (((pixel) & 0x00FF) <<  4) | \
                                   (((pixel) & 0x000F)      ) )

/* RGBA to AARRGGBB, so Rxxx left 2, RGxx left 1, xGBx, xxBx right 1, A to the left */
#define RGBA4444_TO_ARGB(pixel)  ( (((pixel) & 0x000F) << 28) | \
                                   (((pixel) & 0x000F) << 24) | \
                                   (((pixel) & 0xF000) <<  8) | \
                                   (((pixel) & 0xFF00) <<  4) | \
                                   (((pixel) & 0x0FF0)      ) | \
                                   (((pixel) & 0x00F0) >>  4) )

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
                                    (((pixel) & 0x001E) >> 1) )

#define RGB16_TO_RGBA4444(pixel)  ( 0x000F |                    \
                                    (((pixel) & 0xF000)     ) | \
                                    (((pixel) & 0x0780) << 1) | \
                                    (((pixel) & 0x001E) << 3) )

#define RGB16_TO_ARGB8565(pixel)  ( 0xFF0000 |                  \
                                    ((pixel) & 0xffff) )

#define RGB16_TO_RGB32(pixel)  ( (((pixel) & 0xF800) << 8) | \
                                 (((pixel) & 0x07E0) << 5) | \
                                 (((pixel) & 0x001F) << 3) )

#define RGB16_TO_ARGB(pixel)   ( 0xFF000000 |                \
                                 (((pixel) & 0xF800) << 8) | \
                                 (((pixel) & 0x07E0) << 5) | \
                                 (((pixel) & 0x001F) << 3) )

#define RGB16_TO_RGB555(pixel)  ( (((pixel) & 0xF800) >> 1) | \
                                    (((pixel) & 0x07C0) >> 1) | \
                                    (((pixel) & 0x001F)) )

#define RGB16_TO_BGR555(pixel)  ( (((pixel) & 0xF800) >> 12) | \
                                    (((pixel) & 0x07C0) >> 1) | \
                                    (((pixel) & 0x001F) << 10 ) )

#define RGB16_TO_RGB444(pixel)  ( (((pixel) & 0xF000) >> 4) | \
                                    (((pixel) & 0x0780) >> 3) | \
                                    (((pixel) & 0x001F) >> 1) )


#define ARGB8565_TO_RGB332(pixel)   ( RGB16_TO_RGB332 (pixel) )

#define ARGB8565_TO_ARGB1555(pixel) ( (((pixel) & 0x800000) >> 16) | \
                                      (((pixel) & 0x00F800) >>  1) | \
                                      (((pixel) & 0x0007C0) >>  1) | \
                                      (((pixel) & 0x00001F)) )

#define ARGB8565_TO_ARGB2554(pixel) ( (((pixel) & 0xC00000) >> 16) | \
                                      (((pixel) & 0x00F800) >>  2) | \
                                      (((pixel) & 0x0007C0) >>  2) | \
                                      (((pixel) & 0x00001F) >>  1) )

#define ARGB8565_TO_ARGB4444(pixel) ( (((pixel) & 0xF00000) >> 16) | \
                                      (((pixel) & 0x00F000) >>  4) | \
                                      (((pixel) & 0x000780) >>  3) | \
                                      (((pixel) & 0x00001F) >>  1) )

#define ARGB8565_TO_RGB16(pixel)    ( ((pixel) & 0xffff) )

#define ARGB8565_TO_RGB32(pixel)    ( RGB16_TO_RGB32 (pixel) )

#define ARGB8565_TO_ARGB(pixel)     ( (((pixel) & 0xFF0000) << 8) | \
                                      (((pixel) & 0x00F800) << 8) | \
                                      (((pixel) & 0x0007E0) << 5) | \
                                      (((pixel) & 0x00001F) << 3) )


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

#define RGB32_TO_RGBA4444(pixel)  ( 0x000F |                       \
                                    (((pixel) & 0xF00000) >>  8) | \
                                    (((pixel) & 0x00F000) >>  4) | \
                                    (((pixel) & 0x0000F0)      ) )

#define RGB32_TO_RGB16(pixel)  ( (((pixel) & 0xF80000) >> 8) | \
                                 (((pixel) & 0x00FC00) >> 5) | \
                                 (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_ARGB8565(pixel) ( 0xff0000 | \
                                   RGB32_TO_RGB16 (pixel) )

#define RGB32_TO_ARGB1555(pixel) ( 0x8000 | \
                                   (((pixel) & 0xF80000) >> 9) | \
                                   (((pixel) & 0x00F800) >> 6) | \
                                   (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_ARGB(pixel)   ( 0xFF000000 | (pixel) )

#define ARGB_TO_ARGB8565(pixel)  ( (((pixel) & 0xFF000000) >>  8) | \
                                   (((pixel) & 0x00F80000) >>  8) | \
                                   (((pixel) & 0x0000FC00) >>  5) | \
                                   (((pixel) & 0x000000F8) >>  3) )


#define RGB32_TO_RGB555(pixel)  ( (((pixel) & 0xF80000) >> 9) | \
                                    (((pixel) & 0x00F800) >> 6) | \
                                    (((pixel) & 0x0000F8) >> 3) )

#define RGB32_TO_BGR555(pixel)  ( (((pixel) & 0xF80000) >> 19) | \
                                    (((pixel) & 0x00F800) >> 6) | \
                                    (((pixel) & 0x0000F8) << 7) )

#define RGB32_TO_RGB444(pixel)  ( (((pixel) & 0xF00000) >> 12) | \
                                    (((pixel) & 0x00F000) >>  8) | \
                                    (((pixel) & 0x0000F0) >>  4) )

#define ARGB_TO_ARGB1555(pixel)  ( (((pixel) & 0x80000000) >> 16) | \
                                   (((pixel) & 0x00F80000) >>  9) | \
                                   (((pixel) & 0x0000F800) >>  6) | \
                                   (((pixel) & 0x000000F8) >>  3) )

#define ARGB_TO_RGBA5551(pixel)  ( (((pixel) & 0x80000000) >> 31) | \
                                   (((pixel) & 0x00F80000) >>  8) | \
                                   (((pixel) & 0x0000F800) >>  5) | \
                                   (((pixel) & 0x000000F8) >>  2) )

#define ARGB_TO_ARGB2554(pixel)  ( (((pixel) & 0xC0000000) >> 16) | \
                                   (((pixel) & 0x00F80000) >> 10) | \
                                   (((pixel) & 0x0000F800) >>  7) | \
                                   (((pixel) & 0x000000F0) >>  4) )

#define ARGB_TO_ARGB4444(pixel)  ( (((pixel) & 0xF0000000) >> 16) | \
                                   (((pixel) & 0x00F00000) >> 12) | \
                                   (((pixel) & 0x0000F000) >>  8) | \
                                   (((pixel) & 0x000000F0) >>  4) )

#define ARGB_TO_RGBA4444(pixel)  ( (((pixel) & 0xF0000000) >> 28) | \
                                   (((pixel) & 0x00F00000) >>  8) | \
                                   (((pixel) & 0x0000F000) >>  4) | \
                                   (((pixel) & 0x000000F0)      ) )

#define ARGB_TO_RGB444(pixel)  ( (((pixel) & 0x00F00000) >> 12) | \
                                   (((pixel) & 0x0000F000) >>  8) | \
                                   (((pixel) & 0x000000F0) >>  4) )

#define ARGB_TO_RGB555(pixel)  ( (((pixel) & 0x00F80000) >>  9) | \
                                   (((pixel) & 0x0000F800) >>  6) | \
                                   (((pixel) & 0x000000F8) >>  3) )

#define ARGB_TO_BGR555(pixel)  ( (((pixel) & 0x00F80000) >>  19) | \
                                   (((pixel) & 0x0000F800) >>  6) | \
                                   (((pixel) & 0x000000F8) <<  7) )

#define ARGB_TO_ABGR(pixel)  ( ((pixel) & 0xFF00FF00) | \
                                  (((pixel) & 0x000000FF) << 16) | \
                                  (((pixel) & 0x00FF0000) >> 16) )

#define ARGB_TO_RGBAF88871(pixel) ( (((pixel) & 0x00FFFFFF) << 8 ) | \
                                    (((pixel) & 0xFE000000) >> 24))



/* RGB <-> YCbCr conversion */

#define YCBCR_TO_RGB( y, cb, cr, r, g, b )                            \
do {                                                                  \
     int _y  = (y)  -  16;                                            \
     int _cb = (cb) - 128;                                            \
     int _cr = (cr) - 128;                                            \
                                                                      \
     int _r = (298 * _y             + 409 * _cr + 128) >> 8;          \
     int _g = (298 * _y - 100 * _cb - 208 * _cr + 128) >> 8;          \
     int _b = (298 * _y + 516 * _cb             + 128) >> 8;          \
                                                                      \
     (r) = CLAMP( _r, 0, 255 );                                       \
     (g) = CLAMP( _g, 0, 255 );                                       \
     (b) = CLAMP( _b, 0, 255 );                                       \
} while (0)

#define RGB_TO_YCBCR( r, g, b, y, cb, cr )                            \
do {                                                                  \
     int _r = (r), _g = (g), _b = (b);                                \
                                                                      \
     (y)  = (   66 * _r + 129 * _g +  25 * _b  +  16*256 + 128) >> 8; \
     (cb) = ( - 38 * _r -  74 * _g + 112 * _b  + 128*256 + 128) >> 8; \
     (cr) = (  112 * _r -  94 * _g -  18 * _b  + 128*256 + 128) >> 8; \
} while (0)


void                  dfb_pixel_to_color  ( DFBSurfacePixelFormat  format,
                                            unsigned long          pixel,
                                            DFBColor              *ret_color );

unsigned long         dfb_pixel_from_color( DFBSurfacePixelFormat  format,
                                            const DFBColor        *color );

void
dfb_pixel_to_components( DFBSurfacePixelFormat  format,
                         unsigned long          pixel,
                         u8                     *a,
                         u8                     *c2,    /* Either Y or R */
                         u8                     *c1,    /* Either U or G */
                         u8                     *c0 );  /* Either V or B */


static __inline__ u32
dfb_color_to_pixel( DFBSurfacePixelFormat format,
                    u8 r, u8 g, u8 b )
{
     const DFBColor color = { 0, r, g, b };

     return dfb_pixel_from_color( format, &color );
}

static __inline__ u32
dfb_color_to_argb( const DFBColor *color )
{
     return (color->a << 24) | (color->r << 16) | (color->g << 8) | color->b;
}

static __inline__ u32
dfb_color_to_aycbcr( const DFBColor *color )
{
     u32 y, cb, cr;
     RGB_TO_YCBCR( color->r, color->g, color->b, y, cb, cr );

     return (color->a << 24) | (y << 16) | (cb << 8) | cr;
}

static __inline__ u32
dfb_color_to_acrycb( const DFBColor *color )
{
     u32 y, cb, cr;
     RGB_TO_YCBCR( color->r, color->g, color->b, y, cb, cr );

     return (color->a << 24) | (cr << 16) | (y << 8) | cb;
}

static __inline__ void
dfb_argb_to_rgb332( const u32 *src, u8 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register u32 argb = src[i];

          dst[i] = RGB32_TO_RGB332( argb );
     }
}

static __inline__ void
dfb_argb_to_argb1555( const u32 *src, u16 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register u32 argb = src[i];

          dst[i] = ARGB_TO_ARGB1555( argb );
     }
}

static __inline__ void
dfb_argb_to_rgba5551( const u32 *src, u16 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register u32 argb = src[i];

          dst[i] = ARGB_TO_RGBA5551( argb );
     }
}

static __inline__ void
dfb_argb_to_argb2554( const u32 *src, u16 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register u32 argb = src[i];

          dst[i] = ARGB_TO_ARGB2554( argb );
     }
}

static __inline__ void
dfb_argb_to_argb4444( const u32 *src, u16 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register u32 argb = src[i];

          dst[i] = ARGB_TO_ARGB4444( argb );
     }
}

static __inline__ void
dfb_argb_to_rgba4444( const u32 *src, u16 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register u32 rgba = src[i];

          dst[i] = ARGB_TO_RGBA4444( rgba );
     }
}

static __inline__ void
dfb_argb_to_argb8565( const u32 *src, u8 *dst, int len )
{
     int i = -1, j = -1;

     while (++i < len) {
        register u32 argb = src[i];

        register u32 d = ARGB_TO_ARGB8565( argb );
#ifdef WORDS_BIGENDIAN
        dst[++j] = (d >> 16) & 0xff;
        dst[++j] = (d >>  8) & 0xff;
        dst[++j] = (d >>  0) & 0xff;
#else
        dst[++j] = (d >>  0) & 0xff;
        dst[++j] = (d >>  8) & 0xff;
        dst[++j] = (d >> 16) & 0xff;
#endif
     }
}

static __inline__ void
dfb_argb_to_rgb16( const u32 *src, u16 *dst, int len )
{
     int i;

     for (i=0; i<len; i++) {
          register u32 argb = src[i];

          dst[i] = RGB32_TO_RGB16( argb );
     }
}

static __inline__ void
dfb_argb_to_a8( const u32 *src, u8 *dst, int len )
{
     int i;

     for (i=0; i<len; i++)
          dst[i] = src[i] >> 24;
}

void dfb_convert_to_rgb16( DFBSurfacePixelFormat  format,
                           const void            *src,
                           int                    spitch,
                           int                    surface_height,
                           u16                   *dst,
                           int                    dpitch,
                           int                    width,
                           int                    height );

void dfb_convert_to_rgb555( DFBSurfacePixelFormat  format,
                            const void            *src,
                            int                    spitch,
                            int                    surface_height,
                            u16                   *dst,
                            int                    dpitch,
                            int                    width,
                            int                    height );

void dfb_convert_to_argb( DFBSurfacePixelFormat  format,
                          const void            *src,
                          int                    spitch,
                          int                    surface_height,
                          u32                   *dst,
                          int                    dpitch,
                          int                    width,
                          int                    height );

void dfb_convert_to_rgb32( DFBSurfacePixelFormat  format,
                           const void            *src,
                           int                    spitch,
                           int                    surface_height,
                           u32                   *dst,
                           int                    dpitch,
                           int                    width,
                           int                    height );

void dfb_convert_to_rgb24( DFBSurfacePixelFormat  format,
                           const void            *src,
                           int                    spitch,
                           int                    surface_height,
                           u8                    *dst,
                           int                    dpitch,
                           int                    width,
                           int                    height );

void dfb_convert_to_a8( DFBSurfacePixelFormat  format,
                        const void            *src,
                        int                    spitch,
                        int                    surface_height,
                        u8                    *dst,
                        int                    dpitch,
                        int                    width,
                        int                    height );

void dfb_convert_to_a4( DFBSurfacePixelFormat  format,
                        const void            *src,
                        int                    spitch,
                        int                    surface_height,
                        u8                    *dst,
                        int                    dpitch,
                        int                    width,
                        int                    height );

void dfb_convert_to_yuy2( DFBSurfacePixelFormat  format,
                          const void            *src,
                          int                    spitch,
                          int                    surface_height,
                          u32                   *dst,
                          int                    dpitch,
                          int                    width,
                          int                    height );

void dfb_convert_to_uyvy( DFBSurfacePixelFormat  format,
                          const void            *src,
                          int                    spitch,
                          int                    surface_height,
                          u32                   *dst,
                          int                    dpitch,
                          int                    width,
                          int                    height );

#endif
