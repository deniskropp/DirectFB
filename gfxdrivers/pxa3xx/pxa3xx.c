#ifdef PXA3XX_DEBUG_DRIVER
#define DIRECT_ENABLE_DEBUG
#endif

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/mman.h>
#include <fcntl.h>

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

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( pxa3xx )


#include "pxa3xx.h"
#include "pxa3xx_blt.h"


D_DEBUG_DOMAIN( PXA3XX_Driver, "PXA3XX/Driver", "Marvell PXA3XX Driver" );

/**********************************************************************************************************************/

static int
driver_probe( CoreGraphicsDevice *device )
{
     D_DEBUG_AT( PXA3XX_Driver, "%s()\n", __FUNCTION__ );

     return dfb_gfxcard_get_accelerator( device ) == 1234;  // FIXME
}

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *info )
{
     D_DEBUG_AT( PXA3XX_Driver, "%s()\n", __FUNCTION__ );

     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "Marvell PXA3XX Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Denis & Janine Kropp" );

     info->version.major = 0;
     info->version.minor = 3;

     info->driver_data_size = sizeof(PXA3XXDriverData);
     info->device_data_size = sizeof(PXA3XXDeviceData);
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     PXA3XXDriverData *pdrv = driver_data;

     D_DEBUG_AT( PXA3XX_Driver, "%s()\n", __FUNCTION__ );

     /* Keep pointer to shared device data. */
     pdrv->dev = device_data;

     /* Keep core and device pointer. */
     pdrv->core   = core;
     pdrv->device = device;

     /* Open the drawing engine device. */
     pdrv->gfx_fd = direct_try_open( "/dev/pxa3xx_gfx", "/dev/misc/pxa3xx_gfx", O_RDWR, true );
     if (pdrv->gfx_fd < 0)
          return DFB_INIT;

     /* Map its shared data. */
     pdrv->gfx_shared = mmap( NULL, direct_page_align( sizeof(PXA3XXGfxSharedArea) ),
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED, pdrv->gfx_fd, 0 );
     if (pdrv->gfx_shared == MAP_FAILED) {
          D_PERROR( "PXA3XX/Driver: Could not map shared area!\n" );
          close( pdrv->gfx_fd );
          return DFB_INIT;
     }

     pdrv->mmio_base = mmap( NULL, 4096,     // FIXME
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, pdrv->gfx_fd, direct_page_align( sizeof(PXA3XXGfxSharedArea) ) );
     if (!pdrv->mmio_base) {
          D_PERROR( "PXA3XX/Driver: Could not map MMIO area!\n" );
          munmap( (void*) pdrv->gfx_shared, direct_page_align( sizeof(PXA3XXGfxSharedArea) ) );
          close( pdrv->gfx_fd );
          return DFB_INIT;
     }

     /* Check the magic value. */
     if (pdrv->gfx_shared->magic != PXA3XX_GCU_SHARED_MAGIC) {
          D_ERROR( "PXA3XX/Driver: Magic value 0x%08x doesn't match 0x%08x!\n",
                   pdrv->gfx_shared->magic, PXA3XX_GCU_SHARED_MAGIC );
          munmap( (void*) pdrv->mmio_base, 4096 );     // FIXME
          munmap( (void*) pdrv->gfx_shared, direct_page_align( sizeof(PXA3XXGfxSharedArea) ) );
          close( pdrv->gfx_fd );
          return DFB_INIT;
     }

     /* Initialize function table. */
     funcs->EngineReset   = pxa3xxEngineReset;
     funcs->EngineSync    = pxa3xxEngineSync;
     funcs->EmitCommands  = pxa3xxEmitCommands;
     funcs->CheckState    = pxa3xxCheckState;
     funcs->SetState      = pxa3xxSetState;
     funcs->FillRectangle = pxa3xxFillRectangle;
     funcs->Blit          = pxa3xxBlit;

     return DFB_OK;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     PXA3XXDriverData *pdrv = driver_data;
     PXA3XXDeviceData *pdev = device_data;

     D_DEBUG_AT( PXA3XX_Driver, "%s()\n", __FUNCTION__ );

     /*
      * Initialize hardware.
      */

     /* Reset the drawing engine. */
     pxa3xxEngineReset( pdrv, pdev );

     /* Fill in the device info. */
     snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   "PXA3XX" );
     snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Marvell" );

     /* Set device limitations. */
     device_info->limits.surface_byteoffset_alignment = 8;
     device_info->limits.surface_bytepitch_alignment  = 8;

     /* Set device capabilities. */
     device_info->caps.flags    = 0;
     device_info->caps.accel    = PXA3XX_SUPPORTED_DRAWINGFUNCTIONS |
                                  PXA3XX_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = PXA3XX_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = PXA3XX_SUPPORTED_BLITTINGFLAGS;

     /* Change font format for acceleration. */
     if (!dfb_config->software_only) {
          dfb_config->font_format  = DSPF_ARGB;
          dfb_config->font_premult = false;
     }

     return DFB_OK;
}

static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data )
{
     D_DEBUG_AT( PXA3XX_Driver, "%s()\n", __FUNCTION__ );
}

static void
driver_close_driver( CoreGraphicsDevice *device,
                     void               *driver_data )
{
     PXA3XXDriverData    *pdrv   = driver_data;
     PXA3XXGfxSharedArea *shared = pdrv->gfx_shared;

     (void) shared;

     D_DEBUG_AT( PXA3XX_Driver, "%s()\n", __FUNCTION__ );

     D_INFO( "PXA3XX/BLT: %u starts, %u done, %u interrupts, %u wait_idle, %u wait_next, %u idle\n",
             shared->num_starts, shared->num_done, shared->num_interrupts,
             shared->num_wait_idle, shared->num_wait_next, shared->num_idle );

     D_INFO( "PXA3XX/BLT: %u words, %u words/start, %u words/idle, %u starts/idle\n",
             shared->num_words,
             shared->num_words  / (shared->num_starts ?: 1),
             shared->num_words  / (shared->num_idle ?: 1),
             shared->num_starts / (shared->num_idle ?: 1) );

     /* Unmap registers. */
     munmap( (void*) pdrv->mmio_base, 4096 );    // FIXME

     /* Unmap shared area. */
     munmap( (void*) pdrv->gfx_shared, direct_page_align( sizeof(PXA3XXGfxSharedArea) ) );

     /* Close Drawing Engine device. */
     close( pdrv->gfx_fd );
}

