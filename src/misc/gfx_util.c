/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

   Scaling routines ported from gdk_pixbuf by Sven Neumann
   <sven@convergence.de>.

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

#include <stdlib.h>
#include <stdio.h>

#include <pthread.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/palette.h>
#include <core/surfaces.h>

#include <misc/memcpy.h>
#include <misc/util.h>
#include <misc/mem.h>

#include <misc/gfx_util.h>

#include <gfx/convert.h>


#define SUBSAMPLE_BITS 4
#define SUBSAMPLE (1 << SUBSAMPLE_BITS)
#define SUBSAMPLE_MASK ((1 << SUBSAMPLE_BITS)-1)
#define SCALE_SHIFT 16


typedef struct _PixopsFilter PixopsFilter;

struct _PixopsFilter {
     int *weights;
     int n_x;
     int n_y;
     double x_offset;
     double y_offset;
};


static inline void rgba_to_dst_format (__u8 *dst,
                                       __u32 r, __u32 g, __u32 b, __u32 a,
                                       DFBSurfacePixelFormat dst_format,
                                       CorePalette *palette)
{
     __u32 out_pixel;

     switch (dst_format) {

#ifdef SUPPORT_RGB332
     case DSPF_RGB332:
          *((__u8*)dst) = PIXEL_RGB332( r, g, b );
          break;
#endif

     case DSPF_A8:
          *((__u8*)dst) = a;
          break;

     case DSPF_ARGB:
          *((__u32*)dst) = PIXEL_ARGB( a, r, g, b );
          break;

     case DSPF_RGB32:
          *((__u32*)dst) = PIXEL_RGB32( r, g, b );
          break;

     case DSPF_RGB15:
          out_pixel  = b;
          out_pixel |= g << 8;
          out_pixel |= r << 16;
          *(__u16 *)dst = RGB32_TO_RGB15 (out_pixel);
          break;

     case DSPF_RGB16:
          out_pixel  = b;
          out_pixel |= g << 8;
          out_pixel |= r << 16;
          *(__u16 *)dst = RGB32_TO_RGB16 (out_pixel);
          break;

     case DSPF_RGB24:
          *dst++ = b;
          *dst++ = g;
          *dst   = r;
          break;

     case DSPF_LUT8:
          if (palette)
               *dst++ = dfb_palette_search( palette, r, g, b, a );
          break;

     default:
          ONCE( "unimplemented destination format" );
          break;
     }
}

void dfb_copy_buffer_32( void *dst, __u32 *src, int w, int h, int dpitch,
                         DFBSurfacePixelFormat dst_format, CorePalette *palette )
{
     int x, y;
     int dskip;
     __u32 rb, a;

     dskip = dpitch - DFB_BYTES_PER_LINE (dst_format, w);

     switch (dst_format) {
          case DSPF_A8:
               for (y = 0; y < h; y++) {
                    for (x = 0; x < w; x++) {
                         *(__u8 *)dst++ = *src >> 24;
                         src++;
                    }
                    (__u8 *)dst += dskip;
               }
               break;

          case DSPF_ARGB:
               for (y = 0; y < h; y++) {
                    dfb_memcpy (dst, src, w * 4);
                    (__u8 *)dst += w * 4 + dskip;
                    src += w;
               }
               break;

          default:
               for (y = 0; y < h; y++) {
                    for (x = 0; x < w; x++) {
                         a = *src >> 24;
                         switch (a) {
                         case 0x0:
                              memset ((__u8 *)dst, 0, DFB_BYTES_PER_PIXEL (dst_format));
                              break;
                         case 0xFF:
                              rgba_to_dst_format ((__u8 *)dst,
                                                  (*src & 0x00FF0000) >> 16,
                                                  (*src & 0x0000FF00) >> 8,
                                                  (*src & 0x000000FF),
                                                  0xFF, dst_format, palette);
                              break;
                         default:
                              rb = (*src & 0x00FF00FF) * a;
                              rgba_to_dst_format ((__u8 *)dst,
                                                  rb >> 24,
                                                  ((*src & 0x0000FF00) * a) >> 16,
                                                  (rb & 0x0000FF00) >> 8,
                                                  a, dst_format, palette);
                              break;
                         }
                         (__u8 *)dst += DFB_BYTES_PER_PIXEL (dst_format);
                         src++;
                    }
                    (__u8 *)dst += dskip;
               }
               break;
     }
}

static int bilinear_make_fast_weights( PixopsFilter *filter, double x_scale,
                                                             double y_scale )
{
     int i_offset, j_offset;
     double *x_weights, *y_weights;
     int n_x, n_y;

     if (x_scale > 1.0) {      /* Bilinear */
          n_x = 2;
          filter->x_offset = 0.5 * (1/x_scale - 1);
     }
     else {                    /* Tile */
          n_x = DFB_ICEIL (1.0 + 1.0 / x_scale);
          filter->x_offset = 0.0;
     }

     if (y_scale > 1.0) {      /* Bilinear */
          n_y = 2;
          filter->y_offset = 0.5 * (1/y_scale - 1);
     }
     else {                    /* Tile */
          n_y = DFB_ICEIL (1.0 + 1.0/y_scale);
          filter->y_offset = 0.0;
     }

     filter->n_y = n_y;
     filter->n_x = n_x;
     filter->weights = (int *) DFBMALLOC(SUBSAMPLE * SUBSAMPLE * n_x * n_y *
                                       sizeof (int));
     if (!filter->weights) {
          DEBUGMSG ("couldn't allocate memory for scaling\n");
          return 0;
     }

     x_weights = (double *) alloca (n_x * sizeof (double));
     y_weights = (double *) alloca (n_y * sizeof (double));

     if (!x_weights || !y_weights) {
          DFBFREE( filter->weights );

          DEBUGMSG ("couldn't allocate memory for scaling\n");
          return 0;
     }

     for (i_offset = 0; i_offset < SUBSAMPLE; i_offset++)
          for (j_offset = 0; j_offset < SUBSAMPLE; j_offset++) {
               int *pixel_weights = filter->weights
                                    + ((i_offset * SUBSAMPLE) + j_offset)
                                    * n_x * n_y;

               double x = (double)j_offset / 16;
               double y = (double)i_offset / 16;
               int i,j;

               if (x_scale > 1.0) {     /* Bilinear */
                    for (i = 0; i < n_x; i++) {
                         x_weights[i] = ((i == 0) ? (1 - x) : x) / x_scale;
                    }
               }
               else {                   /* Tile */
                    for (i = 0; i < n_x; i++) {
                         if (i < x) {
                              if (i + 1 > x)
                                   x_weights[i] = MIN( i+ 1, x+ 1/x_scale ) -x;
                              else
                                   x_weights[i] = 0;
                         }
                         else {
                              if (x + 1/x_scale > i)
                                   x_weights[i] = MIN( i+ 1, x+ 1/x_scale ) -i;
                              else
                                   x_weights[i] = 0;
                         }
                    }
               }

               if (y_scale > 1.0) {     /* Bilinear */
                    for (i = 0; i < n_y; i++) {
                         y_weights[i] = ((i == 0) ? (1 - y) : y) / y_scale;
                    }
               }
               else {                   /* Tile */
                    for (i = 0; i < n_y; i++) {
                         if (i < y) {
                              if (i + 1 > y)
                                   y_weights[i] = MIN( i+ 1, y+ 1/y_scale ) -y;
                              else
                                   y_weights[i] = 0;
                         }
                         else {
                              if (y + 1/y_scale > i)
                                   y_weights[i] = MIN( i+ 1, y+ 1/y_scale ) -i;
                              else
                                   y_weights[i] = 0;
                         }
                    }
               }

               for (i = 0; i < n_y; i++) {
                    for (j = 0; j < n_x; j++) {
                         *(pixel_weights + n_x * i + j) =
                                           65536 * x_weights[j] * x_scale
                                                 * y_weights[i] * y_scale;
                    }
               }
          }

     return 1;
}

static void scale_pixel( int *weights, int n_x, int n_y,
                         void *dst, __u32 **src,
                         int x, int sw, DFBSurfacePixelFormat dst_format,
                         CorePalette *palette )
{
     __u32 r = 0, g = 0, b = 0, a = 0;
     int i, j;

     for (i = 0; i < n_y; i++) {
          int *pixel_weights = weights + n_x * i;

          for (j = 0; j < n_x; j++) {
               __u32  ta;
               __u32 *q;

               if (x + j < 0)
                    q = src[i];
               else if (x + j < sw)
                    q = src[i] + x + j;
               else
                    q = src[i] + sw - 1;

               ta = ((*q & 0xFF000000) >> 24) * pixel_weights[j];

               b += ta * ((*q & 0xFF));
               g += ta * ((*q & 0xFF00) >> 8);
               r += ta * ((*q & 0xFF0000) >> 16);
               a += ta;
          }
      }

     r = (r >> 24) == 0xFF ? 0xFF : (r + 0x800000) >> 24;
     g = (g >> 24) == 0xFF ? 0xFF : (g + 0x800000) >> 24;
     b = (b >> 24) == 0xFF ? 0xFF : (b + 0x800000) >> 24;
     a = (a >> 16) == 0xFF ? 0xFF : (a + 0x8000) >> 16;

     rgba_to_dst_format( dst, r, g, b, a, dst_format, palette );
}

static char *scale_line( int *weights, int n_x, int n_y, __u8 *dst,
                         __u8 *dst_end, __u32 **src, int x, int x_step, int sw,
                         DFBSurfacePixelFormat dst_format, CorePalette *palette )
{
     int i, j;
     int *pixel_weights;
     __u32 *q;
     __u32 r, g, b, a;
     int  x_scaled;
     int *line_weights;

     while (dst < dst_end) {
          r = g = b = a = 0;
          x_scaled = x >> SCALE_SHIFT;

          pixel_weights = weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS))
                                     & SUBSAMPLE_MASK) * n_x * n_y;

          for (i = 0; i < n_y; i++) {
               line_weights = pixel_weights + n_x * i;

               q = src[i] + x_scaled;

               for (j = 0; j < n_x; j++) {
                    __u32 ta;

                    ta = ((*q & 0xFF000000) >> 24) * line_weights[j];

                    b += ta * ((*q & 0xFF));
                    g += ta * ((*q & 0xFF00) >> 8);
                    r += ta * ((*q & 0xFF0000) >> 16);
                    a += ta;

                    q++;
               }
          }

          r = (r >> 24) == 0xFF ? 0xFF : (r + 0x800000) >> 24;
          g = (g >> 24) == 0xFF ? 0xFF : (g + 0x800000) >> 24;
          b = (b >> 24) == 0xFF ? 0xFF : (b + 0x800000) >> 24;
          a = (a >> 16) == 0xFF ? 0xFF : (a + 0x8000) >> 16;

          rgba_to_dst_format( dst, r, g, b, a, dst_format, palette );

          dst += DFB_BYTES_PER_PIXEL (dst_format);
          x += x_step;
     }

     return dst;
}

void dfb_scale_linear_32( void *dst, __u32 *src, int sw, int sh,
                          int dw, int dh, int dpitch,
                          DFBSurfacePixelFormat dst_format,
                          CorePalette *palette )
{
     double scale_x, scale_y;
     int i, j;
     int x, y;
     int x_step, y_step;
     int scaled_x_offset;
     int dskip;
     PixopsFilter filter;

     if (sw < 1 || sh < 1 || dw < 1 || dh < 1)
          return;

     if (dw == sw && dh == sh) {
          dfb_copy_buffer_32( dst, src, sw, sh, dpitch, dst_format, palette );
          return;
     }

     scale_x = (double)dw / sw;
     scale_y = (double)dh / sh;

     x_step = (1 << SCALE_SHIFT) / scale_x;
     y_step = (1 << SCALE_SHIFT) / scale_y;

     if (! bilinear_make_fast_weights( &filter, scale_x, scale_y ))
          return;

     dskip = dpitch - DFB_BYTES_PER_LINE (dst_format, dw);

     scaled_x_offset = DFB_IFLOOR( filter.x_offset * (1 << SCALE_SHIFT) );
     y = DFB_IFLOOR( filter.y_offset * (1 << SCALE_SHIFT) );

     for (i = 0; i < dh; i++) {
          int x_start;
          int y_start;
          int dest_x;
          int *run_weights;
          void *outbuf;
          void *outbuf_end;
          void *new_outbuf;
          __u32 **line_bufs;

          y_start = y >> SCALE_SHIFT;

          run_weights = filter.weights + ((y >> (SCALE_SHIFT - SUBSAMPLE_BITS))
                        & SUBSAMPLE_MASK) * filter.n_x * filter.n_y * SUBSAMPLE;

          line_bufs = (__u32 **) alloca( filter.n_y * sizeof (void *) );

          for (j = 0; j < filter.n_y; j++) {
               if (y_start <  0)
                    line_bufs[j] = src;
               else if (y_start < sh)
                    line_bufs[j] = src + sw * y_start;
               else
                    line_bufs[j] = src + sw * (sh - 1);

               y_start++;
          }

          (__u8 *)outbuf =
          dst + i * (DFB_BYTES_PER_PIXEL (dst_format) * dw + dskip);
          (__u8 *)outbuf_end = outbuf + DFB_BYTES_PER_PIXEL (dst_format) * dw;
          x = scaled_x_offset;
          x_start = x >> SCALE_SHIFT;
          dest_x = 0;

          while (x_start < 0 && outbuf < outbuf_end) {
               scale_pixel( run_weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS))
                            & SUBSAMPLE_MASK) * (filter.n_x * filter.n_y),
                            filter.n_x, filter.n_y, outbuf, line_bufs,
                            x >> SCALE_SHIFT, sw, dst_format, palette );

               x += x_step;
               x_start = x >> SCALE_SHIFT;
               dest_x++;
               (__u8 *)outbuf += DFB_BYTES_PER_PIXEL (dst_format);
          }

          new_outbuf = scale_line (run_weights, filter.n_x, filter.n_y, outbuf,
                                   outbuf_end, line_bufs, x >> SCALE_SHIFT,
                                   x_step, sw, dst_format, palette);

          dest_x += (new_outbuf - outbuf) / DFB_BYTES_PER_PIXEL (dst_format);
          x = dest_x * x_step + scaled_x_offset;
          outbuf = new_outbuf;

          while (outbuf < outbuf_end) {
               scale_pixel( run_weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS))
                            & SUBSAMPLE_MASK) * (filter.n_x * filter.n_y),
                            filter.n_x, filter.n_y, outbuf, line_bufs,
                            x >> SCALE_SHIFT, sw, dst_format, palette);

               x += x_step;
               (__u8 *)outbuf += DFB_BYTES_PER_PIXEL (dst_format);
          }

          y += y_step;
     }

     DFBFREE(filter.weights);
}

