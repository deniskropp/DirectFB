/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef __UC_STATE__
#define __UC_STATE__

#include <directfb.h>
#include <core/state.h>
#include <core/gfxcard.h>

void uc_set_state(void *drv, void *dev, GraphicsDeviceFuncs *funcs,
                  CardState *state, DFBAccelerationMask accel);
void uc_check_state(void *drv, void *dev,
                    CardState *state, DFBAccelerationMask accel);



/*
struct uc_hw_misc
{
    // These control clipping...

    __u32 regHClipTB;
    __u32 regHClipLR;
    __u32 regHFPClipTL;
    __u32 regHFPClipBL;
    __u32 regHFPClipLL;
    __u32 regHFPClipRL;
    __u32 regHFPClipTBH;
    __u32 regHFPClipLRH;

    // Other functions

    __u32 regHLP;           // Line stipple pattern
    __u32 regHLPRF;         // Line stipple factor
    __u32 regHSolidCL;      // --- Don't know. Unused in DRI.
    __u32 regHPixGC;        // Don't know. Is kept cleared in DRI.
    //__u32 regHSPXYOS;     // Polygon stipple x and y offsets. Unused here.
    __u32 regHVertexCNT;    // --- Don't know. Unused in DRI.

    __u8 ps_xos;            // Polygon stipple x-offset. => regHSPXYOS
    __u8 ps_yos;            // Polygon stipple y-offset. => regHSPXYOS
    __u32 ps_pat[32];       // Polygon stipple pattern buffer.
                            // These are not registers...
};


/// Stencil control.

struct uc_hw_stencil
{
    //__u32 regHSBBasL;     // These aren't in regs3d.h, but they should exist...
    //__u32 regHSBBasH;
    //__u32 regHSBFM;

    __u32 regHSTREF;        // Stencil reference value and plane mask
    __u32 regHSTMD;         // Stencil test function and fail operation and
                            // zpass/zfail operations.
};
*/

#endif // __UC_STATE__
