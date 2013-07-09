/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <stdio.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/surface_pool.h>

#include "vmware_2d.h"
#include "vmware_gfxdriver.h"


#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( vmware )


/**********************************************************************************************************************/

static int
driver_probe( CoreGraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_VMWARE_BLITTER:
               return 1;
     }

     return 0;
}

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "VMWare Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Denis Oliver Kropp" );

     info->version.major = 0;
     info->version.minor = 0;

     info->driver_data_size = sizeof(VMWareDriverData);
     info->device_data_size = sizeof(VMWareDeviceData);
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     DirectResult      ret;
     VMWareDriverData *drv = driver_data;

     if (!fusion_config->secure_fusion || dfb_core_is_master(core)) {
          ret = direct_processor_init( &drv->processor, "Virtual2D", virtual2DFuncs, sizeof(Virtual2DPacket), drv, 0 );
          if (ret)
               return ret;
     }

     /* initialize function pointers */
     funcs->EngineSync    = vmwareEngineSync;
     funcs->WaitSerial    = vmwareWaitSerial;
     funcs->GetSerial     = vmwareGetSerial;
     funcs->EngineReset   = vmwareEngineReset;
     funcs->EmitCommands  = vmwareEmitCommands;
     funcs->CheckState    = vmwareCheckState;
     funcs->SetState      = vmwareSetState;
     funcs->FillRectangle = vmwareFillRectangle;
     funcs->Blit          = vmwareBlit;

     return DFB_OK;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     /* fill device info */
     snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "VMWare" );
     snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   "(fake) Blitter" );

     /* device limitations */
     device_info->limits.surface_byteoffset_alignment = 8;
     device_info->limits.surface_bytepitch_alignment  = 8;

     device_info->caps.flags    = CCF_READSYSMEM | CCF_WRITESYSMEM;
     device_info->caps.accel    = VMWARE_SUPPORTED_DRAWINGFUNCTIONS |
                                  VMWARE_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = VMWARE_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = VMWARE_SUPPORTED_BLITTINGFLAGS;

     dfb_surface_pool_gfx_driver_update( CSTF_ALL, CSAID_GPU, CSAF_READ | CSAF_WRITE | CSAF_SHARED );

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
     VMWareDriverData *drv = driver_data;

     if (!fusion_config->secure_fusion || dfb_core_is_master(core_dfb))
          direct_processor_destroy( &drv->processor );
}

