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


static const u32 r100SrcBlend[] = {
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

static const u32 r100DstBlend[] = {
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


void r100_restore( RadeonDriverData *rdrv, RadeonDeviceData *rdev )
{
     volatile u8 *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 10 );
     /* enable caches */
     radeon_out32( mmio, RB2D_DSTCACHE_MODE, RB2D_DC_2D_CACHE_AUTOFLUSH |
                                             RB2D_DC_3D_CACHE_AUTOFLUSH );
     radeon_out32( mmio, RB3D_DSTCACHE_MODE, RB3D_DC_2D_CACHE_AUTOFLUSH |
                                             RB3D_DC_3D_CACHE_AUTOFLUSH );            
     /* restore 3d engine state */ 
     radeon_out32( mmio, SE_COORD_FMT, VTX_XY_PRE_MULT_1_OVER_W0 |
                                       TEX1_W_ROUTING_USE_W0 );
     radeon_out32( mmio, SE_LINE_WIDTH, 0x10 );
#ifdef WORDS_BIGENDIAN
     radeon_out32( mmio, SE_CNTL_STATUS, TCL_BYPASS | VC_32BIT_SWAP );
#else
     radeon_out32( mmio, SE_CNTL_STATUS, TCL_BYPASS );
#endif
     radeon_out32( mmio, PP_MISC, ALPHA_TEST_PASS );
     radeon_out32( mmio, RB3D_ZSTENCILCNTL, Z_TEST_ALWAYS );
     radeon_out32( mmio, RB3D_ROPCNTL, ROP_XOR );
     /* set YUV422 color buffer */
     radeon_out32( mmio, PP_TXFILTER_1, 0 );
     radeon_out32( mmio, PP_TXFORMAT_1, TXFORMAT_VYUY422 );
}

void r100_set_destination( RadeonDriverData *rdrv,
                           RadeonDeviceData *rdev,
                           CardState        *state )
{
     CoreSurface   *surface = state->destination;
     SurfaceBuffer *buffer  = surface->back_buffer;
     volatile u8   *mmio    = rdrv->mmio_base;
     u32            offset;
     u32            pitch;
    
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
          
          radeon_waitfifo( rdrv, rdev, 4 );
          radeon_out32( mmio, DST_OFFSET, offset );
          radeon_out32( mmio, DST_PITCH, pitch );
          radeon_out32( mmio, RB3D_COLOROFFSET, offset );
          radeon_out32( mmio, RB3D_COLORPITCH,  
                              pitch / DFB_BYTES_PER_PIXEL(buffer->format) );
          
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

void r100_set_source( RadeonDriverData *rdrv,
                      RadeonDeviceData *rdev,
                      CardState        *state )
{
     CoreSurface   *surface  = state->source;
     SurfaceBuffer *buffer   = surface->front_buffer;
     volatile u8   *mmio     = rdrv->mmio_base;
     u32            txformat = TXFORMAT_NON_POWER2;
     u32            txfilter = MAG_FILTER_LINEAR  |
                               MIN_FILTER_LINEAR  |
                               CLAMP_S_CLAMP_LAST |
                               CLAMP_T_CLAMP_LAST;

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
               txformat |= TXFORMAT_I8;
               txfilter &= ~(MAG_FILTER_LINEAR |
                             MIN_FILTER_LINEAR);
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_ALUT44:
               txformat |= TXFORMAT_I8;
               txfilter &= ~(MAG_FILTER_LINEAR |
                             MIN_FILTER_LINEAR);
               rdev->src_mask = 0x0000000f;
               break;
          case DSPF_A8:
               txformat |= TXFORMAT_I8 |
                           TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0;
               break;
          case DSPF_RGB332:
               txformat |= TXFORMAT_RGB332;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_ARGB2554:
               txformat |= TXFORMAT_RGB565;
               txfilter &= ~(MAG_FILTER_LINEAR |
                             MIN_FILTER_LINEAR);
               rdev->src_mask = 0x00003fff;
               break;
          case DSPF_ARGB4444:
               txformat |= TXFORMAT_ARGB4444 |
                           TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0x00000fff;
               break;
          case DSPF_ARGB1555:
               txformat |= TXFORMAT_ARGB1555 |
                           TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0x00007fff;
               break;
          case DSPF_RGB16:
               txformat |= TXFORMAT_RGB565;
               rdev->src_mask = 0x0000ffff;
               break;
          case DSPF_RGB32:
               txformat |= TXFORMAT_ARGB8888;
               rdev->src_mask = 0x00ffffff;
               break;
          case DSPF_ARGB:
          case DSPF_AiRGB:
          case DSPF_AYUV:
               txformat |= TXFORMAT_ARGB8888 |
                           TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0x00ffffff;
               break;
          case DSPF_UYVY:
               txformat |= TXFORMAT_YVYU422;
               if (!rdev->dst_422)
                    txfilter |= YUV_TO_RGB;
               rdev->src_mask = 0xffffffff;
               break;
          case DSPF_YUY2:
               txformat |= TXFORMAT_VYUY422;
               if (!rdev->dst_422)
                    txfilter |= YUV_TO_RGB;
               rdev->src_mask = 0xffffffff;
               break;
          case DSPF_I420:
               txformat |= TXFORMAT_I8;
               rdev->src_offset_cb = rdev->src_offset +
                                     rdev->src_pitch * rdev->src_height;
               rdev->src_offset_cr = rdev->src_offset_cb +
                                     rdev->src_pitch/2 * rdev->src_height/2;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_YV12:
               txformat |= TXFORMAT_I8;
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
 
     radeon_waitfifo( rdrv, rdev, 7 );
     radeon_out32( mmio, SRC_OFFSET, rdev->src_offset );
     radeon_out32( mmio, SRC_PITCH, rdev->src_pitch );
     radeon_out32( mmio, PP_TXFILTER_0, txfilter );
     radeon_out32( mmio, PP_TXFORMAT_0, txformat );
     radeon_out32( mmio, PP_TEX_SIZE_0, ((rdev->src_height-1) << 16) |
                                        ((rdev->src_width-1) & 0xffff) );
     radeon_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch - 32 );
     radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset );
     
     if (rdev->src_format != buffer->format)
          RADEON_UNSET( BLITTING_FLAGS );
     rdev->src_format = buffer->format;

     RADEON_SET( SOURCE );
}

void r100_set_clip( RadeonDriverData *rdrv,
                    RadeonDeviceData *rdev,
                    CardState        *state )
{
     DFBRegion     *clip = &state->clip;
     volatile u8   *mmio = rdrv->mmio_base;
     
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

#define R100_SET_YUV422_COLOR( rdrv, rdev, y, u, v ) {          \
     radeon_out32( (rdrv)->fb_base, (rdev)->yuv422_buffer,      \
                   PIXEL_YUY2( y, u, v ) );                     \
     radeon_in8( (rdrv)->fb_base, (rdev)->yuv422_buffer );      \
     radeon_waitfifo( rdrv, rdev, 1 );                          \
     radeon_out32( (rdrv)->mmio_base, PP_TXOFFSET_1,            \
                   (rdev)->fb_offset + (rdev)->yuv422_buffer ); \
}

void r100_set_drawing_color( RadeonDriverData *rdrv,
                             RadeonDeviceData *rdev,
                             CardState        *state )
{
     DFBColor color   = state->color;
     int      index   = state->color_index;
     u32      color2d;
     u32      color3d;
     int      y, u, v;

     if (RADEON_IS_SET( COLOR ) && RADEON_IS_SET( DRAWING_FLAGS ))
          return;

     if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) {
          color.r = ((long) color.r * color.a / 255L);
          color.g = ((long) color.g * color.a / 255L);
          color.b = ((long) color.b * color.a / 255L);
     }

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
               R100_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
               break;
          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color2d = PIXEL_YUY2( y, u, v );
               R100_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
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
     radeon_out32( rdrv->mmio_base, PP_TFACTOR_1, color3d );

     RADEON_SET( COLOR );
}

void r100_set_blitting_color( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state )
{
     DFBColor color   = state->color;
     u32      color3d;
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
               R100_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
          default:
               color3d = PIXEL_ARGB( color.a, color.r,
                                     color.g, color.b );
               break;
     }
     
     radeon_waitfifo( rdrv, rdev, 1 );
     radeon_out32( rdrv->mmio_base, PP_TFACTOR_0, color3d );
     
     RADEON_SET( COLOR );
}

void r100_set_src_colorkey( RadeonDriverData *rdrv,
                            RadeonDeviceData *rdev,
                            CardState        *state )
{
     volatile u8 *mmio = rdrv->mmio_base;
     
     if (RADEON_IS_SET( SRC_COLORKEY ))
          return;
     
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, CLR_CMP_CLR_SRC, state->src_colorkey ); 
     radeon_out32( mmio, CLR_CMP_MASK, rdev->src_mask );    
     
     RADEON_SET( SRC_COLORKEY );
}

void
r100_set_blend_function( RadeonDriverData *rdrv,
                         RadeonDeviceData *rdev,
                         CardState        *state )
{
     volatile u8   *mmio   = rdrv->mmio_base;
     u32            sblend;
     u32            dblend;
     
     if (RADEON_IS_SET( SRC_BLEND ) && RADEON_IS_SET( DST_BLEND ))
          return;

     sblend = r100SrcBlend[state->src_blend-1];
     dblend = r100DstBlend[state->dst_blend-1];

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

void r100_set_drawingflags( RadeonDriverData *rdrv,
                            RadeonDeviceData *rdev,
                            CardState        *state )
{
     volatile u8   *mmio        = rdrv->mmio_base;
     u32            master_cntl = rdev->gui_master_cntl       |
                                  GMC_SRC_DATATYPE_MONO_FG_LA |
                                  GMC_BRUSH_SOLID_COLOR       |
                                  GMC_CLR_CMP_CNTL_DIS;
     u32            rb3d_cntl   = rdev->rb3d_cntl & ~DITHER_ENABLE;
     u32            pp_cntl     = SCISSOR_ENABLE | TEX_BLEND_1_ENABLE;
     u32            cblend      = COLOR_ARG_C_TFACTOR_COLOR;
     
     if (RADEON_IS_SET( DRAWING_FLAGS ))
          return;

     if (rdev->dst_422) {
          pp_cntl     |= TEX_1_ENABLE;
          cblend       = COLOR_ARG_C_T1_COLOR;
     }
     
     if (state->drawingflags & DSDRAW_BLEND) {
          rb3d_cntl   |= ALPHA_BLEND_ENABLE;
     }
     else if (rdev->dst_format == DSPF_A8) {
          cblend       = COLOR_ARG_C_TFACTOR_ALPHA;
     }

     if (state->drawingflags & DSDRAW_XOR) {
          rb3d_cntl   |= ROP_ENABLE;
          master_cntl |= GMC_ROP3_PATXOR;
     } else
          master_cntl |= GMC_ROP3_PATCOPY;
 
     radeon_waitfifo( rdrv, rdev, 8 );
     radeon_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl );
     radeon_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
     radeon_out32( mmio, RB3D_CNTL, rb3d_cntl );
     radeon_out32( mmio, SE_CNTL, DIFFUSE_SHADE_FLAT  |
                                  ALPHA_SHADE_FLAT    |
                                  BFACE_SOLID         |
                                  FFACE_SOLID         |
                                  VTX_PIX_CENTER_OGL  |
                                  ROUND_MODE_ROUND    |
				              ROUND_PREC_4TH_PIX );
     radeon_out32( mmio, PP_CNTL, pp_cntl );
     radeon_out32( mmio, PP_TXCBLEND_1, cblend );
     radeon_out32( mmio, PP_TXABLEND_1, ALPHA_ARG_C_TFACTOR_ALPHA );
     radeon_out32( mmio, SE_VTX_FMT, SE_VTX_FMT_XY );
     
     rdev->drawingflags = state->drawingflags;

     RADEON_SET  ( DRAWING_FLAGS );
     RADEON_UNSET( BLITTING_FLAGS );
}

void r100_set_blittingflags( RadeonDriverData *rdrv,
                             RadeonDeviceData *rdev,
                             CardState        *state )
{
     volatile u8   *mmio        = rdrv->mmio_base;
     u32            master_cntl = rdev->gui_master_cntl |
                                  GMC_BRUSH_NONE        |
                                  GMC_SRC_DATATYPE_COLOR;
     u32            cmp_cntl    = 0;
     u32            rb3d_cntl   = rdev->rb3d_cntl;
     u32            se_cntl     = BFACE_SOLID        |
                                  FFACE_SOLID        |
                                  VTX_PIX_CENTER_OGL |
                                  ROUND_MODE_ROUND;
     u32            pp_cntl     = SCISSOR_ENABLE    | 
                                  TEX_0_ENABLE      |
                                  TEX_BLEND_0_ENABLE;
     u32            cblend      = COLOR_ARG_C_T0_COLOR;
     u32            ablend      = ALPHA_ARG_C_T0_ALPHA;
     u32            vtx_fmt     = SE_VTX_FMT_XY | SE_VTX_FMT_ST0;
     u32            coord_fmt   = VTX_XY_PRE_MULT_1_OVER_W0 | 
                                  TEX1_W_ROUTING_USE_W0;
     
     if (RADEON_IS_SET( BLITTING_FLAGS ))
          return;
 
     if (rdev->accel == DFXL_TEXTRIANGLES) {
          se_cntl   |= DIFFUSE_SHADE_GOURAUD  |
                       ALPHA_SHADE_GOURAUD    |
                       SPECULAR_SHADE_GOURAUD |
                       FLAT_SHADE_VTX_LAST    |
                       ROUND_PREC_8TH_PIX;
          vtx_fmt   |= SE_VTX_FMT_W0 | SE_VTX_FMT_Z;
     }
     else {
          se_cntl   |= DIFFUSE_SHADE_FLAT |
                       ALPHA_SHADE_FLAT   |
                       ROUND_PREC_4TH_PIX;
          coord_fmt |= VTX_ST0_NONPARAMETRIC |
                       VTX_ST1_NONPARAMETRIC;
     }
    
     if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                 DSBLIT_BLEND_ALPHACHANNEL)) {
          if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
               if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                    ablend = ALPHA_ARG_A_T0_ALPHA | ALPHA_ARG_B_TFACTOR_ALPHA;
               else
                    ablend = ALPHA_ARG_C_TFACTOR_ALPHA;
          }
          
          rb3d_cntl |= ALPHA_BLEND_ENABLE;
     }

     if (rdev->dst_format != DSPF_A8) {    
          if (state->blittingflags & DSBLIT_COLORIZE) {
               if (rdev->dst_422) {
                    cblend = (rdev->src_format == DSPF_A8)
                             ? (COLOR_ARG_C_T1_COLOR)
                             : (COLOR_ARG_A_T0_COLOR | COLOR_ARG_B_T1_COLOR);

                    pp_cntl |= TEX_1_ENABLE;
               }
               else {
                    cblend = (rdev->src_format == DSPF_A8)
                             ? (COLOR_ARG_C_TFACTOR_COLOR)
                             : (COLOR_ARG_A_T0_COLOR | COLOR_ARG_B_TFACTOR_COLOR);
               }
          }
          else if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
               cblend = (rdev->src_format == DSPF_A8)
                        ? (COLOR_ARG_C_T0_ALPHA)
                        : (COLOR_ARG_A_T0_COLOR | COLOR_ARG_B_TFACTOR_ALPHA);
          }
     } /* DSPF_A8 */
     else {
          if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                      DSBLIT_BLEND_ALPHACHANNEL))
               cblend = COLOR_ARG_C_TFACTOR_COLOR;
          else
               cblend = COLOR_ARG_C_T0_ALPHA;
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

     radeon_waitfifo( rdrv, rdev, 9 );
     radeon_out32( mmio, CLR_CMP_CNTL, cmp_cntl );
     radeon_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl );
     radeon_out32( mmio, RB3D_CNTL, rb3d_cntl );
     radeon_out32( mmio, SE_CNTL, se_cntl );
     radeon_out32( mmio, PP_CNTL, pp_cntl );
     radeon_out32( mmio, PP_TXCBLEND_0, cblend );
     radeon_out32( mmio, PP_TXABLEND_0, ablend );
     radeon_out32( mmio, SE_VTX_FMT, vtx_fmt );
     radeon_out32( mmio, SE_COORD_FMT, coord_fmt );
     
     rdev->blittingflags = state->blittingflags;
     
     RADEON_SET  ( BLITTING_FLAGS );
     RADEON_UNSET( DRAWING_FLAGS );
}

