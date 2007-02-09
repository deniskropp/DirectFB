/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

// DirectFB headers

#include <config.h>

#include <fbdev/fbdev.h>

#include <directfb.h>

#include <direct/messages.h>

#include <fusion/shmalloc.h>

#include <core/coretypes.h>
#include <core/gfxcard.h>
#include <core/graphics_driver.h>
#include <core/surfacemanager.h>
#include <core/system.h>
#include <core/screens.h>

#include <misc/conf.h>

#include <fbdev/fb.h>

// System headers

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

#include <string.h>

// Driver headers

#include "unichrome.h"
#include "uc_state.h"
#include "uc_accel.h"
#include "uc_fifo.h"
#include "uc_ioctl.h"
#include "mmio.h"
#include "uc_probe.h"

extern DisplayLayerFuncs ucOverlayFuncs;
extern DisplayLayerFuncs ucPrimaryFuncs;
extern DisplayLayerFuncs ucSubpictureFuncs;

extern DisplayLayerFuncs  ucOldPrimaryFuncs;
extern void              *ucOldPrimaryDriverData;


DFB_GRAPHICS_DRIVER(unichrome)

//----------

/* PCI probing code is derived from gfxdrivers/matrox/matrox.c */

/** Read PCI configuration register 'reg' for device at {bus,slot,func}. */
static int pci_config_in8( unsigned int bus,
                           unsigned int slot,
                           unsigned int func,
                           u8 reg )
{
    char  filename[512];
    int   fd;
    int   val;

    val = 0;

    snprintf( filename, 512, "/proc/bus/pci/%02x/%02x.%x", bus, slot, func );

    fd = open( filename, O_RDONLY );
    if (fd < 0) {
        D_PERROR( "DirectFB/Unichrome: Error opening `%s'!\n", filename );
        return -1;
    }

    if (lseek( fd, reg, SEEK_SET ) == reg) {
        if (read( fd, &val, 1 ) == 1) {
            close( fd );
            return val;
        }
    }

    close( fd );
    return -1;
}

/* Probe for a Unichrome device.
* @returns DFB_OK if successful, with ucdrv->hwid, ucdrv->hwrev,
* and ucdrv->name filled in.
*/
DFBResult uc_probe_pci( UcDriverData *ucdrv )
{
    unsigned int    bus, devfn, vendor, device;
    char            line[512];
    FILE            *file;
    int             i;

    const char* filename = "/proc/bus/pci/devices";

    file = fopen( filename, "r" );
    if (!file) {
        D_PERROR( "DirectFB/Unichrome: Error opening `%s'!\n", filename );
        return errno2result( errno );
    }

    while (fgets( line, 512, file )) {
        if (sscanf( line, "%02x%02x\t%04x%04x",
            &bus, &devfn, &vendor, &device ) != 4)
            continue;

        if (vendor != PCI_VENDOR_ID_VIA)
            continue;

        for (i = 0; uc_via_devices[i].id != 0; i++) {
            if (device == uc_via_devices[i].id) {
                // Found a Unichrome device.
                ucdrv->hwid = device;
                ucdrv->name = uc_via_devices[i].name;
                // Read its revision number from the host bridge.
                ucdrv->hwrev = pci_config_in8(0, 0, 0, 0xf6);
                if (ucdrv->hwrev == -1 && dfb_config->unichrome_revision == -1) {
                    ucdrv->hwrev = 0x11;    // a fairly arbitrary default
                    D_ERROR( "DirectFB/Unichrome: Failed to determine hardware revision, assuming %d.\n",
                        ucdrv->hwrev );
                }
                // Because we can only auto-detect if we're superuser,
                // allow an override
                if (dfb_config->unichrome_revision != -1)
                    ucdrv->hwrev = dfb_config->unichrome_revision;
                fclose( file );
                return DFB_OK;
            }
        }
    }

    D_ERROR( "DirectFB/Unichrome: Can't find a Unichrome device in `%s'!\n",
        filename );

    fclose( file );
    return DFB_INIT;
}

/**
 * Dump beginning of virtual queue.
 * Use it to check that the VQ actually is in use. */
#if 0
static void uc_dump_vq(UcDeviceData *ucdev)
{
     int i;
     u8* vq;

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
     volatile u8* hwregs = ucdrv->hwregs;
     int i;

     // Init 2D engine registers to reset 2D engine

     for ( i = 0x04; i <= 0x40; i += 4 )
          VIA_OUT(hwregs, i, 0x0);

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

void uc_init_3d_engine(volatile u8* hwregs, int hwrev, bool init_all)
{
     u32 i;

     if (init_all) {

          // Clear NotTex registers

          VIA_OUT(hwregs, 0x43C, 0x00010000);
          for (i = 0; i <= 0x7d; i++)
               VIA_OUT(hwregs, 0x440, i << 24);

          // Clear texture unit 0

          VIA_OUT(hwregs, 0x43C, 0x00020000);
          for (i = 0; i <= 0x94; i++)
               VIA_OUT(hwregs, 0x440, i << 24);
          VIA_OUT(hwregs, 0x440, 0x82400000);

          // Clear texture unit 1

          VIA_OUT(hwregs, 0x43C, 0x01020000);
          for (i = 0; i <= 0x94; i++)
               VIA_OUT(hwregs, 0x440, i << 24);
          VIA_OUT(hwregs, 0x440, 0x82400000);

          // Clear general texture settings

          VIA_OUT(hwregs, 0x43C, 0xfe020000);
          for (i = 0; i <= 0x03; i++)
               VIA_OUT(hwregs, 0x440, i << 24);

          // Clear palette settings

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

     VIA_OUT(ucdrv->hwregs, VIA_REG_CURSOR_MODE, VIA_IN(ucdrv->hwregs, VIA_REG_CURSOR_MODE) & 0xFFFFFFFE);
}

/** Wait until the engine is idle. */

static DFBResult uc_engine_sync(void* drv, void* dev)
{
     UcDriverData* ucdrv = (UcDriverData*) drv;
     UcDeviceData* ucdev = (UcDeviceData*) dev;

     int loop = 0;

/*    printf("Entering uc_engine_sync(), status is 0x%08x\n",
        VIA_IN(ucdrv->hwregs, VIA_REG_STATUS));
*/

     while ((VIA_IN(ucdrv->hwregs, VIA_REG_STATUS) & 0xfffeffff) != 0x00020000) {
          if (++loop > MAXLOOP) {
               D_ERROR("DirectFB/Unichrome: Timeout waiting for idle engine!\n");
               break;

               /* FIXME: return DFB_TIMEOUT and implement EngineReset! */
          }
     }

     /* printf("Leaving uc_engine_sync(), status is 0x%08x, "
         "waiting for %d (0x%x) cycles.\n",
         VIA_IN(ucdrv->hwregs, VIA_REG_STATUS), loop, loop);
      */

     ucdev->idle_waitcycles += loop;
     ucdev->must_wait = 0;

     return DFB_OK;
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
     info->version.minor = 4;

     info->driver_data_size = sizeof (UcDriverData);
     info->device_data_size = sizeof (UcDeviceData);
}

static void uc_probe_fbdev(UcDriverData *ucdrv)
{
     struct fb_flip flip;
     FBDev *dfb_fbdev = dfb_system_data();
     flip.device = VIAFB_FLIP_NOP;
     if (ioctl(dfb_fbdev->fd, FBIO_FLIPONVSYNC, &flip) == 0)
          ucdrv->canfliponvsync = true;
     else
          ucdrv->canfliponvsync = false;
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
              D_ERROR("DirectFB/Unichrome: Could not access %s. "
                        "Is the ucio module installed?\n", UNICHROME_DEVICE);
               return DFB_IO;
          }

          ucdrv->file = fd;

          ucdrv->hwregs = mmap(NULL, 0x1000000, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
          if ((int) ucdrv->hwregs == -1)
               return DFB_IO;
     }

     // Get hardware id and revision.
     uc_probe_pci(ucdrv);

     // Check framebuffer device capabilities
     uc_probe_fbdev(ucdrv);

     /* FIXME: this belongs to device_data! */
     ucdrv->fifo = uc_fifo_create(ucdrv->pool, UC_FIFO_SIZE);
     if (!ucdrv->fifo)
          return D_OOSHM();

     uc_after_set_var(driver_data, device_data);

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

     ucdrv->ovl = NULL;

     /* install primary layer hooks */
     dfb_layers_hook_primary( device, driver_data, &ucPrimaryFuncs,
                              &ucOldPrimaryFuncs, &ucOldPrimaryDriverData );

     dfb_layers_register( dfb_screens_at(DSCID_PRIMARY),
                          driver_data, &ucOverlayFuncs );
     dfb_layers_register( dfb_screens_at(DSCID_PRIMARY),
                          driver_data, &ucSubpictureFuncs );

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

     if (ucdrv->name != NULL) {
          snprintf(device_info->name,
                   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "%s", ucdrv->name);
     }
     else {
          snprintf(device_info->name,
                   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "UniChrome");
     }
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
          uc_fifo_destroy( ucdrv->pool, ucdrv->fifo );

     if (ucdrv->file != -1)
          close( ucdrv->file );
}
