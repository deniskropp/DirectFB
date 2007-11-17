/*
   Copyright (C) 2005-2006 Claudio Ciccani <klan@users.sf.net>

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/surface.h>
#include <core/gfxcard.h>
#include <core/system.h>
#include <core/screen.h>
#include <core/layer_control.h>

#include <misc/conf.h>

#include <direct/messages.h>

#include "nvidia.h"
#include "nvidia_regs.h"
#include "nvidia_accel.h"


/************************** Primary Screen functions **************************/

static DFBResult
crtc1InitScreen( CoreScreen           *screen,
                 CoreGraphicsDevice   *device,
                 void                 *driver_data,
                 void                 *screen_data,
                 DFBScreenDescription *description )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     volatile u8      *mmio  = nvdrv->mmio_base;

     if (OldPrimaryScreenFuncs.InitScreen)
          OldPrimaryScreenFuncs.InitScreen( screen, device, 
                                            OldPrimaryScreenDriverData,
                                            screen_data, description );
     
     description->caps |= DSCCAPS_VSYNC;

     snprintf( description->name,
               DFB_SCREEN_DESC_NAME_LENGTH, "NVidia Primary Screen" );

     nv_out32( mmio, PCRTC_INTR_EN, PCRTC_INTR_EN_VBLANK_DISABLED );
#ifdef WORDS_BIGENDIAN
     nv_out32( mmio, PCRTC_CONFIG, PCRTC_CONFIG_SIGNAL_HSYNC |
                                   PCRTC_CONFIG_ENDIAN_BIG );
#else
     nv_out32( mmio, PCRTC_CONFIG, PCRTC_CONFIG_SIGNAL_HSYNC |
                                   PCRTC_CONFIG_ENDIAN_LITTLE );
#endif
     nv_out32( mmio, PCRTC_INTR, PCRTC_INTR_VBLANK_RESET );
     
     return DFB_OK;
}

static DFBResult
crtc1WaitVSync( CoreScreen *screen,
                void       *driver_data,
                void       *screen_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     volatile u8      *mmio  = nvdrv->mmio_base;

     if (!dfb_config->pollvsync_none) {
          int i;
          
          for (i = 0; i < 2000000; i++) {
               if (!(nv_in8( mmio, PCIO_CRTC_STATUS ) & 8))
                    break;
          }

          for (i = 0; i < 2000000;) {
               if (nv_in8( mmio, PCIO_CRTC_STATUS ) & 8)
                    break;
               
               i++;
               if ((i % 2000) == 0) {
                    struct timespec ts = { 0, 10000 }; 
                    nanosleep( &ts, NULL );
               }
          }
     }

     return DFB_OK;
}

#if 0
static DFBResult
crtc1GetScreenSize( CoreScreen *screen,
                    void       *driver_data,
                    void       *screen_data,
                    int        *ret_width,
                    int        *ret_height )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     volatile u8      *mmio  = nvdrv->mmio_base;
     int               w, h;
     int               val;

     /* stolen from RivaTV */
     
	w   = nv_incrtc( mmio, CRTC_HORIZ_DISPLAY_END );
	w  |= (nv_incrtc( mmio, CRTC_HORIZ_EXTRA ) & 0x02) << 7;
	w   = (w + 1) << 3;
	
	h   = nv_incrtc( mmio, CRTC_VERT_DISPLAY_END );
	val = nv_incrtc( mmio, CRTC_OVERFLOW );
	h  |= (val & 0x02) << 7;
	h  |= (val & 0x40) << 3;
	h++;
	h  |= nv_incrtc( mmio, CRTC_EXTRA ) << 9;
	h  |= nv_incrtc( mmio, 0x41 ) << 9;
	h >>= (nv_incrtc( mmio, CRTC_MAX_SCAN_LINE ) & 0x80) >> 7;
	
     D_DEBUG( "DirectFB/NVidia/Crtc1: "
              "detected screen resolution %dx%d.\n", w, h );

     *ret_width  = w;
     *ret_height = h;
     
     return DFB_OK;
}
#endif

ScreenFuncs nvidiaPrimaryScreenFuncs = {
     .InitScreen     = crtc1InitScreen,
     .WaitVSync      = crtc1WaitVSync,
     //.GetScreenSize  = crtc1GetScreenSize
};

ScreenFuncs  OldPrimaryScreenFuncs;
void        *OldPrimaryScreenDriverData;

/*************************** Primary Layer hooks ******************************/

static DFBResult
fb0FlipRegion( CoreLayer             *layer,
               void                  *driver_data,
               void                  *layer_data,
               void                  *region_data,
               CoreSurface           *surface,
               DFBSurfaceFlipFlags    flags,
               CoreSurfaceBufferLock *lock )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev  = nvdrv->device_data;
     u32               offset;

     dfb_surface_flip( surface, false );
     
     offset = (lock->offset + nvdev->fb_offset) & ~3;
     nv_out32( nvdrv->mmio_base, PCRTC_START, offset );

     if (flags & DSFLIP_WAIT)
          dfb_layer_wait_vsync( layer );

     return DFB_OK;
}


DisplayLayerFuncs nvidiaPrimaryLayerFuncs = {
     .FlipRegion     = fb0FlipRegion
};

DisplayLayerFuncs  OldPrimaryLayerFuncs;
void              *OldPrimaryLayerDriverData;
