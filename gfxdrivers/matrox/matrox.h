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

#ifndef ___MATROX_H__
#define ___MATROX_H__

#include <dfb_types.h>

#include <core/layers.h>
#include <core/screens.h>


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
     int            maven_fd;
     volatile __u8 *mmio_base;

     bool           g450;

     CoreScreen    *primary;
     CoreScreen    *secondary;
} MatroxDriverData;

typedef struct {
     /* Old cards are older than G200/G400, e.g. Mystique or Millennium */
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
     int dst_pitch;
     int dst_offset;
     int dst_cb_offset;
     int dst_cr_offset;
     int src_pitch;
     int src_offset;
     int src_cb_offset;
     int src_cr_offset;
     int w, h, w2, h2;

     bool draw_blend;
     bool blit_src_colorkey;

     bool blit_deinterlace;
     int field;
     bool planar;

     __u32 texctl;

     DFBRegion clip;
} MatroxDeviceData;

extern DisplayLayerFuncs matroxBesFuncs;
extern DisplayLayerFuncs matroxCrtc2Funcs;
extern DisplayLayerFuncs matroxSpicFuncs;

extern ScreenFuncs matroxCrtc2ScreenFuncs;

static inline int mga_log2( int val )
{
     int ret = 0;

     while (val >> ret)
          ret++;

     return ((1 << (ret-1)) == val) ? (ret-1) : ret;
}

#endif
