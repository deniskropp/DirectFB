/*
   (C) Copyright 2007 Claudio Ciccani <klan@directfb.org>.

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
   
   DVC - DirectFB Video Converter
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <directfb.h>
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/memcpy.h>
#include <direct/util.h>

#include <core/palette.h>
#include <core/surface.h>

#include <display/idirectfbsurface.h>

#include <gfx/clip.h>
#include <gfx/convert.h>

#include <misc/conf.h>

#include "dvc.h"


/*****************************************************************************/

unsigned int
dvc_picture_size( DVCPixelFormat f, int w, int h )
{
     unsigned int size = 0;
     int          i;
     
     D_ASSERT( w > 0 );
     D_ASSERT( h > 0 );
     
     for (i = 0; i < DVC_NUM_PLANES(f); i++) {
          int pitch;
          pitch = ((w + (DVC_PLANE_ALIGN(f,i)-1)) >> DVC_PLANE_H_SHIFT(f,i));
          pitch = (pitch * DVC_PLANE_BPS(f,i) + 7) & ~7;
          size += pitch * (h >> DVC_PLANE_V_SHIFT(f,i));
     }
     
     return size;
}     

void
dvc_picture_init( DVCPicture     *picture,
                  DVCPixelFormat  f,
                  int             w, 
                  int             h, 
                  void           *base )
{
     int i;
     
     D_ASSERT( picture != NULL );
     D_ASSERT( w > 0 );
     D_ASSERT( h > 0 );
     D_ASSERT( base != NULL );
     
     picture->format = f;
     picture->width  = w;
     picture->height = h;
     
     picture->base[0] = base;
     picture->pitch[0] = (((w + (DVC_PLANE_ALIGN(f,0)-1)) >> DVC_PLANE_H_SHIFT(f,0))
                              * DVC_PLANE_BPS(f,0) + 7) & ~7;
     for (i = 1; i < DVC_NUM_PLANES(f); i++) {
          picture->base[i] = picture->base[i-1] + 
                             picture->pitch[i-1] * (h >> DVC_PLANE_V_SHIFT(f,i-1));
          picture->pitch[i] = (((w + (DVC_PLANE_ALIGN(f,i)-1)) >> DVC_PLANE_H_SHIFT(f,i))
                                   * DVC_PLANE_BPS(f,i) + 7) & ~7;
     }
     
     picture->palette = NULL;
     picture->palette_size = 0;
     
     picture->separated = 
     picture->premultiplied = 0;
}

/*****************************************************************************/

void
dvc_colormap_gen( DVCColormap    *colormap,
                  DVCPixelFormat  format,
                  u16             brightness,
                  u16             contrast,
                  u16             saturation )
{
     int i, b, c, s;
     
     D_ASSERT( colormap != NULL );
     
     b = (brightness >> 8) - 128;
     c = contrast;
     s = saturation;
     
     if (DVC_YCBCR_PIXELFORMAT( format )) {
          for (i = 0; i < 256; i++) {
               int luma, chroma;
               
               luma   = (((i -  16) * c) >> 15) + b + 16;
               chroma = (((i - 128) * s) >> 15) + 128;
               colormap->YUV.y[i] = CLAMP( luma, 0, 255 );
               colormap->YUV.u[i] =
               colormap->YUV.v[i] = CLAMP( chroma, 0, 255 );
          }
     }
     else {
          float gray, color;
          
          if (s > 32768) {
               gray  = ((float)s - 32768.f) / 32768.f;
               color = MAX( 1.f - gray, 0.5f );
          } else {
               color = (float)s / 32768.f;
               gray  = 1.f - color;
          }
          
          for (i = 0; i < 256; i++) {
               int value;
               
               value = (s > 32768)
                       ? (((float)i - 128.f * gray) / color)
                       : (((float)i * color) + 128.f * gray);
               value = ((value * c) >> 15) + b;
               colormap->RGB.r[i] =
               colormap->RGB.g[i] =
               colormap->RGB.b[i] = CLAMP( value, 0, 255 );
          }
     }
}

/*****************************************************************************/

typedef union {
     struct {
          u8 b;
          u8 g;
          u8 r;
          u8 a;
     } RGB;
     struct {
          u8 v;
          u8 u;
          u8 y;
          u8 a;
     } YUV;
} __attribute__((packed)) DVCColor;

typedef struct {
     void        *src_base[3];
     void        *src[3];
     int          sx, sy;
     int          sw, sh;
     
     void        *dst_base[3];
     void        *dst[3];
     int          dx, dy;
     int          dw, dh;
     
     DFBColor    *slut;
     int          slut_size;
     DFBColor    *dlut;
     int          dlut_size;
     
     DVCColormap *colormap;

     DVCColor    *buf[2];
     
     int          h_scale;
     int          v_scale;
     int          s_v;
} DVCContext;

typedef void (*DVCFunction) ( DVCContext *ctx );
  
/*****************************************************************************/

/* lookup tables for 2/3bit to 8bit color conversion (from DirectFB/src/gfx/generic.c) */
static const u8 expand3to8[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff};
static const u8 expand2to8[] = { 0x00, 0x55, 0xaa, 0xff};

static __inline__ u8 expand5to8( u8 s ) {
     return (s << 3) | (s >> 2);
}

static __inline__ u8 expand6to8( u8 s ) {
     return (s << 2) | (s >> 4);
}

/*****************************************************************************/

/* 2x2 dithering tables */
static const u8 dither5t[2][2] = { { 2, 6 }, { 4, 0 } };
static const u8 dither6t[2][2] = { { 3, 1 }, { 0, 2 } };

#define dither5_init( y )  const u8 *const d5x = dither5t[(y)&1]

#define dither6_init( y )  const u8 *const d6x = dither6t[(y)&1]

#define dither5( s, x ) (((s) < 0xf8) ? ((u8)((s)+d5x[(x)&1]) & 0xf8) : 0xf8)

#define dither6( s, x ) (((s) < 0xfc) ? ((u8)((s)+d6x[(x)&1]) & 0xfc) : 0xfc)

/*****************************************************************************/

static __inline__ u16 le16( u16 s ) {
#if __BYTE_ORDER == __BIG_ENDIAN
     return BSWAP16(s);
#else
     return s;
#endif
}

static __inline__ u16 be16( u16 s ) {
#if __BYTE_ORDER == __BIG_ENDIAN
     return s;
#else
     return BSWAP16(s);
#endif
}

static __inline__ u32 le32( u32 s ) {
#if __BYTE_ORDER == __BIG_ENDIAN
     return BSWAP32(s);
#else
     return s;
#endif
}

static __inline__ u32 be32( u32 s ) {
#if __BYTE_ORDER == __BIG_ENDIAN
     return s;
#else
     return BSWAP32(s);
#endif
}


/*****************************************************************************/

static void Load_I8( DVCContext *ctx )
{
     u8       *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     DFBColor *P = ctx->slut;
     int       w = ctx->sw;
     
     for (; w; w--) {
          u8 s = *S++;
          D->RGB.r = P[s].r;
          D->RGB.g = P[s].g;
          D->RGB.b = P[s].b;
          D++;
     }
}

static void Load_AI8( DVCContext *ctx )
{
     u8       *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     DFBColor *P = ctx->slut;
     int       w = ctx->sw;
     
     for (; w; w--) {
          u8 s = *S++;
          D->RGB.a = P[s].a;
          D->RGB.r = P[s].r;
          D->RGB.g = P[s].g;
          D->RGB.b = P[s].b;
          D++;
     }
}

static void Load_A4I4( DVCContext *ctx )
{
     u8       *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     DFBColor *P = ctx->slut;
     int       w = ctx->sw;
     
     for (; w; w--) {
          u8 s = *S++;
          D->RGB.a = (s & 0xf0) | (s >> 4);
          D->RGB.r = P[s&0xf].r;
          D->RGB.g = P[s&0xf].g;
          D->RGB.b = P[s&0xf].b;
          D++;
     }
}

static void Load_A8I8_LE( DVCContext *ctx )
{
     u16       *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     DFBColor *P = ctx->slut;
     int       w = ctx->sw;
     
     for (; w; w--) {
          u16 s = le16(*S++);
          D->RGB.a = s >> 8;
          D->RGB.r = P[s&0xff].r;
          D->RGB.g = P[s&0xff].g;
          D->RGB.b = P[s&0xff].b;
          D++;
     }
}

static void Load_A8I8_BE( DVCContext *ctx )
{
     u16       *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     DFBColor *P = ctx->slut;
     int       w = ctx->sw;
     
     for (; w; w--) {
          u16 s = be16(*S++);
          D->RGB.a = s >> 8;
          D->RGB.r = P[s&0xff].r;
          D->RGB.g = P[s&0xff].g;
          D->RGB.b = P[s&0xff].b;
          D++;
     }
}

static void Load_Y8( DVCContext *ctx )
{
     u8       *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          D->YUV.y = *S++;
          D->YUV.u = 128;
          D->YUV.v = 128;
          D++;
     }
}

static void Load_RGB8( DVCContext *ctx )
{
     u8       *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          u8 s = *S++;
          D->RGB.r = expand3to8[(s       ) >> 5];
          D->RGB.g = expand3to8[(s & 0x1c) >> 2];
          D->RGB.b = expand2to8[(s & 0x03)     ];
          D++;
     }
}

static void Load_RGB15_LE( DVCContext *ctx )
{
     u16      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          u16 s = le16(*S++);
          D->RGB.r = expand5to8((s & 0x7c00) >> 10);
          D->RGB.g = expand5to8((s & 0x03e0) >>  5);
          D->RGB.b = expand5to8((s & 0x001f)      );
          D++;
     }
}

static void Load_RGB15_BE( DVCContext *ctx )
{
     u16      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          u16 s = be16(*S++);
          D->RGB.r = expand5to8((s & 0x7c00) >> 10);
          D->RGB.g = expand5to8((s & 0x03e0) >>  5);
          D->RGB.b = expand5to8((s & 0x001f)      );
          D++;
     }
}

static void Load_RGB16_LE( DVCContext *ctx )
{
     u16      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          u16 s = le16(*S++);
          D->RGB.r = expand5to8((s         ) >> 11);
          D->RGB.g = expand6to8((s & 0x07e0) >>  5);
          D->RGB.b = expand5to8((s & 0x001f)      );
          D++;
     }
}

static void Load_RGB16_BE( DVCContext *ctx )
{
     u16      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          u16 s = be16(*S++);
          D->RGB.r = expand5to8((s         ) >> 11);
          D->RGB.g = expand6to8((s & 0x07e0) >>  5);
          D->RGB.b = expand5to8((s & 0x001f)      );
          D++;
     }
}

static void Load_RGB24_LE( DVCContext *ctx )
{
     u8       *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          D->RGB.r = S[2];
          D->RGB.g = S[1];
          D->RGB.b = S[0];
          S += 3;
          D++;
     }
}

static void Load_RGB24_BE( DVCContext *ctx )
{
     u8       *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          D->RGB.r = S[0];
          D->RGB.g = S[1];
          D->RGB.b = S[2];
          S += 3;
          D++;
     }
}

static void Load_RGB32_LE( DVCContext *ctx )
{
     u32      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          u32 s = le32(*S++);
          D->RGB.r = s >> 16;
          D->RGB.g = s >>  8;
          D->RGB.b = s;
          D++;
     }
}

static void Load_RGB32_BE( DVCContext *ctx )
{
     u32      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          u32 s = be32(*S++);
          D->RGB.r = s >> 16;
          D->RGB.g = s >>  8;
          D->RGB.b = s;
          D++;
     }
}

static void Load_ARGB_LE( DVCContext *ctx )
{
     u32      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     direct_memcpy( D, S, w*4 );
}

static void Load_ARGB_BE( DVCContext *ctx )
{
     u32      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          u32 s = be32(*S++);
          D->RGB.a = s >> 24;
          D->RGB.r = s >> 16;
          D->RGB.g = s >>  8;
          D->RGB.b = s;
          D++;
     }
}

static void Load_RGBA_LE( DVCContext *ctx )
{
     u32      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          u32 s = le32(*S++);
          D->RGB.a = s;
          D->RGB.r = s >> 24;
          D->RGB.g = s >> 16;
          D->RGB.b = s >>  8;
          D++;
     }
}

static void Load_RGBA_BE( DVCContext *ctx )
{
     u32      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          u32 s = be32(*S++);
          D->RGB.a = s;
          D->RGB.r = s >> 24;
          D->RGB.g = s >> 16;
          D->RGB.b = s >>  8;
          D++;
     }
}

static void Load_YUYV_LE( DVCContext *ctx )
{
     u32      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw >> 1;
     
     for (; w; w--) {
          u32 s = *S++;
#if __BYTE_ORDER == __BIG_ENDIAN
          D[0].YUV.y = s >> 24;
          D[1].YUV.y = s >>  8;
          D[0].YUV.u = 
          D[1].YUV.u = s >> 16;
          D[0].YUV.v = 
          D[1].YUV.v = s;
#else
          D[0].YUV.y = s;
          D[1].YUV.y = s >> 16;
          D[0].YUV.u = 
          D[1].YUV.u = s >>  8;
          D[0].YUV.v = 
          D[1].YUV.v = s >> 24;
#endif
          D += 2;
     }
}

static void Load_YUYV_BE( DVCContext *ctx )
{
     u32      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw >> 1;
     
     for (; w; w--) {
          u32 s = *S++;
#if __BYTE_ORDER == __BIG_ENDIAN
          D[0].YUV.y = s >> 16;
          D[1].YUV.y = s;
          D[0].YUV.u = 
          D[1].YUV.u = s >> 24;
          D[0].YUV.v = 
          D[1].YUV.v = s >>  8;
#else
          D[0].YUV.y = s >>  8;
          D[1].YUV.y = s >> 24;
          D[0].YUV.u = 
          D[1].YUV.u = s;
          D[0].YUV.v = 
          D[1].YUV.v = s >> 16;
#endif
          D += 2;
     }
}

static void Load_NV12_LE( DVCContext *ctx )
{
     u8       *Sy  = ctx->src[0];
     u16      *Suv = ctx->src[1];
     DVCColor *D   = ctx->buf[0];
     int       w   = ctx->sw;
     int       i;
     
     for (i = 0; i < w; i++) {
          u16 suv = le16(Suv[i>>1]);
          D[i].YUV.y = Sy[i];
          D[i].YUV.u = suv;
          D[i].YUV.v = suv >> 8;
     }
}

static void Load_NV12_BE( DVCContext *ctx )
{
     u8       *Sy  = ctx->src[0];
     u16      *Suv = ctx->src[1];
     DVCColor *D   = ctx->buf[0];
     int       w   = ctx->sw;
     int       i;
     
     for (i = 0; i < w; i++) {
          u16 suv = be16(Suv[i>>1]);
          D[i].YUV.y = Sy[i];
          D[i].YUV.u = suv;
          D[i].YUV.v = suv >> 8;
     }
}

static void Load_YUV444( DVCContext *ctx )
{
     u8       *Sy = ctx->src[0];
     u8       *Su = ctx->src[1];
     u8       *Sv = ctx->src[2];
     DVCColor *D  = ctx->buf[0];
     int       w  = ctx->sw;
     
     for (; w; w--) {
          D->YUV.y = *Sy++;
          D->YUV.u = *Su++;
          D->YUV.v = *Sv++;
          D++;
     }
}

static void Load_YUV422( DVCContext *ctx )
{
     u8       *Sy = ctx->src[0];
     u8       *Su = ctx->src[1];
     u8       *Sv = ctx->src[2];
     DVCColor *D  = ctx->buf[0];
     int       w  = ctx->sw >> 1;
     
     for (; w; w--) {
          D[0].YUV.y = Sy[0];
          D[1].YUV.y = Sy[1];
          D[0].YUV.u =
          D[1].YUV.u = Su[0];
          D[0].YUV.v =
          D[1].YUV.v = Sv[0];
          D  += 2;
          Sy += 2;
          Su++;
          Sv++;
     }
    
     if (ctx->sw & 1) {
          D->YUV.y = *Sy;
          D->YUV.u = *Su;
          D->YUV.v = *Sv;
     }
    
}

static void Load_YUV411( DVCContext *ctx )
{
     u8       *Sy = ctx->src[0];
     u8       *Su = ctx->src[1];
     u8       *Sv = ctx->src[2];
     DVCColor *D  = ctx->buf[0];
     int       w  = ctx->sw >> 2;
     
     for (; w; w--) {
          D[0].YUV.y = Sy[0];
          D[1].YUV.y = Sy[1];
          D[2].YUV.y = Sy[2];
          D[3].YUV.y = Sy[3];
          D[0].YUV.u =
          D[1].YUV.u =
          D[2].YUV.u =
          D[3].YUV.u = Su[0];
          D[0].YUV.v =
          D[1].YUV.v =
          D[2].YUV.v =
          D[3].YUV.v = Sv[0];
          D  += 4;
          Sy += 4;
          Su++;
          Sv++;
     }
     
     for (w = ctx->sw & 3; w; w--) {
          D->YUV.y = *Sy++;
          D->YUV.u = *Su;
          D->YUV.v = *Sv;
          D++;
     }
}

static DVCFunction Load_Proc[DVC_NUM_PIXELFORMATS] = {
     [DVC_PIXELFORMAT_INDEX(DVCPF_I8)]       = Load_I8,
     [DVC_PIXELFORMAT_INDEX(DVCPF_AI8)]      = Load_AI8,
     [DVC_PIXELFORMAT_INDEX(DVCPF_A4I4)]     = Load_A4I4,
     [DVC_PIXELFORMAT_INDEX(DVCPF_A8I8_LE)]  = Load_A8I8_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_A8I8_BE)]  = Load_A8I8_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_Y8)]       = Load_Y8,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB8)]     = Load_RGB8,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB15_LE)] = Load_RGB15_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB15_BE)] = Load_RGB15_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB16_LE)] = Load_RGB16_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB16_BE)] = Load_RGB16_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB24_LE)] = Load_RGB24_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB24_BE)] = Load_RGB24_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB32_LE)] = Load_RGB32_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB32_BE)] = Load_RGB32_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_ARGB_LE)]  = Load_ARGB_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_ARGB_BE)]  = Load_ARGB_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGBA_LE)]  = Load_RGBA_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGBA_BE)]  = Load_RGBA_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUYV_LE)]  = Load_YUYV_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUYV_BE)]  = Load_YUYV_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_NV12_LE)]  = Load_NV12_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_NV12_BE)]  = Load_NV12_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUV444)]   = Load_YUV444,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUV422)]   = Load_YUV422,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUV420)]   = Load_YUV422,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUV411)]   = Load_YUV411,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUV410)]   = Load_YUV411
};

/*****************************************************************************/

static inline u8
palette_search_rgb( DVCColor *c, DFBColor *lut, int lut_size )
{
     u8  idx = 0;
     int i, delta = 0xffffff;
     
     for (i = 0; i < lut_size; i++) {
          int d = ABS(c->RGB.r - lut[i].r) +
                  ABS(c->RGB.g - lut[i].g) +
                  ABS(c->RGB.b - lut[i].b);
          if (d < delta) {
               idx = i;
               if (d == 0)
                    break;
               delta = d;
          }
     }
     
     return idx;
}

static inline u8
palette_search_rgba( DVCColor *c, DFBColor *lut, int lut_size )
{
     u8  idx = 0;
     int i, delta = 0xffffff;
     
     for (i = 0; i < lut_size; i++) {
          int d = ABS(c->RGB.a - lut[i].a) +
                  ABS(c->RGB.r - lut[i].r) +
                  ABS(c->RGB.g - lut[i].g) +
                  ABS(c->RGB.b - lut[i].b);
          if (d < delta) {
               idx = i;
               if (d == 0)
                    break;
               delta = d;
          }
     }
     
     return idx;
}
     
static void Store_I8( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u8       *D = ctx->dst[0];
     int       w = ctx->dw;
     
     for (; w; w--) {
          *D++ = palette_search_rgb( S, ctx->dlut, ctx->dlut_size );
          S++;
     }
}

static void Store_AI8( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u8       *D = ctx->dst[0];
     int       w = ctx->dw;
     
     for (; w; w--) {
          *D++ = palette_search_rgba( S, ctx->dlut, ctx->dlut_size );
          S++;
     }
}

static void Store_A4I4( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u8       *D = ctx->dst[0];
     int       w = ctx->dw;
     
     for (; w; w--) {
          u8 idx = palette_search_rgb( S, ctx->dlut, ctx->dlut_size );
          *D++ = (S->RGB.a & 0xf0) | (idx & 0x0f);
          S++;
     }
}

static void Store_A8I8_LE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u16      *D = ctx->dst[0];
     int       w = ctx->dw;
     
     for (; w; w--) {
          u8 idx = palette_search_rgb( S, ctx->dlut, ctx->dlut_size );
          *D++ = le16((S->RGB.a << 8) | idx);
          S++;
     }
}

static void Store_A8I8_BE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u16      *D = ctx->dst[0];
     int       w = ctx->dw;
     
     for (; w; w--) {
          u8 idx = palette_search_rgb( S, ctx->dlut, ctx->dlut_size );
          *D++ = be16((S->RGB.a << 8) | idx);
          S++;
     }
}

static void Store_Y8( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u8       *D = ctx->dst[0];
     int       w = ctx->dw;
     
     for (; w; w--) {
          *D++ = S->YUV.y;
          S++;
     }
}

static void Store_RGB8( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u8       *D = ctx->dst[0];
     int       w = ctx->dw;
     
     for (; w; w--) {
          *D++ = ((S->RGB.r & 0xe0)     ) |
                 ((S->RGB.g & 0xe0) >> 3) |
                 ((S->RGB.b       ) >> 6);
          S++;
     }
}

static void Store_RGB15_LE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u16      *D = ctx->dst[0];
     int       w = ctx->dw;
     int       n;
     
     dither5_init( ctx->dy );
     
     if ((long)D & 2) {
          *D++ = le16(0x8000                      |
                      (dither5(S->RGB.r, 1) << 7) |
                      (dither5(S->RGB.g, 1) << 2) |
                      (dither5(S->RGB.b, 1) >> 3));
          S++;
          w--;
     }
     
     for (n = w>>1; n; n--) {
          *((u32*)D) = le32(0x80008000                     |
                            (dither5(S[0].RGB.r, 0) <<  7) |
                            (dither5(S[0].RGB.g, 0) <<  2) |
                            (dither5(S[0].RGB.b, 0) >>  3) |
                            (dither5(S[1].RGB.r, 1) << 23) |
                            (dither5(S[1].RGB.g, 1) << 18) |
                            (dither5(S[1].RGB.b, 1) << 13));
          D += 2;
          S += 2;
     }
     
     if (w & 1) {
          *D = le16(0x8000                      |
                    (dither5(S->RGB.r, 0) << 7) |
                    (dither5(S->RGB.g, 0) << 2) |
                    (dither5(S->RGB.b, 0) >> 3));
     }
}

static void Store_RGB15_BE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u16      *D = ctx->dst[0];
     int       w = ctx->dw;
     int       n;
     
     dither5_init( ctx->dy );
     
     if ((long)D & 2) {
          *D++ = be16(0x8000                      |
                      (dither5(S->RGB.r, 1) << 7) |
                      (dither5(S->RGB.g, 1) << 2) |
                      (dither5(S->RGB.b, 1) >> 3));
          S++;
          w--;
     }
     
     for (n = w>>1; n; n--) {
          *((u32*)D) = be32(0x80008000                     |
                            (dither5(S[1].RGB.r, 1) <<  7) |
                            (dither5(S[1].RGB.g, 1) <<  2) |
                            (dither5(S[1].RGB.b, 1) >>  3) |
                            (dither5(S[0].RGB.r, 0) << 23) |
                            (dither5(S[0].RGB.g, 0) << 18) |
                            (dither5(S[0].RGB.b, 0) << 13));
          D += 2;
          S += 2;
     }
     
     if (w & 1) {
          *D = be16(0x8000                      |
                    (dither5(S->RGB.r, 0) << 7) |
                    (dither5(S->RGB.g, 0) << 2) |
                    (dither5(S->RGB.b, 0) >> 3));
     }
}

static void Store_RGB16_LE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u16      *D = ctx->dst[0];
     int       w = ctx->dw;
     int       n;
     
     dither5_init( ctx->dy );
     dither6_init( ctx->dy );
     
     if ((long)D & 2) {
          *D++ = le16((dither5(S->RGB.r, 1) << 8) |
                      (dither6(S->RGB.g, 1) << 3) |
                      (dither5(S->RGB.b, 1) >> 3));
          S++;
          w--;
     }
     
     for (n = w>>1; n; n--) {
          *((u32*)D) = le32((dither5(S[0].RGB.r, 0) <<  8) |
                            (dither6(S[0].RGB.g, 0) <<  3) |
                            (dither5(S[0].RGB.b, 0) >>  3) |
                            (dither5(S[1].RGB.r, 1) << 24) |
                            (dither6(S[1].RGB.g, 1) << 19) |
                            (dither5(S[1].RGB.b, 1) << 13));
          D += 2;
          S += 2;
     }
     
     if (w & 1) {
          *D = le16((dither5(S->RGB.r, 0) << 8) |
                    (dither6(S->RGB.g, 0) << 3) |
                    (dither5(S->RGB.b, 0) >> 3));
     }
}

static void Store_RGB16_BE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u16      *D = ctx->dst[0];
     int       w = ctx->dw;
     int       n;
     
     dither5_init( ctx->dy );
     dither6_init( ctx->dy );
     
     if ((long)D & 2) {
          *D++ = be16((dither5(S->RGB.r, 1) << 8) |
                      (dither6(S->RGB.g, 1) << 3) |
                      (dither5(S->RGB.b, 1) >> 3));
          S++;
          w--;
     }
     
     for (n = w>>1; n; n--) {
          *((u32*)D) = be32((dither5(S[1].RGB.r, 1) <<  8) |
                            (dither6(S[1].RGB.g, 1) <<  3) |
                            (dither5(S[1].RGB.b, 1) >>  3) |
                            (dither5(S[0].RGB.r, 0) << 24) |
                            (dither6(S[0].RGB.g, 0) << 19) |
                            (dither5(S[0].RGB.b, 0) << 13));
          D += 2;
          S += 2;
     }
     
     if (w & 1) {
          *D = be16((dither5(S->RGB.r, 0) << 8) |
                    (dither6(S->RGB.g, 0) << 3) |
                    (dither5(S->RGB.b, 0) >> 3));
     }
}

static void Store_RGB24_LE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u8       *D = ctx->dst[0];
     int       w = ctx->dw; 
     
     for (; w; w--) {
          D[0] = S->RGB.b;
          D[1] = S->RGB.g;
          D[2] = S->RGB.r;
          D += 3;
          S++;
     }
}

static void Store_RGB24_BE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u8       *D = ctx->dst[0];
     int       w = ctx->dw; 
     
     for (; w; w--) {
          D[0] = S->RGB.r;
          D[1] = S->RGB.g;
          D[2] = S->RGB.b;
          D += 3;
          S++;
     }
}

static void Store_RGB32_LE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u32      *D = ctx->dst[0];
     int       w = ctx->dw; 
     
     direct_memcpy( D, S, w*4 );
}

static void Store_RGB32_BE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u32      *D = ctx->dst[0];
     int       w = ctx->dw; 
     
     for (; w; w--) {
          *D++ = be32((S->RGB.r << 16) |
                      (S->RGB.g <<  8) |
                      (S->RGB.b      ));
          S++;
     }
}

static void Store_ARGB_LE( DVCContext *ctx )
{  
     DVCColor *S = ctx->buf[0];
     u32      *D = ctx->dst[0];
     int       w = ctx->dw; 
     
     direct_memcpy( D, S, w*4 );
}

static void Store_ARGB_BE( DVCContext *ctx )
{  
     DVCColor *S = ctx->buf[0];
     u32      *D = ctx->dst[0];
     int       w = ctx->dw; 
     
     for (; w; w--) {
          *D++ = be32((S->RGB.a << 24) |
                      (S->RGB.r << 16) |
                      (S->RGB.g <<  8) |
                      (S->RGB.b      ));
          S++;
     }
}

static void Store_RGBA_LE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u32      *D = ctx->dst[0];
     int       w = ctx->dw;
     
     for (; w; w--) {
          *D++ = le32((S->RGB.r << 24) |
                      (S->RGB.g << 16) |
                      (S->RGB.b <<  8) |
                      (S->RGB.a      ));
          S++;
     }
}

static void Store_RGBA_BE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u32      *D = ctx->dst[0];
     int       w = ctx->dw;
     
     for (; w; w--) {
          *D++ = be32((S->RGB.r << 24) |
                      (S->RGB.g << 16) |
                      (S->RGB.b <<  8) |
                      (S->RGB.a      ));
          S++;
     }
}

static void Store_YUYV_LE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u32      *D = ctx->dst[0];
     int       w = ctx->dw >> 1;
     
     for (; w; w--) {
          u32 y0 = S[0].YUV.y;
          u32 y1 = S[1].YUV.y;
          u32 cb = (S[0].YUV.u + S[1].YUV.u) >> 1;
          u32 cr = (S[0].YUV.v + S[1].YUV.v) >> 1; 
#if __BYTE_ORDER == __BIG_ENDIAN
          *D++ = cr | (y1 << 8) | (cb << 16) | (y0 << 24);
#else
          *D++ = y0 | (cb << 8) | (y1 << 16) | (cr << 24);
#endif
          S += 2;
     }
}

static void Store_YUYV_BE( DVCContext *ctx )
{
     DVCColor *S = ctx->buf[0];
     u32      *D = ctx->dst[0];
     int       w = ctx->dw >> 1;
     
     for (; w; w--) {
          u32 y0 = S[0].YUV.y;
          u32 y1 = S[1].YUV.y;
          u32 cb = (S[0].YUV.u + S[1].YUV.u) >> 1;
          u32 cr = (S[0].YUV.v + S[1].YUV.v) >> 1; 
#if __BYTE_ORDER == __BIG_ENDIAN
          *D++ = y1 | (cr << 8) | (y0 << 16) | (cb << 24);
#else
          *D++ = cb | (y0 << 8) | (cr << 16) | (y1 << 24);
#endif
          S += 2;
     }
}

static void Store_NV12_LE( DVCContext *ctx )
{
     DVCColor *S0  = ctx->buf[0];
     DVCColor *S1  = ctx->buf[1];
     u8       *Dy  = ctx->dst[0];
     u16      *Duv = ctx->dst[1];
     int       w   = ctx->dw & ~1;
     int       i;
     
     for (i = 0; i < w; i++)
          Dy[i] = S0[i].YUV.y;
      
     if (ctx->dy & 1) {
          if (S1) {
               for (w = w>>1; w; w--) {
                    *Duv++ = le16((((S0[0].YUV.u + S0[1].YUV.u +
                                     S1[0].YUV.u + S1[1].YUV.u) >> 2)     ) |
                                  (((S0[0].YUV.v + S0[1].YUV.v +
                                     S1[0].YUV.v + S1[1].YUV.v) >> 2) << 8));
                    S0 += 2;
                    S1 += 2;
               }
          }
          else {
               for (w = w>>1; w; w--) {
                    *Duv++ = le16((((S0[0].YUV.u + S0[1].YUV.u) >> 1)     ) |
                                  (((S0[0].YUV.v + S0[1].YUV.v) >> 1) << 8));
                    S0 += 2;
               }
          }
     }
}

static void Store_NV12_BE( DVCContext *ctx )
{
     DVCColor *S0  = ctx->buf[0];
     DVCColor *S1  = ctx->buf[1];
     u8       *Dy  = ctx->dst[0];
     u16      *Duv = ctx->dst[1];
     int       w   = ctx->dw & ~1;
     int       i;
     
     for (i = 0; i < w; i++)
          Dy[i] = S0[i].YUV.y;
      
     if (ctx->dy & 1) {
          if (S1) {
               for (w = w>>1; w; w--) {
                    *Duv++ = be16((((S0[0].YUV.u + S0[1].YUV.u +
                                     S1[0].YUV.u + S1[1].YUV.u) >> 2)     ) |
                                  (((S0[0].YUV.v + S0[1].YUV.v +
                                     S1[0].YUV.v + S1[1].YUV.v) >> 2) << 8));
                    S0 += 2;
                    S1 += 2;
               }
          }
          else {
               for (w = w>>1; w; w--) {
                    *Duv++ = be16((((S0[0].YUV.u + S0[1].YUV.u) >> 1)     ) |
                                  (((S0[0].YUV.v + S0[1].YUV.v) >> 1) << 8));
                    S0 += 2;
               }
          }
     }
}
   
static void Store_YUV444( DVCContext *ctx )
{
     DVCColor *S  = ctx->buf[0];
     u8       *Dy = ctx->dst[0];
     u8       *Du = ctx->dst[1];
     u8       *Dv = ctx->dst[2];
     int       w  = ctx->dw;
     int       i;
     
     for (i = 0; i < w; i++) {
          Dy[i] = S[i].YUV.y;
          Du[i] = S[i].YUV.u;
          Dv[i] = S[i].YUV.v;
     }
}

static void Store_YUV422( DVCContext *ctx )
{
     DVCColor *S  = ctx->buf[0];
     u8       *Dy = ctx->dst[0];
     u8       *Du = ctx->dst[1];
     u8       *Dv = ctx->dst[2];
     int       w  = ctx->dw >> 1;
     
     for (; w; w--) {
          *Dy++ = S[0].YUV.y;
          *Dy++ = S[1].YUV.y;
          *Du++ = (S[0].YUV.u + S[1].YUV.u) >> 1;
          *Dv++ = (S[0].YUV.v + S[1].YUV.v) >> 1;
          S += 2;
     }
}

static void Store_YUV420( DVCContext *ctx )
{
     Store_Y8( ctx );
          
     if (ctx->dy & 1) {
          DVCColor *S0 = ctx->buf[0];
          DVCColor *S1 = ctx->buf[1];
          u8       *Du = ctx->dst[1];
          u8       *Dv = ctx->dst[2];
          int       w  = ctx->dw >> 1;
     
          if (S1) {
               for (; w; w--) {
                    *Du++ = (S0[0].YUV.u + S0[1].YUV.u +
                             S1[0].YUV.u + S1[1].YUV.u) >> 2;
                    *Dv++ = (S0[0].YUV.v + S0[1].YUV.v +
                             S1[0].YUV.v + S1[1].YUV.v) >> 2;
                    S0 += 2;
                    S1 += 2;
               }
          } 
          else {
               for (; w; w--) {
                    *Du++ = (S0[0].YUV.u + S0[1].YUV.u) >> 1;
                    *Dv++ = (S0[0].YUV.v + S0[1].YUV.v) >> 1;
                    S0 += 2;
               }
          }
     }
}

static void Store_YUV411( DVCContext *ctx )
{
     DVCColor *S  = ctx->buf[0];
     u8       *Dy = ctx->dst[0];
     u8       *Du = ctx->dst[1];
     u8       *Dv = ctx->dst[2];
     int       w  = ctx->dw >> 2;
     
     for (; w; w--) {
          *Dy++ = S[0].YUV.y;
          *Dy++ = S[1].YUV.y;
          *Dy++ = S[2].YUV.y;
          *Dy++ = S[3].YUV.y;
          *Du++ = (S[0].YUV.u + S[1].YUV.u + S[2].YUV.u + S[3].YUV.u) >> 2;
          *Dv++ = (S[0].YUV.v + S[1].YUV.v + S[2].YUV.v + S[3].YUV.v) >> 2;
          S += 4;
     }
}

static void Store_YUV410( DVCContext *ctx )
{     
     Store_Y8( ctx );
      
     if ((ctx->dy & 3) == 3) {   
          DVCColor *S0 = ctx->buf[0];
          DVCColor *S1 = ctx->buf[1];
          u8       *Du = ctx->dst[1];
          u8       *Dv = ctx->dst[2];
          int       w  = ctx->dw >> 2;

          if (S1) {
               /* FIXME: YUV444/YUV422/YUV411 -> YUV410 */
               for (; w; w--) {
                    *Du++ = (S0[0].YUV.u + S0[1].YUV.u +
                             S0[2].YUV.u + S0[3].YUV.u +
                             S1[0].YUV.u + S1[1].YUV.u +
                             S1[2].YUV.u + S1[3].YUV.u) >> 3;
                    *Dv++ = (S0[0].YUV.v + S0[1].YUV.v +
                             S0[2].YUV.v + S0[3].YUV.v +
                             S1[0].YUV.v + S1[1].YUV.v +
                             S1[2].YUV.v + S1[3].YUV.v) >> 3;
                    S0 += 4;
                    S1 += 4;
               }
          }
          else {
               for (; w; w--) {
                    *Du++ = (S0[0].YUV.u + S0[1].YUV.u +
                             S0[2].YUV.u + S0[3].YUV.u) >> 2;
                    *Dv++ = (S0[0].YUV.v + S0[1].YUV.v +
                             S0[2].YUV.v + S0[3].YUV.v) >> 2;
                    S0 += 4;
               }
          }
     }
}

static DVCFunction Store_Proc[DVC_NUM_PIXELFORMATS] = {
     [DVC_PIXELFORMAT_INDEX(DVCPF_I8)]        = Store_I8,
     [DVC_PIXELFORMAT_INDEX(DVCPF_AI8)]       = Store_AI8,
     [DVC_PIXELFORMAT_INDEX(DVCPF_A4I4)]      = Store_A4I4,
     [DVC_PIXELFORMAT_INDEX(DVCPF_A8I8_LE)]   = Store_A8I8_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_A8I8_BE)]   = Store_A8I8_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_Y8)]        = Store_Y8,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB8)]      = Store_RGB8,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB15_LE)]  = Store_RGB15_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB15_BE)]  = Store_RGB15_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB16_LE)]  = Store_RGB16_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB16_BE)]  = Store_RGB16_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB24_LE)]  = Store_RGB24_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB24_BE)]  = Store_RGB24_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB32_LE)]  = Store_RGB32_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGB32_BE)]  = Store_RGB32_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_ARGB_LE)]   = Store_ARGB_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_ARGB_BE)]   = Store_ARGB_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGBA_LE)]   = Store_RGBA_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_RGBA_BE)]   = Store_RGBA_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUYV_LE)]   = Store_YUYV_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUYV_BE)]   = Store_YUYV_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_NV12_LE)]   = Store_NV12_LE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_NV12_BE)]   = Store_NV12_BE,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUV444)]    = Store_YUV444,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUV422)]    = Store_YUV422,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUV420)]    = Store_YUV420,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUV411)]    = Store_YUV411,
     [DVC_PIXELFORMAT_INDEX(DVCPF_YUV410)]    = Store_YUV410
};

/*****************************************************************************/

static void Colormap_Proc( DVCContext *ctx )
{
     DVCColormap *C = ctx->colormap;
     DVCColor    *D = ctx->buf[0];
     int          w = ctx->sw;
     
     for (; w; w--) {
          D->RGB.r = C->RGB.r[D->RGB.r];
          D->RGB.g = C->RGB.g[D->RGB.g];
          D->RGB.b = C->RGB.b[D->RGB.b];
          D++;
     }
}

static void YCbCr_to_RGB_Proc_C( DVCContext *ctx )
{
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          YCBCR_TO_RGB( D->YUV.y, D->YUV.u, D->YUV.v,
                        D->RGB.r, D->RGB.g, D->RGB.b );
          D++;
     }
}

static DVCFunction YCbCr_to_RGB_Proc = YCbCr_to_RGB_Proc_C;

static void RGB_to_YCbCr_Proc_C( DVCContext *ctx )
{
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          RGB_TO_YCBCR( D->RGB.r, D->RGB.g, D->RGB.b,
                        D->YUV.y, D->YUV.u, D->YUV.v );
          D++;
     }
}

static DVCFunction RGB_to_YCbCr_Proc = RGB_to_YCbCr_Proc_C;

static void Premultiply_Proc( DVCContext *ctx )
{
     DVCColor *D = ctx->buf[0];
     int       w = ctx->sw;
     
     for (; w; w--) {
          switch (D->RGB.a) {
               case 0xff:
                    break;
               case 0x00:
                    D->RGB.r = 0;
                    D->RGB.g = 0;
                    D->RGB.b = 0;
                    break;
               default: {
                    int a = D->RGB.a + 1;
                    D->RGB.r = D->RGB.r * a >> 8;
                    D->RGB.g = D->RGB.g * a >> 8;
                    D->RGB.b = D->RGB.b * a >> 8;
                    }
                    break;
          }
     }
}

static void ScaleH_Up_Proc_C( DVCContext *ctx )
{
     u32 *S = (u32*)ctx->buf[0];
     u32 *D = (u32*)ctx->buf[0] + ctx->dw - 1;
     int  i = ctx->h_scale * (ctx->dw - 1);
     int  j = i & ~0xffff;
     int  n = ctx->dw;
     
     for (; n && i >= j; n--) {
          *D-- = S[i>>16];
          i -= ctx->h_scale;
     }

     for (; n; n--) {
          int p = i >> 16;
          int w = i & 0xff00;
               
          if (w) {
               w >>= 8;
               *D = ((((S[p] & 0x00ff00ff) * (256-w) + (S[p+1] & 0x00ff00ff) * w) >> 8) & 0x00ff00ff) |
                    ((((S[p] & 0xff00ff00) >> 8) * (256-w) + ((S[p+1] & 0xff00ff00) >> 8) * w) & 0xff00ff00);
          }
          else {
               *D = S[p];
          }
               
          D--;
          i -= ctx->h_scale;
     }
}

static DVCFunction ScaleH_Up_Proc = ScaleH_Up_Proc_C;

static void ScaleH_Up4_Proc( DVCContext *ctx )
{
     u32 *S = (u32*)ctx->buf[0];
     u32 *D = (u32*)ctx->buf[0] + ctx->dw - 1;
     int  i = ctx->h_scale * (ctx->dw - 1);
     int  j = i & ~0xffff;
     int  n = ctx->dw;
     
     for (; n && i >= j; n--) {
          *D-- = S[i>>16];
          i -= ctx->h_scale;
     }

     for (; n; n--) {
          int p = i >> 16;
          
          switch (i & (3 << 14)) {
               case 0:
                    *D = S[p];
                    break;
               case 1 << 14:
                    *D = ((((S[p] & 0x00ff00ff) * 3 + (S[p+1] & 0x00ff00ff)) >> 2) & 0x00ff00ff) |
                         ((((S[p] & 0xff00ff00) >> 2) * 3 + ((S[p+1] & 0xff00ff00) >> 2)) & 0xff00ff00);
                    break;
               case 2 << 14:
                    *D = (((S[p] ^ S[p+1]) & 0xfefefefe) >> 1) + (S[p] & S[p+1]);
                    break;
               case 3 << 14:
                    *D = ((((S[p] & 0x00ff00ff) + (S[p+1] & 0x00ff00ff) * 3) >> 2) & 0x00ff00ff) |
                         ((((S[p] & 0xff00ff00) >> 2) + ((S[p+1] & 0xff00ff00) >> 2) * 3) & 0xff00ff00);
                    break;
          }
               
          D--;
          i -= ctx->h_scale;
     }
}

static void ScaleH_Down_Proc( DVCContext *ctx )
{
     u32 *S = (u32*)ctx->buf[0];
     u32 *D = (u32*)ctx->buf[0];
     int  i = 0;
     int  n = ctx->dw;
          
     for (; n; n--) {
          *D++ = S[i>>16];
          i += ctx->h_scale;
     }
}

static void ScaleV_Up_Proc_C( DVCContext *ctx )
{
     u32 *S = (u32*)ctx->buf[1];
     u32 *D = (u32*)ctx->buf[0];
     
     if (ctx->s_v & 0xff00) {
          int b = (ctx->s_v & 0xff00) >> 8;
          int a = 256 - b;
          int n = ctx->dw;
          
          for (; n; n--) {
               *D = ((((*S & 0x00ff00ff) * a + (*D & 0x00ff00ff) * b) >> 8) & 0x00ff00ff) |
                    ((((*S & 0xff00ff00) >> 8) * a + ((*D & 0xff00ff00) >> 8) * b) & 0xff00ff00);
               D++;
               S++;
          }
     }
     else if (ctx->s_v & 0x00ff) {
          direct_memcpy( D, S, ctx->dw * sizeof(DVCColor) );
     }
}

static DVCFunction ScaleV_Up_Proc = ScaleV_Up_Proc_C;

static void ScaleV_Up4_Proc( DVCContext *ctx )
{
     u32 *S = (u32*)ctx->buf[1];
     u32 *D = (u32*)ctx->buf[0];
     int  n;
     
     if (ctx->s_v & 0xffff) {
          switch (ctx->s_v & (3 << 14)) {
               case 0:
                    direct_memcpy( D, S, ctx->dw * sizeof(DVCColor) ); 
                    break;
               case 1 << 14:
                    for (n = ctx->dw; n; n--) {
                         *D = ((((*S & 0x00ff00ff) * 3 + (*D & 0x00ff00ff)) >> 2) & 0x00ff00ff) |
                              ((((*S & 0xff00ff00) >> 2) * 3 + ((*D & 0xff00ff00) >> 2)) & 0xff00ff00);
                         D++;
                         S++;
                    }
                    break;
               case 2 << 14:
                    for (n = ctx->dw; n; n--) {
                         *D = (((*S ^ *D) & 0xfefefefe) >> 1) + (*S & *D);
                         D++;
                         S++;
                    }
                    break;
               case 3 << 14:
                    for (n = ctx->dw; n; n--) {
                         *D = ((((*S & 0x00ff00ff) + (*D & 0x00ff00ff) * 3) >> 2) & 0x00ff00ff) |
                              ((((*S & 0xff00ff00) >> 2) + ((*D & 0xff00ff00) >> 2) * 3) & 0xff00ff00);
                         D++;
                         S++;
                    }
                    break;
          }
     }
}

/*****************************************************************************/

#define LINE_PTR( ptr, pitch, y, h, separated ) \
     ( (separated) \
       ? (((u8*)(ptr)) + (y)/2 * (pitch) + (((y)&1) ? (h)/2 * (pitch) : 0)) \
       : (((u8*)(ptr)) + (y) * (pitch)) )


static void
picture_copy_simple( const DVCPicture *source,
                     const DVCPicture *dest,
                     int   src_x,    int   src_y,
                     int   src_w,    int   src_h,
                     int   dst_x,    int   dst_y )
{
     DVCPixelFormat f = source->format;
     int            i;
     
     for (i = 0; i < DVC_NUM_PLANES(f); i++) {
          void *dst, *src;
          int   dy = dst_y >> DVC_PLANE_V_SHIFT(f,i);
          int   sy = src_y >> DVC_PLANE_V_SHIFT(f,i);
          int   h  = src_h >> DVC_PLANE_V_SHIFT(f,i);
          int   w  = (src_w >> DVC_PLANE_H_SHIFT(f,i)) * DVC_PLANE_BPS(f,i);
          
          src = source->base[i] + 
                (src_x >> DVC_PLANE_H_SHIFT(f,i)) * DVC_PLANE_BPS(f,i);
          dst = dest->base[i] +
                (dst_x >> DVC_PLANE_H_SHIFT(f,i)) * DVC_PLANE_BPS(f,i);
          
          for (; h; h--) {
               void *d, *s;
               
               s = LINE_PTR( src, source->pitch[i], 
                             sy, h, source->separated );
               d = LINE_PTR( dst, dest->pitch[i], 
                             dy, h, dest->separated );
               
               direct_memcpy( d, s, w );
               
               sy++;
               dy++;
          }
     }
}


static inline void
compute_h_offset( const DVCPicture *pic,
                  void *const       base[3],
                  int               dx,
                  void             *rptr[3] )
{
     DVCPixelFormat f = pic->format;
     int            i;
     
     rptr[0] = base[0] + (dx >> DVC_PLANE_H_SHIFT(f,0)) * DVC_PLANE_BPS(f,0);
     
     for (i = 1; i < DVC_NUM_PLANES(f); i++)
          rptr[i] = base[i] + (dx >> DVC_PLANE_H_SHIFT(f,i)) * DVC_PLANE_BPS(f,i);
}
          
static inline void
compute_v_offset( const DVCPicture *pic,
                  void *const       base[3],
                  int               dy,
                  void             *rptr[3] )
{
     DVCPixelFormat f = pic->format;
     int            y, h, i;
     
     y = dy >> DVC_PLANE_V_SHIFT(f,0);
     h = pic->height >> DVC_PLANE_V_SHIFT(f,0);
          
     rptr[0] = LINE_PTR( base[0], pic->pitch[0], y, h, pic->separated );
     
     for (i = 1; i < DVC_NUM_PLANES(f); i++) {
          y = dy >> DVC_PLANE_V_SHIFT(f,i);
          h = pic->height >> DVC_PLANE_V_SHIFT(f,i);
          
          rptr[i] = LINE_PTR( base[i], pic->pitch[i], y, h, pic->separated );
     }
}


#ifdef USE_MMX
# include "dvc_mmx.h"
#endif

#define SWAP_BUFFERS() { \
     DVCColor *tmp = ctx.buf[1]; \
     ctx.buf[1] = ctx.buf[0]; \
     ctx.buf[0] = tmp; \
}

DFBResult
dvc_scale( const DVCPicture   *source,
           const DVCPicture   *dest,
           const DFBRectangle *srect,
           const DFBRectangle *drect,
           const DVCColormap  *colormap )
{
     DVCContext   ctx;
     DVCFunction  load;
     DVCFunction  store;
     DVCFunction  filters[6];
     DVCFunction *filter = filters;
     int          len;
     
     D_ASSERT( source != NULL );
     D_ASSERT( dest != NULL );
     
     if (srect) {
          ctx.sx = srect->x;
          ctx.sy = srect->y;
          ctx.sw = srect->w;
          ctx.sh = srect->h;
     }
     else {
          ctx.sx = ctx.sy = 0;
          ctx.sw = source->width;
          ctx.sh = source->height;
     }
     
     if (drect) {
          ctx.dx = drect->x;
          ctx.dy = drect->y;
          ctx.dw = drect->w;
          ctx.dh = drect->h;
     }
     else {
          ctx.dx = ctx.dy = 0;
          ctx.dw = dest->width;
          ctx.dh = dest->height;
     }
     
     ctx.h_scale = (ctx.sw << 16) / ctx.dw;
     ctx.v_scale = (ctx.sh << 16) / ctx.dh;
     
     if (source->format == dest->format && !colormap &&
         ctx.h_scale == (1 << 16) && ctx.v_scale == (1 << 16) &&
         (!dest->premultiplied || source->premultiplied))
     {
          picture_copy_simple( source, dest, 
                               ctx.sx, ctx.sy, ctx.sw, ctx.sh, ctx.dx, ctx.dy );
          return DFB_OK;
     }
    
#ifdef USE_MMX 
     init_mmx();
#endif
     
     /* Begin ctx functions setup. */
     load = Load_Proc[DVC_PIXELFORMAT_INDEX(source->format)];
     if (!load)
          return DFB_UNSUPPORTED;
          
     store = Store_Proc[DVC_PIXELFORMAT_INDEX(dest->format)];
     if (!store)
          return DFB_UNSUPPORTED;
          
     if (colormap)
          *filter++ = Colormap_Proc;
     
     if (DVC_YCBCR_PIXELFORMAT(source->format) && 
        !DVC_YCBCR_PIXELFORMAT(dest->format))
          *filter++ = YCbCr_to_RGB_Proc;
     else if (!DVC_YCBCR_PIXELFORMAT(source->format) &&
               DVC_YCBCR_PIXELFORMAT(dest->format))
          *filter++ = RGB_to_YCbCr_Proc;
          
     if (dest->premultiplied && 
         DVC_ALPHA_PIXELFORMAT(source->format) && !source->premultiplied)
          *filter++ = Premultiply_Proc;
          
     if (ctx.h_scale < (1 << 16)) {
          switch (ctx.h_scale) {
               case 1 << 15:
               case 1 << 14:
                    *filter++ = ScaleH_Up4_Proc;
                    break;
               default:
                    *filter++ = ScaleH_Up_Proc;
                    break;
          }
     }
     else if (ctx.h_scale > (1 << 16))
          *filter++ = ScaleH_Down_Proc;
           
     if (ctx.v_scale < (1 << 16)) {
          switch (ctx.v_scale) {
               case 1 << 15:
               case 1 << 14:
                    *filter++ = ScaleV_Up4_Proc;
                    break;
               default:
                    *filter++ = ScaleV_Up_Proc;
                    break;
          }
     }
     
     *filter = NULL;
     /* End ctx functions setup. */
     
     if (DVC_INDEXED_PIXELFORMAT(source->format)) {
          ctx.slut      = source->palette;
          ctx.slut_size = source->palette_size;
     }
     if (DVC_INDEXED_PIXELFORMAT(dest->format)) {
          ctx.dlut      = dest->palette;
          ctx.dlut_size = dest->palette_size;
     }
     
     ctx.colormap = (DVCColormap*) colormap;
     
     len = MAX( ctx.sw, ctx.dw );
     ctx.buf[0] = (len > 512)
                   ? malloc( len * sizeof(DVCColor) )
                   : alloca( len * sizeof(DVCColor) );
     
     if (ctx.v_scale < (1 << 16) ||
         DVC_PLANE_V_SHIFT(dest->format,1) > DVC_PLANE_V_SHIFT(source->format,1) ||
         DVC_PLANE_V_SHIFT(dest->format,2) > DVC_PLANE_V_SHIFT(source->format,2))
     {
          ctx.buf[1] = (len > 512)
                        ? malloc( len * sizeof(DVCColor) )
                        : alloca( len * sizeof(DVCColor) );
     }
     else {
          ctx.buf[1] = NULL;
     }
     
     if (!DVC_ALPHA_PIXELFORMAT(source->format) &&
          DVC_ALPHA_PIXELFORMAT(dest->format))
     {
          memset( ctx.buf[0], 0xff, len * sizeof(DVCColor) );
          if (ctx.buf[1])
               memset( ctx.buf[0], 0xff, len * sizeof(DVCColor) );
     }
     
     compute_h_offset( source, source->base, ctx.sx, ctx.src_base );
     compute_h_offset( dest,   dest->base,   ctx.dx, ctx.dst_base );
     
     ctx.s_v = ctx.sy << 16;

     if (ctx.v_scale < (1 << 16)) {
          int dy2 = ctx.dy + ctx.dh;
          
          while (ctx.dy < dy2) {
               compute_v_offset( source, ctx.src_base, ctx.sy, ctx.src );
               compute_v_offset( dest,   ctx.dst_base, ctx.dy, ctx.dst );
          
               load( &ctx );
               for (filter = filters; *filter; filter++)
                    (*filter)( &ctx );
               store( &ctx );
          
               ctx.dy++;
               ctx.s_v += ctx.v_scale;
               if (ctx.s_v > (ctx.sy << 16)) {
                    ctx.sy = (ctx.s_v >> 16) + 1;
                    if (ctx.sy == source->height)
                         break;
                    SWAP_BUFFERS()
               }
          }
          
          while (ctx.dy < dy2) {
               compute_v_offset( dest, ctx.dst_base, ctx.dy, ctx.dst );
               
               store( &ctx );
               
               ctx.dy++;
          }
     }
     else {
          int n;
          
          for (n = ctx.dh; n; n--) {
               compute_v_offset( source, ctx.src_base, ctx.sy, ctx.src );
               compute_v_offset( dest,   ctx.dst_base, ctx.dy, ctx.dst );
          
               load( &ctx );
               for (filter = filters; *filter; filter++)
                    (*filter)( &ctx );
               store( &ctx );
          
               ctx.dy++;
               ctx.s_v += ctx.v_scale;
               ctx.sy = ctx.s_v >> 16;
          
               if (ctx.buf[1])
                    SWAP_BUFFERS()
          }
     }
     
     if (len > 512) {
          free( ctx.buf[0] );
          if (ctx.buf[1])
               free( ctx.buf[1]);
     }
     
     return DFB_OK;
}

DFBResult 
dvc_scale_to_surface( const DVCPicture   *source,
                      IDirectFBSurface   *dest,
                      const DFBRectangle *dest_rect,
                      const DVCColormap  *colormap )
{
     IDirectFBSurface_data *dst_data;
     CoreSurfaceBufferLock  lock;
     DVCPicture             picture;
     DFBRectangle           srect;
     DFBRectangle           drect;
     DFBResult              ret;
     
     D_ASSERT( source != NULL );
     D_ASSERT( dest != NULL );
     
     DIRECT_INTERFACE_GET_DATA_FROM( dest, dst_data, IDirectFBSurface );
     
     srect.x = srect.y = 0;
     srect.w = source->width;
     srect.h = source->height;
     
     if (dest_rect) {
          drect = *dest_rect;
          drect.x += dst_data->area.wanted.x;
          drect.y += dst_data->area.wanted.y;
     }
     else {
          drect = dst_data->area.wanted;
     }
     
     {
          DFBRegion clip;
     
          dfb_region_from_rectangle( &clip, &dst_data->area.current );
          if (!dfb_clip_blit_precheck( &clip, drect.w, drect.h, drect.x, drect.y ))
               return DFB_INVAREA;
     
          dfb_clip_stretchblit( &clip, &srect, &drect );
     }
     
     picture.format = dfb2dvc_pixelformat( dst_data->surface->config.format );
     if (!picture.format)
          return DFB_UNSUPPORTED;
     picture.width  = dst_data->surface->config.size.w;
     picture.height = dst_data->surface->config.size.h; 
     
     ret = dfb_surface_lock_buffer( dst_data->surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
     if (ret)
          return ret;

     picture.base[0] = lock.addr;
     picture.pitch[0] = lock.pitch;
                          
     switch (dst_data->surface->config.format) {
          case DSPF_LUT8:
          case DSPF_ALUT44:
               picture.palette = dst_data->surface->palette->entries;
               picture.palette_size = dst_data->surface->palette->num_entries;
               break;
          case DSPF_I420:
               picture.pitch[1] =
               picture.pitch[2] = picture.pitch[0] >> 1;
               picture.base[1] = picture.base[0] + picture.pitch[0]*picture.height;
               picture.base[2] = picture.base[1] + picture.pitch[1]*(picture.height>>1);
               break;
          case DSPF_YV12:
               picture.pitch[1] =
               picture.pitch[2] = picture.pitch[0] >> 1;
               picture.base[2] = picture.base[0] + picture.pitch[0]*picture.height;
               picture.base[1] = picture.base[2] + picture.pitch[2]*(picture.height>>1);
               break;
          case DSPF_NV12:
          case DSPF_NV21:
               picture.pitch[1] = picture.pitch[0] & ~1;
               picture.base[1] = picture.base[0] + picture.pitch[0]*picture.height;
               break;
          default:
               break;
     }
     
     picture.separated = (dst_data->caps & DSCAPS_SEPARATED) != 0;
     picture.premultiplied = (dst_data->caps & DSCAPS_PREMULTIPLIED) != 0;
     
     dvc_scale( source, &picture, &srect, &drect, colormap );
     
     dfb_surface_unlock_buffer( dst_data->surface, &lock );
     
     return DFB_OK;
}
