#ifdef GP2D_DEBUG_DRIVER
#define DIRECT_ENABLE_DEBUG
#endif

#include <stdio.h>

#undef HAVE_STDLIB_H

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/mman.h>
#include <fcntl.h>

#include <asm/types.h>

#include <directfb.h>
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/system.h>

#include <misc/conf.h>

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/screens.h>
#include <core/system.h>

#include <core/Task.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( gp2d )


#include "gp2d_driver.h"
#include "gp2d_blt.h"
#include "gp2d_engine.h"

D_DEBUG_DOMAIN( GP2D_Driver, "GP2D/Driver", "Renesas GP2D Driver" );

/**********************************************************************************************************************/

static void *
gp2d_event_loop( DirectThread *thread,
                 void         *arg )
{
     int             ret;
     GP2DDriverData *gdrv = arg;
     u32             buf[16384];

     while (true) {
          void *p;

          D_DEBUG_AT( GP2D_Driver, "%s() read...\n", __FUNCTION__ );

          ret = read( gdrv->gfx_fd, buf, sizeof(buf) );
          if (ret < 0) {
               switch (errno) {
                    case EINTR:
                         continue;

                    default:
                         D_PERROR( "GP2D: read() from drm device failed!\n" );
                         return NULL;
               }
          }

          p = buf;

          while (ret > 0) {
               struct drm_gp2d_event_executed *event = p;
               GP2DBuffer                     *buffer;

               D_ASSERT( event->base.length <= ret );

               switch (event->base.type) {
                    case DRM_GP2D_EVENT_EXECUTED:
                         D_DEBUG_AT( GP2D_Driver, "  -> EXECUTED %u (%p)\n", event->handle, event->user_data );

                         buffer = event->user_data;
                         D_MAGIC_ASSERT( buffer, GP2DBuffer );

                         if (buffer->task)
                              Task_Done( buffer->task );

                         D_ASSERT( buffer == (GP2DBuffer*) gdrv->emitted );
                         D_ASSERT( buffer->handle == event->handle );

                         gp2d_put_buffer( gdrv, buffer );
                         break;

                    default:
                         break;
               }

               ret -= event->base.length;
               p   += event->base.length;
          }
     }

     return NULL;
}

/**********************************************************************************************************************/

static int
driver_probe( CoreGraphicsDevice *device )
{
     D_DEBUG_AT( GP2D_Driver, "%s()\n", __FUNCTION__ );

     return 1;//dfb_gfxcard_get_accelerator( device ) == 0x2D47;
}

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *info )
{
     D_DEBUG_AT( GP2D_Driver, "%s()\n", __FUNCTION__ );

     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "Renesas GP2D Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Denis Oliver Kropp" );

     info->version.major = 0;
     info->version.minor = 9;

     info->driver_data_size = sizeof(GP2DDriverData);
     info->device_data_size = sizeof(GP2DDeviceData);
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     GP2DDriverData *gdrv = driver_data;

     D_DEBUG_AT( GP2D_Driver, "%s()\n", __FUNCTION__ );

     /* Keep pointer to shared device data. */
     gdrv->dev = device_data;

     /* Keep core and device pointer. */
     gdrv->core   = core;
     gdrv->device = device;

     D_DEBUG_AT( GP2D_Driver, "  -> drmOpen...\n" );
     /* Open the drawing engine device. */
     gdrv->gfx_fd = drmOpen( "gp2d", NULL );
     if (gdrv->gfx_fd < 0) {
          D_PERROR( "GP2D: drmOpen() failed!\n" );
          return DFB_INIT;
     }
     D_DEBUG_AT( GP2D_Driver, "  -> drmOpen done.\n" );

     /* Initialize function table. */
     funcs->EngineReset       = gp2dEngineReset;
     funcs->EngineSync        = gp2dEngineSync;
     funcs->EmitCommands      = gp2dEmitCommands;
     funcs->CheckState        = gp2dCheckState;
     funcs->SetState          = gp2dSetState;
     funcs->FillRectangle     = gp2dFillRectangle;
     funcs->FillTriangle      = gp2dFillTriangle;
     funcs->DrawRectangle     = gp2dDrawRectangle;
     funcs->DrawLine          = gp2dDrawLine;
     funcs->Blit              = gp2dBlit;
     funcs->StretchBlit       = gp2dStretchBlit;

     return DFB_OK;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     GP2DDriverData *gdrv = driver_data;
     GP2DDeviceData *gdev = device_data;

     D_DEBUG_AT( GP2D_Driver, "%s()\n", __FUNCTION__ );

     /* Fill in the device info. */
     snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   "GP2D" );
     snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Renesas" );

     /* Set device limitations. */
     device_info->limits.surface_byteoffset_alignment = 512;
     device_info->limits.surface_bytepitch_alignment  = 64;

     /* Set device capabilities. */
     device_info->caps.flags    = CCF_CLIPPING | CCF_RENDEROPTS;
     device_info->caps.accel    = GP2D_SUPPORTED_DRAWINGFUNCTIONS | \
                                  GP2D_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = GP2D_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = GP2D_SUPPORTED_BLITTINGFLAGS;

     gp2d_blt_gen_free( gdrv, 5 );

     register_gp2d( gdrv );

     gdrv->event_thread = direct_thread_create( DTT_CRITICAL, gp2d_event_loop, gdrv, "GP2D DRM Events" );

     /* Reset the drawing engine. */
     gp2dEngineReset( gdrv, gdev );

     return DFB_OK;
}

static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data )
{
     D_DEBUG_AT( GP2D_Driver, "%s()\n", __FUNCTION__ );
}

static void
driver_close_driver( CoreGraphicsDevice *device,
                     void               *driver_data )
{
     GP2DDriverData *gdrv = driver_data;

     D_DEBUG_AT( GP2D_Driver, "%s()\n", __FUNCTION__ );

     /* Close Drawing Engine device. */
     close( gdrv->gfx_fd );
}

