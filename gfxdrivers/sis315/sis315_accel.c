/*
 * $Id: sis315_accel.c,v 1.4 2006-10-29 23:24:50 dok Exp $
 *
 * Copyright (C) 2003 by Andreas Oberritter <obi@saftware.de>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>

#include <directfb.h>
#include <core/coredefs.h>

#include "sis315.h"
#include "sis315_mmio.h"
#include "sis315_regs.h"

static void dump_cmd(SiSDriverData *drv)
{
	int i;
	fprintf(stderr,"MMIO8200--MMIO8240 \n");
	for( i = 0x8200 ; i < 0x8240 ; i+=0x10 )
	{
		fprintf(stderr,"[%04X]: %08lX %08lX %08lX %08lX\n",i,
				sis_rl(drv->mmio_base, i),
				sis_rl(drv->mmio_base, i+4),
				sis_rl(drv->mmio_base, i+8),
				sis_rl(drv->mmio_base, i+12));
	}
}


static void sis_idle(SiSDriverData *drv)
{
	while (!(sis_rl(drv->mmio_base, SIS315_2D_CMD_QUEUE_STATUS) & 0x80000000));
}

static void sis_cmd(SiSDriverData *drv, SiSDeviceData *dev, u8 pat, u8 src, u32 type, u8 rop)
{
	sis_wl(drv->mmio_base, SIS315_2D_CMD, SIS315_2D_CMD_RECT_CLIP_EN |
					      dev->cmd_bpp | (rop << 8) |
					      pat | src | type);

	sis_wl(drv->mmio_base, SIS315_2D_FIRE_TRIGGER, 0);
	/* dump_cmd(drv); */
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
	sis_wl(drv->mmio_base, SIS315_2D_DST_Y, (dx << 16) | (dy & 0xffff) );
	sis_wl(drv->mmio_base, SIS315_2D_RECT_WIDTH, (rect->h << 16) | rect->w);

	sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
			  SIS315_2D_CMD_SRC_VIDEO,
			  dev->blit_cmd,
			  dev->blit_rop);

	return true;
}

bool sis_stretchblit(void *driver_data, void *device_data, DFBRectangle *sr, DFBRectangle *dr )
{
	SiSDriverData *drv = (SiSDriverData *)driver_data;
	SiSDeviceData *dev = (SiSDeviceData *)device_data;

	long           lDstWidth, lDstHeight, lSrcWidth, lSrcHeight;
	long           lSmallWidth, lLargeWidth, lSmallHeight, lLargeHeight;
	long           lXInitErr, lYInitErr;
	unsigned long  dst_offset, src_offset, src_pitch, dst_pitch, src_colorkey;   

	if((dr->w > 0xfff)|(dr->h > 0xfff))
		return false;

	lSrcWidth  = sr->w;
	lDstWidth  = dr->w;
	lSrcHeight = sr->h;
	lDstHeight = dr->h;

	if(lDstWidth > lSrcWidth)
	{
		lLargeWidth = lDstWidth;
		lSmallWidth = lSrcWidth;
		lXInitErr   = 3 * lSrcWidth - 2 * lDstWidth;
	}
	else
	{
		lLargeWidth = lSrcWidth;
		lSmallWidth = lDstWidth;
		lXInitErr   = lDstWidth;     //HW design
	}

	if(lDstHeight > lSrcHeight)
	{
		lLargeHeight = lDstHeight;
		lSmallHeight = lSrcHeight;
		lYInitErr    = 3 * lSrcHeight - 2 * lDstHeight;
	}
	else
	{
		lLargeHeight = lSrcHeight;
		lSmallHeight = lDstHeight;
		lYInitErr    = lDstHeight;   //HW design
	}

	src_colorkey = sis_rl(drv->mmio_base, SIS315_2D_TRANS_SRC_KEY_HIGH);

	sis_wl(drv->mmio_base, 0x8208, (sr->x << 16) | sr->y & 0xFFFF);
	sis_wl(drv->mmio_base, 0x820C, ( dr->x << 16) | dr->y & 0xFFFF);

	sis_wl(drv->mmio_base, 0x8218, (dr->h << 16) | dr->w & 0x0FFF);
	sis_wl(drv->mmio_base, 0x821c, (sr->h << 16) | sr->w & 0x0FFF);

	sis_wl(drv->mmio_base, 0x8220, ((((lSmallWidth - lLargeWidth) * 2) << 16 ) | ((lSmallWidth * 2) & 0xFFFF)));
	sis_wl(drv->mmio_base, 0x8224, ((((lSmallHeight - lLargeHeight) * 2) << 16 ) | ((lSmallHeight * 2) & 0xFFFF)));
	sis_wl(drv->mmio_base, 0x8228, ((lYInitErr << 16) | (lXInitErr & 0xFFFF)));

	if(dev->blit_rop == SIS315_ROP_AND_INVERTED_PAT) /* DSBLIT_SRC_COLORKEY */
	{
		dst_offset = sis_rl(drv->mmio_base, SIS315_2D_DST_ADDR);
		src_offset = sis_rl(drv->mmio_base, SIS315_2D_SRC_ADDR);
		src_pitch  = sis_rl(drv->mmio_base, 0x8204);
		dst_pitch  = sis_rl(drv->mmio_base, 0x8214);

		/* drv->buffer_offset reserve 1024x768x4 at driver_init_driver() in sis315.c */
		sis_wl(drv->mmio_base, SIS315_2D_DST_ADDR, drv->buffer_offset);
		sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
				  SIS315_2D_CMD_SRC_VIDEO,
				  dev->blit_cmd,
				  SIS315_ROP_COPY);

		sis_wl(drv->mmio_base, SIS315_2D_SRC_ADDR, drv->buffer_offset);
		sis_wl(drv->mmio_base, SIS315_2D_DST_ADDR, dst_offset);
		sis_wl(drv->mmio_base, 0x8204, dst_pitch);
		sis_wl(drv->mmio_base, SIS315_2D_SRC_Y, (dr->x << 16) | dr->y);
		sis_wl(drv->mmio_base, SIS315_2D_DST_Y, (dr->x << 16) | (dr->y & 0xffff));
		sis_wl(drv->mmio_base, SIS315_2D_RECT_WIDTH, (dr->h << 16) | dr->w);

		sis_wl(drv->mmio_base, SIS315_2D_TRANS_SRC_KEY_HIGH, src_colorkey);
		sis_wl(drv->mmio_base, SIS315_2D_TRANS_SRC_KEY_LOW, src_colorkey);

		sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
				  SIS315_2D_CMD_SRC_VIDEO,
				  SIS315_2D_CMD_TRANSPARENT_BITBLT,
				  dev->blit_rop);

		sis_wl(drv->mmio_base, SIS315_2D_SRC_ADDR, src_offset); /*restore*/
		sis_wl(drv->mmio_base, 0x8204, src_pitch);
	}
	else /*simple stretch bitblt */
	{
		//fprintf(stderr,"dev->blit_cmd = %x \n",dev->blit_cmd);
		sis_cmd(drv, dev, SIS315_2D_CMD_PAT_FG_REG,
				  SIS315_2D_CMD_SRC_VIDEO,
				  dev->blit_cmd,
				  dev->blit_rop);
	}

	return true;
}
