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


#ifndef ___ATI128_MMIO_H__
#define ___ATI128_MMIO_H__

#include <asm/types.h>


/* for fifo/performance monitoring */
extern unsigned int ati128_waitfifo_sum;
extern unsigned int ati128_waitfifo_calls;
extern unsigned int ati128_fifo_waitcycles;
extern unsigned int ati128_idle_waitcycles;
extern unsigned int ati128_fifo_cache_hits;

extern unsigned int ati128_fifo_space;

static inline void
ati128_out32(volatile __u8 *mmioaddr, __u32 reg, __u32 value)
{
#ifdef __powerpc__
       asm("stwbrx %0,%1,%2;eieio" : : "r"(value), "b"(reg),
                       "r"((__u32*)mmioaddr) : "memory");

#else
     *((__u32*)(mmioaddr+reg)) = value;
#endif
}

static inline volatile __u32
ati128_in32(volatile __u8 *mmioaddr, __u32 reg)
{
#ifdef __powerpc__
     __u32 value;

     asm("lwbrx %0,%1,%2;eieio" : "=r"(value) : "b"(reg), "r"((__u32*)mmioaddr));

     return value;
#else
     return *((__u32*)(mmioaddr+reg));
#endif
}

static inline void ati128_waitidle( volatile __u8 *mmioaddr )
{
#ifndef __powerpc__    
     int timeout = 1000000;          
     
     while (timeout--) { 
          if ((ati128_in32( mmioaddr, GUI_STAT) & 0x00000FFF) == 64)
               break;
          	 
          ati128_idle_waitcycles++;
     }

     timeout = 1000000;
     
     while (timeout--) {          
          if ((ati128_in32( mmioaddr, GUI_STAT) & (GUI_ACTIVE | ENG_3D_BUSY)) == ENGINE_IDLE)
               break;
          
          ati128_idle_waitcycles++;
     }

     ati128_out32( mmioaddr, PC_NGUI_CTLSTAT,
                   ati128_in32( mmioaddr, PC_NGUI_CTLSTAT) | 0x000000ff);

     timeout = 1000000;
     while (timeout--) {
          if ((ati128_in32( mmioaddr, PC_NGUI_CTLSTAT) & PC_BUSY) != PC_BUSY)
               break;
               
          ati128_idle_waitcycles++;
     }    
     ati128_fifo_space = 60;
#endif
}

static inline void ati128_waitfifo( volatile __u8 *mmioaddr,
                                    int requested_fifo_space)
{
     int timeout = 1000000;
     
     ati128_waitfifo_sum += requested_fifo_space;
     ati128_waitfifo_calls++;

     if (ati128_fifo_space < requested_fifo_space) {
          while (timeout--) {
               ati128_fifo_waitcycles++;

               ati128_fifo_space = ati128_in32( mmioaddr, GUI_STAT) & 0x00000FFF;
               if (ati128_fifo_space >= requested_fifo_space)
                    break;               
          }
     }
     else {
          ati128_fifo_cache_hits++;
     }
     ati128_fifo_space -= requested_fifo_space;
}

#endif
