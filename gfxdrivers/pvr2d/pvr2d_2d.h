/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#ifndef __PVR2D_2D_H__
#define __PVR2D_2D_H__

#include <core/coretypes.h>
#include <core/gfxcard.h>

#define PVR2D_SUPPORTED_DRAWINGFLAGS      (DSDRAW_NOFX)//BLEND | \
                                           //DSDRAW_SRC_PREMULTIPLY)

#define PVR2D_SUPPORTED_DRAWINGFUNCTIONS  (DFXL_FILLRECTANGLE)// | \
                                           //DFXL_DRAWRECTANGLE | \
                                           //DFXL_DRAWLINE      | \
                                           //DFXL_FILLTRIANGLE)

#define PVR2D_SUPPORTED_BLITTINGFLAGS     (DSBLIT_BLEND_ALPHACHANNEL | \
                                           DSBLIT_BLEND_COLORALPHA) //
                                           //DSBLIT_SRC_COLORKEY)
                                           //DSBLIT_COLORIZE           )// \
                                           //DSBLIT_ROTATE180          | \
                                           //DSBLIT_SRC_PREMULTIPLY    | \
                                           //DSBLIT_SRC_PREMULTCOLOR)

#define PVR2D_SUPPORTED_BLITTINGFUNCTIONS (DFXL_BLIT | \
                                           DFXL_STRETCHBLIT)


DFBResult pvr2dEngineSync   (void                *drv,
                             void                *dev);

void      pvr2dEngineReset  (void                *drv,
                             void                *dev);

void      pvr2dEmitCommands (void                *drv,
                             void                *dev);

void      pvr2dCheckState   (void                *drv,
                             void                *dev,
                             CardState           *state,
                             DFBAccelerationMask  accel);

void      pvr2dSetState     (void                *drv,
                             void                *dev,
                             GraphicsDeviceFuncs *funcs,
                             CardState           *state,
                             DFBAccelerationMask  accel);

bool      pvr2dFillRectangle(void                *drv,
                             void                *dev,
                             DFBRectangle        *rect);

bool      pvr2dDrawRectangle(void                *drv,
                             void                *dev,
                             DFBRectangle        *rect);

bool      pvr2dDrawLine     (void                *drv,
                             void                *dev,
                             DFBRegion           *line);

bool      pvr2dFillTriangle (void                *drv,
                             void                *dev,
                             DFBTriangle         *tri);

bool      pvr2dBlit         (void                *drv,
                             void                *dev,
                             DFBRectangle        *srect,
                             int                  dx,
                             int                  dy);

bool      pvr2dStretchBlit  (void                *drv,
                             void                *dev,
                             DFBRectangle        *srect,
                             DFBRectangle        *drect);

#endif

