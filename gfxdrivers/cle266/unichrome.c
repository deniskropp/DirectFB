/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/


/*

EPIA-M benchmarks (df_dok)

                              SW    v0.0.1  v0.1.0 v0.2.0     v0.3

Anti-aliased Text            98.97    -       -       -     280.80  KChars/sec
Anti-aliased Text (blend)    28.85    -       -       -     280.61  KChars/sec
Fill Rectangles              25.21  443.46  437.05  432.39  435.60  Mpixel/sec
Fill Rectangles (blend)       5.54    -     130.12  128.42  127.82  MPixel/sec
Fill Triangles               24.84  173.44  129.76  127.86  128.63  MPixel/sec
Fill Triangles (blend)        5.46    -     129.81  127.86  128.67  MPixel/sec
Draw Rectangles              11.82  58.98    59.07   52.48   55.10  KRects/sec
Draw Rectangles (blend)       1.98    -      32.13   22.76   23.50  KRects/sec
Draw Lines                   42.67  283.81  292.33  193.87  203.20  KLines/sec
Draw Lines (blend)            8.54    -     142.62  101.23  102.80  KLines/sec
Blit                         21.48    -     117.38  114.26  114.41  MPixel/sec
Blit colorkeyed              22.54    -     117.34  114.26  114.41  MPixel/sec
Blit w/ format conversion    16.22    -       -     103.41  103.00  MPixel/sec
Blit from 32bit (blend)       4.19    -       -      87.72   87.32  MPixel/sec
Blit from 8bit palette       11.02    -       -     110.13  113.37  MPixel/sec
Blit from 8bit pal. (blend)   3.78    -       -     110.20  113.40  MPixel/sec
Stretch Blit                 23.19    -       -      99.53   99.32  MPixel/sec
Stretch Blit colorkeyed      25.04    -       -       5.00   38.00  MPixel/sec


Comparing M9000 and M10000

v0.2.0                         M9000  M10000

Anti-aliased Text               -       -     KChars/sec
Anti-aliased Text (blend)       -       -     KChars/sec
Fill Rectangles               401.82  432.39  Mpixel/sec
Fill Rectangles (blend)       129.05  128.42  MPixel/sec
Fill Triangles                128.46  127.86  MPixel/sec
Fill Triangles (blend)        128.46  127.86  MPixel/sec
Draw Rectangles                55.51   52.48  KRects/sec
Draw Rectangles (blend)        26.90   22.76  KRects/sec
Draw Lines                    225.00  193.87  KLines/sec
Draw Lines (blend)            121.29  101.23  KLines/sec
Blit                          112.36  114.26  MPixel/sec
Blit colorkeyed               112.28  114.26  MPixel/sec
Blit w/ format conversion     103.92  103.41  MPixel/sec
Blit from 32bit (blend)        87.89   87.72  MPixel/sec
Blit from 8bit palette        110.56  110.13  MPixel/sec
Blit from 8bit pal. (blend)   110.56  110.20  MPixel/sec
Stretch Blit                  108.67   99.53  MPixel/sec
Stretch Blit colorkeyed         4.79    5.00  MPixel/sec


v0.0.1 and v0.1.0 are tested on an EPIA-M9000,
later versions on an EPIA-M10000.

*/

// DirectFB headers

#include <directfb.h>

#include <direct/messages.h>

#include <core/coretypes.h>
#include <core/gfxcard.h>
#include <core/graphics_driver.h>
#include <core/surfacemanager.h>
#include <core/system.h>
#include <core/screens.h>

// System headers

#include <linux/fb.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <string.h>

// Driver headers

#include "unichrome.h"
#include "uc_state.h"
#include "uc_accel.h"
#include "uc_fifo.h"
#include "mmio.h"

#ifndef FB_ACCEL_VIA_UNICHROME
#define FB_ACCEL_VIA_UNICHROME 77
#endif

extern DisplayLayerFuncs ucOverlayFuncs;
extern DisplayLayerFuncs ucPrimaryFuncs;

extern DisplayLayerFuncs  ucOldPrimaryFuncs;
extern void              *ucOldPrimaryDriverData;

DFB_GRAPHICS_DRIVER(cle266)

//----------


/**
 * Dump beginning of virtual queue.
 * Use it to check that the VQ actually is in use. */
#if 0
static void uc_dump_vq(UcDeviceData *ucdev)
{
     int i;
     __u8* vq;

     if (!ucdev->vq_start) return;
     vq = dfb_system_video_memory_virtual(ucdev->vq_start);

     for (i = 0; i < 128; i++) {
          printf("%02x ", *(vq+i));
          if ((i+1) % 16 == 0) printf("\n");
     }
}
#endif

/** Allocate memory for the virtual queue. */

static DFBResult uc_alloc_vq(GraphicsDevice *device, UcDeviceData *ucdev)
{
     if (ucdev->vq_start) return DFB_OK;

     ucdev->vq_size = 256*1024; // 256kb
     ucdev->vq_start = dfb_gfxcard_reserve_memory( device, ucdev->vq_size );

     if (!ucdev->vq_start)
          return DFB_INIT;

     ucdev->vq_end = ucdev->vq_start + ucdev->vq_size - 1;

     // Debug: clear buffer
     memset((void *) dfb_system_video_memory_virtual(ucdev->vq_start),
            0xcc, ucdev->vq_size);

     // uc_dump_vq(ucdev);

     return DFB_OK;
}

/**
 * Initialize the hardware.
 * @param enable    enable VQ if true (else disable it.)
 */

DFBResult uc_init_2d_engine(GraphicsDevice *device, UcDeviceData *ucdev, UcDriverData *ucdrv, bool enable)
{
     DFBResult result = DFB_OK;
     volatile __u8* hwregs = ucdrv->hwregs;

     // Init 2D engine registers to reset 2D engine

     VIA_OUT(hwregs, 0x04, 0x0);
     VIA_OUT(hwregs, 0x08, 0x0);
     VIA_OUT(hwregs, 0x0c, 0x0);
     VIA_OUT(hwregs, 0x10, 0x0);
     VIA_OUT(hwregs, 0x14, 0x0);
     VIA_OUT(hwregs, 0x18, 0x0);
     VIA_OUT(hwregs, 0x1c, 0x0);
     VIA_OUT(hwregs, 0x20, 0x0);
     VIA_OUT(hwregs, 0x24, 0x0);
     VIA_OUT(hwregs, 0x28, 0x0);
     VIA_OUT(hwregs, 0x2c, 0x0);
     VIA_OUT(hwregs, 0x30, 0x0);
     VIA_OUT(hwregs, 0x34, 0x0);
     VIA_OUT(hwregs, 0x38, 0x0);
     VIA_OUT(hwregs, 0x3c, 0x0);
     VIA_OUT(hwregs, 0x40, 0x0);

     // Init AGP and VQ registers

     VIA_OUT(hwregs, 0x43c, 0x00100000);
     VIA_OUT(hwregs, 0x440, 0x00000000);
     VIA_OUT(hwregs, 0x440, 0x00333004);
     VIA_OUT(hwregs, 0x440, 0x60000000);
     VIA_OUT(hwregs, 0x440, 0x61000000);
     VIA_OUT(hwregs, 0x440, 0x62000000);
     VIA_OUT(hwregs, 0x440, 0x63000000);
     VIA_OUT(hwregs, 0x440, 0x64000000);
     VIA_OUT(hwregs, 0x440, 0x7D000000);

     VIA_OUT(hwregs, 0x43c, 0xfe020000);
     VIA_OUT(hwregs, 0x440, 0x00000000);

     if (enable) {
          result = uc_alloc_vq(device,ucdev);
          enable = (result == DFB_OK);
     }

     if (enable) { // Enable VQ

          VIA_OUT(hwregs, 0x43c, 0x00fe0000);
          VIA_OUT(hwregs, 0x440, 0x080003fe);
          VIA_OUT(hwregs, 0x440, 0x0a00027c);
          VIA_OUT(hwregs, 0x440, 0x0b000260);
          VIA_OUT(hwregs, 0x440, 0x0c000274);
          VIA_OUT(hwregs, 0x440, 0x0d000264);
          VIA_OUT(hwregs, 0x440, 0x0e000000);
          VIA_OUT(hwregs, 0x440, 0x0f000020);
          VIA_OUT(hwregs, 0x440, 0x1000027e);
          VIA_OUT(hwregs, 0x440, 0x110002fe);
          VIA_OUT(hwregs, 0x440, 0x200f0060);

          VIA_OUT(hwregs, 0x440, 0x00000006);
          VIA_OUT(hwregs, 0x440, 0x40008c0f);
          VIA_OUT(hwregs, 0x440, 0x44000000);
          VIA_OUT(hwregs, 0x440, 0x45080c04);
          VIA_OUT(hwregs, 0x440, 0x46800408);

          VIA_OUT(hwregs, 0x440, 0x52000000 |
                  ((ucdev->vq_start & 0xFF000000) >> 24) |
                  ((ucdev->vq_end & 0xFF000000) >> 16));
          VIA_OUT(hwregs, 0x440, 0x50000000 | (ucdev->vq_start & 0xFFFFFF));
          VIA_OUT(hwregs, 0x440, 0x51000000 | (ucdev->vq_end & 0xFFFFFF));
          VIA_OUT(hwregs, 0x440, 0x53000000 | (ucdev->vq_size >> 3));
     }
     else { // Disable VQ

          VIA_OUT(hwregs, 0x43c, 0x00fe0000);
          VIA_OUT(hwregs, 0x440, 0x00000004);
          VIA_OUT(hwregs, 0x440, 0x40008c0f);
          VIA_OUT(hwregs, 0x440, 0x44000000);
          VIA_OUT(hwregs, 0x440, 0x45080c04);
          VIA_OUT(hwregs, 0x440, 0x46800408);
     }

     return result;
}

void uc_init_3d_engine(volatile __u8* hwregs, int hwrev, bool init_all)
{
     __u32 i;

     if (init_all) {

          // Clear NotTex registers (?)

          VIA_OUT(hwregs, 0x43C, 0x00010000);
          for (i = 0; i <= 0x7d; i++)
               VIA_OUT(hwregs, 0x440, i << 24);

          // Clear texture unit 0 (?)

          VIA_OUT(hwregs, 0x43C, 0x00020000);
          for (i = 0; i <= 0x94; i++)
               VIA_OUT(hwregs, 0x440, i << 24);
          VIA_OUT(hwregs, 0x440, 0x82400000);

          // Clear texture unit 1 (?)

          VIA_OUT(hwregs, 0x43C, 0x01020000);
          for (i = 0; i <= 0x94; i++)
               VIA_OUT(hwregs, 0x440, i << 24);
          VIA_OUT(hwregs, 0x440, 0x82400000);

          // Clear general texture settings (?)

          VIA_OUT(hwregs, 0x43C, 0xfe020000);
          for (i = 0; i <= 0x03; i++)
               VIA_OUT(hwregs, 0x440, i << 24);

          // Clear palette settings (?)

          VIA_OUT(hwregs, 0x43C, 0x00030000);
          for (i = 0; i <= 0xff; i++)
               VIA_OUT(hwregs, 0x440, 0);

          VIA_OUT(hwregs, 0x43C, 0x00100000);
          VIA_OUT(hwregs, 0x440, 0x00333004);
          VIA_OUT(hwregs, 0x440, 0x10000002);
          VIA_OUT(hwregs, 0x440, 0x60000000);
          VIA_OUT(hwregs, 0x440, 0x61000000);
          VIA_OUT(hwregs, 0x440, 0x62000000);
          VIA_OUT(hwregs, 0x440, 0x63000000);
          VIA_OUT(hwregs, 0x440, 0x64000000);

          VIA_OUT(hwregs, 0x43C, 0x00fe0000);

          if (hwrev >= 3)
               VIA_OUT(hwregs, 0x440,0x40008c0f);
          else
               VIA_OUT(hwregs, 0x440,0x4000800f);

          VIA_OUT(hwregs, 0x440,0x44000000);
          VIA_OUT(hwregs, 0x440,0x45080C04);
          VIA_OUT(hwregs, 0x440,0x46800408);
          VIA_OUT(hwregs, 0x440,0x50000000);
          VIA_OUT(hwregs, 0x440,0x51000000);
          VIA_OUT(hwregs, 0x440,0x52000000);
          VIA_OUT(hwregs, 0x440,0x53000000);

     }

     VIA_OUT(hwregs, 0x43C,0x00fe0000);
     VIA_OUT(hwregs, 0x440,0x08000001);
     VIA_OUT(hwregs, 0x440,0x0A000183);
     VIA_OUT(hwregs, 0x440,0x0B00019F);
     VIA_OUT(hwregs, 0x440,0x0C00018B);
     VIA_OUT(hwregs, 0x440,0x0D00019B);
     VIA_OUT(hwregs, 0x440,0x0E000000);
     VIA_OUT(hwregs, 0x440,0x0F000000);
     VIA_OUT(hwregs, 0x440,0x10000000);
     VIA_OUT(hwregs, 0x440,0x11000000);
     VIA_OUT(hwregs, 0x440,0x20000000);
}

/** */

static void uc_after_set_var(void* drv, void* dev)
{
     UcDriverData* ucdrv = (UcDriverData*) drv;

     VGA_OUT8(ucdrv->hwregs, 0x3c4, 0x1a);
     // Clear bit 6 in extended VGA register 0x1a to prevent system lockup.
     VGA_OUT8(ucdrv->hwregs, 0x3c5, VGA_IN8(ucdrv->hwregs, 0x3c5) & 0xbf);
     // Set bit 2, it might make a difference.
     VGA_OUT8(ucdrv->hwregs, 0x3c5, VGA_IN8(ucdrv->hwregs, 0x3c5) | 0x4);
}

/** Wait until the engine is idle. */

static void uc_engine_sync(void* drv, void* dev)
{
     UcDriverData* ucdrv = (UcDriverData*) drv;
     UcDeviceData* ucdev = (UcDeviceData*) dev;

     int loop = 0;

/*    printf("Entering uc_engine_sync(), status is 0x%08x\n",
        VIA_IN(ucdrv->hwregs, VIA_REG_STATUS));
*/

     while ((VIA_IN(ucdrv->hwregs, VIA_REG_STATUS) & 0xfffeffff) != 0x00020000) {
          if (++loop > MAXLOOP) {
               D_ERROR("DirectFB/VIA: Timeout waiting for idle engine!\n");
               break;
          }
     }

     /* printf("Leaving uc_engine_sync(), status is 0x%08x, "
         "waiting for %d (0x%x) cycles.\n",
         VIA_IN(ucdrv->hwregs, VIA_REG_STATUS), loop, loop);
      */

     ucdev->idle_waitcycles += loop;
     ucdev->must_wait = 0;
}


// DirectFB interfacing functions --------------------------------------------

static int driver_probe(GraphicsDevice *device)
{
     struct stat s;

     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_VIA_UNICHROME:
               return 1;
     }

     return stat(UNICHROME_DEVICE, &s) + 1;
}

static void driver_get_info(GraphicsDevice* device,
                            GraphicsDriverInfo* info)
{
     // Fill in driver info structure.

     snprintf(info->name,
              DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
              "VIA UniChrome Driver");

     snprintf(info->vendor,
              DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
              "-");

     snprintf(info->url,
              DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH,
              "http://www.directfb.org");

     snprintf(info->license,
              DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH,
              "LGPL");

     info->version.major = 0;
     info->version.minor = 3;

     info->driver_data_size = sizeof (UcDriverData);
     info->device_data_size = sizeof (UcDeviceData);
}


static DFBResult driver_init_driver(GraphicsDevice* device,
                                    GraphicsDeviceFuncs* funcs,
                                    void* driver_data,
                                    void* device_data,
                                    CoreDFB *core)
{
     UcDriverData *ucdrv = (UcDriverData*) driver_data;

     //printf("Entering %s\n", __PRETTY_FUNCTION__);

     ucdrv->file = -1;
     ucdrv->pool = dfb_core_shmpool( core );

     ucdrv->hwregs = dfb_gfxcard_map_mmio( device, 0, 0 );
     if (!ucdrv->hwregs) {
          int fd;

          fd = open(UNICHROME_DEVICE, O_RDWR | O_SYNC, 0);
          if (fd < 0) {
               D_ERROR("Could not access %s. "
                        "Is the cle266vgaio module installed?\n", UNICHROME_DEVICE);
               return DFB_IO;
          }

          ucdrv->file = fd;

          ucdrv->hwregs = mmap(NULL, 0x1000000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
          if ((int) ucdrv->hwregs == -1)
               return DFB_IO;
     }

     /* FIXME: this belongs to device_data! */
     ucdrv->fifo = uc_fifo_create(ucdrv->pool, UC_FIFO_SIZE);
     if (!ucdrv->fifo)
          return DFB_NOSYSTEMMEMORY;

     uc_after_set_var(driver_data, device_data);

     ucdrv->hwrev = 3;   // FIXME: Get the real hardware revision number!!!

     // Driver specific initialization

     funcs->CheckState        = uc_check_state;
     funcs->SetState          = uc_set_state;
     funcs->EngineSync        = uc_engine_sync;
     funcs->EmitCommands      = uc_emit_commands;
     funcs->FlushTextureCache = uc_flush_texture_cache;
     funcs->AfterSetVar       = uc_after_set_var;

     funcs->FillRectangle     = uc_fill_rectangle;
     funcs->DrawRectangle     = uc_draw_rectangle;
     funcs->DrawLine          = uc_draw_line;
     funcs->FillTriangle      = uc_fill_triangle;
     funcs->Blit              = uc_blit;
     funcs->StretchBlit       = uc_stretch_blit;
     funcs->TextureTriangles  = uc_texture_triangles;


     /* install primary layer hooks */
     if ( getenv("DFB_CLE266_UNDERLAY"))
          dfb_layers_hook_primary( device, driver_data, &ucPrimaryFuncs,
                                    &ucOldPrimaryFuncs, &ucOldPrimaryDriverData );

     dfb_layers_register( dfb_screens_at(DSCID_PRIMARY),
                          driver_data, &ucOverlayFuncs );

     return DFB_OK;
}

static DFBResult driver_init_device(GraphicsDevice* device,
                                    GraphicsDeviceInfo* device_info,
                                    void* driver_data,
                                    void* device_data)
{
     UcDriverData *ucdrv = (UcDriverData*) driver_data;
     UcDeviceData *ucdev = (UcDeviceData*) device_data;

     //printf("Entering %s\n", __PRETTY_FUNCTION__);

     snprintf(device_info->name,
              DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "UniChrome");
     snprintf(device_info->vendor,
              DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "VIA/S3G");

     device_info->caps.flags = CCF_CLIPPING;
     device_info->caps.accel =
     UC_DRAWING_FUNCTIONS_2D | UC_DRAWING_FUNCTIONS_3D |
     UC_BLITTING_FUNCTIONS_2D | UC_BLITTING_FUNCTIONS_3D;

     device_info->caps.drawing  = UC_DRAWING_FLAGS_2D | UC_DRAWING_FLAGS_3D;
     device_info->caps.blitting = UC_BLITTING_FLAGS_2D | UC_BLITTING_FLAGS_3D;

     device_info->limits.surface_byteoffset_alignment = 32;
     device_info->limits.surface_pixelpitch_alignment = 32;

     ucdev->pitch = 0;
     ucdev->draw_rop2d = VIA_ROP_P;
     ucdev->draw_rop3d = HC_HROP_P;
     ucdev->color = 0;
     ucdev->bflags = 0;

     ucdev->must_wait = 0;
     ucdev->cmd_waitcycles = 0;
     ucdev->idle_waitcycles = 0;

     uc_init_2d_engine(device, ucdev, ucdrv, false); // VQ disabled - can't make it work.
     uc_init_3d_engine(ucdrv->hwregs, ucdrv->hwrev, 1);

     return DFB_OK;
}

static void driver_close_device(GraphicsDevice *device,
                                void *driver_data, void *device_data)
{
     UcDriverData* ucdrv = (UcDriverData*) driver_data;
     UcDeviceData* ucdev = (UcDeviceData*) device_data;

     // uc_dump_vq(ucdev);

     uc_engine_sync(driver_data, device_data);
     uc_init_2d_engine(device, ucdev, ucdrv, false);
}

static void driver_close_driver(GraphicsDevice* device, void* driver_data)
{
     UcDriverData* ucdrv = (UcDriverData*) driver_data;

     if (ucdrv->fifo)
          uc_fifo_destroy( ucdrv->fifo );

     if (ucdrv->file != -1)
          close( ucdrv->file );
}
