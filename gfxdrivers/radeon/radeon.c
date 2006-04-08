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
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <dfb_types.h>
#include <directfb.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/accelerators.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/screens.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( radeon )


#include "radeon.h"
#include "radeon_chipsets.h"
#include "radeon_regs.h"
#include "radeon_mmio.h"
#include "radeon_state.h"
#include "radeon_2d.h"
#include "radeon_3d.h"


/* driver capability flags */

#define R100_SUPPORTED_DRAWINGFLAGS \
     ( DSDRAW_BLEND | DSDRAW_XOR )

#define R100_SUPPORTED_DRAWINGFUNCTIONS \
     ( DFXL_FILLRECTANGLE | DFXL_FILLTRIANGLE | \
       DFXL_DRAWRECTANGLE | DFXL_DRAWLINE )

#define R100_SUPPORTED_BLITTINGFLAGS \
     ( DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | \
       DSBLIT_COLORIZE           | DSBLIT_SRC_PREMULTCOLOR | \
       DSBLIT_SRC_COLORKEY       | DSBLIT_DEINTERLACE      | \
       DSBLIT_XOR )

#define R100_SUPPORTED_BLITTINGFUNCTIONS \
     ( DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES )


#define R200_SUPPORTED_DRAWINGFLAGS \
     ( DSDRAW_BLEND | DSDRAW_XOR )

#define R200_SUPPORTED_DRAWINGFUNCTIONS \
     ( DFXL_FILLRECTANGLE | DFXL_FILLTRIANGLE | \
       DFXL_DRAWRECTANGLE | DFXL_DRAWLINE )

#define R200_SUPPORTED_BLITTINGFLAGS \
     ( DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | \
       DSBLIT_COLORIZE           | DSBLIT_SRC_PREMULTCOLOR | \
       DSBLIT_SRC_COLORKEY       | DSBLIT_DEINTERLACE      | \
       DSBLIT_XOR )

#define R200_SUPPORTED_BLITTINGFUNCTIONS \
     ( DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES )


#define R300_SUPPORTED_DRAWINGFLAGS \
     ( DSDRAW_XOR )

#define R300_SUPPORTED_DRAWINGFUNCTIONS \
     ( DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE )

#define R300_SUPPORTED_BLITTINGFLAGS \
     ( DSBLIT_SRC_COLORKEY | DSBLIT_XOR )
     
#define R300_SUPPORTED_BLITTINGFUNCTIONS \
     ( DFXL_BLIT )


#define DSBLIT_MODULATE_ALPHA ( DSBLIT_BLEND_ALPHACHANNEL | \
                                DSBLIT_BLEND_COLORALPHA )
#define DSBLIT_MODULATE_COLOR ( DSBLIT_BLEND_COLORALPHA   | \
                                DSBLIT_COLORIZE           | \
                                DSBLIT_SRC_PREMULTCOLOR )
#define DSBLIT_MODULATE       ( DSBLIT_MODULATE_ALPHA     | \
                                DSBLIT_MODULATE_COLOR )

#define RADEON_DRAW_3D()      ( rdev->accel & DFXL_FILLTRIANGLE || \
                                rdev->drawingflags & ~DSDRAW_XOR )
#define RADEON_BLIT_3D()      ( rdev->accel & ~DFXL_BLIT                     || \
                                rdev->blittingflags & ~(DSBLIT_XOR |            \
                                                        DSBLIT_SRC_COLORKEY) || \
                               (rdev->dst_format != rdev->src_format        &&  \
                                !(DFB_PLANAR_PIXELFORMAT(rdev->dst_format) &&   \
                                  DFB_PLANAR_PIXELFORMAT(rdev->src_format)  )))
                                
#define RADEON_FUNC( f ) DFB_PLANAR_PIXELFORMAT(rdev->dst_format) ? f##_420 : f



static void
radeon_get_monitors( RadeonDriverData  *rdrv,
                     RadeonDeviceData  *rdev,
                     RadeonMonitorType *ret_monitor1,
                     RadeonMonitorType *ret_monitor2 )
{
     RadeonMonitorType  dvimon = MT_NONE;
     RadeonMonitorType  vgamon = MT_NONE;
     const char        *name[] = { "NONE", "CRT", "DFP", 
                                   "LCD",  "CTV", "STV" }; 
     __u32              tmp;
      
     if (rdev->chipset != CHIP_R100) {        
          if (rdev->chipset >= CHIP_R300 ||
              rdev->chipset == CHIP_UNKNOWN)
               tmp = radeon_in32( rdrv->mmio_base, BIOS_0_SCRATCH );
          else
               tmp = radeon_in32( rdrv->mmio_base, BIOS_4_SCRATCH );
     
          /* DVI/TVO port */
		if (tmp & 0x08)
			dvimon = MT_DFP;
		else if (tmp & 0x4)
			dvimon = MT_LCD;
		else if (tmp & 0x200)
			dvimon = MT_CRT;
		else if (tmp & 0x10)
			dvimon = MT_CTV;
		else if (tmp & 0x20)
			dvimon = MT_STV;
		
		/* VGA port */
		if (tmp & 0x2)
			vgamon = MT_CRT;
		else if (tmp & 0x800)
			vgamon = MT_DFP;
		else if (tmp & 0x400)
			vgamon = MT_LCD;
		else if (tmp & 0x1000)
			vgamon = MT_CTV;
		else if (tmp & 0x2000)
			vgamon = MT_STV;
     }
     else {
          tmp = radeon_in32( rdrv->mmio_base, FP_GEN_CNTL );
          
          if (tmp & FP_EN_TMDS)
               vgamon = MT_DFP;
          else
               vgamon = MT_CRT;
     }

     D_DEBUG( "DirectFB/Radeon: "
              "DVI/TVO Port -> %s, VGA Port -> %s.\n", 
              name[dvimon], name[vgamon] );
     
     if (dvimon) {
          /* If DVI port is connected, then
           * DVI port is the primary head and 
           * CRT port is the secondary head.
           */
          if (ret_monitor1)
               *ret_monitor1 = dvimon;
          if (ret_monitor2)
               *ret_monitor2 = vgamon;
     } 
     else {
          if (ret_monitor1)
               *ret_monitor1 = vgamon;
          if (ret_monitor2)
               *ret_monitor2 = MT_NONE;
     }     
}

void
radeon_reset( RadeonDriverData *rdrv, RadeonDeviceData *rdev )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     __u32          clock_cntl_index;
     __u32          mclk_cntl;
     __u32          rbbm_soft_reset;
     __u32          host_path_cntl;
     
     clock_cntl_index = radeon_in32( mmio, CLOCK_CNTL_INDEX );
     
     mclk_cntl  = radeon_inpll( mmio, MCLK_CNTL );
     radeon_outpll( mmio, MCLK_CNTL, mclk_cntl     |
                                     FORCEON_MCLKA |
			                      FORCEON_MCLKB |
			                      FORCEON_YCLKA |
			                      FORCEON_YCLKB |
			                      FORCEON_MC    |
			                      FORCEON_AIC );

     host_path_cntl  = radeon_in32( mmio, HOST_PATH_CNTL );
     rbbm_soft_reset = radeon_in32( mmio, RBBM_SOFT_RESET );
    
     radeon_out32( mmio, RBBM_SOFT_RESET, rbbm_soft_reset |
                                        SOFT_RESET_CP | SOFT_RESET_HI |
                                        SOFT_RESET_SE | SOFT_RESET_RE |
                                        SOFT_RESET_PP | SOFT_RESET_E2 |
                                        SOFT_RESET_RB );
     radeon_in32( mmio, RBBM_SOFT_RESET );
     
     radeon_out32( mmio, RBBM_SOFT_RESET, rbbm_soft_reset &
                                       ~(SOFT_RESET_CP | SOFT_RESET_HI |
                                         SOFT_RESET_SE | SOFT_RESET_RE |
                                         SOFT_RESET_PP | SOFT_RESET_E2 |
                                         SOFT_RESET_RB) );
     radeon_in32( mmio, RBBM_SOFT_RESET );
     
     radeon_out32( mmio, HOST_PATH_CNTL, host_path_cntl | HDP_SOFT_RESET );
     radeon_in32( mmio, HOST_PATH_CNTL );
     radeon_out32( mmio, HOST_PATH_CNTL, host_path_cntl );

     radeon_out32( mmio, RBBM_SOFT_RESET, rbbm_soft_reset );

     radeon_out32( mmio, CLOCK_CNTL_INDEX, clock_cntl_index );
     radeon_outpll( mmio, MCLK_CNTL, mclk_cntl );
     
     rdev->set = 0;
     rdev->src_format = DSPF_UNKNOWN;
     rdev->dst_format = DSPF_UNKNOWN;
     rdev->fifo_space = 0;
}
     

static void radeonAfterSetVar( void *drv, void *dev )
{
     radeon_waitidle( (RadeonDriverData*)drv, (RadeonDeviceData*)dev );
     radeon_reset( (RadeonDriverData*)drv, (RadeonDeviceData*)dev );
}

static void radeonEngineReset( void *drv, void *dev )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     volatile __u8    *mmio = rdrv->mmio_base;
     __u32             tmp;

     radeon_out32( mmio, SURFACE_CNTL, rdev->surface_cntl_c );
     
     tmp = radeon_in32( mmio, CRTC_OFFSET_CNTL );
     radeon_out32( mmio, CRTC_OFFSET_CNTL, (tmp & ~CRTC_TILE_EN) | CRTC_HSYNC_EN ); 
     
     radeon_waitfifo( rdrv, rdev, 3 );
     tmp = radeon_in32( mmio, DP_DATATYPE );
#ifdef WORDS_BIGENDIAN
     radeon_out32( mmio, DP_DATATYPE, tmp | HOST_BIG_ENDIAN_EN );
#else
     radeon_out32( mmio, DP_DATATYPE, tmp & ~HOST_BIG_ENDIAN_EN );
#endif
     radeon_out32( mmio, DEFAULT_SC_BOTTOM_RIGHT, DEFAULT_SC_RIGHT_MAX |
                                                  DEFAULT_SC_BOTTOM_MAX );
     radeon_out32( mmio, AUX_SC_CNTL, 0 );
    
     if (rdev->chipset >= CHIP_R300) {
          /* enable caches */
          radeon_waitfifo( rdrv, rdev, 1 );
          radeon_out32( mmio, RB2D_DSTCACHE_MODE, RB2D_DC_2D_CACHE_AUTOFLUSH |
                                                  R300_RB2D_DC_ENABLE );
#if 0
          /* restor 3d engine state */
          radeon_waitfifo( rdrv, rdev, 42 );
          radeon_out32( mmio, 0x2080, 0x0030045a );
          radeon_out32( mmio, 0x2134, 0x00ffffff );
          radeon_out32( mmio, 0x2138, 0 );
          radeon_out32( mmio, 0x2140, 0 );
          radeon_out32( mmio, 0x21dc, 0xaaaaaaaa );
          radeon_out32( mmio, R300_VAP_UNKNOWN_221C, R300_221C_NORMAL );
          if (rdev->chipset >= CHIP_RV350)
               radeon_out32( mmio, R300_VAP_UNKNOWN_2288, R300_2288_RV350 );
          else
               radeon_out32( mmio, R300_VAP_UNKNOWN_2288, R300_2288_R300 );
          radeon_out32( mmio, R300_VAP_PVS_CNTL_1, 0 );
          radeon_out32( mmio, R300_VAP_PVS_CNTL_2, 0 );
          radeon_out32( mmio, R300_VAP_PVS_CNTL_3, 0 );
          radeon_out32( mmio, R300_GB_VAP_RASTER_VTX_FMT_0, 
                              R300_GB_VAP_RASTER_VTX_FMT_0__POS_PRESENT |
                              R300_GB_VAP_RASTER_VTX_FMT_0__COLOR_0_PRESENT );
          radeon_out32( mmio, R300_GB_VAP_RASTER_VTX_FMT_1, 0 );
          radeon_out32( mmio, R300_GB_ENABLE, R300_GB_POINT_STUFF_ENABLE |
                                              R300_GB_LINE_STUFF_ENABLE  |
                                              R300_GB_TRIANGLE_STUFF_ENABLE );
          radeon_out32( mmio, R300_GB_MSPOS0, 0x66666666 );
          radeon_out32( mmio, R300_GB_MSPOS1, 0x66666666 );
          radeon_out32( mmio, R300_GB_TILE_CONFIG, 0 );
          radeon_out32( mmio, R300_GB_FIFO_SIZE, 0x0fffffff );
          radeon_out32( mmio, R300_GB_SELECT, 0 );
          radeon_out32( mmio, R300_GB_AA_CONFIG, 0 );
          radeon_out32( mmio, R300_RE_POINTSIZE, (6 << R300_POINTSIZE_Y_SHIFT) |
                                                 (6 << R300_POINTSIZE_X_SHIFT) );
          radeon_out32( mmio, R300_RE_LINE_CNT, (6 << R300_LINESIZE_SHIFT) |
                                                 R300_LINE_CNT_HO          |
                                                 R300_LINE_CNT_VE );
          radeon_out32( mmio, R300_RE_SHADE_MODEL, R300_RE_SHADE_MODEL_FLAT );
          radeon_out32( mmio, R300_RE_POLYGON_MODE, 0 );
          radeon_out32( mmio, R300_RE_OCCLUSION_CNTL, 0 );
          radeon_out32( mmio, R300_RE_CULL_CNTL, 0 );
          radeon_out32( mmio, R300_RS_CNTL_0, (1 << R300_RS_CNTL_CI_CNT_SHIFT) |
                                              R300_RS_CNTL_0_UNKNOWN_18 );
          radeon_out32( mmio, R300_RS_CNTL_1, 0 );
          radeon_out32( mmio, R300_RS_INTERP_0, 0x00d10000 );
          radeon_out32( mmio, R300_RS_ROUTE_0, 0x00004000 );
          radeon_out32( mmio, R300_PFS_CNTL_0, 0 );
          radeon_out32( mmio, R300_PFS_CNTL_1, 0 );
          radeon_out32( mmio, R300_PFS_CNTL_2, 0 );
          radeon_out32( mmio, R300_PFS_NODE_0, 0 );
          radeon_out32( mmio, R300_PFS_NODE_1, 0 );
          radeon_out32( mmio, R300_PFS_NODE_2, 0 );
          radeon_out32( mmio, R300_PFS_NODE_3, R300_PFS_NODE_OUTPUT_COLOR );
          radeon_out32( mmio, R300_PP_ALPHA_TEST, R300_ALPHA_TEST_PASS );
          radeon_out32( mmio, R300_RB3D_COLORMASK, R300_COLORMASK0_B | 
                                                   R300_COLORMASK0_G |
                                                   R300_COLORMASK0_R |
                                                   R300_COLORMASK0_A );
          radeon_out32( mmio, R300_RB3D_ZSTENCIL_CNTL_1, R300_ZS_ALWAYS );
          radeon_out32( mmio, R300_RB3D_ZSTENCIL_CNTL_2, 0xffffff00 );
          
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
     else if (rdev->chipset >= CHIP_R200) {
          /* enable caches */
          radeon_waitfifo( rdrv, rdev, 2 );
          radeon_out32( mmio, RB2D_DSTCACHE_MODE, RB2D_DC_2D_CACHE_AUTOFLUSH     |
                                                  RB2D_DC_3D_CACHE_AUTOFLUSH     |
                                                  R200_RB2D_DC_2D_CACHE_AUTOFREE |
                                                  R200_RB2D_DC_3D_CACHE_AUTOFREE );
          radeon_out32( mmio, RB3D_DSTCACHE_MODE, RB3D_DC_2D_CACHE_AUTOFLUSH     |
                                                  RB3D_DC_3D_CACHE_AUTOFLUSH     |
                                                  R200_RB3D_DC_2D_CACHE_AUTOFREE |
                                                  R200_RB3D_DC_3D_CACHE_AUTOFREE );
          
          /* restor 3d engine state */
          radeon_waitfifo( rdrv, rdev, 9 );
          radeon_out32( mmio, SE_LINE_WIDTH, 0x10 );
          radeon_out32( mmio, PP_MISC, ALPHA_TEST_PASS ); 
          radeon_out32( mmio, R200_PP_CNTL_X, 0 );
          radeon_out32( mmio, R200_PP_TXMULTI_CTL_0, 0 ); 
          radeon_out32( mmio, R200_RE_CNTL, R200_SCISSOR_ENABLE );
          radeon_out32( mmio, R200_SE_VTX_STATE_CNTL, 0 );
          radeon_out32( mmio, R200_SE_VAP_CNTL, R200_VAP_VF_MAX_VTX_NUM |
                                                R200_VAP_FORCE_W_TO_ONE );
          radeon_out32( mmio, R200_SE_VAP_CNTL_STATUS, TCL_BYPASS );
          radeon_out32( mmio, RB3D_ROPCNTL, ROP_XOR );
          
          /* set YUV422 color buffer */
          radeon_waitfifo( rdrv, rdev, 3 );
          radeon_out32( mmio, R200_PP_TXFILTER_1, 0 );
          radeon_out32( mmio, R200_PP_TXFORMAT_1, R200_TXFORMAT_VYUY422 );
          radeon_out32( mmio, R200_PP_TXFORMAT_X_1, 0 );
     }
     else if (rdev->chipset >= CHIP_R100) {
          /* enable caches */
          radeon_waitfifo( rdrv, rdev, 2 );
          radeon_out32( mmio, RB2D_DSTCACHE_MODE, RB2D_DC_2D_CACHE_AUTOFLUSH |
                                                  RB2D_DC_3D_CACHE_AUTOFLUSH );
          radeon_out32( mmio, RB3D_DSTCACHE_MODE, RB3D_DC_2D_CACHE_AUTOFLUSH |
                                                  RB3D_DC_3D_CACHE_AUTOFLUSH );
          
          /* restor 3d engine state */
          radeon_waitfifo( rdrv, rdev, 5 ); 
          radeon_out32( mmio, SE_COORD_FMT, VTX_XY_PRE_MULT_1_OVER_W0 |
                                            TEX1_W_ROUTING_USE_W0 );
          radeon_out32( mmio, SE_LINE_WIDTH, 0x10 );
          radeon_out32( mmio, SE_CNTL_STATUS, TCL_BYPASS );
          radeon_out32( mmio, PP_MISC, ALPHA_TEST_PASS );
          radeon_out32( mmio, RB3D_ROPCNTL, ROP_XOR );
     
          /* set YUV422 color buffer */
          radeon_waitfifo( rdrv, rdev, 2 );
          radeon_out32( mmio, PP_TXFILTER_1, 0 );
          radeon_out32( mmio, PP_TXFORMAT_1, TXFORMAT_VYUY422 );
     }
     
     /* sync 2d and 3d engines */
     radeon_waitfifo( rdrv, rdev, 1 );
     radeon_out32( mmio, ISYNC_CNTL, ISYNC_ANY2D_IDLE3D |
                                     ISYNC_ANY3D_IDLE2D );
}

static void radeonEngineSync( void *drv, void *dev )
{
     radeon_waitidle( (RadeonDriverData*)drv, (RadeonDeviceData*)dev );
}

static void radeonFlushTextureCache( void *drv, void *dev )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     volatile __u8    *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 1 );
     if (rdev->chipset >= CHIP_R300)
          radeon_out32( mmio, R300_TX_CNTL, 0 );
     else if (rdev->chipset >= CHIP_R200)
          radeon_out32( mmio, R200_PP_TXOFFSET_0, rdev->src_offset );
     else if (rdev->chipset >= CHIP_R100)
          radeon_out32( mmio, PP_TXOFFSET_0, rdev->src_offset );
}

#ifdef WORDS_BIGENDIAN
static void radeonSurfaceEnter( void *drv, void *dev, 
                                SurfaceBuffer *buffer, DFBSurfaceLockFlags flags )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     volatile __u8    *mmio = rdrv->mmio_base;
     __u32             tmp;

     if (!(flags & DSLF_WRITE))
          return;

     rdev->surface_cntl_p = radeon_in32( mmio, SURFACE_CNTL );
     tmp = rdev->surface_cntl_p & ~SURF_TRANSLATION_DIS;

     switch (buffer->storage) {
          case CSS_VIDEO:
               tmp &= ~(NONSURF_AP0_SWP_16BPP | NONSURF_AP0_SWP_32BPP);
               switch (DFB_BITS_PER_PIXEL( buffer->format )) {
                    case 16:
                         tmp |= NONSURF_AP0_SWP_16BPP;
                         break;
                    case 32:
                         tmp |= NONSURF_AP0_SWP_32BPP;
                         break;
                    default:
                         break;
               }
               break;
          case CSS_AUXILIARY:
               tmp &= ~(NONSURF_AP1_SWP_16BPP | NONSURF_AP1_SWP_32BPP);
               switch (DFB_BITS_PER_PIXEL( buffer->format )) {
                    case 16:
                         tmp |= NONSURF_AP1_SWP_16BPP;
                         break;
                    case 32:
                         tmp |= NONSURF_AP1_SWP_32BPP;
                         break;
                   default:
                         break;
               }
               break;
          default:
               D_BUG( "unknown buffer storage" );
               return;
     }

     radeon_out32( mmio, SURFACE_CNTL, tmp );
     rdev->surface_cntl_c = tmp;
}

static void radeonSurfaceLeave( void *drv, void *dev, SurfaceBuffer *buffer )
{ 
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
     volatile __u8    *mmio = rdrv->mmio_base;
     
     if (rdev->surface_cntl_p != rdev->surface_cntl_c) {
          radeon_out32( mmio, SURFACE_CNTL, rdev->surface_cntl_p );
          rdev->surface_cntl_c = rdev->surface_cntl_p;
     }
}
#endif
          
static void r100CheckState( void *drv, void *dev,
                            CardState *state, DFBAccelerationMask accel )
{
     RadeonDeviceData *rdev        = (RadeonDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;
     
     int supported_drawingfuncs  = R100_SUPPORTED_DRAWINGFUNCTIONS; 
     int supported_drawingflags  = R100_SUPPORTED_DRAWINGFLAGS;
     int supported_blittingfuncs = R100_SUPPORTED_BLITTINGFUNCTIONS;
     int supported_blittingflags = R100_SUPPORTED_BLITTINGFLAGS;
     
     switch (destination->format) {               
          case DSPF_A8:
          case DSPF_RGB332:
          case DSPF_ARGB4444:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
               
          case DSPF_LUT8:
          case DSPF_ALUT44:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != destination->format)
                    return;
               supported_drawingflags   =  DSDRAW_NOFX;
               supported_blittingfuncs &= ~DFXL_TEXTRIANGLES;
               supported_blittingflags &= ~DSBLIT_MODULATE;
               break;

          case DSPF_ARGB2554:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != destination->format)
                    return;
               supported_drawingfuncs  &= ~DFXL_FILLTRIANGLE;
               supported_drawingflags   =  DSDRAW_XOR;
               supported_blittingfuncs &= ~DFXL_TEXTRIANGLES;
               supported_blittingflags &= ~DSBLIT_MODULATE;
               break;
               
          case DSPF_AiRGB:
               supported_drawingflags  &= ~DSDRAW_BLEND;
               supported_blittingflags &= ~DSBLIT_MODULATE_ALPHA;
               break;

          case DSPF_I420:
          case DSPF_YV12:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != DSPF_A8      &&
                   source->format != DSPF_I420    &&
                   source->format != DSPF_YV12)
                    return;
          case DSPF_YUY2:
          case DSPF_UYVY:
               if (source && source->format != DSPF_A8)
                    supported_blittingflags &= ~(DSBLIT_COLORIZE | DSBLIT_SRC_COLORKEY);
               break;

          case DSPF_AYUV:
               if (DFB_BLITTING_FUNCTION( accel ) && source->format != DSPF_A8) {
                    if (source->format != DSPF_AYUV)
                         return;
                    supported_blittingflags &= ~DSBLIT_COLORIZE;
               }
               break;
               
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
               if (destination->format != source->format)
                    return;
               supported_blittingfuncs  = DFXL_BLIT;
               supported_blittingflags &= DSBLIT_SRC_COLORKEY | DSBLIT_XOR;
          }
               
          if (accel & ~supported_blittingfuncs ||
              state->blittingflags & ~supported_blittingflags)
               return;
          
          if (source->width  < 8 || source->width  > 2048 ||
              source->height < 8 || source->height > 2048)
               return;

          if (state->blittingflags & DSBLIT_MODULATE_ALPHA &&
              state->dst_blend == DSBF_SRCALPHASAT)
               return;
               
          switch (source->format) {                    
               case DSPF_RGB332:
               case DSPF_ARGB4444:
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
                    if (destination->format == DSPF_UYVY ||
                        destination->format == DSPF_YUY2)
                         return;
               case DSPF_A8:
               case DSPF_YUY2:
               case DSPF_UYVY:
                    break;
               
               case DSPF_LUT8:
               case DSPF_ALUT44:
               case DSPF_ARGB2554:
               case DSPF_AYUV:
                    if (destination->format != source->format)
                         return;
                    break;

               case DSPF_I420:
               case DSPF_YV12:
                    if (destination->format != DSPF_I420 &&
                        destination->format != DSPF_YV12)
                         return;
                    break;
               
               default:
                    return;
          }

          state->accel |= supported_blittingfuncs;
          rdev->blitting_mask = supported_blittingfuncs;
     } 
     else {
          if (accel & ~supported_drawingfuncs ||
              state->drawingflags & ~supported_drawingflags)
               return;

          if (state->drawingflags & DSDRAW_BLEND &&
              state->dst_blend == DSBF_SRCALPHASAT)
               return;
               
          state->accel |= supported_drawingfuncs;
          rdev->drawing_mask = supported_drawingfuncs;
     }
}

static void r200CheckState( void *drv, void *dev,
                            CardState *state, DFBAccelerationMask accel )
{
     RadeonDeviceData *rdev        = (RadeonDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;
     
     int supported_drawingfuncs  = R200_SUPPORTED_DRAWINGFUNCTIONS; 
     int supported_drawingflags  = R200_SUPPORTED_DRAWINGFLAGS;
     int supported_blittingfuncs = R200_SUPPORTED_BLITTINGFUNCTIONS;
     int supported_blittingflags = R200_SUPPORTED_BLITTINGFLAGS;
     
     switch (destination->format) {               
          case DSPF_A8:
          case DSPF_RGB332:
          case DSPF_ARGB4444:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
               
          case DSPF_LUT8:
          case DSPF_ALUT44:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != destination->format)
                    return;
               supported_drawingflags   =  DSDRAW_NOFX;
               supported_blittingfuncs &= ~DFXL_TEXTRIANGLES;
               supported_blittingflags &= ~DSBLIT_MODULATE;
               break;

          case DSPF_ARGB2554:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != destination->format)
                    return;
               supported_drawingfuncs  &= ~DFXL_FILLTRIANGLE;
               supported_drawingflags   =  DSDRAW_XOR;
               supported_blittingfuncs &= ~DFXL_TEXTRIANGLES;
               supported_blittingflags &= ~DSBLIT_MODULATE;
               break;
               
          case DSPF_AiRGB:
               supported_drawingflags  &= ~DSDRAW_BLEND;
               supported_blittingflags &= ~DSBLIT_MODULATE_ALPHA;
               break;

          case DSPF_I420:
          case DSPF_YV12:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != DSPF_A8      &&
                   source->format != DSPF_I420    &&
                   source->format != DSPF_YV12)
                    return;
          case DSPF_YUY2:
          case DSPF_UYVY:
               if (source && source->format != DSPF_A8)
                    supported_blittingflags &= ~(DSBLIT_COLORIZE | DSBLIT_SRC_COLORKEY);
               break;
               
          case DSPF_AYUV:
               if (DFB_BLITTING_FUNCTION( accel ) && source->format != DSPF_A8) {
                    if (source->format != DSPF_AYUV)
                         return;
                    supported_blittingflags &= ~DSBLIT_COLORIZE;
               }
               break;
          
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
               if (destination->format != source->format)
                    return;
               supported_blittingfuncs  = DFXL_BLIT;
               supported_blittingflags &= DSBLIT_SRC_COLORKEY | DSBLIT_XOR;
          }
               
          if (accel & ~supported_blittingfuncs ||
              state->blittingflags & ~supported_blittingflags)
               return;
          
          if (source->width  < 8 || source->width  > 2048 ||
              source->height < 8 || source->height > 2048)
               return;

          if (state->blittingflags & DSBLIT_MODULATE_ALPHA &&
              state->dst_blend == DSBF_SRCALPHASAT)
               return;
               
          switch (source->format) {                    
               case DSPF_RGB332:
               case DSPF_ARGB4444:
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
                    if (destination->format == DSPF_UYVY ||
                        destination->format == DSPF_YUY2)
                         return;
               case DSPF_A8:
               case DSPF_YUY2:
               case DSPF_UYVY:
                    break;
               
               case DSPF_LUT8:
               case DSPF_ALUT44:
               case DSPF_ARGB2554:
               case DSPF_AYUV:
                    if (destination->format != source->format)
                         return;
                    break;

               case DSPF_I420:
               case DSPF_YV12:
                    if (destination->format != DSPF_I420 &&
                        destination->format != DSPF_YV12)
                         return;
                    break;
               
               default:
                    return;
          }

          state->accel |= supported_blittingfuncs;
          rdev->blitting_mask = supported_blittingfuncs;
     } 
     else {
          if (accel & ~supported_drawingfuncs ||
              state->drawingflags & ~supported_drawingflags)
               return;

          if (state->drawingflags & DSDRAW_BLEND &&
              state->dst_blend == DSBF_SRCALPHASAT)
               return;
               
          state->accel |= supported_drawingfuncs;
          rdev->drawing_mask = supported_drawingfuncs;
     }
}

static void r300CheckState( void *drv, void *dev,
                            CardState *state, DFBAccelerationMask accel )
{
     RadeonDeviceData *rdev        = (RadeonDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;
     
     int supported_drawingfuncs  = R300_SUPPORTED_DRAWINGFUNCTIONS; 
     int supported_drawingflags  = R300_SUPPORTED_DRAWINGFLAGS;
     int supported_blittingfuncs = R300_SUPPORTED_BLITTINGFUNCTIONS;
     int supported_blittingflags = R300_SUPPORTED_BLITTINGFLAGS;
     
     switch (destination->format) {               
          case DSPF_A8:
          case DSPF_RGB332:
          case DSPF_ARGB4444:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
               
          case DSPF_LUT8:
          case DSPF_ALUT44:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != destination->format)
                    return;
               supported_drawingflags   =  DSDRAW_NOFX;
               supported_blittingfuncs &= ~DFXL_TEXTRIANGLES;
               supported_blittingflags &= ~DSBLIT_MODULATE;
               break;

          case DSPF_ARGB2554:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != destination->format)
                    return;
               supported_drawingfuncs  &= ~DFXL_FILLTRIANGLE;
               supported_drawingflags   =  DSDRAW_XOR;
               supported_blittingfuncs &= ~DFXL_TEXTRIANGLES;
               supported_blittingflags &= ~DSBLIT_MODULATE;
               break;
               
          case DSPF_AiRGB:
               supported_drawingflags  &= ~DSDRAW_BLEND;
               supported_blittingflags &= ~DSBLIT_MODULATE_ALPHA;
               break;

          case DSPF_I420:
          case DSPF_YV12:
               if (DFB_BLITTING_FUNCTION( accel ) &&
                   source->format != DSPF_A8      &&
                   source->format != DSPF_I420    &&
                   source->format != DSPF_YV12)
                    return;
          case DSPF_YUY2:
          case DSPF_UYVY:
               if (source && source->format != DSPF_A8)
                    supported_blittingflags &= ~(DSBLIT_COLORIZE | DSBLIT_SRC_COLORKEY);
               break;
               
          case DSPF_AYUV:
               if (DFB_BLITTING_FUNCTION( accel ) && source->format != DSPF_A8) {
                    if (source->format != DSPF_AYUV)
                         return;
                    supported_blittingflags &= ~DSBLIT_COLORIZE;
               }
               break;
          
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* actually 3D acceleration doesn't work,
           * therefore format conversion is unsupported */
          if (source->format != destination->format)
               supported_blittingfuncs = DFXL_NONE;
               
          if (accel & ~supported_blittingfuncs ||
              state->blittingflags & ~supported_blittingflags)
               return;
          
          if (source->width  < 8 || source->width  > 2048 ||
              source->height < 8 || source->height > 2048)
               return;

          if (state->blittingflags & DSBLIT_MODULATE_ALPHA &&
              state->dst_blend == DSBF_SRCALPHASAT)
               return;
               
          switch (source->format) {                    
               case DSPF_RGB332:
               case DSPF_ARGB4444:
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
                    if (destination->format == DSPF_UYVY ||
                        destination->format == DSPF_YUY2)
                         return;
               case DSPF_A8:
                    break;
               
               case DSPF_LUT8:
               case DSPF_ALUT44:
               case DSPF_ARGB2554:
               case DSPF_AYUV:
                    if (destination->format != source->format)
                         return;
                    break;
                    
               case DSPF_YUY2:
               case DSPF_UYVY:
                    if (destination->format != DSPF_YUY2 &&
                        destination->format != DSPF_UYVY)
                         return;
                    break;

               case DSPF_I420:
               case DSPF_YV12:
                    if (destination->format != DSPF_I420 &&
                        destination->format != DSPF_YV12)
                         return;
                    break;
               
               default:
                    return;
          }

          state->accel |= supported_blittingfuncs;
          rdev->blitting_mask = supported_blittingfuncs;
     } 
     else {
          if (accel & ~supported_drawingfuncs ||
              state->drawingflags & ~supported_drawingflags)
               return;

          if (state->drawingflags & DSDRAW_BLEND &&
              state->dst_blend == DSBF_SRCALPHASAT)
               return;
               
          state->accel |= supported_drawingfuncs;
          rdev->drawing_mask = supported_drawingfuncs;
     }
}

static void r100SetState( void *drv, void *dev,
                          GraphicsDeviceFuncs *funcs,
                          CardState *state, DFBAccelerationMask accel )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
 
     rdev->set &= ~state->modified;
     if (DFB_BLITTING_FUNCTION( accel )) {
          if ((rdev->accel ^ accel) & DFXL_TEXTRIANGLES)
               rdev->set &= ~SMF_BLITTING_FLAGS;
     }
     
     rdev->accel = accel;
     
     r100_set_destination( rdrv, rdev, state );
     r100_set_clip( rdrv, rdev, state );
    
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               r100_set_drawing_color( rdrv, rdev, state );
               
               if (state->drawingflags & DSDRAW_BLEND)
                    r100_set_blend_function( rdrv, rdev, state );
               
               r100_set_drawingflags( rdrv, rdev, state );

               if (RADEON_DRAW_3D()) {
                    funcs->FillRectangle = RADEON_FUNC(r100FillRectangle3D);
                    funcs->FillTriangle  = RADEON_FUNC(r100FillTriangle);
                    funcs->DrawRectangle = RADEON_FUNC(r100DrawRectangle3D);
                    funcs->DrawLine      = RADEON_FUNC(r100DrawLine3D);
               } else {
                    funcs->FillRectangle = RADEON_FUNC(radeonFillRectangle2D);
                    funcs->FillTriangle  = NULL;
                    funcs->DrawRectangle = RADEON_FUNC(radeonDrawRectangle2D);
                    funcs->DrawLine      = RADEON_FUNC(radeonDrawLine2D);
               }

               state->set = rdev->drawing_mask;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
          case DFXL_TEXTRIANGLES:     
               r100_set_source( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE_ALPHA)
                    r100_set_blend_function( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE_COLOR)
                    r100_set_blitting_color( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    r100_set_src_colorkey( rdrv, rdev, state );
               
               r100_set_blittingflags( rdrv, rdev, state );

               if (RADEON_BLIT_3D()) {
                    funcs->Blit             = RADEON_FUNC(r100Blit3D);
                    funcs->StretchBlit      = RADEON_FUNC(r100StretchBlit);
                    funcs->TextureTriangles = RADEON_FUNC(r100TextureTriangles);
               } else {
                    funcs->Blit             = RADEON_FUNC(radeonBlit2D);
                    funcs->StretchBlit      = NULL;
                    funcs->TextureTriangles = NULL;
               }
               
               state->set = (accel & DFXL_TEXTRIANGLES) 
                            ? : (rdev->blitting_mask & ~DFXL_TEXTRIANGLES);
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }
     
     state->modified = 0;
}

static void r200SetState( void *drv, void *dev,
                          GraphicsDeviceFuncs *funcs,
                          CardState *state, DFBAccelerationMask accel )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
 
     rdev->set &= ~state->modified;
     if (DFB_BLITTING_FUNCTION( accel )) {
          if ((rdev->accel ^ accel) & DFXL_TEXTRIANGLES)
               rdev->set &= ~SMF_BLITTING_FLAGS;
     }
     
     rdev->accel = accel;
     
     r200_set_destination( rdrv, rdev, state );
     r200_set_clip( rdrv, rdev, state );
    
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               r200_set_drawing_color( rdrv, rdev, state );
               
               if (state->drawingflags & DSDRAW_BLEND)
                    r200_set_blend_function( rdrv, rdev, state );
               
               r200_set_drawingflags( rdrv, rdev, state );

               if (RADEON_DRAW_3D()) {
                    funcs->FillRectangle = RADEON_FUNC(r200FillRectangle3D);
                    funcs->FillTriangle  = RADEON_FUNC(r200FillTriangle);
                    funcs->DrawRectangle = RADEON_FUNC(r200DrawRectangle3D);
                    funcs->DrawLine      = RADEON_FUNC(r200DrawLine3D);
               } else {
                    funcs->FillRectangle = RADEON_FUNC(radeonFillRectangle2D);
                    funcs->FillTriangle  = NULL;
                    funcs->DrawRectangle = RADEON_FUNC(radeonDrawRectangle2D);
                    funcs->DrawLine      = RADEON_FUNC(radeonDrawLine2D);
               }

               state->set = rdev->drawing_mask;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
          case DFXL_TEXTRIANGLES:     
               r200_set_source( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE_ALPHA)
                    r200_set_blend_function( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE_COLOR)
                    r200_set_blitting_color( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    r200_set_src_colorkey( rdrv, rdev, state );
               
               r200_set_blittingflags( rdrv, rdev, state );
               
               if (RADEON_BLIT_3D()) {
                    funcs->Blit             = RADEON_FUNC(r200Blit3D);
                    funcs->StretchBlit      = RADEON_FUNC(r200StretchBlit);
                    funcs->TextureTriangles = RADEON_FUNC(r200TextureTriangles);
               } else {
                    funcs->Blit             = RADEON_FUNC(radeonBlit2D);
                    funcs->StretchBlit      = NULL;
                    funcs->TextureTriangles = NULL;
               }
               
               state->set = (accel & DFXL_TEXTRIANGLES) 
                            ? : (rdev->blitting_mask & ~DFXL_TEXTRIANGLES);
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }
     
     state->modified = 0;
}

static void r300SetState( void *drv, void *dev,
                          GraphicsDeviceFuncs *funcs,
                          CardState *state, DFBAccelerationMask accel )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) drv;
     RadeonDeviceData *rdev = (RadeonDeviceData*) dev;
 
     rdev->set &= ~state->modified;
     if (DFB_BLITTING_FUNCTION( accel )) {
          if ((rdev->accel ^ accel) & DFXL_TEXTRIANGLES)
               rdev->set &= ~SMF_BLITTING_FLAGS;
     }
     
     rdev->accel = accel;
     
     r300_set_destination( rdrv, rdev, state );
     r300_set_clip( rdrv, rdev, state );
    
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          /*case DFXL_FILLTRIANGLE:*/
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               r300_set_drawing_color( rdrv, rdev, state );
               
               if (state->drawingflags & DSDRAW_BLEND)
                    r300_set_blend_function( rdrv, rdev, state );
               
               r300_set_drawingflags( rdrv, rdev, state );

               funcs->FillRectangle = RADEON_FUNC(radeonFillRectangle2D);
               funcs->FillTriangle  = NULL;
               funcs->DrawRectangle = RADEON_FUNC(radeonDrawRectangle2D);
               funcs->DrawLine      = RADEON_FUNC(radeonDrawLine2D);

               state->set = rdev->drawing_mask;
               break;

          case DFXL_BLIT:
          /*case DFXL_STRETCHBLIT:*/
          /*case DFXL_TEXTRIANGLES:*/  
               r300_set_source( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE_ALPHA)
                    r300_set_blend_function( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE_COLOR)
                    r300_set_blitting_color( rdrv, rdev, state );
               
               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    r300_set_src_colorkey( rdrv, rdev, state );
               
               r300_set_blittingflags( rdrv, rdev, state );

               funcs->Blit             = RADEON_FUNC(radeonBlit2D);
               funcs->StretchBlit      = NULL;
               funcs->TextureTriangles = NULL;
               
               state->set = (accel & DFXL_TEXTRIANGLES) 
                            ? : (rdev->blitting_mask & ~DFXL_TEXTRIANGLES);
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }
     
     state->modified = 0;
}


/* primary screen hook */

static DFBResult
crtc1WaitVSync( CoreScreen *screen,
                void       *driver_data,
                void       *screen_data )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) driver_data;
     volatile __u8    *mmio = rdrv->mmio_base; 
     int               i;
     
     if (dfb_config->pollvsync_none)
          return DFB_OK;
          
     radeon_out32( mmio, GEN_INT_STATUS, VSYNC_INT_AK );
     
     for (i = 0; i < 2000000; i++) {
          struct timespec t = { 0, 0 };     
          
          if (radeon_in32( mmio, GEN_INT_STATUS ) & VSYNC_INT)
               break;
          nanosleep( &t, NULL );
     }

     return DFB_OK;
}

ScreenFuncs RadeonPrimaryScreenFuncs = {
     .WaitVSync = crtc1WaitVSync
};

ScreenFuncs  OldPrimaryScreenFuncs;
void        *OldPrimaryScreenDriverData;


/* chipset detection */

static int 
radeon_find_chipset( RadeonDriverData *rdrv, int *ret_devid, int *ret_index )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     unsigned int   vendor_id;
     unsigned int   device_id;
     int            i;

     vendor_id = radeon_in16( mmio, CONFIG_VENDOR_ID );
     device_id = radeon_in16( mmio, CONFIG_DEVICE_ID );
     if (vendor_id != 0x1002 || !device_id)
          dfb_system_get_deviceid( &vendor_id, &device_id );
        
     if (vendor_id == 0x1002) {
          if (ret_devid)
               *ret_devid = device_id;        
          
          for (i = 0; i < sizeof(dev_table)/sizeof(dev_table[0]); i++) {
               if ((unsigned int)dev_table[i].id == device_id) {
                    if (ret_index)
                         *ret_index = i;
                    return 1;
               }
          }
     }
     
     return 0;
}

/* exported symbols */

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_ATI_RADEON:
               return 1;
          default:
               break;
     }

     return 0;
}

static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "ATI Radeon Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Claudio Ciccani" );

     snprintf( info->license,
               DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH,
               "LGPL" );

     snprintf( info->url,
               DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH,
               "http://www.directfb.org" );

     info->version.major = 1;
     info->version.minor = 0;

     info->driver_data_size = sizeof(RadeonDriverData);
     info->device_data_size = sizeof(RadeonDeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     RadeonDriverData    *rdrv = (RadeonDriverData*) driver_data;
     RadeonChipsetFamily  chip = CHIP_UNKNOWN;
     int                  idx;
     
     rdrv->device_data = (RadeonDeviceData*) device_data;
     
     /* gain access to memory mapped registers */
     rdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!rdrv->mmio_base)
          return DFB_IO;
     
     rdrv->fb_base = dfb_gfxcard_memory_virtual( device, 0 );

     if (radeon_find_chipset( rdrv, NULL, &idx ))
          chip = dev_table[idx].chip;

     /* fill function table */
     funcs->AfterSetVar       = radeonAfterSetVar;
     funcs->EngineReset       = radeonEngineReset;
     funcs->EngineSync        = radeonEngineSync;
     funcs->FlushTextureCache = radeonFlushTextureCache;
#ifdef WORDS_BIGENDIAN
     funcs->SurfaceEnter      = radeonSurfaceEnter;
     funcs->SurfaceLeave      = radeonSurfaceLeave;
#endif
     
     if (chip >= CHIP_R300) {
          funcs->CheckState   = r300CheckState;
          funcs->SetState     = r300SetState;
     }
     else if (chip >= CHIP_R200) {
          funcs->CheckState   = r200CheckState;
          funcs->SetState     = r200SetState;
     }
     else if (chip >= CHIP_R100) {
          funcs->CheckState   = r100CheckState;
          funcs->SetState     = r100SetState;
     }
     
     /* primary screen */
     dfb_screens_hook_primary( device, driver_data, 
                               &RadeonPrimaryScreenFuncs,
                               &OldPrimaryScreenFuncs,
                               &OldPrimaryScreenDriverData ); 
     /* overlay support */
     dfb_layers_register( dfb_screens_at( DSCID_PRIMARY ),
                          driver_data, &RadeonOverlayFuncs );
                          
     if (chip != CHIP_R100) {
          CoreScreen *screen;

          /* secondary screen support */
          screen = dfb_screens_register( device, driver_data,
                                         &RadeonCrtc2ScreenFuncs );
     
          /* secondary underlay support */     
          dfb_layers_register( screen, driver_data,
                               &RadeonCrtc2LayerFuncs );
         
          /* secondary overlay support */
          dfb_layers_register( screen, driver_data,
                               &RadeonOverlayFuncs );
     }
     
     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) driver_data;
     RadeonDeviceData *rdev = (RadeonDeviceData*) device_data;
     volatile void    *mmio = rdrv->mmio_base;
     int               dev  = 0;
     int               idx  = 0;
     const char       *name = "Unknown";
     
     if (radeon_find_chipset( rdrv, &dev, &idx )) {
          rdev->chipset = dev_table[idx].chip;
          rdev->igp     = dev_table[idx].igp;
          name          = dev_table[idx].name;
     }
     else {
          if (!dev) {
               D_ERROR( "DirectFB/Radeon: Could not detect device id!\n"
                        "     -> Please, specify the bus location of"
                        "        the card by using the 'busid' option.\n" );
          }         
          D_INFO( "DirectFB/Radeon: "
                  "Unknown chipset, disabling acceleration!\n" );
     }
     
     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, 
               "%s (%04x)", name, dev );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "ATI" );

     device_info->caps.flags = CCF_CLIPPING | CCF_AUXMEMORY;
     
     if (rdev->chipset >= CHIP_R300) {
          device_info->caps.accel    = R300_SUPPORTED_DRAWINGFUNCTIONS |
                                       R300_SUPPORTED_BLITTINGFUNCTIONS;
          device_info->caps.drawing  = R300_SUPPORTED_DRAWINGFLAGS;
          device_info->caps.blitting = R300_SUPPORTED_BLITTINGFLAGS;
     }
     else if (rdev->chipset >= CHIP_R200) {
          device_info->caps.accel    = R200_SUPPORTED_DRAWINGFUNCTIONS |
                                       R200_SUPPORTED_BLITTINGFUNCTIONS;
          device_info->caps.drawing  = R200_SUPPORTED_DRAWINGFLAGS;
          device_info->caps.blitting = R200_SUPPORTED_BLITTINGFLAGS;
     }
     else if (rdev->chipset >= CHIP_R100) {
          device_info->caps.accel    = R100_SUPPORTED_DRAWINGFUNCTIONS |
                                       R100_SUPPORTED_BLITTINGFUNCTIONS;
          device_info->caps.drawing  = R100_SUPPORTED_DRAWINGFLAGS;
          device_info->caps.blitting = R100_SUPPORTED_BLITTINGFLAGS;
     }

     device_info->limits.surface_byteoffset_alignment = 32;
     device_info->limits.surface_pixelpitch_alignment = 32;
     device_info->limits.surface_bytepitch_alignment  = 128;

     dfb_config->pollvsync_after = 1;

     /* reserve memory for YUV422 color buffer */
     rdev->yuv422_buffer = dfb_gfxcard_reserve_memory( device, 128 );
     if (rdev->yuv422_buffer == (__u32)-1) {
          D_ERROR( "DirectFB/Radeon: "
                   "couldn't reserve 128 bytes of video memory!\n" );
          return DFB_NOVIDEOMEMORY;
     }

     radeon_waitidle( rdrv, rdev );
     
     /* get connected monitors */
     radeon_get_monitors( rdrv, rdev, &rdev->monitor1, &rdev->monitor2 );

     /* save the following regs */
     rdev->mc_fb_location = radeon_in32( mmio, MC_FB_LOCATION );
     rdev->mc_agp_location = radeon_in32( mmio, MC_AGP_LOCATION );  
     rdev->crtc_base_addr = radeon_in32( mmio, CRTC_BASE_ADDR );
     rdev->crtc2_base_addr = radeon_in32( mmio, CRTC2_BASE_ADDR );
     rdev->agp_base = radeon_in32( mmio, AGP_BASE );
     rdev->agp_cntl = radeon_in32( mmio, AGP_CNTL );
     rdev->aic_cntl = radeon_in32( mmio, AIC_CNTL );
     rdev->bus_cntl = radeon_in32( mmio, BUS_CNTL );
     rdev->fcp_cntl = radeon_in32( mmio, FCP_CNTL );
     rdev->cap0_trig_cntl = radeon_in32( mmio, CAP0_TRIG_CNTL );
     rdev->vid_buffer_control = radeon_in32( mmio, VID_BUFFER_CONTROL );
     rdev->display_test_debug_cntl = radeon_in32( mmio, DISPLAY_TEST_DEBUG_CNTL );
     rdev->surface_cntl = radeon_in32( mmio, SURFACE_CNTL ); 
     rdev->dp_gui_master_cntl = radeon_in32( mmio, DP_GUI_MASTER_CNTL );   
     
     rdev->surface_cntl_p = rdev->surface_cntl & ~(NONSURF_AP0_SWP_16BPP |
                                                   NONSURF_AP0_SWP_32BPP |
                                                   NONSURF_AP1_SWP_16BPP |
                                                   NONSURF_AP1_SWP_32BPP);
     rdev->surface_cntl_c = rdev->surface_cntl_p;
     
     if (rdev->igp) {
          __u32 tom;
          /* force MC_FB_LOCATION to NB_TOM */
          tom = radeon_in32( mmio, NB_TOM );
          rdev->fb_offset = tom << 16;
          rdev->fb_size   = ((tom >> 16) - (tom & 0xffff) + 1) << 16;
     } 
     else { 
          rdev->fb_offset = dfb_system_video_memory_physical( 0 );
          rdev->fb_size   = dfb_system_videoram_length();
     }
     
     radeon_out32( mmio, MC_FB_LOCATION, (rdev->fb_offset>>16) |
                        ((rdev->fb_offset + rdev->fb_size - 1) & 0xffff0000) ); 

     D_DEBUG( "DirectFB/Radeon: "
              "Framebuffer located at 0x%08x:0x%08x.\n",
              rdev->fb_offset, rdev->fb_offset + rdev->fb_size - 1 );
 
     if (dfb_system_auxram_length()) { 
          rdev->agp_offset = dfb_system_aux_memory_physical( 0 );
          rdev->agp_size   = dfb_system_auxram_length();
          
          /* enable AGP support */ 
          radeon_out32( mmio, AIC_CNTL, rdev->aic_cntl & ~PCIGART_TRANSLATE_EN );
          radeon_out32( mmio, AGP_BASE, rdev->agp_offset );
          radeon_out32( mmio, AGP_CNTL, rdev->agp_cntl | 0x000e0000 );
          radeon_out32( mmio, BUS_CNTL, rdev->bus_cntl & ~BUS_MASTER_DIS );

          radeon_out32( mmio, MC_AGP_LOCATION, (rdev->agp_offset>>16) |
                        ((rdev->agp_offset + rdev->agp_size - 1) & 0xffff0000) );
          
          D_DEBUG( "DirectFB/Radeon: "
                   "AGP Aperture located at 0x%08x:0x%08x.\n",
                   rdev->agp_offset, rdev->agp_offset + rdev->agp_size - 1 );
     }

     radeon_out32( mmio, CRTC_BASE_ADDR, rdev->fb_offset );
     radeon_out32( mmio, DISP_MERGE_CNTL, 0xffff0000 );
     if (rdev->chipset != CHIP_R100) {
          radeon_out32( mmio, CRTC2_BASE_ADDR, rdev->fb_offset );
          radeon_out32( mmio, DISP2_MERGE_CNTL, 0xffff0000 );
     }

     radeon_out32( mmio, FCP_CNTL, FCP0_SRC_GND );
     radeon_out32( mmio, CAP0_TRIG_CNTL, 0 );
     radeon_out32( mmio, VID_BUFFER_CONTROL, 0x00010001 );
     radeon_out32( mmio, DISPLAY_TEST_DEBUG_CNTL, 0 );
 
     radeon_reset( rdrv, rdev );
         
     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) driver_data;
     RadeonDeviceData *rdev = (RadeonDeviceData*) device_data;
     volatile __u8    *mmio = rdrv->mmio_base;
    
     D_DEBUG( "DirectFB/Radeon: FIFO Performance Monitoring:\n" );
     D_DEBUG( "DirectFB/Radeon:  %9d radeon_waitfifo calls\n",
              rdev->waitfifo_calls );
     D_DEBUG( "DirectFB/Radeon:  %9d register writes (radeon_waitfifo sum)\n",
              rdev->waitfifo_sum );
     D_DEBUG( "DirectFB/Radeon:  %9d FIFO wait cycles (depends on CPU)\n",
              rdev->fifo_waitcycles );
     D_DEBUG( "DirectFB/Radeon:  %9d IDLE wait cycles (depends on CPU)\n",
              rdev->idle_waitcycles );
     D_DEBUG( "DirectFB/Radeon:  %9d FIFO space cache hits(depends on CPU)\n",
              rdev->fifo_cache_hits );
     D_DEBUG( "DirectFB/Radeon: Conclusion:\n" );
     D_DEBUG( "DirectFB/Radeon:  Average register writes/radeon_waitfifo call:%.2f\n",
              rdev->waitfifo_sum / (float)rdev->waitfifo_calls );
     D_DEBUG( "DirectFB/Radeon:  Average wait cycles/radeon_waitfifo call: %.2f\n",
              rdev->fifo_waitcycles / (float)rdev->waitfifo_calls );
     D_DEBUG( "DirectFB/Radeon:  Average fifo space cache hits: %02d%%\n",
              (int)(100 * rdev->fifo_cache_hits / (float)rdev->waitfifo_calls) );

     radeon_reset( rdrv, rdev );

     /* restore previously saved regs */
     radeon_out32( mmio, MC_FB_LOCATION, rdev->mc_fb_location );
     radeon_out32( mmio, MC_AGP_LOCATION, rdev->mc_agp_location ); 
     radeon_out32( mmio, CRTC_BASE_ADDR, rdev->crtc_base_addr );
     radeon_out32( mmio, CRTC2_BASE_ADDR, rdev->crtc2_base_addr );
     radeon_out32( mmio, AGP_CNTL, rdev->agp_cntl );
     radeon_out32( mmio, AGP_BASE, rdev->agp_base );
     radeon_out32( mmio, AIC_CNTL, rdev->aic_cntl );
     radeon_out32( mmio, BUS_CNTL, rdev->bus_cntl );
     radeon_out32( mmio, FCP_CNTL, rdev->fcp_cntl );
     radeon_out32( mmio, CAP0_TRIG_CNTL, rdev->cap0_trig_cntl );
     radeon_out32( mmio, VID_BUFFER_CONTROL, rdev->vid_buffer_control );
     radeon_out32( mmio, DISPLAY_TEST_DEBUG_CNTL, rdev->display_test_debug_cntl );
     radeon_out32( mmio, SURFACE_CNTL, rdev->surface_cntl );
     
     radeon_waitfifo( rdrv, rdev, 3 );
     radeon_out32( mmio, SC_TOP_LEFT, 0 );
     radeon_out32( mmio, DEFAULT_SC_BOTTOM_RIGHT, DEFAULT_SC_RIGHT_MAX |
                                                  DEFAULT_SC_BOTTOM_MAX );
     radeon_out32( mmio, DP_GUI_MASTER_CNTL, rdev->dp_gui_master_cntl );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
    RadeonDriverData *rdrv = (RadeonDriverData*) driver_data;

    dfb_gfxcard_unmap_mmio( device, rdrv->mmio_base, -1 );
}

