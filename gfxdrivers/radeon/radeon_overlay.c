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
#include <math.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/screen.h>
#include <core/screens.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layer_control.h>
#include <core/layers_internal.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/util.h>

#include "radeon.h"
#include "radeon_regs.h"
#include "radeon_mmio.h"


typedef struct {
     CoreLayerRegionConfig  config;
     float                  brightness;
     float                  contrast;
     float                  saturation;
     float                  hue;
     int                    field;
     int                    level;
   
     CoreScreen            *screen;
     int                    crtc2;

     CoreSurface           *surface;
     
     /* overlay registers */
     struct {
          __u32 H_INC;
          __u32 STEP_BY;
          __u32 Y_X_START;
          __u32 Y_X_END;
          __u32 V_INC;
          __u32 P1_BLANK_LINES_AT_TOP;
          __u32 P23_BLANK_LINES_AT_TOP;
          __u32 VID_BUF_PITCH0_VALUE;
          __u32 VID_BUF_PITCH1_VALUE;
          __u32 P1_X_START_END;
          __u32 P2_X_START_END;
          __u32 P3_X_START_END;
          __u32 BASE_ADDR;
          __u32 VID_BUF0_BASE_ADRS;
          __u32 VID_BUF1_BASE_ADRS;
          __u32 VID_BUF2_BASE_ADRS;
          __u32 VID_BUF3_BASE_ADRS;
          __u32 VID_BUF4_BASE_ADRS;
          __u32 VID_BUF5_BASE_ADRS;
          __u32 P1_V_ACCUM_INIT;
          __u32 P23_V_ACCUM_INIT;
          __u32 P1_H_ACCUM_INIT;
          __u32 P23_H_ACCUM_INIT;
          __u32 VID_KEY_CLR_LOW;
          __u32 VID_KEY_CLR_HIGH;
          __u32 GRPH_KEY_CLR_LOW;
          __u32 GRPH_KEY_CLR_HIGH;
          __u32 KEY_CNTL;
          __u32 MERGE_CNTL;
          __u32 SCALE_CNTL;
     } regs;
} RadeonOverlayLayerData;

static void ovl_calc_regs     ( RadeonDriverData        *rdrv,
                                RadeonOverlayLayerData  *rovl,
                                CoreSurface             *surface,
                                CoreLayerRegionConfig   *config );
static void ovl_set_regs      ( RadeonDriverData        *rdrv,
                                RadeonOverlayLayerData  *rovl );                            
static void ovl_calc_buffers  ( RadeonDriverData        *rdrv,
                                RadeonOverlayLayerData  *rovl,
                                CoreSurface             *surface,
                                CoreLayerRegionConfig   *config );
static void ovl_set_buffers   ( RadeonDriverData        *rdrv,
                                RadeonOverlayLayerData  *rovl );
static void ovl_set_colorkey  ( RadeonDriverData        *rdrv,
                                RadeonOverlayLayerData  *rovl,
                                CoreLayerRegionConfig   *config );
static void ovl_set_adjustment( RadeonDriverData        *rdrv,
                                RadeonOverlayLayerData  *rovl,
                                float                    brightness,
                                float                    contrast,
                                float                    saturation,
                                float                    hue );
                
#define OVL_SUPPORTED_OPTIONS \
     ( DLOP_DST_COLORKEY | DLOP_OPACITY | DLOP_DEINTERLACING )

/**********************/

static int
ovlLayerDataSize()
{
     return sizeof(RadeonOverlayLayerData);
}

static DFBResult
ovlInitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
     RadeonDriverData       *rdrv = (RadeonDriverData*) driver_data;
     RadeonOverlayLayerData *rovl = (RadeonOverlayLayerData*) layer_data;
     volatile __u8          *mmio = rdrv->mmio_base;
     DFBScreenDescription    dsc;
     
     dfb_screen_get_info( layer->screen, NULL, &dsc );
     if (strstr( dsc.name, "CRTC2" ))
          rovl->crtc2 = 1;
          
     rovl->level = 1;
     
     /* fill layer description */
     description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;
     description->caps = DLCAPS_SURFACE       | DLCAPS_SCREEN_LOCATION |
                         DLCAPS_BRIGHTNESS    | DLCAPS_CONTRAST        |
                         DLCAPS_SATURATION    | DLCAPS_HUE             |
                         DLCAPS_DST_COLORKEY  | DLCAPS_OPACITY         |
                         DLCAPS_DEINTERLACING | DLCAPS_LEVELS;

     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, 
               "Radeon CRTC%c's Overlay", rovl->crtc2 ? '2' : '1' );

     /* set default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT     |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;
     config->width       = 640;
     config->height      = 480; 
     config->pixelformat = DSPF_YUY2;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;
     
     /* set default color adjustment */
     adjustment->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST |
                              DCAF_SATURATION | DCAF_HUE;
     adjustment->brightness = 0x8000;
     adjustment->contrast   = 0x8000;
     adjustment->saturation = 0x8000;
     adjustment->hue        = 0x8000;

     /* reset overlay */
     radeon_out32( mmio, OV0_SCALE_CNTL, SCALER_SOFT_RESET ); 
     radeon_out32( mmio, OV0_AUTO_FLIP_CNTL, 0 );
     radeon_out32( mmio, OV0_DEINTERLACE_PATTERN, 0 );
     radeon_out32( mmio, OV0_EXCLUSIVE_HORZ, 0 ); 
     radeon_out32( mmio, OV0_FILTER_CNTL, FILTER_HARDCODED_COEF );
     radeon_out32( mmio, OV0_TEST, 0 );
     
     /* reset color adjustments */
     ovl_set_adjustment( rdrv, rovl, 0, 0, 0, 0 );

     return DFB_OK;
}

static DFBResult
ovlTestRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags *failed )
{
     RadeonOverlayLayerData     *rovl = (RadeonOverlayLayerData*) layer_data;
     CoreLayerRegionConfigFlags  fail = 0;

     /* check for unsupported options */
     if (config->options & ~OVL_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     if (rovl->level == -1) {
          if (config->options & ~DLOP_DEINTERLACING)
               fail |= CLRCF_OPTIONS;
     }
     else {
          if (config->options &  DLOP_OPACITY &&
              config->options & (DLOP_SRC_COLORKEY | DLOP_DST_COLORKEY))
               fail |= CLRCF_OPTIONS;
     }

     /* check buffermode */
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
     
     /* check pixel format */
     switch (config->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               break;

          default:
               fail |= CLRCF_FORMAT;
               break;
     }

     /* check width */
     if (config->width > 2048 || config->width < 1)
          fail |= CLRCF_WIDTH;

     /* check height */
     if (config->height > 2048 || config->height < 1)
          fail |= CLRCF_HEIGHT;

     /* write back failing fields */
     if (failed)
          *failed = fail;

     /* return failure if any field failed */
     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
ovlAddRegion( CoreLayer             *layer,
              void                  *driver_data,
              void                  *layer_data,
              void                  *region_data,
              CoreLayerRegionConfig *config )
{
     RadeonDriverData       *rdrv = (RadeonDriverData*) driver_data;
     RadeonDeviceData       *rdev = rdrv->device_data;
     RadeonOverlayLayerData *rovl = (RadeonOverlayLayerData*) layer_data;
     
     if (rovl->crtc2 && !rdev->monitor2) {
          D_ERROR( "DirectFB/Radeon/Overlay: "
                   "no secondary monitor connected!\n" );
          return DFB_IO;
     }
     
     return DFB_OK;
}

static DFBResult
ovlSetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags  updated,
              CoreSurface                *surface,
              CorePalette                *palette )
{
     RadeonDriverData       *rdrv = (RadeonDriverData*) driver_data;
     RadeonOverlayLayerData *rovl = (RadeonOverlayLayerData*) layer_data;

     /* save configuration */
     rovl->config = *config;
     rovl->surface = surface;
     rovl->screen = layer->screen;
     
     if (updated & (CLRCF_WIDTH  | CLRCF_HEIGHT | CLRCF_FORMAT  |
                    CLRCF_SOURCE | CLRCF_DEST   | CLRCF_OPTIONS | CLRCF_OPACITY)) 
     {
          ovl_calc_regs( rdrv, rovl, surface, &rovl->config );
          ovl_set_regs( rdrv, rovl );
     }
     
     if (updated & (CLRCF_SRCKEY | CLRCF_DSTKEY))
          ovl_set_colorkey( rdrv, rovl, &rovl->config );

     return DFB_OK;
}

static DFBResult
ovlFlipRegion( CoreLayer           *layer,
               void                *driver_data,
               void                *layer_data,
               void                *region_data,
               CoreSurface         *surface,
               DFBSurfaceFlipFlags  flags )
{
     RadeonDriverData       *rdrv = (RadeonDriverData*) driver_data;
     RadeonOverlayLayerData *rovl = (RadeonOverlayLayerData*) layer_data;

     dfb_surface_flip_buffers( surface, false );
      
     ovl_calc_buffers( rdrv, rovl, surface, &rovl->config );
     ovl_set_buffers( rdrv, rovl );
   
     if (flags & DSFLIP_WAIT)
          dfb_layer_wait_vsync( layer );

     return DFB_OK;
}

static DFBResult
ovlSetColorAdjustment( CoreLayer          *layer,
                       void               *driver_data,
                       void               *layer_data,
                       DFBColorAdjustment *adj )
{
     RadeonDriverData       *rdrv = (RadeonDriverData*) driver_data;
     RadeonOverlayLayerData *rovl = (RadeonOverlayLayerData*) layer_data;

     if (adj->flags & DCAF_BRIGHTNESS)
          rovl->brightness = (float)(adj->brightness-0x8000) / 65535.0;

     if (adj->flags & DCAF_CONTRAST)
          rovl->contrast   = (float)adj->contrast / 32768.0;

     if (adj->flags & DCAF_SATURATION)
          rovl->saturation = (float)adj->saturation / 32768.0;

     if (adj->flags & DCAF_HUE)
          rovl->hue        = (float)(adj->hue-0x8000) * 3.1416 / 65535.0;

     ovl_set_adjustment( rdrv, rovl, rovl->brightness, rovl->contrast,
                                     rovl->saturation, rovl->hue );

     return DFB_OK;
}

static DFBResult
ovlSetInputField( CoreLayer *layer,
                  void      *driver_data,
                  void      *layer_data,
                  void      *region_data,
                  int        field )
{
     RadeonDriverData       *rdrv = (RadeonDriverData*) driver_data;
     RadeonOverlayLayerData *rovl = (RadeonOverlayLayerData*) layer_data;

     rovl->field = field;
             
     if (rovl->surface) {
          ovl_calc_buffers( rdrv, rovl, rovl->surface, &rovl->config );
          ovl_set_buffers( rdrv, rovl );
     }
     
     return DFB_OK;
}

static DFBResult
ovlGetLevel( CoreLayer *layer,
             void      *driver_data,
             void      *layer_data,
             int       *level )
{
     RadeonOverlayLayerData *rovl = (RadeonOverlayLayerData*) layer_data;
     
     *level = rovl->level;
     
     return DFB_OK;
}

static DFBResult
ovlSetLevel( CoreLayer *layer,
             void      *driver_data,
             void      *layer_data,
             int        level )
{
     RadeonDriverData       *rdrv = (RadeonDriverData*) driver_data;
     RadeonOverlayLayerData *rovl = (RadeonOverlayLayerData*) layer_data;
     
     if (!rovl->surface)
          return DFB_UNSUPPORTED;
     
     switch (level) {
          case -1:
          case  1:
               rovl->level = level;
               ovl_calc_regs( rdrv, rovl, rovl->surface, &rovl->config );
               ovl_set_regs( rdrv, rovl );
               break;
          default:
               return DFB_UNSUPPORTED;
     }
     
     return DFB_OK;
}

static DFBResult
ovlRemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
     RadeonDriverData *rdrv = (RadeonDriverData*) driver_data;
     RadeonDeviceData *rdev = rdrv->device_data;

     /* disable overlay */
     radeon_waitfifo( rdrv, rdev, 1 ); 
     radeon_out32( rdrv->mmio_base, OV0_SCALE_CNTL, 0 );

     return DFB_OK;
}


DisplayLayerFuncs RadeonOverlayFuncs = {
     .LayerDataSize      = ovlLayerDataSize,
     .InitLayer          = ovlInitLayer,
     .TestRegion         = ovlTestRegion,
     .AddRegion          = ovlAddRegion,
     .SetRegion          = ovlSetRegion,
     .RemoveRegion       = ovlRemoveRegion,
     .FlipRegion         = ovlFlipRegion,
     .SetColorAdjustment = ovlSetColorAdjustment,
     .SetInputField      = ovlSetInputField,
     .GetLevel           = ovlGetLevel,
     .SetLevel           = ovlSetLevel
};


/*** Internal Functions ***/

static void
ovl_calc_coordinates( RadeonDriverData       *rdrv,
                      RadeonOverlayLayerData *rovl,
                      CoreSurface            *surface,
                      CoreLayerRegionConfig  *config )
{
     RadeonDeviceData *rdev    = rdrv->device_data;
     DFBRectangle      source  = config->source;
     DFBRectangle      dest    = config->dest; 
     __u32             ecp_div = 0;
     __u32             h_inc;
     __u32             h_inc2;
     __u32             v_inc;
     __u32             step_by;
     __u32             tmp;
     int               xres;
     int               yres;

     dfb_screen_get_screen_size( rovl->screen, &xres, &yres );
 
     if (dest.w > (source.w << 4))
          dest.w = source.w << 4;

     if (dest.h > (source.h << 4))
          dest.h = source.h << 4;
 
     if (dest.x < 0) {
          source.w += dest.x * source.w / dest.w; 
          dest.w   += dest.x;
          dest.x    = 0;
     }
     
     if (dest.y < 0) {
          source.h += dest.y * source.h / dest.h;
          dest.h   += dest.y;
          dest.y    = 0;
     }

     if ((dest.x + dest.w) > xres) {
          source.w = (xres - dest.x) * source.w / dest.w;
          dest.w   =  xres - dest.x;
     }

     if ((dest.y + dest.h) > yres) {
          source.h = (yres - dest.y) * source.h / dest.h;
          dest.h   =  yres - dest.y;
     }
     
     if (dest.w < 1 || dest.h < 1 || source.w < 1 || source.h < 1) {
          config->opacity = 0;
          return;
     }

     if (config->options & DLOP_DEINTERLACING)
          source.h /= 2;

     tmp = radeon_in32( rdrv->mmio_base, 
                        rovl->crtc2 ? CRTC2_GEN_CNTL : CRTC_GEN_CNTL );
     
     if (tmp & CRTC_DBL_SCAN_EN) {
          dest.y *= 2;
          dest.h *= 2;
     }

     if (tmp & CRTC_INTERLACE_EN) {
          dest.y /= 2;
          dest.h /= 2;
     }

     /* FIXME: We need to know the VideoMode of the current screen. */
#if 0
     if ((100000000 / mode->pixclock) >= 17500)
          ecp_div = 1;
#endif

     h_inc = (source.w << (12 + ecp_div)) / dest.w;
     v_inc = (source.h << 20)             / dest.h;
     
     for (step_by = 1; h_inc >= (2 << 12);) {
          step_by++;
          h_inc >>= 1;
     }

     switch (surface->format) { 
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
               h_inc2 = h_inc;
               break;
          default:
               h_inc2 = h_inc >> 1;
               break;
     }

     rovl->regs.V_INC   = v_inc;
     rovl->regs.H_INC   = h_inc   | (h_inc2 << 16);
     rovl->regs.STEP_BY = step_by | (step_by << 8);

     /* compute values for horizontal accumulators */
     tmp = 0x00028000 + (h_inc << 3);
     rovl->regs.P1_H_ACCUM_INIT = ((tmp <<  4) & 0x000f8000) |
                                  ((tmp << 12) & 0xf0000000);
     tmp = 0x00028000 + (h_inc2 << 3);
     rovl->regs.P23_H_ACCUM_INIT = ((tmp <<  4) & 0x000f8000) |
                                   ((tmp << 12) & 0x70000000);

     /* compute values for vertical accumulators */
     tmp = 0x00018000;
     rovl->regs.P1_V_ACCUM_INIT  = ((tmp << 4) & OV0_P1_V_ACCUM_INIT_MASK) |
                                   (OV0_P1_MAX_LN_IN_PER_LN_OUT & 1);
     tmp = 0x00018000;
     rovl->regs.P23_V_ACCUM_INIT = ((tmp << 4) & OV0_P23_V_ACCUM_INIT_MASK) |
                                   (OV0_P23_MAX_LN_IN_PER_LN_OUT & 1);
    
     if (!rovl->crtc2) {
          if (rdev->chipset <  CHIP_R300 &&
              rdev->chipset != CHIP_R200 &&
              rdev->chipset != CHIP_UNKNOWN)
               dest.x += 8;
     }

     /* compute destination coordinates */
     rovl->regs.Y_X_START = (dest.x & 0xffff) | (dest.y << 16);
     rovl->regs.Y_X_END   = ((dest.x + dest.w - 1) & 0xffff) | 
                            ((dest.y + dest.h - 1) << 16);
     
     /* compute source coordinates */
     rovl->regs.P1_BLANK_LINES_AT_TOP = P1_BLNK_LN_AT_TOP_M1_MASK  | 
                                         ((source.h - 1) << 16);
     rovl->regs.P1_X_START_END = (source.w - 1) & 0xffff;
     
     if (DFB_PLANAR_PIXELFORMAT( surface->format )) {
          rovl->regs.P23_BLANK_LINES_AT_TOP = P23_BLNK_LN_AT_TOP_M1_MASK |
                                              ((source.h/2 - 1) << 16);
          rovl->regs.P2_X_START_END = (source.w/2 - 1) & 0xffff;
          rovl->regs.P3_X_START_END = rovl->regs.P2_X_START_END;
     }
     else {
          rovl->regs.P23_BLANK_LINES_AT_TOP = P23_BLNK_LN_AT_TOP_M1_MASK |
                                              ((source.h - 1) << 16);
          rovl->regs.P2_X_START_END = rovl->regs.P1_X_START_END;
          rovl->regs.P3_X_START_END = rovl->regs.P1_X_START_END;
     }
}

static void
ovl_calc_buffers( RadeonDriverData       *rdrv,
                  RadeonOverlayLayerData *rovl,
                  CoreSurface            *surface,
                  CoreLayerRegionConfig  *config )
{
     RadeonDeviceData *rdev       = rdrv->device_data;
     SurfaceBuffer    *buffer     = surface->front_buffer;
     DFBRectangle      source     = config->source;
     __u32             offsets[3] = { 0, 0, 0 };
     __u32             pitch      = buffer->video.pitch;
     int               even       = 0;
     int               cropleft;
     int               croptop;
     
     if (config->options & DLOP_DEINTERLACING) {
          source.y /= 2;
          source.h /= 2;
          pitch    *= 2;
          even      = rovl->field;
     }
     
     cropleft = source.x;
     croptop  = source.y;
     
     if (config->dest.x < 0)
          cropleft += -config->dest.x * source.w / config->dest.w;
          
     if (config->dest.y < 0)
          croptop  += -config->dest.y * source.h / config->dest.h;

     if (DFB_PLANAR_PIXELFORMAT( surface->format )) {
          cropleft &= ~31;
          croptop  &= ~1;
     
          offsets[0]  = buffer->video.offset;
          offsets[1]  = offsets[0] + surface->height   * buffer->video.pitch; 
          offsets[2]  = offsets[1] + surface->height/2 * buffer->video.pitch/2;
          offsets[0] += croptop   * pitch   + cropleft;
          offsets[1] += croptop/2 * pitch/2 + cropleft/2;
          offsets[2] += croptop/2 * pitch/2 + cropleft/2;
          
          if (even) {
               offsets[0] += buffer->video.pitch;
               offsets[1] += buffer->video.pitch/2;
               offsets[2] += buffer->video.pitch/2;
          }

          if (surface->format == DSPF_YV12) {
               __u32 tmp  = offsets[1];
               offsets[1] = offsets[2];
               offsets[2] = tmp;
          }
     } 
     else {
          offsets[0] = buffer->video.offset + croptop * pitch +
                       cropleft * DFB_BYTES_PER_PIXEL( surface->format );
          if (even) 
               offsets[0] += buffer->video.pitch;
          
          offsets[1] = 
          offsets[2] = offsets[0];
     }

     switch (buffer->storage) {
          case CSS_VIDEO:
               rovl->regs.BASE_ADDR = rdev->fb_offset;
               break;
          case CSS_AUXILIARY:
               rovl->regs.BASE_ADDR = rdev->agp_offset;
               break;
          default:
               D_BUG( "unknown buffer storage" );
               config->opacity = 0;
               return;
     }
 
     rovl->regs.VID_BUF0_BASE_ADRS   = (offsets[0] & VIF_BUF0_BASE_ADRS_MASK);
     rovl->regs.VID_BUF1_BASE_ADRS   = (offsets[1] & VIF_BUF1_BASE_ADRS_MASK) |
                                        VIF_BUF1_PITCH_SEL;
     rovl->regs.VID_BUF2_BASE_ADRS   = (offsets[2] & VIF_BUF2_BASE_ADRS_MASK) |
                                        VIF_BUF2_PITCH_SEL;
     rovl->regs.VID_BUF3_BASE_ADRS   = (offsets[0] & VIF_BUF3_BASE_ADRS_MASK);
     rovl->regs.VID_BUF4_BASE_ADRS   = (offsets[1] & VIF_BUF4_BASE_ADRS_MASK) |
                                        VIF_BUF4_PITCH_SEL;
     rovl->regs.VID_BUF5_BASE_ADRS   = (offsets[2] & VIF_BUF5_BASE_ADRS_MASK) |
                                        VIF_BUF5_PITCH_SEL;   
     rovl->regs.VID_BUF_PITCH0_VALUE = pitch;
     rovl->regs.VID_BUF_PITCH1_VALUE = pitch/2;
}

static void 
ovl_calc_regs( RadeonDriverData       *rdrv,
               RadeonOverlayLayerData *rovl,
               CoreSurface            *surface,
               CoreLayerRegionConfig  *config )
{
     rovl->regs.SCALE_CNTL = 0;
     
     /* Configure coordinates */
     ovl_calc_coordinates( rdrv, rovl, surface, config );
     
     /* Configure buffers */                                   
     ovl_calc_buffers( rdrv, rovl, surface, config );
     
     /* Configure scaler */
     if (rovl->level == -1) {
          rovl->regs.KEY_CNTL   = GRAPHIC_KEY_FN_FALSE |
                                  VIDEO_KEY_FN_FALSE   |
                                  CMP_MIX_AND;
          rovl->regs.MERGE_CNTL = DISP_ALPHA_MODE_PER_PIXEL |
                                  0x00ff0000 | /* graphic alpha */
                                  0xff000000; /* overlay alpha */
     }
     else if (config->options & DLOP_OPACITY) {
          rovl->regs.KEY_CNTL   = GRAPHIC_KEY_FN_TRUE |
                                  VIDEO_KEY_FN_TRUE   |
                                  CMP_MIX_AND;
          rovl->regs.MERGE_CNTL = DISP_ALPHA_MODE_GLOBAL |
                                  0x00ff0000             |
                                  (config->opacity << 24);
     }
     else {
          rovl->regs.KEY_CNTL = CMP_MIX_AND;
          
          if (config->options & DLOP_SRC_COLORKEY)
               rovl->regs.KEY_CNTL |= VIDEO_KEY_FN_NE;
          else
               rovl->regs.KEY_CNTL |= VIDEO_KEY_FN_TRUE;

          if (config->options & DLOP_DST_COLORKEY)
               rovl->regs.KEY_CNTL |= GRAPHIC_KEY_FN_EQ;
          else
               rovl->regs.KEY_CNTL |= GRAPHIC_KEY_FN_TRUE;

          rovl->regs.MERGE_CNTL = 0xffff0000;
     }
     
     if (config->opacity) {
          rovl->regs.SCALE_CNTL = SCALER_SMART_SWITCH   |
                                  SCALER_DOUBLE_BUFFER  |
                                  SCALER_ADAPTIVE_DEINT |
                                  (rovl->crtc2 << 14);
          
          if (config->source.w == config->dest.w)
               rovl->regs.SCALE_CNTL |= SCALER_HORZ_PICK_NEAREST;
          if (config->source.h == config->dest.h)
               rovl->regs.SCALE_CNTL |= SCALER_VERT_PICK_NEAREST;
 
          switch (surface->format) {
               case DSPF_ARGB1555:
                    rovl->regs.SCALE_CNTL |= SCALER_SOURCE_15BPP |
                                             SCALER_PRG_LOAD_START;
                    break;
               case DSPF_RGB16:
                    rovl->regs.SCALE_CNTL |= SCALER_SOURCE_16BPP |
                                             SCALER_PRG_LOAD_START;
                    break; 
               case DSPF_RGB32:
               case DSPF_ARGB:
                    rovl->regs.SCALE_CNTL |= SCALER_SOURCE_32BPP |
                                             SCALER_PRG_LOAD_START;
                    break;
               case DSPF_UYVY:
                    rovl->regs.SCALE_CNTL |= SCALER_SOURCE_YVYU422;
                    break;
               case DSPF_YUY2:
                    rovl->regs.SCALE_CNTL |= SCALER_SOURCE_VYUY422;
                    break;
               case DSPF_YV12:
               case DSPF_I420:
                    rovl->regs.SCALE_CNTL |= SCALER_SOURCE_YUV12;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    config->opacity = 0;
                    return;
          }
          
          rovl->regs.SCALE_CNTL |= SCALER_ENABLE;
     }          
}

static void 
ovl_set_regs( RadeonDriverData       *rdrv,
              RadeonOverlayLayerData *rovl )
{
     RadeonDeviceData *rdev = rdrv->device_data;
     volatile __u8    *mmio = rdrv->mmio_base;
     
     radeon_waitfifo( rdrv, rdev, 1 );
     radeon_out32( mmio, OV0_REG_LOAD_CNTL, REG_LD_CTL_LOCK );
     while(!(radeon_in32( mmio, OV0_REG_LOAD_CNTL ) & REG_LD_CTL_LOCK_READBACK));
     
     radeon_waitfifo( rdrv, rdev, 17 );
     radeon_out32( mmio, OV0_H_INC,                  rovl->regs.H_INC );
     radeon_out32( mmio, OV0_STEP_BY,                rovl->regs.STEP_BY );
     if (rovl->crtc2) {
          radeon_out32( mmio, OV1_Y_X_START,         rovl->regs.Y_X_START );
          radeon_out32( mmio, OV1_Y_X_END,           rovl->regs.Y_X_END );
     } else {
          radeon_out32( mmio, OV0_Y_X_START,         rovl->regs.Y_X_START );
          radeon_out32( mmio, OV0_Y_X_END,           rovl->regs.Y_X_END );
     }
     radeon_out32( mmio, OV0_V_INC,                  rovl->regs.V_INC );
     radeon_out32( mmio, OV0_P1_BLANK_LINES_AT_TOP,  rovl->regs.P1_BLANK_LINES_AT_TOP );
     radeon_out32( mmio, OV0_P23_BLANK_LINES_AT_TOP, rovl->regs.P23_BLANK_LINES_AT_TOP );
     radeon_out32( mmio, OV0_VID_BUF_PITCH0_VALUE,   rovl->regs.VID_BUF_PITCH0_VALUE );
     radeon_out32( mmio, OV0_VID_BUF_PITCH1_VALUE,   rovl->regs.VID_BUF_PITCH1_VALUE );
     radeon_out32( mmio, OV0_P1_X_START_END,         rovl->regs.P1_X_START_END );
     radeon_out32( mmio, OV0_P2_X_START_END,         rovl->regs.P2_X_START_END );
     radeon_out32( mmio, OV0_P3_X_START_END,         rovl->regs.P3_X_START_END );
     radeon_out32( mmio, OV0_P1_V_ACCUM_INIT,        rovl->regs.P1_V_ACCUM_INIT );
     radeon_out32( mmio, OV0_BASE_ADDR,              rovl->regs.BASE_ADDR );
     radeon_out32( mmio, OV0_VID_BUF0_BASE_ADRS,     rovl->regs.VID_BUF0_BASE_ADRS );
     radeon_out32( mmio, OV0_VID_BUF1_BASE_ADRS,     rovl->regs.VID_BUF1_BASE_ADRS );
     radeon_out32( mmio, OV0_VID_BUF2_BASE_ADRS,     rovl->regs.VID_BUF2_BASE_ADRS );
    
     radeon_waitfifo( rdrv, rdev, 10 );
     radeon_out32( mmio, OV0_VID_BUF3_BASE_ADRS,     rovl->regs.VID_BUF3_BASE_ADRS );
     radeon_out32( mmio, OV0_VID_BUF4_BASE_ADRS,     rovl->regs.VID_BUF4_BASE_ADRS );
     radeon_out32( mmio, OV0_VID_BUF5_BASE_ADRS,     rovl->regs.VID_BUF5_BASE_ADRS );
     radeon_out32( mmio, OV0_P1_H_ACCUM_INIT,        rovl->regs.P1_H_ACCUM_INIT );
     radeon_out32( mmio, OV0_P23_V_ACCUM_INIT,       rovl->regs.P23_V_ACCUM_INIT );
     radeon_out32( mmio, OV0_P23_H_ACCUM_INIT,       rovl->regs.P23_H_ACCUM_INIT );

     radeon_out32( mmio, DISP_MERGE_CNTL,            rovl->regs.MERGE_CNTL );
     radeon_out32( mmio, OV0_KEY_CNTL,               rovl->regs.KEY_CNTL ); 
     radeon_out32( mmio, OV0_SCALE_CNTL,             rovl->regs.SCALE_CNTL );
     
     radeon_out32( mmio, OV0_REG_LOAD_CNTL, 0 );
}

static void
ovl_set_buffers( RadeonDriverData       *rdrv,
                 RadeonOverlayLayerData *rovl )
{
     RadeonDeviceData *rdev = rdrv->device_data;
     volatile __u8    *mmio = rdrv->mmio_base;
      
     radeon_waitfifo( rdrv, rdev, 1 );
     radeon_out32( mmio, OV0_REG_LOAD_CNTL, REG_LD_CTL_LOCK );
     while(!(radeon_in32( mmio, OV0_REG_LOAD_CNTL ) & REG_LD_CTL_LOCK_READBACK));

     radeon_waitfifo( rdrv, rdev, 8 );
     radeon_out32( mmio, OV0_BASE_ADDR,          rovl->regs.BASE_ADDR );
     radeon_out32( mmio, OV0_VID_BUF0_BASE_ADRS, rovl->regs.VID_BUF0_BASE_ADRS );
     radeon_out32( mmio, OV0_VID_BUF1_BASE_ADRS, rovl->regs.VID_BUF1_BASE_ADRS );
     radeon_out32( mmio, OV0_VID_BUF2_BASE_ADRS, rovl->regs.VID_BUF2_BASE_ADRS );
     radeon_out32( mmio, OV0_VID_BUF3_BASE_ADRS, rovl->regs.VID_BUF3_BASE_ADRS );
     radeon_out32( mmio, OV0_VID_BUF4_BASE_ADRS, rovl->regs.VID_BUF4_BASE_ADRS );
     radeon_out32( mmio, OV0_VID_BUF5_BASE_ADRS, rovl->regs.VID_BUF5_BASE_ADRS );

     radeon_out32( mmio, OV0_REG_LOAD_CNTL, 0 );
}

static void
ovl_set_colorkey( RadeonDriverData       *rdrv,
                  RadeonOverlayLayerData *rovl,
                  CoreLayerRegionConfig  *config )              
{
     volatile __u8 *mmio = rdrv->mmio_base; 
     __u32          SkeyLow, SkeyHigh;
     __u32          DkeyLow, DkeyHigh;
     __u32          tmp;
     
     SkeyLow  = PIXEL_RGB32( config->src_key.r,
                             config->src_key.g,
                             config->src_key.b );
     SkeyHigh = SkeyLow | 0xff000000;
     
     tmp = radeon_in32( mmio, rovl->crtc2 ? CRTC2_GEN_CNTL : CRTC_GEN_CNTL );
     switch ((tmp >> 8) & 0xf) {
          case DST_8BPP:
          case DST_8BPP_RGB332:
               DkeyLow = ((MAX( config->dst_key.r - 0x20, 0 ) & 0xe0) << 16) |
                         ((MAX( config->dst_key.g - 0x20, 0 ) & 0xe0) <<  8) |
                         ((MAX( config->dst_key.b - 0x40, 0 ) & 0xc0)      );
               break;
          case DST_15BPP:
               DkeyLow = ((MAX( config->dst_key.r - 0x08, 0 ) & 0xf8) << 16) |
                         ((MAX( config->dst_key.g - 0x08, 0 ) & 0xf8) <<  8) |
                         ((MAX( config->dst_key.b - 0x08, 0 ) & 0xf8)      );
               break;
          case DST_16BPP:
               DkeyLow = ((MAX( config->dst_key.r - 0x08, 0 ) & 0xf8) << 16) |
                         ((MAX( config->dst_key.g - 0x04, 0 ) & 0xfc) <<  8) |
                         ((MAX( config->dst_key.b - 0x08, 0 ) & 0xf8)      );
               break;
          default:
               DkeyLow = PIXEL_RGB32( config->dst_key.r,
                                      config->dst_key.g,
                                      config->dst_key.b );
               break;
     }

     DkeyHigh = PIXEL_RGB32( config->dst_key.r,
                             config->dst_key.g,
                             config->dst_key.b ) | 0xff000000;

     radeon_waitfifo( rdrv, rdrv->device_data, 4 );
     radeon_out32( mmio, OV0_VID_KEY_CLR_LOW,   SkeyLow  );
     radeon_out32( mmio, OV0_VID_KEY_CLR_HIGH,  SkeyHigh );
     radeon_out32( mmio, OV0_GRPH_KEY_CLR_LOW,  DkeyLow  );
     radeon_out32( mmio, OV0_GRPH_KEY_CLR_HIGH, DkeyHigh ); 
}

static void
ovl_set_adjustment( RadeonDriverData       *rdrv,
                    RadeonOverlayLayerData *rovl,
                    float                   brightness,
                    float                   contrast,
                    float                   saturation,
                    float                   hue )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     float          HueSin, HueCos; 
     float          Luma;
     float          RCb, RCr;
     float          GCb, GCr;
     float          BCb, BCr;
     float          AdjOff, ROff, GOff, BOff;
     __u32          dwLuma, dwROff, dwGOff, dwBOff;
     __u32          dwRCb, dwRCr;
     __u32          dwGCb, dwGCr;
     __u32          dwBCb, dwBCr;
     
     HueSin = sin( hue );
     HueCos = cos( hue );

     Luma = contrast   *           +1.1678;
     RCb  = saturation * -HueSin * +1.6007;
     RCr  = saturation *  HueCos * +1.6007;
     GCb  = saturation * (HueCos * -0.3929 - HueSin * -0.8154);
     GCr  = saturation * (HueCos * -0.3929 + HueCos * -0.8154);
     BCb  = saturation *  HueCos * +2.0232;
     BCr  = saturation *  HueSin * +2.0232;
    
     AdjOff = contrast * 1.1678 * brightness * 1023.0;
     ROff   = AdjOff - Luma * 64.0 - (RCb + RCr) * 512.0;
     GOff   = AdjOff - Luma * 64.0 - (GCb + GCr) * 512.0;
     BOff   = AdjOff - Luma * 64.0 - (BCb + BCr) * 512.0;
     ROff   = CLAMP( ROff, -2048.0, 2047.5 );
     GOff   = CLAMP( GOff, -2048.0, 2047.5 );
     BOff   = CLAMP( BOff, -2048.0, 2047.5 );
     dwROff = ((__u32)(ROff * 2.0)) & 0x1fff;
     dwGOff = ((__u32)(GOff * 2.0)) & 0x1fff;
     dwBOff = ((__u32)(BOff * 2.0)) & 0x1fff;
 
     dwLuma = (((__u32)(Luma * 256.0)) & 0xfff) << 20;
     dwRCb  = (((__u32)(RCb  * 256.0)) & 0xfff) <<  4;
     dwRCr  = (((__u32)(RCr  * 256.0)) & 0xfff) << 20;
     dwGCb  = (((__u32)(GCb  * 256.0)) & 0xfff) <<  4;
     dwGCr  = (((__u32)(GCr  * 256.0)) & 0xfff) << 20;
     dwBCb  = (((__u32)(BCb  * 256.0)) & 0xfff) <<  4;
     dwBCr  = (((__u32)(BCr  * 256.0)) & 0xfff) << 20;
 
     radeon_waitfifo( rdrv, rdrv->device_data, 6 );
     radeon_out32( mmio, OV0_LIN_TRANS_A, dwRCb  | dwLuma );
     radeon_out32( mmio, OV0_LIN_TRANS_B, dwROff | dwRCr );
     radeon_out32( mmio, OV0_LIN_TRANS_C, dwGCb  | dwLuma );
     radeon_out32( mmio, OV0_LIN_TRANS_D, dwGOff | dwGCr );
     radeon_out32( mmio, OV0_LIN_TRANS_E, dwBCb  | dwLuma );
     radeon_out32( mmio, OV0_LIN_TRANS_F, dwBOff | dwBCr );
}

