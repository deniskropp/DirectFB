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


#ifndef ___ATI128_MMIO_H__
#define ___ATI128_MMIO_H__

#include <asm/types.h>

#include "ati128.h"

static inline void
ati128_out32(volatile __u8 *mmioaddr, __u32 reg, __u32 value)
{
#ifdef __powerpc__
       asm("stwbrx %0,%1,%2;eieio" : : "r"(value), "b"(reg),
                       "r"((volatile __u32*)mmioaddr) : "memory");

#else
     *((volatile __u32*)(mmioaddr+reg)) = value;
#endif
}

static inline volatile __u32
ati128_in32(volatile __u8 *mmioaddr, __u32 reg)
{
#ifdef __powerpc__
     __u32 value;

     asm("lwbrx %0,%1,%2;eieio" : "=r"(value) : "b"(reg), "r"((volatile __u32*)mmioaddr));

     return value;
#else
     return *((volatile __u32*)(mmioaddr+reg));
#endif
}

static inline void ati128_waitidle( ATI128DriverData *adrv,
                                    ATI128DeviceData *adev )
{
     int timeout = 1000000;

     while (timeout--) {
          if ((ati128_in32( adrv->mmio_base, GUI_STAT) & 0x00000FFF) == 64)
               break;

          adev->idle_waitcycles++;
     }

     timeout = 1000000;

     while (timeout--) {
          if ((ati128_in32( adrv->mmio_base, GUI_STAT) & (GUI_ACTIVE | ENG_3D_BUSY)) == ENGINE_IDLE)
               break;

          adev->idle_waitcycles++;
     }

     ati128_out32( adrv->mmio_base, PC_NGUI_CTLSTAT,
                   ati128_in32( adrv->mmio_base, PC_NGUI_CTLSTAT) | 0x000000ff);

     timeout = 1000000;
     while (timeout--) {
          if ((ati128_in32( adrv->mmio_base, PC_NGUI_CTLSTAT) & PC_BUSY) != PC_BUSY)
               break;

          adev->idle_waitcycles++;
     }
     adev->fifo_space = 60;
}

static inline void ati128_waitfifo( ATI128DriverData *adrv,
                                    ATI128DeviceData *adev,
                                    int requested_fifo_space)
{
     int timeout = 1000000;

     adev->waitfifo_sum += requested_fifo_space;
     adev->waitfifo_calls++;

     if (adev->fifo_space < requested_fifo_space) {
          while (timeout--) {
               adev->fifo_waitcycles++;

               adev->fifo_space = ati128_in32( adrv->mmio_base, GUI_STAT) & 0x00000FFF;
               if (adev->fifo_space >= requested_fifo_space)
                    break;
          }
     }
     else {
          adev->fifo_cache_hits++;
     }
     adev->fifo_space -= requested_fifo_space;
}

#endif
