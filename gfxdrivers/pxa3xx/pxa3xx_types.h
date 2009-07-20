/*
   PXA3xx Graphics Controller

   (c) Copyright 2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2009  Raumfeld GmbH (raumfeld.com)

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Sven Neumann <s.neumann@raumfeld.com>

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

#ifndef __PXA3XX__TYPES_H__
#define __PXA3XX__TYPES_H__

#include <pxa3xx-gcu.h>


#define PXA3XX_GFX_MAX_PREPARE             8192


typedef volatile struct pxa3xx_gcu_shared PXA3XXGfxSharedArea;


typedef struct {
     /* fake source buffer */
     int                      fake_size;
     int                      fake_offset;
     unsigned long            fake_phys;

     /* state validation */
     int                      v_flags;

     /* cached values */
     unsigned long            dst_phys;
     int                      dst_pitch;
     int                      dst_bpp;
     int                      dst_index;

     unsigned long            src_phys;
     int                      src_pitch;
     int                      src_bpp;
     int                      src_index;
     bool                     src_alpha;

     unsigned long            mask_phys;
     int                      mask_pitch;
     DFBSurfacePixelFormat    mask_format;
     int                      mask_index;
     DFBPoint                 mask_offset;
     DFBSurfaceMaskFlags      mask_flags;

     DFBSurfaceDrawingFlags   dflags;
     DFBSurfaceBlittingFlags  bflags;
     DFBSurfaceRenderOptions  render_options;

     DFBColor                 color;
} PXA3XXDeviceData;


typedef struct {
     void                    *fake_virt;

     PXA3XXDeviceData        *dev;

     CoreDFB                 *core;
     CoreGraphicsDevice      *device;

     int                      gfx_fd;
     PXA3XXGfxSharedArea     *gfx_shared;

     int                      prep_num;
     __u32                    prep_buf[PXA3XX_GFX_MAX_PREPARE];

     volatile void           *mmio_base;
} PXA3XXDriverData;

#endif

