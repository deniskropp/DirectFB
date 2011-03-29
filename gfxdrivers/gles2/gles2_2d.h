/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

#ifndef __GLES2_2D_H__
#define __GLES2_2D_H__

#include "gles2_gfxdriver.h"

#include <core/coretypes.h>
#include <core/gfxcard.h>

#define GLES2_SUPPORTED_DRAWINGFLAGS      (DSDRAW_BLEND | \
                                           DSDRAW_SRC_PREMULTIPLY)

#define GLES2_SUPPORTED_DRAWINGFUNCTIONS  (DFXL_FILLRECTANGLE | \
                                           DFXL_DRAWRECTANGLE | \
                                           DFXL_DRAWLINE      | \
                                           DFXL_FILLTRIANGLE)

#define GLES2_SUPPORTED_BLITTINGFLAGS     (DSBLIT_BLEND_ALPHACHANNEL | \
                                           DSBLIT_BLEND_COLORALPHA   | \
                                           DSBLIT_COLORIZE           | \
                                           DSBLIT_ROTATE180          | \
                                           DSBLIT_SRC_PREMULTIPLY    | \
                                           DSBLIT_SRC_PREMULTCOLOR   | \
                                           DSBLIT_SRC_COLORKEY)

#define GLES2_SUPPORTED_BLITTINGFUNCTIONS (DFXL_BLIT | \
                                           DFXL_STRETCHBLIT)


DFBResult gles2EngineSync   (void                *drv,
			     void                *dev);

void      gles2EngineReset  (void                *drv,
			     void                *dev);

void      gles2EmitCommands (void                *drv,
			     void                *dev);

void      gles2CheckState   (void                *drv,
			     void                *dev,
			     CardState           *state,
			     DFBAccelerationMask  accel);

void      gles2SetState     (void                *drv,
			     void                *dev,
			     GraphicsDeviceFuncs *funcs,
			     CardState           *state,
			     DFBAccelerationMask  accel);

bool      gles2FillRectangle(void                *drv,
			     void                *dev,
			     DFBRectangle        *rect);

bool      gles2DrawRectangle(void                *drv,
			     void                *dev,
			     DFBRectangle        *rect);

bool      gles2DrawLine     (void                *drv,
			     void                *dev,
			     DFBRegion           *line);

bool      gles2FillTriangle (void                *drv,
			     void                *dev,
			     DFBTriangle         *tri);

bool      gles2Blit         (void                *drv,
			     void                *dev,
			     DFBRectangle        *srect,
			     int                  dx,
			     int                  dy);

bool      gles2StretchBlit  (void                *drv,
			     void                *dev,
			     DFBRectangle        *srect,
			     DFBRectangle        *drect);

#endif

