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

#ifndef ___MATROX_H__
#define ___MATROX_H__

#include <asm/types.h>

#include <core/layers.h>


typedef enum {
     m_Source       = 0x0001,
     m_source       = 0x0002,

     m_Color        = 0x0004,
     m_color        = 0x0008,

     m_SrcKey       = 0x0010,
     m_srckey       = 0x0020,

     m_drawBlend    = 0x0040,
     m_blitBlend    = 0x0080
} MatroxStateBits;

#define MGA_VALIDATE(b)       (mdev->valid |= (b))
#define MGA_INVALIDATE(b)     (mdev->valid &= ~(b))
#define MGA_IS_VALID(b)       (mdev->valid & (b))

typedef struct {
     int            accelerator;
     volatile __u8 *mmio_base;
} MatroxDriverData;

typedef struct {
     /* Old cards are older than G200/G400, e.g. Mystique or Millenium */
     int old_matrox;

     /* FIFO Monitoring */
     unsigned int fifo_space;
     unsigned int waitfifo_sum;
     unsigned int waitfifo_calls;
     unsigned int fifo_waitcycles;
     unsigned int idle_waitcycles;
     unsigned int fifo_cache_hits;

     /* ATYPE_BLK or ATYPE_RSTR, depending on SGRAM setting */
     __u32 atype_blk_rstr;

     /* State handling */
     MatroxStateBits valid;

     /* Stored values */
     int dst_pixelpitch;
     int dst_pixeloffset;
     int src_pixelpitch;
     int src_pixeloffset;

     int draw_blend;
     int blit_src_colorkey;

     int matrox_w2;
     int matrox_h2;
} MatroxDeviceData;

extern DisplayLayerFuncs matroxBesFuncs;

static inline unsigned int log2( unsigned int val )
{
     unsigned int ret = 0;

     while (val >> ret)
          ret++;

     return ((1 << (ret-1)) == val) ? (ret-1) : ret;
}

#endif
