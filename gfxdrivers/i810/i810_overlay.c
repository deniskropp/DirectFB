/*
   i810_overlay.c -- Video Overlay Support (based partly from
                     XFree86 i810_video.c)

   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

   Written by Antonino Daplas <adaplas@pol.net>

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

#include <sys/ioctl.h>

#include <string.h>

#include <core/coredefs.h>
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/screens.h>
#include <core/screen.h>

#include <fbdev/fbdev.h>

#include <direct/mem.h>

#include <gfx/convert.h>

#include "i810.h"


/*
 * OV0CMD - Overlay Command Register
 */
#define	VERTICAL_CHROMINANCE_FILTER 	0x70000000
#define VC_SCALING_OFF		0x00000000
#define VC_LINE_REPLICATION	0x10000000
#define VC_UP_INTERPOLATION	0x20000000
#define VC_PIXEL_DROPPING	0x50000000
#define VC_DOWN_INTERPOLATION	0x60000000
#define VERTICAL_LUMINANCE_FILTER	0x0E000000
#define VL_SCALING_OFF		0x00000000
#define VL_LINE_REPLICATION	0x02000000
#define VL_UP_INTERPOLATION	0x04000000
#define VL_PIXEL_DROPPING	0x0A000000
#define VL_DOWN_INTERPOLATION	0x0C000000
#define	HORIZONTAL_CHROMINANCE_FILTER 	0x01C00000
#define HC_SCALING_OFF		0x00000000
#define HC_LINE_REPLICATION	0x00400000
#define HC_UP_INTERPOLATION	0x00800000
#define HC_PIXEL_DROPPING	0x01400000
#define HC_DOWN_INTERPOLATION	0x01800000
#define HORIZONTAL_LUMINANCE_FILTER	0x00380000
#define HL_SCALING_OFF		0x00000000
#define HL_LINE_REPLICATION	0x00080000
#define HL_UP_INTERPOLATION	0x00100000
#define HL_PIXEL_DROPPING	0x00280000
#define HL_DOWN_INTERPOLATION	0x00300000

#define Y_ADJUST		0x00010000	
#define OV_BYTE_ORDER		0x0000C000
#define UV_SWAP			0x00004000
#define Y_SWAP			0x00008000
#define Y_AND_UV_SWAP		0x0000C000
#define SOURCE_FORMAT		0x00003C00
#define	RGB_555			0x00000800
#define	RGB_565			0x00000C00
#define	YUV_422			0x00002000
#define	YUV_411			0x00002400
#define	YUV_420			0x00003000
#define	YUV_410			0x00003800
#define FIELD_MODE              0x00000020
#define FRAME_MODE              0x00000000
#define BUFFER_AND_FIELD	0x00000006
#define	BUFFER0_FIELD0		0x00000000
#define	BUFFER1_FIELD0		0x00000004
#define OVERLAY_ENABLE		0x00000001

#define UV_VERT_BUF1 		0x02
#define UV_VERT_BUF0 		0x04

#define SRC_CONSTANT_ALPHA_BLEND  1 << 31
#define MINUV_SCALE	        0x1

#define I810FB_IOC_UPDATEOVERLAY       _IOW ('F', 0xF7, struct i810_ovl_regs)
#define I810FB_IOC_UPDATEOVERLAYCMD    _IOW ('F', 0xF6, int)

extern u32 i810_wait_for_space(I810DriverData *i810drv,
			       I810DeviceData *i810dev,
			       u32             space   );

#define I810_OVERLAY_SUPPORTED_OPTIONS (DLOP_DST_COLORKEY | DLOP_DEINTERLACING)

static void ovl_calc_regs (I810DriverData        *i810drv,
                           I810OverlayLayerData  *i810ovl,
                           CoreLayer             *layer,
                           CoreSurface           *surface,
                           CoreLayerRegionConfig *config );

static void update_overlay(I810DriverData       *i810drv,
                           I810DeviceData       *i810dev)
{
	i810_writel(i810drv->mmio_base, OV0ADDR, i810dev->ovl_mem.physical);
}
	
void
i810ovlOnOff( I810DriverData *idrv,
              I810DeviceData *idev,
              bool            on )
{
     if (on)
          idrv->oregs->ov0cmd |= 1;
     else
          idrv->oregs->ov0cmd &= ~1;

     update_overlay( idrv, idev );
}

static int
ovlLayerDataSize()
{
	return sizeof(I810OverlayLayerData);
}

static DFBResult
ovlInitLayer(
              CoreLayer                  *layer,
	      void                       *driver_data,
	      void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
	I810OverlayLayerData *i810ovl = (I810OverlayLayerData *) layer_data;
	I810DriverData *idrv = driver_data;
    I810DeviceData *idev = idrv->idev;
	
    idev->iovl = i810ovl;

    idrv->oregs = (volatile struct i810_ovl_regs*) idrv->ovl_base;

    memset( (void*) idrv->oregs, 0, sizeof(struct i810_ovl_regs) );

	/* set_capabilities */
	description->caps = DLCAPS_SURFACE | DLCAPS_SCREEN_LOCATION |
		DLCAPS_BRIGHTNESS | DLCAPS_CONTRAST | DLCAPS_SATURATION |
		DLCAPS_DST_COLORKEY | DLCAPS_OPACITY | DLCAPS_DEINTERLACING;

	description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;
	/* set name */
	snprintf( description->name,
		  DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Intel 810/815 Overlay" );

	/* fill out the default configuration */
	config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
		DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;
	config->width       = 640;
	config->height      = 480;
	config->pixelformat = DSPF_YUY2;
	config->buffermode  = DLBM_FRONTONLY;
	config->options     = DLOP_NONE;

	/* fill out default color adjustment,
	   only fields set in flags will be accepted from applications */
	adjustment->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST | DCAF_SATURATION;
	adjustment->brightness = 0x8000;
	adjustment->contrast   = 0x8000;
	adjustment->saturation = 0x8000;

	idrv->oregs->yrgb_vph  = 0;
	idrv->oregs->uv_vph    = 0;
	idrv->oregs->horz_ph   = 0;
	idrv->oregs->init_ph   = 0;
	idrv->oregs->dwinpos   = 0;
	idrv->oregs->dwinsz    = (640 << 16) | 480;
	idrv->oregs->swid      = 640 | (640 << 15);
	idrv->oregs->swidqw    = (640 >> 3) | (640 << 12);
	idrv->oregs->sheight   = 480 | (480 << 15);
	idrv->oregs->yrgbscale = 0x80004000; /* scale factor 1 */
	idrv->oregs->uvscale   = 0x80004000; /* scale factor 1 */
	idrv->oregs->ov0clrc0  = 0x4000; /* brightness: 0 contrast: 1.0 */
	idrv->oregs->ov0clrc1  = 0x80; /* saturation: bypass */
	
	idrv->oregs->sclrkvh = 0;
	idrv->oregs->sclrkvl = 0;
	idrv->oregs->sclrkm  = 0; /* source color key disable */
	idrv->oregs->ov0conf = 0; /* two 720 pixel line buffers */
	
	idrv->oregs->ov0cmd = VC_UP_INTERPOLATION | HC_UP_INTERPOLATION | Y_ADJUST | YUV_420;
	
	update_overlay( idrv, idev );
	
	/*
	 * FIXME: If the fence registers are enabled, then the buffer pointers
	 *        require specific alignment.  This is a problem with planar formats
	 *        which have separate pointers for each of the U and V planes.  Packed
	 *        formats should not be a problem.
	 */
	i810ovl->planar_bug = 0;
	if (i810_readl(idrv->mmio_base, FENCE) & 1)
		i810ovl->planar_bug = 1;

	return DFB_OK;
}

static DFBResult
ovlTestRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags *failed )
{
	DFBDisplayLayerConfigFlags fail = 0;
	I810OverlayLayerData *i810ovl = (I810OverlayLayerData *) layer_data;

	if (config->options & ~I810_OVERLAY_SUPPORTED_OPTIONS)
		fail |= DLCONF_OPTIONS;

	switch (config->format) {
	case DSPF_I420:
	case DSPF_YV12:
	case DSPF_YUY2:
	case DSPF_UYVY:
		break;
	default:
		fail |= DLCONF_PIXELFORMAT;
	}
	if (i810ovl->planar_bug && (config->format == DSPF_I420 ||
				    config->format == DSPF_YV12    )) {
		D_DEBUG("Sorry, planar formats will not work when memory tiling "
			 "is enabled\n");
		fail |= DLCONF_PIXELFORMAT;
	}

	if (config->width > 1440 || config->width < 1)
		fail |= DLCONF_WIDTH;

	if (config->height > 1023 || config->height < 1)
		fail |= DLCONF_HEIGHT;

	if (failed)
		*failed = fail;

	if (fail)
		return DFB_UNSUPPORTED;

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
	I810DriverData       *i810drv = (I810DriverData *) driver_data;
	I810OverlayLayerData *i810ovl = (I810OverlayLayerData *) layer_data;

	i810ovl->config = *config;

	ovl_calc_regs (i810drv, i810ovl, layer, surface, config);
	update_overlay(i810drv, i810drv->idev);

	i810ovlOnOff(i810drv, i810drv->idev, 1);

	return DFB_OK;
}

static DFBResult
ovlRemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
	I810DriverData *i810drv = (I810DriverData *) driver_data;

	/* disable overlay */
	i810ovlOnOff( i810drv, i810drv->idev, 0 );

	return DFB_OK;
}

static DFBResult
ovlFlipRegion(  CoreLayer           *layer,
                void                *driver_data,
                void                *layer_data,
                void                *region_data,
                CoreSurface         *surface,
                DFBSurfaceFlipFlags  flags )
{
	I810DriverData       *i810drv = (I810DriverData *) driver_data;
	I810OverlayLayerData *i810ovl = (I810OverlayLayerData *) layer_data;
	u32 current_buffer;

	dfb_surface_flip_buffers( surface, false );

	/* select buffer */
	current_buffer =  (i810drv->oregs->ov0cmd & 4) >> 2;

	if (current_buffer) {
		i810drv->oregs->ov0cmd &= ~4;
	}
	else {
		i810drv->oregs->ov0cmd |= 4;
	}

	ovl_calc_regs (i810drv, i810ovl, layer, surface, &i810ovl->config);
	update_overlay(i810drv, i810drv->idev);
	
	if (flags & DSFLIP_WAIT)
		dfb_screen_wait_vsync( dfb_screens_at( DSCID_PRIMARY ) );

	return DFB_OK;
}

static DFBResult
ovlSetColorAdjustment( CoreLayer          *layer,
                       void               *driver_data,
                       void               *layer_data,
                       DFBColorAdjustment *adj )
{
	I810DriverData *i810drv = (I810DriverData*) driver_data;
	
	i810drv->oregs->ov0clrc0 = (((adj->brightness >> 8) - 128) & 0xFF) |
		                  ((adj->contrast >> 9) << 8);
	i810drv->oregs->ov0clrc1 = (adj->saturation >> 8) & 0xFF;

	update_overlay(i810drv, i810drv->idev);
	return DFB_OK;
}
	
static DFBResult
ovlSetInputField( CoreLayer *layer,
		  void      *driver_data,
		  void      *layer_data,
		  void      *region_data,
		  int        field )
{
	I810DriverData *i810drv = (I810DriverData*) driver_data;
	
	i810drv->oregs->ov0cmd &= ~2;
	i810drv->oregs->ov0cmd |= (field) ? 2 : 0;
	
	update_overlay(i810drv, i810drv->idev);
	return DFB_OK;
}

DisplayLayerFuncs i810OverlayFuncs = {
     LayerDataSize:      ovlLayerDataSize,
     InitLayer:          ovlInitLayer,
     TestRegion:         ovlTestRegion,
     SetRegion:          ovlSetRegion,
     RemoveRegion:       ovlRemoveRegion,
     FlipRegion:         ovlFlipRegion,
     SetColorAdjustment: ovlSetColorAdjustment,
     SetInputField:      ovlSetInputField

};


static void ovl_calc_regs (I810DriverData        *i810drv,
			   I810OverlayLayerData  *i810ovl,
			   CoreLayer             *layer,
			   CoreSurface           *surface,
			   CoreLayerRegionConfig *config )
{
	u32 swidth = 0, y_offset, v_offset = 0, u_offset = 0;
	u32 drw_w, src_w, drw_h, src_h, xscaleInt, xscaleFract, yscaleInt;
	u32 xscaleFractUV = 0, xscaleIntUV, yscaleIntUV = 0, yscaleFract, yscaleFractUV = 0;

	SurfaceBuffer *front_buffer = surface->front_buffer;

        DFBSurfacePixelFormat primary_format;

	drw_w = config->dest.w;
	drw_h = config->dest.h;
	src_w = surface->width;
	src_h = surface->height;

	if (config->options & DLOP_DEINTERLACING)
		src_h >>= 1;

        /* reset command register except the enable bit and buffer select bits */
	i810drv->oregs->ov0cmd &= 7;

	/* Set source dimension in bytes */
	switch (surface->format) {
	case DSPF_I420:
	case DSPF_YV12:
		swidth = (src_w + 7) & ~7;
		i810drv->oregs->swid = (swidth << 15) | swidth;
		i810drv->oregs->swidqw = (swidth << 12) | (swidth >> 3);
		break;
	case DSPF_UYVY:
	case DSPF_YUY2:
		swidth = ((src_w + 3) & ~3) << 1;
		i810drv->oregs->swid = swidth;
		i810drv->oregs->swidqw = swidth >> 3;
		break;
	default:
		break;
	}
	i810drv->oregs->sheight = src_h | (src_h << 15);

	/* select buffer size */
	if (swidth > 720)
		i810drv->oregs->ov0conf = 1;
	else
		i810drv->oregs->ov0conf = 0;

	/* set dest window position and dimension */
	i810drv->oregs->dwinpos = (config->dest.y << 16) | config->dest.x;
	i810drv->oregs->dwinsz = (drw_h << 16) | drw_w;

	/* Set buffer pointers */
	y_offset = (dfb_gfxcard_memory_physical(NULL, front_buffer->video.offset));

	switch (surface->format) {
	case DSPF_I420:
		u_offset = y_offset + surface->height * front_buffer->video.pitch;
		v_offset = u_offset + ((surface->height >> 1) * (front_buffer->video.pitch >> 1));
		break;
	case DSPF_YV12:
		v_offset = y_offset + surface->height * front_buffer->video.pitch;
		u_offset = v_offset + ((surface->height >> 1) * (front_buffer->video.pitch >> 1));
		break;
	default:
		break;
	}

	if (i810drv->oregs->ov0cmd & 4) {
		i810drv->oregs->obuf_1y = y_offset;
		i810drv->oregs->obuf_1v = v_offset;
		i810drv->oregs->obuf_1u = u_offset;
	}
	else {
		i810drv->oregs->obuf_0y = y_offset;
		i810drv->oregs->obuf_0v = v_offset;
		i810drv->oregs->obuf_0u = u_offset;
	}

	/* set scaling */
	i810drv->oregs->yrgbscale = 0x80004000;
	i810drv->oregs->uvscale = 0x80004000;

	i810drv->oregs->ov0cmd |= VC_UP_INTERPOLATION | HC_UP_INTERPOLATION | Y_ADJUST | FRAME_MODE;

	if (config->options & DLOP_DEINTERLACING)
		i810drv->oregs->ov0cmd |= FIELD_MODE;

	if ((drw_w != src_w) || (drw_h != src_h))
	{
		xscaleInt = (src_w / drw_w) & 0x3;
		xscaleFract = (src_w << 12) / drw_w;
		yscaleInt = (src_h / drw_h) & 0x3;
		yscaleFract = (src_h << 12) / drw_h;

		i810drv->oregs->yrgbscale = (xscaleInt << 15) |
			((xscaleFract & 0xFFF) << 3) |
			(yscaleInt) |
			((yscaleFract & 0xFFF) << 20);

		if (drw_w > src_w)
		{
			i810drv->oregs->ov0cmd &= ~HORIZONTAL_CHROMINANCE_FILTER;
			i810drv->oregs->ov0cmd &= ~HORIZONTAL_LUMINANCE_FILTER;
			i810drv->oregs->ov0cmd |= (HC_UP_INTERPOLATION | HL_UP_INTERPOLATION);
		}

		if (drw_h > src_h)
		{
			i810drv->oregs->ov0cmd &= ~VERTICAL_CHROMINANCE_FILTER;
			i810drv->oregs->ov0cmd &= ~VERTICAL_LUMINANCE_FILTER;
			i810drv->oregs->ov0cmd |= (VC_UP_INTERPOLATION | VL_UP_INTERPOLATION);
		}

		if (drw_w < src_w)
		{
			i810drv->oregs->ov0cmd &= ~HORIZONTAL_CHROMINANCE_FILTER;
			i810drv->oregs->ov0cmd &= ~HORIZONTAL_LUMINANCE_FILTER;
			i810drv->oregs->ov0cmd |= (HC_DOWN_INTERPOLATION | HL_DOWN_INTERPOLATION);
		}
		
		if (drw_h < src_h)
		{
			i810drv->oregs->ov0cmd &= ~VERTICAL_CHROMINANCE_FILTER;
			i810drv->oregs->ov0cmd &= ~VERTICAL_LUMINANCE_FILTER;
			i810drv->oregs->ov0cmd |= (VC_DOWN_INTERPOLATION | VL_DOWN_INTERPOLATION);
		}

		if (xscaleFract)
		{
			xscaleFractUV = xscaleFract >> MINUV_SCALE;
			i810drv->oregs->ov0cmd &= ~HC_DOWN_INTERPOLATION;
			i810drv->oregs->ov0cmd |= HC_UP_INTERPOLATION;
		}
		
		if (xscaleInt)
		{
			xscaleIntUV = xscaleInt >> MINUV_SCALE;
			if (xscaleIntUV)
			{
				i810drv->oregs->ov0cmd &= ~HC_UP_INTERPOLATION;
			}
		}
		
		if (yscaleFract)
		{
			yscaleFractUV = yscaleFract >> MINUV_SCALE;
			i810drv->oregs->ov0cmd &= ~VC_DOWN_INTERPOLATION;
			i810drv->oregs->ov0cmd |= VC_UP_INTERPOLATION;
		}
		
		if (yscaleInt)
		{
			yscaleIntUV = yscaleInt >> MINUV_SCALE;
			if (yscaleIntUV)
			{
				i810drv->oregs->ov0cmd &= ~VC_UP_INTERPOLATION;
				i810drv->oregs->ov0cmd |= VC_DOWN_INTERPOLATION;
			}
		}
		
		i810drv->oregs->uvscale = yscaleIntUV | ((xscaleFractUV & 0xFFF) << 3) |
	                   ((yscaleFractUV & 0xFFF) << 20);
	}
	
	switch(surface->format) {
	case DSPF_YV12:
	case DSPF_I420:
		/* set UV vertical phase to -0.25 */
		i810drv->oregs->uv_vph = 0x30003000;
		i810drv->oregs->init_ph = UV_VERT_BUF0 | UV_VERT_BUF1;
		i810drv->oregs->ov0stride = (front_buffer->video.pitch) | (front_buffer->video.pitch << 15);
		i810drv->oregs->ov0cmd &= ~SOURCE_FORMAT;
		i810drv->oregs->ov0cmd |= YUV_420;
		break;
	case DSPF_UYVY:
	case DSPF_YUY2:
		i810drv->oregs->uv_vph = 0;
		i810drv->oregs->init_ph = 0;
		i810drv->oregs->ov0stride = front_buffer->video.pitch;
		i810drv->oregs->ov0cmd &= ~SOURCE_FORMAT;
		i810drv->oregs->ov0cmd |= YUV_422;
		i810drv->oregs->ov0cmd &= ~OV_BYTE_ORDER;
		if (surface->format == DSPF_UYVY)
			i810drv->oregs->ov0cmd |= Y_SWAP;
		break;
	default:
		D_BUG("unexpected pixelformat");
		break;
	}

	/* Set alpha window */
	i810drv->oregs->awinpos = i810drv->oregs->dwinpos;
	i810drv->oregs->awinsz = i810drv->oregs->dwinsz;


    /*
     * Destination color keying.
     */

    primary_format = dfb_primary_layer_pixelformat();

    i810drv->oregs->dclrkv = dfb_color_to_pixel( primary_format,
                                                 config->dst_key.r,
                                                 config->dst_key.g,
                                                 config->dst_key.b );

    i810drv->oregs->dclrkm = (1 << DFB_COLOR_BITS_PER_PIXEL( primary_format )) - 1;

    if (config->options & DLOP_DST_COLORKEY)
         i810drv->oregs->dclrkm |= 0x80000000;
}
