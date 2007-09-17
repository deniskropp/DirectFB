/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <sys/ioctl.h>

#include <dfb_types.h>

#include <fbdev/fb.h>
#include "omapfb.h"

#include <directfb.h>

#include <direct/util.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>

#include <fbdev/fbdev.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( omap )

#include "omap.h"

/* */

static DFBResult
omapEngineSync( void *drv, void *dev )
{
     FBDev *dfb_fbdev = dfb_system_data();

     /* FIXME needed? */
     ioctl( dfb_fbdev->fd, OMAPFB_SYNC_GFX );
}

/* exported symbols */

static int
driver_probe( CoreGraphicsDevice *device )
{
     FBDev *dfb_fbdev = dfb_system_data();
     struct omapfb_caps caps;

     if (ioctl( dfb_fbdev->fd, OMAPFB_GET_CAPS, &caps))
          return 0;

     return 1;
}

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *driver_info )
{
     driver_info->version.major = 0;
     driver_info->version.minor = 1;

     direct_snputs( driver_info->name,
                    "TI OMAP Driver", DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH );
     direct_snputs( driver_info->vendor,
                    "Ville Syrjala", DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH );
     direct_snputs( driver_info->url,
                    "http://www.directfb.org", DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH );
     direct_snputs( driver_info->license,
                    "LGPL", DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH );

#if 0
     driver_info->driver_data_size = sizeof (OmapDriverData);
     driver_info->device_data_size = sizeof (OmapDeviceData);
#endif
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     funcs->EngineSync = omapEngineSync;

     dfb_layers_hook_primary( device, driver_data, &omapPrimaryLayerFuncs, NULL, NULL );

     return DFB_OK;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     direct_snputs( device_info->name,
                    "OMAP", DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH );
     direct_snputs( device_info->vendor,
                    "TI", DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH );

     return DFB_OK;
}

static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data )
{
}

static void
driver_close_driver( CoreGraphicsDevice *device,
                     void               *driver_data )
{
}
