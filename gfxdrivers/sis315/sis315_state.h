/*
 * $Id: sis315_state.h,v 1.1 2003-11-25 10:53:48 oberritter Exp $
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

#ifndef _SIS315_STATE_H
#define _SIS315_STATE_H

void sis_validate_color(SiSDriverData *drv, SiSDeviceData *dev, CardState *state);
void sis_validate_dst(SiSDriverData *drv, SiSDeviceData *dev, CardState *state);
void sis_validate_src(SiSDriverData *drv, SiSDeviceData *dev, CardState *state);
void sis_set_dst_colorkey(SiSDriverData *drv, SiSDeviceData *dev, CardState *state);
void sis_set_src_colorkey(SiSDriverData *drv, SiSDeviceData *dev, CardState *state);
void sis_set_blittingflags(SiSDeviceData *dev, CardState *state);
void sis_set_clip(SiSDriverData *drv, DFBRegion *clip);

#endif /* _SIS315_STATE_H */
