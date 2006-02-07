/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI R100 based chipsets written by
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
#include <math.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/screen.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layer_control.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <direct/messages.h>

#include "r100.h"
#include "r100_regs.h"
#include "r100_mmio.h"


typedef struct {
     CoreLayerRegionConfig config;
     float                 brightness;
     float                 contrast;
     float                 saturation;
     float                 hue;
     int                   field;

     CoreSurface          *surface;
     
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
          __u32 AUTO_FLIP_CNTL;
          __u32 DEINTERLACE_PATTERN;
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
} R100OverlayLayerData;

static void ov0_calc_regs     ( R100DriverData        *rdrv,
                                R100OverlayLayerData  *rov0,
                                CoreSurface           *surface,
                                CoreLayerRegionConfig *config );
static void ov0_set_regs      ( R100DriverData        *rdrv,
                                R100OverlayLayerData  *rov0 );                            
static void ov0_calc_buffers  ( R100DriverData        *rdrv,
                                R100OverlayLayerData  *rov0,
                                CoreSurface           *surface,
                                CoreLayerRegionConfig *config );
static void ov0_set_buffers   ( R100DriverData        *rdrv,
                                R100OverlayLayerData  *rov0 );
static void ov0_set_colorkey  ( R100DriverData        *rdrv,
                                R100OverlayLayerData  *rov0,
                                CoreLayerRegionConfig *config );
static void ov0_set_adjustment( R100DriverData        *rdrv,
                                R100OverlayLayerData  *rov0,
                                float                  brightness,
                                float                  contrast,
                                float                  saturation,
                                float                  hue );
                
#define OV0_SUPPORTED_OPTIONS \
     ( DLOP_DST_COLORKEY | DLOP_OPACITY | DLOP_DEINTERLACING )

/**********************/

static int
ov0LayerDataSize()
{
     return sizeof(R100OverlayLayerData);
}

static DFBResult
ov0InitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
     R100DriverData       *rdrv = (R100DriverData*) driver_data;
     R100OverlayLayerData *rov0 = (R100OverlayLayerData*) layer_data;
     volatile __u8        *mmio = rdrv->mmio_base;
     
     /* fill layer description */
     description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;
     description->caps = DLCAPS_SURFACE       | DLCAPS_SCREEN_LOCATION |
                         DLCAPS_BRIGHTNESS    | DLCAPS_CONTRAST        |
                         DLCAPS_SATURATION    | DLCAPS_HUE             |
                         DLCAPS_DST_COLORKEY  | DLCAPS_OPACITY         |
                         DLCAPS_DEINTERLACING;

     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Radeon200 Overlay" );

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
     r100_waitfifo( rdrv, rdrv->device_data, 9 );
     r100_out32( mmio, OV0_SCALE_CNTL, SCALER_SOFT_RESET ); 
     r100_out32( mmio, OV0_AUTO_FLIP_CNTL, 0 );
     r100_out32( mmio, OV0_EXCLUSIVE_HORZ, 0 ); 
     r100_out32( mmio, OV0_FILTER_CNTL, FILTER_HARDCODED_COEF );
     r100_out32( mmio, OV0_TEST, 0 ); 
     r100_out32( mmio, FCP_CNTL, FCP0_SRC_GND );
     r100_out32( mmio, CAP0_TRIG_CNTL, 0 );
     r100_out32( mmio, VID_BUFFER_CONTROL, 0x00010001 );
     r100_out32( mmio, DISPLAY_TEST_DEBUG_CNTL, 0 );
     
     /* reset color adjustments */
     ov0_set_adjustment( rdrv, rov0, 0, 0, 0, 0 );
     
     return DFB_OK;
}

static DFBResult
ov0TestRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     /* check for unsupported options */
     if (config->options & ~OV0_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     if (config->options & DLOP_OPACITY &&
         config->options & (DLOP_SRC_COLORKEY | DLOP_DST_COLORKEY))
          fail |= CLRCF_OPTIONS;

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
ov0SetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags  updated,
              CoreSurface                *surface,
              CorePalette                *palette )
{
     R100DriverData       *rdrv = (R100DriverData*) driver_data;
     R100OverlayLayerData *rov0 = (R100OverlayLayerData*) layer_data;

     /* save configuration */
     rov0->config = *config;
     rov0->surface = surface;
     
     if (updated & (CLRCF_WIDTH  | CLRCF_HEIGHT | CLRCF_FORMAT  |
                    CLRCF_SOURCE | CLRCF_DEST   | CLRCF_OPTIONS | CLRCF_OPACITY)) 
     {
          ov0_calc_regs( rdrv, rov0, surface, &rov0->config );
          ov0_set_regs( rdrv, rov0 );
     }
     
     if (updated & (CLRCF_SRCKEY | CLRCF_DSTKEY))
          ov0_set_colorkey( rdrv, rov0, &rov0->config );

     return DFB_OK;
}

static DFBResult
ov0FlipRegion( CoreLayer           *layer,
               void                *driver_data,
               void                *layer_data,
               void                *region_data,
               CoreSurface         *surface,
               DFBSurfaceFlipFlags  flags )
{
     R100DriverData       *rdrv = (R100DriverData*) driver_data;
     R100OverlayLayerData *rov0 = (R100OverlayLayerData*) layer_data;

     dfb_surface_flip_buffers( surface, false );
      
     ov0_calc_buffers( rdrv, rov0, surface, &rov0->config );
     ov0_set_buffers( rdrv, rov0 );
   
     if (flags & DSFLIP_WAIT)
          dfb_layer_wait_vsync( layer );

     return DFB_OK;
}

static DFBResult
ov0SetColorAdjustment( CoreLayer          *layer,
                       void               *driver_data,
                       void               *layer_data,
                       DFBColorAdjustment *adj )
{
     R100DriverData       *rdrv = (R100DriverData*) driver_data;
     R100OverlayLayerData *rov0 = (R100OverlayLayerData*) layer_data;

     if (adj->flags & DCAF_BRIGHTNESS)
          rov0->brightness = (float)(adj->brightness-0x8000) / 65535.0;

     if (adj->flags & DCAF_CONTRAST)
          rov0->contrast   = (float)adj->contrast / 32768.0;

     if (adj->flags & DCAF_SATURATION)
          rov0->saturation = (float)adj->saturation / 32768.0;

     if (adj->flags & DCAF_HUE)
          rov0->hue        = (float)(adj->hue-0x8000) * 3.1416 / 65535.0;

     ov0_set_adjustment( rdrv, rov0, rov0->brightness, rov0->contrast,
                                     rov0->saturation, rov0->hue );

     return DFB_OK;
}

static DFBResult
ov0SetInputField( CoreLayer *layer,
                  void      *driver_data,
                  void      *layer_data,
                  void      *region_data,
                  int        field )
{
     R100DriverData       *rdrv = (R100DriverData*) driver_data;
     R100OverlayLayerData *rov0 = (R100OverlayLayerData*) layer_data;

     rov0->field = field;
             
     if (rov0->surface) {
          ov0_calc_buffers( rdrv, rov0, rov0->surface, &rov0->config );
          ov0_set_buffers( rdrv, rov0 );
     }
     
     return DFB_OK;
}

static DFBResult
ov0RemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
     R100DriverData *rdrv = (R100DriverData*) driver_data;
     R100DeviceData *rdev = rdrv->device_data;

     /* disable overlay */
     r100_waitfifo( rdrv, rdev, 1 ); 
     r100_out32( rdrv->mmio_base, OV0_SCALE_CNTL, 0 );

     return DFB_OK;
}


DisplayLayerFuncs R100OverlayFuncs = {
     .LayerDataSize      = ov0LayerDataSize,
     .InitLayer          = ov0InitLayer,
     .TestRegion         = ov0TestRegion,
     .SetRegion          = ov0SetRegion,
     .RemoveRegion       = ov0RemoveRegion,
     .FlipRegion         = ov0FlipRegion,
     .SetColorAdjustment = ov0SetColorAdjustment,
     .SetInputField      = ov0SetInputField
};


/*** Internal Functions ***/

static void
ov0_calc_coordinates( R100DriverData        *rdrv,
                      R100OverlayLayerData  *rov0,
                      CoreSurface           *surface,
                      CoreLayerRegionConfig *config )
{
     VideoMode    *mode     = dfb_system_current_mode();
     DFBRectangle  source   = config->source;
     DFBRectangle  dest     = config->dest; 
     __u32         ecp_div  = 0;
     __u32         h_inc;
     __u32         v_inc;
     __u32         step_by;
     __u32         tmp;

     if (!mode) {
          D_BUG( "no video mode set" );
          return;
     }
 
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

     if ((dest.x + dest.w) > mode->xres) {
          source.w = (mode->xres - dest.x) * source.w / dest.w;
          dest.w   =  mode->xres - dest.x;
     }

     if ((dest.y + dest.h) > mode->yres) {
          source.h = (mode->yres - dest.y) * source.h / dest.h;
          dest.h   =  mode->yres - dest.y;
     }
     
     if (dest.w < 1 || dest.h < 1 || source.w < 1 || source.h < 1) {
          config->opacity = 0;
          return;
     }

     if (config->options & DLOP_DEINTERLACING)
          source.h /= 2;
     
     if (mode->doubled) {
          dest.y *= 2;
          dest.h *= 2;
     }

     if (mode->laced) {
          dest.y /= 2;
          dest.h /= 2;
     }

     if ((100000000 / mode->pixclock) >= 17500)
          ecp_div = 1;

     h_inc = (source.w << (12 + ecp_div)) / dest.w;
     v_inc = (source.h << 20)             / dest.h;
     
     for (step_by = 1; h_inc >= (2 << 12);) {
          step_by++;
          h_inc >>= 1;
     }

     /* calculate values for horizontal accumulators */
     tmp = 0x00028000 + (h_inc << 3);
     rov0->regs.P1_H_ACCUM_INIT = ((tmp <<  4) & 0x000f8000) |
                                  ((tmp << 12) & 0xf0000000);
     tmp = 0x00028000 + (h_inc << 2);
     rov0->regs.P23_H_ACCUM_INIT = ((tmp <<  4) & 0x000f8000) |
                                   ((tmp << 12) & 0x70000000);

     /* calculate values for vertical accumulators */
     tmp = 0x00018000;
     rov0->regs.P1_V_ACCUM_INIT  = ((tmp << 4) & OV0_P1_V_ACCUM_INIT_MASK) |
                                   (OV0_P1_MAX_LN_IN_PER_LN_OUT & 1);
     tmp = 0x00018000;
     rov0->regs.P23_V_ACCUM_INIT = ((tmp << 4) & OV0_P23_V_ACCUM_INIT_MASK) |
                                   (OV0_P23_MAX_LN_IN_PER_LN_OUT & 1);
     
     switch (surface->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               rov0->regs.H_INC = h_inc | (h_inc << 16);
               break;
          default:
               rov0->regs.H_INC = h_inc | (h_inc/2 << 16);
               break;
     }
     
     rov0->regs.STEP_BY = step_by | (step_by << 8);
     rov0->regs.V_INC   = v_inc;
     
     /* calculate destination coordinates */
     rov0->regs.Y_X_START = ((dest.x + 8) & 0xffff) | (dest.y << 16);
     rov0->regs.Y_X_END   = ((dest.x + dest.w + 8) & 0xffff) | 
                            ((dest.y + dest.h) << 16);
     
     /* calculate source coordinates */
     rov0->regs.P1_BLANK_LINES_AT_TOP  = P1_BLNK_LN_AT_TOP_M1_MASK  | 
                                         ((source.h - 1) << 16);
     rov0->regs.P23_BLANK_LINES_AT_TOP = P23_BLNK_LN_AT_TOP_M1_MASK |
                                         ((source.h/2 - 1) << 16);
    
     rov0->regs.P1_X_START_END = (source.w - 1) & 0xffff;
     if (DFB_PLANAR_PIXELFORMAT( surface->format )) {
          rov0->regs.P2_X_START_END = (source.w/2 - 1) & 0xffff;
          rov0->regs.P3_X_START_END = rov0->regs.P2_X_START_END;
     } else {
          rov0->regs.P2_X_START_END = rov0->regs.P1_X_START_END;
          rov0->regs.P3_X_START_END = rov0->regs.P1_X_START_END;
     }
}

static void
ov0_calc_buffers( R100DriverData        *rdrv,
                  R100OverlayLayerData  *rov0,
                  CoreSurface           *surface,
                  CoreLayerRegionConfig *config )
{
     R100DeviceData *rdev       = rdrv->device_data;
     SurfaceBuffer  *buffer     = surface->front_buffer;
     DFBRectangle    source     = config->source;
     __u32           base;
     __u32           offsets[3] = { 0, 0, 0 };
     __u32           pitch      = buffer->video.pitch;
     int             even       = 0;
     int             cropleft;
     int             croptop;
     
     if (config->options & DLOP_DEINTERLACING) {
          source.y /= 2;
          source.h /= 2;
          pitch    *= 2;
          even      = rov0->field;
     }
     
     cropleft = source.x;
     croptop  = source.y;
     
     if (config->dest.x < 0)
          cropleft += -config->dest.x * source.w / config->dest.w;
          
     if (config->dest.y < 0)
          croptop  += -config->dest.y * source.h / config->dest.h;

     if (buffer->storage == CSS_AUXILIARY)
          base = rdev->agp_offset;
     else
          base = rdev->fb_offset;

     if (DFB_PLANAR_PIXELFORMAT( surface->format )) {
          cropleft &= ~31;
          croptop  &= ~1;
     
          offsets[0]  = base + buffer->video.offset;
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
          offsets[0] = base + buffer->video.offset + croptop * pitch +
                       cropleft * DFB_BYTES_PER_PIXEL( surface->format );
          if (even) 
               offsets[0] += buffer->video.pitch;
          
          offsets[1] = 
          offsets[2] = offsets[0];
     }
 
     rov0->regs.BASE_ADDR            = base;
     rov0->regs.VID_BUF0_BASE_ADRS   = (offsets[0] & VIF_BUF0_BASE_ADRS_MASK);
     rov0->regs.VID_BUF1_BASE_ADRS   = (offsets[1] & VIF_BUF1_BASE_ADRS_MASK) |
                                        VIF_BUF1_PITCH_SEL;
     rov0->regs.VID_BUF2_BASE_ADRS   = (offsets[2] & VIF_BUF2_BASE_ADRS_MASK) |
                                        VIF_BUF2_PITCH_SEL;
     rov0->regs.VID_BUF3_BASE_ADRS   = (offsets[0] & VIF_BUF3_BASE_ADRS_MASK);
     rov0->regs.VID_BUF4_BASE_ADRS   = (offsets[1] & VIF_BUF4_BASE_ADRS_MASK) |
                                        VIF_BUF4_PITCH_SEL;
     rov0->regs.VID_BUF5_BASE_ADRS   = (offsets[2] & VIF_BUF5_BASE_ADRS_MASK) |
                                        VIF_BUF5_PITCH_SEL;   
     rov0->regs.VID_BUF_PITCH0_VALUE = pitch;
     rov0->regs.VID_BUF_PITCH1_VALUE = pitch/2;
}

static void 
ov0_calc_regs( R100DriverData        *rdrv,
               R100OverlayLayerData  *rov0,
               CoreSurface           *surface,
               CoreLayerRegionConfig *config )
{
     /* Configure coordinates */
     ov0_calc_coordinates( rdrv, rov0, surface, config );
     
     /* Configure buffers */                                   
     ov0_calc_buffers( rdrv, rov0, surface, config );
     
     /* Configure options and enable scaler */
     if (config->opacity) {
          rov0->regs.SCALE_CNTL = SCALER_ENABLE        |
                                  SCALER_SMART_SWITCH  |
                                  SCALER_DOUBLE_BUFFER;

          if (config->options & DLOP_OPACITY) {
               rov0->regs.KEY_CNTL   = GRAPHIC_KEY_FN_TRUE |
                                       VIDEO_KEY_FN_TRUE   |
                                       CMP_MIX_AND;
               rov0->regs.MERGE_CNTL = DISP_ALPHA_MODE_GLOBAL |
                                       0x00ff0000             |
                                       (config->opacity << 24);
          }
          else {
               rov0->regs.KEY_CNTL = CMP_MIX_AND;
          
               if (config->options & DLOP_SRC_COLORKEY)
                    rov0->regs.KEY_CNTL |= VIDEO_KEY_FN_NE;
               else
                    rov0->regs.KEY_CNTL |= VIDEO_KEY_FN_TRUE;

               if (config->options & DLOP_DST_COLORKEY)
                    rov0->regs.KEY_CNTL |= GRAPHIC_KEY_FN_EQ;
               else
                    rov0->regs.KEY_CNTL |= GRAPHIC_KEY_FN_TRUE;

               rov0->regs.MERGE_CNTL = 0xffff0000;
          }

          if (config->options & DLOP_DEINTERLACING) {
               rov0->regs.SCALE_CNTL    |= SCALER_ADAPTIVE_DEINT;
               rov0->regs.AUTO_FLIP_CNTL = OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD |
                                           OV0_AUTO_FLIP_CNTL_SHIFT_ODD_DOWN;
          } else
               rov0->regs.AUTO_FLIP_CNTL = OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD;
     
          /* onefield ?! */
          rov0->regs.DEINTERLACE_PATTERN = 0x9000eeee;
 
          switch (surface->format) {
               case DSPF_ARGB1555:
                    rov0->regs.SCALE_CNTL |= SCALER_SOURCE_15BPP |
                                             SCALER_PRG_LOAD_START;
                    break;
               case DSPF_RGB16:
                    rov0->regs.SCALE_CNTL |= SCALER_SOURCE_16BPP |
                                             SCALER_PRG_LOAD_START;
                    break; 
               case DSPF_RGB32:
               case DSPF_ARGB:
                    rov0->regs.SCALE_CNTL |= SCALER_SOURCE_32BPP |
                                             SCALER_PRG_LOAD_START;
                    break;
               case DSPF_UYVY:
                    rov0->regs.SCALE_CNTL |= SCALER_SOURCE_YVYU422;
                    break;
               case DSPF_YUY2:
                    rov0->regs.SCALE_CNTL |= SCALER_SOURCE_VYUY422;
                    break;
               case DSPF_YV12:
               case DSPF_I420:
                    rov0->regs.SCALE_CNTL |= SCALER_SOURCE_YUV12;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    rov0->regs.SCALE_CNTL = 0;
                    break;
          }
     } 
     else {
          rov0->regs.SCALE_CNTL = 0;
          rov0->regs.KEY_CNTL   = CMP_MIX_AND         | 
                                  VIDEO_KEY_FN_TRUE   |
                                  GRAPHIC_KEY_FN_TRUE;
          rov0->regs.MERGE_CNTL = 0xffff0000;
          rov0->regs.AUTO_FLIP_CNTL = OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD;
          rov0->regs.DEINTERLACE_PATTERN = 0x9000eeee;
     }         
}

static void 
ov0_set_regs( R100DriverData       *rdrv,
              R100OverlayLayerData *rov0 )
{
     R100DeviceData *rdev = rdrv->device_data;
     volatile __u8  *mmio = rdrv->mmio_base;
     
     r100_waitfifo( rdrv, rdev, 1 );
     r100_out32( mmio, OV0_REG_LOAD_CNTL, REG_LD_CTL_LOCK );
     while(!(r100_in32( mmio, OV0_REG_LOAD_CNTL ) & REG_LD_CTL_LOCK_READBACK));
      
     r100_waitfifo( rdrv, rdev, 4 );
     r100_out32( mmio, OV0_KEY_CNTL,               rov0->regs.KEY_CNTL ); 
     r100_out32( mmio, DISP_MERGE_CNTL,            rov0->regs.MERGE_CNTL );
     r100_out32( mmio, OV0_AUTO_FLIP_CNTL,         rov0->regs.AUTO_FLIP_CNTL );
     r100_out32( mmio, OV0_DEINTERLACE_PATTERN,    rov0->regs.DEINTERLACE_PATTERN ); 
     
     r100_waitfifo( rdrv, rdev, 17 );
     r100_out32( mmio, OV0_H_INC,                  rov0->regs.H_INC );
     r100_out32( mmio, OV0_STEP_BY,                rov0->regs.STEP_BY );
     r100_out32( mmio, OV0_Y_X_START,              rov0->regs.Y_X_START );
     r100_out32( mmio, OV0_Y_X_END,                rov0->regs.Y_X_END );
     r100_out32( mmio, OV0_V_INC,                  rov0->regs.V_INC );
     r100_out32( mmio, OV0_P1_BLANK_LINES_AT_TOP,  rov0->regs.P1_BLANK_LINES_AT_TOP );
     r100_out32( mmio, OV0_P23_BLANK_LINES_AT_TOP, rov0->regs.P23_BLANK_LINES_AT_TOP );
     r100_out32( mmio, OV0_VID_BUF_PITCH0_VALUE,   rov0->regs.VID_BUF_PITCH0_VALUE );
     r100_out32( mmio, OV0_VID_BUF_PITCH1_VALUE,   rov0->regs.VID_BUF_PITCH1_VALUE );
     r100_out32( mmio, OV0_P1_X_START_END,         rov0->regs.P1_X_START_END );
     r100_out32( mmio, OV0_P2_X_START_END,         rov0->regs.P2_X_START_END );
     r100_out32( mmio, OV0_P3_X_START_END,         rov0->regs.P3_X_START_END );
     r100_out32( mmio, OV0_P1_V_ACCUM_INIT,        rov0->regs.P1_V_ACCUM_INIT );
     r100_out32( mmio, OV0_BASE_ADDR,              rov0->regs.BASE_ADDR );
     r100_out32( mmio, OV0_VID_BUF0_BASE_ADRS,     rov0->regs.VID_BUF0_BASE_ADRS );
     r100_out32( mmio, OV0_VID_BUF1_BASE_ADRS,     rov0->regs.VID_BUF1_BASE_ADRS );
     r100_out32( mmio, OV0_VID_BUF2_BASE_ADRS,     rov0->regs.VID_BUF2_BASE_ADRS );
    
     r100_waitfifo( rdrv, rdev, 8 );
     r100_out32( mmio, OV0_VID_BUF3_BASE_ADRS,     rov0->regs.VID_BUF3_BASE_ADRS );
     r100_out32( mmio, OV0_VID_BUF4_BASE_ADRS,     rov0->regs.VID_BUF4_BASE_ADRS );
     r100_out32( mmio, OV0_VID_BUF5_BASE_ADRS,     rov0->regs.VID_BUF5_BASE_ADRS );
     r100_out32( mmio, OV0_P1_H_ACCUM_INIT,        rov0->regs.P1_H_ACCUM_INIT );
     r100_out32( mmio, OV0_P23_V_ACCUM_INIT,       rov0->regs.P23_V_ACCUM_INIT );
     r100_out32( mmio, OV0_P23_H_ACCUM_INIT,       rov0->regs.P23_H_ACCUM_INIT );

     r100_out32( mmio, OV0_SCALE_CNTL, rov0->regs.SCALE_CNTL );
     r100_out32( mmio, OV0_REG_LOAD_CNTL, 0 );
}

static void
ov0_set_buffers( R100DriverData       *rdrv,
                 R100OverlayLayerData *rov0 )
{
     R100DeviceData *rdev = rdrv->device_data;
     volatile __u8  *mmio = rdrv->mmio_base;
      
     r100_waitfifo( rdrv, rdev, 1 );
     r100_out32( mmio, OV0_REG_LOAD_CNTL, REG_LD_CTL_LOCK );
     while(!(r100_in32( mmio, OV0_REG_LOAD_CNTL ) & REG_LD_CTL_LOCK_READBACK));

     r100_waitfifo( rdrv, rdev, 8 );
     r100_out32( mmio, OV0_BASE_ADDR, rov0->regs.BASE_ADDR );
     r100_out32( mmio, OV0_VID_BUF0_BASE_ADRS, rov0->regs.VID_BUF0_BASE_ADRS );
     r100_out32( mmio, OV0_VID_BUF1_BASE_ADRS, rov0->regs.VID_BUF1_BASE_ADRS );
     r100_out32( mmio, OV0_VID_BUF2_BASE_ADRS, rov0->regs.VID_BUF2_BASE_ADRS );
     r100_out32( mmio, OV0_VID_BUF3_BASE_ADRS, rov0->regs.VID_BUF3_BASE_ADRS );
     r100_out32( mmio, OV0_VID_BUF4_BASE_ADRS, rov0->regs.VID_BUF4_BASE_ADRS );
     r100_out32( mmio, OV0_VID_BUF5_BASE_ADRS, rov0->regs.VID_BUF5_BASE_ADRS );

     r100_out32( mmio, OV0_REG_LOAD_CNTL, 0 );
}

static void
ov0_set_colorkey( R100DriverData        *rdrv,
                  R100OverlayLayerData  *rov0,
                  CoreLayerRegionConfig *config )              
{
     volatile __u8 *mmio = rdrv->mmio_base; 
     __u32          crtc_gen_ctl;
     __u32          SkeyLow, SkeyHigh;
     __u32          DkeyLow, DkeyHigh;
     
     SkeyLow  = PIXEL_RGB32( config->src_key.r,
                             config->src_key.g,
                             config->src_key.b );
     SkeyHigh = SkeyLow | 0xff000000;
     
     DkeyLow  = PIXEL_RGB32( config->dst_key.r,
                             config->dst_key.g,
                             config->dst_key.b );
     DkeyHigh = DkeyLow | 0xff000000;
     
     crtc_gen_ctl = r100_in32( mmio, CRTC_GEN_CNTL );
     
     switch ((crtc_gen_ctl >> 8) & 0xf) {
          case DST_8BPP:
          case DST_8BPP_RGB332:
               DkeyLow &= 0xe0e0e0;
               break;
          case DST_15BPP:
               DkeyLow &= 0xf8f8f8;
               break;
          case DST_16BPP:
               DkeyLow &= 0xf8fcf8;
               break;
          default:
               break;
     }

     r100_waitfifo( rdrv, rdrv->device_data, 4 );
     r100_out32( mmio, OV0_VID_KEY_CLR_LOW,   SkeyLow  );
     r100_out32( mmio, OV0_VID_KEY_CLR_HIGH,  SkeyHigh );
     r100_out32( mmio, OV0_GRPH_KEY_CLR_LOW,  DkeyLow  );
     r100_out32( mmio, OV0_GRPH_KEY_CLR_HIGH, DkeyHigh ); 
}


static void
ov0_set_adjustment( R100DriverData       *rdrv,
                    R100OverlayLayerData *rov0,
                    float                 brightness,
                    float                 contrast,
                    float                 saturation,
                    float                 hue )
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
 
     r100_waitfifo( rdrv, rdrv->device_data, 6 );
     r100_out32( mmio, OV0_LIN_TRANS_A, dwRCb  | dwLuma );
     r100_out32( mmio, OV0_LIN_TRANS_B, dwROff | dwRCr );
     r100_out32( mmio, OV0_LIN_TRANS_C, dwGCb  | dwLuma );
     r100_out32( mmio, OV0_LIN_TRANS_D, dwGOff | dwGCr );
     r100_out32( mmio, OV0_LIN_TRANS_E, dwBCb  | dwLuma );
     r100_out32( mmio, OV0_LIN_TRANS_F, dwBOff | dwBCr );
}

