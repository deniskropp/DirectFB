/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#ifndef __UNICHROME_H__
#define __UNICHROME_H__

#include <core/coredefs.h>
#include <core/surfaces.h>
#include <core/layers.h>
#include <directfb.h>

#define UNICHROME_DEVICE "/dev/cle266vgaio"
#define UC_FIFO_SIZE 2000

/** If defined - the driver will use the 3D engine. */
#define UC_ENABLE_3D
//#undef UC_ENABLE_3D


/** Register settings for the current source surface. (3D) */
struct uc_hw_texture
{
    DFBSurfaceBlittingFlags bltflags;
    CoreSurface* surface;

    __u32 l2w;      //width, rounded up to nearest 2^m, eg 600 => 1024
    __u32 l2h;      //height, rounded up, e.g 480 => 512
    __u32 we;       //width exponent, i.e m in the number 2^m
    __u32 he;       //height exponent

    __u32 format;   // HW pixel format

    // 3d engine texture environment, texture unit 0

    // Used for the DSBLIT_BLEND_ALPHACHANNEL, DSBLIT_BLEND_COLORALPHA
    // and DSBLIT_COLORIZE blitting flags.

    __u32 regHTXnTB;
    __u32 regHTXnMPMD;

    __u32 regHTXnTBLCsat_0;
    __u32 regHTXnTBLCop_0;
    __u32 regHTXnTBLMPfog_0;
    __u32 regHTXnTBLAsat_0;
    __u32 regHTXnTBLRCb_0;
    __u32 regHTXnTBLRAa_0;
    __u32 regHTXnTBLRFog_0;
};


/** Hardware source-destination blending registers. */
struct uc_hw_alpha
{
/*
    __u32 regHABBasL;       // Alpha buffer, low 24 bits.
    __u32 regHABBasH;       // Alpha buffer, high 8 bits.
    __u32 regHABFM;         // Alpha pixel format, memory type and pitch.
    __u32 regHATMD;         // Alpha test function and reference value.

    // Blending function
*/
    __u32 regHABLCsat;
    __u32 regHABLCop;
    __u32 regHABLAsat;
    __u32 regHABLAop;
    __u32 regHABLRCa;
    __u32 regHABLRFCa;
    __u32 regHABLRCbias;
    __u32 regHABLRCb;
    __u32 regHABLRFCb;
    __u32 regHABLRAa;
    __u32 regHABLRAb;
};

typedef struct _UcDeviceData {

    __u32 pitch;                    // Current combined src/dst pitch (2D)
    __u32 color;                    // Current fill color
    __u32 color3d;                  // Current color for 3D operations
    __u32 colormask;                // Current color mask
    __u32 alphamask;                // Current alpha mask
    __u32 draw_rop2d;               // Current logical drawing ROP (2D)
    __u32 draw_rop3d;               // Current logical drawing ROP (3D)
    struct uc_hw_alpha hwalpha;     // Current alpha blending setting (3D)

    // Entries related to the current source surface.

    DFBSurfaceBlittingFlags bflags; // Current blitting flags
    struct uc_hw_texture hwtex;     // Current hardware settings for blitting (3D)
    
    /// Set directly after a 2D/3D engine command is sent.
    int must_wait;
    unsigned int cmd_waitcycles;
    unsigned int idle_waitcycles;


    __u32           vq_start;   // VQ related
    __u32           vq_size;
    __u32           vq_end;

    /*state validation*/
    int v_source2d;
    int v_source3d;
    int v_texenv;
    int v_blending_fn;

} UcDeviceData;


typedef struct _UcDriverData {
    int             file;       // File handle to mmapped IO region.
    int             hwrev;      // Hardware revision
    __u8*           hwregs;     // Hardware register base
    struct uc_fifo* fifo;       // Data FIFO.

    __u8            vga1A_save;

} UcDriverData;


#endif // __UNICHROME_H__
