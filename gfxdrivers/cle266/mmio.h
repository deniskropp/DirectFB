/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef _VIA_MMIO_H
#define _VIA_MMIO_H

#define TRACE_ENTER() printf("Entering %s\n", __PRETTY_FUNCTION__)
#define TRACE_LEAVE() printf("Leaving %s\n", __PRETTY_FUNCTION__)

#ifdef KERNEL

#define VIA_OUT(hwregs, reg, val) *(volatile u32 *)((hwregs) + (reg)) = (val)
#define VIA_IN(hwregs, reg)       *(volatile u32 *)((hwregs) + (reg))
#define VGA_OUT8(hwregs, reg, val) *(volatile u8 *)((hwregs) + (reg) + 0x8000) = (val)
#define VGA_IN8(hwregs, reg)       *(volatile u8 *)((hwregs) + (reg) + 0x8000)
#define RS16(val)               ((u16)((s16)(val)))
#define RS12(val)               (((u16)((s16)(val))) & 0xfff)


#else // !KERNEL

#define VIA_OUT(hwregs, reg, val)   *(volatile __u32 *)((hwregs) + (reg)) = (val)
#define VIA_IN(hwregs, reg)         *(volatile __u32 *)((hwregs) + (reg))
#define VGA_OUT8(hwregs, reg, val)  *(volatile __u8 *)((hwregs) + (reg) + 0x8000) = (val)
#define VGA_IN8(hwregs, reg)        *(volatile __u8 *)((hwregs) + (reg) + 0x8000)

#define RS16(val)               ((__u16)((__s16)(val)))
#define RS12(val)               (((__u16)((__s16)(val))) & 0xfff)

#endif // KERNEL

#define VIDEO_OUT(hwregs, reg, val) VIA_OUT((hwregs)+0x200, reg, val)
#define VIDEO_IN(hwregs, reg)       VIA_IN((hwregs)+0x200, reg)

#define MAXLOOP                 0xffffff

#endif /* _VIA_MMIO_H */
