/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

#include "gles2_2d.h"
#include "gles2_gfxdriver.h"

#include <core/graphics_driver.h>

D_DEBUG_DOMAIN(GLES2__2D, "GLES2/2D", "OpenGL ES2 2D Acceleration");

DFB_GRAPHICS_DRIVER(gles2)


/*****************************************************************************/

static int
driver_probe(CoreGraphicsDevice *device)
{
     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     switch (dfb_system_type()) {
          case CORE_MESA:
          case CORE_PVR2D:
          case CORE_EGL:
          case CORE_ANDROID:
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
     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     // fill driver info structure
     snprintf(info->name,
              DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
              "OpenGLES2 Driver");

     snprintf(info->vendor,
              DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
              "Mark J Hood / Denis Oliver Kropp");

     info->version.major = 0;
     info->version.minor = 1;

     info->driver_data_size = sizeof(GLES2DriverData);
     info->device_data_size = sizeof(GLES2DeviceData);
}

static DFBResult
driver_init_driver(CoreGraphicsDevice  *device,
                   GraphicsDeviceFuncs *funcs,
                   void                *driver_data,
                   void                *device_data,
                   CoreDFB             *core)
{
     GLES2DriverData *drv = driver_data;

     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     // initialize function pointers
     funcs->EngineSync    = gles2EngineSync;
     funcs->EngineReset   = gles2EngineReset;
     funcs->EmitCommands  = gles2EmitCommands;
     funcs->CheckState    = gles2CheckState;
     funcs->SetState      = gles2SetState;
     funcs->FillRectangle = gles2FillRectangle;
     funcs->DrawRectangle = gles2DrawRectangle;
     funcs->DrawLine      = gles2DrawLine;
     funcs->FillTriangle  = gles2FillTriangle;
     funcs->Blit          = gles2Blit;
     funcs->BatchBlit     = gles2BatchBlit;
     funcs->StretchBlit   = gles2StretchBlit;

     // Choose accelerated font format
     if (!dfb_config->software_only) {
          dfb_config->font_format  = DSPF_ARGB;
          dfb_config->font_premult = true;
     }


#ifdef GLES2_USE_FBO
     glGenFramebuffers( 1, &drv->fbo );
     glBindFramebuffer( GL_FRAMEBUFFER, drv->fbo );
#endif

     return DFB_OK;
}

static DFBResult
driver_init_device(CoreGraphicsDevice *device,
                   GraphicsDeviceInfo *device_info,
                   void               *driver_data,
                   void               *device_data)
{
     const char   *renderer;
     DFBResult     status;

     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);

     // Now that we have a connection and can query GLES.
     renderer = (const char*)glGetString(GL_RENDERER);

     // Fill device info.
     snprintf(device_info->vendor,
              DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "%s", "GLES2 Acceleration -");
     snprintf(device_info->name,
              DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "%s", renderer ?: "Unknown");

     // Initialize shader program objects, shared across all EGL contexts.
     status = gles2_init_shader_programs((GLES2DeviceData *)device_data);

     if (status != DFB_OK) {
          D_ERROR("GLES2/Driver: Could not create shader program objects!\n");
          return status;
     }

     /* device limitations */
     device_info->limits.surface_byteoffset_alignment = 8;
     device_info->limits.surface_bytepitch_alignment  = 8;

     device_info->caps.flags    = CCF_CLIPPING | CCF_RENDEROPTS;
     device_info->caps.accel    = GLES2_SUPPORTED_DRAWINGFUNCTIONS |
                                  GLES2_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = GLES2_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = GLES2_SUPPORTED_BLITTINGFLAGS;

     return DFB_OK;
}

static void
driver_close_device(CoreGraphicsDevice *device,
                    void               *driver_data,
                    void               *device_data)
{
     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);
}

static void
driver_close_driver(CoreGraphicsDevice *device,
                    void               *driver_data)
{
     D_DEBUG_AT(GLES2__2D, "%s()\n", __FUNCTION__);
}

