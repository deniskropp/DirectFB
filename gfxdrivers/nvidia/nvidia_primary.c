/*
   Copyright (C) 2004 Claudio Ciccani <klan82@cheapnet.it>

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

     /* turn off VBlank enable */
     PCRTC[0x140/4] = 0x00000000;
     /* set screen type (0=vga, 2=hsync) */
#ifdef WORDS_BIGENDIAN
     PCRTC[0x804/4] = 0x80000002;
#else
     PCRTC[0x804/4] = 0x00000002;
#endif
     /* reset VBlank */
     PCRTC[0x100/4] = 0x00000001;

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

     PVIO[0x3C4] = 0x01;
     sr = PVIO[0x3C5] & ~0x20; /* screen on/off */

     PCIO[0x3D4] = 0x1A;
     cr = PCIO[0x3D5] & ~0xC0; /* sync on/off */

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

     PVIO[0x3C4] = 0x01;
     PVIO[0x3C5] = sr;

     PCIO[0x3D4] = 0x1A;
     PCIO[0x3D5] = cr;

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
          /* not the right way, use with caution */
          while (  PCIO[0x3DA] & 8 );
          while (!(PCIO[0x3DA] & 8));
          // the same but uses PCRTC
          //while (  PCRTC[0x808/4] & 0x10000 );
          //while (!(PCRTC[0x808/4] & 0x10000));
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
     VideoMode *mode;

     /* FIXME: detect video mode from hardware configuration */
     mode = dfb_system_current_mode();

     if (!mode)
          mode = dfb_system_modes();

     if (!mode) {
           D_WARN( "no default mode found" );
           return DFB_UNSUPPORTED;
     }

     *ret_width  = mode->xres;
     *ret_height = mode->yres;

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
     __u32             offset = surface->back_buffer->video.offset;

     dfb_gfxcard_sync();

     if (nvdrv->chip == 0x2A0)
          offset += nvdrv->fb_base;
     offset &= nvdrv->fb_mask;

     nvdrv->PCRTC[0x800/4] = offset;

     if (flags & DSFLIP_WAIT)
          dfb_screen_wait_vsync( dfb_screens_at( DSCID_PRIMARY ) );

     dfb_surface_flip_buffers( surface, false );

     return DFB_OK;
}


DisplayLayerFuncs nvidiaPrimaryLayerFuncs = {
     .FlipRegion     = nvfb0FlipRegion
};

