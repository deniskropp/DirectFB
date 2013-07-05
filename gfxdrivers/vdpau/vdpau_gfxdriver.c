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

#include <core/gfxcard.h>
#include <core/system.h>

#include <misc/conf.h>

#include "vdpau_2d.h"
#include "vdpau_gfxdriver.h"


#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( vdpau )


/**********************************************************************************************************************/

static int
driver_probe( CoreGraphicsDevice *device )
{
     switch (dfb_system_type()) {
          case CORE_X11VDPAU:
               return 1;

          default:
               break;
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
               "VDPAU Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Denis Oliver Kropp" );

     info->version.major = 0;
     info->version.minor = 0;

     info->driver_data_size = sizeof(VDPAUDriverData);
     info->device_data_size = sizeof(VDPAUDeviceData);
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     DFBX11          *x11  = dfb_system_data();
     VDPAUDriverData *vdrv = driver_data;

     vdrv->x11     = x11;
     vdrv->vdp     = &x11->vdp;
     vdrv->display = x11->display;

     vdrv->render_draw.blend_state.struct_version = VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION;
     vdrv->render_blit.blend_state.struct_version = VDP_OUTPUT_SURFACE_RENDER_BLEND_STATE_VERSION;

     /* initialize function pointers */
     funcs->EngineSync    = vdpauEngineSync;
     funcs->EngineReset   = vdpauEngineReset;
     funcs->EmitCommands  = vdpauEmitCommands;
     funcs->CheckState    = vdpauCheckState;
     funcs->SetState      = vdpauSetState;
     funcs->FillRectangle = vdpauFillRectangle;
     funcs->Blit          = vdpauBlit;
     funcs->StretchBlit   = vdpauStretchBlit;

     if (!dfb_config->software_only) {
          dfb_config->font_format  = DSPF_ARGB;
          dfb_config->font_premult = true;
     }

     return DFB_OK;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     VDPAUDriverData *vdrv = driver_data;
     VDPAUDeviceData *vdev = device_data;
     DFBX11VDPAU     *vdp  = vdrv->vdp;

     VdpStatus status;

     XLockDisplay( vdrv->display );
     status = vdp->OutputSurfaceCreate( vdp->device, VDP_RGBA_FORMAT_B8G8R8A8, 1, 1, &vdev->white );
     XUnlockDisplay( vdrv->display );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceCreate( RGBA 1x1 ) failed (status %d, '%s')!\n",
                   status, vdp->GetErrorString( status ) );
          return DFB_FAILURE;
     }

     uint32_t    white_bits  = 0xffffffff;
     const void *white_ptr   = &white_bits;
     uint32_t    white_pitch = 4;
     VdpRect     white_rect  = { 0, 0, 1, 1 };

     XLockDisplay( vdrv->display );
     status = vdp->OutputSurfacePutBitsNative( vdev->white, &white_ptr, &white_pitch, &white_rect );
     XUnlockDisplay( vdrv->display );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: OutputSurfacePutBitsNative( RGBA 1x1 ) failed (status %d, '%s')!\n",
                   status, vdp->GetErrorString( status ) );
          return DFB_FAILURE;
     }

     /* fill device info */
     snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "VDPAU" );
     snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   "Output Surface Rendering" );

     /* device limitations */
     device_info->limits.surface_byteoffset_alignment = 8;
     device_info->limits.surface_bytepitch_alignment  = 8;

     device_info->caps.flags    = 0;
     device_info->caps.accel    = VDPAU_SUPPORTED_DRAWINGFUNCTIONS |
                                  VDPAU_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = VDPAU_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = VDPAU_SUPPORTED_BLITTINGFLAGS;

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

