/*
   Copyright (C) 2004-2005 Claudio Ciccani <klan@users.sf.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __NVIDIA_3D_H__
#define __NVIDIA_3D_H__


bool nvFillRectangle3D( void *drv, void *dev, DFBRectangle *rect );

bool nvFillTriangle3D( void *drv, void *dev, DFBTriangle *tri );

bool nvDrawRectangle3D( void *drv, void *dev, DFBRectangle *rect );

bool nvDrawLine3D( void *drv, void *dev, DFBRegion *line );

bool nvTextureTriangles( void *drv, void *dev, DFBVertex *vertices,
                         int num, DFBTriangleFormation formation );


#endif /* __NVIDIA_3D_H__ */

