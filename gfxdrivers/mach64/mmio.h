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


#ifndef ___MACH64_MMIO_H__
#define ___MACH64_MMIO_H__

#include <dfb_types.h>

#include "mach64.h"

static inline void
mach64_out8( volatile u8 *mmioaddr, u32 reg, u8 value )
{
     *((volatile u8*)(mmioaddr+reg)) = value;
}

static inline u8
mach64_in8( volatile u8 *mmioaddr, u32 reg )
{
     return *((volatile u8*)(mmioaddr+reg));
}

static inline void
mach64_out32( volatile u8 *mmioaddr, u32 reg, u32 value )
{
#ifdef __powerpc__
     if (reg >= 0x400)
          asm volatile("stwbrx %0,%1,%2;eieio" : : "r"(value), "b"(reg-0x800),
                              "r"(mmioaddr) : "memory");
     else
          asm volatile("stwbrx %0,%1,%2;eieio" : : "r"(value), "b"(reg),
                              "r"(mmioaddr) : "memory");
#else
     if (reg >= 0x400)
          *((volatile u32*)(mmioaddr+reg-0x800)) = value;
     else
          *((volatile u32*)(mmioaddr+reg)) = value;
#endif
}

static inline u32
mach64_in32( volatile u8 *mmioaddr, u32 reg )
{
#ifdef __powerpc__
     u32 value;

     if (reg >= 0x400)
          asm volatile("lwbrx %0,%1,%2;eieio" : "=r"(value) : "b"(reg-0x800), "r"(mmioaddr));
     else
          asm volatile("lwbrx %0,%1,%2;eieio" : "=r"(value) : "b"(reg), "r"(mmioaddr));

     return value;
#else
     if (reg >= 0x400)
          return *((volatile u32*)(mmioaddr+reg-0x800));
     else
          return *((volatile u32*)(mmioaddr+reg));
#endif
}

static const u32 lt_lcd_regs[] = {
     CONFIG_PANEL_LT,
     LCD_GEN_CTRL_LT,
     DSTN_CONTROL_LT,
     HFB_PITCH_ADDR_LT,
     HORZ_STRETCHING_LT,
     VERT_STRETCHING_LT,
     0, /* EXT_VERT_STRETCH */
     LT_GIO_LT,
     POWER_MANAGEMENT_LT
};

#if 0
static inline void
mach64_in_lcd( Mach64DeviceData *mdev,
               volatile u8      *mmioaddr, u8 reg, u32 value )
{
     if (mdev->chip == CHIP_3D_RAGE_LT) {
          mach64_out32( mmioaddr, lt_lcd_regs[reg], value );
     } else if (mdev->chip >= CHIP_3D_RAGE_LT_PRO) {
          mach64_out8( mmioaddr, LCD_INDEX, reg );
          mach64_out32( mmioaddr, LCD_DATA, value );
     }
}
#endif

static inline u32
mach64_in_lcd( Mach64DeviceData *mdev,
               volatile u8      *mmioaddr, u8 reg )
{
     if (mdev->chip == CHIP_3D_RAGE_LT) {
          return mach64_in32( mmioaddr, lt_lcd_regs[reg] );
     } else if (mdev->chip >= CHIP_3D_RAGE_LT_PRO) {
          mach64_out8( mmioaddr, LCD_INDEX, reg );
          return mach64_in32( mmioaddr, LCD_DATA );
     } else {
          return 0;
     }
}

#if 0
static inline void
mach64_out_pll( volatile u8 *mmioaddr, u8 reg, u8 value )
{
     mach64_out8( mmioaddr, CLOCK_CNTL1, (reg << 2) | PLL_WR_EN );
     mach64_out8( mmioaddr, CLOCK_CNTL2, value );
}
#endif

static inline u8
mach64_in_pll( volatile u8 *mmioaddr, u8 reg )
{
     mach64_out8( mmioaddr, CLOCK_CNTL1, reg << 2 );
     return mach64_in8( mmioaddr, CLOCK_CNTL2 );
}

static inline void mach64_waitidle( Mach64DriverData *mdrv,
                                    Mach64DeviceData *mdev )
{
     int timeout = 1000000;

     while (timeout--) {
          if ((mach64_in32( mdrv->mmio_base, FIFO_STAT ) & 0x0000FFFF) == 0)
               break;

          mdev->idle_waitcycles++;
     }

     timeout = 1000000;

     while (timeout--) {
          if ((mach64_in32( mdrv->mmio_base, GUI_STAT ) & GUI_ACTIVE) == 0)
               break;

          mdev->idle_waitcycles++;
     }

     mdev->fifo_space = 16;
}

static inline void mach64_waitfifo( Mach64DriverData *mdrv,
                                    Mach64DeviceData *mdev,
                                    unsigned int requested_fifo_space )
{
     u32 fifo_stat;
     int timeout = 1000000;

     mdev->waitfifo_sum += requested_fifo_space;
     mdev->waitfifo_calls++;

     if (mdev->fifo_space < requested_fifo_space) {
          while (timeout--) {
               mdev->fifo_waitcycles++;

               fifo_stat = mach64_in32( mdrv->mmio_base, FIFO_STAT ) & 0x0000FFFF;
               mdev->fifo_space = 16;
               while (fifo_stat) {
                    mdev->fifo_space--;
                    fifo_stat >>= 1;
               }

               if (mdev->fifo_space >= requested_fifo_space)
                    break;
          }
     }
     else {
          mdev->fifo_cache_hits++;
     }
     mdev->fifo_space -= requested_fifo_space;
}

#endif
