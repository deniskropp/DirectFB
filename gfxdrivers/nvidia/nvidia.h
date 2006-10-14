/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2006  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#ifndef __NVIDIA_H__
#define __NVIDIA_H__

#include <dfb_types.h>

#include <core/state.h>
#include <core/screens.h>
#include <core/layers.h>


/*
 * Object's identifier
 */
enum {
     OBJ_DMA_IN         = 0x00800000,
     OBJ_SURFACES2D     = 0x00800001,
     OBJ_SURFACES3D     = 0x00800002,
     OBJ_CLIP           = 0x00800003,
     OBJ_BETA1          = 0x00800004,
     OBJ_BETA4          = 0x00800005,
     OBJ_RECTANGLE      = 0x00800010,
     OBJ_TRIANGLE       = 0x00800011,
     OBJ_LINE           = 0x00800012,
     OBJ_SCREENBLT      = 0x00800013,
     OBJ_IMAGEBLT       = 0x00800014,
     OBJ_SCALEDIMAGE    = 0x00800015,
     OBJ_STRETCHEDIMAGE = 0x00800016,
     OBJ_TEXTRIANGLE    = 0x00800017,
     OBJ_DMA_OUT        = 0x00800018
};

/*
 * Object's offset into context table [PRAMIN + (address)*16]
 */
enum {
     ADDR_DMA_IN         = 0x1160,
     ADDR_SURFACES2D     = 0x1162,
     ADDR_SURFACES3D     = 0x1163,
     ADDR_CLIP           = 0x1164,
     ADDR_BETA1          = 0x1165,
     ADDR_BETA4          = 0x1166,
     ADDR_RECTANGLE      = 0x1167,
     ADDR_TRIANGLE       = 0x1168,
     ADDR_LINE           = 0x1169,
     ADDR_SCREENBLT      = 0x116A,
     ADDR_IMAGEBLT       = 0x116B,
     ADDR_SCALEDIMAGE    = 0x116C,
     ADDR_STRETCHEDIMAGE = 0x116D,
     ADDR_TEXTRIANGLE    = 0x116E,
     ADDR_DMA_OUT        = 0x116F
};

/*
 * Object's subchannel
 */
enum {
     SUBC_SURFACES2D     = 0,
     SUBC_SURFACES3D     = 0,
     SUBC_BETA1          = 0,
     SUBC_BETA4          = 0,
     SUBC_CLIP           = 1,
     SUBC_RECTANGLE      = 2,
     SUBC_TRIANGLE       = 3,
     SUBC_LINE           = 4,
     SUBC_SCREENBLT      = 5,
     SUBC_IMAGEBLT       = 5,
     SUBC_SCALEDIMAGE    = 6,
     SUBC_STRETCHEDIMAGE = 6,
     SUBC_TEXTRIANGLE    = 7
};
     

#define SMF_DRAWING_COLOR  (SMF_COLOR << 16)
#define SMF_BLITTING_COLOR (SMF_COLOR << 17)
#define SMF_SOURCE_TEXTURE (SMF_SOURCE << 1) 

typedef struct {
     StateModificationFlags  set;
     
     u32                     fb_offset;
     u32                     fb_size;
     u32                     agp_offset;
     
     DFBSurfacePixelFormat   dst_format;
     u32                     dst_offset;
     u32                     dst_pitch;
     bool                    dst_422;

     DFBSurfacePixelFormat   src_format;
     u32                     src_offset;
     u8                     *src_address;
     u32                     src_pitch;
     u32                     src_width;
     u32                     src_height;
     bool                    src_system;
     bool                    src_interlaced;

     DFBRectangle            clip;

     u32                     color2d;
     u32                     color3d;
     
     DFBSurfaceDrawingFlags  drawingflags;
     DFBSurfaceBlittingFlags blittingflags;
     
     /* NVRectangle/NVTriangle/NVLine registers */
     u32                     drawing_operation; // SetOperation
     
     /* NVScaledImage registers */
     u32                     scaler_operation;  // SetOperation
     u32                     scaler_format;     // SetColorFormat
     u32                     scaler_filter;     // SetImageInFormat
     
     /* NVImageBlt/NVStretchedImage registers */
     u32                     system_operation;  // SetOperation
     u32                     system_format;     // SetColorFormat

     /* Remember value of NVBeta1 & NVBeta4 */
     bool                    beta1_set;
     u32                     beta1_val;
     bool                    beta4_set;
     u32                     beta4_val;

     /* 3D stuff */
     bool                    enabled_3d;       // 3d engine enabled
     u32                     buf_offset[2];    // reserved buffers
     SurfaceBuffer          *src_texture;      // current source for TextureTriangles
     u32                     max_texture_size;
     
     struct {
          bool               modified;
          u32                colorkey;
          u32                offset;
          u32                format;
          u32                filter;
          u32                blend;
          u32                control;
          u32                fog;
     } state3d[2]; // 0 => drawing | 1 => blitting

     /* Remember subchannels configuration */
     u32                     subchannel_object[8];
     
     /* Chipsets informations */
     u32                     chip;
     u32                     arch;
     
     /* AGP control */
     bool                    use_agp;
     int                     agp_key;
     unsigned int            agp_aper_base;
     unsigned int            agp_aper_size;
     
     /* DMA control */
     bool                    use_dma;
     unsigned int            dma_size;
     unsigned int            dma_offset;
     unsigned int            dma_max;
     unsigned int            dma_cur;
     unsigned int            dma_free;
     unsigned int            dma_put;
     unsigned int            dma_get;
     volatile u32           *cmd_ptr;

     /* FIFO control */
     unsigned int            fifo_free;

     /* for performance monitoring */
     unsigned int            waitfree_sum;
     unsigned int            waitfree_calls;
     unsigned int            free_waitcycles;
     unsigned int            idle_waitcycles;
     unsigned int            cache_hits;
} NVidiaDeviceData;


enum {
     NV_ARCH_04 = 0x04,
     NV_ARCH_05 = 0x05,
     NV_ARCH_10 = 0x10,
     NV_ARCH_20 = 0x20,
     NV_ARCH_30 = 0x30
};

typedef struct {
     GraphicsDevice         *device;
     NVidiaDeviceData       *device_data;

     volatile void          *fb_base;
     volatile void          *agp_base;
     volatile void          *mmio_base; 
     volatile void          *dma_base;
} NVidiaDriverData;


extern ScreenFuncs        nvidiaPrimaryScreenFuncs;
extern ScreenFuncs        OldPrimaryScreenFuncs;
extern void              *OldPrimaryScreenDriverData;

extern DisplayLayerFuncs  nvidiaPrimaryLayerFuncs;
extern DisplayLayerFuncs  OldPrimaryLayerFuncs;
extern void              *OldPrimaryLayerDriverData;

extern DisplayLayerFuncs  nvidiaOverlayFuncs;


#endif /* __NVIDIA_H__ */
