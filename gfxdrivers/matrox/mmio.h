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

#include <asm/types.h>

typedef __u8 uint8;
typedef __u16 uint16;
typedef __u32 uint32;

typedef __s8 sint8;
typedef __s16 sint16;
typedef __s32 sint32;


static inline void
mga_out8(volatile uint8 *mmioaddr, uint8 value, uint32 reg)
{
     *((uint8*)(mmioaddr+reg)) = value;
}

static inline void
mga_out32(volatile uint8 *mmioaddr, uint32 value, uint32 reg)
{
     *((uint32*)(mmioaddr+reg)) = value;
}

static inline volatile uint8
mga_in8(volatile uint8 *mmioaddr, uint32 reg)
{
     return *((uint8*)(mmioaddr+reg));
}

static inline volatile uint32
mga_in32(volatile uint8 *mmioaddr, uint32 reg)
{
     return *((uint32*)(mmioaddr+reg));
}

extern unsigned int matrox_waitfifo_sum;
extern unsigned int matrox_waitfifo_calls;
extern unsigned int matrox_fifo_waitcycles;
extern unsigned int matrox_idle_waitcycles;
extern unsigned int matrox_fifo_cache_hits;

extern unsigned int matrox_fifo_space;

/* Wait for fifo space */
static inline void
mga_waitfifo(volatile uint8 *mmioaddr, int space)
{
     matrox_waitfifo_sum += space;
     matrox_waitfifo_calls++;

     if (matrox_fifo_space < space) {
          do { /* not needed on a G400,
                  hardware does retries on writing if FIFO is full,
                  but results in DMA problems */
               matrox_fifo_space = mga_in8(mmioaddr, FIFOSTATUS);
               matrox_fifo_waitcycles++;
          } while (matrox_fifo_space < space);
     }
     else {
          matrox_fifo_cache_hits++;
     }

     matrox_fifo_space -= space;
}

/* Wait for idle accelerator */
static inline void
mga_waitidle(volatile uint8 *mmioaddr)
{
     while (mga_in32(mmioaddr, STATUS) & 0x10000) {
          matrox_idle_waitcycles++;
     }
}

