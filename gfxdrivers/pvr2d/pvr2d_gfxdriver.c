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

//#define DIRECT_ENABLE_DEBUG

#include <stdio.h>
#include <string.h>

#include <directfb.h>
#include <direct/debug.h>
#include <direct/messages.h>

#include <core/gfxcard.h>
#include <core/system.h>

#include <misc/conf.h>


#include "pvr2d_2d.h"
#include "pvr2d_gfxdriver.h"

#include <core/graphics_driver.h>

D_DEBUG_DOMAIN(PVR2D__2D, "PVR2D/2D", "PVR2D Acceleration");

DFB_GRAPHICS_DRIVER(pvr2d)


/*****************************************************************************/

static int
driver_probe(CoreGraphicsDevice *device)
{
     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);

     switch (dfb_system_type()) {
          case CORE_PVR2D:
          case CORE_CARE1:
               return 1;

          default:
               break;
     }

     return 0;
}

static void
driver_get_info(CoreGraphicsDevice *device,
                GraphicsDriverInfo *info)
{
     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);

     // fill driver info structure
     snprintf(info->name,
              DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
              "PVR2D Driver");

     snprintf(info->vendor,
              DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
              "Denis Oliver Kropp");

     info->version.major = 0;
     info->version.minor = 1;

     info->driver_data_size = sizeof(PVR2DDriverData);
     info->device_data_size = sizeof(PVR2DDeviceData);
}

static DFBResult
driver_init_driver(CoreGraphicsDevice  *device,
                   GraphicsDeviceFuncs *funcs,
                   void                *driver_data,
                   void                *device_data,
                   CoreDFB             *core)
{
     PVR2DDriverData *drv = driver_data;
     PVR2DERROR       ePVR2DStatus;

     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);

     // initialize function pointers
     funcs->EngineSync    = pvr2dEngineSync;
     funcs->EngineReset   = pvr2dEngineReset;
     funcs->EmitCommands  = pvr2dEmitCommands;
     funcs->CheckState    = pvr2dCheckState;
     funcs->SetState      = pvr2dSetState;
     funcs->FillRectangle = pvr2dFillRectangle;
     funcs->DrawRectangle = pvr2dDrawRectangle;
     funcs->DrawLine      = pvr2dDrawLine;
     funcs->FillTriangle  = pvr2dFillTriangle;
     funcs->Blit          = pvr2dBlit;
     funcs->StretchBlit   = pvr2dStretchBlit;

     // Choose accelerated font format
     if (!dfb_config->software_only) {
          dfb_config->font_format  = DSPF_ARGB;
          dfb_config->font_premult = true;
     }

     drv->nDevices = PVR2DEnumerateDevices(0);
     if (drv->nDevices < 1) {
          D_ERROR( "DirectFB/CarE1: PVR2DEnumerateDevices(0) returned %d!\n", drv->nDevices );
          return DFB_INIT;
     }

     drv->pDevInfo = (PVR2DDEVICEINFO *) malloc(drv->nDevices * sizeof(PVR2DDEVICEINFO));

     PVR2DEnumerateDevices(drv->pDevInfo);

     drv->nDeviceNum = drv->pDevInfo[0].ulDevID;

     ePVR2DStatus = PVR2DCreateDeviceContext (drv->nDeviceNum, &drv->hPVR2DContext, 0);
     if (ePVR2DStatus) {
          D_ERROR( "DirectFB/CarE1: PVR2DCreateDeviceContext() failed! (status %d)\n", ePVR2DStatus );
          return DFB_INIT;
     }

     return DFB_OK;
}

static DFBResult
driver_init_device(CoreGraphicsDevice *device,
                   GraphicsDeviceInfo *device_info,
                   void               *driver_data,
                   void               *device_data)
{
     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);

     // Fill device info.
     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "PVR2D" );
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Blt" );

     /* device limitations */
     device_info->limits.surface_byteoffset_alignment = 8;
     device_info->limits.surface_bytepitch_alignment  = 8;

     device_info->caps.flags    = /*CCF_CLIPPING |*/ CCF_RENDEROPTS;
     device_info->caps.accel    = PVR2D_SUPPORTED_DRAWINGFUNCTIONS |
                                  PVR2D_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = PVR2D_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = PVR2D_SUPPORTED_BLITTINGFLAGS;

     return DFB_OK;
}

static void
driver_close_device(CoreGraphicsDevice *device,
                    void               *driver_data,
                    void               *device_data)
{
     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);
}

static void
driver_close_driver(CoreGraphicsDevice *device,
                    void               *driver_data)
{
     D_DEBUG_AT(PVR2D__2D, "%s()\n", __FUNCTION__);
}

