/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

     /* overlay registers */
     struct {
          __u32 H_INC;
          __u32 STEP_BY;
          __u32 Y_X_START;
          __u32 Y_X_END;
          __u32 V_INC;
          __u32 P1_BLANK_LINES_AT_TOP;
          __u32 P23_BLANK_LINES_AT_TOP;
          __u32 VID_BUF_PITCH0_VALUE;
          __u32 VID_BUF_PITCH1_VALUE;
          __u32 P1_X_START_END;
          __u32 P2_X_START_END;
          __u32 P3_X_START_END;
          __u32 VID_BUF0_BASE_ADRS;
          __u32 VID_BUF1_BASE_ADRS;
          __u32 VID_BUF2_BASE_ADRS;
          __u32 P1_V_ACCUM_INIT;
          __u32 P23_V_ACCUM_INIT;
          __u32 P1_H_ACCUM_INIT;
          __u32 P23_H_ACCUM_INIT;
          __u32 SCALE_CNTL;
     } ov0;
} ATI128DeviceData;

void
ati128_init_layers( void *drv, void *dev );

#endif
