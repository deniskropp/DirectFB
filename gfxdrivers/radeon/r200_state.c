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


static __u32 r200SrcBlend[] = {
     SRC_BLEND_GL_ZERO,                 // DSBF_ZERO
     SRC_BLEND_GL_ONE,                  // DSBF_ONE
     SRC_BLEND_GL_SRC_COLOR,            // DSBF_SRCCOLOR
     SRC_BLEND_GL_ONE_MINUS_SRC_COLOR,  // DSBF_INVSRCCOLOR
     SRC_BLEND_GL_SRC_ALPHA,            // DSBF_SRCALPHA
     SRC_BLEND_GL_ONE_MINUS_SRC_ALPHA,  // DSBF_INVSRCALPHA
     SRC_BLEND_GL_DST_ALPHA,            // DSBF_DSTALPHA
     SRC_BLEND_GL_ONE_MINUS_DST_ALPHA,  // DSBF_INVDSTALPHA
     SRC_BLEND_GL_DST_COLOR,            // DSBF_DSTCOLOR
     SRC_BLEND_GL_ONE_MINUS_DST_COLOR,  // DSBF_INVDSTCOLOR
     SRC_BLEND_GL_SRC_ALPHA_SATURATE    // DSBF_SRCALPHASAT
};

static __u32 r200DstBlend[] = {
     DST_BLEND_GL_ZERO,                 // DSBF_ZERO
     DST_BLEND_GL_ONE,                  // DSBF_ONE
     DST_BLEND_GL_SRC_COLOR,            // DSBF_SRCCOLOR
     DST_BLEND_GL_ONE_MINUS_SRC_COLOR,  // DSBF_INVSRCCOLOR
     DST_BLEND_GL_SRC_ALPHA,            // DSBF_SRCALPHA
     DST_BLEND_GL_ONE_MINUS_SRC_ALPHA,  // DSBF_INVSRCALPHA
     DST_BLEND_GL_DST_ALPHA,            // DSBF_DSTALPHA
     DST_BLEND_GL_ONE_MINUS_DST_ALPHA,  // DSBF_INVDSTALPHA
     DST_BLEND_GL_DST_COLOR,            // DSBF_DSTCOLOR
     DST_BLEND_GL_ONE_MINUS_DST_COLOR,  // DSBF_INVDSTCOLOR
     DST_BLEND_GL_ZERO                  // DSBF_SRCALPHASAT
};


void r200_set_destination( RadeonDriverData *rdrv,
                           RadeonDeviceData *rdev,
                           CardState        *state )
{
     CoreSurface   *surface = state->destination;
     SurfaceBuffer *buffer  = surface->back_buffer;
     volatile __u8 *mmio    = rdrv->mmio_base;
     __u32          offset;
     __u32          pitch;
    
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
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB8;
                    break;
               case DSPF_RGB332:          
                    rdev->gui_master_cntl = GMC_DST_8BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB332 | DITHER_ENABLE;
                    break;
               case DSPF_ARGB2554:
                    rdev->gui_master_cntl = GMC_DST_16BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB565;
                    break;
               case DSPF_ARGB4444:
                    rdev->gui_master_cntl = GMC_DST_16BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_ARGB4444 | DITHER_ENABLE;
                    break;
               case DSPF_ARGB1555:          
                    rdev->gui_master_cntl = GMC_DST_15BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_ARGB1555 | DITHER_ENABLE;
                    break;
               case DSPF_RGB16:
                    rdev->gui_master_cntl = GMC_DST_16BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB565 | DITHER_ENABLE;
                    break;
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
               case DSPF_AYUV:
                    rdev->gui_master_cntl = GMC_DST_32BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_ARGB8888;
                    break;
               case DSPF_UYVY:
                    rdev->gui_master_cntl = GMC_DST_YVYU;
                    rdev->rb3d_cntl = COLOR_FORMAT_YUV422_YVYU;
                    dst_422 = true;
                    break;
               case DSPF_YUY2:
                    rdev->gui_master_cntl = GMC_DST_VYUY;
                    rdev->rb3d_cntl = COLOR_FORMAT_YUV422_VYUY;
                    dst_422 = true;
                    break;
               case DSPF_I420:
                    rdev->gui_master_cntl = GMC_DST_8BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB8;
                    rdev->dst_offset_cb = offset + pitch * surface->height;
                    rdev->dst_offset_cr = rdev->dst_offset_cb + 
                                          pitch/2 * surface->height/2;
                    break;
               case DSPF_YV12:
                    rdev->gui_master_cntl = GMC_DST_8BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB8;
                    rdev->dst_offset_cr = offset + pitch * surface->height;
                    rdev->dst_offset_cb = rdev->dst_offset_cr +
                                          pitch/2 * surface->height/2;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    return;
          }

          rdev->gui_master_cntl |= GMC_DP_SRC_SOURCE_MEMORY  |
                                   GMC_WR_MSK_DIS            |
                                   GMC_SRC_PITCH_OFFSET_CNTL |
                                   GMC_DST_PITCH_OFFSET_CNTL |
                                   GMC_DST_CLIPPING;
          
          radeon_waitfifo( rdrv, rdev, 2 ); 
          radeon_out32( mmio, DST_OFFSET, offset );
          radeon_out32( mmio, DST_PITCH,  pitch );
          
          radeon_waitfifo( rdrv, rdev, 2 );
          radeon_out32( mmio, RB3D_COLOROFFSET, offset );
          radeon_out32( mmio, RB3D_COLORPITCH,  
                              pitch / DFB_BYTES_PER_PIXEL(buffer->format) );
          
          if (surface->caps & DSCAPS_DEPTH) {
               SurfaceBuffer *depth = surface->depth_buffer;
               
               offset = radeon_buffer_offset( rdev, depth );
               pitch  = depth->video.pitch >> 1;
               
               radeon_waitfifo( rdrv, rdev, 3 );
               radeon_out32( mmio, RB3D_DEPTHOFFSET, offset );
               radeon_out32( mmio, RB3D_DEPTHPITCH,  pitch );
               radeon_out32( mmio, RB3D_ZSTENCILCNTL, DEPTH_FORMAT_16BIT_INT_Z |
                                                      Z_TEST_ALWAYS );
          
               rdev->rb3d_cntl |= Z_ENABLE;
          }
          
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

void r200_set_source( RadeonDriverData *rdrv,
                      RadeonDeviceData *rdev,
                      CardState        *state )
{
     CoreSurface   *surface  = state->source;
     SurfaceBuffer *buffer   = surface->front_buffer;
     volatile __u8 *mmio     = rdrv->mmio_base;
     __u32          txformat = R200_TXFORMAT_NON_POWER2;
     __u32          txfilter = R200_MAG_FILTER_LINEAR  |
                               R200_MIN_FILTER_LINEAR  |
                               R200_CLAMP_S_CLAMP_LAST |
                               R200_CLAMP_T_CLAMP_LAST;
     
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
               txformat |= R200_TXFORMAT_I8;
               txfilter &= ~(R200_MAG_FILTER_LINEAR |
                             R200_MIN_FILTER_LINEAR);
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_ALUT44:
               txformat |= R200_TXFORMAT_I8;
               txfilter &= ~(R200_MAG_FILTER_LINEAR |
                             R200_MIN_FILTER_LINEAR);
               rdev->src_mask = 0x0000000f;
               break;
          case DSPF_A8:
               txformat |= R200_TXFORMAT_I8 |
                           R200_TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0;
               break;
          case DSPF_RGB332:
               txformat |= R200_TXFORMAT_RGB332;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_ARGB2554:
               txformat |= R200_TXFORMAT_RGB565;
               txfilter &= ~(R200_MAG_FILTER_LINEAR |
                             R200_MIN_FILTER_LINEAR);
               rdev->src_mask = 0x00003fff;
               break;
          case DSPF_ARGB4444:
               txformat |= R200_TXFORMAT_ARGB4444 |
                           R200_TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0x00000fff;
               break;
          case DSPF_ARGB1555:
               txformat |= R200_TXFORMAT_ARGB1555 |
                           R200_TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0x00007fff;
               break;
          case DSPF_RGB16:
               txformat |= R200_TXFORMAT_RGB565;
               rdev->src_mask = 0x0000ffff;
               break;
          case DSPF_RGB32:
               txformat |= R200_TXFORMAT_ARGB8888;
               rdev->src_mask = 0x00ffffff;
               break;
          case DSPF_ARGB:
          case DSPF_AiRGB:
          case DSPF_AYUV:
               txformat |= R200_TXFORMAT_ARGB8888 |
                           R200_TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0x00ffffff;
               break;
          case DSPF_UYVY:
               txformat |= R200_TXFORMAT_YVYU422;
               if (!rdev->dst_422)
                    txfilter |= R200_YUV_TO_RGB;
               rdev->src_mask = 0xffffffff;
               break;
          case DSPF_YUY2:
               txformat |= R200_TXFORMAT_VYUY422;
               if (!rdev->dst_422)
                    txfilter |= R200_YUV_TO_RGB;
               rdev->src_mask = 0xffffffff;
               break;
          case DSPF_I420:
               txformat |= R200_TXFORMAT_I8;
               rdev->src_offset_cb = rdev->src_offset +
                                     rdev->src_pitch * rdev->src_height;
               rdev->src_offset_cr = rdev->src_offset_cb +
                                     rdev->src_pitch/2 * rdev->src_height/2;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_YV12:
               txformat |= R200_TXFORMAT_I8;
               rdev->src_offset_cr = rdev->src_offset +
                                     rdev->src_pitch * rdev->src_height;
               rdev->src_offset_cb = rdev->src_offset_cr +
                                     rdev->src_pitch/2 * rdev->src_height/2;
               rdev->src_mask = 0x000000ff;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               return;
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
           
     radeon_waitfifo( rdrv, rdev, 6 );
     radeon_out32( mmio, R200_PP_TXFILTER_0, txfilter );
     radeon_out32( mmio, R200_PP_TXFORMAT_0, txformat );
     radeon_out32( mmio, R200_PP_TXFORMAT_X_0, 0 );
     radeon_out32( mmio, R200_PP_TXSIZE_0, ((rdev->src_height-1) << 16) | 
                                           ((rdev->src_width-1) & 0xffff) );
     radeon_out32( mmio, R200_PP_TXPITCH_0, rdev->src_pitch - 32 );
     radeon_out32( mmio, R200_PP_TXOFFSET_0, rdev->src_offset );
     
     if (rdev->src_format != buffer->format)
          RADEON_UNSET( BLITTING_FLAGS );
     rdev->src_format = buffer->format;

     RADEON_SET( SOURCE );
}

void r200_set_clip( RadeonDriverData *rdrv,
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
      
     /* 3d clip */
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, RE_TOP_LEFT, 
                 (clip->y1 << 16) | (clip->x1 & 0xffff) );
     radeon_out32( mmio, RE_BOTTOM_RIGHT,
                 (clip->y2 << 16) | (clip->x2 & 0xffff) );
     
     rdev->clip = state->clip;
     
     RADEON_SET( CLIP );
}

#define R200_SET_YUV422_COLOR( rdrv, rdev, y, u, v ) {          \
     radeon_out32( (rdrv)->fb_base, (rdev)->yuv422_buffer,      \
                   PIXEL_YUY2( y, u, v ) );                     \
     radeon_in8( (rdrv)->fb_base, (rdev)->yuv422_buffer );      \
     radeon_waitfifo( rdrv, rdev, 1 );                          \
     radeon_out32( (rdrv)->mmio_base, R200_PP_TXOFFSET_1,       \
                   (rdev)->fb_offset + (rdev)->yuv422_buffer ); \
}

void r200_set_drawing_color( RadeonDriverData *rdrv,
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
               R200_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
               break;
          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color2d = PIXEL_YUY2( y, u, v );
               R200_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
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
     
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( rdrv->mmio_base, DP_BRUSH_FRGD_CLR, color2d );
     radeon_out32( rdrv->mmio_base, R200_PP_TFACTOR_1, color3d );

     RADEON_SET( COLOR );
}

void r200_set_blitting_color( RadeonDriverData *rdrv,
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
               R200_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
          default:
               color3d = PIXEL_ARGB( color.a, color.r,
                                     color.g, color.b );
               break;
     }
     
     radeon_waitfifo( rdrv, rdev, 1 );
     radeon_out32( rdrv->mmio_base, R200_PP_TFACTOR_0, color3d );
     
     RADEON_SET( COLOR );
}

void r200_set_src_colorkey( RadeonDriverData *rdrv,
                            RadeonDeviceData *rdev,
                            CardState        *state )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     
     if (RADEON_IS_SET( SRC_COLORKEY ))
          return;
     
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, CLR_CMP_CLR_SRC, state->src_colorkey ); 
     radeon_out32( mmio, CLR_CMP_MASK, rdev->src_mask );    
     
     RADEON_SET( SRC_COLORKEY );
}

void
r200_set_blend_function( RadeonDriverData *rdrv,
                         RadeonDeviceData *rdev,
                         CardState        *state )
{
     volatile __u8 *mmio   = rdrv->mmio_base;
     __u32          sblend;
     __u32          dblend;
     
     if (RADEON_IS_SET( SRC_BLEND ) && RADEON_IS_SET( DST_BLEND ))
          return;

     sblend = r200SrcBlend[state->src_blend-1];
     dblend = r200DstBlend[state->dst_blend-1];

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

     radeon_waitfifo( rdrv, rdev, 1 ); 
     radeon_out32( mmio, RB3D_BLENDCNTL, sblend | dblend );
     
     RADEON_SET( SRC_BLEND );
     RADEON_SET( DST_BLEND );
}

/* NOTES:
 * - We use texture unit 0 for blitting functions,
 *          texture unit 1 for drawing functions
 * - Default blend equation is ADD_CLAMP (A * B + C)
 */

void r200_set_drawingflags( RadeonDriverData *rdrv,
                            RadeonDeviceData *rdev,
                            CardState        *state )
{
     volatile __u8 *mmio        = rdrv->mmio_base;
     __u32          master_cntl = rdev->gui_master_cntl       |
                                  GMC_SRC_DATATYPE_MONO_FG_LA | 
                                  GMC_BRUSH_SOLID_COLOR       |
                                  GMC_CLR_CMP_CNTL_DIS;
     __u32          rb3d_cntl   = rdev->rb3d_cntl & ~DITHER_ENABLE;
     __u32          pp_cntl     = TEX_BLEND_1_ENABLE;
     __u32          cblend      = R200_TXC_ARG_C_TFACTOR_COLOR;
     
     if (RADEON_IS_SET( DRAWING_FLAGS ))
          return;

     if (rdev->dst_422) {
          pp_cntl     |= TEX_1_ENABLE;
          cblend       = R200_TXC_ARG_C_R1_COLOR;
     }
     
     if (state->drawingflags & DSDRAW_BLEND) {
          rb3d_cntl   |= ALPHA_BLEND_ENABLE;
     }
     else if (rdev->dst_format == DSPF_A8) {
          cblend       = R200_TXC_ARG_C_TFACTOR_ALPHA;
     }

     if (state->drawingflags & DSDRAW_XOR) {
          rb3d_cntl   |= ROP_ENABLE;
          master_cntl |= GMC_ROP3_PATXOR;
     } else
          master_cntl |= GMC_ROP3_PATCOPY;
     
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl );
     radeon_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
     
     radeon_waitfifo( rdrv, rdev, 9 );
     radeon_out32( mmio, RB3D_CNTL, rb3d_cntl );
     radeon_out32( mmio, SE_CNTL, DIFFUSE_SHADE_FLAT  |
                                  ALPHA_SHADE_FLAT    |
                                  BFACE_SOLID         |
                                  FFACE_SOLID         |
                                  VTX_PIX_CENTER_OGL  |
                                  ROUND_MODE_ROUND    |
				              ROUND_PREC_4TH_PIX );
     radeon_out32( mmio, PP_CNTL, pp_cntl );
     radeon_out32( mmio, R200_PP_TXCBLEND_1, cblend );
     radeon_out32( mmio, R200_PP_TXCBLEND2_1, (1 << R200_TXC_TFACTOR_SEL_SHIFT) |
                                              R200_TXC_OUTPUT_REG_R0            |
                                              R200_TXC_CLAMP_0_1 );
     radeon_out32( mmio, R200_PP_TXABLEND_1, R200_TXA_ARG_C_TFACTOR_ALPHA );
     radeon_out32( mmio, R200_PP_TXABLEND2_1, (1 << R200_TXA_TFACTOR_SEL_SHIFT) |
                                              R200_TXA_OUTPUT_REG_R0            |
                                              R200_TXA_CLAMP_0_1 );
     radeon_out32( mmio, R200_SE_VTX_FMT_0, R200_VTX_XY );
     radeon_out32( mmio, R200_SE_VTX_FMT_1, 0 );
     
     rdev->drawingflags = state->drawingflags;

     RADEON_SET  ( DRAWING_FLAGS );
     RADEON_UNSET( BLITTING_FLAGS );
}

void r200_set_blittingflags( RadeonDriverData *rdrv,
                             RadeonDeviceData *rdev,
                             CardState        *state )
{
     volatile __u8 *mmio        = rdrv->mmio_base;
     __u32          master_cntl = rdev->gui_master_cntl |
                                  GMC_BRUSH_NONE        |
                                  GMC_SRC_DATATYPE_COLOR;
     __u32          cmp_cntl    = 0;
     __u32          rb3d_cntl   = rdev->rb3d_cntl;
     __u32          se_cntl     = BFACE_SOLID        |
                                  FFACE_SOLID        |
                                  VTX_PIX_CENTER_OGL |
                                  ROUND_MODE_ROUND;
     __u32          pp_cntl     = TEX_0_ENABLE;
     __u32          cblend      = R200_TXC_ARG_C_R0_COLOR;
     __u32          ablend      = R200_TXA_ARG_C_R0_ALPHA;
     __u32          vtx_fmt     = R200_VTX_XY;
     __u32          vte_cntl;
     
     if (RADEON_IS_SET( BLITTING_FLAGS ))
          return;
 
     if (rdev->accel == DFXL_TEXTRIANGLES) {
          se_cntl  |= DIFFUSE_SHADE_GOURAUD  |
                      ALPHA_SHADE_GOURAUD    |
                      SPECULAR_SHADE_GOURAUD |
                      FLAT_SHADE_VTX_LAST    |
                      ROUND_PREC_8TH_PIX;
          vtx_fmt  |= R200_VTX_Z0 | R200_VTX_W0;
          vte_cntl  = 0;
     }
     else {
          se_cntl  |= DIFFUSE_SHADE_FLAT |
                      ALPHA_SHADE_FLAT   |
                      ROUND_PREC_4TH_PIX; 
          vte_cntl  = R200_VTX_ST_DENORMALIZED;
     }
    
     if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                 DSBLIT_BLEND_ALPHACHANNEL)) {
          if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
               if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                    ablend = R200_TXA_ARG_A_R0_ALPHA | R200_TXA_ARG_B_TFACTOR_ALPHA;
               else
                    ablend = R200_TXA_ARG_C_TFACTOR_ALPHA;

               pp_cntl |= TEX_BLEND_0_ENABLE;
          }
          
          rb3d_cntl |= ALPHA_BLEND_ENABLE;
     }

     if (rdev->dst_format != DSPF_A8) {    
          if (state->blittingflags & DSBLIT_COLORIZE) {
               if (rdev->dst_422) {
                    cblend = (rdev->src_format == DSPF_A8)
                             ? (R200_TXC_ARG_C_R1_COLOR)
                             : (R200_TXC_ARG_A_R0_COLOR | R200_TXC_ARG_B_R1_COLOR);

                    pp_cntl |= TEX_1_ENABLE;
               }
               else {
                    cblend = (rdev->src_format == DSPF_A8)
                             ? (R200_TXC_ARG_C_TFACTOR_COLOR)
                             : (R200_TXC_ARG_A_R0_COLOR | R200_TXC_ARG_B_TFACTOR_COLOR);
               }
          
               pp_cntl |= TEX_BLEND_0_ENABLE;
          }
          else if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
               cblend = (rdev->src_format == DSPF_A8)
                        ? (R200_TXC_ARG_C_R0_ALPHA)
                        : (R200_TXC_ARG_A_R0_COLOR | R200_TXC_ARG_B_TFACTOR_ALPHA);
            
               pp_cntl |= TEX_BLEND_0_ENABLE;
          }
     } /* DSPF_A8 */
     else {
          if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                      DSBLIT_BLEND_ALPHACHANNEL))
               cblend = R200_TXC_ARG_C_TFACTOR_COLOR;
          else
               cblend = R200_TXC_ARG_C_R0_ALPHA;

          pp_cntl |= TEX_BLEND_0_ENABLE;
     }
 
     if (state->blittingflags & DSBLIT_SRC_COLORKEY)
          cmp_cntl = SRC_CMP_EQ_COLOR | CLR_CMP_SRC_SOURCE;
     else
          master_cntl |= GMC_CLR_CMP_CNTL_DIS;

     if (state->blittingflags & DSBLIT_XOR) {
          master_cntl |= GMC_ROP3_XOR;
          rb3d_cntl   |= ROP_ENABLE; 
     } else
          master_cntl |= GMC_ROP3_SRCCOPY;

     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, CLR_CMP_CNTL, cmp_cntl );
     radeon_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl );
     
     radeon_waitfifo( rdrv, rdev, 10 );
     radeon_out32( mmio, RB3D_CNTL, rb3d_cntl );
     radeon_out32( mmio, SE_CNTL, se_cntl );
     radeon_out32( mmio, PP_CNTL, pp_cntl );
     radeon_out32( mmio, R200_PP_TXCBLEND_0, cblend );
     radeon_out32( mmio, R200_PP_TXCBLEND2_0, R200_TXC_OUTPUT_REG_R0 |
                                              R200_TXC_CLAMP_0_1 );
     radeon_out32( mmio, R200_PP_TXABLEND_0, ablend );
     radeon_out32( mmio, R200_PP_TXABLEND2_0, R200_TXA_OUTPUT_REG_R0 |
                                              R200_TXA_CLAMP_0_1 );
     radeon_out32( mmio, R200_SE_VTX_FMT_0, vtx_fmt );
     radeon_out32( mmio, R200_SE_VTX_FMT_1, 2 << R200_VTX_TEX0_COMP_CNT_SHIFT );
     radeon_out32( mmio, R200_SE_VTE_CNTL, vte_cntl );
     
     rdev->blittingflags = state->blittingflags;
     
     RADEON_SET  ( BLITTING_FLAGS );
     RADEON_UNSET( DRAWING_FLAGS );
}

