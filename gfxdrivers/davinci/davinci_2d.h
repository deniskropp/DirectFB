/*
   TI Davinci driver

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org>

   Code is derived from VMWare driver.

   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __DAVINCI_2D_H__
#define __DAVINCI_2D_H__


#define DAVINCI_SUPPORTED_DRAWINGFLAGS      (DSDRAW_NOFX)

#define DAVINCI_SUPPORTED_DRAWINGFUNCTIONS  (DFXL_NONE)

#define DAVINCI_SUPPORTED_BLITTINGFLAGS     (DSBLIT_NOFX)

#define DAVINCI_SUPPORTED_BLITTINGFUNCTIONS (DFXL_NONE)


DFBResult davinciEngineSync   ( void                *drv,
                                void                *dev );

void      davinciEngineReset  ( void                *drv,
                                void                *dev );

void      davinciEmitCommands ( void                *drv,
                                void                *dev );

void      davinciCheckState   ( void                *drv,
                                void                *dev,
                                CardState           *state,
                                DFBAccelerationMask  accel );

void      davinciSetState     ( void                *drv,
                                void                *dev,
                                GraphicsDeviceFuncs *funcs,
                                CardState           *state,
                                DFBAccelerationMask  accel );

bool      davinciFillRectangle( void                *drv,
                                void                *dev,
                                DFBRectangle        *rect );

bool      davinciBlit         ( void                *drv,
                                void                *dev,
                                DFBRectangle        *srect,
                                int                  dx,
                                int                  dy );

#endif

