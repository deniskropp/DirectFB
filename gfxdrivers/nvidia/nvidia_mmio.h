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

#ifndef __NVIDIA_MMIO_H__
#define __NVIDIA_MMIO_H__

#include <unistd.h>

#include "nvidia.h"


static __inline__ void
nv_out8( volatile __u8 *mmioaddr, __u32 reg, __u8 value )
{
     *((volatile __u8*)(mmioaddr+reg)) = value;
}

static __inline__ void
nv_out16( volatile __u8 *mmioaddr, __u32 reg, __u16 value )
{
     *((volatile __u16*)(mmioaddr+reg)) = value;
}

static __inline__ void
nv_out32( volatile __u8 *mmioaddr, __u32 reg, __u32 value )
{
    *((volatile __u32*)(mmioaddr+reg)) = value;
}

static __inline__ __u8
nv_in8( volatile __u8 *mmioaddr, __u32 reg )
{
     return *((volatile __u8*)(mmioaddr+reg));
}

static __inline__ __u16
nv_in16( volatile __u8 *mmioaddr, __u32 reg )
{
     return *((volatile __u16*)(mmioaddr+reg));
}

static __inline__ __u32
nv_in32( volatile __u8 *mmioaddr, __u32 reg )
{
    return *((volatile __u32*)(mmioaddr+reg));
}


static __inline__ void
nv_outcrtc( volatile __u8 *mmioaddr, __u8 reg, __u8 value )
{
     nv_out8( mmioaddr, PCIO_CRTC_INDEX, reg );
     nv_out8( mmioaddr, PCIO_CRTC_DATA, value );
}

static __inline__ __u8
nv_incrtc( volatile __u8 *mmioaddr, __u8 reg )
{
     nv_out8( mmioaddr, PCIO_CRTC_INDEX, reg );
     return nv_in8( mmioaddr, PCIO_CRTC_DATA );
}


static inline void
nv_waitidle( NVidiaDriverData *nvdrv,
             NVidiaDeviceData *nvdev )
{
     __u32 status;
     int   waitcycles = 0;
     
     do {
          status = nv_in32( nvdrv->mmio_base, PGRAPH_STATUS );
          if (++waitcycles > 10000000) {
               D_BREAK( "card hung" );
               /* avoid card crash */
               _exit(-1);
          }
     } while (status & PGRAPH_STATUS_STATE_BUSY);
      
     nvdev->idle_waitcycles += waitcycles;
}

static inline void
nv_waitfifo( NVidiaDriverData *nvdrv,
             NVidiaDeviceData *nvdev,
             unsigned int      space )
{
     volatile __u8 *mmio       = nvdrv->mmio_base;
     int            waitcycles = 0;

     nvdev->waitfifo_sum += (space);
     nvdev->waitfifo_calls++;

     if (nvdev->fifo_space < space) {
          do {
#ifdef WORDS_BIGENDIAN
               nvdev->fifo_space = nv_in16( mmio, FIFO_FREE ) >> 2;
#else
               nvdev->fifo_space = nv_in32( mmio, FIFO_FREE ) >> 2;
#endif
               if (++waitcycles > 0x10000) {
                    D_BREAK( "card hung" );
                    /* avoid card crash */
                    _exit(-1);
               }
          } while (nvdev->fifo_space < space);

          nvdev->fifo_waitcycles += waitcycles;
     } else
          nvdev->fifo_cache_hits++;

     nvdev->fifo_space -= space;
}


static inline void
nv_assign_object( NVidiaDriverData *nvdrv,
                  NVidiaDeviceData *nvdev,
                  int               subchannel,
                  __u32             object )
{
     NVFifoChannel *Fifo = nvdrv->Fifo;

     if (nvdev->subchannel_object[subchannel] != object) {
          nv_waitfifo( nvdrv, nvdev, 1 );
          Fifo->sub[subchannel].SetObject      = object;
          nvdev->subchannel_object[subchannel] = object;
     }
}


#endif /* __NVIDIA_MMIO_H__ */

