/*
 * $Id: sis315_accel.c,v 1.3 2006-10-14 13:05:37 dok Exp $
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
#include <core/coredefs.h>

#include "sis315.h"
#include "sis315_mmio.h"
#include "sis315_regs.h"

static void sis_idle(SiSDriverData *drv)
{
	while (!(sis_rl(drv->mmio_base, SIS315_2D_CMD_QUEUE_STATUS) & 0x80000000));
}

static void sis_cmd(SiSDriverData *drv, SiSDeviceData *dev, u8 pat, u8 src, u8 type, u8 rop)
{
	sis_wl(drv->mmio_base, SIS315_2D_CMD, SIS315_2D_CMD_RECT_CLIP_EN |
					      dev->cmd_bpp | (rop << 8) |
					      pat | src | type);

	sis_wl(drv->mmio_base, SIS315_2D_FIRE_TRIGGER, 0);

	sis_idle(drv);
}

bool sis_fill_rectangle(void *driver_data, void *device_data, DFBRectangle *rect)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;
	SiSDeviceData *dev = (SiSDeviceData *)device_data;

	sis_wl(drv->mmio_base, SIS315_2D_DST_Y, (rect->x << 16) | rect->y);
	sis_wl(drv->mmio_base, SIS315_2D_RECT_WIDTH, (rect->h << 16) | rect->w);

	sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
			  SIS315_2D_CMD_SRC_VIDEO,
			  SIS315_2D_CMD_BITBLT,
			  SIS315_ROP_COPY_PAT);

	return true;
}

bool sis_draw_rectangle(void *driver_data, void *device_data, DFBRectangle *rect)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;
	SiSDeviceData *dev = (SiSDeviceData *)device_data;

	/* from top left ... */
	sis_wl(drv->mmio_base, SIS315_2D_LINE_X0, (rect->y << 16) | rect->x);
	/* ... to top right ... */
	sis_wl(drv->mmio_base, SIS315_2D_LINE_X1, (rect->y << 16) | (rect->x + rect->w - 1));
	/* ... to bottom right ... */
	sis_wl(drv->mmio_base, SIS315_2D_LINE_X(2), ((rect->y + rect->h - 1) << 16) | (rect->x + rect->w - 1));
	/* ... to bottom left ... */
	sis_wl(drv->mmio_base, SIS315_2D_LINE_X(3), ((rect->y + rect->h - 1) << 16) | rect->x);
	/* ... and back to top left */
	sis_wl(drv->mmio_base, SIS315_2D_LINE_X(4), ((rect->y + 1) << 16) | rect->x);

	sis_wl(drv->mmio_base, SIS315_2D_LINE_COUNT, 4);

	sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
			  SIS315_2D_CMD_SRC_VIDEO,
			  SIS315_2D_CMD_LINE_DRAW,
			  SIS315_ROP_COPY_PAT);

	return true;
}

bool sis_draw_line(void *driver_data, void *device_data, DFBRegion *line)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;
	SiSDeviceData *dev = (SiSDeviceData *)device_data;

	sis_wl(drv->mmio_base, SIS315_2D_LINE_X0, (line->y1 << 16) | line->x1);
	sis_wl(drv->mmio_base, SIS315_2D_LINE_X1, (line->y2 << 16) | line->x2);
	sis_wl(drv->mmio_base, SIS315_2D_LINE_COUNT, 1);

	sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
			  SIS315_2D_CMD_SRC_VIDEO,
			  SIS315_2D_CMD_LINE_DRAW,
			  SIS315_ROP_COPY_PAT);

	return true;
}

bool sis_blit(void *driver_data, void *device_data, DFBRectangle *rect, int dx, int dy)
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;
	SiSDeviceData *dev = (SiSDeviceData *)device_data;

	sis_wl(drv->mmio_base, SIS315_2D_SRC_Y, (rect->x << 16) | rect->y);
	sis_wl(drv->mmio_base, SIS315_2D_DST_Y, (dx << 16) | dy);
	sis_wl(drv->mmio_base, SIS315_2D_RECT_WIDTH, (rect->h << 16) | rect->w);

	sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
			  SIS315_2D_CMD_SRC_VIDEO,
			  dev->blit_cmd,
			  dev->blit_rop);

	return true;
}

