/*
 * $Id: sis315_accel.h,v 1.1 2003-11-25 10:53:48 oberritter Exp $
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

#ifndef _SIS315_ACCEL_H
#define _SIS315_ACCEL_H

bool sis_fill_rectangle(void *driver_data, void *device_data, DFBRectangle *rect);
bool sis_draw_rectangle(void *driver_data, void *device_data, DFBRectangle *rect);
bool sis_draw_line(void *driver_data, void *device_data, DFBRegion *line);
bool sis_blit(void *driver_data, void *device_data, DFBRectangle *rect, int dx, int dy);

#endif /* _SIS315_ACCEL_H */
