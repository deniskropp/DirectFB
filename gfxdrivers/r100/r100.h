/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI R!00 based chipsets written by
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

#ifndef __R100_H__
#define __R100_H__

#include <dfb_types.h>

#include <core/coretypes.h>
#include <core/state.h>
#include <core/layers.h>
#include <core/screens.h>


enum {
     CHIP_R100,
     CHIP_RV100,
     CHIP_RS100,
     CHIP_RV200,
     CHIP_RS200,
     CHIP_RS250
};

typedef struct { 
     /* validated flags */
     StateModificationFlags  set;
     /* current function */
     DFBAccelerationMask     accel;
     /* mask of currently supported drawing functions */
     DFBAccelerationMask     drawing_mask;
     /* mask of currently supported blitting functions */
     DFBAccelerationMask     blitting_mask;

     __u32                   fb_offset;
     __u32                   agp_offset;
    
     DFBSurfacePixelFormat   dst_format;
     __u32                   dst_offset;
     __u32                   dst_offset_cb;
     __u32                   dst_offset_cr;
     __u32                   dst_pitch;
     bool                    dst_422;
     
     DFBSurfacePixelFormat   src_format;
     __u32                   src_offset;
     __u32                   src_offset_cb;
     __u32                   src_offset_cr;
     __u32                   src_pitch;
     __u32                   src_width;
     __u32                   src_height;
     __u32                   src_mask;
     __u32                   src_key;

     DFBRegion               clip;

     __u32                   y_cop;
     __u32                   cb_cop;
     __u32                   cr_cop;

     DFBSurfaceDrawingFlags  drawingflags;
     DFBSurfaceBlittingFlags blittingflags;
 
     /* registers */
     __u32                   surface_cntl;
     __u32                   dp_gui_master_cntl;
     __u32                   rb3d_cntl;

     /* chipset identified */
     __u32                   chipset;
     bool                    igp;
     
     /* for 2d<->3d engine switching */
     bool                    write_2d;
     bool                    write_3d;

     /* faked texture for YUV422 drawing functions */
     __u32                   yuv422_buffer;
     
     /* for fifo/performance monitoring */
     unsigned int            fifo_space;
     
     unsigned int            waitfifo_sum;
     unsigned int            waitfifo_calls;
     unsigned int            fifo_waitcycles;
     unsigned int            idle_waitcycles;
     unsigned int            fifo_cache_hits;
} R100DeviceData;

typedef struct {
     R100DeviceData         *device_data; 
     
     __u8                   *fb_base;
     volatile __u8          *mmio_base;
} R100DriverData;


extern void r100_reset( R100DriverData *rdrv, R100DeviceData *rdev );


extern DisplayLayerFuncs  R100OverlayFuncs;


#endif /* __R100_H__ */

