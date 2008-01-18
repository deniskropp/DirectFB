/*
   TI Davinci driver - Graphics Driver

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org>

   Code is derived from VMWare driver.

   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <asm/types.h>

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/surface_pool.h>
#include <core/system.h>

#include <misc/conf.h>

#include "davincifb.h"

#include "davinci_2d.h"
#include "davinci_gfxdriver.h"
#include "davinci_osd.h"
#include "davinci_osd_pool.h"
#include "davinci_screen.h"
#include "davinci_video.h"
#include "davinci_video_pool.h"


#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( davinci )

/**********************************************************************************************************************/

static DFBResult
open_fb( DavinciDriverData *ddrv,
         DavinciDeviceData *ddev,
         unsigned int       fbnum )
{
     int                       ret;
     char                      buf1[16];
     char                      buf2[16];
     DavinciFB                *fb;
     struct fb_var_screeninfo  var;

     D_ASSERT( ddrv != NULL );
     D_ASSERT( ddev != NULL );
     D_ASSERT( fbnum < D_ARRAY_SIZE(ddrv->fb) );
     D_ASSERT( fbnum < D_ARRAY_SIZE(ddev->fix) );

     fb = &ddrv->fb[fbnum];

     fb->num = fbnum;

     snprintf( buf1, sizeof(buf1), "/dev/fb%u", fbnum );
     snprintf( buf2, sizeof(buf2), "/dev/fb/%u", fbnum );

     fb->fd = direct_try_open( buf1, buf2, O_RDWR, true );
     if (fb->fd < 0)
          return DFB_INIT;

     ret = ioctl( fb->fd, FBIOGET_VSCREENINFO, &var );
     if (ret) {
          D_PERROR( "Davinci/Driver: FBIOGET_VSCREENINFO (fb%d) failed!\n", fbnum );
          close( fb->fd );
          return DFB_INIT;
     }

     ret = ioctl( fb->fd, FBIOGET_FSCREENINFO, &ddev->fix[fbnum] );
     if (ret) {
          D_PERROR( "Davinci/Driver: FBIOGET_FSCREENINFO (fb%d) failed!\n", fbnum );
          close( fb->fd );
          return DFB_INIT;
     }

     fb->size = ddev->fix[fbnum].smem_len;

     fb->mem = mmap( NULL, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0 );
     if (fb->mem == MAP_FAILED) {
          D_PERROR( "Davinci/Driver: mmap (fb%d, length %d) failed!\n", fbnum, fb->size );
          close( fb->fd );
          return DFB_INIT;
     }

     D_INFO( "Davinci/Driver: Mapped fb%d with length %u at %p to %p\n",
             fbnum, fb->size, (void*)ddev->fix[fbnum].smem_start, fb->mem );

     return DFB_OK;
}

static void
close_fb( DavinciFB *fb )
{
     munmap( fb->mem, fb->size );
     close( fb->fd );
}

/**********************************************************************************************************************/

static int
driver_probe( CoreGraphicsDevice *device )
{
     int                 ret;
     int                 fd;
     vpbe_fb_videomode_t videomode;

     switch (dfb_system_type()) {
          case CORE_DEVMEM:
          case CORE_TI_CMEM:
               break;

          default:
               return 0;
     }

     fd = direct_try_open( "/dev/fb0", "/dev/fb/0", O_RDWR, true );
     if (fd < 0)
          return 0;

     ret = ioctl( fd, FBIO_GET_TIMING, &videomode);

     close( fd );

     if (ret) {
          D_PERROR( "Davinci/Driver: FBIO_GET_TIMING failed!\n" );
          return 0;
     }

     if (videomode.xres > 768 || videomode.yres > 576 || videomode.fps > 60) {
          D_ERROR( "Davinci/Driver: Invalid mode %dx%d @%d!\n", videomode.xres, videomode.yres, videomode.fps );
          return 0;
     }

     if (strncmp( (char*)videomode.name, "PAL", 3 ) &&
         strncmp( (char*)videomode.name, "NTSC", 4 ))
     {
          D_ERROR( "Davinci/Driver: Unknown mode name '%s'!\n", videomode.name );
          return 0;
     }

     return 1;
}

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "TI Davinci Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Telio AG" );

     info->version.major = 0;
     info->version.minor = 4;

     info->driver_data_size = sizeof(DavinciDriverData);
     info->device_data_size = sizeof(DavinciDeviceData);
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     DFBResult          ret;
     DavinciDriverData *ddrv   = driver_data;
     DavinciDeviceData *ddev   = device_data;
     bool               master = dfb_core_is_master( core );

     ddrv->ddev = ddev;
     ddrv->core = core;

     ret = open_fb( ddrv, ddev, OSD0 );
     if (ret)
          return ret;

     ret = open_fb( ddrv, ddev, VID0 );
     if (ret)
          goto error_fb1;

     ret = open_fb( ddrv, ddev, OSD1 );
     if (ret)
          goto error_fb2;

     ret = open_fb( ddrv, ddev, VID1 );
     if (ret)
          goto error_fb3;

     ret = davinci_c64x_open( &ddrv->c64x );
     if (ret)
          D_WARN( "running without DSP acceleration" );
     else {
          ddrv->c64x_present = true;

          /* initialize function pointers */
          funcs->EngineSync        = davinciEngineSync;
          funcs->EngineReset       = davinciEngineReset;
          funcs->EmitCommands      = davinciEmitCommands;
          funcs->FlushTextureCache = davinciFlushTextureCache;
          funcs->CheckState        = davinciCheckState;
          funcs->SetState          = davinciSetState;
     }

     ddrv->screen = dfb_screens_register( device, driver_data, &davinciScreenFuncs );

     ddrv->osd   = dfb_layers_register( ddrv->screen, driver_data, &davinciOSDLayerFuncs );
     ddrv->video = dfb_layers_register( ddrv->screen, driver_data, &davinciVideoLayerFuncs );

     if (!master) {
          dfb_surface_pool_join( core, ddev->osd_pool, &davinciOSDSurfacePoolFuncs );
          dfb_surface_pool_join( core, ddev->video_pool, &davinciVideoSurfacePoolFuncs );
     }

     if (!dfb_config->software_only) {
          dfb_config->font_format  = DSPF_ARGB;
          dfb_config->font_premult = true;
     }

     return DFB_OK;

error_fb3:
     close_fb( &ddrv->fb[OSD1] );

error_fb2:
     close_fb( &ddrv->fb[VID0] );

error_fb1:
     close_fb( &ddrv->fb[OSD0] );

     return DFB_INIT;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     DavinciDriverData *ddrv = driver_data;
     DavinciDeviceData *ddev = device_data;

     /* fill device info */
     snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Texas Instruments" );
     snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   "Davinci" );

     /* device limitations */
     device_info->limits.surface_byteoffset_alignment = 64;
     device_info->limits.surface_bytepitch_alignment  = 32;

     if (ddrv->c64x_present) {
          device_info->caps.flags    = 0;
          device_info->caps.accel    = DAVINCI_SUPPORTED_DRAWINGFUNCTIONS |
                                       DAVINCI_SUPPORTED_BLITTINGFUNCTIONS;
          device_info->caps.drawing  = DAVINCI_SUPPORTED_DRAWINGFLAGS;
          device_info->caps.blitting = DAVINCI_SUPPORTED_BLITTINGFLAGS;
          device_info->caps.clip     = DFXL_STRETCHBLIT;
     }

     dfb_surface_pool_initialize( ddrv->core, &davinciOSDSurfacePoolFuncs, &ddev->osd_pool );
     dfb_surface_pool_initialize( ddrv->core, &davinciVideoSurfacePoolFuncs, &ddev->video_pool );

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
     DavinciDriverData *ddrv = driver_data;

     if (ddrv->c64x_present)
          davinci_c64x_close( &ddrv->c64x );

     close_fb( &ddrv->fb[VID1] );
     close_fb( &ddrv->fb[OSD1] );
     close_fb( &ddrv->fb[VID0] );
     close_fb( &ddrv->fb[OSD0] );
}

