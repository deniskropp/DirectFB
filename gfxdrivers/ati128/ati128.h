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

#ifndef ___ATI128_H__
#define ___ATI128_H__

#include <asm/types.h>
#include <core/coretypes.h>
#include <core/layers.h>

typedef struct {
     volatile __u8 *mmio_base;
} ATI128DriverData;

typedef struct {
     CoreSurface *source;
     CoreSurface *destination;
     DFBSurfaceBlittingFlags blittingflags;

     /* store some ATI register values in native format */
     __u32 ATI_dst_bpp;
     __u32 ATI_color_compare;
     __u32 ATI_blend_function;

     /* used for the fake texture hack */
     __u32 ATI_fake_texture_src;
     __u32 fake_texture_color;
     unsigned int fake_texture_number;

     /* for fifo/performance monitoring */
     unsigned int fifo_space;

     unsigned int waitfifo_sum;
     unsigned int waitfifo_calls;
     unsigned int fifo_waitcycles;
     unsigned int idle_waitcycles;
     unsigned int fifo_cache_hits;
} ATI128DeviceData;

extern DisplayLayerFuncs atiOverlayFuncs;

#endif
