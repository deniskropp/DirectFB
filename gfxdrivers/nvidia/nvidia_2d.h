/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#ifndef __NVIDIA_2D_H__
#define __NVIDIA_2D_H__

bool nvFillRectangle2D( void *drv, void *dev, DFBRectangle *rect );

bool nvFillTriangle2D( void *drv, void *dev, DFBTriangle *tri );

bool nvDrawRectangle2D( void *drv, void *dev, DFBRectangle *rect );

bool nvDrawLine2D( void *drv, void *dev, DFBRegion *line );

bool nvBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy );

bool nvBlitFromCPU( void *drv, void *dev, DFBRectangle *rect, int dx, int dy );

bool nvStretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr );

bool nvStretchBlitFromCPU( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr );

#endif /* __NVIDIA_2D_H__ */

