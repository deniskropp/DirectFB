/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

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

#ifndef __MATROX_MMIO_H__
#define __MATROX_MMIO_H__

#include <dfb_types.h>

#include "matrox.h"

static inline void
mga_out8(volatile u8 *mmioaddr, u8 value, u32 reg)
{
     *((volatile u8*)(mmioaddr+reg)) = value;
}

static inline void
mga_out32(volatile u8 *mmioaddr, u32 value, u32 reg)
{
#ifdef __powerpc__
     asm volatile("stwbrx %0,%1,%2;eieio" : : "r"(value), "b"(reg), "r"(mmioaddr) : "memory");
#else
     *((volatile u32*)(mmioaddr+reg)) = value;
#endif
}

static inline u8
mga_in8(volatile u8 *mmioaddr, u32 reg)
{
     return *((volatile u8*)(mmioaddr+reg));
}

static inline u32
mga_in32(volatile u8 *mmioaddr, u32 reg)
{
#ifdef __powerpc__
     u32 value;

     asm volatile("lwbrx %0,%1,%2;eieio" : "=r"(value) : "b"(reg), "r"(mmioaddr));

     return value;
#else
     return *((volatile u32*)(mmioaddr+reg));
#endif
}

/* Wait for idle accelerator and DMA */
static inline void
mga_waitidle(MatroxDriverData *mdrv, MatroxDeviceData *mdev)
{
     while ((mga_in32(mdrv->mmio_base, STATUS) & (DWGENGSTS | ENDPRDMASTS)) != mdev->idle_status) {
          mdev->idle_waitcycles++;
     }
}

/* Wait for fifo space */
static inline void
mga_waitfifo(MatroxDriverData *mdrv, MatroxDeviceData *mdev, unsigned int space)
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

static inline void
mga_out_dac( volatile u8 *mmioaddr, u8 reg, u8 val )
{
     mga_out8( mmioaddr, reg, DAC_INDEX );
     mga_out8( mmioaddr, val, DAC_DATA );
}

static inline u8
mga_in_dac( volatile u8 *mmioaddr, u8 reg )
{
     mga_out8( mmioaddr, reg, DAC_INDEX );
     return mga_in8( mmioaddr, DAC_DATA );
}

#endif

