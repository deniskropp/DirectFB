/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <asm/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <directfb.h>
#include <core/coredefs.h>
#include "uc_probe.h"

/**
* Match a device string with a device ID.
*
* @param s          Device string read from /proc/bus/pci/devices
* @param devinfo    Output: device information.
*
* @returns 1: success, 0: line too short, -1: parse error.
*/

int uc_parse_vga_device(char* s, struct uc_vga_device* devinfo)
{
    int i;
    int item[17];

    /* Build list of item start positions. */

    item[0] = 0;
    item[1] = 5;
    item[2] = 14;

    for (i = 3; i < 18; i++) {
        item[i] = (i - 3) * 9 + 16;
    }

    /* Perform some basic sanity checks */

    if (strlen(s) < 142) return 0;

    for (i = 1; i < 17; i++) {  /* Verify tab positions. */
        if (s[item[i]-1] != 9) return -1;
    }

    /* Read data. */

    devinfo->devid = strtoul(s + item[1], NULL, 16);
    devinfo->busid = strtoul(s + item[0], NULL, 16);
    devinfo->irq = strtoul(s + item[2], NULL, 16);
    devinfo->fbmem = strtoul(s + item[3], NULL, 16) & ~0xf;
    devinfo->iomem = strtoul(s + item[4], NULL, 16) & ~0xf;
    devinfo->fbsize = strtoul(s + item[3 + 7], NULL, 16);
    devinfo->iosize = strtoul(s + item[4 + 7], NULL, 16);

    /* More sanity checks */

    if ((devinfo->fbmem == 0) || (devinfo->iomem == 0) ||
        (devinfo->fbsize == 0) || (devinfo->iosize == 0))
        return 0;

    return 1;
}

/**
 * Probe the PCI bus for the first matching device.
 *
 * @param vendor    Vendor ID
 * @param device    Device ID
 * @param name      Device name, used for error messages.
 *                  Example: "UniChrome graphics controller"
 * @param devinfo   Output.
 *
 * @returns DFB_OK or DFB_FAILURE
 */

DFBResult uc_probe_vga_device(u16 vendor, u16 device, char* name,
                        struct uc_vga_device* devinfo)
{
    char fbuf[256];
    FILE* file;
    char* s;
    int r;
    int n;

    memset(devinfo, 0, sizeof(struct uc_vga_device));

    file = fopen("/proc/bus/pci/devices", "r");
    if (!file) {
        D_ERROR("Can not open /proc/bus/pci/devices. "
            "Will not probe for %s.\n", name);
        return DFB_FAILURE;
    }

    n = 0;

    while (1)
    {
        errno = 0;
        s = fgets(fbuf, 256, file);
        if (s == NULL) {
            if (errno > 0) {
                D_ERROR("Error reading /proc/bus/pci/devices. "
                    "Probing for %s failed.\n", name);
            }
            fclose(file);
            return DFB_FAILURE;
        }
        s[255] = 0;
        n++;

        r = uc_parse_vga_device(s, devinfo);

        if (r == 1) {
            if (devinfo->devid == ((vendor << 16) | device)) {
                fclose(file);
                return DFB_OK;
            }
        }
        else if (r == -1) {
            D_ERROR("Error parsing /proc/bus/pci/devices on line %d. "
                "Probing for %s failed.\n", n, name);
            fclose(file);
            return DFB_FAILURE;
        }
    }
}

/**
 * Mmap the IO memory area of a VGA device.
 *
 * @param devinfo   result from probing function
 * @param name      Device name, used for error messges.
 *
 * @returns DFB_OK or DFB_FAILURE
 */

DFBResult uc_mmap_vga_device(struct uc_vga_device* devinfo, char* name)
{
    int fd;
    void* result;

    fd = open("/dev/mem", O_RDWR);
    if (fd < 0) {
        D_ERROR("Cannot open /dev/mem: %s.\n", strerror(errno));
        return DFB_FAILURE;
    }

    result = mmap(NULL, devinfo->iosize, PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, devinfo->iomem);

    close(fd);  
    
    if (result == MAP_FAILED) {
        D_ERROR("Cannot mmap %s hardware registers. "
            "Initialization failed.\n", name);
        return DFB_FAILURE;
    }

    devinfo->iomap = result;
    return DFB_OK;
}

void uc_munmap_vga_device(struct uc_vga_device* devinfo)
{
    if (devinfo->iomap) munmap(devinfo->iomap, devinfo->iosize);
    devinfo->iomap = NULL;
}

/* Usage example / test program */

/*

int main()
{
    char* name = "CLE266 VGA adapter";

    int r;
    struct uc_vga_device devinfo;

    r = uc_probe_vga_device(0x1106, 0x3122, name, &devinfo);

    if (r == DFB_OK) {
        printf("%s found at %02x:%02x.%x. Using IRQ %d.\n",
            name,
            devinfo.busid >> 8, (devinfo.busid >> 3) & 0x1f,
            devinfo.busid & 3, devinfo.irq);
        printf("FB: %dMB @ 0x%x  ", devinfo.fbsize >> 20, devinfo.fbmem);
        printf("MMIO: %dKB @ 0x%x\n", devinfo.iosize >> 10, devinfo.iomem);

        r = uc_mmap_vga_device(&devinfo, name);
        if (r == DFB_OK) {
            printf("IO area mmapped to 0x%x.\n", (u32) devinfo.iomap);
        }
    }

    uc_munmap_vga_device(&devinfo);
    return 0;
}

*/
