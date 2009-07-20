/*
   PXA3xx Graphics Controller

   (c) Copyright 2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2009  Raumfeld GmbH (raumfeld.com)

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Sven Neumann <s.neumann@raumfeld.com>

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

#ifndef __PXA3XX_BLT_H__
#define __PXA3XX_BLT_H__

#include "pxa3xx_types.h"



#define PXA3XX_SUPPORTED_DRAWINGFLAGS      (DSDRAW_NOFX | \
                                            DSDRAW_BLEND)

#define PXA3XX_SUPPORTED_DRAWINGFUNCTIONS  (DFXL_FILLRECTANGLE)

#define PXA3XX_SUPPORTED_BLITTINGFLAGS     (DSBLIT_BLEND_ALPHACHANNEL | \
                                            DSBLIT_COLORIZE | \
                                            DSBLIT_ROTATE90 | \
                                            DSBLIT_ROTATE180 | \
                                            DSBLIT_ROTATE270)

#define PXA3XX_SUPPORTED_BLITTINGFUNCTIONS (DFXL_BLIT)


DFBResult pxa3xxEngineSync  ( void *drv, void *dev );

void pxa3xxEngineReset      ( void *drv, void *dev );

void pxa3xxEmitCommands     ( void *drv, void *dev );

void pxa3xxCheckState       ( void *drv, void *dev,
                              CardState *state, DFBAccelerationMask accel );

void pxa3xxSetState         ( void *drv, void *dev,
                              GraphicsDeviceFuncs *funcs,
                              CardState *state, DFBAccelerationMask accel );


#define PXA3XX_S16S16(h,l)         ((u32)((((u16)(h)) << 16) | ((u16)(l))))

#define PXA3XX_WH(w,h)             PXA3XX_S16S16(h,w)


#endif
