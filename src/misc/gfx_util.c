/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include "directfb.h"

#include "core/core.h"
#include "core/coredefs.h"
#include "core/coretypes.h"

#include "core/surfaces.h"

#include "gfx_util.h"
#include "util.h"
#include "mem.h"

#include <gfx/convert.h>


#define REGION_CODE(x,y,cx1,cx2,cy1,cy2) ( ( (y) > (cy2) ? 8 : 0) | \
                                           ( (y) < (cy1) ? 4 : 0) | \
                                           ( (x) > (cx2) ? 2 : 0) | \
                                           ( (x) < (cx1) ? 1 : 0) )

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
                                       DFBSurfacePixelFormat dst_format)
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

     default:
          ERRORMSG( "DirectFB/misc/gfx_util: unimplemented format!\n" );
          break;
     }
}

void copy_buffer_32( void *dst, __u32 *src, int w, int h, int dskip,
                     DFBSurfacePixelFormat dst_format )
{
     int x, y;
     __u32 rb, a;

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
                    memcpy (dst, src, w * 4);
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
                                                  0xFF, dst_format);
                              break;
                         default:
                              rb = (*src & 0x00FF00FF) * a;
                              rgba_to_dst_format ((__u8 *)dst, 
                                                  rb >> 24,
                                                  ((*src & 0x0000FF00) * a) >> 16,
                                                  (rb & 0x0000FF00) >> 8, 
                                                  a, dst_format);
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
          n_x = ICEIL (1.0 + 1.0 / x_scale);
          filter->x_offset = 0.0;
     }

     if (y_scale > 1.0) {      /* Bilinear */
          n_y = 2;
          filter->y_offset = 0.5 * (1/y_scale - 1);
     }
     else {                    /* Tile */
          n_y = ICEIL (1.0 + 1.0/y_scale);
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

               for (i = 0; i < n_y; i++)
                    for (j = 0; j < n_x; j++) {
                         *(pixel_weights + n_x * i + j) =
                                           65536 * x_weights[j] * x_scale
                                                 * y_weights[i] * y_scale;
                    }
          }

     return 1;
}

static void scale_pixel( int *weights, int n_x, int n_y, void *dst, __u32 **src,
                         int x, int sw, DFBSurfacePixelFormat dst_format )
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

     rgba_to_dst_format( dst, r >> 24, g >> 24, b >> 24, a >> 16, dst_format );
}

static char *scale_line( int *weights, int n_x, int n_y, __u8 *dst,
                         __u8 *dst_end, __u32 **src, int x, int x_step, int sw,
                         DFBSurfacePixelFormat dst_format )
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
                    
                    b+= ta * ((*q & 0xFF));
                    g+= ta * ((*q & 0xFF00) >> 8);
                    r+= ta * ((*q & 0xFF0000) >> 16);
                    a += ta;
                    
                    q++;
               }
          }
  
          rgba_to_dst_format( dst, 
                              r >> 24, g >> 24, b >> 24, a >> 16, dst_format );

          dst += DFB_BYTES_PER_PIXEL (dst_format);
          x += x_step;
     }

     return dst;
}

void scale_linear_32( void *dst, __u32 *src, int sw, int sh, int dw, int dh,
                      int dskip, DFBSurfacePixelFormat dst_format )
{
     double scale_x, scale_y;
     int i, j;
     int x, y;
     int x_step, y_step;
     int scaled_x_offset;
     PixopsFilter filter;

     if (sw < 1 || sh < 1 || dw < 1 || dh < 1)
          return;

     if (dw == sw && dh == sh) {
          copy_buffer_32( dst, src, sw, sh, dskip, dst_format );
          return;
     }

     scale_x = (double)dw / sw;
     scale_y = (double)dh / sh;

     x_step = (1 << SCALE_SHIFT) / scale_x;
     y_step = (1 << SCALE_SHIFT) / scale_y;

     if (! bilinear_make_fast_weights( &filter, scale_x, scale_y ))
          return;

     scaled_x_offset = IFLOOR( filter.x_offset * (1 << SCALE_SHIFT) );
     y = IFLOOR( filter.y_offset * (1 << SCALE_SHIFT) );

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
                            x >> SCALE_SHIFT, sw, dst_format );

               x += x_step;
               x_start = x >> SCALE_SHIFT;
               dest_x++;
               (__u8 *)outbuf += DFB_BYTES_PER_PIXEL (dst_format);
          }

          new_outbuf = scale_line (run_weights, filter.n_x, filter.n_y, outbuf,
                                   outbuf_end, line_bufs, x >> SCALE_SHIFT,
                                   x_step, sw, dst_format);

          dest_x += (new_outbuf - outbuf) / DFB_BYTES_PER_PIXEL (dst_format);
          x = dest_x * x_step + scaled_x_offset;
          outbuf = new_outbuf;

          while (outbuf < outbuf_end) {
               scale_pixel( run_weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS))
                            & SUBSAMPLE_MASK) * (filter.n_x * filter.n_y),
                            filter.n_x, filter.n_y, outbuf, line_bufs,
                            x >> SCALE_SHIFT, sw, dst_format);

               x += x_step;
               (__u8 *)outbuf += DFB_BYTES_PER_PIXEL (dst_format);
          }

          y += y_step;
     }

     DFBFREE(filter.weights);
}

int clip_line( DFBRegion *clip, DFBRegion *line )
{
     unsigned char region_code1 = REGION_CODE( line->x1, line->y1,
                                               clip->x1,
                                               clip->x2,
                                               clip->y1,
                                               clip->y2 );

     unsigned char region_code2 = REGION_CODE( line->x2, line->y2,
                                               clip->x1,
                                               clip->x2,
                                               clip->y1,
                                               clip->y2 );

     while (region_code1 | region_code2) {
          if (region_code1 & region_code2)

               return 0;  /* line completely outside the clipping rectangle */


          if (region_code1) {
               if (region_code1 & 8) { /* divide line at bottom*/
                    line->x1 = line->x1 +(line->x2-line->x1) * (clip->y2 - line->y1) / (line->y2-line->y1);
                    line->y1 = clip->y2;
               }
               else
                    if (region_code1 & 4) { /* divide line at top*/
                    line->x1 = line->x1 +(line->x2-line->x1) * (clip->y1 - line->y1) / (line->y2-line->y1);
                    line->y1 = clip->y1;
               }
               else
                    if (region_code1 & 2) { /* divide line at right*/
                    line->y1 = line->y1 +(line->y2-line->y1) * (clip->x2 - line->x1) / (line->x2-line->x1);
                    line->x1 = clip->x2;
               }
               else
                    if (region_code1 & 1) { /* divide line at right*/
                    line->y1 = line->y1 +(line->y2-line->y1) * (clip->x1 - line->x1) / (line->x2-line->x1);
                    line->x1 = clip->x1;
               }
               region_code1 = REGION_CODE( line->x1, line->y1,
                                           clip->x1,
                                           clip->x2,
                                           clip->y1,
                                           clip->y2 );
          }
          else {
               if (region_code2 & 8) {  /* divide line at bottom*/
                    line->x2 = line->x1 +(line->x2-line->x1) * (clip->y2 - line->y1) / (line->y2-line->y1);
                    line->y2 = clip->y2;
               }
               else
                    if (region_code2 & 4) { /* divide line at top*/
                    line->x2 = line->x1 +(line->x2-line->x1) * (clip->y1 - line->y1) / (line->y2-line->y1);
                    line->y2 = clip->y1;
               }
               else
                    if (region_code2 & 2) { /* divide line at right*/
                    line->y2 = line->y1 +(line->y2-line->y1) * (clip->x2 - line->x1) / (line->x2-line->x1);
                    line->x2 = clip->x2;
               }
               else
                    if (region_code2 & 1) { /* divide line at right*/
                    line->y2 = line->y1 +(line->y2-line->y1) * (clip->x1 - line->x1) / (line->x2-line->x1);
                    line->x2 = clip->x1;
               }
               region_code2 = REGION_CODE( line->x2, line->y2, clip->x1,
                                                     clip->x2,
                                                     clip->y1,
                                                     clip->y2 );
          }
     }

     return 1; /* successfully clipped or clipping not neccessary */
}

unsigned int clip_rectangle( DFBRegion    *clip,
                             DFBRectangle *rect )
{
     unsigned int result = 0xF;  /* returns bit flags for clipped edges  */

     if ((clip->x1 >= rect->x + rect->w) ||
         (clip->x2 < rect->x) ||
         (clip->y1 >= rect->y + rect->h) ||
         (clip->y2 < rect->y)) {
          return 0;
     }

     if (clip->x1 > rect->x) {
          rect->w += rect->x - clip->x1;
          rect->x = clip->x1;
          result &= ~1;
     }

     if (clip->y1 > rect->y) {
          rect->h += rect->y - clip->y1;
          rect->y = clip->y1;
          result &= ~2;
     }

     if (clip->x2 < rect->x + rect->w - 1) {
          rect->w = clip->x2 - rect->x + 1;
          result &= ~4;
     }

     if (clip->y2 < rect->y + rect->h - 1) {
          rect->h = clip->y2 - rect->y + 1;
          result &= ~8;
     }

     return result;
}

int clip_triangle_precheck( DFBRegion   *clip,
                            DFBTriangle *tri )
{
    int x, y, w, h;
  
    x = MIN (MIN (tri->x1, tri->x2), tri->x3);
    y = MIN (MIN (tri->y1, tri->y2), tri->y3);
    w = MAX (MAX (tri->x1, tri->x2), tri->x3) - x;
    h = MAX (MAX (tri->y1, tri->y2), tri->y3) - y;
    
    if (clip->x1 > x ||
        clip->x2 < x + w ||
        clip->y1 > y ||
        clip->y2 < y + h)
      return 0;
    
    return 1;
}

int clip_blit_precheck( DFBRegion *clip,
                        int w, int h, int dx, int dy )
{
     if (w < 1 || h < 1 ||
         (clip->x1 >= dx + w) ||
         (clip->x2 < dx) ||
         (clip->y1 >= dy + h) ||
         (clip->y2 < dy))
     {
          return 0;
     }

     return 1;
}

void clip_blit( DFBRegion *clip, DFBRectangle *srect,
                int *dx, int *dy )
{
     if (clip->x1 > *dx ) {
          srect->w = MIN( (clip->x2 - clip->x1) + 1,
                    (*dx + srect->w) - clip->x1);

          srect->x+= clip->x1 - *dx;
          *dx = clip->x1;
     }
     else if (clip->x2 < *dx + srect->w - 1) {
          srect->w = clip->x2 - *dx + 1;
     }

     if (clip->y1 > *dy ) {
          srect->h = MIN( (clip->y2 - clip->y1) + 1,
                          (*dy + srect->h) - clip->y1);
          srect->y+= clip->y1 - *dy;
          *dy = clip->y1;
     }
     else if (clip->y2 < *dy + srect->h - 1) {
          srect->h = clip->y2 - *dy + 1;
     }
}

void clip_stretchblit( DFBRegion *clip,
                       DFBRectangle *srect, DFBRectangle *drect )
{
     DFBRectangle orig_dst = *drect;

     clip_rectangle( clip, drect );

     if (drect->x != orig_dst.x)
          srect->x += (int)( (drect->x - orig_dst.x) *
                             (srect->w / (float)orig_dst.w) + 0.5f );

     if (drect->y != orig_dst.y)
          srect->y += (int)( (drect->y - orig_dst.y) *
                             (srect->h / (float)orig_dst.h) + 0.5f );
     
     if (drect->w != orig_dst.w)
          srect->w = ICEIL(srect->w * (drect->w / (float)orig_dst.w));

     if (drect->h != orig_dst.h)
          srect->h = ICEIL(srect->h * (drect->h / (float)orig_dst.h));
}

