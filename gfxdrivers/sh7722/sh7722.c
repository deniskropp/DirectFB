#ifdef SH7722_DEBUG
#define DIRECT_FORCE_DEBUG
#endif


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

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/system.h>

#include <misc/conf.h>

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/screens.h>
#include <core/system.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( sh7722 )


#include "sh7722.h"
#include "sh7722_blt.h"
#include "sh7722_layer.h"
#include "sh7722_lcd.h"
#include "sh7722_multi.h"
#include "sh7722_screen.h"


D_DEBUG_DOMAIN( SH7722_Driver, "SH7722/Driver", "Renesas SH7722 Driver" );

/**********************************************************************************************************************/

static int
driver_probe( CoreGraphicsDevice *device )
{
     D_DEBUG_AT( SH7722_Driver, "%s()\n", __FUNCTION__ );

     return dfb_gfxcard_get_accelerator( device ) == 0x2D47;
}

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *info )
{
     D_DEBUG_AT( SH7722_Driver, "%s()\n", __FUNCTION__ );

     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "Renesas SH7722 Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Denis Oliver Kropp" );

     info->version.major = 0;
     info->version.minor = 7;

     info->driver_data_size = sizeof(SH7722DriverData);
     info->device_data_size = sizeof(SH7722DeviceData);
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     SH7722DriverData *sdrv = driver_data;

     D_DEBUG_AT( SH7722_Driver, "%s()\n", __FUNCTION__ );

     /* Keep pointer to shared device data. */
     sdrv->dev = device_data;

     /* Keep core and device pointer. */
     sdrv->core   = core;
     sdrv->device = device;

     /* Open the drawing engine device. */
     sdrv->gfx_fd = direct_try_open( "/dev/sh7722gfx", "/dev/misc/sh7722gfx", O_RDWR, true );
     if (sdrv->gfx_fd < 0)
          return DFB_INIT;

     /* Map its shared data. */
     sdrv->gfx_shared = mmap( NULL, direct_page_align( sizeof(SH7722GfxSharedArea) ),
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED, sdrv->gfx_fd, 0 );
     if (sdrv->gfx_shared == MAP_FAILED) {
          D_PERROR( "SH7722/Driver: Could not map shared area!\n" );
          close( sdrv->gfx_fd );
          return DFB_INIT;
     }

     sdrv->mmio_base = dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!sdrv->mmio_base) {
          D_PERROR( "SH7722/Driver: Could not map MMIO area!\n" );
          munmap( (void*) sdrv->gfx_shared, direct_page_align( sizeof(SH7722GfxSharedArea) ) );
          close( sdrv->gfx_fd );
          return DFB_INIT;
     }

     /* Check the magic value. */
     if (sdrv->gfx_shared->magic != SH7722GFX_SHARED_MAGIC) {
          D_ERROR( "SH7722/Driver: Magic value 0x%08x doesn't match 0x%08x!\n",
                   sdrv->gfx_shared->magic, SH7722GFX_SHARED_MAGIC );
          dfb_gfxcard_unmap_mmio( device, sdrv->mmio_base, -1 );
          munmap( (void*) sdrv->gfx_shared, direct_page_align( sizeof(SH7722GfxSharedArea) ) );
          close( sdrv->gfx_fd );
          return DFB_INIT;
     }

     /* Initialize function table. */
     funcs->EngineReset       = sh7722EngineReset;
     funcs->EngineSync        = sh7722EngineSync;
     funcs->EmitCommands      = sh7722EmitCommands;
     funcs->CheckState        = sh7722CheckState;
     funcs->SetState          = sh7722SetState;
     funcs->FillRectangle     = sh7722FillRectangle;
     funcs->FillTriangle      = sh7722FillTriangle;
     funcs->DrawRectangle     = sh7722DrawRectangle;
     funcs->DrawLine          = sh7722DrawLine;
     funcs->Blit              = sh7722Blit;
     funcs->StretchBlit       = sh7722StretchBlit;
     funcs->FlushTextureCache = sh7722FlushTextureCache;

     /* Register primary screen. */
     sdrv->screen = dfb_screens_register( device, driver_data, &sh7722ScreenFuncs );

     /* Register three input system layers. */
     sdrv->input1 = dfb_layers_register( sdrv->screen, driver_data, &sh7722LayerFuncs );
     sdrv->input2 = dfb_layers_register( sdrv->screen, driver_data, &sh7722LayerFuncs );
     sdrv->input3 = dfb_layers_register( sdrv->screen, driver_data, &sh7722LayerFuncs );

     /* Register multi window layer. */
     sdrv->multi = dfb_layers_register( sdrv->screen, driver_data, &sh7722MultiLayerFuncs );

     return DFB_OK;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     SH7722DriverData *sdrv = driver_data;
     SH7722DeviceData *sdev = device_data;

     D_DEBUG_AT( SH7722_Driver, "%s()\n", __FUNCTION__ );

     /* Setup LCD buffer. */
     sdev->lcd_width  = SH7722_LCD_WIDTH;
     sdev->lcd_height = SH7722_LCD_HEIGHT;
     sdev->lcd_pitch  = sdev->lcd_width * 2;
     sdev->lcd_offset = dfb_gfxcard_reserve_memory( device, sdev->lcd_pitch * sdev->lcd_height );

     if (sdev->lcd_offset < 0) {
          D_ERROR( "SH7722/Driver: Allocating %d bytes for the LCD buffer failed!\n",
                   sdev->lcd_pitch * sdev->lcd_height );
          return DFB_FAILURE;
     }

     sh7722_lcd_setup( sdrv, sdev->lcd_width, sdev->lcd_height,
                       dfb_gfxcard_memory_physical( device, sdev->lcd_offset ), sdev->lcd_pitch, 16 );

     /* Fill in the device info. */
     snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   "SH7722" );
     snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Renesas" );

     /* Set device limitations. */
     device_info->limits.surface_byteoffset_alignment = 16;
     device_info->limits.surface_bytepitch_alignment  = 8;

     /* Set device capabilities. */
     device_info->caps.flags    = CCF_CLIPPING;
     device_info->caps.accel    = SH7722_SUPPORTED_DRAWINGFUNCTIONS |
                                  SH7722_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = SH7722_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = SH7722_SUPPORTED_BLITTINGFLAGS;

     /* Reset the drawing engine. */
     sh7722EngineReset( sdrv, sdev );

     /* Wait for idle BEU. */
     while (SH7722_GETREG32( sdrv, BSTAR ) & 1);

     /* Disable all inputs. */
     SH7722_SETREG32( sdrv, BESTR, 0 );

     /* Disable all multi windows. */
     SH7722_SETREG32( sdrv, BMWCR0, SH7722_GETREG32( sdrv, BMWCR0 ) & ~0xf );

     /* Set output pixel format of the BEU. */
     SH7722_SETREG32( sdrv, BPKFR,  0x800 | WPCK_RGB16 );
     SH7722_SETREG32( sdrv, BPROCR, 0x00000000 );

     /* Have BEU render into LCD buffer. */
     SH7722_SETREG32( sdrv, BBLCR1, MT_MEMORY );
     SH7722_SETREG32( sdrv, BDAYR, (dfb_config->video_phys + sdev->lcd_offset) & 0xfffffffc );
     SH7722_SETREG32( sdrv, BDMWR, sdev->lcd_pitch  & 0x0003fffc );

     /* Clear LCD buffer. */
     memset( dfb_gfxcard_memory_virtual( device, sdev->lcd_offset ), 0, sdev->lcd_height * sdev->lcd_pitch );

     return DFB_OK;
}

static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data )
{
     D_DEBUG_AT( SH7722_Driver, "%s()\n", __FUNCTION__ );
}

static void
driver_close_driver( CoreGraphicsDevice *device,
                     void               *driver_data )
{
     SH7722DriverData    *sdrv   = driver_data;
     SH7722GfxSharedArea *shared = sdrv->gfx_shared;

     D_DEBUG_AT( SH7722_Driver, "%s()\n", __FUNCTION__ );

     D_INFO( "SH7722/BLT: %u starts, %u done, %u interrupts, %u wait_idle, %u wait_next, %u idle\n",
             shared->num_starts, shared->num_done, shared->num_interrupts,
             shared->num_wait_idle, shared->num_wait_next, shared->num_idle );

     D_INFO( "SH7722/BLT: %u words, %u words/start, %u words/idle, %u starts/idle\n",
             shared->num_words,
             shared->num_words  / shared->num_starts,
             shared->num_words  / shared->num_idle,
             shared->num_starts / shared->num_idle );

     /* Unmap shared area. */
     munmap( (void*) sdrv->gfx_shared, direct_page_align( sizeof(SH7722GfxSharedArea) ) );

     /* Close Drawing Engine device. */
     close( sdrv->gfx_fd );
}

