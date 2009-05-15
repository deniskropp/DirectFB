/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#include <config.h>

#include <directfb.h>
#include <directfb_util.h>

#include "convert.h"

/* lookup tables for 2/3bit to 8bit color conversion */
static const u8 lookup3to8[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff};
static const u8 lookup2to8[] = { 0x00, 0x55, 0xaa, 0xff};

#define EXPAND_1to8(v)   ((v) ? 0xff : 0x00)
#define EXPAND_2to8(v)   (lookup2to8[v])
#define EXPAND_3to8(v)   (lookup3to8[v])
#define EXPAND_4to8(v)   (((v) << 4) | ((v)     ))
#define EXPAND_5to8(v)   (((v) << 3) | ((v) >> 2))
#define EXPAND_6to8(v)   (((v) << 2) | ((v) >> 4))
#define EXPAND_7to8(v)   (((v) << 1) | ((v) >> 6))


DFBSurfacePixelFormat
dfb_pixelformat_for_depth( int depth )
{
     switch (depth) {
          case 2:
               return DSPF_LUT2;
          case 8:
               return DSPF_LUT8;
          case 12:
               return DSPF_ARGB4444;
          case 14:
               return DSPF_ARGB2554;
          case 15:
               return DSPF_ARGB1555;
          case 16:
               return DSPF_RGB16;
          case 18:
               return DSPF_RGB18;
          case 24:
               return DSPF_RGB24;
          case 32:
               return DSPF_RGB32;
     }

     return DSPF_UNKNOWN;
}

void
dfb_pixel_to_color( DFBSurfacePixelFormat  format,
                    unsigned long          pixel,
                    DFBColor              *ret_color )
{
     ret_color->a = 0xff;

     switch (format) {
          case DSPF_RGB332:
               ret_color->r = EXPAND_3to8( (pixel & 0xe0) >> 5 );
               ret_color->g = EXPAND_3to8( (pixel & 0x1c) >> 2 );
               ret_color->b = EXPAND_2to8( (pixel & 0x03)      );
               break;

          case DSPF_ARGB1555:
               ret_color->a = EXPAND_1to8(  pixel >> 15 );
          case DSPF_RGB555:
               ret_color->r = EXPAND_5to8( (pixel & 0x7c00) >> 10 );
               ret_color->g = EXPAND_5to8( (pixel & 0x03e0) >>  5 );
               ret_color->b = EXPAND_5to8( (pixel & 0x001f)       );
               break;

          case DSPF_BGR555:
               ret_color->r = EXPAND_5to8( (pixel & 0x001f)       );
               ret_color->g = EXPAND_5to8( (pixel & 0x03e0) >>  5 );
               ret_color->b = EXPAND_5to8( (pixel & 0x7c00) >> 10 );
               break;

          case DSPF_ARGB2554:
               ret_color->a = EXPAND_2to8(  pixel >> 14 );
               ret_color->r = EXPAND_5to8( (pixel & 0x3e00) >>  9 );
               ret_color->g = EXPAND_5to8( (pixel & 0x01f0) >>  4 );
               ret_color->b = EXPAND_4to8( (pixel & 0x000f)       );
               break;

          case DSPF_ARGB4444:
               ret_color->a = EXPAND_4to8(  pixel >> 12 );
          case DSPF_RGB444:
               ret_color->r = EXPAND_4to8( (pixel & 0x0f00) >>  8 );
               ret_color->g = EXPAND_4to8( (pixel & 0x00f0) >>  4 );
               ret_color->b = EXPAND_4to8( (pixel & 0x000f)       );
               break;

          case DSPF_RGBA4444:
               ret_color->r = EXPAND_4to8( (pixel         ) >> 12 );
               ret_color->g = EXPAND_4to8( (pixel & 0x0f00) >>  8 );
               ret_color->b = EXPAND_4to8( (pixel & 0x00f0) >>  4 );
               ret_color->a = EXPAND_4to8( (pixel & 0x000f)       );
               break;

          case DSPF_RGB16:
               ret_color->r = EXPAND_5to8( (pixel & 0xf800) >> 11 );
               ret_color->g = EXPAND_6to8( (pixel & 0x07e0) >>  5 );
               ret_color->b = EXPAND_5to8( (pixel & 0x001f)       );
               break;

          case DSPF_ARGB:
               ret_color->a = pixel >> 24;
          case DSPF_RGB24:
          case DSPF_RGB32:
               ret_color->r = (pixel & 0xff0000) >> 16;
               ret_color->g = (pixel & 0x00ff00) >>  8;
               ret_color->b = (pixel & 0x0000ff);
               break;

          case DSPF_AiRGB:
               ret_color->a = (pixel >> 24) ^ 0xff;
               ret_color->r = (pixel & 0xff0000) >> 16;
               ret_color->g = (pixel & 0x00ff00) >>  8;
               ret_color->b = (pixel & 0x0000ff);
               break;

          default:
               ret_color->r = 0;
               ret_color->g = 0;
               ret_color->b = 0;
     }
}

unsigned long
dfb_pixel_from_color( DFBSurfacePixelFormat  format,
                      const DFBColor        *color )
{
     u32 y, cb, cr;

     switch (format) {
          case DSPF_RGB332:
               return PIXEL_RGB332( color->r, color->g, color->b );

          case DSPF_ARGB1555:
               return PIXEL_ARGB1555( color->a, color->r, color->g, color->b );

          case DSPF_RGB555:
               return PIXEL_RGB555( color->r, color->g, color->b );

          case DSPF_BGR555:
               return PIXEL_BGR555( color->r, color->g, color->b );

          case DSPF_ARGB2554:
               return PIXEL_ARGB2554( color->a, color->r, color->g, color->b );

          case DSPF_ARGB4444:
               return PIXEL_ARGB4444( color->a, color->r, color->g, color->b );

          case DSPF_RGBA4444:
               return PIXEL_RGBA4444( color->a, color->r, color->g, color->b );

          case DSPF_RGB444:
               return PIXEL_RGB444( color->r, color->g, color->b );

          case DSPF_RGB16:
               return PIXEL_RGB16( color->r, color->g, color->b );

          case DSPF_RGB18:
               return PIXEL_RGB18( color->r, color->g, color->b );

          case DSPF_ARGB1666:
               return PIXEL_ARGB1666( color->a, color->r, color->g, color->b );

          case DSPF_ARGB6666:
               return PIXEL_ARGB6666( color->a, color->r, color->g, color->b );

          case DSPF_RGB24:
               return PIXEL_RGB32( color->r, color->g, color->b );

          case DSPF_RGB32:
               return PIXEL_RGB32( color->r, color->g, color->b );

          case DSPF_ARGB:
               return PIXEL_ARGB( color->a, color->r, color->g, color->b );

          case DSPF_AiRGB:
               return PIXEL_AiRGB( color->a, color->r, color->g, color->b );

          case DSPF_AYUV:
               RGB_TO_YCBCR( color->r, color->g, color->b, y, cb, cr );
               return PIXEL_AYUV( color->a, y, cb, cr );

          case DSPF_YUY2:
               RGB_TO_YCBCR( color->r, color->g, color->b, y, cb, cr );
               return PIXEL_YUY2( y, cb, cr );

          case DSPF_UYVY:
               RGB_TO_YCBCR( color->r, color->g, color->b, y, cb, cr );
               return PIXEL_UYVY( y, cb, cr );

          case DSPF_I420:
          case DSPF_YV12:
               RGB_TO_YCBCR( color->r, color->g, color->b, y, cb, cr );
               return y | (cb << 8) | (cr << 16);

          default:
               D_WARN( "unknown format 0x%08x", format );
     }

     return 0x55555555;
}

const char *
dfb_pixelformat_name( DFBSurfacePixelFormat format )
{
     int i = 0;

     do {
          if (format == dfb_pixelformat_names[i].format)
               return dfb_pixelformat_names[i].name;
     } while (dfb_pixelformat_names[i++].format != DSPF_UNKNOWN);

     return "<invalid>";
}

void
dfb_convert_to_rgb16( DFBSurfacePixelFormat  format,
                      const void            *src,
                      int                    spitch,
                      int                    surface_height,
                      u16                   *dst,
                      int                    dpitch,
                      int                    width,
                      int                    height )
{
     const int dp2 = dpitch / 2;
     int       x;

     switch (format) {
          case DSPF_RGB16:
               while (height--) {
                    direct_memcpy( dst, src, width * 2 );

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_NV16:
               while (height--) {
                    const u8  *src8  = src;
                    const u16 *src16 = src + surface_height * spitch;
     
                    for (x=0; x<width; x++) {
                         int r, g, b;

#ifdef WORDS_BIGENDIAN
                         YCBCR_TO_RGB( src8[x], src16[x>>1] >> 8, src16[x>>1] & 0xff, r, g, b );
#else
                         YCBCR_TO_RGB( src8[x], src16[x>>1] & 0xff, src16[x>>1] >> 8, r, g, b );
#endif

                         dst[x] = PIXEL_RGB16( r, g, b );
                    }

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_RGB444:
          case DSPF_ARGB4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_RGB16( EXPAND_4to8( (src16[x] & 0x0f00) >> 8 ),
                                               EXPAND_4to8( (src16[x] & 0x00f0) >> 4 ),
                                               EXPAND_4to8( (src16[x] & 0x000f)      ) );

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_RGBA4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_RGB16( EXPAND_4to8( (src16[x] & 0xf000) >> 12 ),
                                               EXPAND_4to8( (src16[x] & 0x0f00) >>  8 ),
                                               EXPAND_4to8( (src16[x] & 0x00f0) >>  4 ) );

                    src += spitch;
                    dst += dp2;
               }
               break;
               
          case DSPF_RGB555:
          case DSPF_ARGB1555:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = ((src16[x] & 0x7c00) << 1) | ((src16[x] & 0x03e0) << 1) | (src16[x] & 0x003f);

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_BGR555:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = ((src16[x] & 0x7c00) >> 10) | ((src16[x] & 0x03e0) << 1) | ((src16[x] & 0x001f) << 11 );

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_RGB32:
          case DSPF_ARGB:
               while (height--) {
                    const u32 *src32 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_RGB16( (src32[x] & 0xff0000) >> 16,
                                               (src32[x] & 0x00ff00) >>  8,
                                               (src32[x] & 0x0000ff) );

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_AYUV:
               while (height--) {
                    const u32 *src32 = src;

                    for (x=0; x<width; x++) {
                         int r, g, b;

                         YCBCR_TO_RGB( (src32[x] >> 16) & 0xff, (src32[x] >> 8) & 0xff, src32[x] & 0xff, r, g, b );

                         dst[x] = PIXEL_RGB16( r, g, b );
                    }

                    src += spitch;
                    dst += dp2;
               }
               break;

          default:
               D_ONCE( "unsupported format" );
     }
}

void
dfb_convert_to_rgb555( DFBSurfacePixelFormat  format,
                       const void            *src,
                       int                    spitch,
                       int                    surface_height,
                       u16                   *dst,
                       int                    dpitch,
                       int                    width,
                       int                    height )
{
     const int dp2 = dpitch / 2;
     int       x;

     switch (format) {
          case DSPF_RGB555:
          case DSPF_ARGB1555:
               while (height--) {
                    direct_memcpy( dst, src, width * 2 );

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_NV16:
               while (height--) {
                    const u8  *src8  = src;
                    const u16 *src16 = src + surface_height * spitch;
     
                    for (x=0; x<width; x++) {
                         int r, g, b;
     
#ifdef WORDS_BIGENDIAN
                         YCBCR_TO_RGB( src8[x], src16[x>>1] >> 8, src16[x>>1] & 0xff, r, g, b );
#else
                         YCBCR_TO_RGB( src8[x], src16[x>>1] & 0xff, src16[x>>1] >> 8, r, g, b );
#endif
     
                         dst[x] = PIXEL_RGB555( r, g, b );
                    }

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_RGB444:
          case DSPF_ARGB4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_RGB555( EXPAND_4to8( (src16[x] & 0x0f00) >> 8 ),
                                                EXPAND_4to8( (src16[x] & 0x00f0) >> 4 ),
                                                EXPAND_4to8( (src16[x] & 0x000f)      ) );

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_RGBA4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_RGB555( EXPAND_4to8( (src16[x] & 0xf000) >> 12 ),
                                                EXPAND_4to8( (src16[x] & 0x0f00) >>  8 ),
                                                EXPAND_4to8( (src16[x] & 0x00f0) >>  4 ) );

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_RGB16:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = ((src16[x] & 0xffc0) >> 1) | (src16[x] & 0x001f);

                    src += spitch;
                    dst += dp2;
               }
               break;

 	  case DSPF_BGR555:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = ((src16[x] & 0x7c00) >> 10) | (src16[x] & 0x03e0) | ((src16[x] & 0x001f) << 10 );

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_RGB32:
          case DSPF_ARGB:
               while (height--) {
                    const u32 *src32 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_RGB555( (src32[x] & 0xff0000) >> 16,
                                                (src32[x] & 0x00ff00) >>  8,
                                                (src32[x] & 0x0000ff) );

                    src += spitch;
                    dst += dp2;
               }
               break;

          case DSPF_AYUV:
               while (height--) {
                    const u32 *src32 = src;

                    for (x=0; x<width; x++) {
                         int r, g, b;

                         YCBCR_TO_RGB( (src32[x] >> 16) & 0xff, (src32[x] >> 8) & 0xff, src32[x] & 0xff, r, g, b );

                         dst[x] = PIXEL_RGB555( r, g, b );
                    }

                    src += spitch;
                    dst += dp2;
               }
               break;

          default:
               D_ONCE( "unsupported format" );
     }
}

void
dfb_convert_to_rgb32( DFBSurfacePixelFormat  format,
                      const void            *src,
                      int                    spitch,
                      int                    surface_height,
                      u32                   *dst,
                      int                    dpitch,
                      int                    width,
                      int                    height )
{
     const int dp4 = dpitch / 4;
     int       x;

     switch (format) {
          case DSPF_RGB32:
          case DSPF_ARGB:
               while (height--) {
                    direct_memcpy( dst, src, width * 4 );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGB24:
               while (height--) {
                    const u8 *src8 = src;

                    for (x=0; x<width; x++)
#ifdef WORDS_BIGENDIAN
                         dst[x] = ( src8[x*3+0] << 16 ) |
                                  ( src8[x*3+1] <<  8 ) |
                                  ( src8[x*3+2]       );
#else
                         dst[x] = ( src8[x*3+2] << 16 ) |
                                  ( src8[x*3+1] <<  8 ) |
                                  ( src8[x*3+0]       );
#endif

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_AYUV:
               while (height--) {
                    const u32 *src32 = src;

                    for (x=0; x<width; x++) {
                         int r, g, b;

                         YCBCR_TO_RGB( (src32[x] >> 16) & 0xff, (src32[x] >> 8) & 0xff, src32[x] & 0xff, r, g, b );

                         dst[x] = PIXEL_RGB32( r, g, b );
                    }

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_NV16:
               while (height--) {
                    const u8  *src8  = src;
                    const u16 *src16 = src + surface_height * spitch;
     
                    for (x=0; x<width; x++) {
                         int r, g, b;
     
#ifdef WORDS_BIGENDIAN
                         YCBCR_TO_RGB( src8[x], src16[x>>1] >> 8, src16[x>>1] & 0xff, r, g, b );
#else
                         YCBCR_TO_RGB( src8[x], src16[x>>1] & 0xff, src16[x>>1] >> 8, r, g, b );
#endif
     
                         dst[x] = PIXEL_RGB32( r, g, b );
                    }

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGB444:
          case DSPF_ARGB4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = ARGB4444_TO_RGB32( src16[x] );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGBA4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = RGBA4444_TO_RGB32( src16[x] );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGB555:
          case DSPF_ARGB1555:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_RGB32( ((src16[x] & 0x7c00) >> 7) | ((src16[x] & 0x7000) >> 12),
                                               ((src16[x] & 0x03e0) >> 2) | ((src16[x] & 0x0380) >> 7),
                                               ((src16[x] & 0x001f) << 3) | ((src16[x] & 0x001c) >> 2) );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_BGR555:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_RGB32( ((src16[x] & 0x001f) << 3) | ((src16[x] & 0x001c) >> 2),
                                               ((src16[x] & 0x03e0) >> 2) | ((src16[x] & 0x0380) >> 7),
                                               ((src16[x] & 0x7c00) >> 7) | ((src16[x] & 0x7000) >> 12) );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGB16:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_RGB32( ((src16[x] & 0xf800) >> 8) | ((src16[x] & 0xe000) >> 13),
                                               ((src16[x] & 0x07e0) >> 3) | ((src16[x] & 0x0300) >> 8),
                                               ((src16[x] & 0x001f) << 3) | ((src16[x] & 0x001c) >> 2) );

                    src += spitch;
                    dst += dp4;
               }
               break;

          default:
               D_ONCE( "unsupported format" );
     }
}

void
dfb_convert_to_argb( DFBSurfacePixelFormat  format,
                     const void            *src,
                     int                    spitch,
                     int                    surface_height,
                     u32                   *dst,
                     int                    dpitch,
                     int                    width,
                     int                    height )
{
     const int dp4 = dpitch / 4;
     int       x;

     switch (format) {
          case DSPF_ARGB:
               while (height--) {
                    direct_memcpy( dst, src, width * 4 );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGB32:
               while (height--) {
                    const u32 *src32 = src;

                    for (x=0; x<width; x++)
                         dst[x] = src32[x] | 0xff000000;

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGB24:
               while (height--) {
                    const u8 *src8 = src;

                    for (x=0; x<width; x++)
#ifdef WORDS_BIGENDIAN
                         dst[x] = ( src8[x*3+0] << 16 ) |
                                  ( src8[x*3+1] <<  8 ) |
                                  ( src8[x*3+2]       ) | 0xff000000;
#else
                         dst[x] = ( src8[x*3+2] << 16 ) |
                                  ( src8[x*3+1] <<  8 ) |
                                  ( src8[x*3+0]       ) | 0xff000000;
#endif

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_AYUV:
               while (height--) {
                    const u32 *src32 = src;
     
                    for (x=0; x<width; x++) {
                         int r, g, b;
     
                         YCBCR_TO_RGB( (src32[x] >> 16) & 0xff, (src32[x] >> 8) & 0xff, src32[x] & 0xff, r, g, b );
     
                         dst[x] = PIXEL_ARGB( src32[x] >> 24, r, g, b );
                    }

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_NV16:
               while (height--) {
                    const u8  *src8  = src;
                    const u16 *src16 = src + surface_height * spitch;

                    for (x=0; x<width; x++) {
                         int r, g, b;

#ifdef WORDS_BIGENDIAN
                         YCBCR_TO_RGB( src8[x], src16[x>>1] >> 8, src16[x>>1] & 0xff, r, g, b );
#else
                         YCBCR_TO_RGB( src8[x], src16[x>>1] & 0xff, src16[x>>1] >> 8, r, g, b );
#endif

                         dst[x] = PIXEL_ARGB( 0xff, r, g, b );
                    }

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_ARGB4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = ARGB4444_TO_ARGB( src16[x] );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGBA4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = RGBA4444_TO_ARGB( src16[x] );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGB444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_ARGB( 0xff,
                                              ((src16[x] & 0x0f00) >> 4) | ((src16[x] & 0x0f00) >> 8),
                                              ((src16[x] & 0x00f0)     ) | ((src16[x] & 0x00f0) >> 4),
                                              ((src16[x] & 0x000f) << 4) | ((src16[x] & 0x000f)     ) );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_ARGB1555:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_ARGB( (src16[x] & 0x8000) ? 0xff : 0x00,
                                              ((src16[x] & 0x7c00) >> 7) | ((src16[x] & 0x7000) >> 12),
                                              ((src16[x] & 0x03e0) >> 2) | ((src16[x] & 0x0380) >> 7),
                                              ((src16[x] & 0x001f) << 3) | ((src16[x] & 0x001c) >> 2) );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGB555:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_ARGB( 0xff,
                                              ((src16[x] & 0x7c00) >> 7) | ((src16[x] & 0x7000) >> 12),
                                              ((src16[x] & 0x03e0) >> 2) | ((src16[x] & 0x0380) >> 7),
                                              ((src16[x] & 0x001f) << 3) | ((src16[x] & 0x001c) >> 2) );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_BGR555:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_ARGB( 0xff,
                                              ((src16[x] & 0x001f) << 3) | ((src16[x] & 0x001c) >> 2),
                                              ((src16[x] & 0x03e0) >> 2) | ((src16[x] & 0x0380) >> 7),
                                              ((src16[x] & 0x7c00) >> 7) | ((src16[x] & 0x7000) >> 12) );

                    src += spitch;
                    dst += dp4;
               }
               break;

          case DSPF_RGB16:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0; x<width; x++)
                         dst[x] = PIXEL_ARGB( 0xff,
                                              ((src16[x] & 0xf800) >> 8) | ((src16[x] & 0xe000) >> 13),
                                              ((src16[x] & 0x07e0) >> 3) | ((src16[x] & 0x0300) >> 8),
                                              ((src16[x] & 0x001f) << 3) | ((src16[x] & 0x001c) >> 2) );

                    src += spitch;
                    dst += dp4;
               }
               break;

          default:
               D_ONCE( "unsupported format" );
     }
}

void
dfb_convert_to_rgb24( DFBSurfacePixelFormat  format,
                      const void            *src,
                      int                    spitch,
                      int                    surface_height,
                      u8                    *dst,
                      int                    dpitch,
                      int                    width,
                      int                    height )
{
     int n, n3;

     switch (format) {
          case DSPF_A8:
               while (height--) {
                    const u8 *src8 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = src8[n];
                         dst[n3+1] = src8[n];
                         dst[n3+2] = src8[n];
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_AiRGB:
               while (height--) {
                    const u32 *src32 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = (src32[n] & 0xFF0000) >> 16;
                         dst[n3+1] = (src32[n] & 0x00FF00) >>  8;
                         dst[n3+2] = (src32[n] & 0x0000FF);
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_ARGB:
               while (height--) {
                    const u32 *src32 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = (src32[n] & 0xFF0000) >> 16;
                         dst[n3+1] = (src32[n] & 0x00FF00) >>  8;
                         dst[n3+2] = (src32[n] & 0x0000FF);
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_ARGB1555:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = (src16[n] & 0x7C00) >> 7;
                         dst[n3+1] = (src16[n] & 0x03E0) >> 2;
                         dst[n3+2] = (src16[n] & 0x001F) << 3;
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_RGB555:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = (src16[n] & 0x7C00) >> 7;
                         dst[n3+1] = (src16[n] & 0x03E0) >> 2;
                         dst[n3+2] = (src16[n] & 0x001F) << 3;
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;

          case DSPF_BGR555:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+2] = (src16[n] & 0x7C00) >> 7;
                         dst[n3+1] = (src16[n] & 0x03E0) >> 2;
                         dst[n3+0] = (src16[n] & 0x001F) << 3;
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;

          case DSPF_ARGB2554:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = (src16[n] & 0x3E00) >> 6;
                         dst[n3+1] = (src16[n] & 0x01F0) >> 1;
                         dst[n3+2] = (src16[n] & 0x000F) << 4;
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_ARGB4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = (src16[n] & 0x0F00) >> 4;
                         dst[n3+1] = (src16[n] & 0x00F0);
                         dst[n3+2] = (src16[n] & 0x000F) << 4;
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_RGBA4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = (src16[n] & 0xF000) >> 8;
                         dst[n3+1] = (src16[n] & 0x0F00) >> 4;
                         dst[n3+2] = (src16[n] & 0x00F0);
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_RGB444:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = (src16[n] & 0x0F00) >> 4;
                         dst[n3+1] = (src16[n] & 0x00F0);
                         dst[n3+2] = (src16[n] & 0x000F) << 4;
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_RGB332:
               while (height--) {
                    const u8 *src8 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = lookup3to8[ (src8[n] >> 5)        ];
                         dst[n3+1] = lookup3to8[ (src8[n] >> 2) & 0x07 ];
                         dst[n3+2] = lookup2to8[ (src8[n]     ) & 0x03 ];
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_RGB16:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = (src16[n] & 0xF800) >> 8;
                         dst[n3+1] = (src16[n] & 0x07E0) >> 3;
                         dst[n3+2] = (src16[n] & 0x001F) << 3;
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_RGB24:
               while (height--) {
                    const u8 *src8 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
#ifdef WORDS_BIGENDIAN
                         dst[n3+0] = src8[n3+0];
                         dst[n3+1] = src8[n3+1];
                         dst[n3+2] = src8[n3+2];
#else
                         dst[n3+0] = src8[n3+2];
                         dst[n3+1] = src8[n3+1];
                         dst[n3+2] = src8[n3+0];
#endif
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_RGB32:
               while (height--) {
                    const u32 *src32 = src;

                    for (n=0, n3=0; n<width; n++, n3+=3) {
                         dst[n3+0] = (src32[n] & 0xFF0000) >> 16;
                         dst[n3+1] = (src32[n] & 0x00FF00) >>  8;
                         dst[n3+2] = (src32[n] & 0x0000FF);
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_YUY2:
               while (height--) {
                    const u32 *src32 = src;

                    for (n=0, n3=0; n<width/2; n++, n3+=6) {
                         register u32 y0, cb, y1, cr;
                         y0 = (src32[n] & 0x000000FF);
                         cb = (src32[n] & 0x0000FF00) >>  8;
                         y1 = (src32[n] & 0x00FF0000) >> 16;
                         cr = (src32[n] & 0xFF000000) >> 24;
                         YCBCR_TO_RGB( y0, cb, cr,
                                       dst[n3+0], dst[n3+1], dst[n3+2] );
                         YCBCR_TO_RGB( y1, cb, cr,
                                       dst[n3+3], dst[n3+4], dst[n3+5] );
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_UYVY:
               while (height--) {
                    const u32 *src32 = src;

                    for (n=0, n3=0; n<width/2; n++, n3+=6) {
                         register u32 y0, cb, y1, cr;
                         cb = (src32[n] & 0x000000FF);
                         y0 = (src32[n] & 0x0000FF00) >>  8;
                         cr = (src32[n] & 0x00FF0000) >> 16;
                         y1 = (src32[n] & 0xFF000000) >> 24;
                         YCBCR_TO_RGB( y0, cb, cr,
                                       dst[n3+0], dst[n3+1], dst[n3+2] );
                         YCBCR_TO_RGB( y1, cb, cr,
                                       dst[n3+3], dst[n3+4], dst[n3+5] );
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_NV16: {
               while (height--) {
                    const u16 *cbcr = src + surface_height * spitch;
                    const u8  *src8 = src;

                    for (n=0, n3=0; n<width/2; n++, n3+=6) {
#ifdef WORDS_BIGENDIAN
                         YCBCR_TO_RGB( src8[n*2+0], cbcr[n] >> 8, cbcr[n] & 0xff,
                                       dst[n3+0], dst[n3+1], dst[n3+2] );
     
                         YCBCR_TO_RGB( src8[n*2+1], cbcr[n] >> 8, cbcr[n] & 0xff,
                                       dst[n3+3], dst[n3+4], dst[n3+5] );
#else
                         YCBCR_TO_RGB( src8[n*2+0], cbcr[n] & 0xff, cbcr[n] >> 8,
                                       dst[n3+0], dst[n3+1], dst[n3+2] );
     
                         YCBCR_TO_RGB( src8[n*2+1], cbcr[n] & 0xff, cbcr[n] >> 8,
                                       dst[n3+3], dst[n3+4], dst[n3+5] );
#endif
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          }
          default:
               D_ONCE( "unsupported format" );
     }
}

void
dfb_convert_to_a8( DFBSurfacePixelFormat  format,
                   const void            *src,
                   int                    spitch,
                   int                    surface_height,
                   u8                    *dst,
                   int                    dpitch,
                   int                    width,
                   int                    height )
{
     int n;

     switch (format) {
          case DSPF_A8:
               while (height--) {
                    const u8 *src8 = src;

                    direct_memcpy( dst, src8, width );

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_AiRGB:
               while (height--) {
                    const u32 *src32 = src;

                    for (n=0; n<width; n++) {
                         dst[n] = ~(src32[n] >> 24);
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_ARGB:
               while (height--) {
                    const u32 *src32 = src;

                    for (n=0; n<width; n++) {
                         dst[n] = src32[n] >> 24;
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_ARGB1555:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0; n<width; n++) {
                         dst[n] = (src16[n] & 0x8000) ? 0xff : 0x00;
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_ARGB2554:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0; n<width; n++) {
                         switch (src16[n] >> 14) {
                              case 0:
                                   dst[n] = 0x00;
                                   break;
                              case 1:
                                   dst[n] = 0x55;
                                   break;
                              case 2:
                                   dst[n] = 0xAA;
                                   break;
                              case 3:
                                   dst[n] = 0xFF;
                                   break;
                         }
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_ARGB4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0; n<width; n++) {
                         dst[n]  = (src16[n] >> 12);
                         dst[n] |= dst[n] << 4;
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;
          case DSPF_RGBA4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (n=0; n<width; n++) {
                         dst[n] = EXPAND_4to8(src16[n] & 0xf);
                    }

                    src += spitch;
                    dst += dpitch;
               }
               break;

          case DSPF_RGB332:
          case DSPF_RGB444:
          case DSPF_RGB555:
          case DSPF_BGR555:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_NV16:
               while (height--) {
                    memset( dst, 0xff, width );

                    dst += dpitch;
               }
               break;
          default:
               D_ONCE( "unsupported format" );
     }
}

void
dfb_convert_to_a4( DFBSurfacePixelFormat  format,
                   const void            *src,
                   int                    spitch,
                   int                    surface_height,
                   u8                    *dst,
                   int                    dpitch,
                   int                    width,
                   int                    height )
{
     const int w2 = width / 2;
     int       x, n;

     D_ASSUME( (width & 1) == 0 );

     switch (format) {
          case DSPF_A8:
               while (height--) {
                    const u8 *src8 = src;

                    for (x=0, n=0; x<w2; x++, n+=2)
                         dst[x] = (src8[n] & 0xf0) | ((src8[n+1] & 0xf0) >> 4);

                    src += spitch;
                    dst += dpitch;
               }
               break;

          case DSPF_ARGB4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0, n=0; x<w2; x++, n+=2)
                         dst[x] = ((src16[n] & 0xf000) >> 8) | (src16[n+1] >> 12);

                    src += spitch;
                    dst += dpitch;
               }
               break;

          case DSPF_RGBA4444:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0, n=0; x<w2; x++, n+=2)
                         dst[x] = ((src16[n] & 0x000f) << 4) | (src16[n+1] & 0x000f);

                    src += spitch;
                    dst += dpitch;
               }
               break;

          case DSPF_ARGB1555:
               while (height--) {
                    const u16 *src16 = src;

                    for (x=0, n=0; x<w2; x++, n+=2)
                         dst[x] = ((src16[n] & 0x8000) ? 0xf0 : 0) | ((src16[n+1] & 0x8000) ? 0x0f : 0);

                    src += spitch;
                    dst += dpitch;
               }
               break;

          case DSPF_ARGB:
               while (height--) {
                    const u32 *src32 = src;

                    for (x=0, n=0; x<w2; x++, n+=2)
                         dst[x] = ((src32[n] & 0xf0000000) >> 24) | ((src32[n+1] & 0xf0000000) >> 28);

                    src += spitch;
                    dst += dpitch;
               }
               break;

          default:
               if (DFB_PIXELFORMAT_HAS_ALPHA( format ))
                    D_ONCE( "unsupported format" );
     }
}

void
dfb_convert_to_yuy2( DFBSurfacePixelFormat  format,
                     const void            *src,
                     int                    spitch,
                     int                    surface_height,
                     u32                   *dst,
                     int                    dpitch,
                     int                    width,
                     int                    height )
{
     const int dp4 = dpitch / 4;

     switch (format) {
          case DSPF_YUY2:
               while (height--) {
                    direct_memcpy( dst, src, width * 2 );

                    src += spitch;
                    dst += dp4;
               }
               break;

          default:
               D_ONCE( "unsupported format" );
     }
}

void
dfb_convert_to_uyvy( DFBSurfacePixelFormat  format,
                     const void            *src,
                     int                    spitch,
                     int                    surface_height,
                     u32                   *dst,
                     int                    dpitch,
                     int                    width,
                     int                    height )
{
     const int dp4 = dpitch / 4;

     switch (format) {
          case DSPF_UYVY:
               while (height--) {
                    direct_memcpy( dst, src, width * 2 );

                    src += spitch;
                    dst += dp4;
               }
               break;

          default:
               D_ONCE( "unsupported format" );
     }
}

