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

#include "nvidia.h"


#define nv_out8(  ptr, i, val )  (((volatile __u8 *) (ptr))[(i)]   = (val))
#define nv_out16( ptr, i, val )  (((volatile __u16*) (ptr))[(i)/2] = (val))
#define nv_out32( ptr, i, val )  (((volatile __u32*) (ptr))[(i)/4] = (val))

#define nv_in8(  ptr, i )        (((volatile __u8 *) (ptr))[(i)]  )
#define nv_in16( ptr, i )        (((volatile __u16*) (ptr))[(i)/2])
#define nv_in32( ptr, i )        (((volatile __u32*) (ptr))[(i)/4])


static inline void
nv_waitidle( NVidiaDriverData *nvdrv,
             NVidiaDeviceData *nvdev )
{
     int waitcycles = 0;
     
     while (nv_in32( nvdrv->PGRAPH, 0x700 ) & 1) {
          if (++waitcycles > 10000000) {
               D_BREAK( "card hung" );
               /* avoid card crash */
               _exit(-1);
          }
     }
      
     nvdev->idle_waitcycles += waitcycles;
}

static inline void
nv_waitfifo( NVidiaDeviceData *nvdev,
             NVFifoSubChannel *subch,
             int               space )
{
     int waitcycles = 0;

     nvdev->waitfifo_sum += (space);
     nvdev->waitfifo_calls++;

     if (nvdev->fifo_space < space) {
          do {
               nvdev->fifo_space = subch->Free >> 2;
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

#define subchannelof( obj )  ((NVFifoSubChannel*) ((__u8*) (obj) - 256))


#endif /* __NVIDIA_MMIO_H__ */

