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
 
#ifndef __RADEON_3D_H__
#define __RADEON_3D_H__

/* R100 Functions */
bool r100FillRectangle3D( void *drv, void *dev, DFBRectangle *rect );

bool r100FillTriangle( void *drv, void *dev, DFBTriangle *tri );

bool r100DrawRectangle3D( void *drv, void *dev, DFBRectangle *rect );

bool r100DrawLine3D( void *drv, void *dev, DFBRegion *line );

bool r100Blit3D( void *drv, void *dev, DFBRectangle *sr, int dx, int dy );

bool r100StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr );

bool r100TextureTriangles( void *drv, void *dev, DFBVertex *ve,
                           int num, DFBTriangleFormation formation );
          
void r100EmitCommands3D( void *drv, void *dev );
                                                                  
/* R200 Functions */
bool r200FillRectangle3D( void *drv, void *dev, DFBRectangle *rect );

bool r200FillTriangle( void *drv, void *dev, DFBTriangle *tri );

bool r200DrawRectangle3D( void *drv, void *dev, DFBRectangle *rect );

bool r200DrawLine3D( void *drv, void *dev, DFBRegion *line );

bool r200Blit3D( void *drv, void *dev, DFBRectangle *sr, int dx, int dy );

bool r200StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr );

bool r200TextureTriangles( void *drv, void *dev, DFBVertex *ve,
                           int num, DFBTriangleFormation formation );

void r200EmitCommands3D( void *drv, void *dev );

/* R300 Functions */
bool r300FillRectangle3D( void *drv, void *dev, DFBRectangle *rect );

bool r300FillTriangle( void *drv, void *dev, DFBTriangle *tri );

bool r300DrawRectangle3D( void *drv, void *dev, DFBRectangle *rect );

bool r300DrawLine3D( void *drv, void *dev, DFBRegion *line );

bool r300Blit3D( void *drv, void *dev, DFBRectangle *sr, int dx, int dy );

bool r300StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr );

bool r300TextureTriangles( void *drv, void *dev, DFBVertex *ve,
                           int num, DFBTriangleFormation formation );

void r300EmitCommands3D( void *drv, void *dev );
                               

#endif /* __RADEON_3D_H__ */
