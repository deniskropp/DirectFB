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

#include <stdio.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/gfxcard.h>
#include <core/system.h>

#include <misc/conf.h>

#include <GL/glx.h>

#include <x11/x11.h>

#include "gl_2d.h"
#include "gl_gfxdriver.h"


#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( gl )


/**********************************************************************************************************************/

static int
driver_probe( CoreGraphicsDevice *device )
{
     switch (dfb_system_type()) {
          case CORE_X11: {
               int     ee;
               DFBX11 *x11 = dfb_system_data();

               return glXQueryExtension( x11->display, &ee, &ee );
          }

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
               "OpenGL Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Denis Oliver Kropp" );

     info->version.major = 0;
     info->version.minor = 5;

     info->driver_data_size = sizeof(GLDriverData);
     info->device_data_size = sizeof(GLDeviceData);
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     /* initialize function pointers */
     funcs->EngineSync    = glEngineSync;
     funcs->EngineReset   = glEngineReset;
     funcs->EmitCommands  = glEmitCommands;
     funcs->CheckState    = glCheckState;
     funcs->SetState      = glSetState;
     funcs->FillRectangle = glFillRectangle;
     funcs->DrawRectangle = glDrawRectangle;
     funcs->DrawLine      = glDrawLine;
     funcs->FillTriangle  = glFillTriangle;
     funcs->Blit          = glBlit;
     funcs->StretchBlit   = glStretchBlit;

     /* Choose accelerated font format */
     dfb_config->font_format  = DSPF_ARGB;
     dfb_config->font_premult = true;

     return DFB_OK;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     const char   *renderer;
     Display      *display;
     XVisualInfo  *visual;
     GLXContext    context;
     DFBX11       *x11;

     int attr[] = {
          GLX_RGBA,
          GLX_RED_SIZE, 1,
          GLX_GREEN_SIZE, 1,
          GLX_BLUE_SIZE, 1,
          None
     };

     x11 = dfb_system_data();

     display = x11->display;

     visual = glXChooseVisual( display, DefaultScreen(display), attr );
     if (!visual) {
          D_ERROR( "GL/Driver: Could not find a suitable visual!\n" );
          return DFB_INIT;
     }

     context = glXCreateContext( display, visual, NULL, GL_TRUE );
     if (!context) {
          D_ERROR( "GL/Driver: Could not create a context!\n" );
          return DFB_INIT;
     }

     glXMakeCurrent( display, RootWindowOfScreen(DefaultScreenOfDisplay(display)), context );

     renderer = (const char*) glGetString( GL_RENDERER );

     glXMakeCurrent( display, None, NULL );
     glXDestroyContext( display, context );


     /* fill device info */
     snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "OpenGL Acceleration -" );
     snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   renderer ?: "Unknown" );

     /* device limitations */
     device_info->limits.surface_byteoffset_alignment = 8;
     device_info->limits.surface_bytepitch_alignment  = 8;

     device_info->caps.flags    = CCF_CLIPPING | CCF_RENDEROPTS;
     device_info->caps.accel    = GL_SUPPORTED_DRAWINGFUNCTIONS |
                                  GL_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = GL_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = GL_SUPPORTED_BLITTINGFLAGS;

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

