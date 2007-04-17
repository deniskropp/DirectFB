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
   
   DVC - DirectFB Video Converter (also Scaler)
*/

#ifndef __DVC_H__
#define __DVC_H__

#include <endian.h>

#include <directfb.h>

#include <direct/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Encoding of pixelformat (bit 0 - 31):
 *
 *   aaaa:abcd | eeff:fggg | hhhi:ijj | kkll:mmnn
 *
 * a) pixelformat index
 * b) indexed format
 * c) format has an alpha channel
 * d) format is YCbCr
 * e) number of color planes
 * f) number of bytes per sample in 1st plane
 * g) number of bytes per sample in 2nd plane
 * h) number of bytes per sample in 3rd plane
 * i) 1st plane horizontal scale
 * j) 1st plane vertical scale
 * k) 2nd plane horizontal scale
 * l) 2nd plane vertical scale
 * m) 3rd plane horizontal scale
 * n) 3rd plane vertical scale
 */
#define DVC_PIXELFORMAT( id, indexed, alpha, ycbcr, planes, \
                         p1_bps, p2_bps, p3_bps,            \
                         p1_h_shift, p1_v_shift,            \
                         p2_h_shift, p2_v_shift,            \
                         p3_h_shift, p3_v_shift )           \
          ( (((id)         & 0x1f)      ) |                 \
            (((indexed)    & 0x01) <<  5) |                 \
            (((alpha)      & 0x01) <<  6) |                 \
            (((ycbcr)      & 0x01) <<  7) |                 \
            (((planes)     & 0x03) <<  8) |                 \
            (((p1_bps)     & 0x07) << 10) |                 \
            (((p2_bps)     & 0x07) << 13) |                 \
            (((p3_bps)     & 0x07) << 16) |                 \
            (((p1_h_shift) & 0x03) << 19) |                 \
            (((p1_v_shift) & 0x03) << 21) |                 \
            (((p2_h_shift) & 0x03) << 23) |                 \
            (((p2_v_shift) & 0x03) << 25) |                 \
            (((p3_h_shift) & 0x03) << 27) |                 \
            (((p3_v_shift) & 0x03) << 29) )


/* All pixelformats are in host endianess. */
typedef enum {
     DVCPF_UNKNOWN = 0,
     /* 8bit  (Indexed) */
     DVCPF_I8       = DVC_PIXELFORMAT(  0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 8bit  (Indexed with alpha from palette) */
     DVCPF_AI8      = DVC_PIXELFORMAT(  1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 8bit  (A4 I4) */
     DVCPF_A4I4     = DVC_PIXELFORMAT(  2, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 16bit (A8 I8) */
     DVCPF_A8I8_LE  = DVC_PIXELFORMAT(  3, 1, 1, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 16bit (A8 I8) */
     DVCPF_A8I8_BE  = DVC_PIXELFORMAT(  4, 1, 1, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 8bit  (Grayscale) */
     DVCPF_Y8       = DVC_PIXELFORMAT(  5, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 8bit  (R3 G3 B2) */
     DVCPF_RGB8     = DVC_PIXELFORMAT(  6, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 16bit (X1 R5 G5 B5) */
     DVCPF_RGB15_LE = DVC_PIXELFORMAT(  7, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 16bit (X1 R5 G5 B5) */
     DVCPF_RGB15_BE = DVC_PIXELFORMAT(  8, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 16bit (R5 G6 B5) */
     DVCPF_RGB16_LE = DVC_PIXELFORMAT(  9, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 16bit (R5 G6 B5) */
     DVCPF_RGB16_BE = DVC_PIXELFORMAT( 10, 0, 0, 0, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 24bit (R8 G8 B8) */
     DVCPF_RGB24_LE = DVC_PIXELFORMAT( 11, 0, 0, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 24bit (R8 G8 B8) */
     DVCPF_RGB24_BE = DVC_PIXELFORMAT( 12, 0, 0, 0, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 32bit (X8 R8 G8 B8) */
     DVCPF_RGB32_LE = DVC_PIXELFORMAT( 13, 0, 0, 0, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 32bit (X8 R8 G8 B8) */
     DVCPF_RGB32_BE = DVC_PIXELFORMAT( 14, 0, 0, 0, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 32bit (A8 R8 G8 B8) */
     DVCPF_ARGB_LE  = DVC_PIXELFORMAT( 15, 0, 1, 0, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 32bit (A8 R8 G8 B8) */
     DVCPF_ARGB_BE  = DVC_PIXELFORMAT( 16, 0, 1, 0, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 32bit (R8 G8 B8 A8) */
     DVCPF_RGBA_LE  = DVC_PIXELFORMAT( 17, 0, 1, 0, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 32bit (R8 G8 B8 A8) */
     DVCPF_RGBA_BE  = DVC_PIXELFORMAT( 18, 0, 1, 0, 1, 4, 0, 0, 0, 0, 0, 0, 0, 0 ),
     /* 16bit (Y0Cb Y1Cr) */
     DVCPF_YUYV_LE  = DVC_PIXELFORMAT( 19, 0, 0, 1, 1, 4, 0, 0, 1, 0, 0, 0, 0, 0 ),
     /* 16bit (Y0Cb Y1Cr) */
     DVCPF_YUYV_BE  = DVC_PIXELFORMAT( 20, 0, 0, 1, 1, 4, 0, 0, 1, 0, 0, 0, 0, 0 ),
     /* 12bit (Planar YCbCr 4:2:0 -> [0] = Y, [1] = CbCr) */
     DVCPF_NV12_LE  = DVC_PIXELFORMAT( 21, 0, 0, 1, 2, 1, 2, 0, 0, 0, 1, 1, 0, 0 ),
     /* 12bit (Planar YCbCr 4:2:0 -> [0] = Y, [1] = CrCb) */
     DVCPF_NV12_BE  = DVC_PIXELFORMAT( 22, 0, 0, 1, 2, 1, 2, 0, 0, 0, 1, 1, 0, 0 ),
     /* 24bit (Planar YCbCr 4:4:4 -> [0] = Y, [1] = Cb, [2] = Cr) */
     DVCPF_YUV444   = DVC_PIXELFORMAT( 23, 0, 0, 1, 3, 1, 1, 1, 0, 0, 0, 0, 0, 0 ),
     /* 16bit (Planar YCbCr 4:2:2 -> [0] = Y, [1] = Cb, [2] = Cr) */
     DVCPF_YUV422   = DVC_PIXELFORMAT( 24, 0, 0, 1, 3, 1, 1, 1, 0, 0, 1, 0, 1, 0 ),
     /* 12bit (Planar YCbCr 4:2:0 -> [0] = Y, [1] = Cb, [2] = Cr) */
     DVCPF_YUV420   = DVC_PIXELFORMAT( 25, 0, 0, 1, 3, 1, 1, 1, 0, 0, 1, 1, 1, 1 ),
     /* 12bit (Planar YCbCr 4:1:1 -> [0] = Y, [1] = Cb, [2] = Cr) */
     DVCPF_YUV411   = DVC_PIXELFORMAT( 26, 0, 0, 1, 3, 1, 1, 1, 0, 0, 2, 0, 2, 0 ),
     /* 9bit  (Planar YCbCr 4:1:0 -> [0] = Y, [1] = Cb, [2] = Cr) */
     DVCPF_YUV410   = DVC_PIXELFORMAT( 27, 0, 0, 1, 3, 1, 1, 1, 0, 0, 2, 2, 2, 2 ),
     
#if __BYTE_ORDER == __BIG_ENDIAN
     DVCPF_A8I8     = DVCPF_A8I8_BE,
     DVCPF_I8A8     = DVCPF_A8I8_LE,
     DVCPF_RGB15    = DVCPF_RGB15_BE,
     DVCPF_RGB16    = DVCPF_RGB16_BE,
     DVCPF_RGB24    = DVCPF_RGB24_BE,
     DVCPF_BGR24    = DVCPF_RGB24_LE,
     DVCPF_RGB32    = DVCPF_RGB32_BE,
     DVCPF_BGR32    = DVCPF_RGB32_LE,
     DVCPF_ARGB     = DVCPF_ARGB_BE,
     DVCPF_BGRA     = DVCPF_ARGB_LE,
     DVCPF_RGBA     = DVCPF_RGBA_BE,
     DVCPF_ABGR     = DVCPF_RGBA_LE,
     DVCPF_YUY2     = DVCPF_YUYV_BE,
     DVCPF_UYVY     = DVCPF_YUYV_LE,
     DVCPF_NV12     = DVCPF_NV12_BE,
     DVCPF_NV21     = DVCPF_NV12_LE
#else
     DVCPF_A8I8     = DVCPF_A8I8_LE,
     DVCPF_I8A8     = DVCPF_A8I8_BE,
     DVCPF_RGB15    = DVCPF_RGB15_LE,
     DVCPF_RGB16    = DVCPF_RGB16_LE,
     DVCPF_RGB24    = DVCPF_RGB24_LE,
     DVCPF_BGR24    = DVCPF_RGB24_BE,
     DVCPF_RGB32    = DVCPF_RGB32_LE,
     DVCPF_BGR32    = DVCPF_RGB32_BE,
     DVCPF_ARGB     = DVCPF_ARGB_LE,
     DVCPF_BGRA     = DVCPF_ARGB_BE,
     DVCPF_RGBA     = DVCPF_RGBA_LE,
     DVCPF_ABGR     = DVCPF_RGBA_BE,
     DVCPF_YUY2     = DVCPF_YUYV_LE,
     DVCPF_UYVY     = DVCPF_YUYV_BE,
     DVCPF_NV12     = DVCPF_NV12_LE,
     DVCPF_NV21     = DVCPF_NV12_BE
#endif
} DVCPixelFormat;

#define DVC_NUM_PIXELFORMATS  28

#define DVC_PIXELFORMAT_INDEX(f)   ((f) & 0x1f)
#define DVC_INDEXED_PIXELFORMAT(f) (((f) & (1<<5)) != 0)
#define DVC_ALPHA_PIXELFORMAT(f)   (((f) & (1<<6)) != 0)
#define DVC_YCBCR_PIXELFORMAT(f)   (((f) & (1<<7)) != 0)
#define DVC_NUM_PLANES(f)          (((f)>>8) & 0x03)
#define DVC_PLANE_BPS(f,i)         (((f)>>(10+(i)*3)) & 0x07)
#define DVC_PLANE_H_SHIFT(f,i)     (((f)>>(19+(i)*4)) & 0x03)
#define DVC_PLANE_V_SHIFT(f,i)     (((f)>>(21+(i)*4)) & 0x03)
#define DVC_PLANE_ALIGN(f,i)       (1 << DVC_PLANE_H_SHIFT(f,i))

static __inline__ DVCPixelFormat
dfb2dvc_pixelformat( DFBSurfacePixelFormat format )
{
     switch (format) {
          case DSPF_LUT8:
               return DVCPF_I8;
          case DSPF_ALUT44:
               return DVCPF_A4I4;
          case DSPF_RGB332:
               return DVCPF_RGB8;
          case DSPF_ARGB1555:
               return DVCPF_RGB15;
          case DSPF_RGB16:
               return DVCPF_RGB16;
          case DSPF_RGB24:
               return DVCPF_RGB24;
          case DSPF_RGB32:
               return DVCPF_RGB32;
          case DSPF_ARGB:
               return DVCPF_ARGB;
          case DSPF_YUY2:
               return DVCPF_YUY2;
          case DSPF_UYVY:
               return DVCPF_UYVY;
          case DSPF_NV12:
               return DVCPF_NV12;
          case DSPF_NV21:
               return DVCPF_NV21;
          case DSPF_I420:
          case DSPF_YV12:
               return DVCPF_YUV420;
          default:
               break;
     }
     
     return DVCPF_UNKNOWN;
}

static __inline__ DFBSurfacePixelFormat
dvc2dfb_pixelformat( DVCPixelFormat format )
{
     switch (format) {
          case DVCPF_I8:
               return DSPF_LUT8;
          case DVCPF_A4I4:
               return DSPF_ALUT44;
          case DVCPF_RGB8:
               return DSPF_RGB332;
          case DVCPF_RGB15:
               return DSPF_ARGB1555;
          case DVCPF_RGB16:
               return DSPF_RGB16;
          case DVCPF_RGB24:
               return DSPF_RGB24;
          case DVCPF_RGB32:
               return DSPF_RGB32;
          case DVCPF_ARGB:
               return DSPF_ARGB;
          case DVCPF_YUY2:
               return DSPF_YUY2;
          case DVCPF_UYVY:
               return DSPF_UYVY;
          case DVCPF_NV12:
               return DSPF_NV12;
          case DVCPF_NV21:
               return DSPF_NV21;
          case DVCPF_YUV420:
               return DSPF_I420;
          default:
               break;
     }
     
     return DSPF_UNKNOWN;
}

/* DVC Picture */
typedef struct {     
     DVCPixelFormat  format;
     int             width;
     int             height;
     
     void           *base[3];
     int             pitch[3];
     
     DFBColor       *palette;
     int             palette_size;
     
     bool            separated;
     bool            premultiplied;
} DVCPicture;

/* DVC ColorMap */
typedef union {
     struct {
          u8         b[256];
          u8         g[256];
          u8         r[256];
     } RGB;
     struct {
          u8         v[256];
          u8         u[256];
          u8         y[256];
     } YUV;
} DVCColormap;

/*
 * Generate a colormap for the given source format.
 */
void         dvc_colormap_gen( DVCColormap    *colormap,
                               DVCPixelFormat  format,
                               u16             brightness,
                               u16             contrast,
                               u16             saturation );

/*
 * Compute the total size in bytes of a picture.
 */
unsigned int dvc_picture_size( DVCPixelFormat  format,
                               int             width,
                               int             height );
/*
 * Initialize a picture.
 * Automatically compute plane pointer and pitch for
 * the given format and size.
 */
void         dvc_picture_init( DVCPicture     *picture,
                               DVCPixelFormat  format,
                               int             width,
                               int             height,
                               void           *base );
/*
 * Copy contents of "source" picture to "dest" picture 
 * performing format conversion when requested.
 * No clipping is done. 
 */
DFBResult    dvc_scale( const DVCPicture   *source,
                        const DVCPicture   *dest,
                        const DFBRectangle *srect,
                        const DFBRectangle *drect,
                        const DVCColormap  *colormap );                         
/*
 * Copy contents of "source" picture to "dest" surface
 * performing format conversion when requested.
 */                    
DFBResult    dvc_scale_to_surface( const DVCPicture   *source,
                                   IDirectFBSurface   *dest,
                                   const DFBRectangle *dest_rect,
                                   const DVCColormap  *colormap );

                                   
#ifdef __cplusplus
}
#endif

#endif /* __DVC_H__ */
