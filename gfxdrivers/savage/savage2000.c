/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <asm/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>

#include <gfx/util.h>
#include <misc/conf.h>

#include "savage.h"
#include "savage2000.h"
#include "mmio.h"


/* state validation */


/* required implementations */

static void savage2000EngineSync( void *drv, void *dev )
{
     Savage2000DriverData *sdrv = (Savage2000DriverData*) drv;
     Savage2000DeviceData *sdev = (Savage2000DeviceData*) dev;
     
     savage2000_waitidle( sdrv, sdev );
}

#define SAVAGE2000_DRAWING_FLAGS \
               (DSDRAW_NOFX)

#define SAVAGE2000_DRAWING_FUNCTIONS \
               (DFXL_NONE)

#define SAVAGE2000_BLITTING_FLAGS \
               (DSBLIT_NOFX)

#define SAVAGE2000_BLITTING_FUNCTIONS \
               (DFXL_NONE)


static void savage2000CheckState( void *drv, void *dev,
                                  CardState *state, DFBAccelerationMask accel )
{
}

static void savage2000SetState( void *drv, void *dev,
                                GraphicsDeviceFuncs *funcs,
                                CardState *state, DFBAccelerationMask accel )
{
}

static void savage2000FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
}

static void savage2000DrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
}

static void savage2000DrawLine( void *drv, void *dev, DFBRegion *line )
{
}

static void savage2000FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
}

static void savage2000Blit( void *drv, void *dev,
                            DFBRectangle *rect, int dx, int dy )
{
}

static void savage2000StretchBlit( void *drv, void *dev,
                                   DFBRectangle *sr, DFBRectangle *dr )
{
}

/* exported symbols */

void
savage2000_get_info( GraphicsDevice     *device,
                     GraphicsDriverInfo *info )
{
     info->version.major = 0;
     info->version.minor = 0;

     info->driver_data_size = sizeof (Savage2000DriverData);
     info->device_data_size = sizeof (Savage2000DeviceData);
}

DFBResult
savage2000_init_driver( GraphicsDevice      *device,
                        GraphicsDeviceFuncs *funcs,
                        void                *driver_data )
{
     funcs->CheckState    = savage2000CheckState;
     funcs->SetState      = savage2000SetState;
     funcs->EngineSync    = savage2000EngineSync;          

     funcs->FillRectangle = savage2000FillRectangle;
     funcs->DrawRectangle = savage2000DrawRectangle;
     funcs->DrawLine      = savage2000DrawLine;
     funcs->FillTriangle  = savage2000FillTriangle;
     funcs->Blit          = savage2000Blit;
     funcs->StretchBlit   = savage2000StretchBlit;

     return DFB_OK;
}

DFBResult
savage2000_init_device( GraphicsDevice     *device,
                        GraphicsDeviceInfo *device_info,
                        void               *driver_data,
                        void               *device_data )
{
     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Savage2000 Series" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "S3" );


     device_info->caps.flags    = 0;
     device_info->caps.accel    = SAVAGE2000_DRAWING_FUNCTIONS |
                                  SAVAGE2000_BLITTING_FUNCTIONS;
     device_info->caps.drawing  = SAVAGE2000_DRAWING_FLAGS;
     device_info->caps.blitting = SAVAGE2000_BLITTING_FLAGS;

     device_info->limits.surface_byteoffset_alignment = 2048;
     device_info->limits.surface_pixelpitch_alignment = 32;

     return DFB_OK;
}

void
savage2000_close_device( GraphicsDevice *device,
                         void           *driver_data,
                         void           *device_data )
{
}

void
savage2000_close_driver( GraphicsDevice *device,
                         void           *driver_data )
{
}

