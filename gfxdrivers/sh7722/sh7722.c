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
     SH7722DeviceData *sdev = device_data;

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

     /* Get virtual addresses for LCD buffer and JPEG reload buffers in slaves here,
        master does it in driver_init_device(). */
     if (!dfb_core_is_master( core )) {
          sdrv->lcd_virt  = dfb_gfxcard_memory_virtual( device, sdev->lcd_offset );
          sdrv->jpeg_virt = dfb_gfxcard_memory_virtual( device, sdev->jpeg_offset );
     }

     /* Initialize function table. */
     funcs->EngineReset       = sh7722EngineReset;
     funcs->EngineSync        = sh7722EngineSync;
     funcs->EmitCommands      = sh7722EmitCommands;
     funcs->CheckState        = sh7722CheckState;
     funcs->SetState          = sh7722SetState;
     funcs->FillTriangle      = sh7722FillTriangle;
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


     /*
      * Setup LCD buffer for YCbCr 4:2:2 (NV16).
      */
     sdev->lcd_width  = SH7722_LCD_WIDTH;
     sdev->lcd_height = SH7722_LCD_HEIGHT;
     sdev->lcd_pitch  = (sdev->lcd_width + 0xf) & ~0xf;
     sdev->lcd_size   = sdev->lcd_height * sdev->lcd_pitch * 2;
     sdev->lcd_offset = dfb_gfxcard_reserve_memory( device, sdev->lcd_size );

     if (sdev->lcd_offset < 0) {
          D_ERROR( "SH7722/Driver: Allocating %d bytes for the LCD buffer failed!\n", sdev->lcd_size );
          return DFB_FAILURE;
     }

     sdev->lcd_phys = dfb_gfxcard_memory_physical( device, sdev->lcd_offset );

     /* Get virtual addresses for JPEG reload buffers in master here,
        slaves do it in driver_init_driver(). */
     sdrv->lcd_virt = dfb_gfxcard_memory_virtual( device, sdev->lcd_offset );

     D_INFO( "SH7722/LCD: Allocated %dx%d YCbCr 4:2:2 Buffer (%d bytes) at 0x%08lx (%p)\n",
             sdev->lcd_width, sdev->lcd_height, sdev->lcd_size, sdev->lcd_phys, sdrv->lcd_virt );

     D_ASSERT( ! (sdev->lcd_pitch & 0xf) );
     D_ASSERT( ! (sdev->lcd_phys & 0xf) );


     /*
      * Setup JPEG reload buffers.
      */
     sdev->jpeg_size   = SH7722GFX_JPEG_RELOAD_SIZE * 2;
     sdev->jpeg_offset = dfb_gfxcard_reserve_memory( device, sdev->jpeg_size );

     if (sdev->jpeg_offset < 0) {
          D_ERROR( "SH7722/Driver: Allocating %d bytes for the JPEG buffer failed!\n", sdev->jpeg_size );
          return DFB_FAILURE;
     }

     sdev->jpeg_phys = dfb_gfxcard_memory_physical( device, sdev->jpeg_offset );

     /* Get virtual addresses for JPEG reload buffers in master here,
        slaves do it in driver_init_driver(). */
     sdrv->jpeg_virt = dfb_gfxcard_memory_virtual( device, sdev->jpeg_offset );

     D_INFO( "SH7722/JPEG: Allocated reload buffers (%d bytes) at 0x%08lx (%p)\n",
             sdev->jpeg_size, sdev->jpeg_phys, sdrv->jpeg_virt );

     D_ASSERT( ! (sdev->jpeg_size & 0xff) );
     D_ASSERT( ! (sdev->jpeg_phys & 0xf) );


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

     /* Change font format for acceleration. */
     if (!dfb_config->software_only) {
          dfb_config->font_format  = DSPF_ARGB;
          dfb_config->font_premult = false;
     }


     /*
      * Initialize hardware.
      */

     /* Reset the drawing engine. */
     sh7722EngineReset( sdrv, sdev );

     /* Wait for idle BEU. */
     while (SH7722_GETREG32( sdrv, BSTAR ) & 1);

     /* Disable all inputs. */
     SH7722_SETREG32( sdrv, BESTR, 0 );

     /* Disable all multi windows. */
     SH7722_SETREG32( sdrv, BMWCR0, SH7722_GETREG32( sdrv, BMWCR0 ) & ~0xf );

     /* Set output pixel format of the BEU to NV16. */
     SH7722_SETREG32( sdrv, BPKFR,  CHDS_YCBCR422 );
     SH7722_SETREG32( sdrv, BPROCR, 0x00000000 );

     /* Have BEU render into LCD buffer. */
     SH7722_SETREG32( sdrv, BBLCR1, MT_MEMORY );
     SH7722_SETREG32( sdrv, BDAYR, sdev->lcd_phys );
     SH7722_SETREG32( sdrv, BDACR, sdev->lcd_phys + sdev->lcd_height * sdev->lcd_pitch );
     SH7722_SETREG32( sdrv, BDMWR, sdev->lcd_pitch );

     /* Clear LCD buffer. */
     memset( (void*) sdrv->lcd_virt, 0x10, sdev->lcd_height * sdev->lcd_pitch );
     memset( (void*) sdrv->lcd_virt + sdev->lcd_height * sdev->lcd_pitch, 0x80, sdev->lcd_height * sdev->lcd_pitch );

     /* Setup LCD controller to show the buffer. */
     sh7722_lcd_setup( sdrv, sdev->lcd_width, sdev->lcd_height, sdev->lcd_phys, sdev->lcd_pitch, DSPF_NV16, false );

     /* Initialize BEU lock. */
     fusion_skirmish_init( &sdev->beu_lock, "BEU", dfb_core_world(sdrv->core) );

     /* Initialize JPEG lock. */
     fusion_skirmish_init( &sdev->jpeg_lock, "JPEG", dfb_core_world(sdrv->core) );

     return DFB_OK;
}

static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data )
{
     SH7722DeviceData *sdev = device_data;

     D_DEBUG_AT( SH7722_Driver, "%s()\n", __FUNCTION__ );

     /* Destroy JPEG lock. */
     fusion_skirmish_destroy( &sdev->jpeg_lock );

     /* Destroy BEU lock. */
     fusion_skirmish_destroy( &sdev->beu_lock );
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

