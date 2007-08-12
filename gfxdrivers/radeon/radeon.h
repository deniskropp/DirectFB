/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI Radeon cards written by
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

#ifndef __RADEON_H__
#define __RADEON_H__

#include <dfb_types.h>

#include <core/coretypes.h>
#include <core/state.h>
#include <core/screens.h>
#include <core/layers.h>


typedef enum {
     CHIP_UNKNOWN = 0,
     CHIP_R100,
     CHIP_RV100,
     CHIP_RS100,
     CHIP_RV200,
     CHIP_RS200,
     CHIP_RS250,
     CHIP_R200,
     CHIP_RV250,
     CHIP_RV280,
     CHIP_RS300,
     CHIP_RS350,
     CHIP_R300,
     CHIP_R350,
     CHIP_RV350,
     CHIP_RV380,
     CHIP_R420,
     CHIP_RV410,
     CHIP_RS400,
} RadeonChipsetFamily;

typedef enum {
     MT_NONE = 0,
     MT_CRT  = 1,
     MT_DFP  = 2,
     MT_LCD  = 3,
     MT_CTV  = 4,
     MT_STV  = 5
} RadeonMonitorType;

typedef struct { 
     /* validated flags */
     StateModificationFlags  set;
     /* current function */
     DFBAccelerationMask     accel;
     /* mask of currently supported drawing functions */
     DFBAccelerationMask     drawing_mask;
     /* mask of currently supported blitting functions */
     DFBAccelerationMask     blitting_mask;

     unsigned long           fb_phys;
     u32                     fb_offset;
     u32                     fb_size;
     u32                     agp_offset;
     u32                     agp_size;
     
     DFBSurfacePixelFormat   dst_format;
     u32                     dst_offset;
     u32                     dst_offset_cb;
     u32                     dst_offset_cr;
     u32                     dst_pitch;
     DFBBoolean              dst_422;
     
     DFBSurfacePixelFormat   src_format;
     u32                     src_offset;
     u32                     src_offset_cb;
     u32                     src_offset_cr;
     u32                     src_pitch;
     u32                     src_width;
     u32                     src_height;
     u32                     src_mask;

     DFBRegion               clip;

     float                   color[4];
     u32                     y_cop;
     u32                     cb_cop;
     u32                     cr_cop;

     DFBSurfaceDrawingFlags  drawingflags;
     DFBSurfaceBlittingFlags blittingflags;
     
     /* chipset identified */
     RadeonChipsetFamily     chipset;
     DFBBoolean              igp;
     
     /* connected monitors */
     RadeonMonitorType       monitor1;
     RadeonMonitorType       monitor2;
 
     /* saved registers */
     u32                     mc_fb_location;
     u32                     mc_agp_location;
     u32                     crtc_base_addr;
     u32                     crtc2_base_addr;
     u32                     agp_base;
     u32                     agp_cntl;
     u32                     aic_cntl;
     u32                     bus_cntl;
     u32                     fcp_cntl;
     u32                     cap0_trig_cntl;
     u32                     vid_buffer_control;
     u32                     display_test_debug_cntl;
     u32                     surface_cntl;
     u32                     dp_gui_master_cntl;

     /* recorded registers */
     u32                     surface_cntl_p;
     u32                     surface_cntl_c;
     u32                     gui_master_cntl;
     u32                     rb3d_cntl;
     u32                     rb3d_blend;

     /* faked texture for YUV422 drawing functions */
     u32                     yuv422_buffer;
     
     /* for fifo/performance monitoring */
     unsigned int            fifo_space;
     
     unsigned int            waitfifo_sum;
     unsigned int            waitfifo_calls;
     unsigned int            fifo_waitcycles;
     unsigned int            idle_waitcycles;
     unsigned int            fifo_cache_hits;
} RadeonDeviceData;

typedef struct {
     RadeonDeviceData       *device_data; 
     
     u8                     *fb_base;
     volatile u8            *mmio_base;
     unsigned int            mmio_size;
} RadeonDriverData;


extern void radeon_reset( RadeonDriverData *rdrv, RadeonDeviceData *rdev );

extern ScreenFuncs        RadeonCrtc1ScreenFuncs;
extern ScreenFuncs        OldPrimaryScreenFuncs;
extern void              *OldPrimaryScreenDriverData;

extern DisplayLayerFuncs  RadeonCrtc1LayerFuncs;
extern DisplayLayerFuncs  OldPrimaryLayerFuncs;
extern void              *OldPrimaryLayerDriverData;

extern DisplayLayerFuncs  RadeonOverlayFuncs;

extern ScreenFuncs        RadeonCrtc2ScreenFuncs;

extern DisplayLayerFuncs  RadeonCrtc2LayerFuncs;


/* utility function */
static __inline__ u32 f2d( float f )
{
     union {
          float f;
          u32 d;
     } tmp;
     tmp.f = f;
     return tmp.d;
}     


#endif /* __RADEON_H__ */
