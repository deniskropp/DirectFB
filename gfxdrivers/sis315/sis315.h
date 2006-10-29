/*
 * $Id: sis315.h,v 1.5 2006-10-29 23:24:50 dok Exp $
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

#ifndef _SIS315_H
#define _SIS315_H

#include <direct/types.h>

typedef struct {
	volatile u8 *mmio_base;
	bool has_auto_maximize;
	unsigned long auto_maximize;
	/* ioctls */
	int get_info;
	int get_automaximize;
	int set_automaximize;
} SiSDriverData;

typedef struct {
	/* state validation */
	int v_blittingflags;
	int v_color;
	int v_destination;
	int v_source;
	int v_dst_colorkey;
	int v_src_colorkey;

	/* stored values */
	int blit_cmd;
	int blit_rop;
	int cmd_bpp;
	int color;
	int src_offset;
	int src_pitch;
	int dst_offset;
	int dst_pitch;
} SiSDeviceData;

#endif /* _SIS315_H */
