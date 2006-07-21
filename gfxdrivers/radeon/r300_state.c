/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI Radeon cards written by
 *             Claudio Ciccani <klan@users.sf.net>.  
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include "radeon.h"
#include "radeon_regs.h"
#include "radeon_mmio.h"
#include "radeon_state.h"


static __u32 r300SrcBlend[] = {
     R300_SRC_BLEND_GL_ZERO,                 // DSBF_ZERO
     R300_SRC_BLEND_GL_ONE,                  // DSBF_ONE
     R300_SRC_BLEND_GL_SRC_COLOR,            // DSBF_SRCCOLOR
     R300_SRC_BLEND_GL_ONE_MINUS_SRC_COLOR,  // DSBF_INVSRCCOLOR
     R300_SRC_BLEND_GL_SRC_ALPHA,            // DSBF_SRCALPHA
     R300_SRC_BLEND_GL_ONE_MINUS_SRC_ALPHA,  // DSBF_INVSRCALPHA
     R300_SRC_BLEND_GL_DST_ALPHA,            // DSBF_DSTALPHA
     R300_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA,  // DSBF_INVDSTALPHA
     R300_SRC_BLEND_GL_DST_COLOR,            // DSBF_DSTCOLOR
     R300_SRC_BLEND_GL_ONE_MINUS_DST_COLOR,  // DSBF_INVDSTCOLOR
     R300_SRC_BLEND_GL_SRC_ALPHA_SATURATE    // DSBF_SRCALPHASAT
};

static __u32 r300DstBlend[] = {
     R300_DST_BLEND_GL_ZERO,                 // DSBF_ZERO
     R300_DST_BLEND_GL_ONE,                  // DSBF_ONE
     R300_DST_BLEND_GL_SRC_COLOR,            // DSBF_SRCCOLOR
     R300_DST_BLEND_GL_ONE_MINUS_SRC_COLOR,  // DSBF_INVSRCCOLOR
     R300_DST_BLEND_GL_SRC_ALPHA,            // DSBF_SRCALPHA
     R300_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA,  // DSBF_INVSRCALPHA
     R300_DST_BLEND_GL_DST_ALPHA,            // DSBF_DSTALPHA
     R300_DST_BLEND_GL_ONE_MINUS_DST_ALPHA,  // DSBF_INVDSTALPHA
     R300_DST_BLEND_GL_DST_COLOR,            // DSBF_DSTCOLOR
     R300_DST_BLEND_GL_ONE_MINUS_DST_COLOR,  // DSBF_INVDSTCOLOR
     R300_DST_BLEND_GL_ZERO                  // DSBF_SRCALPHASAT
};



void r300_set_destination( RadeonDriverData *rdrv,
                           RadeonDeviceData *rdev,
                           CardState        *state )
{
     CoreSurface   *surface = state->destination;
     SurfaceBuffer *buffer  = surface->back_buffer;
     volatile __u8 *mmio    = rdrv->mmio_base;
     __u32          offset;
     __u32          pitch;
     __u32          format  = 0;
    
     if (RADEON_IS_SET( DESTINATION ))
          return;
     
     D_ASSERT( (buffer->video.offset % 32) == 0 );
     D_ASSERT( (buffer->video.pitch % 32) == 0 );

     offset = radeon_buffer_offset( rdev, buffer );
     pitch  = buffer->video.pitch;
    
     if (rdev->dst_offset != offset        ||
         rdev->dst_pitch  != pitch         ||
         rdev->dst_format != buffer->format)
     {
          bool dst_422 = false;
          
          switch (buffer->format) {
               case DSPF_LUT8:
               case DSPF_ALUT44:
               case DSPF_A8:
                    rdev->gui_master_cntl = GMC_DST_8BPP;
                    format = 0;
                    break;
               case DSPF_RGB332:          
                    rdev->gui_master_cntl = GMC_DST_8BPP;
                    format = 0;
                    break;
               case DSPF_ARGB2554:
                    rdev->gui_master_cntl = GMC_DST_16BPP;
                    format = 0;
                    break;
               case DSPF_ARGB4444:
                    rdev->gui_master_cntl = GMC_DST_16BPP;
                    format = 0;
                    break;
               case DSPF_ARGB1555:          
                    rdev->gui_master_cntl = GMC_DST_15BPP;
                    format = 0;
                    break;
               case DSPF_RGB16:
                    rdev->gui_master_cntl = GMC_DST_16BPP;
                    format = R300_COLOR_FORMAT_RGB565;
                    break;
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
               case DSPF_AYUV:
                    rdev->gui_master_cntl = GMC_DST_32BPP;
                    format = R300_COLOR_FORMAT_ARGB8888;
                    break;
               case DSPF_UYVY:
                    rdev->gui_master_cntl = GMC_DST_YVYU;
                    format = 0;
                    dst_422 = true;
                    break;
               case DSPF_YUY2:
                    rdev->gui_master_cntl = GMC_DST_VYUY;
                    format = 0;
                    dst_422 = true;
                    break;
               case DSPF_I420:
                    rdev->gui_master_cntl = GMC_DST_8BPP;
                    format = 0;
                    rdev->dst_offset_cb = offset + pitch * surface->height;
                    rdev->dst_offset_cr = rdev->dst_offset_cb + 
                                          pitch/2 * surface->height/2;
                    break;
               case DSPF_YV12:
                    rdev->gui_master_cntl = GMC_DST_8BPP;
                    format = 0;
                    rdev->dst_offset_cr = offset + pitch * surface->height;
                    rdev->dst_offset_cb = rdev->dst_offset_cr +
                                          pitch/2 * surface->height/2;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    break;
          }

          rdev->gui_master_cntl |= GMC_DP_SRC_SOURCE_MEMORY  |
                                   GMC_WR_MSK_DIS            |
                                   GMC_SRC_PITCH_OFFSET_CNTL |
                                   GMC_DST_PITCH_OFFSET_CNTL |
                                   GMC_DST_CLIPPING;
          
          radeon_waitfifo( rdrv, rdev, 2 ); 
          radeon_out32( mmio, DST_OFFSET, offset );
          radeon_out32( mmio, DST_PITCH,  pitch );
         
#if 0
          radeon_waitfifo( rdrv, rdev, 2 );
          radeon_out32( mmio, R300_RB3D_COLOROFFSET0, offset );
          radeon_out32( mmio, R300_RB3D_COLORPITCH0, 
                      (pitch / DFB_BYTES_PER_PIXEL(buffer->format)) | format ); 
          
          if (surface->caps & DSCAPS_DEPTH) {
               SurfaceBuffer *depth = surface->depth_buffer;
               
               offset = radeon_buffer_offset( rdev, depth );
               pitch  = depth->video.pitch >> 1;
               
               radeon_waitfifo( rdrv, rdev, 4 );
               radeon_out32( mmio, R300_RB3D_DEPTHOFFSET, offset );
               radeon_out32( mmio, R300_RB3D_DEPTHPITCH, pitch );
               radeon_out32( mmio, R300_RB3D_ZSTENCIL_FORMAT,
                                   R300_DEPTH_FORMAT_16BIT_INT_Z );
               radeon_out32( mmio, R300_RB3D_ZSTENCIL_CNTL_0, 0 );
          } 
          else {
               radeoon_waitfifo( rdrv, rdev, 1 );
               radeon_out32( mmio, R300_RB3D_ZSTENCIL_CNTL_0,
                                   R300_RB3D_Z_DISABLED_1 );
          }               
#endif    
          if (rdev->dst_format != buffer->format) {
               if (dst_422 && !rdev->dst_422) {
                    RADEON_UNSET( SOURCE );
                    RADEON_UNSET( CLIP );
               }
               
               RADEON_UNSET( COLOR );
               RADEON_UNSET( SRC_BLEND );
          }
          
          rdev->dst_format = buffer->format;
          rdev->dst_offset = offset;
          rdev->dst_pitch  = pitch;
          rdev->dst_422    = dst_422;
     }

     RADEON_SET( DESTINATION );
}

void r300_set_source( RadeonDriverData *rdrv,
                      RadeonDeviceData *rdev,
                      CardState        *state )
{
     CoreSurface   *surface  = state->source;
     SurfaceBuffer *buffer   = surface->front_buffer;
     volatile __u8 *mmio     = rdrv->mmio_base;
     __u32          txformat = 0;
     __u32          txfilter = R300_TX_CLAMP             |
                               R300_TX_MAG_FILTER_LINEAR |
                               R300_TX_MIN_FILTER_LINEAR;

     if (RADEON_IS_SET( SOURCE )) {
          if ((state->blittingflags & DSBLIT_DEINTERLACE) ==
              (rdev->blittingflags  & DSBLIT_DEINTERLACE))
               return;
     }

     D_ASSERT( (buffer->video.offset % 32) == 0 );
     D_ASSERT( (buffer->video.pitch % 32) == 0 );
     
     rdev->src_offset = radeon_buffer_offset( rdev, buffer );
     rdev->src_pitch  = buffer->video.pitch;
     rdev->src_width  = surface->width;
     rdev->src_height = surface->height;

     switch (buffer->format) {
          case DSPF_LUT8:
               txformat |= R300_TXFORMAT_I8;
               txfilter &= ~(R300_TX_MAG_FILTER_LINEAR |
                             R300_TX_MIN_FILTER_LINEAR);
               txfilter |= R300_TX_MAG_FILTER_NEAREST |
                           R300_TX_MIN_FILTER_NEAREST;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_ALUT44:
               txformat |= R300_TXFORMAT_I8;
               txfilter &= ~(R300_TX_MAG_FILTER_LINEAR |
                             R300_TX_MIN_FILTER_LINEAR);
               txfilter |= R300_TX_MAG_FILTER_NEAREST |
                           R300_TX_MIN_FILTER_NEAREST;
               rdev->src_mask = 0x0000000f;
               break;
          case DSPF_A8:
               txformat |= R300_TXFORMAT_I8;
               rdev->src_mask = 0;
               break;
          case DSPF_RGB332:
               txformat |= R300_TXFORMAT_RGB332;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_ARGB2554:
               txformat |= R300_TXFORMAT_RGB565;
               txfilter &= ~(R300_TX_MAG_FILTER_LINEAR |
                             R300_TX_MIN_FILTER_LINEAR);
               txfilter |= R300_TX_MAG_FILTER_NEAREST |
                           R300_TX_MIN_FILTER_NEAREST;
               rdev->src_mask = 0x00003fff;
               break;
          case DSPF_ARGB4444:
               txformat |= R300_TXFORMAT_ARGB4444;
               rdev->src_mask = 0x00000fff;
               break;
          case DSPF_ARGB1555:
               txformat |= R300_TXFORMAT_ARGB1555;
               rdev->src_mask = 0x00007fff;
               break;
          case DSPF_RGB16:
               txformat |= R300_TXFORMAT_RGB565;
               rdev->src_mask = 0x0000ffff;
               break;
          case DSPF_RGB32:
               txformat |= R300_TXFORMAT_ARGB8888;
               rdev->src_mask = 0x00ffffff;
               break;
          case DSPF_ARGB:
          case DSPF_AiRGB:
          case DSPF_AYUV:
               txformat |= R300_TXFORMAT_ARGB8888;
               rdev->src_mask = 0x00ffffff;
               break;
          case DSPF_UYVY:
               //txformat |= R300_TXFORMAT_YVYU422;
               rdev->src_mask = 0xffffffff;
               break;
          case DSPF_YUY2:
               txformat |= R300_TXFORMAT_VYUY422;
               rdev->src_mask = 0xffffffff;
               break;
          case DSPF_I420:
               txformat |= R300_TXFORMAT_I8;
               rdev->src_offset_cb = rdev->src_offset +
                                     rdev->src_pitch * rdev->src_height;
               rdev->src_offset_cr = rdev->src_offset_cb +
                                     rdev->src_pitch/2 * rdev->src_height/2;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_YV12:
               txformat |= R300_TXFORMAT_I8;
               rdev->src_offset_cr = rdev->src_offset +
                                     rdev->src_pitch * rdev->src_height;
               rdev->src_offset_cb = rdev->src_offset_cr +
                                     rdev->src_pitch/2 * rdev->src_height/2;
               rdev->src_mask = 0x000000ff;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               break;
     }

     if (state->blittingflags & DSBLIT_DEINTERLACE) { 
          rdev->src_height /= 2;
          if (surface->caps & DSCAPS_SEPARATED) {
               if (surface->field) {
                    rdev->src_offset    += rdev->src_height * rdev->src_pitch;
                    rdev->src_offset_cr += rdev->src_height * rdev->src_pitch/4;
                    rdev->src_offset_cb += rdev->src_height * rdev->src_pitch/4;
               }
          } else {
               if (surface->field) {
                    rdev->src_offset    += rdev->src_pitch;
                    rdev->src_offset_cr += rdev->src_pitch/2;
                    rdev->src_offset_cb += rdev->src_pitch/2;
               }
               rdev->src_pitch *= 2;
          }
     }
 
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, SRC_OFFSET, rdev->src_offset );
     radeon_out32( mmio, SRC_PITCH,  rdev->src_pitch );
#if 0     
     radeon_waitfifo( rdrv, rdev, 6 );
     radeon_out32( mmio, R300_TX_FILTER_0, txfilter );
     radeon_out32( mmio, R300_TX_FILTER1_0, R300_TX_TRI_PERF_0_8 );
     radeon_out32( mmio, R300_TX_FORMAT_0, txformat );
     radeon_out32( mmio, R300_TX_SIZE_0,  rdev->src_width         |
                                         (rdev->src_height << 11) |
                                         R300_TX_SIZE_TXPITCH_EN );
     radeon_out32( mmio, R300_TX_PITCH_0,  rdev->src_pitch - 32 );
     radeon_out32( mmio, R300_TX_OFFSET_0, rdev->src_offset << R300_TXO_OFFSET_SHIFT );
#endif
     if (rdev->src_format != buffer->format)
          RADEON_UNSET( BLITTING_FLAGS );
     rdev->src_format = buffer->format;

     RADEON_SET( SOURCE );
}

void r300_set_clip( RadeonDriverData *rdrv,
                    RadeonDeviceData *rdev,
                    CardState        *state )
{
     DFBRegion     *clip = &state->clip;
     volatile __u8 *mmio = rdrv->mmio_base;
     
     if (RADEON_IS_SET( CLIP ))
          return;
  
     /* 2d clip */
     radeon_waitfifo( rdrv, rdev, 2 );
     if (rdev->dst_422) {
          radeon_out32( mmio, SC_TOP_LEFT,
                      (clip->y1 << 16) | (clip->x1/2 & 0xffff) );
          radeon_out32( mmio, SC_BOTTOM_RIGHT,
                      ((clip->y2+1) << 16) | ((clip->x2+1)/2 & 0xffff) );
     } else {     
          radeon_out32( mmio, SC_TOP_LEFT, 
                      (clip->y1 << 16) | (clip->x1 & 0xffff) );
          radeon_out32( mmio, SC_BOTTOM_RIGHT,
                      ((clip->y2+1) << 16) | ((clip->x2+1) & 0xffff) );
     }
#if 0
     /* 3d clip */
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, R300_RE_SCISSORS_TL,
                 ((clip->y1 << R300_SCISSORS_Y_SHIFT) & R300_SCISSORS_Y_MASK) |
                 ((clip->x1 << R300_SCISSORS_X_SHIFT) & R300_SCISSORS_X_MASK) );
     radeon_out32( mmio, R300_RE_SCISSORS_BR,
                 ((clip->y2 << R300_SCISSORS_Y_SHIFT) & R300_SCISSORS_Y_MASK) |
                 ((clip->x2 << R300_SCISSORS_X_SHIFT) & R300_SCISSORS_X_MASK) );
#endif
     rdev->clip = state->clip;
     
     RADEON_SET( CLIP );
}

#define R300_SET_YUV422_COLOR( rdrv, rdev, y, u, v ) {                \
     radeon_out32( (rdrv)->fb_base, (rdev)->yuv422_buffer,            \
                                     PIXEL_YUY2( y, u, v ) );         \
     radeon_in8( (rdrv)->fb_base, (rdev)->yuv422_buffer );            \
     radeon_waitfifo( rdrv, rdev, 1 );                                \
     radeon_out32( (rdrv)->mmio_base, R300_TX_OFFSET_1,               \
                   ((rdev)->fb_offset + (rdev)->yuv422_buffer) << 5 );\
}

void r300_set_drawing_color( RadeonDriverData *rdrv,
                             RadeonDeviceData *rdev,
                             CardState        *state )
{
     DFBColor color   = state->color;
     int      index   = state->color_index;
     __u32    color2d;
     __u32    color3d;
     int      y, u, v;

     if (RADEON_IS_SET( COLOR ) && RADEON_IS_SET( DRAWING_FLAGS ))
          return;

     color3d = PIXEL_ARGB( color.a, color.r,
                           color.g, color.b );
 
     switch (rdev->dst_format) {
          case DSPF_ALUT44:
               index |= (color.a & 0xf0);
          case DSPF_LUT8:
               color2d = index;
               color3d = PIXEL_RGB32( index, index, index );
               break;
          case DSPF_A8:
               color2d = color.a;
               color3d = (color.a << 24) | 0x00ffffff;
               break;
          case DSPF_RGB332:
               color2d = PIXEL_RGB332( color.r, color.g, color.b );
               break;
          case DSPF_ARGB2554:
               color2d = PIXEL_ARGB2554( color.a, color.r,
                                         color.g, color.b );
               break;
          case DSPF_ARGB4444:
               color2d = PIXEL_ARGB4444( color.a, color.r,
                                         color.g, color.b );
               break;
          case DSPF_ARGB1555:
               color2d = PIXEL_ARGB1555( color.a, color.r,
                                         color.g, color.b );
               break;
          case DSPF_RGB16:
               color2d = PIXEL_RGB16( color.r, color.g, color.b );
               break;
          case DSPF_RGB32:
               color2d = PIXEL_RGB32( color.r, color.g, color.b );
               break;
          case DSPF_ARGB:
               color2d = PIXEL_ARGB( color.a, color.r,
                                     color.g, color.b );
               break;
          case DSPF_AiRGB:
               color2d = PIXEL_AiRGB( color.a, color.r,
                                      color.g, color.b );
               break;
          case DSPF_AYUV:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color3d = color2d = PIXEL_AYUV( color.a, y, u, v );
               break;
          case DSPF_UYVY:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color2d = PIXEL_UYVY( y, u, v );
               //R300_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
               break;
          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color2d = PIXEL_YUY2( y, u, v );
               //R300_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
               break;
          case DSPF_I420:
          case DSPF_YV12:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               rdev->y_cop  = PIXEL_ARGB( color.a, y, y, y );
               rdev->cb_cop = PIXEL_ARGB( color.a, u, u, u );
               rdev->cr_cop = PIXEL_ARGB( color.a, v, v, v );
               color3d = color2d = rdev->y_cop;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               color2d = 0;
               break;
     }
     
     radeon_waitfifo( rdrv, rdev, 1 );
     radeon_out32( rdrv->mmio_base, DP_BRUSH_FRGD_CLR, color2d );
     /* missing 3d */

     RADEON_SET( COLOR );
}

void r300_set_blitting_color( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state )
{
     DFBColor color   = state->color;
     __u32    color3d;
     int      y, u, v;
     
     if (RADEON_IS_SET( COLOR ) && RADEON_IS_SET( BLITTING_FLAGS ))
          return;

     if (state->blittingflags & DSBLIT_COLORIZE &&
         state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          color.r = ((long) color.r * color.a / 255L);
          color.g = ((long) color.g * color.a / 255L);
          color.b = ((long) color.b * color.a / 255L);
     }

     switch (rdev->dst_format) {
          case DSPF_A8:
               color3d = (color.a << 24) | 0x00ffffff;
               break;
          case DSPF_I420:
          case DSPF_YV12: 
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               rdev->y_cop  = PIXEL_ARGB( color.a, y, y, y );
               rdev->cb_cop = PIXEL_ARGB( color.a, u, u, u );
               rdev->cr_cop = PIXEL_ARGB( color.a, v, v, v );
               color3d = rdev->y_cop;
               break;
          case DSPF_AYUV:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color3d = PIXEL_AYUV( color.a, y, u, v );
               break;
          case DSPF_UYVY:
          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               //R300_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
          default:
               color3d = PIXEL_ARGB( color.a, color.r,
                                     color.g, color.b );
               break;
     }
     
     /* missing */
     
     RADEON_SET( COLOR );
}

void r300_set_src_colorkey( RadeonDriverData *rdrv,
                            RadeonDeviceData *rdev,
                            CardState        *state )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     __u32          key  = state->src_colorkey;
     __u32          chroma;
     
     if (RADEON_IS_SET( SRC_COLORKEY ))
          return;
     
     switch (rdev->src_format) {
          case DSPF_RGB332:
               /*chroma = RGB332_TO_RGB32( key );*/
               break;
          case DSPF_ARGB4444:
               key |= 0xf000;
               /*chroma = ARGB4444_TO_RGB32( key );*/
               break;
          case DSPF_ARGB2554:
               key |= 0xc000; 
               /*chroma = ARGB2554_TO_RGB32( key );*/
               break;
          case DSPF_ARGB1555:
               key |= 0x8000;
               /*chroma = ARGB1555_TO_RGB32( key );*/
               break;
          case DSPF_RGB16:
               /*chroma = RGB16_TO_RGB32( key );*/
               break;
          case DSPF_RGB32:
               chroma = key;
               break;
          default:
               key |= 0xff000000;
               chroma = key;
               break;
     }
     chroma |= 0xff000000;               
     
     radeon_waitfifo( rdrv, rdev, 3 );
     radeon_out32( mmio, CLR_CMP_CLR_SRC, key );
     /* XXX: R300 seems to ignore CLR_CMP_MASK. */
     radeon_out32( mmio, CLR_CMP_MASK, rdev->src_mask );
     //radeon_out32( mmio, R300_TX_CHROMA_KEY_0, chroma );
     
     RADEON_SET( SRC_COLORKEY );
}

void
r300_set_blend_function( RadeonDriverData *rdrv,
                         RadeonDeviceData *rdev,
                         CardState        *state )
{
     __u32 sblend, dblend;
     
     if (RADEON_IS_SET( SRC_BLEND ) && RADEON_IS_SET( DST_BLEND ))
          return;

     sblend = r300SrcBlend[state->src_blend-1];
     dblend = r300DstBlend[state->dst_blend-1];

     if (!DFB_PIXELFORMAT_HAS_ALPHA( rdev->dst_format )) {
          switch (state->src_blend) {
               case DSBF_DESTALPHA:
                    sblend = SRC_BLEND_GL_ONE;
                    break;
               case DSBF_INVDESTALPHA:
                    sblend = SRC_BLEND_GL_ZERO;
                    break;
               default:
                    break;
          }
     }
     
     rdev->rb3d_blend = R300_BLEND_NO_SEPARATE | sblend | dblend;
     
     RADEON_UNSET( DRAWING_FLAGS );
     RADEON_UNSET( BLITTING_FLAGS );
     RADEON_SET( SRC_BLEND );
     RADEON_SET( DST_BLEND );
}

/* NOTES:
 * - We use texture unit 0 for blitting functions,
 *          texture unit 1 for drawing functions
 * - Default blend equation is ADD_CLAMP (A * B + C)
 */

void r300_set_drawingflags( RadeonDriverData *rdrv,
                            RadeonDeviceData *rdev,
                            CardState        *state )
{
     volatile __u8 *mmio        = rdrv->mmio_base;
     __u32          master_cntl = rdev->gui_master_cntl       |
                                  GMC_SRC_DATATYPE_MONO_FG_LA |
                                  GMC_BRUSH_SOLID_COLOR       |
                                  GMC_CLR_CMP_CNTL_DIS;
     __u32          rb3d_blend  = rdev->rb3d_blend;
     __u32          tex_enable  = 0;
     __u32          input_cntl1 = R300_INPUT_CNTL_POS |
                                  R300_INPUT_CNTL_COLOR;
     __u32          vtx_fmt1    = 0;
     
     if (RADEON_IS_SET( DRAWING_FLAGS ))
          return;

     if (rdev->dst_422) {
          tex_enable  |= TEX_1_ENABLE;
          input_cntl1 |= R300_INPUT_CNTL_TC1;
          vtx_fmt1    |= 2 << R300_VAP_OUTPUT_VTX_FMT_1__TEX_1_COMP_CNT_SHIFT;
     }
     
     if (state->drawingflags & DSDRAW_BLEND)
          rb3d_blend  |= R300_BLEND_ENABLE;

     if (state->drawingflags & DSDRAW_XOR)
          master_cntl |= GMC_ROP3_PATXOR;
     else
          master_cntl |= GMC_ROP3_PATCOPY;
     
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl );
     radeon_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
     
#if 0
     radeon_waitfifo( rdrv, rdev, 14 );
     radeon_out32( mmio, R300_TX_ENABLE, tex_enable );
     radeon_out32( mmio, R300_VAP_INPUT_ROUTE_0_0, 0x21030003 ); 
     radeon_out32( mmio, R300_VAP_INPUT_ROUTE_1_0, 0xf688f688 );
     radeon_out32( mmio, R300_VAP_INPUT_CNTL_0, 0x5555 );
     radeon_out32( mmio, R300_VAP_INPUT_CNTL_1, input_cntl1 );
     radeon_out32( mmio, R300_VAP_OUTPUT_VTX_FMT_0,
                         R300_VAP_OUTPUT_VTX_FMT_0__POS_PRESENT |
                         R300_VAP_OUTPUT_VTX_FMT_0__COLOR_PRESENT );
     radeon_out32( mmio, R300_VAP_OUTPUT_VTX_FMT_1, vtx_fmt1 );
     radeon_out32( mmio, R300_SE_VTE_CNTL, R300_VTX_W0_FMT | 
                                           R300_VTX_ST_DENORMALIZED );
     radeon_out32( mmio, R300_PFS_INSTR0_0, R300_FPI0_OUTC_MAD                |
                         (R300_FPI0_ARGC_SRC0C_XYZ << R300_FPI0_ARG0C_SHIFT) |
                         (R300_FPI0_ARGC_ONE       << R300_FPI0_ARG1C_SHIFT) |
                         (R300_FPI0_ARGC_ZERO      << R300_FPI0_ARG2C_SHIFT) );
     radeon_out32( mmio, R300_PFS_INSTR1_0, (0 << 23) | (7 << 26) );
     radeon_out32( mmio, R300_PFS_INSTR2_0, R300_FPI2_OUTA_MAD |
                         (R300_FPI2_ARGA_SRC0A << R300_FPI2_ARG0A_SHIFT) |
                         (R300_FPI2_ARGA_ONE   << R300_FPI2_ARG1A_SHIFT) |
                         (R300_FPI2_ARGA_ZERO  << R300_FPI2_ARG2A_SHIFT) );
     radeon_out32( mmio, R300_PFS_INSTR3_0, (0 << 23) | (1  << 26) );
     radeon_out32( mmio, R300_RB3D_CBLEND, rb3d_blend );
     radeon_out32( mmio, R300_RB3D_ABLEND, rb3d_blend );  
#endif
     rdev->drawingflags = state->drawingflags;

     RADEON_SET  ( DRAWING_FLAGS );
     RADEON_UNSET( BLITTING_FLAGS );
}

void r300_set_blittingflags( RadeonDriverData *rdrv,
                             RadeonDeviceData *rdev,
                             CardState      *state )
{
     volatile __u8 *mmio        = rdrv->mmio_base;
     __u32          master_cntl = rdev->gui_master_cntl |
                                  GMC_BRUSH_NONE        |
                                  GMC_SRC_DATATYPE_COLOR;
     __u32          cmp_cntl    = 0;
     __u32          rb3d_blend  = rdev->rb3d_blend;
     
     if (RADEON_IS_SET( BLITTING_FLAGS ))
          return;
          
     if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                 DSBLIT_BLEND_ALPHACHANNEL))
          rb3d_blend |= R300_BLEND_ENABLE;
 
     if (state->blittingflags & DSBLIT_SRC_COLORKEY)
          cmp_cntl = SRC_CMP_EQ_COLOR | CLR_CMP_SRC_SOURCE;
     else
          master_cntl |= GMC_CLR_CMP_CNTL_DIS;

     if (state->blittingflags & DSBLIT_XOR)
          master_cntl |= GMC_ROP3_XOR;
     else
          master_cntl |= GMC_ROP3_SRCCOPY;

     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, CLR_CMP_CNTL, cmp_cntl );
     radeon_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl );
     
#if 0
     radeon_waitfifo( rdrv, rdev, 14 );
     radeon_out32( mmio, R300_TX_ENABLE, R300_TX_ENABLE_0 );
     radeon_out32( mmio, R300_VAP_INPUT_ROUTE_0_0, 0x21030003 );
     radeon_out32( mmio, R300_VAP_INPUT_ROUTE_1_0, 0xf688f688 );
     radeon_out32( mmio, R300_VAP_INPUT_CNTL_0, 0x5555 );
     radeon_out32( mmio, R300_VAP_INPUT_CNTL_1, R300_INPUT_CNTL_POS |
                                                R300_INPUT_CNTL_TC0 );
     radeon_out32( mmio, R300_VAP_OUTPUT_VTX_FMT_0,
                         R300_VAP_OUTPUT_VTX_FMT_0__POS_PRESENT );
     radeon_out32( mmio, R300_VAP_OUTPUT_VTX_FMT_1,
                         4 << R300_VAP_OUTPUT_VTX_FMT_1__TEX_0_COMP_CNT_SHIFT );
     radeon_out32( mmio, R300_SE_VTE_CNTL, R300_VTX_W0_FMT ); 
     radeon_out32( mmio, R300_PFS_INSTR0_0, R300_FPI0_OUTC_MAD                |
                         (R300_FPI0_ARGC_SRC0C_XYZ  << R300_FPI0_ARG0C_SHIFT) |
                         (R300_FPI0_ARGC_ONE  << R300_FPI0_ARG1C_SHIFT) |
                         (R300_FPI0_ARGC_ZERO << R300_FPI0_ARG2C_SHIFT) );
     radeon_out32( mmio, R300_PFS_INSTR1_0, (0 << 23) | (7 << 26) );
     radeon_out32( mmio, R300_PFS_INSTR2_0, R300_FPI2_OUTA_MAD |
                         (R300_FPI2_ARGA_SRC0A << R300_FPI2_ARG0A_SHIFT) |
                         (R300_FPI2_ARGA_ONE   << R300_FPI2_ARG1A_SHIFT) |
                         (R300_FPI2_ARGA_ZERO  << R300_FPI2_ARG2A_SHIFT) );
     radeon_out32( mmio, R300_PFS_INSTR3_0, (0 << 23) | (1 << 26) );
     radeon_out32( mmio, R300_RB3D_CBLEND, rb3d_blend );
     radeon_out32( mmio, R300_RB3D_ABLEND, rb3d_blend );
#endif
     
     rdev->blittingflags = state->blittingflags;
     
     RADEON_SET  ( BLITTING_FLAGS );
     RADEON_UNSET( DRAWING_FLAGS );
}

