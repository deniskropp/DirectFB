/*
 * Copyright (C) 2005 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI R200 based chipsets written by
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

#include "r200.h"
#include "r200_regs.h"
#include "r200_mmio.h"

#include "r200_state.h"


#define R200_IS_SET( flag ) \
     ((rdev->set & SMF_##flag) == SMF_##flag)

#define R200_SET( flag ) \
     rdev->set |= SMF_##flag

#define R200_UNSET( flag ) \
     rdev->set &= ~(SMF_##flag)



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



void r200_set_destination( R200DriverData *rdrv,
                           R200DeviceData *rdev,
                           CardState      *state )
{
     CoreSurface   *surface = state->destination;
     SurfaceBuffer *buffer  = surface->back_buffer;
     volatile __u8 *mmio    = rdrv->mmio_base;
    
     if (R200_IS_SET( DESTINATION ))
          return;

     D_ASSERT( (buffer->video.offset % 128) == 0 );
     D_ASSERT( (buffer->video.pitch % 32) == 0 );
    
     if (rdev->dst_format != buffer->format       ||
         rdev->dst_offset != buffer->video.offset ||
         rdev->dst_pitch  != buffer->video.pitch)
     {
          __u32 offset;
          __u32 pitch;
          
          switch (buffer->format) {
               case DSPF_A8:
                    rdev->dp_gui_master_cntl = GMC_DST_RGB8;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB8;
                    break;
               case DSPF_RGB332:          
                    rdev->dp_gui_master_cntl = GMC_DST_8BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB332;
                    break;
               case DSPF_ARGB4444:
                    rdev->dp_gui_master_cntl = GMC_DST_ARGB4444;
                    rdev->rb3d_cntl = COLOR_FORMAT_ARGB4444;
                    break;
               case DSPF_ARGB1555:          
                    rdev->dp_gui_master_cntl = GMC_DST_15BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_ARGB1555;
                    break;
               case DSPF_RGB16:
                    rdev->dp_gui_master_cntl = GMC_DST_16BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB565;
                    break;
               case DSPF_RGB32:
               case DSPF_ARGB:
                    rdev->dp_gui_master_cntl = GMC_DST_32BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_ARGB8888;
                    break;
               case DSPF_UYVY:
                    rdev->dp_gui_master_cntl = GMC_DST_YVYU;
                    rdev->rb3d_cntl = COLOR_FORMAT_YUV422_YVYU;
                    break;
               case DSPF_YUY2:
                    rdev->dp_gui_master_cntl = GMC_DST_VYUY;
                    rdev->rb3d_cntl = COLOR_FORMAT_YUV422_VYUY;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    break;
          }

          rdev->dp_gui_master_cntl |= GMC_WR_MSK_DIS            |
                                      GMC_SRC_PITCH_OFFSET_CNTL |
                                      GMC_DST_PITCH_OFFSET_CNTL |
                                      GMC_DST_CLIPPING;
 
          offset = rdev->fb_offset + buffer->video.offset;
          pitch  = buffer->video.pitch;
          
          r200_waitfifo( rdrv, rdev, 2 ); 
          r200_out32( mmio, DST_OFFSET, offset );
          r200_out32( mmio, DST_PITCH,  pitch );
          
          r200_waitfifo( rdrv, rdev, 2 );
          r200_out32( mmio, RB3D_COLOROFFSET, offset );
          r200_out32( mmio, RB3D_COLORPITCH,  pitch /
                                              DFB_BYTES_PER_PIXEL(buffer->format) );
          
          if (surface->caps & DSCAPS_DEPTH) {
               SurfaceBuffer *depth = surface->depth_buffer;
               
               offset = rdev->fb_offset + depth->video.offset;
               pitch  = depth->video.pitch >> 1;
               
               r200_waitfifo( rdrv, rdev, 3 );
               r200_out32( mmio, RB3D_DEPTHOFFSET, offset );
               r200_out32( mmio, RB3D_DEPTHPITCH,  pitch );
               r200_out32( mmio, RB3D_ZSTENCILCNTL, DEPTH_FORMAT_16BIT_INT_Z |
                                                    Z_TEST_ALWAYS            |
                                                    Z_WRITE_ENABLE );
          
               rdev->rb3d_cntl |= Z_ENABLE;
          }
          
          if (rdev->dst_format != buffer->format) {
               R200_UNSET( COLOR );
               R200_UNSET( SRC_BLEND );
          }
          
          rdev->dst_format = buffer->format;
          rdev->dst_offset = buffer->video.offset;
          rdev->dst_pitch  = buffer->video.pitch;
     }

     R200_SET( DESTINATION );
}

void r200_set_source( R200DriverData *rdrv,
                      R200DeviceData *rdev,
                      CardState      *state )
{
     CoreSurface   *surface = state->source;
     SurfaceBuffer *buffer  = surface->front_buffer;
     volatile __u8 *mmio    = rdrv->mmio_base;

     if (R200_IS_SET( SOURCE ))
          return;

     D_ASSERT( (buffer->video.offset % 128) == 0 );
     D_ASSERT( (buffer->video.pitch % 32) == 0 );

     if (rdev->src_format != buffer->format       ||
         rdev->src_offset != buffer->video.offset ||
         rdev->src_width  != surface->width       ||
         rdev->src_height != surface->height)
     {
          __u32 offset;
          __u32 pitch;
          __u32 mask = 0;

          switch (buffer->format) {
               case DSPF_A8:
                    rdev->txformat = R200_TXFORMAT_I8;
                    break;
               case DSPF_RGB332:
                    rdev->txformat = R200_TXFORMAT_RGB332;
                    mask = 0x000000ff;
                    break;
               case DSPF_ARGB4444:
                    rdev->txformat = R200_TXFORMAT_ARGB4444;
                    mask = 0x00000fff;
                    break;
               case DSPF_ARGB1555:
                    rdev->txformat = R200_TXFORMAT_ARGB1555;
                    mask =  0x00007fff;
                    break;
               case DSPF_RGB16:
                    rdev->txformat = R200_TXFORMAT_RGB565;
                    mask = 0x0000ffff;
                    break;
               case DSPF_RGB32:
               case DSPF_ARGB:
                    rdev->txformat = R200_TXFORMAT_ARGB8888;
                    mask = 0x00ffffff;
                    break;
               case DSPF_UYVY:
                    rdev->txformat = R200_TXFORMAT_YVYU422;
                    mask = 0xffffffff;
                    break;
               case DSPF_YUY2:
                    rdev->txformat = R200_TXFORMAT_VYUY422;
                    mask = 0xffffffff;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    break;
          }

          rdev->txformat |= R200_TXFORMAT_NON_POWER2; 

          offset = rdev->fb_offset + buffer->video.offset;
          pitch  = buffer->video.pitch;
 
          r200_waitfifo( rdrv, rdev, 3 );
          r200_out32( mmio, CLR_CMP_MASK, mask ); 
          r200_out32( mmio, SRC_OFFSET, offset );
          r200_out32( mmio, SRC_PITCH,  pitch );
           
          r200_waitfifo( rdrv, rdev, 4 );
          r200_out32( mmio, R200_PP_TXFORMAT_X_0, 0 );
          r200_out32( mmio, R200_PP_TXSIZE_0,  (surface->width-1) |
                                              ((surface->height-1) << 16) );
          r200_out32( mmio, R200_PP_TXPITCH_0,  pitch - 32 );
          r200_out32( mmio, R200_PP_TXOFFSET_0, offset );
         
          if (rdev->src_format != buffer->format)
               R200_UNSET( BLITTING_FLAGS );
          
          rdev->src_format = buffer->format;
          rdev->src_offset = buffer->video.offset;
          rdev->src_pitch  = buffer->video.pitch;
          rdev->src_width  = surface->width;
          rdev->src_height = surface->height;
     }

     R200_SET( SOURCE );
}

void r200_set_clip( R200DriverData *rdrv,
                    R200DeviceData *rdev,
                    CardState      *state )
{
     DFBRegion     *clip = &state->clip;
     volatile __u8 *mmio = rdrv->mmio_base;
     
     if (R200_IS_SET( CLIP ))
          return;
     
     r200_waitfifo( rdrv, rdev, 2 );
     r200_out32( mmio, SC_TOP_LEFT, 
                 (clip->y1 << 16) | (clip->x1 & 0xffff) );
     r200_out32( mmio, SC_BOTTOM_RIGHT,
                 ((clip->y2+1) << 16) | ((clip->x2+1) & 0xffff) );
     
     r200_waitfifo( rdrv, rdev, 2 );
     r200_out32( mmio, RE_TOP_LEFT, 
                 (clip->y1 << 16) | (clip->x1 & 0xffff) );
     r200_out32( mmio, RE_BOTTOM_RIGHT,
                 (clip->y2 << 16) | (clip->x2 & 0xffff) );

     R200_SET( CLIP );
}

void r200_set_drawing_color( R200DriverData *rdrv,
                             R200DeviceData *rdev,
                             CardState      *state )
{
     DFBColor color   = state->color;
     __u32    color2d;
     __u32    color3d;
     int      y, u, v;

     if (R200_IS_SET( COLOR ) && R200_IS_SET( DRAWING_FLAGS ))
          return;
 
     switch (rdev->dst_format) {
          case DSPF_A8:
               color2d = color.a;
               break;
          case DSPF_RGB332:
               color2d = PIXEL_RGB332( color.r,
                                       color.g,
                                       color.b );
               break;
          case DSPF_ARGB4444:
               color2d = PIXEL_ARGB4444( color.a,
                                         color.r,
                                         color.g,
                                         color.b );
               break;
          case DSPF_ARGB1555:
               color2d = PIXEL_ARGB1555( color.a,
                                         color.r,
                                         color.g,
                                         color.b );
               break;
          case DSPF_RGB16:
               color2d = PIXEL_RGB16( color.r,
                                      color.g,
                                      color.b );
               break;
          case DSPF_RGB32:
               color2d = PIXEL_RGB32( color.r,
                                      color.g,
                                      color.b );
               break;
          case DSPF_ARGB:
               color2d = PIXEL_ARGB( color.a,
                                     color.r,
                                     color.g,
                                     color.b );
               break;
          case DSPF_UYVY:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color2d = PIXEL_UYVY( y, u, v );
               break;
          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color2d = PIXEL_YUY2( y, u, v );
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               color2d = 0;
               break;
     }

     color3d = PIXEL_ARGB( color.a, color.r,
                           color.g, color.b );

     r200_waitfifo( rdrv, rdev, 2 );
     r200_out32( rdrv->mmio_base, DP_BRUSH_FRGD_CLR, color2d );
     r200_out32( rdrv->mmio_base, R200_PP_TFACTOR_0, color3d );

     R200_SET( COLOR );
}

void r200_set_blitting_color( R200DriverData *rdrv,
                              R200DeviceData *rdev,
                              CardState      *state )
{
     DFBColor color   = state->color;
     __u32    color3d;

     if (R200_IS_SET( COLOR ) && R200_IS_SET( BLITTING_FLAGS ))
          return;

     if (state->blittingflags & DSBLIT_COLORIZE &&
         state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          color3d = PIXEL_ARGB( color.a,
                                color.r * color.a / 0xff,
                                color.g * color.a / 0xff,
                                color.b * color.a / 0xff );
     } else
          color3d = PIXEL_ARGB( color.a, color.r,
                                color.g, color.b );

     r200_waitfifo( rdrv, rdev, 1 );
     r200_out32( rdrv->mmio_base, R200_PP_TFACTOR_0, color3d );

     R200_SET( COLOR );
}

void r200_set_src_colorkey( R200DriverData *rdrv,
                            R200DeviceData *rdev,
                            CardState      *state )
{
     if (R200_IS_SET( SRC_COLORKEY ))
          return;
     
     r200_waitfifo( rdrv, rdev, 1 );
     r200_out32( rdrv->mmio_base, CLR_CMP_CLR_SRC, state->src_colorkey );
     
     R200_SET( SRC_COLORKEY );
}

void
r200_set_blend_function( R200DriverData *rdrv,
                         R200DeviceData *rdev,
                         CardState      *state )
{
     volatile __u8 *mmio   = rdrv->mmio_base;
     __u32          sblend;
     __u32          dblend;
     
     if (R200_IS_SET( SRC_BLEND ) && R200_IS_SET( DST_BLEND ))
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

     r200_waitfifo( rdrv, rdev, 1 ); 
     r200_out32( mmio, RB3D_BLENDCNTL, sblend | dblend );
     
     R200_SET( SRC_BLEND );
     R200_SET( DST_BLEND );
}

/* Default blend equation is ADD (A * B + C) */

void r200_set_drawingflags( R200DriverData *rdrv,
                            R200DeviceData *rdev,
                            CardState      *state )
{
     volatile __u8 *mmio        = rdrv->mmio_base;
     __u32          master_cntl = rdev->dp_gui_master_cntl;
     __u32          rb3d_cntl   = rdev->rb3d_cntl;

     if (R200_IS_SET( DRAWING_FLAGS ))
          return;

     master_cntl |= GMC_SRC_DATATYPE_MONO_FG_LA | 
                    GMC_BRUSH_SOLID_COLOR       |
                    GMC_DP_SRC_SOURCE_MEMORY    |
                    GMC_CLR_CMP_CNTL_DIS;
 
     if (state->drawingflags & DSDRAW_XOR) {
          rb3d_cntl   |= ROP_ENABLE;
          master_cntl |= GMC_ROP3_PATXOR;
     } else
          master_cntl |= GMC_ROP3_PATCOPY;

     if (state->drawingflags & DSDRAW_BLEND)
          rb3d_cntl   |= ALPHA_BLEND_ENABLE;
     
     r200_waitfifo( rdrv, rdev, 2 );
     r200_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl );
     r200_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
     
     r200_waitfifo( rdrv, rdev, 9 );
     r200_out32( mmio, RB3D_CNTL, rb3d_cntl );
     r200_out32( mmio, PP_CNTL, TEX_BLEND_0_ENABLE );
     r200_out32( mmio, R200_PP_TXFILTER_0, R200_MAG_FILTER_NEAREST |
                                           R200_MIN_FILTER_NEAREST );
     r200_out32( mmio, R200_PP_TXCBLEND_0, R200_TXC_ARG_C_TFACTOR_COLOR );
     r200_out32( mmio, R200_PP_TXCBLEND2_0, R200_TXC_OUTPUT_REG_R0 );
     r200_out32( mmio, R200_PP_TXABLEND_0, R200_TXA_ARG_C_TFACTOR_ALPHA );
     r200_out32( mmio, R200_PP_TXABLEND2_0, R200_TXA_OUTPUT_REG_R0 );
     r200_out32( mmio, R200_SE_VTX_FMT_0, R200_VTX_XY );
     r200_out32( mmio, R200_SE_VTX_FMT_1, 2 << R200_VTX_TEX0_COMP_CNT_SHIFT );

     rdev->drawingflags = state->drawingflags;

     R200_SET  ( DRAWING_FLAGS );
     R200_UNSET( BLITTING_FLAGS );
}

void r200_set_blittingflags( R200DriverData *rdrv,
                             R200DeviceData *rdev,
                             CardState      *state )
{
     volatile __u8 *mmio      = rdrv->mmio_base;
     __u32          cmp_cntl  = 0;
     __u32          rb3d_cntl = rdev->rb3d_cntl;
     __u32          txformat  = rdev->txformat;
     __u32          pp_cntl   = TEX_0_ENABLE;
     __u32          filter    = R200_MAG_FILTER_LINEAR |
                                R200_MIN_FILTER_LINEAR;
     __u32          cblend    = R200_TXC_ARG_C_R0_COLOR;
     __u32          ablend    = R200_TXA_ARG_C_R0_ALPHA;
     __u32          vtx_fmt   = R200_VTX_XY;
     
     if (R200_IS_SET( BLITTING_FLAGS ))
          return;

     if (rdev->accel == DFXL_TEXTRIANGLES) {
          filter  |= R200_CLAMP_S_CLAMP_GL | R200_CLAMP_T_CLAMP_GL;
          vtx_fmt |= R200_VTX_Z0 | R200_VTX_W0;
     }
 
     if ((rdev->src_format == DSPF_UYVY || rdev->src_format == DSPF_YUY2) &&
         (rdev->dst_format != DSPF_UYVY && rdev->dst_format != DSPF_YUY2))
          filter |= R200_YUV_TO_RGB;

     if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL &&
         DFB_PIXELFORMAT_HAS_ALPHA( rdev->src_format )) {
          txformat  |= R200_TXFORMAT_ALPHA_IN_MAP;
          rb3d_cntl |= ALPHA_BLEND_ENABLE;
     }
     
     if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
          if (txformat & R200_TXFORMAT_ALPHA_IN_MAP)
               ablend = R200_TXA_ARG_A_TFACTOR_ALPHA | R200_TXA_ARG_B_R0_ALPHA;
          else
               ablend = R200_TXA_ARG_C_TFACTOR_ALPHA;
          
          pp_cntl   |= TEX_BLEND_0_ENABLE;
          rb3d_cntl |= ALPHA_BLEND_ENABLE;
     }
     
     if (state->blittingflags & DSBLIT_COLORIZE) { 
          if (rdev->src_format != DSPF_A8)
               cblend = R200_TXC_ARG_A_TFACTOR_COLOR | R200_TXC_ARG_B_R0_COLOR;
          else
               cblend = R200_TXC_ARG_C_TFACTOR_COLOR;
          
          pp_cntl |= TEX_BLEND_0_ENABLE;
     }
     else if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          if (rdev->src_format != DSPF_A8)
               cblend = R200_TXC_ARG_A_TFACTOR_ALPHA | R200_TXC_ARG_B_R0_COLOR;
          else
               cblend = R200_TXC_ARG_C_R0_ALPHA;
            
          pp_cntl |= TEX_BLEND_0_ENABLE;
     }
 
     if (state->blittingflags & DSBLIT_SRC_COLORKEY)
          cmp_cntl = SRC_CMP_EQ_COLOR | CLR_CMP_SRC_SOURCE; 

     r200_waitfifo( rdrv, rdev, 2 );
     r200_out32( mmio, CLR_CMP_CNTL, cmp_cntl );
     r200_out32( mmio, DP_GUI_MASTER_CNTL, rdev->dp_gui_master_cntl |
                                           GMC_BRUSH_NONE           |
                                           GMC_SRC_DATATYPE_COLOR   |
                                           GMC_ROP3_SRCCOPY         |
                                           GMC_DP_SRC_SOURCE_MEMORY );
     
     r200_waitfifo( rdrv, rdev, 10 );
     r200_out32( mmio, RB3D_CNTL, rb3d_cntl | DITHER_ENABLE );
     r200_out32( mmio, PP_CNTL, pp_cntl ); 
     r200_out32( mmio, R200_PP_TXFORMAT_0, txformat );
     r200_out32( mmio, R200_PP_TXFILTER_0, filter );
     r200_out32( mmio, R200_PP_TXCBLEND_0, cblend );
     r200_out32( mmio, R200_PP_TXCBLEND2_0, R200_TXC_OUTPUT_REG_R0 |
                                            R200_TXC_CLAMP_0_1 );
     r200_out32( mmio, R200_PP_TXABLEND_0, ablend );
     r200_out32( mmio, R200_PP_TXABLEND2_0, R200_TXA_OUTPUT_REG_R0 | 
                                            R200_TXA_CLAMP_0_1 );
     r200_out32( mmio, R200_SE_VTX_FMT_0, vtx_fmt );
     r200_out32( mmio, R200_SE_VTX_FMT_1, 2 << R200_VTX_TEX0_COMP_CNT_SHIFT );
     
     rdev->blittingflags = state->blittingflags;
     
     R200_SET  ( BLITTING_FLAGS );
     R200_UNSET( DRAWING_FLAGS );
}

