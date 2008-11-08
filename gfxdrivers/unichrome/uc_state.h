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

    u32 regHClipTB;
    u32 regHClipLR;
    u32 regHFPClipTL;
    u32 regHFPClipBL;
    u32 regHFPClipLL;
    u32 regHFPClipRL;
    u32 regHFPClipTBH;
    u32 regHFPClipLRH;

    // Other functions

    u32 regHLP;             // Line stipple pattern
    u32 regHLPRF;           // Line stipple factor
    u32 regHSolidCL;        // --- Don't know. Unused in DRI.
    u32 regHPixGC;          // Don't know. Is kept cleared in DRI.
    //u32 regHSPXYOS;       // Polygon stipple x and y offsets. Unused here.
    u32 regHVertexCNT;      // --- Don't know. Unused in DRI.

    u8 ps_xos;              // Polygon stipple x-offset. => regHSPXYOS
    u8 ps_yos;              // Polygon stipple y-offset. => regHSPXYOS
    u32 ps_pat[32];         // Polygon stipple pattern buffer.
                            // These are not registers...
};


/// Stencil control.

struct uc_hw_stencil
{
    //u32 regHSBBasL;       // These aren't in regs3d.h, but they should exist...
    //u32 regHSBBasH;
    //u32 regHSBFM;

    u32 regHSTREF;          // Stencil reference value and plane mask
    u32 regHSTMD;           // Stencil test function and fail operation and
                            // zpass/zfail operations.
};
*/

#endif // __UC_STATE__
