/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __VMWARE_2D_H__
#define __VMWARE_2D_H__

#include <direct/processor.h>


#define VMWARE_SUPPORTED_DRAWINGFLAGS      (DSDRAW_NOFX)

#define VMWARE_SUPPORTED_DRAWINGFUNCTIONS  (DFXL_FILLRECTANGLE)

#define VMWARE_SUPPORTED_BLITTINGFLAGS     (DSBLIT_NOFX)

#define VMWARE_SUPPORTED_BLITTINGFUNCTIONS (DFXL_BLIT)


void      vmwareCheckState   ( void                *drv,
                               void                *dev,
                               CardState           *state,
                               DFBAccelerationMask  accel );

void      vmwareSetState     ( void                *drv,
                               void                *dev,
                               GraphicsDeviceFuncs *funcs,
                               CardState           *state,
                               DFBAccelerationMask  accel );

bool      vmwareFillRectangle( void                *drv,
                               void                *dev,
                               DFBRectangle        *rect );

bool      vmwareBlit         ( void                *drv,
                               void                *dev,
                               DFBRectangle        *srect,
                               int                  dx,
                               int                  dy );



void      vmwareEngineReset  ( void                *drv,
                               void                *dev );

DFBResult vmwareEngineSync   ( void                *drv,
                               void                *dev );
DFBResult vmwareWaitSerial   ( void                *drv,
                               void                *dev,
                               const CoreGraphicsSerial *serial );

void      vmwareGetSerial    ( void                *drv,
                               void                *dev,
                               CoreGraphicsSerial  *serial );

void      vmwareEmitCommands ( void                *drv,
                               void                *dev );


extern const DirectProcessorFuncs *virtual2DFuncs;


typedef enum {
     V2D_OP_SERIAL,
     V2D_OP_FILL,
     V2D_OP_BLIT
} Virtual2DOp;

typedef struct {
     CoreGraphicsSerial serial;

     Virtual2DOp    op;

     void          *dst_addr;
     int            dst_bpp;
     int            dst_pitch;

     void          *src_addr;
     int            src_bpp;
     int            src_pitch;

     DFBRectangle   dst;
     DFBRectangle   src;

     u32            color;
} Virtual2DPacket;

#endif

