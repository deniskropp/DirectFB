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
                                  DFBRegion   *clip )
{
    int x;
    int h      = (clip->y2 - clip->y1 + 1);
    int w2     = (clip->x2 - clip->x1 + 1) / 2;
    int hfraq  = (width  << 20) / dst_width;
    int vfraq  = (height << 20) / dst_height;
    int point0 = clip->x1 * hfraq;
    int line   = clip->y1 * vfraq;
    int hfraq2 = hfraq << 1;

    u32 *dst32 = dst + clip->y1 * dpitch + clip->x1 * 2;

    D_ASSUME( !(clip->x1 & 1) );
    D_ASSUME( clip->x2 & 1 );

    while (h--) {
         int        point = point0;
         const u16 *src16 = src + spitch * (line >> 20);

         for (x=0; x<w2; x++) {
              dst32[x] = ((src16[point>>20] == 0x20) ? 0x40 : src16[point>>20]) |
                         (((src16[(point+hfraq)>>20] == 0x20) ? 0x40 : src16[(point+hfraq)>>20]) << 16);

              point += hfraq2;
         }

         line  += vfraq;
         dst32 += dpitch/4;
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
                                        u16          src_key )
{
    int x;
    int h      = (clip->y2 - clip->y1 + 1);
    int w2     = (clip->x2 - clip->x1 + 1) / 2;
    int hfraq  = (width  << 20) / dst_width;
    int vfraq  = (height << 20) / dst_height;
    int point0 = clip->x1 * hfraq;
    int line   = clip->y1 * vfraq;
    int hfraq2 = hfraq << 1;

    u32 *dst32 = dst + clip->y1 * dpitch + clip->x1 * 2;

    D_ASSUME( !(clip->x1 & 1) );
    D_ASSUME( clip->x2 & 1 );

    while (h--) {
         int        point = point0;
         const u16 *src16 = src + spitch * (line >> 20);

         for (x=0; x<w2; x++) {
              u32 l = src16[point>>20];
              u32 r = src16[(point+hfraq)>>20];

              if (l != src_key) {
                   if (r != src_key)
                        dst32[x] = (((r == 0x20) ? 0x40 : r) << 16) | ((l == 0x20) ? 0x40 : l);
                   else
                        *(__u16*)dst32 = (l == 0x20) ? 0x40 : l;
              }
              else if (r != src_key)
                   *(((__u16*)dst32)+1) = (r == 0x20) ? 0x40 : r;

              point += hfraq2;
         }

         line  += vfraq;
         dst32 += dpitch/4;
    }
}

/**********************************************************************************************************************/

static void stretch_down2_rgb16( void         *dst,
                                 int           dpitch,
                                 const void   *src,
                                 int           spitch,
                                 int           width,
                                 int           height,
                                 int           dst_width,
                                 int           dst_height,
                                 DFBRegion    *clip )
{
     int x, y;
     int w2     = (clip->x2 - clip->x1 + 1) / 2;
     int hfraq  = (width  << 20) / dst_width;
     int vfraq  = (height << 20) / dst_height;
     int sp2    = spitch / 2;
     int point0 = clip->x1 * hfraq;
     int line   = clip->y2 * vfraq;

     D_ASSUME( !(clip->x1 & 1) );
     D_ASSUME( clip->x2 & 1 );

     dst += clip->x1 * 2;

     for (y=clip->y2; y>=clip->y1; y--) {
          int point = point0;

          __u16 s1;
          __u16 s2;
          __u16 s3;
          __u16 s4;
          __u32 dp;
          __u32 g;

          int sy = (line >> 20);

          if (sy > height - 2)
               sy = height - 2;

          __u32       *dst32 = dst + dpitch * y;
          const __u16 *src16 = src + spitch * sy;


          for (x=0; x<w2; x++) {
               s1 = src16[point>>20];
               s2 = src16[(point>>20) + 1];
               s3 = src16[(point>>20) + sp2];
               s4 = src16[(point>>20) + sp2 + 1];

               point += hfraq;

               g = ((((s1 & 0x07e0) + (s2 & 0x07e0) + (s3 & 0x07e0) + (s4 & 0x07e0)) >> 2) & 0x07e0);
               if (g == 0x20)
                    g = 0x40;

               dp = g | ((((s1 & 0xf81f) + (s2 & 0xf81f) + (s3 & 0xf81f) + (s4 & 0xf81f)) >> 2) & 0xf81f);



               s1 = src16[point>>20];
               s2 = src16[(point>>20) + 1];
               s3 = src16[(point>>20) + sp2];
               s4 = src16[(point>>20) + sp2 + 1];

               point += hfraq;

               g = ((((s1 & 0x07e0) + (s2 & 0x07e0) + (s3 & 0x07e0) + (s4 & 0x07e0)) << 14) & 0x07e00000);
               if (g == 0x200000)
                    g = 0x400000;

               dp |= g | ((((s1 & 0xf81f) + (s2 & 0xf81f) + (s3 & 0xf81f) + (s4 & 0xf81f)) << 14) & 0xf81f0000);


               dst32[x] = dp;
          }

          line -= vfraq;
     }
}

static void stretch_down2_rgb16_keyed( void         *dst,
                                       int           dpitch,
                                       const void   *src,
                                       int           spitch,
                                       int           width,
                                       int           height,
                                       int           dst_width,
                                       int           dst_height,
                                       DFBRegion    *clip,
                                       u16           src_key )
{
     int x, y;
     int w2     = (clip->x2 - clip->x1 + 1) / 2;
     int hfraq  = (width  << 20) / dst_width;
     int vfraq  = (height << 20) / dst_height;
     int sp2    = spitch / 2;
     int point0 = clip->x1 * hfraq;
     int line   = clip->y2 * vfraq;

     D_ASSUME( !(clip->x1 & 1) );
     D_ASSUME( clip->x2 & 1 );

     dst += clip->x1 * 2;

     for (y=clip->y2; y>=clip->y1; y--) {
          int point = point0;

          __u16 s1;
          __u16 s2;
          __u16 s3;
          __u16 s4;

          int sy = (line >> 20);

          if (sy > height - 2)
               sy = height - 2;

          __u32       *dst32 = dst + dpitch * y;
          const __u16 *src16 = src + spitch * sy;


          for (x=0; x<w2; x++) {
               s1 = src16[point>>20];
               s2 = src16[(point>>20) + 1];
               s3 = src16[(point>>20) + sp2];
               s4 = src16[(point>>20) + sp2 + 1];

               point += hfraq;

               register u32 l = ((((s1 & 0xf81f) + (s2 & 0xf81f) + (s3 & 0xf81f) + (s4 & 0xf81f)) >> 2) & 0xf81f) |
                                ((((s1 & 0x07e0) + (s2 & 0x07e0) + (s3 & 0x07e0) + (s4 & 0x07e0)) >> 2) & 0x07e0);



               s1 = src16[point>>20];
               s2 = src16[(point>>20) + 1];
               s3 = src16[(point>>20) + sp2];
               s4 = src16[(point>>20) + sp2 + 1];

               point += hfraq;

               register u32 h = ((((s1 & 0xf81f) + (s2 & 0xf81f) + (s3 & 0xf81f) + (s4 & 0xf81f)) >> 2) & 0xf81f) |
                                ((((s1 & 0x07e0) + (s2 & 0x07e0) + (s3 & 0x07e0) + (s4 & 0x07e0)) >> 2) & 0x07e0);


               if (l != src_key) {
                    if (h != src_key)
                         dst32[x] = (((h == 0x20) ? 0x40 : h) << 16) | ((l == 0x20) ? 0x40 : l);
                    else
                         *(__u16*)dst32 = (l == 0x20) ? 0x40 : l;
               }
               else if (h != src_key)
                    *(((__u16*)dst32)+1) = (h == 0x20) ? 0x40 : h;
          }

          line -= vfraq;
     }
}

static void stretch_down2_rgb16_from32( void         *dst,
                                        int           dpitch,
                                        const void   *src,
                                        int           spitch,
                                        int           width,
                                        int           height,
                                        int           dst_width,
                                        int           dst_height,
                                        DFBRegion    *clip )
{
     int x, y;
     int w2 = dst_width / 2;
     int hfraq = (width  << 20) / dst_width;
     int vfraq = (height << 20) / dst_height;
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

          int sy = (line >> 20);

          if (sy > height - 2)
               sy = height - 2;

          __u32       *dst32 = dst + dpitch * y;
          const __u32 *src32 = src + spitch * sy;


          for (x=0; x<w2; x++) {
               s1 = src32[point>>20];
               s2 = src32[(point>>20) + 1];
               s3 = src32[(point>>20) + sp4];
               s4 = src32[(point>>20) + sp4 + 1];

               point += hfraq;

               d1 = ((s1 & 0xfcfcfc) + (s2 & 0xfcfcfc) + (s3 & 0xfcfcfc) + (s4 & 0xfcfcfc)) >> 2;


               s1 = src32[point>>20];
               s2 = src32[(point>>20) + 1];
               s3 = src32[(point>>20) + sp4];
               s4 = src32[(point>>20) + sp4 + 1];

               point += hfraq;

               d2 = ((s1 & 0xfcfcfc) + (s2 & 0xfcfcfc) + (s3 & 0xfcfcfc) + (s4 & 0xfcfcfc)) >> 2;


               dst32[x] = PIXEL_RGB32TO16(d1) | (PIXEL_RGB32TO16(d2) << 16);
          }

          line -= vfraq;
     }
}

static void stretch_down2_rgb16_indexed( void           *dst,
                                         int             dpitch,
                                         const void     *src,
                                         int             spitch,
                                         int             width,
                                         int             height,
                                         int             dst_width,
                                         int             dst_height,
                                         const DFBColor *palette )
{
     int x, y;
     int w2 = dst_width / 2;
     int hfraq = (width  << 20) / dst_width;
     int vfraq = (height << 20) / dst_height;
     int line  = vfraq * (dst_height - 1);

     __u16 lookup[256];

     for (x=0; x<256; x++)
          lookup[x] = PIXEL_RGB16( palette[x].r, palette[x].g, palette[x].b );

     for (y=dst_height-1; y>=0; y--) {
          int point = 0;

          __u16 s1;
          __u16 s2;
          __u16 s3;
          __u16 s4;
          __u32 dp;

          int sy = (line >> 20);

          if (sy > height - 2)
               sy = height - 2;

          __u32      *dst32 = dst + dpitch * y;
          const __u8 *src8  = src + spitch * sy;


          for (x=0; x<w2; x++) {
               s1 = lookup[ src8[point>>20] ];
               s2 = lookup[ src8[(point>>20) + 1] ];
               s3 = lookup[ src8[(point>>20) + spitch] ];
               s4 = lookup[ src8[(point>>20) + spitch + 1] ];

               point += hfraq;

               dp = ((((s1 & 0xf81f) + (s2 & 0xf81f) + (s3 & 0xf81f) + (s4 & 0xf81f)) >> 2) & 0xf81f) |
                    ((((s1 & 0x07e0) + (s2 & 0x07e0) + (s3 & 0x07e0) + (s4 & 0x07e0)) >> 2) & 0x07e0);


               s1 = lookup[ src8[point>>20] ];
               s2 = lookup[ src8[(point>>20) + 1] ];
               s3 = lookup[ src8[(point>>20) + spitch] ];
               s4 = lookup[ src8[(point>>20) + spitch + 1] ];

               point += hfraq;

               dp |= (((((s1 & 0xf81f) + (s2 & 0xf81f) + (s3 & 0xf81f) + (s4 & 0xf81f)) << 14) & 0xf81f0000) |
                      ((((s1 & 0x07e0) + (s2 & 0x07e0) + (s3 & 0x07e0) + (s4 & 0x07e0)) << 14) & 0x07e00000));


               dst32[x] = dp;
          }

          line -= vfraq;
     }
}

static void stretch_down2_argb4444( void         *dst,
                                    int           dpitch,
                                    const void   *src,
                                    int           spitch,
                                    int           width,
                                    int           height,
                                    int           dst_width,
                                    int           dst_height,
                                    DFBRegion    *clip )
{
     int x, y;
     int w2 = dst_width / 2;
     int hfraq = (width  << 20) / dst_width;
     int vfraq = (height << 20) / dst_height;
     int line  = vfraq * (dst_height - 1);
     int sp2   = spitch / 2;

     for (y=dst_height-1; y>=0; y--) {
          int point = 0;

          __u16 s1;
          __u16 s2;
          __u16 s3;
          __u16 s4;
          __u32 dp;

          int sy = (line >> 20);

          if (sy > height - 2)
               sy = height - 2;

          __u32       *dst32 = dst + dpitch * y;
          const __u16 *src16 = src + spitch * sy;


          for (x=0; x<w2; x++) {
               s1 = src16[point>>20];
               s2 = src16[(point>>20) + 1];
               s3 = src16[(point>>20) + sp2];
               s4 = src16[(point>>20) + sp2 + 1];

               point += hfraq;

               dp = ((((s1 & 0xf0f0) + (s2 & 0xf0f0) + (s3 & 0xf0f0) + (s4 & 0xf0f0)) >> 2) & 0xf0f0) |
                    ((((s1 & 0x0f0f) + (s2 & 0x0f0f) + (s3 & 0x0f0f) + (s4 & 0x0f0f)) >> 2) & 0x0f0f);


               s1 = src16[point>>20];
               s2 = src16[(point>>20) + 1];
               s3 = src16[(point>>20) + sp2];
               s4 = src16[(point>>20) + sp2 + 1];

               point += hfraq;

               dp |= (((((s1 & 0xf0f0) + (s2 & 0xf0f0) + (s3 & 0xf0f0) + (s4 & 0xf0f0)) << 14) & 0xf0f00000) |
                      ((((s1 & 0x0f0f) + (s2 & 0x0f0f) + (s3 & 0x0f0f) + (s4 & 0x0f0f)) << 14) & 0x0f0f0000));


               dst32[x] = dp;
          }

          line -= vfraq;
     }
}

/**********************************************************************************************************************/

//#define COLOR_KEY_PROTECT

#if 1
static void stretch_hvx_rgb16( void         *dst,
                               int           dpitch,
                               const void   *src,
                               int           spitch,
                               int           width,
                               int           height,
                               int           dst_width,
                               int           dst_height,
                               DFBRegion    *clip )
{
    int x;
    int w      = clip->x2 - clip->x1 + 1;
    int h      = clip->y2 - clip->y1 + 1;
    int w2     = w / 2;
    int hfraq  = (width  << 20) / dst_width;
    int vfraq  = (height << 20) / dst_height;
    int point0 = clip->x1 * hfraq;
    int line   = clip->y2 * vfraq;
    int dp4    = dpitch / 4;

    int        point = point0;
    int        last  = (line >> 20) + 1;
    const u16 *src16 = src + spitch * ((last > height - 1) ? height - 1 : last);
    u32       *dst32 = dst + clip->x1 * 2 + clip->y2 * dpitch;
    u32        linecache[w2];

    D_ASSUME( !(clip->x1 & 1) );
    D_ASSUME( clip->x2 & 1 );

    /* Prefill the line cache. */
    for (x=0; x<w2; x++) {
         u32 X, L, R, dp;

         /* Horizontal interpolation of 1st pixel */
         X = (point >> 14) & 0x3F;
         L = src16[point>>20];
         R = src16[(point>>20) + 1];

         dp = (((((R & 0xf81f)-(L & 0xf81f))*X + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
               ((((R & 0x07e0)-(L & 0x07e0))*X + ((L & 0x07e0)<<6)) & 0x0001f800)) >> 6;

         point += hfraq;

         /* Horizontal interpolation of 2nd pixel */
         X = (point >> 14) & 0x3F;
         L = src16[point>>20];
         R = src16[(point>>20) + 1];

         dp |= (((((R & 0xf81f)-(L & 0xf81f))*X + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                ((((R & 0x07e0)-(L & 0x07e0))*X + ((L & 0x07e0)<<6)) & 0x0001f800)) << 10;

         point += hfraq;

         /* Store pixels in line cache. */
         linecache[x] = dp;
    }

    /* Scale the image. */
    while (h--) {
         point = point0;
         src16 = src + spitch * (line >> 20);

         for (x=0; x<w2; x++) {
              u32 X, L, R, dp;

              /* Horizontal interpolation of 1st pixel */
              L = src16[point>>20];
              R = src16[(point>>20) + 1];
              X = (point >> 14) & 0x3F;

              dp = (((((R & 0xf81f)-(L & 0xf81f))*X + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                    ((((R & 0x07e0)-(L & 0x07e0))*X + ((L & 0x07e0)<<6)) & 0x0001f800)) >> 6;

              point += hfraq;

              /* Horizontal interpolation of 2nd pixel */
              L = src16[point>>20];
              R = src16[(point>>20) + 1];
              X = (point >> 14) & 0x3F;

              dp |= (((((R & 0xf81f)-(L & 0xf81f))*X + ((L & 0xf81f)<<6)) & 0x003e07c0) + 
                     ((((R & 0x07e0)-(L & 0x07e0))*X + ((L & 0x07e0)<<6)) & 0x0001f800)) << 10;

              point += hfraq;

              /* Vertical interpolation of both pixels */
              X = (line >> 15) & 0x1F;

#ifdef COLOR_KEY_PROTECT
              u32 dt = ((((((linecache[x] & 0x07e0f81f) - (dp & 0x07e0f81f))*X) >> 5) + (dp & 0x07e0f81f)) & 0x07e0f81f) +
                       ((((((linecache[x]>>5) & 0x07c0f83f) - ((dp>>5) & 0x07c0f83f))*X) + (dp & 0xf81f07e0)) & 0xf81f07e0);

              /* Get two new pixels. */
              u16 l = dt;
              u16 h = dt >> 16;

              /* Write to destination with color key protection */
              dst32[x] = (((h == 0x20) ? 0x40 : h) << 16) | ((l == 0x20) ? 0x40 : l);
#else
              /* Write to destination without color key protection */
              dst32[x] = ((((((linecache[x] & 0x07e0f81f) - (dp & 0x07e0f81f))*X) >> 5) + (dp & 0x07e0f81f)) & 0x07e0f81f) +
                         ((((((linecache[x]>>5) & 0x07c0f83f) - ((dp>>5) & 0x07c0f83f))*X) + (dp & 0xf81f07e0)) & 0xf81f07e0);
#endif

              /* Store pixels in line cache. */
              linecache[x] = dp;
         }

         dst32 -= dp4;



         /*
          * What a great optimization!  -24% time
          */
         int next = line - vfraq;

         while ((next >> 20) == (line >> 20) && h) {
              h--;

#ifdef COLOR_KEY_PROTECT
              for (x=0; x<w2; x++) {
                   /* Get two new pixels. */
                   u16 l = linecache[x];
                   u16 h = linecache[x] >> 16;

                   /* Write to destination with color key protection */
                   dst32[x] = (((h == 0x20) ? 0x40 : h) << 16) | ((l == 0x20) ? 0x40 : l);
              }
#else
              memcpy( dst32, linecache, w * 2 );
#endif

              dst32 -= dp4;
              next  -= vfraq;
         }

         line = next;
    }
}
#else
static void stretch_hv4_rgb16( void         *dst,
                               int           dpitch,
                               const void   *src,
                               int           spitch,
                               int           width,
                               int           height,
                               int           dst_width,
                               int           dst_height,
                               DFBRegion    *clip )
{
    int x, y;
    int w2     = (clip->x2 - clip->x1 + 1) / 2;
    int hfraq  = (width  << 20) / dst_width;
    int vfraq  = (height << 20) / dst_height;
    int point0 = clip->x1 * hfraq;
    int line   = clip->y2 * vfraq;

    __u32 linecache[w2];

    D_ASSUME( !(clip->x1 & 1) );
    D_ASSUME( clip->x2 & 1 );

    dst += clip->x1 * 2;

    for (y=clip->y2; y>=clip->y1; y--) {
         int point = point0;

         __u32 dp;

         __u32       *dst32 = dst + dpitch * y;
         const __u16 *src16 = src + spitch * (line >> 20);


         for (x=0; x<w2; x++) {
              register u32 s1;
              register u32 s2;

              s1 = src16[point>>20];
              s2 = src16[(point>>20) + 1];

              switch ((point >> 18) & 0x3) {
                   case 0:
                        dp = s1;
                        break;
                   case 1:
                        dp = ((((s2 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f)) >> 2) & 0xf81f) |
                             ((((s2 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0)) >> 2) & 0x07e0);
                        break;
                   case 2:
                        dp = ((((s2 & 0xf81f) + (s2 & 0xf81f) + (s2 & 0xf81f) + (s1 & 0xf81f)) >> 2) & 0xf81f) |
                             ((((s2 & 0x07e0) + (s2 & 0x07e0) + (s2 & 0x07e0) + (s1 & 0x07e0)) >> 2) & 0x07e0);
                        break;
                   case 3:
                        dp = s2;
                        break;
              }

              point += hfraq;


              s1 = src16[point>>20];
              s2 = src16[(point>>20) + 1];

              switch ((point >> 18) & 0x3) {
                   case 0:
                        dp |= (s1 << 16);
                        break;
                   case 1:
                        dp |= (((((s2 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f)) << 14) & 0xf81f0000) |
                               ((((s2 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0)) << 14) & 0x07e00000));
                        break;
                   case 2:
                        dp |= (((((s2 & 0xf81f) + (s2 & 0xf81f) + (s2 & 0xf81f) + (s1 & 0xf81f)) << 14) & 0xf81f0000) |
                               ((((s2 & 0x07e0) + (s2 & 0x07e0) + (s2 & 0x07e0) + (s1 & 0x07e0)) << 14) & 0x07e00000));
                        break;
                   case 3:
                        dp |= (s2 << 16);
                        break;
              }

              point += hfraq;


              register u32 dt;

              if (y == clip->y2)
                   dt = dp;
              else {
                   switch ((line >> 18) & 0x3) {
                        case 0:
                             dt = dp;
                             break;
                        case 1:
                             dt = (((((linecache[x] & 0x07e0f81f) + (dp & 0x07e0f81f) + (dp & 0x07e0f81f) + (dp & 0x07e0f81f)) >> 2) & 0x07e0f81f) |
                                   (((((linecache[x] >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e)) << 2) & 0xf81f07e0));
                             break;
                        case 2:
                             dt = (((((linecache[x] & 0x07e0f81f) + (dp & 0x07e0f81f)) >> 1) & 0x07e0f81f) |
                                   (((((linecache[x] >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e)) << 3) & 0xf81f07e0));
                             break;
                        case 3:
                             dt = (((((linecache[x] & 0x07e0f81f) + (linecache[x] & 0x07e0f81f) + (linecache[x] & 0x07e0f81f) + (dp & 0x07e0f81f)) >> 2) & 0x07e0f81f) |
                                   (((((linecache[x] >> 4) & 0x0f81f07e) + ((linecache[x] >> 4) & 0x0f81f07e) + ((linecache[x] >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e)) << 2) & 0xf81f07e0));
                             break;
                   }
              }

              register u16 l = dt;
              register u16 h = dt >> 16;

              dst32[x] = (((h == 0x20) ? 0x40 : h) << 16) | ((l == 0x20) ? 0x40 : l);

              linecache[x] = dp;
         }

         line -= vfraq;
    }
}
#endif

static void stretch_hv4_rgb16_keyed( void         *dst,
                                     int           dpitch,
                                     const void   *src,
                                     int           spitch,
                                     int           width,
                                     int           height,
                                     int           dst_width,
                                     int           dst_height,
                                     DFBRegion    *clip,
                                     u16           src_key )
{
    int x, y;
    int w2     = (clip->x2 - clip->x1 + 1) / 2;
    int hfraq  = (width  << 20) / dst_width;
    int vfraq  = (height << 20) / dst_height;
    int point0 = clip->x1 * hfraq;
    int line   = clip->y2 * vfraq;
    u32 dkey   = src_key | (src_key << 16);

    __u32 linecache[w2];

    D_ASSUME( !(clip->x1 & 1) );
    D_ASSUME( clip->x2 & 1 );

    dst += clip->x1 * 2;

    for (y=clip->y2; y>=clip->y1; y--) {
         int point = point0;

         __u32 dp = 0;   /* fix warning */

         __u32       *dst32 = dst + dpitch * y;
         const __u16 *src16 = src + spitch * (line >> 20);


         for (x=0; x<w2; x++) {
              register u32 s1;
              register u32 s2;

              s1 = src16[point>>20];
              s2 = src16[(point>>20) + 1];

              switch ((point >> 18) & 0x3) {
                   case 0:
                        dp = s1;
                        break;
                   case 1:
                        dp = ((((s2 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f)) >> 2) & 0xf81f) |
                             ((((s2 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0)) >> 2) & 0x07e0);
                        break;
                   case 2:
                        dp = ((((s2 & 0xf81f) + (s2 & 0xf81f) + (s2 & 0xf81f) + (s1 & 0xf81f)) >> 2) & 0xf81f) |
                             ((((s2 & 0x07e0) + (s2 & 0x07e0) + (s2 & 0x07e0) + (s1 & 0x07e0)) >> 2) & 0x07e0);
                        break;
                   case 3:
                        dp = s2;
                        break;
              }

              point += hfraq;


              s1 = src16[point>>20];
              s2 = src16[(point>>20) + 1];

              switch ((point >> 18) & 0x3) {
                   case 0:
                        dp |= (s1 << 16);
                        break;
                   case 1:
                        dp |= (((((s2 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f)) << 14) & 0xf81f0000) |
                               ((((s2 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0)) << 14) & 0x07e00000));
                        break;
                   case 2:
                        dp |= (((((s2 & 0xf81f) + (s2 & 0xf81f) + (s2 & 0xf81f) + (s1 & 0xf81f)) << 14) & 0xf81f0000) |
                               ((((s2 & 0x07e0) + (s2 & 0x07e0) + (s2 & 0x07e0) + (s1 & 0x07e0)) << 14) & 0x07e00000));
                        break;
                   case 3:
                        dp |= (s2 << 16);
                        break;
              }

              point += hfraq;


              register u32 dt = 0;   /* fix warning */

              if (y == clip->y2)
                   dt = dp;
              else {
                   switch ((line >> 18) & 0x3) {
                        case 0:
                             dt = dp;
                             break;
                        case 1:
                             dt = (((((linecache[x] & 0x07e0f81f) + (dp & 0x07e0f81f) + (dp & 0x07e0f81f) + (dp & 0x07e0f81f)) >> 2) & 0x07e0f81f) |
                                   (((((linecache[x] >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e)) << 2) & 0xf81f07e0));
                             break;
                        case 2:
                             dt = (((((linecache[x] & 0x07e0f81f) + (dp & 0x07e0f81f)) >> 1) & 0x07e0f81f) |
                                   (((((linecache[x] >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e)) << 3) & 0xf81f07e0));
                             break;
                        case 3:
                             dt = (((((linecache[x] & 0x07e0f81f) + (linecache[x] & 0x07e0f81f) + (linecache[x] & 0x07e0f81f) + (dp & 0x07e0f81f)) >> 2) & 0x07e0f81f) |
                                   (((((linecache[x] >> 4) & 0x0f81f07e) + ((linecache[x] >> 4) & 0x0f81f07e) + ((linecache[x] >> 4) & 0x0f81f07e) + ((dp >> 4) & 0x0f81f07e)) << 2) & 0xf81f07e0));
                             break;
                   }
              }

              if (dt != dkey) {
                   register u16 l = dt;
                   register u16 h = dt >> 16;

                   if (l != src_key) {
                        if (h != src_key)
                             dst32[x] = (((h == 0x20) ? 0x40 : h) << 16) | ((l == 0x20) ? 0x40 : l);
                        else
                             *(__u16*)dst32 = (l == 0x20) ? 0x40 : l;
                   }
                   else if (h != src_key)
                        *(((__u16*)dst32)+1) = (h == 0x20) ? 0x40 : h;
              }

              linecache[x] = dp;
         }

         line -= vfraq;
    }
}

static void stretch_hv4_rgb16_from32( void         *dst,
                                      int           dpitch,
                                      const void   *src,
                                      int           spitch,
                                      int           width,
                                      int           height,
                                      int           dst_width,
                                      int           dst_height,
                                      DFBRegion    *clip )
{
    int x, y;
    int w2 = dst_width / 2;
    int hfraq = (width  << 20) / dst_width;
    int vfraq = (height << 20) / dst_height;
    int line  = vfraq * (dst_height - 1);

    __u32 linecache[dst_width/2];

    for (y=dst_height-1; y>=0; y--) {
         int point = 0;

         __u32 s1;
         __u32 s2;
         __u32 d1 = 0;   /* fix warning */
         __u32 d2 = 0;   /* fix warning */
         __u32 dp;

         __u32       *dst32 = dst + dpitch * y;
         const __u32 *src32 = src + spitch * (line >> 20);


         for (x=0; x<w2; x++) {
              s1 = src32[point>>20];
              s2 = src32[(point>>20) + 1];

              switch ((point >> 18) & 0x3) {
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

              s1 = src32[point>>20];
              s2 = src32[(point>>20) + 1];

              dp = PIXEL_RGB32TO16(d1);

              switch ((point >> 18) & 0x3) {
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
                   switch ((line >> 18) & 0x3) {
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

static void stretch_hv4_rgb16_indexed( void           *dst,
                                       int             dpitch,
                                       const void     *src,
                                       int             spitch,
                                       int             width,
                                       int             height,
                                       int             dst_width,
                                       int             dst_height,
                                       const DFBColor *palette )
{
    int x, y;
    int w2 = dst_width / 2;
    int hfraq = (width  << 20) / dst_width;
    int vfraq = (height << 20) / dst_height;
    int line  = vfraq * (dst_height - 1);

    __u16 lookup[256];
    __u32 linecache[dst_width/2];

    for (x=0; x<256; x++)
         lookup[x] = PIXEL_RGB16( palette[x].r, palette[x].g, palette[x].b );

    for (y=dst_height-1; y>=0; y--) {
         int point = 0;

         __u16 s1;
         __u16 s2;
         __u32 dp = 0;   /* fix warning */

         __u32      *dst32 = dst + dpitch * y;
         const __u8 *src8  = src + spitch * (line >> 20);


         for (x=0; x<w2; x++) {
              s1 = lookup[ src8[point>>20] ];
              s2 = lookup[ src8[(point>>20) + 1] ];

              switch ((point >> 18) & 0x3) {
                   case 0:
                        dp = s1;
                        break;
                   case 1:
                        dp = ((((s2 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f)) >> 2) & 0xf81f) |
                             ((((s2 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0)) >> 2) & 0x07e0);
                        break;
                   case 2:
                        dp = ((((s2 & 0xf81f) + (s2 & 0xf81f) + (s2 & 0xf81f) + (s1 & 0xf81f)) >> 2) & 0xf81f) |
                             ((((s2 & 0x07e0) + (s2 & 0x07e0) + (s2 & 0x07e0) + (s1 & 0x07e0)) >> 2) & 0x07e0);
                        break;
                   case 3:
                        dp = s2;
                        break;
              }

              point += hfraq;


              s1 = lookup[ src8[point>>20] ];
              s2 = lookup[ src8[(point>>20) + 1] ];

              switch ((point >> 18) & 0x3) {
                   case 0:
                        dp |= (s1 << 16);
                        break;
                   case 1:
                        dp |= (((((s2 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f) + (s1 & 0xf81f)) << 14) & 0xf81f0000) |
                               ((((s2 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0) + (s1 & 0x07e0)) << 14) & 0x07e00000));
                        break;
                   case 2:
                        dp |= (((((s2 & 0xf81f) + (s2 & 0xf81f) + (s2 & 0xf81f) + (s1 & 0xf81f)) << 14) & 0xf81f0000) |
                               ((((s2 & 0x07e0) + (s2 & 0x07e0) + (s2 & 0x07e0) + (s1 & 0x07e0)) << 14) & 0x07e00000));
                        break;
                   case 3:
                        dp |= (s2 << 16);
                        break;
              }

              point += hfraq;


              if (y == dst_height - 1)
                   dst32[x] = dp;
              else {
                   switch ((line >> 18) & 0x3) {
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

static void stretch_hv4_argb4444( void         *dst,
                                  int           dpitch,
                                  const void   *src,
                                  int           spitch,
                                  int           width,
                                  int           height,
                                  int           dst_width,
                                  int           dst_height,
                                  DFBRegion    *clip )
{
    int x, y;
    int w2 = dst_width / 2;
    int hfraq = (width  << 20) / dst_width;
    int vfraq = (height << 20) / dst_height;
    int line  = vfraq * (dst_height - 1);

    __u32 linecache[dst_width/2];

    for (y=dst_height-1; y>=0; y--) {
         int point = 0;

         __u16 s1;
         __u16 s2;
         __u32 dp = 0;   /* fix warning */

         __u32       *dst32 = dst + dpitch * y;
         const __u16 *src16 = src + spitch * (line >> 20);


         for (x=0; x<w2; x++) {
              s1 = src16[point>>20];
              s2 = src16[(point>>20) + 1];

              switch ((point >> 18) & 0x3) {
                   case 0:
                        dp = s1;
                        break;
                   case 1:
                        dp = ((((s2 & 0x0f0f) + (s1 & 0x0f0f) + (s1 & 0x0f0f) + (s1 & 0x0f0f)) >> 2) & 0x0f0f) |
                             ((((s2 & 0xf0f0) + (s1 & 0xf0f0) + (s1 & 0xf0f0) + (s1 & 0xf0f0)) >> 2) & 0xf0f0);
                        break;
                   case 2:
                        dp = ((((s2 & 0x0f0f) + (s2 & 0x0f0f) + (s2 & 0x0f0f) + (s1 & 0x0f0f)) >> 2) & 0x0f0f) |
                             ((((s2 & 0xf0f0) + (s2 & 0xf0f0) + (s2 & 0xf0f0) + (s1 & 0xf0f0)) >> 2) & 0xf0f0);
                        break;
                   case 3:
                        dp = s2;
                        break;
              }

              point += hfraq;


              s1 = src16[point>>20];
              s2 = src16[(point>>20) + 1];

              switch ((point >> 18) & 0x3) {
                   case 0:
                        dp |= (s1 << 16);
                        break;
                   case 1:
                        dp |= (((((s2 & 0x0f0f) + (s1 & 0x0f0f) + (s1 & 0x0f0f) + (s1 & 0x0f0f)) << 14) & 0x0f0f0000) |
                               ((((s2 & 0xf0f0) + (s1 & 0xf0f0) + (s1 & 0xf0f0) + (s1 & 0xf0f0)) << 14) & 0xf0f00000));
                        break;
                   case 2:
                        dp |= (((((s2 & 0x0f0f) + (s2 & 0x0f0f) + (s2 & 0x0f0f) + (s1 & 0x0f0f)) << 14) & 0x0f0f0000) |
                               ((((s2 & 0xf0f0) + (s2 & 0xf0f0) + (s2 & 0xf0f0) + (s1 & 0xf0f0)) << 14) & 0xf0f00000));
                        break;
                   case 3:
                        dp |= (s2 << 16);
                        break;
              }

              point += hfraq;


              if (y == dst_height - 1)
                   dst32[x] = dp;
              else {
                   switch ((line >> 18) & 0x3) {
                        case 0:
                             dst32[x] = dp;
                             break;
                        case 1:
                             dst32[x] = (((((linecache[x] & 0x0f0f0f0f) + (dp & 0x0f0f0f0f) + (dp & 0x0f0f0f0f) + (dp & 0x0f0f0f0f)) >> 2) & 0x0f0f0f0f) |
                                         (((((linecache[x] >> 4) & 0x0f0f0f0f) + ((dp >> 4) & 0x0f0f0f0f) + ((dp >> 4) & 0x0f0f0f0f) + ((dp >> 4) & 0x0f0f0f0f)) << 2) & 0xf0f0f0f0));
                             break;
                        case 2:
                             dst32[x] = (((((linecache[x] & 0x0f0f0f0f) + (dp & 0x0f0f0f0f)) >> 1) & 0x0f0f0f0f) |
                                         (((((linecache[x] >> 4) & 0x0f0f0f0f) + ((dp >> 4) & 0x0f0f0f0f)) << 3) & 0xf0f0f0f0));
                             break;
                        case 3:
                             dst32[x] = (((((linecache[x] & 0x0f0f0f0f) + (linecache[x] & 0x0f0f0f0f) + (linecache[x] & 0x0f0f0f0f) + (dp & 0x0f0f0f0f)) >> 2) & 0x0f0f0f0f) |
                                         (((((linecache[x] >> 4) & 0x0f0f0f0f) + ((linecache[x] >> 4) & 0x0f0f0f0f) + ((linecache[x] >> 4) & 0x0f0f0f0f) + ((dp >> 4) & 0x0f0f0f0f)) << 2) & 0xf0f0f0f0));
                             break;
                   }
              }

              linecache[x] = dp;
         }

         line -= vfraq;
    }
}

/**********************************************************************************************************************/

const StretchAlgo wm_stretch_simple =
     { "simple", "Simple Scaler",                           stretch_simple_rgb16,
                                                            stretch_simple_rgb16_keyed,
                                                            NULL,
                                                            NULL,
                                                            NULL };

const StretchAlgo wm_stretch_down2 =
     { "down2",  "2x2 Down Scaler",                         stretch_down2_rgb16,
                                                            stretch_down2_rgb16_keyed,
                                                            stretch_down2_argb4444,
                                                            stretch_down2_rgb16_indexed,
                                                            stretch_down2_rgb16_from32 };

const StretchAlgo wm_stretch_hv4 =
     { "hv4",    "Horizontal/vertical interpolation",       stretch_hvx_rgb16,
                                                            stretch_hv4_rgb16_keyed,
                                                            stretch_hv4_argb4444,
                                                            stretch_hv4_rgb16_indexed,
                                                            stretch_hv4_rgb16_from32 };

