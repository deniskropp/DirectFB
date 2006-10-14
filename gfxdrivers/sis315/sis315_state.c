/*
 * $Id: sis315_state.c,v 1.5 2006-10-14 13:05:37 dok Exp $
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

#include <config.h>

#include <directfb.h>

#include <direct/messages.h>

#include <core/state.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include "sis315.h"
#include "sis315_mmio.h"
#include "sis315_regs.h"
#include "sis315_state.h"

static u16 dspfToSrcColor(DFBSurfacePixelFormat pf)
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

static u32 dspfToCmdBpp(DFBSurfacePixelFormat pf)
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

void sis_validate_color(SiSDriverData *drv, SiSDeviceData *dev, CardState *state)
{
	u32 color;

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
		D_BUG("unexpected pixelformat");
		return;
	}

	sis_wl(drv->mmio_base, SIS315_2D_PAT_FG_COLOR, color);

	dev->v_color = 1;
}

void sis_validate_dst(SiSDriverData *drv, SiSDeviceData *dev, CardState *state)
{
	CoreSurface *dst = state->destination;
	SurfaceBuffer *buf = dst->back_buffer;

	if (dev->v_destination)
		return;

	dev->cmd_bpp = dspfToCmdBpp(dst->format);

	sis_wl(drv->mmio_base, SIS315_2D_DST_ADDR, buf->video.offset);
	sis_wl(drv->mmio_base, SIS315_2D_DST_PITCH, (0xffff << 16) | buf->video.pitch);

	dev->v_destination = 1;
}

void sis_validate_src(SiSDriverData *drv, SiSDeviceData *dev, CardState *state)
{
	CoreSurface *src = state->source;
	SurfaceBuffer *buf = src->front_buffer;

	if (dev->v_source)
		return;

	sis_wl(drv->mmio_base, SIS315_2D_SRC_ADDR, buf->video.offset);
	sis_wl(drv->mmio_base, SIS315_2D_SRC_PITCH, (dspfToSrcColor(src->format) << 16) | buf->video.pitch);

	dev->v_source = 1;
}

void sis_set_dst_colorkey(SiSDriverData *drv, SiSDeviceData *dev, CardState *state)
{
	if (dev->v_dst_colorkey)
		return;

	sis_wl(drv->mmio_base, SIS315_2D_TRANS_DEST_KEY_HIGH, state->dst_colorkey);
	sis_wl(drv->mmio_base, SIS315_2D_TRANS_DEST_KEY_LOW, state->dst_colorkey);

	dev->v_dst_colorkey = 1;
}

void sis_set_src_colorkey(SiSDriverData *drv, SiSDeviceData *dev, CardState *state)
{
	if (dev->v_src_colorkey)
		return;

	sis_wl(drv->mmio_base, SIS315_2D_TRANS_SRC_KEY_HIGH, state->src_colorkey);
	sis_wl(drv->mmio_base, SIS315_2D_TRANS_SRC_KEY_LOW, state->src_colorkey);

	dev->v_src_colorkey = 1;
}

void sis_set_blittingflags(SiSDeviceData *dev, CardState *state)
{
	if (dev->v_blittingflags)
		return;

	if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
		dev->blit_cmd = SIS315_2D_CMD_TRANSPARENT_BITBLT;
		dev->blit_rop = SIS315_ROP_AND_INVERTED_PAT;
	}
	else {
		dev->blit_cmd = SIS315_2D_CMD_BITBLT;
		dev->blit_rop = SIS315_ROP_COPY;
	}

	dev->v_blittingflags = 1;
}

void sis_set_clip(SiSDriverData *drv, DFBRegion *clip)
{
	sis_wl(drv->mmio_base, SIS315_2D_LEFT_CLIP, (clip->y1 << 16) | clip->x1);
	sis_wl(drv->mmio_base, SIS315_2D_RIGHT_CLIP, (clip->y2 << 16) | clip->x2);
}

