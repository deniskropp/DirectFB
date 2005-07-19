/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   Written by Vadim Catana <vcatana@registru.md>
   This file is based on sources from mach64 and ati128 drivers.

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

#include <stdio.h>
#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/layers.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include "radeon_regs.h"
#include "radeon_mmio.h"
#include "radeon.h"


typedef struct {
    CoreLayerRegionConfig config;

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
	__u32 DISPLAY_BASE_ADDR;
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
	__u32 DISP_MERGE_CONTROL;
        __u32 SCALE_CNTL;
    } regs;
} RadeonOverlayLayerData;

#define OV0_SUPPORTED_OPTIONS ( DLOP_DST_COLORKEY | DLOP_SRC_COLORKEY | DLOP_OPACITY | DLOP_ALPHACHANNEL)


static void ov_calc_scaler_regs (
	    RADEONDriverData       *rdrv,
            RadeonOverlayLayerData *rov0,
	    CoreSurface            *surface,
            CoreLayerRegionConfig  *config
	    )
{
    __u32 tmp;
    __u32 h_inc;
    __u32 step_by;

    h_inc = (surface->width << 12) / config->dest.w;

    step_by = 1;
    while (h_inc >= (2 << 12)) {
      step_by++;
      h_inc >>= 1;
    }

    /* calculate values for horizontal accumulators */
    tmp = 0x00028000 + (h_inc << 3);
    rov0->regs.P1_H_ACCUM_INIT = ((tmp <<  4) & 0x000f8000) | ((tmp << 12) & 0xf0000000);

    tmp = 0x00028000 + (h_inc << 2);
    rov0->regs.P23_H_ACCUM_INIT = ((tmp <<  4) & 0x000f8000) | ((tmp << 12) & 0x70000000);

    /* calculate values for vertical accumulators */
    tmp = 0x00018000;
    rov0->regs.P1_V_ACCUM_INIT = ((tmp << 4) & OV0_P1_V_ACCUM_INIT_MASK) | (OV0_P1_MAX_LN_IN_PER_LN_OUT & 1);

    tmp = 0x00018000;
    rov0->regs.P23_V_ACCUM_INIT = ((tmp << 4) & OV0_P23_V_ACCUM_INIT_MASK) | (OV0_P23_MAX_LN_IN_PER_LN_OUT & 1);


    rov0->regs.H_INC = h_inc | ((h_inc >> 1) << 16);

    rov0->regs.STEP_BY = step_by | (step_by << 8);

    rov0->regs.V_INC = (surface->height << 20) / config->dest.h;

    rov0->regs.Y_X_START = (config->dest.x) | (config->dest.y << 16);
    rov0->regs.Y_X_END = (config->dest.x + config->dest.w) | ((config->dest.y + config->dest.h) << 16);

    rov0->regs.P1_BLANK_LINES_AT_TOP  = P1_BLNK_LN_AT_TOP_M1_MASK  | ( (surface->height - 1) << 16);
    rov0->regs.P23_BLANK_LINES_AT_TOP = P23_BLNK_LN_AT_TOP_M1_MASK | (( ((surface->height + 1) >> 1) - 1) << 16);

    rov0->regs.P1_X_START_END = (surface->width - 1);
    rov0->regs.P2_X_START_END = ( ( surface->width >> 1 ) - 1 );
    rov0->regs.P3_X_START_END = ( ( surface->width >> 1 ) - 1 );

    rov0->regs.VID_BUF_PITCH0_VALUE   = surface->front_buffer->video.pitch;
    rov0->regs.VID_BUF_PITCH1_VALUE   = surface->front_buffer->video.pitch >> 1;

    rov0->regs.DISPLAY_BASE_ADDR = radeon_in32( rdrv->mmio_base, DISP_BASE_ADDR);
}

static void ov_calc_buffer_regs(
		RADEONDriverData       *rdrv,
                RadeonOverlayLayerData *rov0,
                CoreSurface            *surface )
{
    __u32 offset, offset_u = 0, offset_v = 0;
    SurfaceBuffer *front_buffer = surface->front_buffer;
    int cropleft = 0, croptop = 0;

    CoreLayerRegionConfig *config = &rov0->config;

    if (config->dest.x < 0)
          cropleft = -config->dest.x * surface->width / config->dest.w;

    if (config->dest.y < 0)
          croptop = -config->dest.y * surface->height / config->dest.h;

    switch (surface->format) {
          case DSPF_I420:
               cropleft &= ~15;
               croptop &= ~1;

               offset_u = front_buffer->video.offset + surface->height * front_buffer->video.pitch;
               offset_v = offset_u + (surface->height/2) * (front_buffer->video.pitch/2);
               offset_u += (croptop/2) * (front_buffer->video.pitch/2) + (cropleft/2);
               offset_v += (croptop/2) * (front_buffer->video.pitch/2) + (cropleft/2);
               break;

          case DSPF_YV12:
               cropleft &= ~15;
               croptop &= ~1;

               offset_v = front_buffer->video.offset + surface->height * front_buffer->video.pitch;
               offset_u = offset_v + (surface->height/2) * (front_buffer->video.pitch/2);
               offset_u += (croptop/2) * (front_buffer->video.pitch/2) + (cropleft/2);
               offset_v += (croptop/2) * (front_buffer->video.pitch/2) + (cropleft/2);
               break;

          default:
               break;
    }

    offset = front_buffer->video.offset;
    offset += croptop * front_buffer->video.pitch + cropleft * DFB_BYTES_PER_PIXEL( surface->format );

    rov0->regs.VID_BUF0_BASE_ADRS     = offset & VIF_BUF0_BASE_ADRS_MASK;
    rov0->regs.VID_BUF1_BASE_ADRS     = (offset_u & VIF_BUF1_BASE_ADRS_MASK) | VIF_BUF1_PITCH_SEL;
    rov0->regs.VID_BUF2_BASE_ADRS     = (offset_v & VIF_BUF2_BASE_ADRS_MASK) | VIF_BUF2_PITCH_SEL;

    rov0->regs.VID_BUF3_BASE_ADRS     = offset & VIF_BUF3_BASE_ADRS_MASK;
    rov0->regs.VID_BUF4_BASE_ADRS     = (offset_u & VIF_BUF4_BASE_ADRS_MASK) | VIF_BUF1_PITCH_SEL;
    rov0->regs.VID_BUF5_BASE_ADRS     = (offset_v & VIF_BUF5_BASE_ADRS_MASK) | VIF_BUF2_PITCH_SEL;
}

static void ov_set_buffer_regs(
		RADEONDriverData       *rdrv,
                RadeonOverlayLayerData *rov0,
                CoreSurface            *surface )
{
    RADEONDeviceData *rdev = rdrv->device_data;

    radeon_waitfifo(rdrv, rdev, 15);
    radeon_out32( rdrv->mmio_base, OV0_REG_LOAD_CNTL, REG_LD_CTL_LOCK);
    radeon_waitidle(rdrv, rdev);
    while(!(radeon_in32( rdrv->mmio_base, OV0_REG_LOAD_CNTL) & REG_LD_CTL_LOCK_READBACK));

    radeon_out32( rdrv->mmio_base, OV0_VID_BUF0_BASE_ADRS, rov0->regs.VID_BUF0_BASE_ADRS );
    radeon_out32( rdrv->mmio_base, OV0_VID_BUF1_BASE_ADRS, rov0->regs.VID_BUF1_BASE_ADRS );
    radeon_out32( rdrv->mmio_base, OV0_VID_BUF2_BASE_ADRS, rov0->regs.VID_BUF2_BASE_ADRS );

    radeon_out32( rdrv->mmio_base, OV0_VID_BUF3_BASE_ADRS, rov0->regs.VID_BUF3_BASE_ADRS );
    radeon_out32( rdrv->mmio_base, OV0_VID_BUF4_BASE_ADRS, rov0->regs.VID_BUF4_BASE_ADRS );
    radeon_out32( rdrv->mmio_base, OV0_VID_BUF5_BASE_ADRS, rov0->regs.VID_BUF5_BASE_ADRS );

    radeon_out32( rdrv->mmio_base, OV0_REG_LOAD_CNTL, 0);
}



static int
ov0LayerDataSize()
{
     return sizeof(RadeonOverlayLayerData);
}

static DFBResult
ov0InitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
    /* set capabilities and type */
    description->caps = DLCAPS_SURFACE      | DLCAPS_SCREEN_LOCATION |
                        DLCAPS_OPACITY      | DLCAPS_ALPHACHANNEL    |
			DLCAPS_SRC_COLORKEY | DLCAPS_DST_COLORKEY;

    description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;

    /* set name */
    snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Radeon Overlay" );

    /* set default configuration */
    config->flags 	= DLCONF_WIDTH | DLCONF_HEIGHT |
                          DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                          DLCONF_OPTIONS;
    config->width       = 640;
    config->height      = 480;
    config->pixelformat = DSPF_YUY2;
    config->buffermode  = DLBM_FRONTONLY;
    config->options     = DLOP_NONE;

    /* set default color adjustment */
    adjustment->flags = DCAF_NONE;

    return DFB_OK;
}

static DFBResult
ov0AddRegion( CoreLayer             *layer,
              void                  *driver_data,
              void                  *layer_data,
              void                  *region_data,
              CoreLayerRegionConfig *config )
{
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

     /* check pixel format */
     switch (config->format) {
        case DSPF_ARGB1555:
        case DSPF_RGB16:
        case DSPF_RGB32:
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
    RADEONDriverData       *rdrv = (RADEONDriverData*) driver_data;
    RADEONDeviceData       *rdev = rdrv->device_data;
    RadeonOverlayLayerData *rov0 = (RadeonOverlayLayerData*) layer_data;

    volatile __u8 *mmio = rdrv->mmio_base;

    DFBSurfacePixelFormat primary_format = dfb_primary_layer_pixelformat();

    /* save configuration */
    rov0->config = *config;

    /* set graphics and overlay blend mode */
    rov0->regs.VID_KEY_CLR_LOW  = PIXEL_RGB32( config->src_key.r, config->src_key.g, config->src_key.b );
    rov0->regs.VID_KEY_CLR_HIGH = rov0->regs.VID_KEY_CLR_LOW;

    switch (primary_format) {
        case DSPF_ARGB1555:
    	    rov0->regs.GRPH_KEY_CLR_LOW = PIXEL_ARGB1555( config->dst_key.a, config->dst_key.r, config->dst_key.g, config->dst_key.b );
            break;

        case DSPF_RGB16:
            rov0->regs.GRPH_KEY_CLR_LOW = PIXEL_RGB16( config->dst_key.r, config->dst_key.g, config->dst_key.b );
            break;

        case DSPF_RGB32:
            rov0->regs.GRPH_KEY_CLR_LOW = PIXEL_RGB32( config->dst_key.r, config->dst_key.g, config->dst_key.b );
            break;

        case DSPF_ARGB:
            rov0->regs.GRPH_KEY_CLR_LOW = PIXEL_ARGB( config->dst_key.a, config->dst_key.r, config->dst_key.g, config->dst_key.b );
            break;

        default:
            D_BUG("unexpected pixelformat");
    	    return DFB_UNSUPPORTED;	
    }

    rov0->regs.GRPH_KEY_CLR_HIGH = rov0->regs.GRPH_KEY_CLR_LOW;


    rov0->regs.DISP_MERGE_CONTROL = 0xffff0001;
    rov0->regs.KEY_CNTL = 0;

    if (config->options & DLOP_SRC_COLORKEY) {
	rov0->regs.DISP_MERGE_CONTROL = 0xffff0000;
	rov0->regs.KEY_CNTL = VIDEO_KEY_FN_NE;
    }

    if (config->options & DLOP_DST_COLORKEY) {
	rov0->regs.DISP_MERGE_CONTROL = 0xffff0000;
	rov0->regs.KEY_CNTL = GRAPHIC_KEY_FN_EQ;
    }

    if (config->options & DLOP_OPACITY) {
	rov0->regs.DISP_MERGE_CONTROL = 0x00ff0002 | ( (__u32)config->opacity << 24);
	rov0->regs.KEY_CNTL = VIDEO_KEY_FN_FALSE | GRAPHIC_KEY_FN_FALSE;
    }

    if (config->options & DLOP_ALPHACHANNEL) {
    	rov0->regs.DISP_MERGE_CONTROL = 0xffff0001;
	rov0->regs.KEY_CNTL = VIDEO_KEY_FN_FALSE | GRAPHIC_KEY_FN_FALSE;
    }


    ov_calc_scaler_regs ( rdrv, rov0, surface, config );
    ov_calc_buffer_regs ( rdrv, rov0, surface );


    rov0->regs.SCALE_CNTL = SCALER_ENABLE |
			    SCALER_SMART_SWITCH |
			    SCALER_Y2R_TEMP |
			    SCALER_PIX_EXPAND |
			    SCALER_DOUBLE_BUFFER_REGS;

    /* set pixel format */
    switch (surface->format) {
        case DSPF_ARGB1555:
            rov0->regs.SCALE_CNTL |= SCALER_SOURCE_15BPP;
            break;

        case DSPF_RGB16:
            rov0->regs.SCALE_CNTL |= SCALER_SOURCE_16BPP;
            break;

        case DSPF_RGB32:
            rov0->regs.SCALE_CNTL |= SCALER_SOURCE_32BPP;
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
    	    D_BUG("unexpected pixelformat");
    	    rov0->regs.SCALE_CNTL = 0;
    	    return DFB_UNSUPPORTED;
    }


    /* reset overlay */
    radeon_waitidle(rdrv, rdev);

    if(rdev->chipset == R_100 || rdev->chipset == R_120 || rdev->chipset == R_150)
    {
	radeon_out32( mmio, OV0_LIN_TRANS_A, 0x12A00000);
	radeon_out32( mmio, OV0_LIN_TRANS_B, 0x199018FE);
	radeon_out32( mmio, OV0_LIN_TRANS_C, 0x12A0F9B0);
	radeon_out32( mmio, OV0_LIN_TRANS_D, 0xF2F0043B);
	radeon_out32( mmio, OV0_LIN_TRANS_E, 0x12A02050);
	radeon_out32( mmio, OV0_LIN_TRANS_F, 0x0000174E);
    } else {
	radeon_out32( mmio, OV0_LIN_TRANS_A, 0x12a20000);
	radeon_out32( mmio, OV0_LIN_TRANS_B, 0x198a190e);
	radeon_out32( mmio, OV0_LIN_TRANS_C, 0x12a2f9da);
	radeon_out32( mmio, OV0_LIN_TRANS_D, 0xf2fe0442);
	radeon_out32( mmio, OV0_LIN_TRANS_E, 0x12a22046);
	radeon_out32( mmio, OV0_LIN_TRANS_F, 0x0000175f);
    }

    radeon_out32( mmio, OV0_SCALE_CNTL,	SCALER_SOFT_RESET );
    radeon_out32( mmio, OV0_EXCLUSIVE_HORZ, 0 );
    radeon_out32( mmio, OV0_AUTO_FLIP_CNTL, 0 );
    radeon_out32( mmio, OV0_FILTER_CNTL, FILTER_HARDCODED_COEF );
    radeon_out32( mmio, OV0_KEY_CNTL, GRAPHIC_KEY_FN_EQ);
    radeon_out32( mmio, OV0_TEST, 0 );

    /* set registers */
    radeon_waitfifo(rdrv, rdev, 2);
    radeon_out32( mmio, OV0_REG_LOAD_CNTL, REG_LD_CTL_LOCK );
    radeon_waitidle(rdrv, rdev);
    while (!(radeon_in32( mmio, OV0_REG_LOAD_CNTL ) & REG_LD_CTL_LOCK_READBACK));
    radeon_waitfifo(rdrv, rdev, 15);

    /* Shutdown capturing */
    radeon_out32( mmio, FCP_CNTL, FCP_CNTL__GND);
    radeon_out32( mmio, CAP0_TRIG_CNTL, 0);
    radeon_out32( mmio, VID_BUFFER_CONTROL, (1<<16) | 0x01);
    radeon_out32( mmio, DISP_TEST_DEBUG_CNTL, 0);

    radeon_out32( mmio, OV0_AUTO_FLIP_CNTL,OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD);

    radeon_waitfifo(rdrv, rdev, 2);
    radeon_out32( mmio, DISP_MERGE_CNTL,		rov0->regs.DISP_MERGE_CONTROL);
    radeon_out32( mmio, OV0_VID_KEY_CLR_LOW,		rov0->regs.VID_KEY_CLR_LOW);
    radeon_out32( mmio, OV0_VID_KEY_CLR_HIGH,		rov0->regs.VID_KEY_CLR_HIGH);
    radeon_out32( mmio, OV0_GRPH_KEY_CLR_LOW,		rov0->regs.GRPH_KEY_CLR_LOW);
    radeon_out32( mmio, OV0_GRPH_KEY_CLR_HIGH,		rov0->regs.GRPH_KEY_CLR_HIGH);
    radeon_out32( mmio, OV0_KEY_CNTL,	  		rov0->regs.KEY_CNTL);

    radeon_out32( mmio, OV0_H_INC,			rov0->regs.H_INC );
    radeon_out32( mmio, OV0_STEP_BY,			rov0->regs.STEP_BY );
    radeon_out32( mmio, OV0_Y_X_START,			rov0->regs.Y_X_START );
    radeon_out32( mmio, OV0_Y_X_END,			rov0->regs.Y_X_END );
    radeon_out32( mmio, OV0_V_INC,			rov0->regs.V_INC );
    radeon_out32( mmio, OV0_P1_BLANK_LINES_AT_TOP,	rov0->regs.P1_BLANK_LINES_AT_TOP );
    radeon_out32( mmio, OV0_P23_BLANK_LINES_AT_TOP,	rov0->regs.P23_BLANK_LINES_AT_TOP );
    radeon_out32( mmio, OV0_VID_BUF_PITCH0_VALUE,	rov0->regs.VID_BUF_PITCH0_VALUE );
    radeon_out32( mmio, OV0_VID_BUF_PITCH1_VALUE,	rov0->regs.VID_BUF_PITCH1_VALUE );
    radeon_out32( mmio, OV0_P1_X_START_END,		rov0->regs.P1_X_START_END );
    radeon_out32( mmio, OV0_P2_X_START_END,		rov0->regs.P2_X_START_END );
    radeon_out32( mmio, OV0_P3_X_START_END,		rov0->regs.P3_X_START_END );
    radeon_out32( mmio, OV0_BASE_ADDR,			rov0->regs.DISPLAY_BASE_ADDR);

    radeon_out32( mmio, OV0_VID_BUF0_BASE_ADRS,		rov0->regs.VID_BUF0_BASE_ADRS );
    radeon_out32( mmio, OV0_VID_BUF1_BASE_ADRS,		rov0->regs.VID_BUF1_BASE_ADRS );
    radeon_out32( mmio, OV0_VID_BUF2_BASE_ADRS,		rov0->regs.VID_BUF2_BASE_ADRS );

    radeon_waitfifo(rdrv, rdev, 9);
    radeon_out32( mmio, OV0_VID_BUF3_BASE_ADRS,		rov0->regs.VID_BUF3_BASE_ADRS );
    radeon_out32( mmio, OV0_VID_BUF4_BASE_ADRS,		rov0->regs.VID_BUF4_BASE_ADRS );
    radeon_out32( mmio, OV0_VID_BUF5_BASE_ADRS,		rov0->regs.VID_BUF5_BASE_ADRS );

    radeon_out32( mmio, OV0_P1_V_ACCUM_INIT,		rov0->regs.P1_V_ACCUM_INIT );
    radeon_out32( mmio, OV0_P1_H_ACCUM_INIT,		rov0->regs.P1_H_ACCUM_INIT );
    radeon_out32( mmio, OV0_P23_V_ACCUM_INIT,		rov0->regs.P23_V_ACCUM_INIT );
    radeon_out32( mmio, OV0_P23_H_ACCUM_INIT,		rov0->regs.P23_H_ACCUM_INIT );


    /* enable overlay */
    radeon_out32( mmio, OV0_SCALE_CNTL, rov0->regs.SCALE_CNTL );

    radeon_out32( mmio, OV0_REG_LOAD_CNTL, 0 );

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
    RADEONDriverData       *rdrv = (RADEONDriverData*) driver_data;
    RadeonOverlayLayerData *rov0 = (RadeonOverlayLayerData*) layer_data;

    dfb_surface_flip_buffers( surface, false );

    ov_calc_buffer_regs( rdrv, rov0, surface );
    ov_set_buffer_regs( rdrv, rov0, surface );

    return DFB_OK;
}

static DFBResult
ov0RemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
    RADEONDriverData       *rdrv = (RADEONDriverData*) driver_data;
    RadeonOverlayLayerData *rov0 = (RadeonOverlayLayerData*) layer_data;

    /* disable overlay */
    rov0->regs.SCALE_CNTL &= ~SCALER_ENABLE;
    radeon_out32( rdrv->mmio_base, OV0_SCALE_CNTL, rov0->regs.SCALE_CNTL );

    return DFB_OK;
}

DisplayLayerFuncs RadeonOverlayFuncs = {
     LayerDataSize:      ov0LayerDataSize,
     InitLayer:          ov0InitLayer,
     AddRegion:          ov0AddRegion,
     TestRegion:         ov0TestRegion,
     SetRegion:          ov0SetRegion,
     RemoveRegion:       ov0RemoveRegion,
     FlipRegion:         ov0FlipRegion
};
