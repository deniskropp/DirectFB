/*
   Copyright (C) 2005 Claudio Ciccani <klan82@cheapnet.it>

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
#include <core/surfaces.h>
#include <core/gfxcard.h>
#include <core/system.h>
#include <core/screen.h>
#include <core/layer_control.h>

#include <misc/conf.h>

#include <direct/messages.h>

#include "nvidia.h"
#include "nvidia_mmio.h"



DisplayLayerFuncs  nvidiaOldPrimaryLayerFuncs;
void              *nvidiaOldPrimaryLayerDriverData;


/************************** Primary Screen functions **************************/

static int
nvcrtc1ScreenDataSize( void )
{
     return 0;
}

static DFBResult
nvcrtc1InitScreen( CoreScreen           *screen,
                   GraphicsDevice       *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     volatile __u32   *PCRTC = nvdrv->PCRTC;
     
     description->caps = DSCCAPS_VSYNC | DSCCAPS_POWER_MANAGEMENT;

     snprintf( description->name,
               DFB_SCREEN_DESC_NAME_LENGTH, "NVidia Primary Screen" );

     /* NV_PCRTC_INTR_EN_0 */
     nv_out32( PCRTC, 0x140, 0x00000000 );
     /* NV_PCRTC_CONFIG */
#ifdef WORDS_BIGENDIAN
     nv_out32( PCRTC, 0x804, 0x80000002 );
#else
     nv_out32( PCRTC, 0x804, 0x00000002 );
#endif
     /* NV_PCRTC_INTR_0 */
     nv_out32( PCRTC, 0x100, 0x00000001 );

     return DFB_OK;
}

static DFBResult
nvcrtc1SetPowerMode( CoreScreen         *screen,
                     void               *driver_data,
                     void               *screen_data,
                     DFBScreenPowerMode  mode )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     volatile __u8    *PVIO  = nvdrv->PVIO;
     volatile __u8    *PCIO  = nvdrv->PCIO;
     __u8              sr;
     __u8              cr;

     nv_out8( PVIO, 0x3C4, 0x01 );
     sr = nv_in8( PVIO, 0x3C5 ) & ~0x20; /* screen on/off */

     nv_out8( PCIO, 0x3D4, 0x1A );
     cr = nv_in8( PCIO, 0x3D5 ) & ~0xC0; /* sync on/off */

     switch (mode) {
          case DSPM_OFF:
               sr |= 0x20;
               cr |= 0xC0;
               break;
          case DSPM_SUSPEND:
               sr |= 0x20;
               cr |= 0x40;
               break;
          case DSPM_STANDBY:
               sr |= 0x20;
               cr |= 0x80;
               break;
          case DSPM_ON:
               break;
          default:
               return DFB_INVARG;
     }

     nv_out8( PVIO, 0x3C4, 0x01 );
     nv_out8( PVIO, 0x3C5, sr );

     nv_out8( PCIO, 0x3D4, 0x1A );
     nv_out8( PCIO, 0x3D5, cr );

     return DFB_OK;
}

static DFBResult
nvcrtc1WaitVSync( CoreScreen *screen,
                  void       *driver_data,
                  void       *screen_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     volatile __u8    *PCIO  = nvdrv->PCIO;

     if (!dfb_config->pollvsync_none) {
          while (  nv_in8( PCIO, 0x3DA ) & 8 );
          while (!(nv_in8( PCIO, 0x3DA ) & 8));
          // the same but uses PCRTC
          //while (  nv_in32( nvdrv->PCRTC, 0x808 ) & 0x10000 );
          //while (!(nv_in32( nvdrv->PCRTC, 0x808 ) & 0x10000));
     }

     return DFB_OK;
}

static DFBResult
nvcrtc1GetScreenSize( CoreScreen *screen,
                      void       *driver_data,
                      void       *screen_data,
                      int        *ret_width,
                      int        *ret_height )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     volatile __u8    *PCIO  = nvdrv->PCIO;
     int               w, h;
     int               val;

     /* stolen from RivaTV */
     
	/* NV_PCRTC_HORIZ_DISPLAY_END */
	nv_out8( PCIO, 0x3D4, 0x01 );
	w = nv_in8( PCIO, 0x3D5 );
	/* NV_PCRTC_HORIZ_EXTRA_DISPLAY_END_8 */
	nv_out8( PCIO, 0x3D4, 0x2D );
	w |= (nv_in8( PCIO, 0x3D5) & 0x02) << 7;
     w  = (w + 1) << 3;
	
     /* NV_PCRTC_VERT_DISPLAY_END */
	nv_out8( PCIO, 0x3D4, 0x12 );
	h = nv_in8( PCIO, 0x3D5 );     
     /* NV_PCRTC_OVERFLOW_VERT_DISPLAY_END_[89] */
	nv_out8( PCIO, 0x3D4, 0x07 );
	val = nv_in8( PCIO, 0x3D5 );
	h |= (val & 0x02) << 7;
     h |= (val & 0x40) << 3;
     h++;
	/* NV_PCRTC_EXTRA_VERT_DISPLAY_END_10 */
	nv_out8( PCIO, 0x3D4, 0x25 );
	h |= (nv_in8( PCIO, 0x3D5 ) & 0x02) << 9;
	/* NV_PCRTC_???_VERT_DISPLAY_END_11 */
	nv_out8( PCIO, 0x3D4, 0x41 );
	h |= (nv_in8( PCIO, 0x3D5) & 0x04) << 9;
	/* NV_PCRTC_MAX_SCAN_LINE_DOUBLE_SCAN */
	nv_out8( PCIO, 0x3D4, 0x09 );
	h >>= (nv_in8( PCIO, 0x3D5 ) & 0x80) >> 7;

     D_DEBUG( "DirectFB/NVidia/Crtc1: "
              "detected screen resolution %dx%d.\n", w, h );

     *ret_width  = w;
     *ret_height = h;
     
     return DFB_OK;
}


ScreenFuncs nvidiaPrimaryScreenFuncs = {
     .ScreenDataSize = nvcrtc1ScreenDataSize,
     .InitScreen     = nvcrtc1InitScreen,
     .SetPowerMode   = nvcrtc1SetPowerMode,
     .WaitVSync      = nvcrtc1WaitVSync,
     .GetScreenSize  = nvcrtc1GetScreenSize
};


/*************************** Primary Layer hooks ******************************/

static DFBResult
nvfb0FlipRegion( CoreLayer           *layer,
                 void                *driver_data,
                 void                *layer_data,
                 void                *region_data,
                 CoreSurface         *surface,
                 DFBSurfaceFlipFlags  flags )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) driver_data;
     SurfaceBuffer    *buffer = surface->back_buffer;
     __u32             offset = (buffer->video.offset + nvdrv->fb_offset) &
                                 nvdrv->fb_mask;

     dfb_gfxcard_sync();

     /* NV_PCRTC_START */
     nv_out32( nvdrv->PCRTC, 0x800, offset );

     if (flags & DSFLIP_WAIT)
          dfb_screen_wait_vsync( dfb_screens_at( DSCID_PRIMARY ) );

     dfb_surface_flip_buffers( surface, false );

     return DFB_OK;
}


DisplayLayerFuncs nvidiaPrimaryLayerFuncs = {
     .FlipRegion     = nvfb0FlipRegion
};

