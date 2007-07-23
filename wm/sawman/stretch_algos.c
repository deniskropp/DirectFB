/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>

#include <directfb.h>
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <gfx/convert.h>

#include "stretch_algos.h"

#define PIXEL_RGB32TO16(p)      ((((p) >> 8) & 0xf800) | (((p) >> 5) & 0x07e0) | (((p) >> 3) & 0x001f))

/**********************************************************************************************************************/

static void stretch_simple_rgb16( void        *dst,
                                  int          dpitch,
                                  const void  *src,
                                  int          spitch,
                                  int          width,
                                  int          height,
                                  int          dst_width,
                                  int          dst_height,
                                  DFBRegion   *clip,
                                  u16          key )
{
    int  x, y;
    int  head   = ((((unsigned long) dst) & 2) >> 1) ^ (clip->x1 & 1);
    int  cw     = clip->x2 - clip->x1 + 1;
    int  ch     = clip->y2 - clip->y1 + 1;
    int  tail   = (cw - head) & 1;
    int  w2     = (cw - head) / 2;
    int  hfraq  = (width  << 18) / dst_width;
    int  vfraq  = (height << 18) / dst_height;
    int  point0 = clip->x1 * hfraq;
    int  line   = clip->y1 * vfraq;
    int  hfraq2 = hfraq << 1;
    int  point  = point0;
    u32 *dst32;

    dst += clip->x1 * 2 + clip->y1 * dpitch;

    dst32 = dst;

    if (head) {
         u16 *dst16 = dst;

         for (y=0; y<ch; y++) {
              const u16 *src16 = src + spitch * (line >> 18);

#ifdef COLOR_KEY
              if (src16[point>>18] != (COLOR_KEY))
#endif
                   /* Write to destination with color key protection */
                   dst16[0] = (src16[point>>18] == (key)) ? (key^1) : src16[point>>18];

              dst16 += dpitch / 2;
              line  += vfraq;
         }

         /* Adjust */
         point0 += hfraq;
         dst32   = dst + 2;

         /* Reset */
         line = clip->y1 * vfraq;
    }

    for (y=0; y<ch; y++) {
         const u16 *src16 = src + spitch * (line >> 18);

         point = point0;

         for (x=0; x<w2; x++) {
              dst32[x] = ((src16[point>>18] == (key)) ? (key^1) : src16[point>>18]) |
                         (((src16[(point+hfraq)>>18] == (key)) ? (key^1) : src16[(point+hfraq)>>18]) << 16);

              point += hfraq2;
         }

         line  += vfraq;
         dst32 += dpitch/4;
    }

    if (tail) {
         u16 *dst16 = dst + cw * 2 - 2;

         /* Reset */
         line = clip->y1 * vfraq;

         for (y=0; y<ch; y++) {
              const u16 *src16 = src + spitch * (line >> 18);

#ifdef COLOR_KEY
              if (src16[point>>18] != (COLOR_KEY))
#endif
                   /* Write to destination with color key protection */
                   dst16[0] = (src16[point>>18] == (key)) ? (key^1) : src16[point>>18];

              dst16 += dpitch / 2;
              line  += vfraq;
         }
    }
}

static void stretch_simple_rgb16_keyed( void        *dst,
                                        int          dpitch,
                                        const void  *src,
                                        int          spitch,
                                        int          width,
                                        int          height,
                                        int          dst_width,
                                        int          dst_height,
                                        DFBRegion   *clip,
                                        u16          key,
                                        u16          src_key )
{
     int  x, y;
     int  head   = ((((unsigned long) dst) & 2) >> 1) ^ (clip->x1 & 1);
     int  cw     = clip->x2 - clip->x1 + 1;
     int  ch     = clip->y2 - clip->y1 + 1;
     int  tail   = (cw - head) & 1;
     int  w2     = (cw - head) / 2;
     int  hfraq  = (width  << 18) / dst_width;
     int  vfraq  = (height << 18) / dst_height;
     int  point0 = clip->x1 * hfraq;
     int  line   = clip->y1 * vfraq;
     int  hfraq2 = hfraq << 1;
     int  point  = point0;
     u32 *dst32;

     dst += clip->x1 * 2 + clip->y1 * dpitch;

     dst32 = dst;

     if (head) {
          u16 *dst16 = dst;

          for (y=0; y<ch; y++) {
               const u16 *src16 = src + spitch * (line >> 18);

               /* Write to destination with color key protection */
               if (src16[point>>18] != src_key)
                    dst16[0] = (src16[point>>18] == (key)) ? (key^1) : src16[point>>18];

               dst16 += dpitch / 2;
               line  += vfraq;
          }

          /* Adjust */
          point0 += hfraq;
          dst32   = dst + 2;

          /* Reset */
          line = clip->y1 * vfraq;
     }

     for (y=0; y<ch; y++) {
          const u16 *src16 = src + spitch * (line >> 18);

          point = point0;

          for (x=0; x<w2; x++) {
               u32 l = src16[point>>18];
               u32 r = src16[(point+hfraq)>>18];

               /* Write to destination with color key protection */
               if (l != src_key) {
                    if (r != src_key)
                         dst32[x] = (((r == (key)) ? (key^1) : r) << 16) | ((l == (key)) ? (key^1) : l);
                    else
                         *(__u16*)(&dst32[x]) = (l == (key)) ? (key^1) : l;
               }
               else if (r != src_key)
                    *(((__u16*)(&dst32[x]))+1) = (r == (key)) ? (key^1) : r;

               point += hfraq2;
          }

          line  += vfraq;
          dst32 += dpitch/4;
     }

     if (tail) {
          u16 *dst16 = dst + cw * 2 - 2;

          /* Reset */
          line = clip->y1 * vfraq;

          for (y=0; y<ch; y++) {
               const u16 *src16 = src + spitch * (line >> 18);

               /* Write to destination with color key protection */
               if (src16[point>>18] != src_key)
                    dst16[0] = (src16[point>>18] == (key)) ? (key^1) : src16[point>>18];

               dst16 += dpitch / 2;
               line  += vfraq;
          }
     }
}

/**********************************************************************************************************************/

static void stretch_down2_rgb16_from32( void         *dst,
                                        int           dpitch,
                                        const void   *src,
                                        int           spitch,
                                        int           width,
                                        int           height,
                                        int           dst_width,
                                        int           dst_height,
                                        DFBRegion    *clip,
                                        u16           key )
{
     int x, y;
     int w2 = dst_width / 2;
     int hfraq = (width  << 18) / dst_width;
     int vfraq = (height << 18) / dst_height;
     int line  = vfraq * (dst_height - 1);
     int sp4   = spitch / 4;

     for (y=dst_height-1; y>=0; y--) {
          int point = 0;

          __u32 s1;
          __u32 s2;
          __u32 s3;
          __u32 s4;
          __u32 d1;
          __u32 d2;

          int sy = ((line >> 18) > height - 2) ? height - 2 : (line >> 18);

          __u32       *dst32 = dst + dpitch * y;
          const __u16 *src32 = src + spitch * sy;


          for (x=0; x<w2; x++) {
               s1 = src32[point>>18];
               s2 = src32[(point>>18) + 1];
               s3 = src32[(point>>18) + sp4];
               s4 = src32[(point>>18) + sp4 + 1];

               point += hfraq;

               d1 = ((s1 & 0xfcfcfc) + (s2 & 0xfcfcfc) + (s3 & 0xfcfcfc) + (s4 & 0xfcfcfc)) >> 2;


               s1 = src32[point>>18];
               s2 = src32[(point>>18) + 1];
               s3 = src32[(point>>18) + sp4];
               s4 = src32[(point>>18) + sp4 + 1];

               point += hfraq;

               d2 = ((s1 & 0xfcfcfc) + (s2 & 0xfcfcfc) + (s3 & 0xfcfcfc) + (s4 & 0xfcfcfc)) >> 2;


               dst32[x] = PIXEL_RGB32TO16(d1) | (PIXEL_RGB32TO16(d2) << 16);
          }

          line -= vfraq;
     }
}

/**********************************************************************************************************************/

static void stretch_hv4_rgb16_from32( void         *dst,
                                      int           dpitch,
                                      const void   *src,
                                      int           spitch,
                                      int           width,
                                      int           height,
                                      int           dst_width,
                                      int           dst_height,
                                      DFBRegion    *clip,
                                      u16           key )
{
    int x, y;
    int w2 = dst_width / 2;
    int hfraq = (width  << 18) / dst_width;
    int vfraq = (height << 18) / dst_height;
    int line  = vfraq * (dst_height - 1);

    __u32 linecache[dst_width/2];

    for (y=dst_height-1; y>=0; y--) {
         int point = 0;

         __u32 s1;
         __u32 s2;
         __u32 d1 = 0;
         __u32 d2 = 0;
         __u32 dp;

         __u32       *dst32 = dst + dpitch * y;
         const __u32 *src32 = src + spitch * (line >> 18);


         for (x=0; x<w2; x++) {
              s1 = src32[point>>18];
              s2 = src32[(point>>18) + 1];

              switch ((point >> 16) & 0x3) {
                   case 0:
                        d1 = s1;
                        break;
                   case 1:
                        d1 = ((s1 & 0xfcfcfc) + (s1 & 0xfcfcfc) + (s1 & 0xfcfcfc) + (s2 & 0xfcfcfc)) >> 2;
                        break;
                   case 2:
                        d1 = ((s1 & 0xfcfcfc) + (s2 & 0xfcfcfc) + (s2 & 0xfcfcfc) + (s2 & 0xfcfcfc)) >> 2;
                        break;
                   case 3:
                        d1 = s2;
                        break;
              }

              point += hfraq;

              s1 = src32[point>>18];
              s2 = src32[(point>>18) + 1];

              dp = PIXEL_RGB32TO16(d1);

              switch ((point >> 16) & 0x3) {
                   case 0:
                        d2 = s1;
                        break;
                   case 1:
                        d2 = ((s1 & 0xfcfcfc) + (s1 & 0xfcfcfc) + (s1 & 0xfcfcfc) + (s2 & 0xfcfcfc)) >> 2;
                        break;
                   case 2:
                        d2 = ((s1 & 0xfcfcfc) + (s2 & 0xfcfcfc) + (s2 & 0xfcfcfc) + (s2 & 0xfcfcfc)) >> 2;
                        break;
                   case 3:
                        d2 = s2;
                        break;
              }

              dp |= (PIXEL_RGB32TO16(d2) << 16);

              point += hfraq;


              if (y == dst_height - 1)
                   dst32[x] = dp;
              else {
                   switch ((line >> 16) & 0x3) {
                        case 0:
                             dst32[x] = dp;
                             break;
                        case 1:
                             dst32[x] = (((((linecache[x] & 0x07e0f81f) + (dp & 0x07e0f81f) + (dp & 0x07e0f81f) + (dp & 0x07e0f81f)) >> 2) & 0x07e0f81f) |
                                         (((((linecache[x] >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e)) << 2) & 0xf81f07e0));
                             break;
                        case 2:
                             dst32[x] = (((((linecache[x] & 0x07e0f81f) + (dp & 0x07e0f81f)) >> 1) & 0x07e0f81f) |
                                         (((((linecache[x] >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e)) << 3) & 0xf81f07e0));
                             break;
                        case 3:
                             dst32[x] = (((((linecache[x] & 0x07e0f81f) + (linecache[x] & 0x07e0f81f) + (linecache[x] & 0x07e0f81f) + (dp & 0x07e0f81f)) >> 2) & 0x07e0f81f) |
                                         (((((linecache[x] >> 4) & 0x0f81f07e) + ((linecache[x] >> 4) & 0x0f81f07e) + ((linecache[x] >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e)) << 2) & 0xf81f07e0));
                             break;
                   }
              }

              linecache[x] = dp;
         }

         line -= vfraq;
    }
}

/**********************************************************************************************************************/

#define COLOR_KEY_PROTECT

#define KEY_PROTECT (key)
#define KEY_REPLACE (key^1)

/**********************************************************************************************************************/
/*** 16 bit RGB 565 scalers *******************************************************************************************/
/**********************************************************************************************************************/

#define SHIFT_R5          5
#define SHIFT_R6          6
#define X_F81F            0xf81f
#define X_07E0            0x07e0

/**********************************************************************************************************************/

#define POINT_0               hfraq
#define LINE_0                vfraq
#define POINT_TO_RATIO(p,ps)  ( (((((p)) & 0x3ffff) ? : 0x40000) << 6) / (ps) )
#define LINE_TO_RATIO(l,ls)   ( (((((l)) & 0x3ffff) ? : 0x40000) << 5) / (ls) )

#define POINT_L(p,ps)  ( (((p)-1) >> 18) - 1 )
#define POINT_R(p,ps)  ( (((p)-1) >> 18) )

#define LINE_T(l,ls)  ( (((l)-1) >> 18) - 1 )
#define LINE_B(l,ls)  ( (((l)-1) >> 18) )

static void stretch_hvx_rgb16_down( void       *dst,
                                    int         dpitch,
                                    const void *src,
                                    int         spitch,
                                    int         width,
                                    int         height,
                                    int         dst_width,
                                    int         dst_height,
                                    DFBRegion  *clip,
                                    u16         key )
{
#include <gfx/generic/stretch_hvx_16.h>
}

static void stretch_hvx_rgb16_down_keyed( void       *dst,
                                          int         dpitch,
                                          const void *src,
                                          int         spitch,
                                          int         width,
                                          int         height,
                                          int         dst_width,
                                          int         dst_height,
                                          DFBRegion  *clip,
                                          u16         key,
                                          u16         srckey )
{
#define COLOR_KEY srckey
#include <gfx/generic/stretch_hvx_16.h>
#undef COLOR_KEY
}

static void stretch_hvx_rgb16_down_index( void           *dst,
                                          int             dpitch,
                                          const void     *src,
                                          int             spitch,
                                          int             width,
                                          int             height,
                                          int             dst_width,
                                          int             dst_height,
                                          DFBRegion      *clip,
                                          u16             key,
                                          const DFBColor *palette )
{
     int pn;
     u16 lookup[256];

     for (pn=0; pn<256; pn++)
          lookup[pn] = PIXEL_RGB16( palette[pn].r, palette[pn].g, palette[pn].b );

#define SOURCE_LOOKUP(x) lookup[x]
#define SOURCE_TYPE      u8
#include <gfx/generic/stretch_hvx_16.h>
#undef SOURCE_LOOKUP
#undef SOURCE_TYPE
}

#undef POINT_0
#undef LINE_0
#undef POINT_TO_RATIO
#undef LINE_TO_RATIO
#undef POINT_L
#undef POINT_R
#undef LINE_T
#undef LINE_B

/**********************************************************************************************************************/

#define POINT_0               0
#define LINE_0                0
#define POINT_TO_RATIO(p,ps)  ( ((p) & 0x3ffff) >> 12 )
#define LINE_TO_RATIO(l,ls)   ( ((l) & 0x3ffff) >> 13 )

#define POINT_L(p,ps)  ( (((p)) >> 18) )
#define POINT_R(p,ps)  ( (((p)) >> 18) + 1 )

#define LINE_T(l,ls)  ( (((l)) >> 18) )
#define LINE_B(l,ls)  ( (((l)) >> 18) + 1 )

static void stretch_hvx_rgb16_up( void       *dst,
                                  int         dpitch,
                                  const void *src,
                                  int         spitch,
                                  int         width,
                                  int         height,
                                  int         dst_width,
                                  int         dst_height,
                                  DFBRegion  *clip,
                                  u16         key )
{
#include <gfx/generic/stretch_hvx_16.h>
}

static void stretch_hvx_rgb16_up_keyed( void       *dst,
                                        int         dpitch,
                                        const void *src,
                                        int         spitch,
                                        int         width,
                                        int         height,
                                        int         dst_width,
                                        int         dst_height,
                                        DFBRegion  *clip,
                                        u16         key,
                                        u16         srckey )
{
#define COLOR_KEY srckey
#include <gfx/generic/stretch_hvx_16.h>
#undef COLOR_KEY
}

static void stretch_hvx_rgb16_up_index( void           *dst,
                                        int             dpitch,
                                        const void     *src,
                                        int             spitch,
                                        int             width,
                                        int             height,
                                        int             dst_width,
                                        int             dst_height,
                                        DFBRegion      *clip,
                                        u16             key,
                                        const DFBColor *palette )
{
     int pn;
     u16 lookup[256];

     for (pn=0; pn<256; pn++)
          lookup[pn] = PIXEL_RGB16( palette[pn].r, palette[pn].g, palette[pn].b );

#define SOURCE_LOOKUP(x) lookup[x]
#define SOURCE_TYPE      u8
#include <gfx/generic/stretch_hvx_16.h>
#undef SOURCE_LOOKUP
#undef SOURCE_TYPE
}

#undef POINT_0
#undef LINE_0
#undef POINT_TO_RATIO
#undef LINE_TO_RATIO
#undef POINT_L
#undef POINT_R
#undef LINE_T
#undef LINE_B

/**********************************************************************************************************************/

#undef SHIFT_R5
#undef SHIFT_R6
#undef X_F81F
#undef X_07E0

/**********************************************************************************************************************/
/*** 16 bit ARGB 4444 scalers *****************************************************************************************/
/**********************************************************************************************************************/

#define SHIFT_R5          4
#define SHIFT_R6          4
#define X_F81F            0x0f0f
#define X_07E0            0xf0f0
#define HAS_ALPHA

/**********************************************************************************************************************/

#define POINT_0               hfraq
#define LINE_0                vfraq
#define POINT_TO_RATIO(p,ps)  ( (((((p)) & 0x3ffff) ? : 0x40000) << 6) / (ps) )
#define LINE_TO_RATIO(l,ls)   ( (((((l)) & 0x3ffff) ? : 0x40000) << 5) / (ls) )

#define POINT_L(p,ps)  ( (((p)-1) >> 18) - 1 )
#define POINT_R(p,ps)  ( (((p)-1) >> 18) )

#define LINE_T(l,ls)  ( (((l)-1) >> 18) - 1 )
#define LINE_B(l,ls)  ( (((l)-1) >> 18) )

static void stretch_hvx_argb4444_down( void       *dst,
                                       int         dpitch,
                                       const void *src,
                                       int         spitch,
                                       int         width,
                                       int         height,
                                       int         dst_width,
                                       int         dst_height,
                                       DFBRegion  *clip,
                                       u16         key )
{
#include <gfx/generic/stretch_hvx_16.h>
}

static void stretch_hvx_argb4444_down_keyed( void       *dst,
                                             int         dpitch,
                                             const void *src,
                                             int         spitch,
                                             int         width,
                                             int         height,
                                             int         dst_width,
                                             int         dst_height,
                                             DFBRegion  *clip,
                                             u16         key,
                                             u16         srckey )
{
#define COLOR_KEY srckey
#include <gfx/generic/stretch_hvx_16.h>
#undef COLOR_KEY
}

static void stretch_hvx_argb4444_down_index( void           *dst,
                                             int             dpitch,
                                             const void     *src,
                                             int             spitch,
                                             int             width,
                                             int             height,
                                             int             dst_width,
                                             int             dst_height,
                                             DFBRegion      *clip,
                                             u16             key,
                                             const DFBColor *palette )
{
     int pn;
     u16 lookup[256];

     for (pn=0; pn<256; pn++)
          lookup[pn] = PIXEL_ARGB4444( palette[pn].a, palette[pn].r, palette[pn].g, palette[pn].b );

#define SOURCE_LOOKUP(x) lookup[x]
#define SOURCE_TYPE      u8
#include <gfx/generic/stretch_hvx_16.h>
#undef SOURCE_LOOKUP
#undef SOURCE_TYPE
}

#undef POINT_0
#undef LINE_0
#undef POINT_TO_RATIO
#undef LINE_TO_RATIO
#undef POINT_L
#undef POINT_R
#undef LINE_T
#undef LINE_B

/**********************************************************************************************************************/

#define POINT_0               0
#define LINE_0                0
#define POINT_TO_RATIO(p,ps)  ( ((p) & 0x3ffff) >> 12 )
#define LINE_TO_RATIO(l,ls)   ( ((l) & 0x3ffff) >> 13 )

#define POINT_L(p,ps)  ( (((p)) >> 18) )
#define POINT_R(p,ps)  ( (((p)) >> 18) + 1 )

#define LINE_T(l,ls)  ( (((l)) >> 18) )
#define LINE_B(l,ls)  ( (((l)) >> 18) + 1 )

static void stretch_hvx_argb4444_up( void       *dst,
                                     int         dpitch,
                                     const void *src,
                                     int         spitch,
                                     int         width,
                                     int         height,
                                     int         dst_width,
                                     int         dst_height,
                                     DFBRegion  *clip,
                                     u16         key )
{
#include <gfx/generic/stretch_hvx_16.h>
}

static void stretch_hvx_argb4444_up_keyed( void       *dst,
                                           int         dpitch,
                                           const void *src,
                                           int         spitch,
                                           int         width,
                                           int         height,
                                           int         dst_width,
                                           int         dst_height,
                                           DFBRegion  *clip,
                                           u16         key,
                                           u16         srckey )
{
#define COLOR_KEY srckey
#include <gfx/generic/stretch_hvx_16.h>
#undef COLOR_KEY
}

static void stretch_hvx_argb4444_up_index( void           *dst,
                                           int             dpitch,
                                           const void     *src,
                                           int             spitch,
                                           int             width,
                                           int             height,
                                           int             dst_width,
                                           int             dst_height,
                                           DFBRegion      *clip,
                                           u16             key,
                                           const DFBColor *palette )
{
     int pn;
     u16 lookup[256];

     for (pn=0; pn<256; pn++)
          lookup[pn] = PIXEL_ARGB4444( palette[pn].a, palette[pn].r, palette[pn].g, palette[pn].b );

#define SOURCE_LOOKUP(x) lookup[x]
#define SOURCE_TYPE      u8
#include <gfx/generic/stretch_hvx_16.h>
#undef SOURCE_LOOKUP
#undef SOURCE_TYPE
}

#undef POINT_0
#undef LINE_0
#undef POINT_TO_RATIO
#undef LINE_TO_RATIO
#undef POINT_L
#undef POINT_R
#undef LINE_T
#undef LINE_B

/**********************************************************************************************************************/

#undef HAS_ALPHA
#undef SHIFT_R5
#undef SHIFT_R6
#undef X_F81F
#undef X_07E0

/**********************************************************************************************************************/
/**********************************************************************************************************************/

const StretchAlgo wm_stretch_simple =
     { "simple", "Simple Scaler",                           stretch_simple_rgb16,
                                                            stretch_simple_rgb16_keyed,
                                                            NULL,
                                                            NULL,
                                                            NULL };

const StretchAlgo wm_stretch_down =
     { "down2",  "2x2 Down Scaler",                         stretch_hvx_rgb16_down,
                                                            stretch_hvx_rgb16_down_keyed,
                                                            stretch_hvx_argb4444_down,
                                                            stretch_hvx_rgb16_down_index,
                                                            stretch_down2_rgb16_from32 };

const StretchAlgo wm_stretch_up =
     { "hv4",    "Horizontal/vertical interpolation",       stretch_hvx_rgb16_up,
                                                            stretch_hvx_rgb16_up_keyed,
                                                            stretch_hvx_argb4444_up,
                                                            stretch_hvx_rgb16_up_index,
                                                            stretch_hv4_rgb16_from32 };

