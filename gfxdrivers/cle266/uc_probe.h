/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef __UC_PROBE_H__
#define __UC_PROBE_H__

struct uc_vga_device 
{
    __u32 busid;    /* bus<8>:dev<5>:sub<3> */
    __u32 devid;    /* mfr<16>:device<16> */
    __u32 irq;      /* IRQ number */

    __u32 fbmem;    /* Framebuffer base address */
    __u32 fbsize;   /* Framebuffer size in bytes */
    void* fbmap;    /* Mmapped frambuffer address */

    __u32 iomem;    /* IO area base address */
    __u32 iosize;   /* IO area size in bytes */
    void* iomap;    /* Mmapped IO area address */
};

int uc_parse_vga_device(char* s, struct uc_vga_device* devinfo);
DFBResult uc_probe_vga_device(__u16 vendor, __u16 device, char* name,
                        struct uc_vga_device* devinfo);
DFBResult uc_mmap_vga_device(struct uc_vga_device* devinfo, char* name);
void uc_munmap_vga_device(struct uc_vga_device* devinfo);

#endif /* __UC_PROBE_H__ */
