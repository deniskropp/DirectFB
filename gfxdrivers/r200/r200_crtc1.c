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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <dfb_types.h>
#include <directfb.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/screens.h>
#include <core/layer_control.h>
#include <core/system.h>

#include <fbdev/fbdev.h>

#include <misc/conf.h>

#include "r200.h"
#include "r200_regs.h"
#include "r200_mmio.h"



static DFBResult
crtc1InitScreen( CoreScreen           *screen,
                 GraphicsDevice       *device,
                 void                 *driver_data,
                 void                 *screen_data,
                 DFBScreenDescription *description )
{
     R200DriverData *rdrv = (R200DriverData*) driver_data;
     
     (void) rdrv;
     
     /* Set the screen capabilities. */
     description->caps = DSCCAPS_VSYNC | DSCCAPS_POWER_MANAGEMENT;

     /* Set the screen name. */
     snprintf( description->name,
               DFB_SCREEN_DESC_NAME_LENGTH, "Radeon200 Primary Screen" );
     
     return DFB_OK;
}

static DFBResult
crtc1SetPowerMode( CoreScreen         *screen,
                   void               *driver_data,
                   void               *screen_data,
                   DFBScreenPowerMode  mode )
{
     FBDev *fbdev = dfb_system_data();
     int    level;

     switch (mode) {
          case DSPM_OFF:
               level = 4;
               break;
          case DSPM_SUSPEND:
               level = 3;
               break;
          case DSPM_STANDBY:
               level = 2;
               break;
          case DSPM_ON:
               level = 0;
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     if (ioctl( fbdev->fd, FBIOBLANK, level ) < 0) {
          D_PERROR( "DirectFB/R200/Crtc1: display blanking failed!\n" );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
crtc1WaitVSync( CoreScreen *screen,
                void       *driver_data,
                void       *screen_data )
{
     R200DriverData *rdrv = (R200DriverData*) driver_data;
     volatile __u8  *mmio = rdrv->mmio_base; 
     int             i;
     
     if (dfb_config->pollvsync_none)
          return DFB_OK;
          
     r200_out32( mmio, GEN_INT_STATUS, VSYNC_INT_AK );
     
     for (i = 0; i < 2000000; i++) {
          struct timespec t = { 0, 0 };     
          
          if (r200_in32( mmio, GEN_INT_STATUS ) & VSYNC_INT)
               break;
          nanosleep( &t, NULL );
     }

     return DFB_OK;
}

static DFBResult
crtc1GetScreenSize( CoreScreen *screen,
                    void       *driver_data,
                    void       *screen_data,
                    int        *ret_width,
                    int        *ret_height )
{
     R200DriverData *rdrv    = (R200DriverData*) driver_data;
     volatile __u8  *mmio    = rdrv->mmio_base;
     __u32           h_total;
     __u32           v_total;
     __u32           xres;
     __u32           yres;

     h_total = r200_in32( mmio, CRTC_H_TOTAL_DISP );
     v_total = r200_in32( mmio, CRTC_V_TOTAL_DISP );
     
     xres = ((h_total >> 16) + 1) << 3;
     yres = ((v_total >> 16) + 1);

     D_DEBUG( "DirectFB/R200/Crtc1: "
              "detected screen size %dx%d.\n", xres, yres );

     *ret_width  = xres;
     *ret_height = yres;
     
     return DFB_OK;
}


ScreenFuncs R200PrimaryScreenFuncs = {
     .InitScreen    = crtc1InitScreen,
     .SetPowerMode  = crtc1SetPowerMode,
     .WaitVSync     = crtc1WaitVSync,
     .GetScreenSize = crtc1GetScreenSize
};

