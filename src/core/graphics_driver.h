/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

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

#ifndef __GRAPHICS_DRIVER_H__
#define __GRAPHICS_DRIVER_H__

#include <direct/modules.h>

#include <core/gfxcard.h>


static int
driver_probe( CoreGraphicsDevice *device );

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *info );

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core );

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data );

static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data );

static void
driver_close_driver( CoreGraphicsDevice *device,
                     void               *driver_data );

static GraphicsDriverFuncs driver_funcs = {
     .Probe              = driver_probe,
     .GetDriverInfo      = driver_get_info,
     .InitDriver         = driver_init_driver,
     .InitDevice         = driver_init_device,
     .CloseDevice        = driver_close_device,
     .CloseDriver        = driver_close_driver
};

#define DFB_GRAPHICS_DRIVER(shortname)                                     \
__attribute__((constructor)) void directfb_##shortname##_ctor( void );     \
__attribute__((destructor))  void directfb_##shortname##_dtor( void );     \
                                                                           \
void                                                                       \
directfb_##shortname##_ctor( void )                                        \
{                                                                          \
     direct_modules_register( &dfb_graphics_drivers,                       \
                              DFB_GRAPHICS_DRIVER_ABI_VERSION,             \
                              #shortname, &driver_funcs );                 \
}                                                                          \
                                                                           \
void                                                                       \
directfb_##shortname##_dtor( void )                                        \
{                                                                          \
     direct_modules_unregister( &dfb_graphics_drivers,                     \
                                #shortname );                              \
}

#endif
