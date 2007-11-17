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
#include <unistd.h>
#include <time.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/gfxcard.h>
#include <core/screens.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layer_control.h>
#include <core/layers_internal.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/system.h>

#include <misc/conf.h>

#include <gfx/convert.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/util.h>

#include "radeon.h"
#include "radeon_regs.h"
#include "radeon_mmio.h"


typedef struct {
     CoreLayerRegionConfig  config;
     CorePalette           *palette; 
     DFBColorAdjustment     adjustment;
     
     unsigned int           pll_max_freq;
     unsigned int           pll_min_freq;
     unsigned int           pll_ref_div;
     unsigned int           pll_ref_clk;

     struct {
          unsigned int      size;
          u8                r[256];
          u8                g[256];
          u8                b[256];
     } lut;

     struct { 
          u32 rCRTC2_GEN_CNTL;
          u32 rFP2_GEN_CNTL;
          u32 rDAC_CNTL2;
          u32 rTV_DAC_CNTL;
          u32 rDISP_OUTPUT_CNTL;
          u32 rDISP_HW_DEBUG;
          u32 rCRTC2_OFFSET_CNTL;
     } save;
     
     struct {
          u32 rCRTC2_GEN_CNTL;
          u32 rDAC_CNTL2;
          u32 rTV_DAC_CNTL;
          u32 rDISP_OUTPUT_CNTL;
          u32 rDISP_HW_DEBUG;
          u32 rCRTC2_H_TOTAL_DISP;
          u32 rCRTC2_H_SYNC_STRT_WID;
          u32 rCRTC2_V_TOTAL_DISP;
          u32 rCRTC2_V_SYNC_STRT_WID;
          u32 rCRTC2_BASE_ADDR;
          u32 rCRTC2_OFFSET;
          u32 rCRTC2_OFFSET_CNTL;
          u32 rCRTC2_PITCH;
          u32 rFP2_GEN_CNTL;
          u32 rFP2_H_SYNC_STRT_WID;
          u32 rFP2_V_SYNC_STRT_WID;
          u32 rP2PLL_REF_DIV;
          u32 rP2PLL_DIV_0;
          u32 rHTOTAL2_CNTL;
     } regs;
} RadeonCrtc2LayerData;

static VideoMode*  crtc2_find_mode    ( RadeonDriverData     *drv,
                                        int                   xres,
                                        int                   yres );
static bool        crtc2_calc_regs    ( RadeonDriverData      *rdrv, 
                                        RadeonCrtc2LayerData  *rcrtc2,
                                        CoreLayerRegionConfig *config,
                                        CoreSurface           *surface,
                                        CoreSurfaceBufferLock *lock );
static void        crtc2_set_regs     ( RadeonDriverData      *rdrv,
                                        RadeonCrtc2LayerData  *rcrtc2 );
static void        crtc2_calc_palette ( RadeonDriverData      *rdrv,
                                        RadeonCrtc2LayerData  *rcrtc2,
                                        CoreLayerRegionConfig *config,
                                        DFBColorAdjustment    *adjustment,
                                        CorePalette           *palette );
static void        crtc2_set_palette  ( RadeonDriverData      *rdrv,
                                        RadeonCrtc2LayerData  *rcrtc2 );

/*************************** CRTC2 Screen functions **************************/

static DFBResult
crtc2InitScreen( CoreScreen           *screen,
                 CoreGraphicsDevice   *device,
                 void                 *driver_data,
                 void                 *screen_data,
                 DFBScreenDescription *description )
{
     /* Set the screen capabilities. */
     description->caps = DSCCAPS_VSYNC | DSCCAPS_POWER_MANAGEMENT;

     /* Set the screen name. */
     snprintf( description->name,
               DFB_SCREEN_DESC_NAME_LENGTH, "Radeon CRTC2" );
               
     return DFB_OK;
}

static DFBResult
crtc2SetPowerMode( CoreScreen         *screen,
                   void               *driver_data,
                   void               *screen_data,
                   DFBScreenPowerMode  mode )
{
     RadeonDriverData *rdrv          = (RadeonDriverData*) driver_data;
     volatile u8      *mmio          = rdrv->mmio_base;
     u32               crtc2_gen_cntl;
     
     crtc2_gen_cntl  = radeon_in32( mmio, CRTC2_GEN_CNTL );
     crtc2_gen_cntl &= ~(CRTC2_HSYNC_DIS | CRTC2_VSYNC_DIS | CRTC2_DISP_DIS);
     
     switch (mode) {
          case DSPM_OFF:
               crtc2_gen_cntl |= CRTC2_HSYNC_DIS | 
                                 CRTC2_VSYNC_DIS | 
                                 CRTC2_DISP_DIS;
               break;
          case DSPM_SUSPEND:
               crtc2_gen_cntl |= CRTC2_VSYNC_DIS |
                                 CRTC2_DISP_DIS;
               break;
          case DSPM_STANDBY:
               crtc2_gen_cntl |= CRTC2_HSYNC_DIS |
                                 CRTC2_DISP_DIS;
               break;
          case DSPM_ON:
               break;
          default:
               D_DEBUG( "unknown power mode" );
               return DFB_INVARG;
     }
     
     radeon_out32( mmio, CRTC2_GEN_CNTL, crtc2_gen_cntl );

     return DFB_OK;
}

static DFBResult
crtc2WaitVSync( CoreScreen *screen,
                void       *driver_data,
                void       *screen_data )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) driver_data;
     volatile u8      *mmio = rdrv->mmio_base;
     int               i;
     
     if (dfb_config->pollvsync_none)
          return DFB_OK;
          
     radeon_out32( mmio, GEN_INT_STATUS, 
          (radeon_in32( mmio, GEN_INT_STATUS ) & ~VSYNC2_INT) | VSYNC2_INT_AK );
     
     for (i = 0; i < 2000000; i++) {
          struct timespec t = { 0, 10000 };     
          
          if (radeon_in32( mmio, GEN_INT_STATUS ) & VSYNC2_INT)
               break;
          nanosleep( &t, NULL );
     }

     return DFB_OK;
}

static DFBResult
crtc2GetScreenSize( CoreScreen *screen,
                    void       *driver_data,
                    void       *screen_data,
                    int        *ret_width,
                    int        *ret_height )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) driver_data;
     volatile u8      *mmio = rdrv->mmio_base;
     unsigned int      xres;
     unsigned int      yres;
    
     xres = ((radeon_in32( mmio, CRTC2_H_TOTAL_DISP ) >> 16) + 1) * 8;
     yres = ((radeon_in32( mmio, CRTC2_V_TOTAL_DISP ) >> 16) + 1);
     
     D_DEBUG( "DirectFB/Radeon/CRTC2: "
              "detected screen size %dx%d.\n", xres, yres );
     
     if (xres <= 1 || yres <= 1) {
          VideoMode *mode = dfb_system_modes();
          
          if (!mode) {
               D_WARN( "no default video mode" );
               return DFB_UNSUPPORTED;
          }
          xres = mode->xres;
          yres = mode->yres;
     }
     
     *ret_width  = xres;
     *ret_height = yres;
     
     return DFB_OK;
}

ScreenFuncs RadeonCrtc2ScreenFuncs = {
     .InitScreen    = crtc2InitScreen,
     .SetPowerMode  = crtc2SetPowerMode,
     .WaitVSync     = crtc2WaitVSync,
     .GetScreenSize = crtc2GetScreenSize
};
          
/**************************** CRTC2 Layer functions **************************/

#define CRTC2_SUPPORTED_OPTIONS ( DLOP_ALPHACHANNEL )

static int
crtc2LayerDataSize()
{
     return sizeof(RadeonCrtc2LayerData);
}

static DFBResult
crtc2InitLayer( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                DFBDisplayLayerDescription *description,
                DFBDisplayLayerConfig      *config,
                DFBColorAdjustment         *adjustment )
{
     RadeonDriverData     *rdrv   = (RadeonDriverData*) driver_data;
     RadeonCrtc2LayerData *rcrtc2 = (RadeonCrtc2LayerData*) layer_data;
     volatile u8          *mmio   = rdrv->mmio_base;
     VideoMode            *mode;
     
     mode = dfb_system_modes();
     if (!mode) {
          D_BUG( "no default video mode" );
          return DFB_FAILURE;
     }
     
     /* Fill layer description. */
     description->caps = DLCAPS_SURFACE     | DLCAPS_BRIGHTNESS |
                         DLCAPS_CONTRAST    | DLCAPS_SATURATION |
                         DLCAPS_ALPHACHANNEL;
     
     description->type = DLTF_GRAPHICS;
     
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Radeon CRTC2's Underlay" );
               
     /* Set default configuration. */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT     |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;
     config->width       = mode->xres;
     config->height      = mode->yres;
     config->pixelformat = DSPF_RGB16;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;
 
     /* Set default color adjustment. */ 
     adjustment->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST |
                              DCAF_SATURATION;
     adjustment->brightness = 0x8000;
     adjustment->contrast   = 0x8000;
     adjustment->saturation = 0x8000;
     
     /* Set PLL coefficients (should be done by reading the BIOS). */
     rcrtc2->pll_max_freq = 35000;
     rcrtc2->pll_min_freq = 12000;
     rcrtc2->pll_ref_div  = 60;
     rcrtc2->pll_ref_clk  = 2700;

     /* Save common registers. */
     rcrtc2->save.rCRTC2_GEN_CNTL = radeon_in32( mmio, CRTC2_GEN_CNTL );
     rcrtc2->save.rFP2_GEN_CNTL = radeon_in32( mmio, FP2_GEN_CNTL );
     rcrtc2->save.rDAC_CNTL2 = radeon_in32( mmio, DAC_CNTL2 );
     rcrtc2->save.rTV_DAC_CNTL = radeon_in32( mmio, TV_DAC_CNTL );
     rcrtc2->save.rDISP_OUTPUT_CNTL = radeon_in32( mmio, DISP_OUTPUT_CNTL );
     rcrtc2->save.rDISP_HW_DEBUG = radeon_in32( mmio, DISP_HW_DEBUG );
     rcrtc2->save.rCRTC2_OFFSET_CNTL = radeon_in32( mmio, CRTC2_OFFSET_CNTL );
     
     return DFB_OK;
}

static DFBResult
crtc2TestRegion( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 CoreLayerRegionConfig      *config,
                 CoreLayerRegionConfigFlags *failed )
{
     RadeonDriverData           *rdrv = (RadeonDriverData*) driver_data;
     CoreLayerRegionConfigFlags  fail = 0;

     /* check for unsupported options */
     if (config->options & ~CRTC2_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;
          
     if (config->options & DLOP_ALPHACHANNEL && config->format != DSPF_ARGB)
          fail |= CLRCF_OPTIONS;
          
     /* check for unsupported buffermode */
     switch (config->buffermode) {
          case DLBM_FRONTONLY:
          case DLBM_BACKSYSTEM:
          case DLBM_BACKVIDEO:
          case DLBM_TRIPLE:
               break;
               
          default:
               fail |= CLRCF_BUFFERMODE;
               break;
     }
     
     /* check for unsupported pixelformat */
     switch (config->format) {
          case DSPF_LUT8:
          case DSPF_RGB332:
          case DSPF_RGB555:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
               
          default:
               fail |= CLRCF_FORMAT;
               break;
     }
     
     /* check for unsupported size */
     if (!crtc2_find_mode( rdrv, config->width, config->height ))
          fail |= CLRCF_WIDTH | CLRCF_HEIGHT;
          
     if (failed)
          *failed = fail;
          
     return fail ? DFB_UNSUPPORTED : DFB_OK;
}

static DFBResult
crtc2AddRegion( CoreLayer             *layer,
                void                  *driver_data,
                void                  *layer_data,
                void                  *region_data,
                CoreLayerRegionConfig *config )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) driver_data;
     RadeonDeviceData *rdev = rdrv->device_data;

     if (!rdev->monitor2) {
          D_ERROR( "DirectFB/Radeon/CRTC2: "
                   "no secondary monitor connected!\n" );
          return DFB_IO;
     }
     
     return DFB_OK;
}

static DFBResult
crtc2SetRegion( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                void                       *region_data,
                CoreLayerRegionConfig      *config,
                CoreLayerRegionConfigFlags  updated,
                CoreSurface                *surface,
                CorePalette                *palette,
                CoreSurfaceBufferLock      *lock )
{
     RadeonDriverData     *rdrv   = (RadeonDriverData*) driver_data;
     RadeonCrtc2LayerData *rcrtc2 = (RadeonCrtc2LayerData*) layer_data;
    
     rcrtc2->config  = *config;
     rcrtc2->palette = palette;
    
     updated &= CLRCF_WIDTH  | CLRCF_HEIGHT  | 
                CLRCF_FORMAT | CLRCF_SURFACE | CLRCF_PALETTE;
                
     if (updated & ~CLRCF_PALETTE) {
          if (!crtc2_calc_regs( rdrv, rcrtc2, &rcrtc2->config, surface, lock ))
               return DFB_UNSUPPORTED;
          
          crtc2_set_regs( rdrv, rcrtc2 );
     }

     if (updated) {
          crtc2_calc_palette( rdrv, rcrtc2, &rcrtc2->config,
                              &rcrtc2->adjustment, rcrtc2->palette );
          crtc2_set_palette( rdrv, rcrtc2 );
     }
     
     return DFB_OK;
}

static DFBResult
crtc2RemoveRegion( CoreLayer *layer,
                   void      *driver_data,
                   void      *layer_data,
                   void      *region_data )
{
     RadeonDriverData     *rdrv   = (RadeonDriverData*) driver_data;
     RadeonCrtc2LayerData *rcrtc2 = (RadeonCrtc2LayerData*) layer_data;
     volatile u8          *mmio   = rdrv->mmio_base;
    
     radeon_waitidle( rdrv, rdrv->device_data );
    
     radeon_out32( mmio, CRTC2_GEN_CNTL, rcrtc2->save.rCRTC2_GEN_CNTL );
     radeon_out32( mmio, FP2_GEN_CNTL, rcrtc2->save.rFP2_GEN_CNTL );
     radeon_out32( mmio, DAC_CNTL2, rcrtc2->save.rDAC_CNTL2 );
     radeon_out32( mmio, TV_DAC_CNTL, rcrtc2->save.rTV_DAC_CNTL );
     radeon_out32( mmio, DISP_OUTPUT_CNTL, rcrtc2->save.rDISP_OUTPUT_CNTL );
     radeon_out32( mmio, DISP_HW_DEBUG, rcrtc2->save.rDISP_HW_DEBUG ); 
     radeon_out32( mmio, CRTC2_OFFSET_CNTL, rcrtc2->save.rCRTC2_OFFSET_CNTL );
     
     return DFB_OK;
}

static DFBResult
crtc2FlipRegion( CoreLayer             *layer,
                 void                  *driver_data,
                 void                  *layer_data,
                 void                  *region_data,
                 CoreSurface           *surface,
                 DFBSurfaceFlipFlags    flags,
                 CoreSurfaceBufferLock *lock )
{
     RadeonDriverData     *rdrv   = (RadeonDriverData*) driver_data;
     RadeonDeviceData     *rdev   = rdrv->device_data;
     RadeonCrtc2LayerData *rcrtc2 = (RadeonCrtc2LayerData*) layer_data;
     volatile u8          *mmio   = rdrv->mmio_base;
     
     if (lock->phys - lock->offset == rdev->fb_phys)
          rcrtc2->regs.rCRTC2_BASE_ADDR = rdev->fb_offset;
     else
          rcrtc2->regs.rCRTC2_BASE_ADDR = rdev->agp_offset;
     
     rcrtc2->regs.rCRTC2_OFFSET = lock->offset;
     
     radeon_waitidle( rdrv, rdrv->device_data );
     
     radeon_out32( mmio, CRTC2_BASE_ADDR, rcrtc2->regs.rCRTC2_BASE_ADDR );
     radeon_out32( mmio, CRTC2_OFFSET, rcrtc2->regs.rCRTC2_OFFSET );
     
     dfb_surface_flip( surface, false );
     
     if (flags & DSFLIP_WAIT)
          dfb_layer_wait_vsync( layer );
          
     return DFB_OK;
}

static DFBResult
crtc2SetColorAdjustment( CoreLayer          *layer,
                         void               *driver_data,
                         void               *layer_data,
                         DFBColorAdjustment *adj )
{
     RadeonDriverData     *rdrv   = (RadeonDriverData*) driver_data;
     RadeonCrtc2LayerData *rcrtc2 = (RadeonCrtc2LayerData*) layer_data;

     if (adj->flags & DCAF_BRIGHTNESS) {
          if (adj->brightness == 0x8000) {
               rcrtc2->adjustment.flags &= ~DCAF_BRIGHTNESS;
          } else {
               rcrtc2->adjustment.flags |= DCAF_BRIGHTNESS; 
               rcrtc2->adjustment.brightness = adj->brightness;
          }
     }
     if (adj->flags & DCAF_CONTRAST) {
          if (adj->contrast == 0x8000) {
               rcrtc2->adjustment.flags &= ~DCAF_CONTRAST;
          } else {
               rcrtc2->adjustment.flags |= DCAF_CONTRAST;
               rcrtc2->adjustment.contrast = adj->contrast;
          }
     }
     if (adj->flags & DCAF_SATURATION) {
          if (adj->saturation == 0x8000) {
               rcrtc2->adjustment.flags &= ~DCAF_SATURATION;
          } else {
               rcrtc2->adjustment.flags |= DCAF_SATURATION;
               rcrtc2->adjustment.saturation = adj->saturation;
          }
     }

     crtc2_calc_palette( rdrv, rcrtc2, &rcrtc2->config, 
                         &rcrtc2->adjustment, rcrtc2->palette );
     crtc2_set_palette( rdrv, rcrtc2 );

     return DFB_OK;
}

DisplayLayerFuncs RadeonCrtc2LayerFuncs = {
     .LayerDataSize      = crtc2LayerDataSize,
     .InitLayer          = crtc2InitLayer,
     .TestRegion         = crtc2TestRegion,
     .AddRegion          = crtc2AddRegion,
     .SetRegion          = crtc2SetRegion,
     .RemoveRegion       = crtc2RemoveRegion,
     .FlipRegion         = crtc2FlipRegion,
     .SetColorAdjustment = crtc2SetColorAdjustment
};

/************************** CRTC2 internal functions *************************/

static VideoMode* 
crtc2_find_mode( RadeonDriverData *rdrv,
                 int               xres,
                 int               yres )
{
     VideoMode *modes = dfb_system_modes();
     VideoMode *mode;
     
     for (mode = modes; mode; mode = mode->next) {
          if (mode->xres == xres && mode->yres == yres)
               return mode;
     }
     
     return NULL;
}

static void
crtc2_calc_pllregs( RadeonDriverData     *rdrv,
                    RadeonCrtc2LayerData *rcrtc2,
                    unsigned int          freq )
{ 
     struct {
          int divider;
          int bitvalue;
     } *post_div, post_divs[] = {
          {  1, 0 }, /* VCLK_SRC    */
          {  2, 1 }, /* VCLK_SRC/2  */
          {  4, 2 }, /* VCLK_SRC/4  */
          {  8, 3 }, /* VCLK_SRC/8  */
          {  3, 4 }, /* VCLK_SRC/3  */
          {  6, 6 }, /* VCLK_SRC/6  */
          { 12, 7 }, /* VCLK_SRC/12 */
          {  0, 0 }
     };
     u32 pll_output_freq_2 = 0;
     u32 feedback_div_2;

     if (freq > rcrtc2->pll_max_freq)
          freq = rcrtc2->pll_max_freq;
     if (freq*12 < rcrtc2->pll_min_freq)
          freq = rcrtc2->pll_min_freq/12;
          
     for (post_div = &post_divs[0]; post_div->divider; ++post_div) {
          pll_output_freq_2 = post_div->divider * freq;
          if (pll_output_freq_2 >= rcrtc2->pll_min_freq &&
              pll_output_freq_2 <= rcrtc2->pll_max_freq)
               break;
     }
     
     if (!post_div->divider) {
          pll_output_freq_2 = freq;
          post_div = &post_divs[0];
     }
    
     feedback_div_2  = rcrtc2->pll_ref_div * pll_output_freq_2;
     feedback_div_2 += rcrtc2->pll_ref_clk/2;
     feedback_div_2 /= rcrtc2->pll_ref_clk;
     
     D_DEBUG( "DirectFB/Radeon/CRTC2: "
              "DotCLock=%d OutputFreq=%d FeedbackDiv=%d PostDiv=%d.\n",
              freq, pll_output_freq_2, feedback_div_2, post_div->divider );

     rcrtc2->regs.rP2PLL_REF_DIV = rcrtc2->pll_ref_div;
     rcrtc2->regs.rP2PLL_DIV_0   = feedback_div_2 | (post_div->bitvalue << 16); 
     rcrtc2->regs.rHTOTAL2_CNTL  = 0;
}     

static bool 
crtc2_calc_regs( RadeonDriverData      *rdrv, 
                 RadeonCrtc2LayerData  *rcrtc2,
                 CoreLayerRegionConfig *config,
                 CoreSurface           *surface,
                 CoreSurfaceBufferLock *lock )
{
     RadeonDeviceData  *rdev   = rdrv->device_data;
     VideoMode         *mode; 
     u32                format = 0;
    
     int   h_total, h_sync_start, h_sync_end, h_sync_wid;
     int   v_total, v_sync_start, v_sync_end, v_sync_wid;
           
     
     mode = crtc2_find_mode( rdrv, config->width, config->height );
     if (!mode) {
          D_BUG( "unexpected error while searching video mode" );
          return false;
     }

     switch (config->format) {
          case DSPF_LUT8:
          case DSPF_RGB332:
               format = DST_8BPP;
               break;
          case DSPF_RGB555:
          case DSPF_ARGB1555:
               format = DST_15BPP;
               break;
          case DSPF_RGB16:
               format = DST_16BPP;
               break;
          case DSPF_RGB24:
               format = DST_24BPP;
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               format = DST_32BPP;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               return false;
     }
     
     h_sync_start = mode->xres + mode->right_margin;
     h_sync_end   = h_sync_start + mode->hsync_len;
     h_total      = h_sync_end + mode->left_margin;
     h_sync_wid   = (h_sync_end - h_sync_start) / 8;
     h_sync_wid   = CLAMP( h_sync_wid, 1, 0x3f );
     h_sync_start = h_sync_start - 8;
     
     v_sync_start = mode->yres + mode->lower_margin;
     v_sync_end   = v_sync_start + mode->vsync_len;
     v_total      = v_sync_end + mode->upper_margin;
     v_sync_wid   = v_sync_end - v_sync_start;
     v_sync_wid   = CLAMP( v_sync_wid, 1, 0x1f );
      
     D_DEBUG( "DirectFB/Radeon/CRTC2: \n"
              "\t\thSyncStart:%d hSyncEnd:%d hTotal:%d hSyncWid:%d\n"
              "\t\tvSyncStart:%d vSyncEnd:%d vTotal:%d vSyncWid:%d\n",
              h_sync_start, h_sync_end, h_total, h_sync_wid,
              v_sync_start, v_sync_end, v_total, v_sync_wid );
     
     rcrtc2->regs.rCRTC2_GEN_CNTL = CRTC2_EN | CRTC2_CRT2_ON | (format << 8);
     if (mode->laced)
          rcrtc2->regs.rCRTC2_GEN_CNTL |= CRTC2_INTERLACE_EN;
     if (mode->doubled)
          rcrtc2->regs.rCRTC2_GEN_CNTL |= CRTC2_DBL_SCAN_EN;
     if (mode->sync_on_green)
          rcrtc2->regs.rCRTC2_GEN_CNTL |= CRTC2_CSYNC_EN;
          
     rcrtc2->regs.rDAC_CNTL2 = rcrtc2->save.rDAC_CNTL2 | DAC2_DAC2_CLK_SEL;
     rcrtc2->regs.rTV_DAC_CNTL = 0x00280203;
     rcrtc2->regs.rDISP_OUTPUT_CNTL = rcrtc2->save.rDISP_OUTPUT_CNTL;
     rcrtc2->regs.rDISP_HW_DEBUG = rcrtc2->save.rDISP_HW_DEBUG;
     
     if (rdev->chipset == CHIP_UNKNOWN ||
         rdev->chipset == CHIP_R200    ||
         rdev->chipset >= CHIP_R300)
     {
          rcrtc2->regs.rDISP_OUTPUT_CNTL &= ~(DISP_DAC_SOURCE_MASK |
                                              DISP_DAC2_SOURCE_MASK);
          
          /* If primary monitor is a TV monitor, 
           * reverse the DAC source to control it using the CRTC2. */
          if (rdev->monitor1 == MT_CTV || rdev->monitor1 == MT_STV)
               rcrtc2->regs.rDISP_OUTPUT_CNTL |= DISP_DAC2_SOURCE_CRTC2;
          else
               rcrtc2->regs.rDISP_OUTPUT_CNTL |= DISP_DAC_SOURCE_CRTC2;
     }
     else {
          if (rdev->monitor1 == MT_CTV || rdev->monitor1 == MT_STV) {
               rcrtc2->regs.rDISP_HW_DEBUG &= ~CRT2_DISP1_SEL;
               rcrtc2->regs.rDAC_CNTL2     &= ~DAC2_DAC_CLK_SEL;
          }
          else {        
               rcrtc2->regs.rDISP_HW_DEBUG |= CRT2_DISP1_SEL;
               rcrtc2->regs.rDAC_CNTL2     |= DAC2_DAC_CLK_SEL;
          }
     }
     
     rcrtc2->regs.rCRTC2_H_TOTAL_DISP = ((h_total/8 - 1) & 0x3ff) | 
                                        ((mode->xres/8 - 1) << 16);
     rcrtc2->regs.rCRTC2_H_SYNC_STRT_WID = (h_sync_start & 0x1fff) |
                                           ((h_sync_wid & 0x3f) << 16);
     if (!mode->hsync_high)
          rcrtc2->regs.rCRTC2_H_SYNC_STRT_WID |= CRTC2_H_SYNC_POL;
          
     rcrtc2->regs.rCRTC2_V_TOTAL_DISP = ((v_total - 1) & 0xffff) |
                                        ((mode->yres - 1) << 16);
     rcrtc2->regs.rCRTC2_V_SYNC_STRT_WID = ((v_sync_start - 1) & 0xfff) |
                                           ((v_sync_wid & 0x1f) << 16);
     if (!mode->vsync_high)
          rcrtc2->regs.rCRTC2_V_SYNC_STRT_WID |= CRTC2_V_SYNC_POL;
          
     if (lock->phys - lock->offset == rdev->fb_phys)
          rcrtc2->regs.rCRTC2_BASE_ADDR = rdev->fb_offset;
     else
          rcrtc2->regs.rCRTC2_BASE_ADDR = rdev->agp_offset;
     
     rcrtc2->regs.rCRTC2_OFFSET = lock->offset;
     
     rcrtc2->regs.rCRTC2_OFFSET_CNTL = rcrtc2->save.rCRTC2_OFFSET_CNTL;
     rcrtc2->regs.rCRTC2_OFFSET_CNTL &= ~CRTC_TILE_EN;
     rcrtc2->regs.rCRTC2_OFFSET_CNTL |= CRTC_HSYNC_EN;
     
     rcrtc2->regs.rCRTC2_PITCH  = (lock->pitch / 
                                   DFB_BYTES_PER_PIXEL(surface->config.format)) >> 3;
     rcrtc2->regs.rCRTC2_PITCH |= rcrtc2->regs.rCRTC2_PITCH << 16;

     if (rdev->monitor2 == MT_DFP) {
          rcrtc2->regs.rCRTC2_GEN_CNTL &= ~CRTC2_CRT2_ON;
          rcrtc2->regs.rFP2_GEN_CNTL    = rcrtc2->save.rFP2_GEN_CNTL | FP2_ON;
           
          if (rdev->chipset == CHIP_UNKNOWN ||
              rdev->chipset == CHIP_R200    ||
              rdev->chipset >= CHIP_R300)
          {
               rcrtc2->regs.rFP2_GEN_CNTL &= ~(R200_FP2_SOURCE_SEL_MASK |
                                               FP2_DVO_RATE_SEL_SDR);
               rcrtc2->regs.rFP2_GEN_CNTL |= R200_FP2_SOURCE_SEL_CRTC2 | FP2_DVO_EN;
          }
          else {
               rcrtc2->regs.rFP2_GEN_CNTL &= ~FP2_SRC_SEL_MASK;
               rcrtc2->regs.rFP2_GEN_CNTL |= FP2_SRC_SEL_CRTC2;
          }

          rcrtc2->regs.rFP2_H_SYNC_STRT_WID = rcrtc2->regs.rCRTC2_H_SYNC_STRT_WID;
          rcrtc2->regs.rFP2_V_SYNC_STRT_WID = rcrtc2->regs.rCRTC2_V_SYNC_STRT_WID;
     }
     else {
          rcrtc2->regs.rFP2_GEN_CNTL = rcrtc2->save.rFP2_GEN_CNTL;
          rcrtc2->regs.rFP2_H_SYNC_STRT_WID = 0;
          rcrtc2->regs.rFP2_V_SYNC_STRT_WID = 0;
     }

     crtc2_calc_pllregs( rdrv, rcrtc2, 100000000 / mode->pixclock );
 
     return true;
}

static void
crtc2_set_regs ( RadeonDriverData     *rdrv,
                 RadeonCrtc2LayerData *rcrtc2 )
{
     volatile u8 *mmio = rdrv->mmio_base;
     u32          tmp;
     
     /* Lock the card during mode switching. */
     dfb_gfxcard_lock( GDLF_WAIT | GDLF_SYNC );
     
     radeon_out32( mmio, CRTC2_GEN_CNTL, 
                         rcrtc2->regs.rCRTC2_GEN_CNTL | CRTC2_DISP_DIS );
   
     radeon_out32( mmio, DAC_CNTL2, rcrtc2->regs.rDAC_CNTL2 );
     radeon_out32( mmio, TV_DAC_CNTL, rcrtc2->regs.rTV_DAC_CNTL );
     radeon_out32( mmio, DISP_OUTPUT_CNTL, rcrtc2->regs.rDISP_OUTPUT_CNTL );
     radeon_out32( mmio, DISP_HW_DEBUG, rcrtc2->regs.rDISP_HW_DEBUG );
     
     radeon_out32( mmio, CRTC2_H_TOTAL_DISP, rcrtc2->regs.rCRTC2_H_TOTAL_DISP );
     radeon_out32( mmio, CRTC2_H_SYNC_STRT_WID, rcrtc2->regs.rCRTC2_H_SYNC_STRT_WID );
     
     radeon_out32( mmio, CRTC2_V_TOTAL_DISP, rcrtc2->regs.rCRTC2_V_TOTAL_DISP );
     radeon_out32( mmio, CRTC2_V_SYNC_STRT_WID, rcrtc2->regs.rCRTC2_V_SYNC_STRT_WID );
    
     radeon_out32( mmio, CRTC2_BASE_ADDR, rcrtc2->regs.rCRTC2_BASE_ADDR );
     radeon_out32( mmio, CRTC2_OFFSET, rcrtc2->regs.rCRTC2_OFFSET );
     radeon_out32( mmio, CRTC2_OFFSET_CNTL, rcrtc2->regs.rCRTC2_OFFSET_CNTL );
     radeon_out32( mmio, CRTC2_PITCH, rcrtc2->regs.rCRTC2_PITCH );                      

     radeon_out32( mmio, FP2_GEN_CNTL, rcrtc2->regs.rFP2_GEN_CNTL );
     radeon_out32( mmio, FP2_H_SYNC_STRT_WID, rcrtc2->regs.rFP2_H_SYNC_STRT_WID );
     radeon_out32( mmio, FP2_V_SYNC_STRT_WID, rcrtc2->regs.rFP2_V_SYNC_STRT_WID );

     tmp = radeon_inpll( mmio, PIXCLKS_CNTL) & ~PIX2CLK_SRC_SEL_MASK;
     radeon_outpll( mmio, PIXCLKS_CNTL, tmp | PIX2CLK_SRC_SEL_CPUCLK );
     
     tmp = radeon_inpll( mmio, P2PLL_CNTL );
     radeon_outpll( mmio, P2PLL_CNTL, tmp | P2PLL_RESET |
                                      P2PLL_ATOMIC_UPDATE_EN |
                                      P2PLL_VGA_ATOMIC_UPDATE_EN );
     
     tmp = radeon_inpll( mmio, P2PLL_REF_DIV ) & ~P2PLL_REF_DIV_MASK;
     radeon_outpll( mmio, P2PLL_REF_DIV, tmp | rcrtc2->regs.rP2PLL_REF_DIV );
     
     tmp = radeon_inpll( mmio, P2PLL_DIV_0 ) & ~P2PLL_FB0_DIV_MASK; 
     radeon_outpll( mmio, P2PLL_DIV_0, tmp | rcrtc2->regs.rP2PLL_DIV_0 );
     
     tmp = radeon_inpll( mmio, P2PLL_DIV_0 ) & ~P2PLL_POST0_DIV_MASK;     
     radeon_outpll( mmio, P2PLL_DIV_0, tmp | rcrtc2->regs.rP2PLL_DIV_0 );
           
     while (radeon_inpll( mmio, P2PLL_REF_DIV ) & P2PLL_ATOMIC_UPDATE_R);
     
     radeon_outpll( mmio, P2PLL_REF_DIV,
                    radeon_inpll( mmio, P2PLL_REF_DIV ) | P2PLL_ATOMIC_UPDATE_W );
     
     for (tmp = 0; tmp < 1000; tmp++) {
          if (!(radeon_inpll( mmio, P2PLL_REF_DIV ) & P2PLL_ATOMIC_UPDATE_R))
               break;
     }
     
     radeon_outpll( mmio, HTOTAL2_CNTL, rcrtc2->regs.rHTOTAL2_CNTL );
     
     tmp = radeon_inpll( mmio, P2PLL_CNTL );
     radeon_outpll( mmio, P2PLL_CNTL, tmp & ~(P2PLL_RESET | P2PLL_SLEEP |
                                              P2PLL_ATOMIC_UPDATE_EN |
                                              P2PLL_VGA_ATOMIC_UPDATE_EN) );
                                           
     usleep( 5000 );
     
     tmp = radeon_inpll( mmio, PIXCLKS_CNTL ) & ~PIX2CLK_SRC_SEL_MASK;
     radeon_outpll( mmio, PIXCLKS_CNTL, tmp | PIX2CLK_SRC_SEL_P2PLLCLK );
     
     radeon_out32( mmio, CRTC2_GEN_CNTL, rcrtc2->regs.rCRTC2_GEN_CNTL );

     dfb_gfxcard_unlock();
}

static inline u8
calc_gamma( float n, float d )
{
     int ret;
     
     ret = 255.0 * n / d + 0.5;
     if (ret > 255)
          ret = 255;
     else if (ret < 0)
          ret = 0;

     return ret;
}

static void
crtc2_calc_palette( RadeonDriverData      *rdrv,
                    RadeonCrtc2LayerData  *rcrtc2,
                    CoreLayerRegionConfig *config,
                    DFBColorAdjustment    *adjustment,
                    CorePalette           *palette )
{
     unsigned int i;
     int          r, g, b;
     
     switch (config->format) {
          case DSPF_LUT8:
               rcrtc2->lut.size = MAX( palette->num_entries, 256 );
               for (i = 0; i < rcrtc2->lut.size; i++) {
                    rcrtc2->lut.r[i] = palette->entries[i].r;
                    rcrtc2->lut.g[i] = palette->entries[i].g;
                    rcrtc2->lut.b[i] = palette->entries[i].b;
               }
               break;
          case DSPF_RGB332:
               rcrtc2->lut.size = 256;
               for (i = 0, r = 0; r < 8; r++) {
                    for (g = 0; g < 8; g++) {
                         for (b = 0; b < 4; b++) {
                              rcrtc2->lut.r[i] = calc_gamma( r, 7 );
                              rcrtc2->lut.g[i] = calc_gamma( g, 7 );
                              rcrtc2->lut.b[i] = calc_gamma( b, 3 );
                              i++;
                         }
                    }
               }
               break;
          case DSPF_RGB555:
          case DSPF_ARGB1555:
               rcrtc2->lut.size = 32;
               for (i = 0; i < 32; i++) {
                    rcrtc2->lut.r[i] =
                    rcrtc2->lut.g[i] =
                    rcrtc2->lut.b[i] = calc_gamma( i, 31 );
               }
               break;
          case DSPF_RGB16:
               rcrtc2->lut.size = 64;
               for (i = 0; i < 64; i++) {
                    rcrtc2->lut.r[i] =
                    rcrtc2->lut.b[i] = calc_gamma( i/2, 31 );
                    rcrtc2->lut.g[i] = calc_gamma( i, 63 );
               }
               break;
          default:
               rcrtc2->lut.size = 256;
               for (i = 0; i < 256; i++) {
                    rcrtc2->lut.r[i] =
                    rcrtc2->lut.b[i] =
                    rcrtc2->lut.g[i] = i;
               }
               break;
     }

     if (adjustment->flags & DCAF_BRIGHTNESS) {
          int brightness = (adjustment->brightness >> 8) - 128;

          for (i = 0; i < rcrtc2->lut.size; i++) {
               r = rcrtc2->lut.r[i] + brightness;
               g = rcrtc2->lut.g[i] + brightness;
               b = rcrtc2->lut.b[i] + brightness;
               rcrtc2->lut.r[i] = CLAMP( r, 0, 255 );
               rcrtc2->lut.g[i] = CLAMP( g, 0, 255 );
               rcrtc2->lut.b[i] = CLAMP( b, 0, 255 );
          }
     }

     if (adjustment->flags & DCAF_CONTRAST) {
          int contrast = adjustment->contrast;

          for (i = 0; i < rcrtc2->lut.size; i++) {
               r = rcrtc2->lut.r[i] * contrast / 0x8000;
               g = rcrtc2->lut.g[i] * contrast / 0x8000;
               b = rcrtc2->lut.b[i] * contrast / 0x8000;
               rcrtc2->lut.r[i] = CLAMP( r, 0, 255 );
               rcrtc2->lut.g[i] = CLAMP( g, 0, 255 );
               rcrtc2->lut.b[i] = CLAMP( b, 0, 255 );
          }
     }

     if (adjustment->flags & DCAF_SATURATION) {
          int saturation = adjustment->saturation >> 8;

          for (i = 0; i < rcrtc2->lut.size; i++) {
               if (saturation > 128) {
                    float gray  = ((float)saturation - 128.0)/128.0;
                    float color = 1.0 - gray;

                    r = (((float)rcrtc2->lut.r[i] - 128.0 * gray)/color);
                    g = (((float)rcrtc2->lut.g[i] - 128.0 * gray)/color);
                    b = (((float)rcrtc2->lut.b[i] - 128.0 * gray)/color);
               }
               else {
                    float color = (float)saturation/128.0;
                    float gray  = 1.0 - color;

                    r = (((float)rcrtc2->lut.r[i] * color) + (128.0 * gray));
                    g = (((float)rcrtc2->lut.g[i] * color) + (128.0 * gray));
                    b = (((float)rcrtc2->lut.b[i] * color) + (128.0 * gray));
               }
               rcrtc2->lut.r[i] = CLAMP( r, 0, 255 );
               rcrtc2->lut.g[i] = CLAMP( g, 0, 255 );
               rcrtc2->lut.b[i] = CLAMP( b, 0, 255 );
          }
     }
}
    
static void
crtc2_set_palette( RadeonDriverData     *rdrv,
                   RadeonCrtc2LayerData *rcrtc2 )
{
     volatile u8 *mmio = rdrv->mmio_base;
     u32          tmp;
     int          i, j;

     if (!rcrtc2->lut.size) {
          D_WARN( "palette is empty" );
          return;
     }

     dfb_gfxcard_lock( GDLF_WAIT | GDLF_SYNC );

     tmp = radeon_in32( mmio, DAC_CNTL2 );
     radeon_out32( mmio, DAC_CNTL2, tmp | DAC2_PALETTE_ACC_CTL );

     j = 256 / rcrtc2->lut.size;
     for (i = 0; i < rcrtc2->lut.size; i++) {
          radeon_out32( mmio, PALETTE_INDEX, i*j );
          radeon_out32( mmio, PALETTE_DATA, (rcrtc2->lut.b[i]      ) |
                                            (rcrtc2->lut.g[i] <<  8) |
                                            (rcrtc2->lut.r[i] << 16) );
     }

     radeon_out32( mmio, DAC_CNTL2, tmp );

     dfb_gfxcard_unlock();
}

