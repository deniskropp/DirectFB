// Note: This is a modified version of via_regs.h from the XFree86 CVS tree.

/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __VIA_REGS_2D_H__
#define __VIA_REGS_2D_H__

/* Selected 2D engine raster operations.
 * See xc/programs/Xserver/hw/xfree86/xaa/xaarop.h
 * in the XFree86 project for the full list.
 */
#define VIA_ROP_DPx             (0x5A << 24)
#define VIA_ROP_DSx             (0x66 << 24)
#define VIA_ROP_S               (0xCC << 24)
#define VIA_ROP_P			    (0xF0 << 24)

/* My own reverse-engineered bit definitions */

// Use the following definitions with VIA_KEY_CONTROL

/// When set, red channel is not drawn
#define VIA_KEY_MASK_RED		0x40000000
/// When set, green channel is not drawn
#define VIA_KEY_MASK_GREEN		0x20000000
/// When set, blue channel is not drawn
#define VIA_KEY_MASK_BLUE		0x10000000

/** When set, destination keying is enabled.
 * Caveat: VIA's destination key is the opposite of DirectFB's:
 * It draws where there is no match in the destination surface.
 */
#define VIA_KEY_ENABLE_DSTKEY	0x8000
/** When set, source keying is enabled
 * It draws the pixels in the source that do not match the color key.
 */
#define VIA_KEY_ENABLE_SRCKEY	0x4000
/** Inverts the behaviour of the color keys:
 * Dst key: draw where the destination matches the key
 * Src key: draw where the source matches the key
 * Problem: Since this bit affects both keys, you can not do
 * combined source and destination keying with DirectFB.
 * The inverted source key is all but useless since it will
 * only draw the source pixels that match the key! 
 * It must be a design error...
 */
#define VIA_KEY_INVERT_KEY		0x2000

/* 2D engine registers and bit definitions */

#define VIA_MMIO_REGSIZE        0x9000
#define VIA_MMIO_REGBASE        0x0
#define VIA_MMIO_VGABASE        0x8000
#define VIA_MMIO_BLTBASE        0x200000
#define VIA_MMIO_BLTSIZE        0x10000

#define VIA_VQ_SIZE             (256*1024)

/* defines for VIA 2D registers */
#define VIA_REG_GECMD           0x000
#define VIA_REG_GEMODE          0x004
#define VIA_REG_GESTATUS        0x004       /* as same as VIA_REG_GEMODE */
#define VIA_REG_SRCPOS          0x008
#define VIA_REG_DSTPOS          0x00C
#define VIA_REG_LINE_K1K2       0x008
#define VIA_REG_LINE_XY         0x00C
#define VIA_REG_DIMENSION       0x010       /* width and height */
#define VIA_REG_PATADDR         0x014
#define VIA_REG_FGCOLOR         0x018
#define VIA_REG_DSTCOLORKEY     0x018       /* as same as VIA_REG_FG */
#define VIA_REG_BGCOLOR         0x01C
#define VIA_REG_SRCCOLORKEY     0x01C       /* as same as VIA_REG_BG */
#define VIA_REG_CLIPTL          0x020       /* top and left of clipping */
#define VIA_REG_CLIPBR          0x024       /* bottom and right of clipping */
#define VIA_REG_OFFSET          0x028
#define VIA_REG_LINE_ERROR      0x028
#define VIA_REG_KEYCONTROL      0x02C       /* color key control */
#define VIA_REG_SRCBASE         0x030
#define VIA_REG_DSTBASE         0x034
#define VIA_REG_PITCH           0x038       /* pitch of src and dst */
#define VIA_REG_MONOPAT0        0x03C
#define VIA_REG_MONOPAT1        0x040
#define VIA_REG_COLORPAT        0x100       /* from 0x100 to 0x1ff */


/* defines for VIA video registers */
#define VIA_REG_INTERRUPT       0x200
#define VIA_REG_CRTCSTART       0x214


/* defines for VIA HW cursor registers */
#define VIA_REG_CURSOR_MODE     0x2D0
#define VIA_REG_CURSOR_POS      0x2D4
#define VIA_REG_CURSOR_ORG      0x2D8
#define VIA_REG_CURSOR_BG       0x2DC
#define VIA_REG_CURSOR_FG       0x2E0


/* defines for VIA 3D registers */
#define VIA_REG_STATUS          0x400
#define VIA_REG_TRANSET         0x43C
#define VIA_REG_TRANSPACE       0x440

/* VIA_REG_STATUS(0x400): Engine Status */
#define VIA_CMD_RGTR_BUSY       0x00000080  /* Command Regulator is busy */
#define VIA_2D_ENG_BUSY         0x00000001  /* 2D Engine is busy */
#define VIA_3D_ENG_BUSY         0x00000002  /* 3D Engine is busy */
#define VIA_VR_QUEUE_BUSY       0x00020000 /* Virtual Queue is busy */


/* VIA_REG_GECMD(0x00): 2D Engine Command  */
#define VIA_GEC_NOOP            0x00000000
#define VIA_GEC_BLT             0x00000001
#define VIA_GEC_LINE            0x00000005

#define VIA_GEC_SRC_XY          0x00000000
#define VIA_GEC_SRC_LINEAR      0x00000010
#define VIA_GEC_DST_XY          0x00000000
#define VIA_GEC_DST_LINRAT      0x00000020

#define VIA_GEC_SRC_FB          0x00000000
#define VIA_GEC_SRC_SYS         0x00000040
#define VIA_GEC_DST_FB          0x00000000
#define VIA_GEC_DST_SYS         0x00000080

#define VIA_GEC_SRC_MONO        0x00000100  /* source is mono */
#define VIA_GEC_PAT_MONO        0x00000200  /* pattern is mono */

#define VIA_GEC_MSRC_OPAQUE     0x00000000  /* mono src is opaque */
#define VIA_GEC_MSRC_TRANS      0x00000400  /* mono src is transparent */

#define VIA_GEC_PAT_FB          0x00000000  /* pattern is in frame buffer */
#define VIA_GEC_PAT_REG         0x00000800  /* pattern is from reg setting */

#define VIA_GEC_CLIP_DISABLE    0x00000000
#define VIA_GEC_CLIP_ENABLE     0x00001000

#define VIA_GEC_FIXCOLOR_PAT    0x00002000

#define VIA_GEC_INCX            0x00000000
#define VIA_GEC_DECY            0x00004000
#define VIA_GEC_INCY            0x00000000
#define VIA_GEC_DECX            0x00008000

#define VIA_GEC_MPAT_OPAQUE     0x00000000  /* mono pattern is opaque */
#define VIA_GEC_MPAT_TRANS      0x00010000  /* mono pattern is transparent */

#define VIA_GEC_MONO_UNPACK     0x00000000
#define VIA_GEC_MONO_PACK       0x00020000
#define VIA_GEC_MONO_DWORD      0x00000000
#define VIA_GEC_MONO_WORD       0x00040000
#define VIA_GEC_MONO_BYTE       0x00080000

#define VIA_GEC_LASTPIXEL_ON    0x00000000
#define VIA_GEC_LASTPIXEL_OFF   0x00100000
#define VIA_GEC_X_MAJOR         0x00000000
#define VIA_GEC_Y_MAJOR         0x00200000
#define VIA_GEC_QUICK_START     0x00800000


/* VIA_REG_GEMODE(0x04): GE mode */
#define VIA_GEM_8bpp            0x00000000
#define VIA_GEM_16bpp           0x00000100
#define VIA_GEM_32bpp           0x00000300

#define VIA_GEM_640             0x00000000   /* 640*480 */
#define VIA_GEM_800             0x00000400   /* 800*600 */
#define VIA_GEM_1024            0x00000800   /* 1024*768 */
#define VIA_GEM_1280            0x00000C00   /* 1280*1024 */
#define VIA_GEM_1600            0x00001000   /* 1600*1200 */
#define VIA_GEM_2048            0x00001400   /* 2048*1536 */

/* VIA_REG_PITCH(0x38): Pitch Setting */
#define VIA_PITCH_ENABLE        0x80000000

#endif // __VIA_REGS_2D_H__
