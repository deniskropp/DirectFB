#ifdef SH7722_DEBUG_DRIVER
#define DIRECT_ENABLE_DEBUG
#endif

#include <stdio.h>
#include <jpeglib.h>

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

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( sh7722 )


#include "sh7722.h"
#include "sh7722_blt.h"
#include "sh7722_jpeglib.h"
#include "sh7722_layer.h"
#include "sh7722_lcd.h"
#include "sh7722_multi.h"
#include "sh7722_screen.h"

#include "sh7723_blt.h"

#ifdef SH772X_FBDEV_SUPPORT
#include <linux/fb.h>
#include <sys/mman.h>
#endif


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
               "Renesas SH772x Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Denis & Janine Kropp" );

     info->version.major = 0;
     info->version.minor = 9;

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
     DFBResult         ret;
     SH7722DriverData *sdrv = driver_data;
     SH7722DeviceData *sdev = device_data;

     D_DEBUG_AT( SH7722_Driver, "%s()\n", __FUNCTION__ );

     /* Keep pointer to shared device data. */
     sdrv->dev = device_data;

     /* Keep core and device pointer. */
     sdrv->core   = core;
     sdrv->device = device;

     /* Open the drawing engine device. */
     sdrv->gfx_fd = direct_try_open( "/dev/sh772x_gfx", "/dev/misc/sh772x_gfx", O_RDWR, true );
     if (sdrv->gfx_fd < 0)
          return DFB_INIT;

     /* Map its shared data. */
     sdrv->gfx_shared = mmap( NULL, direct_page_align( sizeof(SH772xGfxSharedArea) ),
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
          munmap( (void*) sdrv->gfx_shared, direct_page_align( sizeof(SH772xGfxSharedArea) ) );
          close( sdrv->gfx_fd );
          return DFB_INIT;
     }

     /* Check the magic value. */
     switch (sdrv->gfx_shared->magic) {
          case SH7722GFX_SHARED_MAGIC:
               sdev->sh772x = 7722;

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

               /* Initialize JPEG library. */
               ret = SH7722_JPEG_Initialize();
               if (ret) {
                    D_DERROR( ret, "SH7722/Driver: JPEG initialization failed!\n" );
                    dfb_gfxcard_unmap_mmio( device, sdrv->mmio_base, -1 );
                    munmap( (void*) sdrv->gfx_shared, direct_page_align( sizeof(SH772xGfxSharedArea) ) );
                    close( sdrv->gfx_fd );
                    return DFB_INIT;
               }
               break;

          case SH7723GFX_SHARED_MAGIC:
               sdev->sh772x = 7723;

               /* Initialize function table. */
               funcs->EngineReset       = sh7723EngineReset;
               funcs->EngineSync        = sh7723EngineSync;
               funcs->EmitCommands      = sh7723EmitCommands;
               funcs->CheckState        = sh7723CheckState;
               funcs->SetState          = sh7723SetState;
               funcs->FillRectangle     = sh7723FillRectangle;
               funcs->FillTriangle      = sh7723FillTriangle;
               funcs->DrawRectangle     = sh7723DrawRectangle;
               funcs->DrawLine          = sh7723DrawLine;
               funcs->Blit              = sh7723Blit;
               break;

          default:
               D_ERROR( "SH772x/Driver: Magic value 0x%08x doesn't match 0x%08x or 0x%08x!\n",
                        sdrv->gfx_shared->magic, SH7722GFX_SHARED_MAGIC, SH7723GFX_SHARED_MAGIC );
               dfb_gfxcard_unmap_mmio( device, sdrv->mmio_base, -1 );
               munmap( (void*) sdrv->gfx_shared, direct_page_align( sizeof(SH772xGfxSharedArea) ) );
               close( sdrv->gfx_fd );
               return DFB_INIT;
     }


     /* Get virtual address for the LCD buffer in slaves here,
        master does it in driver_init_device(). */
#ifndef SH772X_FBDEV_SUPPORT
     if (!dfb_core_is_master( core ))
          sdrv->lcd_virt = dfb_gfxcard_memory_virtual( device, sdev->lcd_offset );
#endif


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

     /* FIXME: Add a runtime option / config file. */
     sdev->lcd_format = DSPF_RGB16;

     /* Check format of LCD buffer. */
     switch (sdev->lcd_format) {
          case DSPF_RGB16:
          case DSPF_NV16:
               break;

          default:
               return DFB_UNSUPPORTED;
     }

     if (sdev->sh772x == 7723)
          memset( dfb_gfxcard_memory_virtual(device,0), 0, dfb_gfxcard_memory_length() );

     /*
      * Setup LCD buffer.
      */
#ifdef SH772X_FBDEV_SUPPORT
     { 
     	  struct fb_fix_screeninfo fsi;
     	  struct fb_var_screeninfo vsi;
		  int fbdev;

		  if ((fbdev = open("/dev/fb", O_RDONLY)) < 0) {
			   D_ERROR( "SH7722/Driver: Can't open fbdev to get LCDC info!\n" );
			   return DFB_FAILURE;
		  }

		  if (ioctl(fbdev, FBIOGET_FSCREENINFO, &fsi) < 0) {
			   D_ERROR( "SH7722/Driver: FBIOGET_FSCREEINFO failed.\n" );
			   close(fbdev);
			   return DFB_FAILURE;
		  }

		  if (ioctl(fbdev, FBIOGET_VSCREENINFO, &vsi) < 0) {
			   D_ERROR( "SH7722/Driver: FBIOGET_VSCREEINFO failed.\n" );
			   close(fbdev);
			   return DFB_FAILURE;
		  }

		  sdev->lcd_width  = vsi.xres;
		  sdev->lcd_height = vsi.yres;
		  sdev->lcd_pitch  = fsi.line_length;
		  sdev->lcd_size   = fsi.smem_len;
		  sdev->lcd_offset = 0;
		  sdev->lcd_phys   = fsi.smem_start;
#if 0
		  sdrv->lcd_virt   = mmap(NULL, fsi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED,
				  				  fbdev, 0);
		  if (sdrv->lcd_virt == MAP_FAILED) {
			   D_PERROR( "SH7722/Driver: mapping fbdev failed.\n" );
			   close(fbdev);
			   return DFB_FAILURE;
		  }

          /* Clear LCD buffer. */
          switch (sdev->lcd_format) {
               case DSPF_RGB16:
                    memset( (void*) sdrv->lcd_virt, 0x00, sdev->lcd_height * sdev->lcd_pitch );
                    break;

               case DSPF_NV16:
                    memset( (void*) sdrv->lcd_virt, 0x10, sdev->lcd_height * sdev->lcd_pitch );
                    memset( (void*) sdrv->lcd_virt + sdev->lcd_height * sdev->lcd_pitch, 0x80, sdev->lcd_height * sdev->lcd_pitch );
                    break;

               default:
                    D_BUG( "unsupported format" );
                    return DFB_BUG;
          }
#endif

		  close(fbdev);
     }     
#else
     sdev->lcd_width  = SH7722_LCD_WIDTH;
     sdev->lcd_height = SH7722_LCD_HEIGHT;
     sdev->lcd_pitch  = (DFB_BYTES_PER_LINE( sdev->lcd_format, sdev->lcd_width ) + 0xf) & ~0xf;
     sdev->lcd_size   = DFB_PLANE_MULTIPLY( sdev->lcd_format, sdev->lcd_height ) * sdev->lcd_pitch;
     sdev->lcd_offset = dfb_gfxcard_reserve_memory( device, sdev->lcd_size );

     if (sdev->lcd_offset < 0) {
          D_ERROR( "SH7722/Driver: Allocating %d bytes for the LCD buffer failed!\n", sdev->lcd_size );
          return DFB_FAILURE;
     }

     sdev->lcd_phys = dfb_gfxcard_memory_physical( device, sdev->lcd_offset );

     /* Get virtual addresses for LCD buffer in master here,
        slaves do it in driver_init_driver(). */
     sdrv->lcd_virt = dfb_gfxcard_memory_virtual( device, sdev->lcd_offset );
#endif

     D_INFO( "SH7722/LCD: Allocated %dx%d %s Buffer (%d bytes) at 0x%08lx (%p)\n",
             sdev->lcd_width, sdev->lcd_height, dfb_pixelformat_name(sdev->lcd_format),
             sdev->lcd_size, sdev->lcd_phys, sdrv->lcd_virt );

     D_ASSERT( ! (sdev->lcd_pitch & 0xf) );
     D_ASSERT( ! (sdev->lcd_phys & 0xf) );

     /*
      * Initialize hardware.
      */

     switch (sdev->sh772x) {
          case 7722:
               /* Reset the drawing engine. */
               sh7722EngineReset( sdrv, sdev );

               /* Fill in the device info. */
               snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   "SH7722" );
               snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Renesas" );

               /* Set device limitations. */
               device_info->limits.surface_byteoffset_alignment = 16;
               device_info->limits.surface_bytepitch_alignment  = 8;

               /* Set device capabilities. */
               device_info->caps.flags    = CCF_CLIPPING | CCF_RENDEROPTS;
               device_info->caps.accel    = SH7722_SUPPORTED_DRAWINGFUNCTIONS |
                                            SH7722_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = SH7722_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = SH7722_SUPPORTED_BLITTINGFLAGS;

               /* Change font format for acceleration. */
               if (!dfb_config->software_only) {
                    dfb_config->font_format  = DSPF_ARGB;
                    dfb_config->font_premult = false;
               }
               break;

          case 7723:
               /* Reset the drawing engine. */
               sh7723EngineReset( sdrv, sdev );

               /* Fill in the device info. */
               snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   "SH7723" );
               snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Renesas" );

               /* Set device limitations. */
               device_info->limits.surface_byteoffset_alignment = 512;
               device_info->limits.surface_bytepitch_alignment  = 64;

               /* Set device capabilities. */
               device_info->caps.flags    = CCF_CLIPPING | CCF_RENDEROPTS;
               device_info->caps.accel    = SH7723_SUPPORTED_DRAWINGFUNCTIONS | \
                                            SH7723_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = SH7723_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = SH7723_SUPPORTED_BLITTINGFLAGS;

               break;

          default:
               D_BUG( "unexpected device" );
               return DFB_BUG;
     }


     /* Wait for idle BEU. */
     while (SH7722_GETREG32( sdrv, BSTAR ) & 1);

     /* Disable all inputs. */
     SH7722_SETREG32( sdrv, BESTR, 0 );

     /* Disable all multi windows. */
     SH7722_SETREG32( sdrv, BMWCR0, SH7722_GETREG32( sdrv, BMWCR0 ) & ~0xf );

#ifndef SH772X_FBDEV_SUPPORT
     /* Clear LCD buffer. */
     switch (sdev->lcd_format) {
          case DSPF_RGB16:
               memset( (void*) sdrv->lcd_virt, 0x00, sdev->lcd_height * sdev->lcd_pitch );
               break;

          case DSPF_NV16:
               memset( (void*) sdrv->lcd_virt, 0x10, sdev->lcd_height * sdev->lcd_pitch );
               memset( (void*) sdrv->lcd_virt + sdev->lcd_height * sdev->lcd_pitch, 0x80, sdev->lcd_height * sdev->lcd_pitch );
               break;

          default:
               D_BUG( "unsupported format" );
               return DFB_BUG;
     }
#endif

     /*
      * TODO: Make LCD Buffer format and primary BEU format runtime configurable.
      */

     /* Set output pixel format of the BEU. */
     switch (sdev->lcd_format) {
          case DSPF_RGB16:
               SH7722_SETREG32( sdrv, BPKFR, BPKFR_RY_RGB | WPCK_RGB16 );
               break;

          case DSPF_NV16:
               SH7722_SETREG32( sdrv, BPKFR, BPKFR_RY_RGB | BPKFR_TE_ENABLED | CHDS_YCBCR422 );
               SH7722_SETREG32( sdrv, BDACR, sdev->lcd_phys + sdev->lcd_height * sdev->lcd_pitch );
               break;

          default:
               D_BUG( "unsupported format" );
               return DFB_BUG;
     }

     SH7722_SETREG32( sdrv, BPROCR, 0x00000000 );

     /* Have BEU render into LCD buffer. */
     SH7722_SETREG32( sdrv, BBLCR1, MT_MEMORY );
     SH7722_SETREG32( sdrv, BDAYR, sdev->lcd_phys & 0xfffffffc );
     SH7722_SETREG32( sdrv, BDMWR, sdev->lcd_pitch & 0x0003fffc );

#ifndef SH772X_FBDEV_SUPPORT
     /* Setup LCD controller to show the buffer. */
     sh7722_lcd_setup( sdrv, sdev->lcd_width, sdev->lcd_height,
                       sdev->lcd_phys, sdev->lcd_pitch, sdev->lcd_format, false );
#endif

     /* Initialize BEU lock. */
     fusion_skirmish_init( &sdev->beu_lock, "BEU", dfb_core_world(sdrv->core) );

     return DFB_OK;
}

static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data )
{
     SH7722DeviceData *sdev = device_data;

     D_DEBUG_AT( SH7722_Driver, "%s()\n", __FUNCTION__ );

     /* Destroy BEU lock. */
     fusion_skirmish_destroy( &sdev->beu_lock );
}

static void
driver_close_driver( CoreGraphicsDevice *device,
                     void               *driver_data )
{
     SH7722DriverData    *sdrv   = driver_data;
     SH772xGfxSharedArea *shared = sdrv->gfx_shared;

     (void) shared;

     D_DEBUG_AT( SH7722_Driver, "%s()\n", __FUNCTION__ );

     D_INFO( "SH7722/BLT: %u starts, %u done, %u interrupts, %u wait_idle, %u wait_next, %u idle\n",
             shared->num_starts, shared->num_done, shared->num_interrupts,
             shared->num_wait_idle, shared->num_wait_next, shared->num_idle );

     D_INFO( "SH7722/BLT: %u words, %u words/start, %u words/idle, %u starts/idle\n",
             shared->num_words,
             shared->num_words  / shared->num_starts,
             shared->num_words  / shared->num_idle,
             shared->num_starts / shared->num_idle );

     /* Shutdown JPEG library. */
     SH7722_JPEG_Shutdown();

     /* Unmap shared area. */
     munmap( (void*) sdrv->gfx_shared, direct_page_align( sizeof(SH772xGfxSharedArea) ) );

     /* Close Drawing Engine device. */
     close( sdrv->gfx_fd );
}

