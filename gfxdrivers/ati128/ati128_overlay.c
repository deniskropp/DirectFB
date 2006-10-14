/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <config.h>

#include <stdio.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/layers.h>
#include <core/surfaces.h>

#include "regs.h"
#include "mmio.h"
#include "ati128.h"

typedef struct {
     CoreLayerRegionConfig config;

     /* overlay registers */
     struct {
          u32 H_INC;
          u32 STEP_BY;
          u32 Y_X_START;
          u32 Y_X_END;
          u32 V_INC;
          u32 P1_BLANK_LINES_AT_TOP;
          u32 P23_BLANK_LINES_AT_TOP;
          u32 VID_BUF_PITCH0_VALUE;
          u32 VID_BUF_PITCH1_VALUE;
          u32 P1_X_START_END;
          u32 P2_X_START_END;
          u32 P3_X_START_END;
          u32 VID_BUF0_BASE_ADRS;
          u32 VID_BUF1_BASE_ADRS;
          u32 VID_BUF2_BASE_ADRS;
          u32 P1_V_ACCUM_INIT;
          u32 P23_V_ACCUM_INIT;
          u32 P1_H_ACCUM_INIT;
          u32 P23_H_ACCUM_INIT;
          u32 SCALE_CNTL;
     } regs;
} ATIOverlayLayerData;

static void ov0_set_regs( ATI128DriverData *adrv, ATIOverlayLayerData *aov0 );
static void ov0_calc_regs( ATI128DriverData *adrv, ATIOverlayLayerData *aov0,
                           CoreLayerRegionConfig *config, CoreSurface *surface );

#define OV0_SUPPORTED_OPTIONS   (DLOP_NONE)

/**********************/

static int
ov0LayerDataSize()
{
     return sizeof(ATIOverlayLayerData);
}

static DFBResult
ov0InitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
     ATI128DriverData *adrv = (ATI128DriverData*) driver_data;
     volatile u8      *mmio = adrv->mmio_base;

     /* set capabilities and type */
     description->caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE;
     description->type = DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "ATI128 Overlay" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;
     config->width       = 640;
     config->height      = 480;
     config->pixelformat = DSPF_YUY2;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;

     /* fill out default color adjustment,
        only fields set in flags will be accepted from applications */
     adjustment->flags = DCAF_NONE;

     /* reset overlay */
     ati128_out32( mmio, OV0_SCALE_CNTL, 0x80000000 );
     ati128_out32( mmio, OV0_EXCLUSIVE_HORZ, 0 );
     ati128_out32( mmio, OV0_AUTO_FLIP_CNTL, 0 );
     ati128_out32( mmio, OV0_FILTER_CNTL, 0x0000000f );
     ati128_out32( mmio, OV0_COLOR_CNTL, 0x00101000 );
     ati128_out32( mmio, OV0_KEY_CNTL, 0x10 );
     ati128_out32( mmio, OV0_TEST, 0 );

     return DFB_OK;
}


static void
ov0OnOff( ATI128DriverData    *adrv,
          ATIOverlayLayerData *aov0,
          int                  on )
{
     /* set/clear enable bit */
     if (on)
          aov0->regs.SCALE_CNTL |= R128_SCALER_ENABLE;
     else
          aov0->regs.SCALE_CNTL &= ~R128_SCALER_ENABLE;

     /* write back to card */
     ati128_out32( adrv->mmio_base, OV0_SCALE_CNTL, aov0->regs.SCALE_CNTL );
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

     /* check pixel format */
     switch (config->format) {
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               break;

          default:
               fail |= CLRCF_FORMAT;
     }

     /* check width */
     if (config->width > 2048 || config->width < 1)
          fail |= CLRCF_WIDTH;

     /* check height */
     if (config->height > 1024 || config->height < 1)
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
     ATI128DriverData    *adrv = (ATI128DriverData*) driver_data;
     ATIOverlayLayerData *aov0 = (ATIOverlayLayerData*) layer_data;

     /* remember configuration */
     aov0->config = *config;

     ov0_calc_regs( adrv, aov0, config, surface );
     ov0_set_regs( adrv, aov0 );

     /* enable overlay */
     ov0OnOff( adrv, aov0, 1 );

     return DFB_OK;
}

static DFBResult
ov0RemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
     ATI128DriverData    *adrv = (ATI128DriverData*) driver_data;
     ATIOverlayLayerData *aov0 = (ATIOverlayLayerData*) layer_data;

     /* disable overlay */
     ov0OnOff( adrv, aov0, 0 );

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
     ATI128DriverData    *adrv = (ATI128DriverData*) driver_data;
     ATIOverlayLayerData *aov0 = (ATIOverlayLayerData*) layer_data;

     dfb_surface_flip_buffers( surface, false );

     ov0_calc_regs( adrv, aov0, &aov0->config, surface );
     ov0_set_regs( adrv, aov0 );

     return DFB_OK;
}


DisplayLayerFuncs atiOverlayFuncs = {
     LayerDataSize:      ov0LayerDataSize,
     InitLayer:          ov0InitLayer,

     TestRegion:         ov0TestRegion,
     SetRegion:          ov0SetRegion,
     RemoveRegion:       ov0RemoveRegion,
     FlipRegion:         ov0FlipRegion
};

/* internal */

static void ov0_set_regs( ATI128DriverData *adrv, ATIOverlayLayerData *aov0 )
{
     volatile u8 *mmio = adrv->mmio_base;

     ati128_out32( mmio, OV0_REG_LOAD_CNTL, 1 );
     while (!(ati128_in32( mmio, OV0_REG_LOAD_CNTL ) & (1 << 3)));

     ati128_out32( mmio, OV0_H_INC,
                   aov0->regs.H_INC );

     ati128_out32( mmio, OV0_STEP_BY,
                   aov0->regs.STEP_BY );

     ati128_out32( mmio, OV0_Y_X_START,
                   aov0->regs.Y_X_START );

     ati128_out32( mmio, OV0_Y_X_END,
                   aov0->regs.Y_X_END );

     ati128_out32( mmio, OV0_V_INC,
                   aov0->regs.V_INC );

     ati128_out32( mmio, OV0_P1_BLANK_LINES_AT_TOP,
                   aov0->regs.P1_BLANK_LINES_AT_TOP );

     ati128_out32( mmio, OV0_P23_BLANK_LINES_AT_TOP,
                   aov0->regs.P23_BLANK_LINES_AT_TOP );

     ati128_out32( mmio, OV0_VID_BUF_PITCH0_VALUE,
                   aov0->regs.VID_BUF_PITCH0_VALUE );

     ati128_out32( mmio, OV0_VID_BUF_PITCH1_VALUE,
                   aov0->regs.VID_BUF_PITCH1_VALUE );

     ati128_out32( mmio, OV0_P1_X_START_END,
                   aov0->regs.P1_X_START_END );

     ati128_out32( mmio, OV0_P2_X_START_END,
                   aov0->regs.P2_X_START_END );

     ati128_out32( mmio, OV0_P3_X_START_END,
                   aov0->regs.P3_X_START_END );

     ati128_out32( mmio, OV0_VID_BUF0_BASE_ADRS,
                   aov0->regs.VID_BUF0_BASE_ADRS );

     ati128_out32( mmio, OV0_VID_BUF1_BASE_ADRS,
                   aov0->regs.VID_BUF1_BASE_ADRS );

     ati128_out32( mmio, OV0_VID_BUF2_BASE_ADRS,
                   aov0->regs.VID_BUF2_BASE_ADRS );

     ati128_out32( mmio, OV0_P1_V_ACCUM_INIT,
                   aov0->regs.P1_V_ACCUM_INIT );

     ati128_out32( mmio, OV0_P23_V_ACCUM_INIT,
                   aov0->regs.P23_V_ACCUM_INIT );

     ati128_out32( mmio, OV0_P1_H_ACCUM_INIT,
                   aov0->regs.P1_H_ACCUM_INIT );

     ati128_out32( mmio, OV0_P23_H_ACCUM_INIT,
                   aov0->regs.P23_H_ACCUM_INIT );

     ati128_out32( mmio, OV0_SCALE_CNTL,
                   aov0->regs.SCALE_CNTL );

     ati128_out32( mmio, OV0_REG_LOAD_CNTL, 0 );
}

static void ov0_calc_regs( ATI128DriverData      *adrv,
                           ATIOverlayLayerData   *aov0,
                           CoreLayerRegionConfig *config,
                           CoreSurface           *surface )
{
     int h_inc, v_inc, step_by, tmp;
     int p1_h_accum_init, p23_h_accum_init;
     int p1_v_accum_init, p23_v_accum_init;

     DFBRegion      dstBox;
     int            dst_w;
     int            dst_h;
     u32            offset_u = 0, offset_v = 0;

     SurfaceBuffer *front_buffer = surface->front_buffer;


     /* destination box */
     dstBox.x1 = config->dest.x;
     dstBox.y1 = config->dest.y;
     dstBox.x2 = config->dest.x + config->dest.w;
     dstBox.y2 = config->dest.y + config->dest.h;

     /* destination size */
     dst_w = config->dest.w;
     dst_h = config->dest.h;

     /* clear everything but the enable bit that may be set*/
     aov0->regs.SCALE_CNTL &= R128_SCALER_ENABLE;


     /* calculate incrementors */
     h_inc   = (surface->width  << 12) / dst_w;
     v_inc   = (surface->height << 20) / dst_h;
     step_by = 1;

     while (h_inc >= (2 << 12)) {
          step_by++;
          h_inc >>= 1;
     }

     /* calculate values for horizontal accumulators */
     tmp = 0x00028000 + (h_inc << 3);
     p1_h_accum_init = ((tmp <<  4) & 0x000f8000) | ((tmp << 12) & 0xf0000000);

     tmp = 0x00028000 + (h_inc << 2);
     p23_h_accum_init = ((tmp <<  4) & 0x000f8000) | ((tmp << 12) & 0x70000000);

     /* calculate values for vertical accumulators */
     tmp = 0x00018000;
     p1_v_accum_init = ((tmp << 4) & 0x03ff8000) | 0x00000001;

     tmp = 0x00018000;
     p23_v_accum_init = ((tmp << 4) & 0x01ff8000) | 0x00000001;

     /* choose pixel format and calculate buffer offsets for planar modes */
     switch (surface->format) {
          case DSPF_UYVY:
               aov0->regs.SCALE_CNTL = R128_SCALER_SOURCE_YVYU422;
               break;

          case DSPF_YUY2:
               aov0->regs.SCALE_CNTL = R128_SCALER_SOURCE_VYUY422;
               break;

          case DSPF_I420:
               aov0->regs.SCALE_CNTL = R128_SCALER_SOURCE_YUV12;

               offset_u = front_buffer->video.offset +
                          surface->height * front_buffer->video.pitch;
               offset_v = offset_u +
                          (surface->height >> 1) * (front_buffer->video.pitch >> 1);
               break;

          case DSPF_YV12:
               aov0->regs.SCALE_CNTL = R128_SCALER_SOURCE_YUV12;

               offset_v = front_buffer->video.offset +
                          surface->height * front_buffer->video.pitch;
               offset_u = offset_v +
                          (surface->height >> 1) * (front_buffer->video.pitch >> 1);
               break;

          default:
               D_BUG("unexpected pixelformat");
               aov0->regs.SCALE_CNTL = 0;
               return;
     }

     aov0->regs.SCALE_CNTL            |= R128_SCALER_DOUBLE_BUFFER |
                                         R128_SCALER_BURST_PER_PLANE |
                                         R128_SCALER_Y2R_TEMP |
                                         R128_SCALER_PIX_EXPAND;

     aov0->regs.H_INC                  = h_inc | ((h_inc >> 1) << 16);
     aov0->regs.V_INC                  = v_inc;
     aov0->regs.STEP_BY                = step_by | (step_by << 8);
     aov0->regs.Y_X_START              = dstBox.x1 | (dstBox.y1 << 16);
     aov0->regs.Y_X_END                = dstBox.x2 | (dstBox.y2 << 16);
     aov0->regs.P1_BLANK_LINES_AT_TOP  = 0x00000fff | ((surface->height - 1) << 16);
     aov0->regs.P23_BLANK_LINES_AT_TOP = 0x000007ff | ((((surface->height + 1) >> 1) - 1) << 16);
     aov0->regs.VID_BUF_PITCH0_VALUE   = front_buffer->video.pitch;
     aov0->regs.VID_BUF_PITCH1_VALUE   = front_buffer->video.pitch >> 1;
     aov0->regs.P1_X_START_END         = surface->width - 1;
     aov0->regs.P2_X_START_END         = (surface->width >> 1) - 1;
     aov0->regs.P3_X_START_END         = (surface->width >> 1) - 1;
     aov0->regs.VID_BUF0_BASE_ADRS     = front_buffer->video.offset & 0x03fffff0;
     aov0->regs.VID_BUF1_BASE_ADRS     = (offset_u & 0x03fffff0) | 1;
     aov0->regs.VID_BUF2_BASE_ADRS     = (offset_v & 0x03fffff0) | 1;
     aov0->regs.P1_H_ACCUM_INIT        = p1_h_accum_init;
     aov0->regs.P23_H_ACCUM_INIT       = p23_h_accum_init;
     aov0->regs.P1_V_ACCUM_INIT        = p1_v_accum_init;
     aov0->regs.P23_V_ACCUM_INIT       = p23_v_accum_init;
}

