/*
 * Copyright (C) 2005 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI R200 based chipsets written by
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

#ifndef __R200_STATE_H__
#define __R200_STATE_H__

#include "r200.h"


void r200_set_destination   ( R200DriverData *rdrv,
                              R200DeviceData *rdev,
                              CardState      *state );
void r200_set_source        ( R200DriverData *rdrv,
                              R200DeviceData *rdev,
                              CardState      *state );
void r200_set_clip          ( R200DriverData *rdrv,
                              R200DeviceData *rdev,
                              CardState      *state );
void r200_set_color         ( R200DriverData *rdrv,
                              R200DeviceData *rdev,
                              CardState      *state );
void r200_set_src_colorkey  ( R200DriverData *rdrv,
                              R200DeviceData *rdev,
                              CardState      *state );
void r200_set_blend_function( R200DriverData *rdrv,
                              R200DeviceData *rdev,
                              CardState      *state );
void r200_set_drawingflags  ( R200DriverData *rdrv,
                              R200DeviceData *rdev,
                              CardState      *state );
void r200_set_blittingflags ( R200DriverData *rdrv,
                              R200DeviceData *rdev,
                              CardState      *state );

#endif /* __R200_STATE_H__ */
