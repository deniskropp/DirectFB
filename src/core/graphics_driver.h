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

#ifndef __GRAPHICS_DRIVER_H__
#define __GRAPHICS_DRIVER_H__

#include <core/gfxcard.h>
#include <core/modules.h>


static int
driver_probe( GraphicsDevice *device );

static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info );

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data );

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data );

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data );

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data );

static GraphicsDriverFuncs driver_funcs = {
     Probe:              driver_probe,
     GetDriverInfo:      driver_get_info,
     InitDriver:         driver_init_driver,
     InitDevice:         driver_init_device,
     CloseDevice:        driver_close_device,
     CloseDriver:        driver_close_driver
};

#define DFB_GRAPHICS_DRIVER(shortname)                      \
__attribute__((constructor)) void directfb_##shortname();   \
                                                            \
void                                                        \
directfb_##shortname()                                      \
{                                                           \
     dfb_modules_register( &dfb_graphics_drivers,           \
                           DFB_GRAPHICS_DRIVER_ABI_VERSION, \
                           #shortname, &driver_funcs );     \
}

#endif
