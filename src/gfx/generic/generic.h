/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __GENERIC_H__
#define __GENERIC_H__

#include <config.h>

#include <asm/types.h>

#include <directfb.h>
#include <core/coretypes.h>

/* this order is required for Intel with MMX, how about bigendian? */

typedef struct
{
     __u16 b;
     __u16 g;
     __u16 r;
     __u16 a;
} Accumulator;

#define ACC_WIDTH 1600

typedef void (*GFunc)();


#ifdef USE_MMX
void gInit_MMX();
#endif

void gGetDriverInfo( GraphicsDriverInfo *info );
void gGetDeviceInfo( GraphicsDeviceInfo *info );

int  gAquire  ( CardState *state, DFBAccelerationMask accel );
void gRelease ( CardState *state );

void gFillRectangle ( DFBRectangle *rect );
void gDrawLine      ( DFBRegion    *line );

void gBlit          ( DFBRectangle *rect, int dx, int dy );
void gStretchBlit   ( DFBRectangle *srect, DFBRectangle *drect );


#endif
