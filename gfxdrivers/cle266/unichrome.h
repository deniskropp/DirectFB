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
#include <core/layer_control.h>

#include <directfb.h>

#define UNICHROME_DEVICE "/dev/cle266vgaio"
#define UC_FIFO_SIZE 4096

/** If defined - the driver will use the 3D engine. */
#define UC_ENABLE_3D
//#undef UC_ENABLE_3D


/** Register settings for the current source surface. (3D) */
struct uc_hw_texture {
     DFBSurfaceBlittingFlags bltflags;

     u32 l2w;        //width, rounded up to nearest 2^m, eg 600 => 1024
     u32 l2h;        //height, rounded up, e.g 480 => 512
     u32 we;         //width exponent, i.e m in the number 2^m
     u32 he;         //height exponent

     u32 format;     // HW pixel format

     // 3d engine texture environment, texture unit 0

     // Used for the DSBLIT_BLEND_ALPHACHANNEL, DSBLIT_BLEND_COLORALPHA
     // and DSBLIT_COLORIZE blitting flags.

     u32 regHTXnTB;
     u32 regHTXnMPMD;

     u32 regHTXnTBLCsat_0;
     u32 regHTXnTBLCop_0;
     u32 regHTXnTBLMPfog_0;
     u32 regHTXnTBLAsat_0;
     u32 regHTXnTBLRCb_0;
     u32 regHTXnTBLRAa_0;
     u32 regHTXnTBLRFog_0;
};


/** Hardware source-destination blending registers. */
struct uc_hw_alpha {
/*
    u32 regHABBasL;         // Alpha buffer, low 24 bits.
    u32 regHABBasH;         // Alpha buffer, high 8 bits.
    u32 regHABFM;           // Alpha pixel format, memory type and pitch.
    u32 regHATMD;           // Alpha test function and reference value.

    // Blending function
*/
     u32 regHABLCsat;
     u32 regHABLCop;
     u32 regHABLAsat;
     u32 regHABLAop;
     u32 regHABLRCa;
     u32 regHABLRFCa;
     u32 regHABLRCbias;
     u32 regHABLRCb;
     u32 regHABLRFCb;
     u32 regHABLRAa;
     u32 regHABLRAb;
};

typedef enum {
     uc_source2d    = 0x00000001,
     uc_source3d    = 0x00000002,
     uc_texenv      = 0x00000004,
     uc_blending_fn = 0x00000008,
     uc_color2d     = 0x00000010,
     uc_colorkey2d  = 0x00000020
} UcStateBits;

#define UC_VALIDATE(b)       (ucdev->valid |= (b))
#define UC_INVALIDATE(b)     (ucdev->valid &= ~(b))
#define UC_IS_VALID(b)       (ucdev->valid & (b))

typedef struct _UcDeviceData {

     /* State validation */
     UcStateBits valid;

     /* Current state settings */
     u32                     pitch;      // combined src/dst pitch (2D)
     u32                     color;      // 2D fill color
     u32                     color3d;    // color for 3D operations
     u32                     draw_rop2d; // logical drawing ROP (2D)
     u32                     draw_rop3d; // logical drawing ROP (3D)

     DFBSurfaceBlittingFlags bflags;     // blitting flags
     DFBRegion               clip;       // clipping region

     DFBSurfacePixelFormat   dst_format; // destination pixel format
     int                     dst_offset; // destination buffer byte offset
     int                     dst_pitch;  // destination buffer byte pitch

     int                     field;      // source field

     /* Hardware settings */
     struct uc_hw_alpha      hwalpha;    // alpha blending setting (3D)
     struct uc_hw_texture    hwtex;      // hardware settings for blitting (3D)


     /// Set directly after a 2D/3D engine command is sent.
     int must_wait;
     unsigned int cmd_waitcycles;
     unsigned int idle_waitcycles;

     u32             vq_start;   // VQ related
     u32             vq_size;
     u32             vq_end;

} UcDeviceData;


typedef struct _UcDriverData {
     int             file;       // File handle to mmapped IO region.
     int             hwrev;      // Hardware revision
     volatile void*  hwregs;     // Hardware register base
     struct uc_fifo* fifo;       // Data FIFO.
     FusionSHMPoolShared *pool;
} UcDriverData;


#endif // __UNICHROME_H__
