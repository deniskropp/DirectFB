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

#ifndef ___MATROX_H__
#define ___MATROX_H__

#include <asm/types.h>

typedef struct {
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
     int m_Source;
     int m_source;

     int m_Color;
     int m_color;

     int m_SrcKey;
     int m_srckey;

     int m_drawBlend;
     int m_blitBlend;

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

static inline unsigned int log2( unsigned int val )
{
     unsigned int ret = 0;

     while (val >>= 1)
          ret++;

     return ret;
}

#endif
