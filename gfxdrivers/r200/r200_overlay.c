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
#include <math.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/layer_region.h>
#include <core/layer_control.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include "r200.h"
#include "r200_regs.h"
#include "r200_mmio.h"


typedef struct {
     CoreLayerRegionConfig config;
     float                 brightness;
     float                 contrast;
     float                 saturation;
     float                 hue;
     
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
          __u32 SCALE_CNTL;
     } regs;
} R200OverlayLayerData;

static void ov0_calc_regs   ( R200DriverData        *rdrv,
                              R200OverlayLayerData  *rov0,
                              CoreSurface           *surface,
                              CoreLayerRegionConfig *config );
static void ov0_set_regs    ( R200DriverData       *rdrv,
                              R200OverlayLayerData *rov0 );
static void ov0_swap_buffers( R200DriverData       *rdrv,
                              R200OverlayLayerData *rov0,
                              CoreSurface          *surface );
static void ov0_set_csc     ( R200DriverData       *rdrv,
                              R200OverlayLayerData *rov0,
                              float                 brightness,
                              float                 contrast,
                              float                 saturation,
                              float                 hue );
                
#define OV0_SUPPORTED_OPTIONS ( DLOP_DST_COLORKEY )

/**********************/

static int
ov0LayerDataSize()
{
     return sizeof(R200OverlayLayerData);
}

static DFBResult
ov0InitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
     R200DriverData       *rdrv = (R200DriverData*) driver_data;
     R200OverlayLayerData *rov0 = (R200OverlayLayerData*) layer_data;
     volatile __u8        *mmio = rdrv->mmio_base;
     
     /* fill layer description */
     description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;
     description->caps = DLCAPS_SURFACE       | DLCAPS_SCREEN_LOCATION |
                         DLCAPS_BRIGHTNESS    | DLCAPS_CONTRAST        |
                         DLCAPS_SATURATION    | DLCAPS_HUE             |
                         DLCAPS_DST_COLORKEY;

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
     r200_waitfifo( rdrv, rdrv->device_data, 10 );
     r200_out32( mmio, OV0_SCALE_CNTL, SCALER_SOFT_RESET ); 
     r200_out32( mmio, OV0_AUTO_FLIP_CNTL, 0 );
     r200_out32( mmio, OV0_EXCLUSIVE_HORZ, 0 ); 
     r200_out32( mmio, OV0_FILTER_CNTL, FILTER_HARDCODED_COEF );
     r200_out32( mmio, OV0_KEY_CNTL, VIDEO_KEY_FN_FALSE  |
                                     GRAPHIC_KEY_FN_TRUE |
                                     CMP_MIX_OR );
     r200_out32( mmio, OV0_TEST, 0 ); 
     r200_out32( mmio, FCP_CNTL, FCP0_SRC_GND );
     r200_out32( mmio, CAP0_TRIG_CNTL, 0 );
     r200_out32( mmio, VID_BUFFER_CONTROL, 0x00010001 );
     r200_out32( mmio, DISPLAY_TEST_DEBUG_CNTL, 0 );
     
     /* reset color adjustments */
     ov0_set_csc( rdrv, rov0, 0, 0, 0, 0 );
     
     /* reset gamma correction */
     r200_waitfifo( rdrv, rdrv->device_data, 18 );
     r200_out32( mmio, OV0_GAMMA_000_00F, 0x00400000 );
     r200_out32( mmio, OV0_GAMMA_010_01F, 0x00400020 );
     r200_out32( mmio, OV0_GAMMA_020_03F, 0x00800040 );
     r200_out32( mmio, OV0_GAMMA_040_07F, 0x01000080 );
     r200_out32( mmio, OV0_GAMMA_080_0BF, 0x01000100 );
     r200_out32( mmio, OV0_GAMMA_0C0_0FF, 0x01000100 );
     r200_out32( mmio, OV0_GAMMA_100_13F, 0x01000200 );
     r200_out32( mmio, OV0_GAMMA_140_17F, 0x01000200 );
     r200_out32( mmio, OV0_GAMMA_180_1BF, 0x01000300 );
     r200_out32( mmio, OV0_GAMMA_1C0_1FF, 0x01000300 );
     r200_out32( mmio, OV0_GAMMA_200_23F, 0x01000400 );
     r200_out32( mmio, OV0_GAMMA_240_27F, 0x01000400 );
     r200_out32( mmio, OV0_GAMMA_280_2BF, 0x01000500 );
     r200_out32( mmio, OV0_GAMMA_2C0_2FF, 0x01000500 );
     r200_out32( mmio, OV0_GAMMA_300_33F, 0x01000600 );
     r200_out32( mmio, OV0_GAMMA_340_37F, 0x01000600 );
     r200_out32( mmio, OV0_GAMMA_380_3BF, 0x01000700 );
     r200_out32( mmio, OV0_GAMMA_3C0_3FF, 0x01000700 );

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
     R200DriverData       *rdrv = (R200DriverData*) driver_data;
     R200OverlayLayerData *rov0 = (R200OverlayLayerData*) layer_data;

     /* save configuration */
     rov0->config = *config;
    
     ov0_calc_regs( rdrv, rov0, surface, config );
     ov0_set_regs( rdrv, rov0 );

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
     R200DriverData       *rdrv = (R200DriverData*) driver_data;
     R200OverlayLayerData *rov0 = (R200OverlayLayerData*) layer_data;

     dfb_surface_flip_buffers( surface, false );
     ov0_swap_buffers( rdrv, rov0, surface );
   
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
     R200DriverData       *rdrv = (R200DriverData*) driver_data;
     R200OverlayLayerData *rov0 = (R200OverlayLayerData*) layer_data;

     if (adj->flags & DCAF_BRIGHTNESS)
          rov0->brightness = (float)(adj->brightness-0x8000) / 65535.0;

     if (adj->flags & DCAF_CONTRAST)
          rov0->contrast   = (float)adj->contrast / 32768.0;

     if (adj->flags & DCAF_SATURATION)
          rov0->saturation = (float)adj->saturation / 32768.0;

     if (adj->flags & DCAF_HUE)
          rov0->hue        = (float)(adj->hue-0x8000) * 3.1416 / 65535.0;

     ov0_set_csc( rdrv, rov0, rov0->brightness, rov0->contrast,
                              rov0->saturation, rov0->hue );

     return DFB_OK;
}

static DFBResult
ov0RemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
     R200DriverData *rdrv = (R200DriverData*) driver_data;
     R200DeviceData *rdev = rdrv->device_data;

     /* disable overlay */
     r200_waitfifo( rdrv, rdev, 1 ); 
     r200_out32( rdrv->mmio_base, OV0_SCALE_CNTL, 0 );

     return DFB_OK;
}


DisplayLayerFuncs R200OverlayFuncs = {
     .LayerDataSize      = ov0LayerDataSize,
     .InitLayer          = ov0InitLayer,
     .TestRegion         = ov0TestRegion,
     .SetRegion          = ov0SetRegion,
     .RemoveRegion       = ov0RemoveRegion,
     .FlipRegion         = ov0FlipRegion,
     .SetColorAdjustment = ov0SetColorAdjustment
};


/*** Internal Functions ***/

static void
ov0_calc_offsets( R200DeviceData       *rdev,
                  R200OverlayLayerData *rov0,
                  CoreSurface          *surface,
                  __u32                 offsets[3] )
{
     SurfaceBuffer *buffer   = surface->front_buffer;
     int            cropleft = 0;
     int            croptop  = 0;
 
     if (rov0->config.dest.x < 0)
          cropleft = -rov0->config.dest.x * surface->width / rov0->config.dest.w;

     if (rov0->config.dest.y < 0)
          croptop  = -rov0->config.dest.y * surface->height / rov0->config.dest.h;

     offsets[0]  = rdev->fb_offset + buffer->video.offset;
     offsets[0] += croptop * buffer->video.pitch + 
                   cropleft * DFB_BYTES_PER_PIXEL( surface->format );

     if (DFB_PLANAR_PIXELFORMAT( surface->format )) {
          cropleft &= ~15;
          croptop  &= ~1;

          offsets[1]  = rdev->fb_offset + buffer->video.offset +
                        surface->height * buffer->video.pitch;
          offsets[2]  = offsets[1] +
                        (surface->height/2) * (buffer->video.pitch/2);
          offsets[1] += (croptop/2) * (buffer->video.pitch/2) + (cropleft/2);
          offsets[2] += (croptop/2) * (buffer->video.pitch/2) + (cropleft/2);
          
          if (surface->format == DSPF_YV12) {
               __u32 tmp  = offsets[1];
               offsets[1] = offsets[2];
               offsets[2] = tmp;
          }
     } else {
          offsets[1] = offsets[0];
          offsets[2] = offsets[0];
     }
}

static inline __u32
ov0_calc_dstkey( __u8 r, __u8 g, __u8 b )
{
     __u32 Kr, Kg, Kb;
     
     switch (dfb_primary_layer_pixelformat()) {
          case DSPF_RGB332:
               Kr = r & 0xe0;
               Kg = g & 0xe0;
               Kb = b & 0xc0;
               break;
          case DSPF_ARGB1555:
               Kr = r & 0xf8;
               Kg = g & 0xf8;
               Kb = b & 0xf8;
               break;
          case DSPF_RGB16:
               Kr = r & 0xf8;
               Kg = g & 0xfc;
               Kb = b & 0xf8;
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
               Kr = r;
               Kg = g;
               Kb = b;
               break;
          default:
               D_BUG( "unexpected primary layer pixelformat" );
               Kr = Kg = Kb = 0;
               break;
     }

     return PIXEL_RGB32( Kr, Kg, Kb );
}

static void 
ov0_calc_regs ( R200DriverData        *rdrv,
                R200OverlayLayerData  *rov0,
                CoreSurface           *surface,
                CoreLayerRegionConfig *config )
{
     R200DeviceData *rdev       = rdrv->device_data;
     __u32           offsets[3];
     __u32           tmp;
     __u32           h_inc;
     __u32           step_by;
     
     /* clear all */
     rov0->regs.KEY_CNTL   = 0;
     rov0->regs.SCALE_CNTL = 0;
     
     h_inc   = (config->source.w << 12) / config->dest.w;
     step_by = 1;
     while (h_inc >= (2 << 12)) {
          step_by++;
          h_inc >>= 1;
     }

     /* calculate values for horizontal accumulators */
     tmp = 0x00028000 + (h_inc << 3);
     rov0->regs.P1_H_ACCUM_INIT = ((tmp << 4) & 0x000f8000) | ((tmp << 12) & 0xf0000000);
     tmp = 0x00028000 + (h_inc << 2);
     rov0->regs.P23_H_ACCUM_INIT = ((tmp << 4) & 0x000f8000) | ((tmp << 12) & 0x70000000);

     /* calculate values for vertical accumulators */
     tmp = 0x00018000;
     rov0->regs.P1_V_ACCUM_INIT  = ((tmp << 4) & OV0_P1_V_ACCUM_INIT_MASK) |
                                   (OV0_P1_MAX_LN_IN_PER_LN_OUT & 1);
     tmp = 0x00018000;
     rov0->regs.P23_V_ACCUM_INIT = ((tmp << 4) & OV0_P23_V_ACCUM_INIT_MASK) |
                                   (OV0_P23_MAX_LN_IN_PER_LN_OUT & 1);

     /* set destination coordinates */
     switch (surface->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               rov0->regs.H_INC = h_inc | (h_inc << 16);
               break;
          default:
               rov0->regs.H_INC = h_inc | ((h_inc >> 1) << 16);
               break;
     }
     
     rov0->regs.STEP_BY = step_by | (step_by << 8);
     rov0->regs.V_INC   = (config->source.h << 20) / config->dest.h;
     
     if (rdev->chipset == CHIP_R200) {
          rov0->regs.Y_X_START = (config->dest.x) | (config->dest.y << 16);
          rov0->regs.Y_X_END   = (config->dest.x + config->dest.w) | 
                                 ((config->dest.y + config->dest.h) << 16);
     } else {    
          rov0->regs.Y_X_START = (config->dest.x + 8) | (config->dest.y << 16);
          rov0->regs.Y_X_END   = (config->dest.x + config->dest.w + 8) | 
                                 ((config->dest.y + config->dest.h) << 16);
     }
                                         
     /* set source coordinates and pitches */
     rov0->regs.P1_BLANK_LINES_AT_TOP  = P1_BLNK_LN_AT_TOP_M1_MASK  | 
                                         ((config->source.h - 1) << 16);
     rov0->regs.P23_BLANK_LINES_AT_TOP = P23_BLNK_LN_AT_TOP_M1_MASK |
                                         ((((config->source.h + 1) >> 1) - 1) << 16);
     rov0->regs.P1_X_START_END         = (config->source.x << 16) |
                                         (config->source.x+config->source.w-1);
     rov0->regs.VID_BUF_PITCH0_VALUE = surface->front_buffer->video.pitch;
     
     if (DFB_PLANAR_PIXELFORMAT( surface->format )) {
          rov0->regs.P2_X_START_END       = ((config->source.x >> 1) << 16) |
                                            (((config->source.x+config->source.w)>>1) - 1);
          rov0->regs.P3_X_START_END       = rov0->regs.P2_X_START_END;
          rov0->regs.VID_BUF_PITCH1_VALUE = rov0->regs.VID_BUF_PITCH0_VALUE >> 1;
     } else {
          rov0->regs.P2_X_START_END       = rov0->regs.P1_X_START_END;
          rov0->regs.P3_X_START_END       = rov0->regs.P1_X_START_END;
          rov0->regs.VID_BUF_PITCH1_VALUE = rov0->regs.VID_BUF_PITCH0_VALUE;
     }
     
     /* set field */
     rov0->regs.AUTO_FLIP_CNTL = OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD;
     
     /* set base address */
     rov0->regs.BASE_ADDR = r200_in32( rdrv->mmio_base, DISPLAY_BASE_ADDR );
                           
     /* set offsets */
     ov0_calc_offsets( rdrv->device_data, rov0, surface, offsets );
     
     rov0->regs.VID_BUF0_BASE_ADRS =  offsets[0] & VIF_BUF0_BASE_ADRS_MASK;
     rov0->regs.VID_BUF1_BASE_ADRS = (offsets[1] & VIF_BUF1_BASE_ADRS_MASK) |
                                      VIF_BUF1_PITCH_SEL;
     rov0->regs.VID_BUF2_BASE_ADRS = (offsets[2] & VIF_BUF2_BASE_ADRS_MASK) |
                                      VIF_BUF2_PITCH_SEL; 
     rov0->regs.VID_BUF3_BASE_ADRS = rov0->regs.VID_BUF0_BASE_ADRS;
     rov0->regs.VID_BUF4_BASE_ADRS = rov0->regs.VID_BUF1_BASE_ADRS;
     rov0->regs.VID_BUF5_BASE_ADRS = rov0->regs.VID_BUF2_BASE_ADRS; 
     
     /* configure options */
     if (config->options & DLOP_SRC_COLORKEY)
          rov0->regs.KEY_CNTL |= VIDEO_KEY_FN_NE;

     if (config->options & DLOP_DST_COLORKEY)
          rov0->regs.KEY_CNTL |= GRAPHIC_KEY_FN_EQ;
     else
          rov0->regs.KEY_CNTL |= GRAPHIC_KEY_FN_TRUE;

     if (config->options & DLOP_DEINTERLACING)
          rov0->regs.SCALE_CNTL |= SCALER_ADAPTIVE_DEINT     |
                                   R200_SCALER_TEMPORAL_DEINT;
     
     rov0->regs.DEINTERLACE_PATTERN = 0x0000aaaa;
          
     /* set source colorkey */
     rov0->regs.VID_KEY_CLR_LOW  = PIXEL_RGB32( config->src_key.r,
                                                config->src_key.g,
                                                config->src_key.b );
     rov0->regs.VID_KEY_CLR_HIGH = rov0->regs.VID_KEY_CLR_LOW | 0xff000000;
     
     /* set destination colorkey */
     rov0->regs.GRPH_KEY_CLR_LOW  = ov0_calc_dstkey( config->dst_key.r,
                                                     config->dst_key.g,
                                                     config->dst_key.b );
     rov0->regs.GRPH_KEY_CLR_HIGH = PIXEL_ARGB( 0xff, 
                                                config->dst_key.r,
                                                config->dst_key.g,
                                                config->dst_key.b );
     
     /* set source format and enable overlay */
     if (config->opacity) { 
          rov0->regs.SCALE_CNTL |= SCALER_ENABLE       |
                                   SCALER_SMART_SWITCH |
                                   SCALER_DOUBLE_BUFFER;
          
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
               case DSPF_I420:
                    rov0->regs.SCALE_CNTL |= SCALER_SOURCE_YUV12;
                    break;
               case DSPF_YV12:
                    rov0->regs.SCALE_CNTL |= SCALER_SOURCE_YUV12;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    rov0->regs.SCALE_CNTL = 0;
                    break;
          }
     } else
          rov0->regs.SCALE_CNTL = 0;
}

static void 
ov0_set_regs( R200DriverData       *rdrv,
              R200OverlayLayerData *rov0 )
{
     R200DeviceData *rdev = rdrv->device_data;
     volatile __u8  *mmio = rdrv->mmio_base;
     
     r200_waitfifo( rdrv, rdev, 1 );
     r200_out32( mmio, OV0_REG_LOAD_CNTL, REG_LD_CTL_LOCK );
     while(!(r200_in32( mmio, OV0_REG_LOAD_CNTL ) & REG_LD_CTL_LOCK_READBACK));
     
     r200_waitfifo( rdrv, rdev, 7 );
     r200_out32( mmio, OV0_VID_KEY_CLR_LOW,        rov0->regs.VID_KEY_CLR_LOW );
     r200_out32( mmio, OV0_VID_KEY_CLR_HIGH,       rov0->regs.VID_KEY_CLR_HIGH );
     r200_out32( mmio, OV0_GRPH_KEY_CLR_LOW,       rov0->regs.GRPH_KEY_CLR_LOW );
     r200_out32( mmio, OV0_GRPH_KEY_CLR_HIGH,      rov0->regs.GRPH_KEY_CLR_HIGH ); 
     r200_out32( mmio, OV0_KEY_CNTL,               rov0->regs.KEY_CNTL );
     r200_out32( mmio, OV0_AUTO_FLIP_CNTL,         rov0->regs.AUTO_FLIP_CNTL );
     r200_out32( mmio, OV0_DEINTERLACE_PATTERN,    rov0->regs.DEINTERLACE_PATTERN );
     
     r200_waitfifo( rdrv, rdev, 16 );
     r200_out32( mmio, OV0_H_INC,                  rov0->regs.H_INC );
     r200_out32( mmio, OV0_STEP_BY,                rov0->regs.STEP_BY );
     r200_out32( mmio, OV0_Y_X_START,              rov0->regs.Y_X_START );
     r200_out32( mmio, OV0_Y_X_END,                rov0->regs.Y_X_END );
     r200_out32( mmio, OV0_V_INC,                  rov0->regs.V_INC );
     r200_out32( mmio, OV0_P1_BLANK_LINES_AT_TOP,  rov0->regs.P1_BLANK_LINES_AT_TOP );
     r200_out32( mmio, OV0_P23_BLANK_LINES_AT_TOP, rov0->regs.P23_BLANK_LINES_AT_TOP );
     r200_out32( mmio, OV0_VID_BUF_PITCH0_VALUE,   rov0->regs.VID_BUF_PITCH0_VALUE );
     r200_out32( mmio, OV0_VID_BUF_PITCH1_VALUE,   rov0->regs.VID_BUF_PITCH1_VALUE );
     r200_out32( mmio, OV0_P1_X_START_END,         rov0->regs.P1_X_START_END );
     r200_out32( mmio, OV0_P2_X_START_END,         rov0->regs.P2_X_START_END );
     r200_out32( mmio, OV0_P3_X_START_END,         rov0->regs.P3_X_START_END );
     r200_out32( mmio, OV0_BASE_ADDR,              rov0->regs.BASE_ADDR );
     r200_out32( mmio, OV0_P1_V_ACCUM_INIT,        rov0->regs.P1_V_ACCUM_INIT );
     r200_out32( mmio, OV0_VID_BUF0_BASE_ADRS,     rov0->regs.VID_BUF0_BASE_ADRS );
     r200_out32( mmio, OV0_VID_BUF1_BASE_ADRS,     rov0->regs.VID_BUF1_BASE_ADRS );
     r200_out32( mmio, OV0_VID_BUF2_BASE_ADRS,     rov0->regs.VID_BUF2_BASE_ADRS );
    
     r200_waitfifo( rdrv, rdev, 8 );
     r200_out32( mmio, OV0_VID_BUF3_BASE_ADRS,     rov0->regs.VID_BUF3_BASE_ADRS );
     r200_out32( mmio, OV0_VID_BUF4_BASE_ADRS,     rov0->regs.VID_BUF4_BASE_ADRS );
     r200_out32( mmio, OV0_VID_BUF5_BASE_ADRS,     rov0->regs.VID_BUF5_BASE_ADRS );
     r200_out32( mmio, OV0_P1_H_ACCUM_INIT,        rov0->regs.P1_H_ACCUM_INIT );
     r200_out32( mmio, OV0_P23_V_ACCUM_INIT,       rov0->regs.P23_V_ACCUM_INIT );
     r200_out32( mmio, OV0_P23_H_ACCUM_INIT,       rov0->regs.P23_H_ACCUM_INIT );

     r200_out32( mmio, OV0_SCALE_CNTL, rov0->regs.SCALE_CNTL );
     r200_out32( mmio, OV0_REG_LOAD_CNTL, 0 );
}

static void
ov0_swap_buffers( R200DriverData       *rdrv,
                  R200OverlayLayerData *rov0,
                  CoreSurface          *surface )
{
     R200DeviceData *rdev = rdrv->device_data;
     volatile __u8  *mmio = rdrv->mmio_base;
     __u32           offsets[3];
     
     ov0_calc_offsets( rdev, rov0, surface, offsets );
     
     /* remember last frame for deinterlacing */
     rov0->regs.VID_BUF3_BASE_ADRS = rov0->regs.VID_BUF0_BASE_ADRS;
     rov0->regs.VID_BUF4_BASE_ADRS = rov0->regs.VID_BUF1_BASE_ADRS;
     rov0->regs.VID_BUF5_BASE_ADRS = rov0->regs.VID_BUF2_BASE_ADRS;
     
     rov0->regs.VID_BUF0_BASE_ADRS =  offsets[0] & VIF_BUF0_BASE_ADRS_MASK;
     rov0->regs.VID_BUF1_BASE_ADRS = (offsets[1] & VIF_BUF1_BASE_ADRS_MASK) |
                                      VIF_BUF1_PITCH_SEL;
     rov0->regs.VID_BUF2_BASE_ADRS = (offsets[2] & VIF_BUF2_BASE_ADRS_MASK) |
                                      VIF_BUF2_PITCH_SEL;
      
     r200_waitfifo( rdrv, rdev, 1 );
     r200_out32( mmio, OV0_REG_LOAD_CNTL, REG_LD_CTL_LOCK );
     while(!(r200_in32( mmio, OV0_REG_LOAD_CNTL ) & REG_LD_CTL_LOCK_READBACK));

     r200_waitfifo( rdrv, rdev, 7 );
     r200_out32( mmio, OV0_VID_BUF0_BASE_ADRS, rov0->regs.VID_BUF0_BASE_ADRS );
     r200_out32( mmio, OV0_VID_BUF1_BASE_ADRS, rov0->regs.VID_BUF1_BASE_ADRS );
     r200_out32( mmio, OV0_VID_BUF2_BASE_ADRS, rov0->regs.VID_BUF2_BASE_ADRS );
     r200_out32( mmio, OV0_VID_BUF3_BASE_ADRS, rov0->regs.VID_BUF3_BASE_ADRS );
     r200_out32( mmio, OV0_VID_BUF4_BASE_ADRS, rov0->regs.VID_BUF4_BASE_ADRS );
     r200_out32( mmio, OV0_VID_BUF5_BASE_ADRS, rov0->regs.VID_BUF5_BASE_ADRS );

     r200_out32( mmio, OV0_REG_LOAD_CNTL, 0 );
}

static void
ov0_set_csc( R200DriverData       *rdrv,
             R200OverlayLayerData *rov0,
             float                 brightness,
             float                 contrast,
             float                 saturation,
             float                 hue )
{
     R200DeviceData *rdev = rdrv->device_data;
     volatile __u8  *mmio = rdrv->mmio_base;
     float           HueSin, HueCos; 
     float           Luma;
     float           RCb, RCr;
     float           GCb, GCr;
     float           BCb, BCr;
     float           AdjOff, ROff, GOff, BOff;
     __u32           dwLuma, dwROff, dwGOff, dwBOff;
     __u32           dwRCb, dwRCr;
     __u32           dwGCb, dwGCr;
     __u32           dwBCb, dwBCr;
     
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
 
     r200_waitfifo( rdrv, rdev, 6 );
     r200_out32( mmio, OV0_LIN_TRANS_A, dwRCb  | dwLuma );
     r200_out32( mmio, OV0_LIN_TRANS_B, dwROff | dwRCr );
     r200_out32( mmio, OV0_LIN_TRANS_C, dwGCb  | dwLuma );
     r200_out32( mmio, OV0_LIN_TRANS_D, dwGOff | dwGCr );
     r200_out32( mmio, OV0_LIN_TRANS_E, dwBCb  | dwLuma );
     r200_out32( mmio, OV0_LIN_TRANS_F, dwBOff | dwBCr );
}
