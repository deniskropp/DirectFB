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

#ifndef __MATROX_MMIO_H__
#define __MATROX_MMIO_H__

#include <asm/types.h>

#include "matrox.h"

typedef __u8 uint8;
typedef __u16 uint16;
typedef __u32 uint32;

typedef __s8 sint8;
typedef __s16 sint16;
typedef __s32 sint32;


static inline void
mga_out8(volatile uint8 *mmioaddr, uint8 value, uint32 reg)
{
     *((volatile uint8*)(mmioaddr+reg)) = value;
}

static inline void
mga_out32(volatile uint8 *mmioaddr, uint32 value, uint32 reg)
{
     *((volatile uint32*)(mmioaddr+reg)) = value;
}

static inline volatile uint8
mga_in8(volatile uint8 *mmioaddr, uint32 reg)
{
     return *((volatile uint8*)(mmioaddr+reg));
}

static inline volatile uint32
mga_in32(volatile uint8 *mmioaddr, uint32 reg)
{
     return *((volatile uint32*)(mmioaddr+reg));
}

/* Wait for fifo space */
static inline void
mga_waitfifo(MatroxDriverData *mdrv, MatroxDeviceData *mdev, int space)
{
     mdev->waitfifo_sum += space;
     mdev->waitfifo_calls++;

     if (mdev->fifo_space < space) {
          do { /* not needed on a G400,
                  hardware does retries on writing if FIFO is full,
                  but results in DMA problems */
               mdev->fifo_space = mga_in32(mdrv->mmio_base, FIFOSTATUS) & 0xff;
               mdev->fifo_waitcycles++;
          } while (mdev->fifo_space < space);
     }
     else {
          mdev->fifo_cache_hits++;
     }

     mdev->fifo_space -= space;
}

/* Wait for idle accelerator */
static inline void
mga_waitidle(MatroxDriverData *mdrv, MatroxDeviceData *mdev)
{
     while (mga_in32(mdrv->mmio_base, STATUS) & 0x10000) {
          mdev->idle_waitcycles++;
     }
}

static inline void
mga_out_dac( volatile uint8 *mmioaddr, uint8 reg, uint8 val )
{
     mga_out8( mmioaddr, reg, DAC_INDEX );
     mga_out8( mmioaddr, val, DAC_DATA );
}

#endif

