/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI Radeon cards written by
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
 
#ifndef __RADEON_2D_H__
#define __RADEON_2D_H__

bool radeonFillRectangle2D( void *drv, void *dev, DFBRectangle *rect );
bool radeonFillRectangle2D_420( void *drv, void *dev, DFBRectangle *rect );

bool radeonDrawRectangle2D( void *drv, void *dev, DFBRectangle *rect );
bool radeonDrawRectangle2D_420( void *drv, void *dev, DFBRectangle *rect );

bool radeonDrawLine2D( void *drv, void *dev, DFBRegion *line );
bool radeonDrawLine2D_420( void *drv, void *dev, DFBRegion *line );

bool radeonBlit2D( void *drv, void *dev, DFBRectangle *sr, int dx, int dy );
bool radeonBlit2D_420( void *drv, void *dev, DFBRectangle *sr, int dx, int dy );

#endif /* __RADEON_2D_H__ */
