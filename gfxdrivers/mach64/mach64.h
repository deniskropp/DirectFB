/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#ifndef ___MACH64_H__
#define ___MACH64_H__

#include <dfb_types.h>
#include <core/coretypes.h>
#include <core/layers.h>

typedef enum {
     m_source       = 0x001,
     m_color        = 0x002,
     m_color_3d     = 0x004,
     m_srckey       = 0x008,
     m_srckey_scale = 0x010,
     m_dstkey       = 0x020,
     m_disable_key  = 0x040,
     m_draw_blend   = 0x080,
     m_blit_blend   = 0x100
} Mach64StateBits;

#define MACH64_VALIDATE(b)      (mdev->valid |= (b))
#define MACH64_INVALIDATE(b)    (mdev->valid &= ~(b))
#define MACH64_IS_VALID(b)      (mdev->valid & (b))

typedef struct {
     bool rage_pro;

     /* for fifo/performance monitoring */
     unsigned int fifo_space;
     unsigned int waitfifo_sum;
     unsigned int waitfifo_calls;
     unsigned int fifo_waitcycles;
     unsigned int idle_waitcycles;
     unsigned int fifo_cache_hits;

     Mach64StateBits valid;

     __u32 src_pix_width;
     __u32 dst_pix_width;

     __u32 src_key_mask;
     __u32 dst_key_mask;

     __u32 draw_blend;
     __u32 blit_blend;

     __u32 src_cntl;

     CoreSurface *source;
} Mach64DeviceData;

typedef struct {
     int accelerator;
     volatile __u8 *mmio_base;
     Mach64DeviceData *device_data;
} Mach64DriverData;

extern DisplayLayerFuncs mach64OverlayFuncs;

#endif
