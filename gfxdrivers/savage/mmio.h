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

/* Streams Processor Registers */

#define SAVAGE_PRIMARY_STREAM_CONTROL                       0x8180
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_CLUT            0x00000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_ARGB            0x01000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_KRGB16          0x03000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB16           0x05000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB24           0x06000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB32           0x07000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSFC_NOT_FILTERED     0x00000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSFC_REP_BOTH         0x10000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSFC_HOR_INTERPOLATE  0x20000000

#define SAVAGE_CHROMA_KEY_CONTROL                           0x8184

#define SAVAGE_GENLOCK_CONTROL                              0x8188

#define SAVAGE_SECONDARY_STREAM_CONTROL                     0x8190
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_CbYCrY422     0x00000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YCbCr422      0x01000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YUV422        0x02000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_KRGB16        0x03000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YCbCr420      0x04000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB16         0x05000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB24         0x06000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB32         0x07000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_LUMA_ONLY_INTERPOL  0x80000000

#define SAVAGE_CHROMA_KEY_UPPER_BOUND                       0x8194
#define SAVAGE_SECONDARY_STREAM_HORIZONTAL_SCALING          0x8198
#define SAVAGE_COLOR_ADJUSTMENT                             0x819C
#define SAVAGE_BLEND_CONTROL                                0x81a0
#define SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_ADRESS0          0x81c0
#define SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_ADRESS1          0x81c4
#define SAVAGE_PRIMARY_STREAM_STRIDE                        0x81c8
#define SAVAGE_SECONDARY_STREAM_MULTIPLE_BUFFER_SUPPORT     0x81cc
#define SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS0       0x81d0
#define SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS1       0x81d4
#define SAVAGE_SECONDARY_STREAM_STRIDE                      0x81d8
#define SAVAGE_SECONDARY_STREAM_VERTICAL_SCALING            0x81e0
#define SAVAGE_SECONDARY_STREAM_VERTICAL_INITIAL_VALUE      0x81e4
#define SAVAGE_SECONDARY_STREAM_SOURCE_LINE_COUNT           0x81e8
#define SAVAGE_STREAMS_FIFO                                 0x81ec
#define SAVAGE_PRIMARY_STREAM_WINDOW_START                  0x81f0
#define SAVAGE_PRIMARY_STREAM_WINDOW_SIZE                   0x81f4
#define SAVAGE_SECONDARY_STREAM_WINDOW_START                0x81f8
#define SAVAGE_SECONDARY_STREAM_WINDOW_SIZE                 0x81fc
#define SAVAGE_PRIMARY_STREAM_FRAMEBUFFER_SIZE              0x8300
#define SAVAGE_SECONDARY_STREAM_FRAMEBUFFER_SIZE            0x8304
#define SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS2       0x8308

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

#define savage_out32(mmio, reg, value) *((uint32*)((mmio)+(reg))) = (uint32)(value)
#define savage_in32(mmio, reg)         *((uint32*)((mmio)+(reg)))
#define savage_out16(mmio, reg, value) *((uint16*)((mmio)+(reg))) = (uint16)(value)

#endif

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

#endif

