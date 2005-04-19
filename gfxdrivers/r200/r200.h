/*
 * Copyright (C) 2005 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI R200 based chipsets written by
 *             Claudio Ciccani <klan@users.sf.net>.  
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __R200_H__
#define __R200_H__

#include <dfb_types.h>

#include <core/coretypes.h>
#include <core/state.h>
#include <core/layers.h>
#include <core/screens.h>


enum {
     CHIP_R200,
     CHIP_RV250,
     CHIP_RV280,
     CHIP_RS300,
     CHIP_RS350
};

typedef struct { 
     /* validated flags */
     StateModificationFlags  set;
     /* current function */
     DFBAccelerationMask     accel;

     __u32                   fb_offset;
     
     DFBSurfacePixelFormat   dst_format;
     __u32                   dst_offset;
     __u32                   dst_pitch;

     DFBSurfacePixelFormat   src_format;
     __u32                   src_offset;
     __u32                   src_pitch;
     __u32                   src_width;
     __u32                   src_height;
     __u32                   src_mask;

     DFBSurfaceDrawingFlags  drawingflags;
     DFBSurfaceBlittingFlags blittingflags;
 
     /* registers */
     __u32                   dp_gui_master_cntl;
     __u32                   rb3d_cntl;
     __u32                   txformat;

     /* chipset identified */
     __u32                   chipset;

     /* for 2d<->3d engine switching */
     bool                    write_2d;
     bool                    write_3d; 
     
     /* for fifo/performance monitoring */
     unsigned int            fifo_space;
     
     unsigned int            waitfifo_sum;
     unsigned int            waitfifo_calls;
     unsigned int            fifo_waitcycles;
     unsigned int            idle_waitcycles;
     unsigned int            fifo_cache_hits;
} R200DeviceData;

typedef struct {
     R200DeviceData         *device_data; 
     volatile __u8          *mmio_base;
} R200DriverData;


extern ScreenFuncs       R200PrimaryScreenFuncs;
extern DisplayLayerFuncs R200OverlayFuncs;


#endif /* __R200_H__ */

