/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de> and
	      Alex Song <alexsong@comports.com>.

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

#include <asm/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( savage )

#include "savage.h"
#include "savage3d.h"
#include "savage4.h"
#include "savage2000.h"
#include "savage_bci.h"

/* exported symbols */

static int
driver_get_abi_version()
{
     return DFB_GRAPHICS_DRIVER_ABI_VERSION;
}

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_SAVAGE3D:       /* Savage3D series     */
          case FB_ACCEL_SAVAGE3D_MV:
          case FB_ACCEL_SAVAGE_MX_MV:
          case FB_ACCEL_SAVAGE_MX:
          case FB_ACCEL_SAVAGE_IX_MV:
          case FB_ACCEL_SAVAGE_IX:
               return 1;

          case FB_ACCEL_SAVAGE4:        /* Savage4 series      */
          case FB_ACCEL_PROSAVAGE_PM:
          case FB_ACCEL_PROSAVAGE_KM:
          case FB_ACCEL_S3TWISTER_P:
          case FB_ACCEL_S3TWISTER_K:
               return 1;
          
          case FB_ACCEL_SAVAGE2000:     /* Savage 2000         */
               return 1;
     }

     return 0;
}

static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "Savage Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     /* some defaults, each driver has it's own version */
     info->version.major = 0;
     info->version.minor = 3;

     info->driver_data_size = sizeof (SavageDriverData);
     info->device_data_size = sizeof (SavageDeviceData);

     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_SAVAGE3D:       /* Savage3D series */
          case FB_ACCEL_SAVAGE3D_MV:
          case FB_ACCEL_SAVAGE_MX_MV:
          case FB_ACCEL_SAVAGE_MX:
          case FB_ACCEL_SAVAGE_IX_MV:
          case FB_ACCEL_SAVAGE_IX:
               savage3d_get_info( device, info );
               break;

          case FB_ACCEL_SAVAGE4:        /* Savage4 series  */
          case FB_ACCEL_PROSAVAGE_PM:
          case FB_ACCEL_PROSAVAGE_KM:
          case FB_ACCEL_S3TWISTER_P:
          case FB_ACCEL_S3TWISTER_K:
               savage4_get_info( device, info );
               break;
          
          case FB_ACCEL_SAVAGE2000:     /* Savage 2000     */
               savage2000_get_info( device, info );
               break;
     }
}


static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;

     sdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!sdrv->mmio_base)
          return DFB_IO;

     sdrv->bci_base = (__u32*)(sdrv->mmio_base + BCI_BUFFER_OFFSET);
     
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_SAVAGE3D:       /* Savage3D series */
          case FB_ACCEL_SAVAGE3D_MV:
          case FB_ACCEL_SAVAGE_MX_MV:
          case FB_ACCEL_SAVAGE_MX:
          case FB_ACCEL_SAVAGE_IX_MV:
          case FB_ACCEL_SAVAGE_IX:
               return savage3d_init_driver( device, funcs, driver_data );

          case FB_ACCEL_SAVAGE4:        /* Savage4 series  */
          case FB_ACCEL_PROSAVAGE_PM:
          case FB_ACCEL_PROSAVAGE_KM:
          case FB_ACCEL_S3TWISTER_P:
          case FB_ACCEL_S3TWISTER_K:
               return savage4_init_driver( device, funcs, driver_data );
          
          case FB_ACCEL_SAVAGE2000:     /* Savage 2000     */
               return savage2000_init_driver( device, funcs, driver_data );
     }

     return DFB_BUG;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageDeviceData *sdev = (SavageDeviceData*) device_data;
     volatile __u8    *mmio = sdrv->mmio_base;

     /* use polling for syncing, artefacts occur otherwise */
     dfb_config->pollvsync_after = 1;

     sdev->accel_id = dfb_gfxcard_get_accelerator( device );

     switch (sdev->accel_id) {
          case FB_ACCEL_SAVAGE3D:       /* Savage3D series */
          case FB_ACCEL_SAVAGE3D_MV:
          case FB_ACCEL_SAVAGE_MX_MV:
          case FB_ACCEL_SAVAGE_MX:
          case FB_ACCEL_SAVAGE_IX_MV:
          case FB_ACCEL_SAVAGE_IX:
               savage3d_init_device( device, device_info,
                                     driver_data, device_data );
               break;

          case FB_ACCEL_SAVAGE4:        /* Savage4 series  */
          case FB_ACCEL_PROSAVAGE_PM:
          case FB_ACCEL_PROSAVAGE_KM:
          case FB_ACCEL_S3TWISTER_P:
          case FB_ACCEL_S3TWISTER_K:
               savage4_init_device( device, device_info,
                                    driver_data, device_data );
               break;
          
          case FB_ACCEL_SAVAGE2000:     /* Savage 2000     */
               savage2000_init_device( device, device_info,
                                       driver_data, device_data );
               break;
          default:
               BUG("unexpected accelerator id");
               return DFB_BUG;
     }

     /* Turn on 16-bit register access. */

     vga_out8( mmio, 0x3d4, 0x31);
     vga_out8( mmio, 0x3d5, 0x0c);

     /* Set stride to use GBD. */

     vga_out8( mmio, 0x3d4, 0x50);
     vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) | 0xC1);

     /* Enable 2D engine. */

     vga_out8( mmio, 0x3d4, 0x40 );
     vga_out8( mmio, 0x3d5, 0x01 );
     
     
     savage_out32( mmio, MONO_PAT_0, ~0 );
     savage_out32( mmio, MONO_PAT_1, ~0 );

     /* Setup plane masks */
     savage_out32( mmio, 0x8128, ~0 ); /* enable all write planes */
     savage_out32( mmio, 0x812C, ~0 ); /* enable all read planes */
     savage_out16( mmio, 0x8134, 0x27 );
     savage_out16( mmio, 0x8136, 0x07 );
     
     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     SavageDeviceData *sdev = (SavageDeviceData*) device_data;

     (void) sdev;

     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_SAVAGE3D:       /* Savage3D series */
          case FB_ACCEL_SAVAGE3D_MV:
          case FB_ACCEL_SAVAGE_MX_MV:
          case FB_ACCEL_SAVAGE_MX:
          case FB_ACCEL_SAVAGE_IX_MV:
          case FB_ACCEL_SAVAGE_IX:
               savage3d_close_device( device, driver_data, device_data );
               break;

          case FB_ACCEL_SAVAGE4:        /* Savage4 series  */
          case FB_ACCEL_PROSAVAGE_PM:
          case FB_ACCEL_PROSAVAGE_KM:
          case FB_ACCEL_S3TWISTER_P:
          case FB_ACCEL_S3TWISTER_K:
               savage4_close_device( device, driver_data, device_data );
               break;
          
          case FB_ACCEL_SAVAGE2000:     /* Savage 2000     */
               savage2000_close_device( device, driver_data, device_data );
               break;
     }

     DEBUGMSG( "DirectFBSavage: FIFO Performance Monitoring:\n" );
     DEBUGMSG( "DirectFBSavage:  %9d savage_waitfifo calls\n",
               sdev->waitfifo_calls );
     DEBUGMSG( "DirectFBSavage:  %9d savage_waitidle calls\n",
               sdev->waitidle_calls );
     DEBUGMSG( "DirectFBSavage:  %9d register writes (savage_waitfifo sum)\n",
               sdev->waitfifo_sum );
     DEBUGMSG( "DirectFBSavage:  %9d FIFO wait cycles (depends on CPU)\n",
               sdev->fifo_waitcycles );
     DEBUGMSG( "DirectFBSavage:  %9d IDLE wait cycles (depends on CPU)\n",
               sdev->idle_waitcycles );
     DEBUGMSG( "DirectFBSavage:  %9d FIFO space cache hits(depends on CPU)\n",
               sdev->fifo_cache_hits );
     DEBUGMSG( "DirectFBSavage: Conclusion:\n" );
     DEBUGMSG( "DirectFBSavage:  Average register writes/savage_waitfifo "
               "call: %.2f\n",
               sdev->waitfifo_sum/(float)(sdev->waitfifo_calls) );
     DEBUGMSG( "DirectFBSavage:  Average wait cycles/savage_waitfifo call:"
               " %.2f\n",
               sdev->fifo_waitcycles/(float)(sdev->waitfifo_calls) );
     DEBUGMSG( "DirectFBSavage:  Average wait cycles/savage_waitidle call:"
               " %.2f\n",
               sdev->idle_waitcycles/(float)(sdev->waitidle_calls) );
     DEBUGMSG( "DirectFBSavage:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * sdev->fifo_cache_hits/
               (float)(sdev->waitfifo_calls)) );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;

     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_SAVAGE3D:       /* Savage3D series */
          case FB_ACCEL_SAVAGE3D_MV:
          case FB_ACCEL_SAVAGE_MX_MV:
          case FB_ACCEL_SAVAGE_MX:
          case FB_ACCEL_SAVAGE_IX_MV:
          case FB_ACCEL_SAVAGE_IX:
               savage3d_close_driver( device, driver_data );
               break;

          case FB_ACCEL_SAVAGE4:        /* Savage4 series  */
          case FB_ACCEL_PROSAVAGE_PM:
          case FB_ACCEL_PROSAVAGE_KM:
          case FB_ACCEL_S3TWISTER_P:
          case FB_ACCEL_S3TWISTER_K:
               savage4_close_driver( device, driver_data );
               break;
          
          case FB_ACCEL_SAVAGE2000:     /* Savage 2000     */
               savage2000_close_driver( device, driver_data );
               break;
     }

     dfb_gfxcard_unmap_mmio( device, sdrv->mmio_base, -1 );
}

