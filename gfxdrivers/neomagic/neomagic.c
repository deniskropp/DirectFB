/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include <core/graphics_driver.h>


DFB_GRAPHICS_DRIVER( neomagic )

#include "neomagic.h"

/* for fifo/performance monitoring */
//unsigned int neo_fifo_space = 0;



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
          /* no support for other NeoMagic cards yet */
          case 95:        /* NM2200 */
          case 96:        /* NM2230 */
          case 97:        /* NM2360 */
          case 98:        /* NM2380 */
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
               "NeoMagic Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 0;

     info->driver_data_size = sizeof (NeoDriverData);
     info->device_data_size = sizeof (NeoDeviceData);

     switch (dfb_gfxcard_get_accelerator( device )) {
          /* no support for other NeoMagic cards yet */
          case 95:        /* NM2200 */
          case 96:        /* NM2230 */
          case 97:        /* NM2360 */
          case 98:        /* NM2380 */
               neo2200_get_info( device, info );
               break;
     }
}


static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data )
{
     NeoDriverData *ndrv = (NeoDriverData*) driver_data;

     ndrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!ndrv->mmio_base)
          return DFB_IO;

     switch (dfb_gfxcard_get_accelerator( device )) {
          /* no support for other NeoMagic cards yet */
          case 95:        /* NM2200 */
          case 96:        /* NM2230 */
          case 97:        /* NM2360 */
          case 98:        /* NM2380 */
               return neo2200_init_driver( device, funcs, driver_data );
     }

     return DFB_BUG;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     /* use polling for syncing, artefacts occur otherwise */
     dfb_config->pollvsync_after = 1;

     switch (dfb_gfxcard_get_accelerator( device )) {
          /* no support for other NeoMagic cards yet */
          case 95:        /* NM2200 */
          case 96:        /* NM2230 */
          case 97:        /* NM2360 */
          case 98:        /* NM2380 */
               return neo2200_init_device( device, device_info,
                                           driver_data, device_data );
     }

     return DFB_BUG;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     NeoDeviceData *ndev = (NeoDeviceData*) device_data;

     (void) ndev;

     switch (dfb_gfxcard_get_accelerator( device )) {
          /* no support for other NeoMagic cards yet */
          case 95:        /* NM2200 */
          case 96:        /* NM2230 */
          case 97:        /* NM2360 */
          case 98:        /* NM2380 */
               neo2200_close_device( device, driver_data, device_data );
     }

     DEBUGMSG( "DirectFB/NEO: FIFO Performance Monitoring:\n" );
     DEBUGMSG( "DirectFB/NEO:  %9d neo_waitfifo calls\n",
               ndev->waitfifo_calls );
     DEBUGMSG( "DirectFB/NEO:  %9d register writes (neo_waitfifo sum)\n",
               ndev->waitfifo_sum );
     DEBUGMSG( "DirectFB/NEO:  %9d FIFO wait cycles (depends on CPU)\n",
               ndev->fifo_waitcycles );
     DEBUGMSG( "DirectFB/NEO:  %9d IDLE wait cycles (depends on CPU)\n",
               ndev->idle_waitcycles );
     DEBUGMSG( "DirectFB/NEO:  %9d FIFO space cache hits(depends on CPU)\n",
               ndev->fifo_cache_hits );
     DEBUGMSG( "DirectFB/NEO: Conclusion:\n" );
     DEBUGMSG( "DirectFB/NEO:  Average register writes/neo_waitfifo"
               "call:%.2f\n",
               ndev->waitfifo_sum/(float)(ndev->waitfifo_calls) );
     DEBUGMSG( "DirectFB/NEO:  Average wait cycles/neo_waitfifo call:"
               " %.2f\n",
               ndev->fifo_waitcycles/(float)(ndev->waitfifo_calls) );
     DEBUGMSG( "DirectFB/NEO:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * ndev->fifo_cache_hits/
               (float)(ndev->waitfifo_calls)) );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     NeoDriverData *ndrv = (NeoDriverData*) driver_data;

     switch (dfb_gfxcard_get_accelerator( device )) {
          /* no support for other NeoMagic cards yet */
          case 95:        /* NM2200 */
          case 96:        /* NM2230 */
          case 97:        /* NM2360 */
          case 98:        /* NM2380 */
               neo2200_close_driver( device, driver_data );
     }

     dfb_gfxcard_unmap_mmio( device, ndrv->mmio_base, -1 );
}

