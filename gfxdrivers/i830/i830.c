/*
   Intel i830 DirectFB graphics driver

   (c) Copyright 2005       Servision Ltd.
                            http://www.servision.net/

   All rights reserved.

   Based on i810 driver written by Antonino Daplas <adaplas@pol.net>

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
#include <string.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/screens.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

/* need fb handle to get accel, MMIO programming in the i830 is useless */
#include <fbdev/fbdev.h>
#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>
#include <misc/util.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( i830 )

#include "i830.h"

/**************************************************************************************************/

#define TIMER_LOOP     1000000000
#define BUFFER_PADDING 2
#define MMIO_SIZE      512 * 1024

#define I830_SUPPORTED_DRAWINGFLAGS       (DSDRAW_NOFX)

#define I830_SUPPORTED_DRAWINGFUNCTIONS   (DFXL_NONE)

#define I830_SUPPORTED_BLITTINGFLAGS      (DSBLIT_NOFX)

#define I830_SUPPORTED_BLITTINGFUNCTIONS  (DFXL_NONE)

/**************************************************************************************************/

static void
i830_lring_enable( I830DriverData *idrv, __u32 mode )
{
     __u32 tmp;

     D_DEBUG_AT( I830_Ring, "%s lp ring...\n", mode ? "Enabling" : "Disabling" );

     tmp = i830_readl(idrv->mmio_base, LP_RING + RING_LEN);
     tmp = (!mode) ? tmp & ~1 : tmp | 1;

     i830_writel( idrv->mmio_base, LP_RING + RING_LEN, tmp );
}


static inline void
i830_wait_for_blit_idle( I830DriverData *idrv,
                         I830DeviceData *idev )
{
/*     __u32 count = 0;

     if (idev != NULL)
          idev->idle_calls++;

     while ((i830_readw(idrv->mmio_base, INST_DONE) & 0x7b) != 0x7b &&
            count++ < TIMER_LOOP) {
          if (idev != NULL)
               idev->idle_waitcycles++;
     }

     if (count >= TIMER_LOOP) {
          if (idev != NULL)
               idev->idle_timeoutsum++;
          D_BUG("warning: idle timeout exceeded");
     }*/
}

static void
i830_init_ringbuffer( I830DriverData *idrv,
                      I830DeviceData *idev )
{
     __u32 tmp1, tmp2;

     D_DEBUG_AT( I830_Ring, "Previous lp ring config: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
                 i830_readl(idrv->mmio_base, LP_RING),
                 i830_readl(idrv->mmio_base, LP_RING + RING_HEAD),
                 i830_readl(idrv->mmio_base, LP_RING + RING_START),
                 i830_readl(idrv->mmio_base, LP_RING + RING_LEN) );

     i830_writel(idrv->mmio_base, LP_RING + RING_LEN, 0);
     i830_writel(idrv->mmio_base, LP_RING + RING_HEAD, 0);
     i830_writel(idrv->mmio_base, LP_RING + RING_TAIL, 0);

     D_DEBUG_AT( I830_Ring, "INST_DONE: 0x%04x\n", i830_readw(idrv->mmio_base, INST_DONE) );


     i830_wait_for_blit_idle(idrv, idev);
     i830_lring_enable(idrv, 0);

     idev->lp_ring.size      = RINGBUFFER_SIZE;
     idev->lp_ring.tail_mask = idev->lp_ring.size - 1;

     i830_writel( idrv->mmio_base, LP_RING + RING_START,
                  (idrv->lring_bind.pg_start * 4096) & I830_RING_START_MASK );

     i830_writel( idrv->mmio_base, LP_RING + RING_LEN,
                  (idev->lp_ring.size        - 4096) & I830_RING_NR_PAGES );

     i830_lring_enable(idrv, 1);

     D_DEBUG_AT( I830_Ring, "Wrote lp ring config: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
                 i830_readl(idrv->mmio_base, LP_RING),
                 i830_readl(idrv->mmio_base, LP_RING + RING_HEAD),
                 i830_readl(idrv->mmio_base, LP_RING + RING_START),
                 i830_readl(idrv->mmio_base, LP_RING + RING_LEN) );
}

DFBResult
i830_wait_lp_ring( I830DriverData *idrv,
                   I830DeviceData *idev,
                   int             space )
{
     I830RingBuffer *buf = &idev->lp_ring;

     idev->waitfifo_calls++;
     idev->waitfifo_sum += space;

     D_DEBUG_AT( I830_Ring, "Waiting for %d...\n", space );

     if (buf->space < space) {
          int head  = 0;
          int loops = 0;

          do {
               idev->fifo_waitcycles++;

               if (loops++ > 100000000) {
                    D_ERROR( "timeout waiting for ring buffer space\n" );
                    return DFB_TIMEOUT;
               }

               buf->head  = i830_readl( idrv->mmio_base,
                                        LP_RING + RING_HEAD ) & I830_HEAD_MASK;
               buf->space = buf->head - (buf->tail + 8);

               if (buf->space < 0)
                    buf->space += buf->size;

               //D_DEBUG_AT( I830_Ring, "... have %d space\n", buf->space );

               if (buf->head != head)
                    loops = 0;

               head = buf->head;
          } while (buf->space < space);
     }
     else
          idev->fifo_cache_hits++;

     return DFB_OK;
}

/**************************************************************************************************/

static void
i830FlushTextureCache( void *drv, void *dev )
{
     I830DriverData *idrv = drv;
     I830DeviceData *idev = dev;
     I830RingBlock   block;

     if (i830_begin_lp_ring( idrv, idev, 2, &block ))
          return;

     i830_out_ring( &block, MI_FLUSH );
     i830_out_ring( &block, MI_NOOP );

     i830_advance_lp_ring( idrv, idev, &block );
}

static void
i830EngineSync( void *drv, void *dev )
{
     I830DriverData *idrv = drv;
     I830DeviceData *idev = dev;

     i830_wait_for_blit_idle( idrv, idev );
}

/**************************************************************************************************/

static void
i830CheckState(void *drv, void *dev,
               CardState *state, DFBAccelerationMask accel )
{
     switch (state->destination->format) {
          default:
               return;
     }

     if (!(accel & ~I830_SUPPORTED_DRAWINGFUNCTIONS) &&
         !(state->drawingflags & ~I830_SUPPORTED_DRAWINGFLAGS))
          state->accel |= I830_SUPPORTED_DRAWINGFUNCTIONS;

     if (!(accel & ~I830_SUPPORTED_BLITTINGFUNCTIONS) &&
         !(state->blittingflags & ~I830_SUPPORTED_BLITTINGFLAGS)) {
          if (state->source->format == state->destination->format)
               state->accel |= I830_SUPPORTED_BLITTINGFUNCTIONS;
     }
}

static void
i830SetState( void *drv, void *dev,
              GraphicsDeviceFuncs *funcs,
              CardState *state, DFBAccelerationMask accel )
{
     switch (accel) {
          default:
               D_BUG("unexpected drawing/blitting function");
     }

     state->modified = 0;
}

/**************************************************************************************************/

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_I830:          /* Intel 830 */
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
               "Intel 830/845G/852GM/855GM/865G Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Denis Oliver Kropp" );

     info->version.major = 0;
     info->version.minor = 1;

     info->driver_data_size = sizeof (I830DriverData);
     info->device_data_size = sizeof (I830DeviceData);
}

static void
i830_release_resource( I830DriverData *idrv )
{
     agp_unbind unbind;

     if (idrv->flags & I830RES_STATE_SAVE) {
          i830_writel( idrv->mmio_base, LP_RING, idrv->lring1 );
          i830_writel( idrv->mmio_base, LP_RING + RING_HEAD, idrv->lring2 );
          i830_writel( idrv->mmio_base, LP_RING + RING_START, idrv->lring3 );
          i830_writel( idrv->mmio_base, LP_RING + RING_LEN, idrv->lring4 );
     }

     if (idrv->flags & I830RES_MMAP)
          munmap((void *) idrv->aper_base, idrv->info.aper_size * 1024 * 1024);

     if (idrv->flags & I830RES_LRING_BIND) {
          unbind.key = idrv->lring_bind.key;
          ioctl(idrv->agpgart, AGPIOC_UNBIND, &unbind);
     }

     if (idrv->flags & I830RES_LRING_ACQ)
          ioctl(idrv->agpgart, AGPIOC_DEALLOCATE, idrv->lring_mem.key);

     if (idrv->flags & I830RES_OVL_BIND) {
          unbind.key = idrv->ovl_bind.key;
          ioctl(idrv->agpgart, AGPIOC_UNBIND, &unbind);
     }

     if (idrv->flags & I830RES_OVL_ACQ)
          ioctl(idrv->agpgart, AGPIOC_DEALLOCATE, idrv->ovl_mem.key);

     if (idrv->flags & I830RES_GART_ACQ)
          ioctl(idrv->agpgart, AGPIOC_RELEASE);

     if (idrv->flags & I830RES_GART)
          close(idrv->agpgart);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data )
{
     I830DriverData *idrv = driver_data;
     agp_setup setup;
     __u32 base;

     idrv->idev = device_data;

     idrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!idrv->mmio_base)
          return DFB_IO;

     idrv->agpgart = open("/dev/agpgart", O_RDWR);
     if (idrv->agpgart == -1)
          return DFB_IO;
     idrv->flags |= I830RES_GART;

     if (ioctl(idrv->agpgart, AGPIOC_ACQUIRE))
          return DFB_IO;
     idrv->flags |= I830RES_GART_ACQ;

     setup.agp_mode = 0;
     if (ioctl(idrv->agpgart, AGPIOC_SETUP, &setup))
          return DFB_IO;

     if (ioctl(idrv->agpgart, AGPIOC_INFO, &idrv->info))
          return DFB_IO;

     idrv->aper_base =  mmap(NULL, idrv->info.aper_size * 1024 * 1024, PROT_WRITE, MAP_SHARED,
                                idrv->agpgart, 0);
     if (idrv->aper_base == MAP_FAILED) {
          i830_release_resource(idrv);
          return DFB_IO;
     }
     idrv->flags |= I830RES_MMAP;

     /* We'll attempt to bind at fb_base + fb_len + 1 MB,
     to be safe */
     base = dfb_gfxcard_memory_physical(device, 0) - idrv->info.aper_base;
     base += dfb_gfxcard_memory_length();
     base += (1024 * 1024);

     idrv->lring_mem.pg_count = RINGBUFFER_SIZE/4096;
     idrv->lring_mem.type = AGP_NORMAL_MEMORY;
     if (ioctl(idrv->agpgart, AGPIOC_ALLOCATE, &idrv->lring_mem)) {
          i830_release_resource(idrv);
          return DFB_IO;
     }
     idrv->flags |= I830RES_LRING_ACQ;

     idrv->lring_bind.key = idrv->lring_mem.key;
     idrv->lring_bind.pg_start = base/4096;
     if (ioctl(idrv->agpgart, AGPIOC_BIND, &idrv->lring_bind)) {
          i830_release_resource(idrv);
          return DFB_IO;
     }
     idrv->flags |= I830RES_LRING_BIND;

     idrv->ovl_mem.pg_count = 1;
     idrv->ovl_mem.type = AGP_PHYSICAL_MEMORY;
     if (ioctl(idrv->agpgart, AGPIOC_ALLOCATE, &idrv->ovl_mem)) {
          i830_release_resource(idrv);
          return DFB_IO;
     }
     idrv->flags |= I830RES_OVL_ACQ;

     idrv->ovl_bind.key = idrv->ovl_mem.key;
     idrv->ovl_bind.pg_start = (base + RINGBUFFER_SIZE)/4096;
     if (ioctl(idrv->agpgart, AGPIOC_BIND, &idrv->ovl_bind)) {
          i830_release_resource(idrv);
          return DFB_IO;
     }
     idrv->flags |= I830RES_OVL_BIND;

     idrv->lring_base = idrv->aper_base + idrv->lring_bind.pg_start * 4096;
     idrv->ovl_base = idrv->aper_base + idrv->ovl_bind.pg_start * 4096;
     idrv->pattern_base = idrv->ovl_base + 1024;
     memset((void *) idrv->ovl_base, 0xff, 1024);
     memset((void *) idrv->pattern_base, 0xff, 4096 - 1024);

     idrv->lring1 = 0;//i830_readl(idrv->mmio_base, LP_RING);
     idrv->lring2 = 0;//i830_readl(idrv->mmio_base, LP_RING + RING_HEAD);
     idrv->lring3 = 0;//i830_readl(idrv->mmio_base, LP_RING + RING_START);
     idrv->lring4 = 0;//i830_readl(idrv->mmio_base, LP_RING + RING_LEN);
     idrv->flags |= I830RES_STATE_SAVE;

     funcs->CheckState         = i830CheckState;
     funcs->SetState           = i830SetState;
     funcs->EngineSync         = i830EngineSync;
     funcs->FlushTextureCache  = i830FlushTextureCache;

     dfb_layers_register( dfb_screens_at(DSCID_PRIMARY), driver_data, &i830OverlayFuncs );

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     I830DriverData *idrv = driver_data;
     I830DeviceData *idev = device_data;

     int offset;

     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "830/845G/852GM/855GM/865G" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Intel" );

     device_info->caps.flags    = 0;
     device_info->caps.accel    = I830_SUPPORTED_DRAWINGFUNCTIONS |
                                  I830_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = I830_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = I830_SUPPORTED_BLITTINGFLAGS;

     device_info->limits.surface_byteoffset_alignment = 32 * 4;
     device_info->limits.surface_pixelpitch_alignment = 32;
     device_info->limits.surface_bytepitch_alignment  = 64;

     dfb_config->pollvsync_after = 1;


/*     offset = dfb_gfxcard_reserve_memory( device, RINGBUFFER_SIZE );

     idrv->lring_mem.physical = dfb_gfxcard_memory_physical( device, offset );
     idrv->lring_base = dfb_gfxcard_memory_virtual( device, offset );


     offset = dfb_gfxcard_reserve_memory( device, 4096 );

     idrv->ovl_mem.physical = dfb_gfxcard_memory_physical( device, offset );
     idrv->ovl_base = dfb_gfxcard_memory_virtual( device, offset );*/

/*     D_DEBUG_AT( I830_Ring, "lp_ring at 0x%08x (%p)\n",
                 idrv->lring_mem.physical, idrv->lring_base );

     D_DEBUG_AT( I830_Ring, "ovl at 0x%08x (%p)\n",
                 idrv->ovl_mem.physical, idrv->ovl_base );*/

     i830_init_ringbuffer( idrv, idev );

     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     I830DeviceData *idev = device_data;
     I830DriverData *idrv = driver_data;

     i830ovlOnOff( idrv, idev, false );

     i830_wait_for_blit_idle(idrv, idev);
     i830_lring_enable(idrv, 0);

     D_DEBUG( "DirectFB/I830: DMA Buffer Performance Monitoring:\n");
     D_DEBUG( "DirectFB/I830:  %9d DMA buffer size in KB\n",
              RINGBUFFER_SIZE/1024 );
     D_DEBUG( "DirectFB/I830:  %9d i830_wait_for_blit_idle calls\n",
              idev->idle_calls );
     D_DEBUG( "DirectFB/I830:  %9d i830_wait_for_space calls\n",
              idev->waitfifo_calls );
     D_DEBUG( "DirectFB/I830:  %9d BUFFER transfers (i830_wait_for_space sum)\n",
              idev->waitfifo_sum );
     D_DEBUG( "DirectFB/I830:  %9d BUFFER wait cycles (depends on GPU/CPU)\n",
              idev->fifo_waitcycles );
     D_DEBUG( "DirectFB/I830:  %9d IDLE wait cycles (depends on GPU/CPU)\n",
              idev->idle_waitcycles );
     D_DEBUG( "DirectFB/I830:  %9d BUFFER space cache hits(depends on BUFFER size)\n",
              idev->fifo_cache_hits );
     D_DEBUG( "DirectFB/I830:  %9d BUFFER timeout sum (possible hardware crash)\n",
              idev->fifo_timeoutsum );
     D_DEBUG( "DirectFB/I830:  %9d IDLE timeout sum (possible hardware crash)\n",
              idev->idle_timeoutsum );
     D_DEBUG( "DirectFB/I830: Conclusion:\n" );
     D_DEBUG( "DirectFB/I830:  Average buffer transfers per i830_wait_for_space "
              "call: %.2f\n",
              idev->waitfifo_sum/(float)(idev->waitfifo_calls) );
     D_DEBUG( "DirectFB/I830:  Average wait cycles per i830_wait_for_space call:"
              " %.2f\n",
              idev->fifo_waitcycles/(float)(idev->waitfifo_calls) );
     D_DEBUG( "DirectFB/I830:  Average wait cycles per i830_wait_for_blit_idle call:"
              " %.2f\n",
              idev->idle_waitcycles/(float)(idev->idle_calls) );
     D_DEBUG( "DirectFB/I830:  Average buffer space cache hits: %02d%%\n",
              (int)(100 * idev->fifo_cache_hits/
                    (float)(idev->waitfifo_calls)) );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     I830DriverData *idrv = (I830DriverData *) driver_data;

     i830_release_resource(idrv);

     dfb_gfxcard_unmap_mmio( device, idrv->mmio_base, -1);
}

