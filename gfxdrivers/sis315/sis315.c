/*
 * $Id: sis315.c,v 1.1 2003-11-06 18:50:11 oberritter Exp $
 *
 * Copyright (C) 2003 by Andreas Oberritter <obi@saftware.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/fb.h>
#include <stdio.h>

#include <directfb.h>

#include <core/gfxcard.h>
#include <core/graphics_driver.h>
#include <core/state.h>
#include <core/surfaces.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include "sis315_mmio.h"
#include "sis315_regs.h"

DFB_GRAPHICS_DRIVER(sis315);

#define SIS_SUPPORTED_DRAWING_FUNCTIONS	\
	(DFXL_FILLRECTANGLE | DFXL_DRAWLINE)

#define SIS_SUPPORTED_DRAWING_FLAGS	\
	(DSDRAW_NOFX)

#define SIS_SUPPORTED_BLITTING_FUNCTIONS \
	(DFXL_BLIT)

#define SIS_SUPPORTED_BLITTING_FLAGS \
	(DSBLIT_NOFX)

typedef struct {
	volatile __u8 *mmio_base;
	int cmd_queue_len;
} SiSDriverData;

typedef struct {
	/* state validation */
	int v_blit;
	int v_color;
	int v_src;
	int v_dst;

	/* stored values */
	int cmd_bpp;
	int color;
	int src_offset;
	int src_pitch;
	int dst_offset;
	int dst_pitch;
} SiSDeviceData;

static __u16 dspfToSrcColor(DFBSurfacePixelFormat pf)
{
	switch (DFB_BITS_PER_PIXEL(pf)) {
	case 16:
		return 0x8000;
	case 32:
		return 0xc000;
	default:
		return 0x0000;
	}
}

static __u32 dspfToCmdBpp(DFBSurfacePixelFormat pf)
{
	switch (DFB_BITS_PER_PIXEL(pf)) {
	case 16:
		return SIS315_2D_CMD_CFB_16;
	case 32:
		return SIS315_2D_CMD_CFB_32;
	default:
		return SIS315_2D_CMD_CFB_8;
	}
}

static void sis_idle(SiSDriverData *drv)
{
	drv->cmd_queue_len = ((512 * 1024) / 4) - 64;

	while ((sis_rw(drv->mmio_base, SIS315_2D_CMD_QUEUE_STATUS + 2) & 0x8000) != 0x8000);
}

static void sis_cmd_queue_wait(SiSDriverData *drv, int count)
{
	if (drv->cmd_queue_len < count)
		sis_idle(drv);

	drv->cmd_queue_len -= count;
}

static void sis_validate_color(SiSDriverData *drv, SiSDeviceData *dev,
			       CardState *state)
{
	__u32 color;

	if (dev->v_color)
		return;

	switch (state->destination->format) {
	case DSPF_LUT8:
		color = state->color_index;
		break;
	case DSPF_ARGB1555:
		color = PIXEL_ARGB1555(state->color.a,
					state->color.r,
					state->color.g,
					state->color.b);
		break;
	case DSPF_RGB16:
		color = PIXEL_RGB16(state->color.r,
					 state->color.g,
					 state->color.b);
		break;
	case DSPF_RGB32:
		color = PIXEL_RGB32(state->color.r,
					 state->color.g,
					 state->color.b);
		break;
	case DSPF_ARGB:
		color = PIXEL_ARGB(state->color.a,
					state->color.r,
					state->color.g,
					state->color.b);
		break;
	default:
		BUG("unexpected pixelformat");
		return;
	}

	sis_cmd_queue_wait(drv, 1);

	sis_wl(drv->mmio_base, SIS315_2D_PAT_FG_COLOR, color);

	dev->v_color = 1;
}

static void sis_validate_dst(SiSDriverData *drv, SiSDeviceData *dev,
			     CardState *state, GraphicsDeviceFuncs *funcs)
{
	CoreSurface *dst = state->destination;
	SurfaceBuffer *buf = dst->back_buffer;

	if (dev->v_dst)
		return;

	dev->cmd_bpp = dspfToCmdBpp(dst->format);

	sis_cmd_queue_wait(drv, 3);

	sis_wl(drv->mmio_base, SIS315_2D_DST_ADDR, buf->video.offset);
	sis_ww(drv->mmio_base, SIS315_2D_DST_PITCH, buf->video.pitch);
	sis_ww(drv->mmio_base, SIS315_2D_DST_HEIGHT, 0xffff);

	dev->v_dst = 1;
}

static void sis_validate_src(SiSDriverData *drv, SiSDeviceData *dev,
			     CardState *state)
{
	CoreSurface *src = state->source;
	SurfaceBuffer *buf = src->front_buffer;

	if (dev->v_src)
		return;

	sis_cmd_queue_wait(drv, 3);

	sis_wl(drv->mmio_base, SIS315_2D_SRC_ADDR, buf->video.offset);
	sis_ww(drv->mmio_base, SIS315_2D_SRC_PITCH, buf->video.pitch);
	sis_ww(drv->mmio_base, SIS315_2D_AGP_BASE, dspfToSrcColor(src->format));

	dev->v_src = 1;
}

static void sis_validate_clip(SiSDriverData *drv, SiSDeviceData *dev,
			      DFBRegion *clip)
{
	sis_cmd_queue_wait(drv, 4);

	sis_ww(drv->mmio_base, SIS315_2D_LEFT_CLIP, clip->x1);
	sis_ww(drv->mmio_base, SIS315_2D_TOP_CLIP, clip->y1);
	sis_ww(drv->mmio_base, SIS315_2D_RIGHT_CLIP, clip->x2);
	sis_ww(drv->mmio_base, SIS315_2D_BOT_CLIP, clip->y2);
}

static void sis_engine_sync(void *driver_data, void *device_data)
{
	sis_idle(driver_data);
}

static void sis_check_state(void *driver_data, void *device_data,
			    CardState *state, DFBAccelerationMask accel)
{
	switch (state->destination->format) {
	case DSPF_LUT8:
	case DSPF_ARGB1555:
	case DSPF_RGB16:
	case DSPF_RGB32:
	case DSPF_ARGB:
		break;
	default:
		return;
	}

	if (DFB_DRAWING_FUNCTION(accel)) {
		if (state->drawingflags & ~SIS_SUPPORTED_DRAWING_FLAGS)
			return;
		state->accel |= SIS_SUPPORTED_DRAWING_FUNCTIONS;
	}
	else {
		if (state->blittingflags & ~SIS_SUPPORTED_BLITTING_FLAGS)
			return;

		switch (state->source->format) {
		case DSPF_LUT8:
		case DSPF_RGB16:
			break;
		default:
			return;
		}

		state->accel |= SIS_SUPPORTED_BLITTING_FUNCTIONS;
	}
}


static void sis_set_state(void *driver_data, void *device_data,
			  GraphicsDeviceFuncs *funcs, CardState *state,
			  DFBAccelerationMask accel)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;
	SiSDeviceData *dev = (SiSDeviceData *)device_data;

	if (state->modified) {
		if (state->modified & SMF_DESTINATION)
			dev->v_color = dev->v_dst = 0;
		else if (state->modified & SMF_COLOR)
			dev->v_color = 0;

		if (state->modified & SMF_SOURCE)
			dev->v_src = 0;

		if (state->modified & SMF_BLITTING_FLAGS)
			dev->v_blit = 0;
	}

	sis_validate_dst(drv, dev, state, funcs);

	switch (accel) {
	case DFXL_FILLRECTANGLE:
	case DFXL_DRAWLINE:
		sis_validate_color(drv, dev, state);
		state->set = SIS_SUPPORTED_DRAWING_FUNCTIONS;
		break;
	case DFXL_BLIT:
	case DFXL_STRETCHBLIT:
		sis_validate_src(drv, dev, state);
		state->set = SIS_SUPPORTED_BLITTING_FUNCTIONS;
		break;
	default:
		BUG("unexpected drawing or blitting function");
		break;
	}

	if (state->modified & SMF_CLIP)
		sis_validate_clip(drv, dev, &state->clip);

	state->modified = 0;
}

static void sis_cmd(SiSDriverData *drv, SiSDeviceData *dev, __u8 pat, __u8 src, __u8 type, __u8 rop)
{
	sis_cmd_queue_wait(drv, 2);

	sis_wl(drv->mmio_base, SIS315_2D_CMD, dev->cmd_bpp | (rop << 8) | pat | src | type);
	sis_wl(drv->mmio_base, SIS315_2D_FIRE_TRIGGER, 0);
}

static bool sis_fill_rectangle(void *driver_data, void *device_data,
			       DFBRectangle *rect)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;
	SiSDeviceData *dev = (SiSDeviceData *)device_data;

	sis_cmd_queue_wait(drv, 4);

	sis_ww(drv->mmio_base, SIS315_2D_DST_Y, rect->y);
	sis_ww(drv->mmio_base, SIS315_2D_DST_X, rect->x);
	sis_ww(drv->mmio_base, SIS315_2D_RECT_WIDTH, rect->w);
	sis_ww(drv->mmio_base, SIS315_2D_RECT_HEIGHT, rect->h);

	sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
			  SIS315_2D_CMD_SRC_VIDEO,
			  SIS315_2D_CMD_BITBLT,
			  SIS315_ROP_COPY_PAT);

	return true;
}

static bool sis_draw_line(void *driver_data, void *device_data,
			  DFBRegion *line)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;
	SiSDeviceData *dev = (SiSDeviceData *)device_data;

	sis_cmd_queue_wait(drv, 5);

	sis_ww(drv->mmio_base, SIS315_2D_LINE_X0, line->x1);
	sis_ww(drv->mmio_base, SIS315_2D_LINE_Y0, line->y1);
	sis_ww(drv->mmio_base, SIS315_2D_LINE_X1, line->x2);
	sis_ww(drv->mmio_base, SIS315_2D_LINE_Y1, line->y2);
	sis_ww(drv->mmio_base, SIS315_2D_LINE_COUNT, 1);

	sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
			  SIS315_2D_CMD_SRC_VIDEO,
			  SIS315_2D_CMD_LINE_DRAW,
			  SIS315_ROP_COPY_PAT);

	return true;
}

static bool sis_blit(void *driver_data, void *device_data,
		     DFBRectangle *rect, int dx, int dy)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;
	SiSDeviceData *dev = (SiSDeviceData *)device_data;

	sis_cmd_queue_wait(drv, 6);

	sis_ww(drv->mmio_base, SIS315_2D_SRC_Y, rect->y);
	sis_ww(drv->mmio_base, SIS315_2D_SRC_X, rect->x);
	sis_ww(drv->mmio_base, SIS315_2D_DST_Y, dy);
	sis_ww(drv->mmio_base, SIS315_2D_DST_X, dx);
	sis_ww(drv->mmio_base, SIS315_2D_RECT_WIDTH, rect->w);
	sis_ww(drv->mmio_base, SIS315_2D_RECT_HEIGHT, rect->h);
	
	sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
			  SIS315_2D_CMD_SRC_VIDEO,
			  SIS315_2D_CMD_BITBLT,
			  SIS315_ROP_COPY);

	return true;
}

static bool sis_stretch_blit(void *driver_data, void *device_data,
			     DFBRectangle *sr, DFBRectangle *dr)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;
	SiSDeviceData *dev = (SiSDeviceData *)device_data;

	sis_cmd_queue_wait(drv, 8);

	sis_ww(drv->mmio_base, SIS315_2D_SRC_Y, sr->y);
	sis_ww(drv->mmio_base, SIS315_2D_SRC_X, sr->x);
	sis_ww(drv->mmio_base, SIS315_2D_DST_Y, dr->y);
	sis_ww(drv->mmio_base, SIS315_2D_DST_X, dr->x);
	//sis_ww(drv->mmio_base, SIS315_2D_DST_PITCH, dr->w);	/* FIXME: wrong register? */
	//sis_ww(drv->mmio_base, SIS315_2D_DST_HEIGHT, dr->h);	/* FIXME: wrong register? */
	sis_ww(drv->mmio_base, SIS315_2D_RECT_WIDTH, sr->w);
	sis_ww(drv->mmio_base, SIS315_2D_RECT_HEIGHT, sr->h);

	sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
			  SIS315_2D_CMD_SRC_VIDEO,
			  SIS315_2D_CMD_STRETCH_BITBLT,
			  SIS315_ROP_COPY);

	return true;
}


/*
 * exported symbols...
 */

static int driver_probe(GraphicsDevice *device)
{
	switch (dfb_gfxcard_get_accelerator(device)) {
	case FB_ACCEL_SIS_GLAMOUR_2:	/* SiS 315, 650, 661, 740 */
		return 1;
	}

	return 0;
}

static void driver_get_info(GraphicsDevice *device,
			    GraphicsDriverInfo *info)
{
	snprintf(info->name, DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
			"SiS 315 Driver");
	snprintf(info->vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
			"Andreas Oberritter <obi@saftware.de>");

	info->version.major = 0;
	info->version.minor = 1;

	info->driver_data_size = sizeof(SiSDriverData);
	info->device_data_size = sizeof(SiSDeviceData);
}

static DFBResult driver_init_driver(GraphicsDevice *device,
				    GraphicsDeviceFuncs *funcs,
				    void *driver_data,
				    void *device_data)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;

	drv->mmio_base = (volatile __u8 *)dfb_gfxcard_map_mmio(device, 0, -1);
	if (!drv->mmio_base)
		return DFB_IO;

	funcs->EngineSync = sis_engine_sync;
	funcs->CheckState = sis_check_state;
	funcs->SetState = sis_set_state;

	/* drawing functions */
	funcs->FillRectangle = sis_fill_rectangle;
	funcs->DrawLine = sis_draw_line;

	/* blitting functions */
	funcs->Blit = sis_blit;
	funcs->StretchBlit = sis_stretch_blit;

	return DFB_OK;
}

static DFBResult driver_init_device(GraphicsDevice *device,
				    GraphicsDeviceInfo *device_info,
				    void *driver_data,
				    void *device_data)
{
	snprintf(device_info->name,
			DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "315");
	snprintf(device_info->vendor,
			DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "SiS");

	device_info->caps.flags = CCF_CLIPPING;
	device_info->caps.accel = SIS_SUPPORTED_DRAWING_FUNCTIONS |
				SIS_SUPPORTED_BLITTING_FUNCTIONS;
	device_info->caps.drawing = SIS_SUPPORTED_DRAWING_FLAGS;
	device_info->caps.blitting = SIS_SUPPORTED_BLITTING_FLAGS;

	device_info->limits.surface_byteoffset_alignment = 32 * 4;
	device_info->limits.surface_pixelpitch_alignment = 32;

	return DFB_OK;
}

static void driver_close_device(GraphicsDevice *device,
				void *driver_data,
				void *device_data)
{
}

static void driver_close_driver(GraphicsDevice *device,
				void *driver_data)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;

	dfb_gfxcard_unmap_mmio(device, drv->mmio_base, -1);
}

