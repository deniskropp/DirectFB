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

#include "r300_program.h"


#define R300_HAS_3DREGS()  (rdrv->mmio_size > 0x4000)


static const u32 r300SrcBlend[] = {
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

static const u32 r300DstBlend[] = {
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


void r300_restore( RadeonDriverData *rdrv, RadeonDeviceData *rdev )
{
     const u32    rs_magic[8] = { 0x00, 0x44, 0x84, 0xc4,
                                  0x04, 0x04, 0x04, 0x04 };
     volatile u8 *mmio = rdrv->mmio_base;
     int          i;
     
     /* enable caches */
     radeon_waitfifo( rdrv, rdev, 1 );
     radeon_out32( mmio, RB2D_DSTCACHE_MODE, RB2D_DC_2D_CACHE_AUTOFLUSH |
                                             R300_RB2D_DC_ENABLE );
    
     if (!R300_HAS_3DREGS())
          return;
    
     /* restore 3d engine state */
     radeon_waitfifo( rdrv, rdev, 50 );
     radeon_out32( mmio, 0x2080, 0x0030045a );
     radeon_out32( mmio, R300_SE_VTE_CNTL, R300_VTX_W0_FMT );
     radeon_out32( mmio, R300_SE_VTE_CNTL+4, 0x00000008 );
     radeon_out32( mmio, 0x2134, 0x00FFFFFF );
     radeon_out32( mmio, 0x2138, 0x00000000 );
#ifdef WORDS_BIGENDIAN
     radeon_out32( mmio, 0x2140, 0x00000002 );
#else
     radeon_out32( mmio, 0x2140, 0x00000000 );
#endif
     radeon_out32( mmio, 0x21dc, 0xaaaaaaaa );
     radeon_out32( mmio, 0x2220, f2d(1.0) );
     radeon_out32( mmio, 0x2224, f2d(1.0) );
     radeon_out32( mmio, 0x2228, f2d(1.0) );
     radeon_out32( mmio, 0x222c, f2d(1.0) );
     if (rdev->chipset >= CHIP_RV350)
          radeon_out32( mmio, R300_VAP_UNKNOWN_2288, R300_2288_RV350 );
     else
          radeon_out32( mmio, R300_VAP_UNKNOWN_2288, R300_2288_R300 );
     radeon_out32( mmio, R300_GB_ENABLE, R300_GB_POINT_STUFF_ENABLE |
                                         R300_GB_LINE_STUFF_ENABLE  |
                                         R300_GB_TRIANGLE_STUFF_ENABLE );
     radeon_out32( mmio, R300_GB_MSPOS0, 0x66666666 );
     radeon_out32( mmio, R300_GB_MSPOS1, 0x06666666 );
     if (rdev->chipset == CHIP_R300 || rdev->chipset == CHIP_R350 || rdev->chipset == CHIP_RV410) {
          radeon_out32( mmio, R300_GB_TILE_CONFIG, R300_GB_TILE_ENABLE          |
                                                   R300_GB_TILE_PIPE_COUNT_R300 |
                                                   R300_GB_TILE_SIZE_16 );
     }
     else if (rdev->chipset == CHIP_R420) {
          radeon_out32( mmio, R300_GB_TILE_CONFIG, R300_GB_TILE_ENABLE          |
                                                   R300_GB_TILE_PIPE_COUNT_R420 |
                                                   R300_GB_TILE_SIZE_16 );
     }
     else {
          radeon_out32( mmio, R300_GB_TILE_CONFIG, R300_GB_TILE_ENABLE           |
                                                   R300_GB_TILE_PIPE_COUNT_RV300 |
                                                   R300_GB_TILE_SIZE_16 );
     }
     radeon_out32( mmio, R300_GB_SELECT, 0 );
     radeon_out32( mmio, R300_GB_AA_CONFIG, 0 );
     radeon_out32( mmio, 0x4200, f2d(0.0) );
     radeon_out32( mmio, 0x4204, f2d(0.0) );
     radeon_out32( mmio, 0x4208, f2d(1.0) );
     radeon_out32( mmio, 0x420c, f2d(1.0) );
     radeon_out32( mmio, 0x4214, 0x00050005 );
     radeon_out32( mmio, R300_RE_POINTSIZE, (6 << R300_POINTSIZE_X_SHIFT) |
                                            (6 << R300_POINTSIZE_Y_SHIFT) );
     radeon_out32( mmio, 0x4230, 0x18000006 );
     radeon_out32( mmio, R300_RE_LINE_CNT, (6 << R300_LINESIZE_SHIFT) |
                                            R300_LINE_CNT_VE );
     radeon_out32( mmio, R300_RE_UNK4238, f2d(1.0/192.0) );
     radeon_out32( mmio, 0x4260, 0x00000000 );
     radeon_out32( mmio, 0x4264, f2d(0.0) );
     radeon_out32( mmio, 0x4268, f2d(1.0) );
     radeon_out32( mmio, 0x4274, 0x00000002 );
     radeon_out32( mmio, 0x427c, 0x00000000 );
     radeon_out32( mmio, 0x4280, 0x00000000 );
     radeon_out32( mmio, R300_RE_POLYGON_MODE, 0 );
     radeon_out32( mmio, 0x428c, 0x00000001 );
     radeon_out32( mmio, 0x4290, 0x00000000 );
     radeon_out32( mmio, 0x4294, 0x00000000 );
     radeon_out32( mmio, 0x4298, 0x00000000 );
     radeon_out32( mmio, 0x42a0, 0x00000000 );
     radeon_out32( mmio, R300_RE_ZBIAS_T_FACTOR, 0 );
     radeon_out32( mmio, R300_RE_ZBIAS_T_CONSTANT, 0 );
     radeon_out32( mmio, R300_RE_ZBIAS_W_FACTOR, 0 );
     radeon_out32( mmio, R300_RE_ZBIAS_W_CONSTANT, 0 );
     radeon_out32( mmio, R300_RE_OCCLUSION_CNTL, 0 );
     radeon_out32( mmio, R300_RE_CULL_CNTL, 0 );
     radeon_out32( mmio, 0x42c0, 0x4b7fffff );
     radeon_out32( mmio, 0x42c4, 0x00000000 ); 

     radeon_waitfifo( rdrv, rdev, 16 );
     for (i = 0; i < 8; i++) {
          radeon_out32( mmio, R300_RS_INTERP_0+i*4, R300_RS_INTERP_USED | rs_magic[i] );
          //radeon_out32( mmio, R300_RS_ROUTE_0+i*4, 0 );
     }
     
     radeon_waitfifo( rdrv, rdev, 43 );
     radeon_out32( mmio, 0x43a4, 0x0000001c );
     radeon_out32( mmio, 0x43a8, 0x2da49525 );
     radeon_out32( mmio, 0x43e8, 0x00ffffff );
     radeon_out32( mmio, 0x46a4, 0x00001b01 );
     radeon_out32( mmio, 0x46a8, 0x00001b0f );
     radeon_out32( mmio, 0x46ac, 0x00001b0f );
     radeon_out32( mmio, 0x46b0, 0x00001b0f );
     radeon_out32( mmio, 0x46b4, 0x00000001 );
     radeon_out32( mmio, 0x4bc0, 0x00000000 );
     radeon_out32( mmio, 0x4bc8, 0x00000000 );
     radeon_out32( mmio, 0x4bcc, 0x00000000 );
     radeon_out32( mmio, 0x4bd0, 0x00000000 );
     radeon_out32( mmio, R300_PP_ALPHA_TEST, R300_ALPHA_TEST_PASS );
     radeon_out32( mmio, 0x4bd8, 0x00000000 );
     radeon_out32( mmio, 0x4e00, 0x00000000 );
     radeon_out32( mmio, R300_RB3D_COLORMASK, R300_COLORMASK0_B | 
                                              R300_COLORMASK0_G |
                                              R300_COLORMASK0_R |
                                              R300_COLORMASK0_A );
     radeon_out32( mmio, R300_RB3D_BLENDCOLOR, 0xffffffff );
     radeon_out32( mmio, 0x4e14, 0x00000000 );
     radeon_out32( mmio, 0x4e18, 0x00000000 );
     radeon_out32( mmio, 0x4e50, 0x00000000 );
     radeon_out32( mmio, 0x4e54, 0x00000000 );
     radeon_out32( mmio, 0x4e58, 0x00000000 );
     radeon_out32( mmio, 0x4e5c, 0x00000000 );
     radeon_out32( mmio, 0x4e60, 0x00000000 );
     radeon_out32( mmio, 0x4e64, 0x00000000 );
     radeon_out32( mmio, 0x4e68, 0x00000000 );
     radeon_out32( mmio, 0x4e6c, 0x00000000 );
     radeon_out32( mmio, 0x4e70, 0x00000000 );
     radeon_out32( mmio, 0x4e88, 0x00000000 );
     radeon_out32( mmio, 0x4ea0, 0x00000000 );
     radeon_out32( mmio, 0x4ea4, 0xffffffff );  
     radeon_out32( mmio, R300_RB3D_ZSTENCIL_CNTL_0, R300_RB3D_Z_DISABLED_1 );
     radeon_out32( mmio, R300_RB3D_ZSTENCIL_CNTL_1, R300_ZS_ALWAYS );
     radeon_out32( mmio, R300_RB3D_ZSTENCIL_CNTL_2, 0xffffff00 );
     radeon_out32( mmio, R300_RB3D_ZSTENCIL_FORMAT, R300_DEPTH_FORMAT_16BIT_INT_Z );
     radeon_out32( mmio, 0x4f14, 0x00000000 );
     radeon_out32( mmio, 0x4f18, 0x00000003 );
     radeon_out32( mmio, 0x4f1c, 0x00000000 );
     radeon_out32( mmio, 0x4f28, 0x00000000 );
     radeon_out32( mmio, 0x4f30, 0x00000000 );
     radeon_out32( mmio, 0x4f34, 0x00000000 );
     radeon_out32( mmio, 0x4f44, 0x00000000 );
     radeon_out32( mmio, 0x4f54, 0x00000000 );
                             
     /* upload vertex program */ 
     radeon_waitfifo( rdrv, rdev, 50 );
     radeon_out32( mmio, R300_VAP_PVS_CNTL_1,
                        (0 << R300_PVS_CNTL_1_PROGRAM_START_SHIFT) |
                        (4 << R300_PVS_CNTL_1_POS_END_SHIFT)       |
                        (4 << R300_PVS_CNTL_1_PROGRAM_END_SHIFT) );
     radeon_out32( mmio, R300_VAP_PVS_CNTL_2, 
                        (0 << R300_PVS_CNTL_2_PARAM_OFFSET_SHIFT) |
                        (4 << R300_PVS_CNTL_2_PARAM_COUNT_SHIFT) );
     radeon_out32( mmio, R300_VAP_PVS_CNTL_3, 
                        (4 << R300_PVS_CNTL_3_PROGRAM_UNKNOWN_SHIFT) |
                        (4 << R300_PVS_CNTL_3_PROGRAM_UNKNOWN2_SHIFT) );
     radeon_out32( mmio, R300_VAP_PVS_WAITIDLE, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_ADDRESS, R300_PVS_UPLOAD_POINTSIZE );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, f2d(1.0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_WAITIDLE, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_ADDRESS, R300_PVS_UPLOAD_PROGRAM );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, EASY_VSF_OP(MAD, 0, ALL, TMP) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_ATTR_X(0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_PARAM(0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, EASY_VSF_SOURCE(0, ZERO, ZERO, ZERO, ZERO, PARAM, NONE) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, EASY_VSF_OP(MAD, 0, ALL, TMP) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_ATTR_Y(0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_PARAM(1) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_TMP(0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, EASY_VSF_OP(MAD, 0, ALL, TMP) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_ATTR_Z(0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_PARAM(2) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_TMP(0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, EASY_VSF_OP(MAD, 0, ALL, RESULT) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_ATTR_W(0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_PARAM(3) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_TMP(0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, EASY_VSF_OP(ADD, 1, ALL, RESULT) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, VSF_REG(1) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, EASY_VSF_SOURCE(1, ZERO, ZERO, ZERO, ZERO, ATTR, NONE) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, EASY_VSF_SOURCE(1, ZERO, ZERO, ZERO, ZERO, ATTR, NONE) );
     radeon_out32( mmio, R300_VAP_PVS_WAITIDLE, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_ADDRESS, R300_PVS_UPLOAD_PARAMETERS );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, f2d(1.0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, f2d(1.0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, f2d(1.0) );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, 0 );
     radeon_out32( mmio, R300_VAP_PVS_UPLOAD_DATA, f2d(1.0) );
     
#if 0  
     /* set YUV422 color buffer */
     radeon_waitfifo( rdrv, rdev, 4 );
     radeon_out32( mmio, R300_TX_FILTER_1, R300_TX_MAG_FILTER_NEAREST |
                                           R300_TX_MIN_FILTER_NEAREST );
     radeon_out32( mmio, R300_TX_FILTER1_0, 0 );
     radeon_out32( mmio, R300_TX_SIZE_1, (1 << R300_TX_WIDTHMASK_SHIFT) |
                                         (1 << R300_TX_HEIGHTMASK_SHIFT) );
     radeon_out32( mmio, R300_TX_FORMAT_1, R300_TXFORMAT_VYUY422 );
#endif
} 

void r300_set_destination( RadeonDriverData *rdrv,
                           RadeonDeviceData *rdev,
                           CardState        *state )
{
     CoreSurface   *surface = state->destination;
     SurfaceBuffer *buffer  = surface->back_buffer;
     volatile u8   *mmio    = rdrv->mmio_base;
     u32            offset;
     u32            pitch;
     u32            format  = 0;
     bool           dst_422 = false;
    
     if (RADEON_IS_SET( DESTINATION ))
          return;
     
     D_ASSERT( (buffer->video.offset % 32) == 0 );
     D_ASSERT( (buffer->video.pitch % 64) == 0 );

     offset = radeon_buffer_offset( rdev, buffer );
     pitch  = buffer->video.pitch;

     if (rdev->dst_offset != offset        ||
         rdev->dst_pitch  != pitch         ||
         rdev->dst_format != buffer->format)
     {         
          switch (buffer->format) {
               case DSPF_LUT8:
               case DSPF_ALUT44:
               case DSPF_A8: 
               case DSPF_RGB332:
                    format = R300_COLOR_FORMAT_RGB8;
                    rdev->gui_master_cntl = GMC_DST_8BPP;
                    break;
               case DSPF_ARGB2554:
                    rdev->gui_master_cntl = GMC_DST_16BPP;
                    break;
               case DSPF_ARGB4444:
                    rdev->gui_master_cntl = GMC_DST_16BPP;
                    break;
               case DSPF_ARGB1555:          
                    rdev->gui_master_cntl = GMC_DST_15BPP;
                    break;
               case DSPF_RGB16: 
                    format = R300_COLOR_FORMAT_RGB565;
                    rdev->gui_master_cntl = GMC_DST_16BPP;
                    break;
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
               case DSPF_AYUV: 
                    format = R300_COLOR_FORMAT_ARGB8888;
                    rdev->gui_master_cntl = GMC_DST_32BPP;
                    break;
               case DSPF_UYVY:
                    rdev->gui_master_cntl = GMC_DST_YVYU;
                    dst_422 = true;
                    break;
               case DSPF_YUY2:
                    rdev->gui_master_cntl = GMC_DST_VYUY;
                    dst_422 = true;
                    break;
               case DSPF_I420:
                    format = R300_COLOR_FORMAT_RGB8;
                    rdev->gui_master_cntl = GMC_DST_8BPP;
                    rdev->dst_offset_cb = offset + pitch * surface->height;
                    rdev->dst_offset_cr = rdev->dst_offset_cb + 
                                          pitch/2 * surface->height/2;
                    break;
               case DSPF_YV12:
                    format = R300_COLOR_FORMAT_RGB8;
                    rdev->gui_master_cntl = GMC_DST_8BPP;
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
         
          if (R300_HAS_3DREGS() && format) {
               radeon_waitfifo( rdrv, rdev, 2 );
               radeon_out32( mmio, R300_RB3D_COLOROFFSET0, offset );
               radeon_out32( mmio, R300_RB3D_COLORPITCH0, 
                           ((pitch / DFB_BYTES_PER_PIXEL(buffer->format))
                                   & R300_COLORPITCH_MASK) | format );
          }
          
          if (rdev->dst_format != buffer->format) {
               if (dst_422 && !rdev->dst_422) {
                    RADEON_UNSET( CLIP );
                    RADEON_UNSET( SOURCE );
                    rdev->src_format = DSPF_UNKNOWN;
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
     volatile u8   *mmio     = rdrv->mmio_base;
     u32            txformat = 0;
     u32            txfilter = (R300_TX_CLAMP_TO_EDGE << R300_TX_WRAP_S_SHIFT) |
                               (R300_TX_CLAMP_TO_EDGE << R300_TX_WRAP_T_SHIFT) |
                               (R300_TX_CLAMP_TO_EDGE << R300_TX_WRAP_Q_SHIFT) |
                                R300_TX_MAG_FILTER_LINEAR | R300_TX_MIN_FILTER_LINEAR;

     if (RADEON_IS_SET( SOURCE )) {
          if ((state->blittingflags & DSBLIT_DEINTERLACE) ==
              (rdev->blittingflags  & DSBLIT_DEINTERLACE))
               return;
     }

     D_ASSERT( (buffer->video.offset % 32) == 0 );
     D_ASSERT( (buffer->video.pitch % 64) == 0 );
     
     rdev->src_offset = radeon_buffer_offset( rdev, buffer );
     rdev->src_pitch  = buffer->video.pitch;
     rdev->src_width  = surface->width;
     rdev->src_height = surface->height;

     switch (buffer->format) {
          case DSPF_LUT8:
               txformat  = R300_TXFORMAT_I8;
               txfilter &= ~(R300_TX_MAG_FILTER_LINEAR |
                             R300_TX_MIN_FILTER_LINEAR);
               txfilter |= R300_TX_MAG_FILTER_NEAREST |
                           R300_TX_MIN_FILTER_NEAREST;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_ALUT44:
               txformat  = R300_TXFORMAT_I8;
               txfilter &= ~(R300_TX_MAG_FILTER_LINEAR |
                             R300_TX_MIN_FILTER_LINEAR);
               txfilter |= R300_TX_MAG_FILTER_NEAREST |
                           R300_TX_MIN_FILTER_NEAREST;
               rdev->src_mask = 0x0000000f;
               break;
          case DSPF_A8:
               txformat = R300_TXFORMAT_A8;
               rdev->src_mask = 0;
               break;
          case DSPF_RGB332:
               txformat = R300_TXFORMAT_RGB332;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_ARGB2554:
               txformat = R300_TXFORMAT_RGB565;
               txfilter &= ~(R300_TX_MAG_FILTER_LINEAR |
                             R300_TX_MIN_FILTER_LINEAR);
               txfilter |= R300_TX_MAG_FILTER_NEAREST |
                           R300_TX_MIN_FILTER_NEAREST;
               rdev->src_mask = 0x00003fff;
               break;
          case DSPF_ARGB4444:
               txformat = R300_TXFORMAT_ARGB4444;
               rdev->src_mask = 0x00000fff;
               break;
          case DSPF_ARGB1555:
               txformat = R300_TXFORMAT_ARGB1555;
               rdev->src_mask = 0x00007fff;
               break;
          case DSPF_RGB16:
               txformat = R300_TXFORMAT_RGB565;
               rdev->src_mask = 0x0000ffff;
               break;
          case DSPF_RGB32:
               txformat = R300_TXFORMAT_XRGB8888;
               rdev->src_mask = 0x00ffffff;
               break;
          case DSPF_ARGB:
          case DSPF_AiRGB:
          case DSPF_AYUV:
               txformat = R300_TXFORMAT_ARGB8888;
               rdev->src_mask = 0x00ffffff;
               break;
          case DSPF_UYVY:
               txformat = R300_TXFORMAT_YVYU422;
               rdev->src_mask = 0xffffffff;
               break;
          case DSPF_YUY2:
               txformat = R300_TXFORMAT_VYUY422;
               rdev->src_mask = 0xffffffff;
               break;
          case DSPF_I420:
               txformat = R300_TXFORMAT_I8;
               rdev->src_offset_cb = rdev->src_offset +
                                     rdev->src_pitch * rdev->src_height;
               rdev->src_offset_cr = rdev->src_offset_cb +
                                     rdev->src_pitch/2 * rdev->src_height/2;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_YV12:
               txformat = R300_TXFORMAT_I8;
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
     
     if (R300_HAS_3DREGS()) {
          radeon_waitfifo( rdrv, rdev, 6 );
          radeon_out32( mmio, R300_TX_CNTL, 0 );
          radeon_out32( mmio, R300_TX_FILTER_0, txfilter );
          radeon_out32( mmio, R300_TX_FORMAT_0, txformat );
          radeon_out32( mmio, R300_TX_SIZE_0, ((rdev->src_width -1) << R300_TX_WIDTH_SHIFT)  |
                                              ((rdev->src_height-1) << R300_TX_HEIGHT_SHIFT) |
                                              R300_TX_SIZE_TXPITCH_EN );
          radeon_out32( mmio, R300_TX_PITCH_0, rdev->src_pitch / 
                                               DFB_BYTES_PER_PIXEL(buffer->format) - 8 );
          radeon_out32( mmio, R300_TX_OFFSET_0, rdev->src_offset ); 
     }

     if (rdev->src_format != buffer->format)
          RADEON_UNSET( BLITTING_FLAGS );
     rdev->src_format = buffer->format;

     RADEON_SET( SOURCE );
}

void r300_set_clip3d( RadeonDriverData *rdrv,
                      RadeonDeviceData *rdev,
                      const DFBRegion  *clip )
{
     volatile u8 *mmio = rdrv->mmio_base;
     int          x1, y1, x2, y2;
     
     x1 = clip->x1 + R300_CLIPRECT_OFFSET;
     y1 = clip->y1 + R300_CLIPRECT_OFFSET;
     x2 = clip->x2 + R300_CLIPRECT_OFFSET;
     y2 = clip->y2 + R300_CLIPRECT_OFFSET;
          
     radeon_waitfifo( rdrv, rdev, 5 );
     radeon_out32( mmio, R300_RE_CLIPRECT_TL_0, 
                  ((y1 << R300_CLIPRECT_Y_SHIFT) & R300_CLIPRECT_Y_MASK) |
                  ((x1 << R300_CLIPRECT_X_SHIFT) & R300_CLIPRECT_X_MASK) );
     radeon_out32( mmio, R300_RE_CLIPRECT_BR_0, 
                  ((y2 << R300_CLIPRECT_Y_SHIFT) & R300_CLIPRECT_Y_MASK) |
                  ((x2 << R300_CLIPRECT_X_SHIFT) & R300_CLIPRECT_X_MASK) );
     radeon_out32( mmio, R300_RE_CLIPRECT_CNTL, 0x0000aaaa );
     radeon_out32( mmio, R300_RE_SCISSORS_TL,
                  ((y1 << R300_SCISSORS_Y_SHIFT) & R300_SCISSORS_Y_MASK) |
                  ((x1 << R300_SCISSORS_X_SHIFT) & R300_SCISSORS_X_MASK) );
     radeon_out32( mmio, R300_RE_SCISSORS_BR,
                  ((y2 << R300_SCISSORS_Y_SHIFT) & R300_SCISSORS_Y_MASK) |
                  ((x2 << R300_SCISSORS_X_SHIFT) & R300_SCISSORS_X_MASK) );
}     

void r300_set_clip( RadeonDriverData *rdrv,
                    RadeonDeviceData *rdev,
                    CardState        *state )
{
     DFBRegion   *clip = &state->clip;
     volatile u8 *mmio = rdrv->mmio_base;
     
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
     if (R300_HAS_3DREGS())
          r300_set_clip3d( rdrv, rdev, clip );

     rdev->clip = state->clip;
     
     RADEON_SET( CLIP );
}

#define R300_SET_YUV422_COLOR( rdrv, rdev, y, u, v ) \
     if (R300_HAS_3DREGS()) { \
          radeon_out32( (rdrv)->fb_base, \
                        (rdev)->yuv422_buffer, PIXEL_YUY2( y, u, v ) ); \
          radeon_in8( (rdrv)->fb_base, (rdev)->yuv422_buffer ); \
          radeon_waitfifo( rdrv, rdev, 1 ); \
          radeon_out32( (rdrv)->mmio_base, R300_TX_OFFSET_1, \
                        ((rdev)->fb_offset + (rdev)->yuv422_buffer) ); \
     }

void r300_set_drawing_color( RadeonDriverData *rdrv,
                             RadeonDeviceData *rdev,
                             CardState        *state )
{
     DFBColor color   = state->color;
     int      index   = state->color_index;
     u32      color2d;
     int      y, u, v;

     if (RADEON_IS_SET( COLOR ) && RADEON_IS_SET( DRAWING_FLAGS ))
          return;

     if (state->drawingflags & DSDRAW_SRC_PREMULTIPLY) {
          color.r = ((long) color.r * color.a / 255L);
          color.g = ((long) color.g * color.a / 255L);
          color.b = ((long) color.b * color.a / 255L);
     }
 
     switch (rdev->dst_format) {
          case DSPF_ALUT44:
               index |= (color.a & 0xf0);
          case DSPF_LUT8:
               color2d = index;
               break;
          case DSPF_A8:
               color2d = color.a;
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
               color2d = PIXEL_AYUV( color.a, y, u, v );
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
               color2d = rdev->y_cop;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               color2d = 0;
               break;
     }
     
     rdev->color[0] = (float)color.r/255.0;
     rdev->color[1] = (float)color.g/255.0;
     rdev->color[2] = (float)color.b/255.0;
     rdev->color[3] = (float)color.a/255.0;
     
     radeon_waitfifo( rdrv, rdev, 1 );
     radeon_out32( rdrv->mmio_base, DP_BRUSH_FRGD_CLR, color2d ); 

     RADEON_SET( COLOR );
}

void r300_set_blitting_color( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state )
{
     DFBColor color = state->color;
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
               color.r = color.g = color.b = 0xff;
               break;
          case DSPF_I420:
          case DSPF_YV12: 
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               rdev->y_cop  = PIXEL_ARGB( color.a, y, y, y );
               rdev->cb_cop = PIXEL_ARGB( color.a, u, u, u );
               rdev->cr_cop = PIXEL_ARGB( color.a, v, v, v );
               break;
          case DSPF_AYUV:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color.r = y;
               color.g = u;
               color.b = v;
               break;
          case DSPF_UYVY:
          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               //R300_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
          default:
               break;
     }
     
     rdev->color[0] = (float)color.r/255.0;
     rdev->color[1] = (float)color.g/255.0;
     rdev->color[2] = (float)color.b/255.0;
     rdev->color[3] = (float)color.a/255.0;
     
     RADEON_SET( COLOR );
}

void r300_set_src_colorkey( RadeonDriverData *rdrv,
                            RadeonDeviceData *rdev,
                            CardState        *state )
{
     volatile u8 *mmio = rdrv->mmio_base;
     u32          key  = state->src_colorkey;
     
     if (RADEON_IS_SET( SRC_COLORKEY ))
          return;
     
     switch (rdev->src_format) {
          case DSPF_ARGB4444:
               key |= 0xf000;
               break;
          case DSPF_ARGB2554:
               key |= 0xc000;
               break;
          case DSPF_ARGB1555:
               key |= 0x8000;
               break;
          case DSPF_ARGB:
          case DSPF_AYUV:
               key |= 0xff000000;
               break;
          default:
               break;
     }
    
     radeon_waitfifo( rdrv, rdev, 3 );
     radeon_out32( mmio, CLR_CMP_CLR_SRC, key );
     /* XXX: R300 seems to ignore CLR_CMP_MASK. */
     radeon_out32( mmio, CLR_CMP_MASK, rdev->src_mask );
     if (R300_HAS_3DREGS())
          radeon_out32( mmio, R300_TX_CHROMA_KEY_0, state->src_colorkey );
     
     RADEON_SET( SRC_COLORKEY );
}

void
r300_set_blend_function( RadeonDriverData *rdrv,
                         RadeonDeviceData *rdev,
                         CardState        *state )
{
     u32 sblend, dblend;
     
     if (RADEON_IS_SET( SRC_BLEND ) && RADEON_IS_SET( DST_BLEND ))
          return;

     sblend = r300SrcBlend[state->src_blend-1];
     dblend = r300DstBlend[state->dst_blend-1];

     if (!DFB_PIXELFORMAT_HAS_ALPHA( rdev->dst_format )) {
          switch (state->src_blend) {
               case DSBF_DESTALPHA:
                    sblend = R300_SRC_BLEND_GL_ONE;
                    break;
               case DSBF_INVDESTALPHA:
                    sblend = R300_SRC_BLEND_GL_ZERO;
                    break;
               default:
                    break;
          }
     }
     
     rdev->rb3d_blend = sblend | dblend;
     
     RADEON_UNSET( DRAWING_FLAGS );
     RADEON_UNSET( BLITTING_FLAGS );
     RADEON_SET( SRC_BLEND );
     RADEON_SET( DST_BLEND );
}

void r300_set_drawingflags( RadeonDriverData *rdrv,
                            RadeonDeviceData *rdev,
                            CardState        *state )
{
     volatile u8   *mmio        = rdrv->mmio_base;
     u32            master_cntl = rdev->gui_master_cntl       |
                                  GMC_SRC_DATATYPE_MONO_FG_LA |
                                  GMC_BRUSH_SOLID_COLOR       |
                                  GMC_CLR_CMP_CNTL_DIS;
     u32            rb3d_blend;
     
     if (RADEON_IS_SET( DRAWING_FLAGS ))
          return;
     
     if (state->drawingflags & DSDRAW_BLEND) {
          rb3d_blend = R300_BLEND_ENABLE      | R300_BLEND_UNKNOWN | 
                       R300_BLEND_NO_SEPARATE | rdev->rb3d_blend;
     }
     else {
          rb3d_blend = R300_SRC_BLEND_GL_ONE | R300_DST_BLEND_GL_ZERO;
     }

     if (state->drawingflags & DSDRAW_XOR)
          master_cntl |= GMC_ROP3_PATXOR;
     else
          master_cntl |= GMC_ROP3_PATCOPY;
     
     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl );
     radeon_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
     
     if (R300_HAS_3DREGS()) {
          radeon_waitfifo( rdrv, rdev, 27 ); 
          radeon_out32( mmio, R300_TX_ENABLE, 0 );
          radeon_out32( mmio, R300_RE_SHADE_MODEL, R300_RE_SHADE_MODEL_FLAT );
          /* fragment program */
          radeon_out32( mmio, R300_PFS_CNTL_0, 0 );
          radeon_out32( mmio, R300_PFS_CNTL_1, 0 );
          radeon_out32( mmio, R300_PFS_CNTL_2, 0 );
          radeon_out32( mmio, R300_PFS_NODE_0, 0 );
          radeon_out32( mmio, R300_PFS_NODE_1, 0 );
          radeon_out32( mmio, R300_PFS_NODE_2, 0 );
          radeon_out32( mmio, R300_PFS_NODE_3, R300_PFS_NODE_OUTPUT_COLOR );
          radeon_out32( mmio, R300_PFS_INSTR0_0,
                              FP_INSTRC(MAD, FP_ARGC(SRC0C_XYZ), FP_ARGC(ONE), FP_ARGC(ZERO)) );
          radeon_out32( mmio, R300_PFS_INSTR1_0,
                              FP_SELC(0,NO,XYZ,FP_TMP(0),FP_TMP(2),FP_TMP(2)) );
          radeon_out32( mmio, R300_PFS_INSTR2_0,
                              FP_INSTRA(MAD, FP_ARGA(SRC0A), FP_ARGA(ONE), FP_ARGA(ZERO)) );
          radeon_out32( mmio, R300_PFS_INSTR3_0,
                              FP_SELA(0,NO,W,FP_TMP(0),FP_TMP(2),FP_TMP(2)) );
          /* blend functions */
          radeon_out32( mmio, R300_RB3D_CBLEND, rb3d_blend );
          radeon_out32( mmio, R300_RB3D_ABLEND, rb3d_blend & 0xfffffff0 );
          /* routing */
          radeon_out32( mmio, R300_RS_CNTL_0, (0 << R300_RS_CNTL_TC_CNT_SHIFT) | 
                                              (1 << R300_RS_CNTL_CI_CNT_SHIFT) |
                                               R300_RS_CNTL_0_UNKNOWN_18 );
          radeon_out32( mmio, R300_RS_CNTL_1, 0x000000c0 );
          radeon_out32( mmio, R300_RS_ROUTE_0, R300_RS_ROUTE_0_COLOR );
          /* input */
          radeon_out32( mmio, R300_VAP_INPUT_ROUTE_0_0, 0x21030003 );
          radeon_out32( mmio, R300_VAP_INPUT_ROUTE_1_0, 0xf688f688 );
          radeon_out32( mmio, R300_VAP_INPUT_CNTL_0, R300_INPUT_CNTL_0_COLOR );
          radeon_out32( mmio, R300_VAP_INPUT_CNTL_1, R300_INPUT_CNTL_POS |
                                                     R300_INPUT_CNTL_COLOR );
          /* output */
          radeon_out32( mmio, R300_VAP_OUTPUT_VTX_FMT_0,
                              R300_VAP_OUTPUT_VTX_FMT_0__POS_PRESENT |
                              R300_VAP_OUTPUT_VTX_FMT_0__COLOR_PRESENT );
          radeon_out32( mmio, R300_VAP_OUTPUT_VTX_FMT_1, 0 );
          radeon_out32( mmio, R300_GB_VAP_RASTER_VTX_FMT_0, 
                              R300_GB_VAP_RASTER_VTX_FMT_0__POS_PRESENT |
                              R300_GB_VAP_RASTER_VTX_FMT_0__COLOR_0_PRESENT );
          radeon_out32( mmio, R300_GB_VAP_RASTER_VTX_FMT_1, 0 );
          radeon_out32( mmio, R300_VAP_UNKNOWN_221C, R300_221C_CLEAR );
     }     

     rdev->drawingflags = state->drawingflags;

     RADEON_SET  ( DRAWING_FLAGS );
     RADEON_UNSET( BLITTING_FLAGS );
}

void r300_set_blittingflags( RadeonDriverData *rdrv,
                             RadeonDeviceData *rdev,
                             CardState      *state )
{
     volatile u8   *mmio        = rdrv->mmio_base;
     u32            master_cntl = rdev->gui_master_cntl |
                                  GMC_BRUSH_NONE        |
                                  GMC_SRC_DATATYPE_COLOR; 
     u32            txfilter1   = R300_TX_TRI_PERF_0_8;
     u32            cmp_cntl    = 0;
     u32            rb3d_blend;
     
     if (RADEON_IS_SET( BLITTING_FLAGS ))
          return;
      
     if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                 DSBLIT_BLEND_ALPHACHANNEL)) {
          rb3d_blend = R300_BLEND_ENABLE      | R300_BLEND_UNKNOWN |
                       R300_BLEND_NO_SEPARATE | rdev->rb3d_blend;
     }
     else {
          rb3d_blend = R300_SRC_BLEND_GL_ONE | R300_DST_BLEND_GL_ZERO;
     }

     if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
          txfilter1 |= R300_CHROMA_KEY_FORCE;
          cmp_cntl   = SRC_CMP_EQ_COLOR | CLR_CMP_SRC_SOURCE;
     }
     else {
          master_cntl |= GMC_CLR_CMP_CNTL_DIS;
     }

     if (state->blittingflags & DSBLIT_XOR)
          master_cntl |= GMC_ROP3_XOR;
     else
          master_cntl |= GMC_ROP3_SRCCOPY;

     radeon_waitfifo( rdrv, rdev, 2 );
     radeon_out32( mmio, CLR_CMP_CNTL, cmp_cntl );
     radeon_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl );
     
     if (R300_HAS_3DREGS()) {
          radeon_waitfifo( rdrv, rdev, 29 );
          radeon_out32( mmio, R300_TX_FILTER1_0, txfilter1 );
          radeon_out32( mmio, R300_TX_ENABLE, R300_TX_ENABLE_0 );
          if (rdev->accel == DFXL_TEXTRIANGLES)
               radeon_out32( mmio, R300_RE_SHADE_MODEL, R300_RE_SHADE_MODEL_SMOOTH );
          else
               radeon_out32( mmio, R300_RE_SHADE_MODEL, R300_RE_SHADE_MODEL_FLAT );
          /* fragment program */
          radeon_out32( mmio, R300_PFS_CNTL_0, R300_PFS_CNTL_FIRST_NODE_HAS_TEX );
          radeon_out32( mmio, R300_PFS_CNTL_1, 0 );
          radeon_out32( mmio, R300_PFS_CNTL_2, 0 );
          radeon_out32( mmio, R300_PFS_NODE_0, 0 );
          radeon_out32( mmio, R300_PFS_NODE_1, 0 );
          radeon_out32( mmio, R300_PFS_NODE_2, 0 );
          radeon_out32( mmio, R300_PFS_NODE_3, R300_PFS_NODE_OUTPUT_COLOR ); 
          radeon_out32( mmio, R300_PFS_TEXI_0, R300_FPITX_OP_TXP );
          radeon_out32( mmio, R300_PFS_INSTR0_0,
                              FP_INSTRC(MAD, FP_ARGC(SRC0C_XYZ), FP_ARGC(ONE), FP_ARGC(ZERO)) );
          radeon_out32( mmio, R300_PFS_INSTR1_0,
                              FP_SELC(0,NO,XYZ,FP_TMP(0),FP_TMP(2),FP_TMP(2)) );
          radeon_out32( mmio, R300_PFS_INSTR2_0,
                              FP_INSTRA(MAD, FP_ARGA(SRC0A), FP_ARGA(ONE), FP_ARGA(ZERO)) );
          radeon_out32( mmio, R300_PFS_INSTR3_0,
                              FP_SELA(0,NO,W,FP_TMP(0),FP_TMP(2),FP_TMP(2)) );
          /* blend functions */
          radeon_out32( mmio, R300_RB3D_CBLEND, rb3d_blend );
          radeon_out32( mmio, R300_RB3D_ABLEND, rb3d_blend & 0xfffffff0 );
          /* routing */
          radeon_out32( mmio, R300_RS_CNTL_0, (1 << R300_RS_CNTL_TC_CNT_SHIFT) | 
                                              (0 << R300_RS_CNTL_CI_CNT_SHIFT) |
                                               R300_RS_CNTL_0_UNKNOWN_18 );
          radeon_out32( mmio, R300_RS_CNTL_1, 0x000000c0 );
          radeon_out32( mmio, R300_RS_ROUTE_0, R300_RS_ROUTE_ENABLE );
          /* input routing */
          radeon_out32( mmio, R300_VAP_INPUT_ROUTE_0_0, 0x21030003 );
          radeon_out32( mmio, R300_VAP_INPUT_ROUTE_1_0, 0xf688f688 );
          radeon_out32( mmio, R300_VAP_INPUT_CNTL_0, 0x5555 );
          radeon_out32( mmio, R300_VAP_INPUT_CNTL_1, R300_INPUT_CNTL_POS |
                                                     R300_INPUT_CNTL_TC0 );
          /* output routing */
          radeon_out32( mmio, R300_VAP_OUTPUT_VTX_FMT_0,
                              R300_VAP_OUTPUT_VTX_FMT_0__POS_PRESENT );
          radeon_out32( mmio, R300_VAP_OUTPUT_VTX_FMT_1, 4 );
          radeon_out32( mmio, R300_GB_VAP_RASTER_VTX_FMT_0, 
                              R300_GB_VAP_RASTER_VTX_FMT_0__POS_PRESENT );
          radeon_out32( mmio, R300_GB_VAP_RASTER_VTX_FMT_1, 4 );
          radeon_out32( mmio, R300_VAP_UNKNOWN_221C, R300_221C_CLEAR );
     }
     
     rdev->blittingflags = state->blittingflags;
     
     RADEON_SET  ( BLITTING_FLAGS );
     RADEON_UNSET( DRAWING_FLAGS );
}

