/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI R100 based chipsets written by
 *             Claudio Ciccani <klan@users.sf.net>.  
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __R100_STATE_H__
#define __R100_STATE_H__

#include "r100.h"


void r100_set_destination   ( R100DriverData *rdrv,
                              R100DeviceData *rdev,
                              CardState      *state );
void r100_set_source        ( R100DriverData *rdrv,
                              R100DeviceData *rdev,
                              CardState      *state );
void r100_set_clip          ( R100DriverData *rdrv,
                              R100DeviceData *rdev,
                              CardState      *state );
void r100_set_drawing_color ( R100DriverData *rdrv,
                              R100DeviceData *rdev,
                              CardState      *state );
void r100_set_blitting_color( R100DriverData *rdrv,
                              R100DeviceData *rdev,
                              CardState      *state );
void r100_set_src_colorkey  ( R100DriverData *rdrv,
                              R100DeviceData *rdev,
                              CardState      *state );
void r100_set_blend_function( R100DriverData *rdrv,
                              R100DeviceData *rdev,
                              CardState      *state );
void r100_set_drawingflags  ( R100DriverData *rdrv,
                              R100DeviceData *rdev,
                              CardState      *state );
void r100_set_blittingflags ( R100DriverData *rdrv,
                              R100DeviceData *rdev,
                              CardState      *state );

#endif /* __R100_STATE_H__ */
