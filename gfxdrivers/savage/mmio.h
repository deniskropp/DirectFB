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

#ifndef __SAVAGE__MMIO_H__
#define __SAVAGE__MMIO_H__

#include <asm/types.h>

typedef __u8 uint8;
typedef __u16 uint16;
typedef __u32 uint32;

typedef __s8 sint8;
typedef __s16 sint16;
typedef __s32 sint32;

#if 0

static inline void
savage_out32(volatile uint8 *mmioaddr, uint32 reg, uint32 value)
{
     *((uint32*)(mmioaddr+reg)) = value;
}

static inline volatile uint32
savage_in32(volatile uint8 *mmioaddr, uint32 reg)
{
     return *((uint32*)(mmioaddr+reg));
}

static inline void
savage_out16(volatile uint8 *mmioaddr, uint32 reg, uint16 value)
{
     *((uint16*)(mmioaddr+reg)) = value;
}

#else

#define savage_out32(mmio, reg, value) (*((volatile uint32 *) ((mmio)+(reg))) = (uint32)(value))
#define savage_in32(mmio, reg)         (*((volatile uint32 *) ((mmio)+(reg))))
#define savage_out16(mmio, reg, value) (*((volatile uint16 *) ((mmio)+(reg))) = (uint16)(value))

#endif

#if 0

static inline void
vga_out8(volatile uint8 *mmioaddr, uint16 reg, uint8 value)
{
     *((uint8*)(mmioaddr+0x8000+reg)) = value;
}

static inline void
vga_out16(volatile uint8 *mmioaddr, uint16 reg, uint16 value)
{
     *((uint8*)(mmioaddr+0x8000+reg)) = value;
}

static inline volatile uint8
vga_in8(volatile uint8 *mmioaddr, uint16 reg)
{
     return *((uint8*)(mmioaddr+0x8000+reg));
}

#else

#define vga_out8(mmio, reg, value)  (*((volatile uint8 *) ((mmio)+0x8000+(reg))) = (uint8)(value))
#define vga_out16(mmio, reg, value) (*((volatile uint16 *) ((mmio)+0x8000+(reg))) = (uint16)(value))
#define vga_in8(mmio, reg)          (*((volatile uint8 *) ((mmio)+0x8000+(reg))))

#endif

#endif /* __SAVAGE__MMIO_H__ */
