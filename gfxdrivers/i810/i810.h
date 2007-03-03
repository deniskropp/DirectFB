/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Antonino Daplas <adaplas@pol.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

/*
 * Intel 810 Chipset Family PRM 15 3.1 
 * GC Register Memory Address Map 
 *
 * Based on:
 * Intel (R) 810 Chipset Family 
 * Programmer s Reference Manual 
 * November 1999 
 * Revision 1.0 
 * Order Number: 298026-001 R
 *
 * All GC registers are memory-mapped. In addition, the VGA and extended VGA registers 
 * are I/O mapped. 
 */
 
#ifndef __I810_H__
#define __I810_H__

#include <dfb_types.h>
#include <sys/io.h>
#include <linux/agpgart.h>

#include <core/gfxcard.h>
#include <core/layers.h>


#define LP_RING     0x2030
#define HP_RING     0x2040

#define RING_TAIL      0x00
#define RING_HEAD      0x04
#define RING_START     0x08
#define RING_LEN       0x0C


/*  Instruction and Interrupt Control Registers (01000h 02FFFh) */
#define FENCE                 0x02000                
#define PGTBL_CTL             0x02020                
#define PGTBL_ER              0x02024
#define RINGBUFFER            0x02030
#define    LRING              0x02030
#define    IRING              0x02040
#define HWS_PGA               0x02080 
#define IPEIR                 0x02088
#define IPEHR                 0x0208C 
#define INSTDONE              0x02090 
#define NOPID                 0x02094
#define HWSTAM                0x02098 
#define IER                   0x020A0
#define IIR                   0x020A4
#define IMR                   0x020A8 
#define ISR                   0x020AC 
#define EIR                   0x020B0 
#define EMR                   0x020B4 
#define ESR                   0x020B8 
#define INSTPM                0x020C0
#define INSTPS                0x020C4 
#define BBP_PTR               0x020C8 
#define ABB_SRT               0x020CC
#define ABB_END               0x020D0
#define DMA_FADD              0x020D4 
#define FW_BLC                0x020D8
#define MEM_MODE              0x020DC        

/*  Memory Control Registers (03000h 03FFFh) */
#define DRT                   0x03000
#define DRAMCL                0x03001
#define DRAMCH                0x03002
 

/* Span Cursor Registers (04000h 04FFFh) */
#define UI_SC_CTL             0x04008 

/* I/O Control Registers (05000h 05FFFh) */
#define HVSYNC                0x05000 
#define GPIOA                 0x05010
#define GPIOB                 0x05014 

/* Clock Control and Power Management Registers (06000h 06FFFh) */
#define DCLK_0D               0x06000
#define DCLK_1D               0x06004
#define DCLK_2D               0x06008
#define LCD_CLKD              0x0600C
#define DCLK_0DS              0x06010
#define PWR_CLKC              0x06014

/* Graphics Translation Table Range Definition (10000h 1FFFFh) */
#define GTT                   0x10000  

/*  Overlay Registers (30000h 03FFFFh) */
#define OV0ADDR               0x30000
#define DOV0STA               0x30008
#define GAMMA                 0x30010
#define OBUF_0Y               0x30100
#define OBUF_1Y               0x30104
#define OBUF_0U               0x30108
#define OBUF_0V               0x3010C
#define OBUF_1U               0x30110
#define OBUF_1V               0x30114 
#define OV0STRIDE             0x30118
#define YRGB_VPH              0x3011C
#define UV_VPH                0x30120
#define HORZ_PH               0x30124
#define INIT_PH               0x30128
#define DWINPOS               0x3012C 
#define DWINSZ                0x30130
#define SWID                  0x30134
#define SWIDQW                0x30138
#define SHEIGHT               0x3013C
#define YRGBSCALE             0x30140 
#define UVSCALE               0x30144
#define OV0CLRCO              0x30148
#define OV0CLRC1              0x3014C
#define DCLRKV                0x30150
#define DLCRKM                0x30154
#define SCLRKVH               0x30158
#define SCLRKVL               0x3015C
#define SCLRKM                0x30160
#define OV0CONF               0x30164
#define OV0CMD                0x30168
#define AWINPOS               0x30170
#define AWINZ                 0x30174

/*  BLT Engine Status (40000h 4FFFFh) (Software Debug) */
#define BR00                  0x40000
#define BRO1                  0x40004
#define BR02                  0x40008
#define BR03                  0x4000C
#define BR04                  0x40010
#define BR05                  0x40014
#define BR06                  0x40018
#define BR07                  0x4001C
#define BR08                  0x40020
#define BR09                  0x40024
#define BR10                  0x40028
#define BR11                  0x4002C
#define BR12                  0x40030
#define BR13                  0x40034
#define BR14                  0x40038
#define BR15                  0x4003C
#define BR16                  0x40040
#define BR17                  0x40044
#define BR18                  0x40048
#define BR19                  0x4004C
#define SSLADD                0x40074
#define DSLH                  0x40078
#define DSLRADD               0x4007C


/* LCD/TV-Out and HW DVD Registers (60000h 6FFFFh) */
/* LCD/TV-Out */
#define HTOTAL                0x60000
#define HBLANK                0x60004
#define HSYNC                 0x60008
#define VTOTAL                0x6000C
#define VBLANK                0x60010
#define VSYNC                 0x60014
#define LCDTV_C               0x60018
#define OVRACT                0x6001C
#define BCLRPAT               0x60020

/*  Display and Cursor Control Registers (70000h 7FFFFh) */
#define DISP_SL               0x70000
#define DISP_SLC              0x70004
#define PIXCONF               0x70008
#define PIXCONF1              0x70009
#define BLTCNTL               0x7000C
#define SWF                   0x70014
#define DPLYBASE              0x70020
#define DPLYSTAS              0x70024
#define CURCNTR               0x70080
#define CURBASE               0x70084
#define CURPOS                0x70088


/* VGA Registers */

/* SMRAM Registers */
#define SMRAM                 0x10

/* Graphics Control Registers */
#define GR_INDEX              0x3CE
#define GR_DATA               0x3CF

#define GR10                  0x10
#define GR11                  0x11

/* CRT Controller Registers */
#define CR_INDEX_MDA          0x3B4
#define CR_INDEX_CGA          0x3D4
#define CR_DATA_MDA           0x3B5
#define CR_DATA_CGA           0x3D5

#define CR30                  0x30
#define CR31                  0x31
#define CR32                  0x32
#define CR33                  0x33
#define CR35                  0x35
#define CR39                  0x39
#define CR40                  0x40
#define CR41                  0x41
#define CR42                  0x42
#define CR70                  0x70
#define CR80                  0x80 
#define CR81                  0x82

/* Extended VGA Registers */

/* General Control and Status Registers */
#define ST00                  0x3C2
#define ST01_MDA              0x3BA
#define ST01_CGA              0x3DA
#define FRC_READ              0x3CA
#define FRC_WRITE_MDA         0x3BA
#define FRC_WRITE_CGA         0x3DA
#define MSR_READ              0x3CC
#define MSR_WRITE             0x3C2

/* Sequencer Registers */
#define SR_INDEX              0x3C4
#define SR_DATA               0x3C5

#define SR01                  0x01
#define SR02                  0x02
#define SR03                  0x03
#define SR04                  0x04
#define SR07                  0x07

/* Graphics Controller Registers */
#define GR00                  0x00   
#define GR01                  0x01
#define GR02                  0x02
#define GR03                  0x03
#define GR04                  0x04
#define GR05                  0x05
#define GR06                  0x06
#define GR07                  0x07
#define GR08                  0x08  

/* Attribute Controller Registers */
#define ATTR_WRITE              0x3C0
#define ATTR_READ               0x3C1

/* VGA Color Palette Registers */

/* CLUT */
#define CLUT_DATA             0x3C9        /* DACDATA */
#define CLUT_INDEX_READ       0x3C7        /* DACRX */
#define CLUT_INDEX_WRITE      0x3C8        /* DACWX */
#define DACMASK               0x3C6

/* CRT Controller Registers */
#define CR00                  0x00
#define CR01                  0x01
#define CR02                  0x02
#define CR03                  0x03
#define CR04                  0x04
#define CR05                  0x05
#define CR06                  0x06
#define CR07                  0x07
#define CR08                  0x08
#define CR09                  0x09
#define CR0A                  0x0A
#define CR0B                  0x0B
#define CR0C                  0x0C
#define CR0D                  0x0D
#define CR0E                  0x0E
#define CR0F                  0x0F
#define CR10                  0x10
#define CR11                  0x11
#define CR12                  0x12
#define CR13                  0x13
#define CR14                  0x14
#define CR15                  0x15
#define CR16                  0x16
#define CR17                  0x17
#define CR18                  0x18


/* Raster ops */
#define COLOR_COPY_ROP        0xF0
#define PAT_COPY_ROP          0xCC
#define CLEAR_ROP             0x00
#define WHITE_ROP             0xFF
#define INVERT_ROP            0x55

/* 2D Engine definitions */
#define SOLIDPATTERN          0x80000000
#define NONSOLID              0x00000000
#define BPP8                  0x00000000
#define BPP16                 0x01 << 24
#define BPP24                 0x02 << 24
#define DYN_COLOR_EN          0x00400000
#define DYN_COLOR_DIS         0x00000000
#define INCREMENT             0x00000000
#define DECREMENT             0x01 << 30
#define ARB_ON                0x00000001
#define ARB_OFF               0x00000000
#define SYNC_FLIP             0x00000000
#define ASYNC_FLIP            0x00000040
#define OPTYPE_MASK           0xE0000000
#define PARSER_MASK           0x001F8000 
#define D2_MASK               0x001FC000         /* 2D mask */

/* Instruction type */
/* There are more but pertains to 3D */
#define PARSER                0x00000000
#define BLIT                  0x02 << 29
#define RENDER                0x03 << 29
            
/* Parser */
#define NOP                   0x00               /* No operation, padding */
#define BP_INT                0x01 << 23         /* Breakpoint interrupt */
#define USR_INT               0x02 << 23         /* User interrupt */
#define WAIT_FOR_EVNT         0x03 << 23         /* Wait for event */
#define FLUSH                 0x04 << 23              
#define CONTEXT_SEL           0x05 << 23
#define REPORT_HEAD           0x07 << 23
#define ARB_ON_OFF            0x08 << 23
#define OVERLAY_FLIP          0x11 << 23
#define LOAD_SCAN_INC         0x12 << 23
#define LOAD_SCAN_EX          0x13 << 23
#define FRONT_BUFFER          0x14 << 23
#define DEST_BUFFER           0x15 << 23
#define Z_BUFFER              0x16 << 23              /* we won't need this */
#define STORE_DWORD_IMM       0x20 << 23
#define STORE_DWORD_IDX       0x21 << 23
#define BATCH_BUFFER          0x30 << 23

/* Blit */
#define SETUP_BLIT                      0x00
#define SETUP_MONO_PATTERN_SL_BLT       0x10 << 22
#define PIXEL_BLT                       0x20 << 22
#define SCANLINE_BLT                    0x21 << 22 
#define TEXT_BLT                        0x22 << 22
#define TEXT_IMM_BLT                    0x30 << 22
#define COLOR_BLT                       0x40 << 22
#define MONO_PAT_BLIT                   0x42 << 22
#define SOURCE_COPY_BLIT                0x43 << 22
#define FULL_BLIT                       0x45 << 22

/* Primitive */
#define TRILIST                         0
#define TRISTRIP                        1 << 18
#define TRISTRIP_REV                    2 << 18
#define TRIFAN                          3 << 18
#define POLYGON                         4 << 18
#define LINELIST                        5 << 18
#define LINESTRIP                       6 << 18
#define RECTANGLE                       7 << 18 
#define V0_ENABLE                       1
#define V1_ENABLE                       2
#define V2_ENABLE                       4

/* Vertex Flags */
#define COORD_1                         0
#define COORD_2                         1 << 8
#define COORD_3                         2 << 8
#define FOG_ENABLE                      1 << 7
#define ARGB_ENABLE                     1 << 6
#define Z_OFFSET_PRESENT                1 << 5
#define XYZ                             0x01 << 1
#define XYZW                            0x02 << 1
#define XY                              0x03 << 1
#define XYW                             0x04 << 1               

/* Antialiasing */
#define AA_UPDATE_EDGEFLAG	 (1<<13)
#define AA_ENABLE_EDGEFLAG	 (1<<12)
#define AA_UPDATE_POLYWIDTH      (1<<11)
#define AA_POLYWIDTH_05		 (1<<9)
#define AA_POLYWIDTH_10		 (2<<9)
#define AA_POLYWIDTH_20 	 (3<<9)
#define AA_POLYWIDTH_40          (4<<9)
#define AA_UPDATE_LINEWIDTH	 (1<<8)
#define AA_LINEWIDTH_05  	 (1<<6)
#define AA_LINEWIDTH_10          (2<<6)
#define AA_LINEWIDTH_20 	 (3<<6)
#define AA_LINEWIDTH_40		  (4<<6)
#define AA_UPDATE_BB_EXPANSION   (1<<5)
#define AA_BB_EXPANSION_SHIFT    2
#define AA_UPDATE_AA_ENABLE	 (1<<1)
#define AA_ENABLE    		 (1<<0)

/* Pixelization Rule */
#define PVK_SMALL_TRI_UPDATE            1 << 12          
#define PVK_SMALL_TRI                   1 << 11
#define PVK_PIX_RULE_UPDATE             1 << 10 
#define PVK_PIX_RULE                    1 << 9
#define PVK_LINE_UPDATE                 1 << 8
#define PVK_LINE_V0                     0 
#define PVK_LINE_V1                     1 << 6
#define PVK_TRIFAN_UPDATE               1 << 5
#define PVK_TRIFAN_V0                   0
#define PVK_TRIFAN_V1                   1 << 3
#define PVK_TRIFAN_V2                   2 << 3
#define PVK_TRISTRIP_UPDATE             1 << 2
#define PVK_TRISTRIP_V0                 0
#define PVK_TRISTRIP_V1                 1 
#define PVK_TRISTRIP_V2                 2 

/* Boolean Enable 1 */
#define B1_ALPHA_SETUP_ENABLE_UPDATE    1 << 17
#define B1_ALPHA_SETUP_ENABLE           1 << 16
#define B1_FOG_ENABLE_UPDATE            1 << 7
#define B1_FOG_ENABLE                   1 << 6
#define B1_ALPHA_STATE_ENABLE_UPDATE    1 << 5
#define B1_ALPHA_STATE_ENABLE           1 << 4
#define B1_BLEND_ENABLE_UPDATE          1 << 3
#define B1_BLEND_ENABLE                 1 << 2
#define B1_Z_ENABLE_UPDATE              1 << 1 
#define B1_Z_ENABLE                     1 

/* Boolean Enable 2 */
#define B2_MCE_UPDATE                   1 << 17
#define B2_MCE                          1 << 16
#define B2_ALPHA_DITHER_UPDATE          1 << 15
#define B2_ALPHA_DITHER                 1 << 14
#define B2_FOG_DITHER_UPDATE            1 << 13
#define B2_FOG_DITHER                   1 << 12
#define B2_SPEC_DITHER_UPDATE           1 << 11 
#define B2_SPEC_DITHER                  1 << 10
#define B2_COLOR_DITHER_UPDATE          1 << 9
#define B2_COLOR_DITHER                 1 << 8
#define B2_FB_WRITE_UPDATE              1 << 3
#define B2_FB_WRITE                     1 << 2
#define B2_ZB_WRITE_UPDATE              1 << 1
#define B2_ZB_WRITE                     1

/* Cull Shade Mode */
#define CULL_Z_UPDATE                   1 << 20                 
#define CULL_Z_ALWAYS                   0
#define CULL_Z_NEVER                    1 << 16
#define CULL_Z_LESS                     2 << 16
#define CULL_Z_EQUAL                    3 << 16
#define CULL_Z_LEQUAL                   4 << 16
#define CULL_Z_GREATER                  5 << 16
#define CULL_Z_NOTEQUAL                 6 << 16
#define CULL_Z_GEQUAL                   7 << 16
#define CULL_LINE_WIDTH_UPDATE          1 << 15
#define CULL_LINE_WIDTH_MASK            7 << 12
#define CULL_ALPHA_SHADE_UPDATE         1 << 11
#define CULL_ALPHA_SHADE                1 << 10
#define CULL_FOG_SHADE_UPDATE           1 << 9
#define CULL_FOG_SHADE                  1 << 8
#define CULL_SPEC_SHADE_UPDATE          1 << 7
#define CULL_SPEC_SHADE                 1 << 6
#define CULL_COLOR_SHADE_UPDATE         1 << 5
#define CULL_COLOR_SHADE                1 << 4
#define CULL_MODE_UPDATE                1 << 3
#define CULL_NONE                       1 << 2
#define CULL_CW                         2 << 2 
#define CULL_CCW                        3 << 2
#define CULL_BOTH                       4 << 2

/* texel map */
#define UPDATE_TEXEL1                   1 << 15
#define UPDATE_TEXEL0                   1 << 7
#define ENABLE_TEXEL1                   1 << 14
#define ENABLE_TEXEL0                   1 << 6
#define TEXEL1_COORD_IDX                1 << 11
#define TEXEL0_COORD_IDX                1 << 3
#define TEXEL1_MAP_IDX                  1 << 8
#define TEXEL0_MAP_IDX                  1

/* color blend stage */                 
#define COLOR_STAGE0                    0
#define COLOR_STAGE1                    1 << 20
#define COLOR_STAGE2                    2 << 20
#define UPDATE_COLOR_SELECT_MASK        1 << 19
#define SELECT_COLOR_ACC                1 << 18
#define SELECT_COLOR_CURRENT            0
#define UPDATE_COLOR_ARG1               1 << 17
#define ARG1_COLOR_FACTOR               1 << 14
#define ARG1_COLOR_ACC                  2 << 14
#define ARG1_COLOR_ITERATED             3 << 14
#define ARG1_COLOR_SPEC                 4 << 14
#define ARG1_COLOR_CURRENT              5 << 14
#define ARG1_COLOR_TEXEL0               6 << 14
#define ARG1_COLOR_TEXEL1               7 << 14
#define ARG1_REPLICATE_ALPHA_TO_COLOR   1 << 13
#define ARG1_COLOR_INVERT               1 << 12
#define UPDATE_COLOR_ARG2               1 << 11
#define ARG2_COLOR_FACTOR               1 << 8
#define ARG2_COLOR_ACC                  2 << 8
#define ARG2_COLOR_ITERATED             3 << 8
#define ARG2_COLOR_SPEC                 4 << 8
#define ARG2_COLOR_CURRENT              5 << 8
#define ARG2_COLOR_TEXEL0               6 << 8
#define ARG2_COLOR_TEXEL1               7 << 8
#define ARG2_REPLICATE_ALPHA_TO_COLOR   1 << 7
#define ARG2_COLORINVERT                1 << 6
#define UPDATE_COLOR_OP                 1 << 5
#define DISABLE_COLOR_OP                0
#define SELECT_COLOR_ARG1_OP            1
#define SELECT_COLOR_ARG2_OP            2
#define MODULATE_COLOR_OP               3
#define MODULATE2X_COLOR_OP             4
#define MODULATE4X_COLOR_OP             5
#define ADD_COLOR_OP                    6
#define ADD_SIGNED_COLOR_OP             7
#define LINEAR_ALPHA_ITER_OP            8
#define LINEAR_ALPHA_FACTOR_OP          0x0a
#define LINEAR_TEXEL0_ALPHA_OP          0x10
#define LINEAR_TEXEL1_ALPHA_OP          0x11
#define LINEAR_TEXEL0_COLOR_OP          0x12
#define LINEAR_TEXEL1_COLOR_OP          0x13
#define SUBTRACT_COLOR_OP               0x14

/* alpha blend stage */                 
#define ALPHA_STAGE0                    0
#define ALPHA_STAGE1                    1 << 20
#define ALPHA_STAGE2                    2 << 20
#define UPDATE_ALPHA_SELECT_MASK        1 << 19
#define UPDATE_ALPHA_ARG1               1 << 18
#define ARG1_ALPHA_FACTOR               1 << 15
#define ARG1_ALPHA_ITERATED             3 << 15
#define ARG1_ALPHA_CURRENT              5 << 15
#define ARG1_ALPHA_TEXEL0               6 << 15
#define ARG1_ALPHA_TEXEL1               7 << 15
#define ARG1_ALPHA_INVERT               1 << 13
#define UPDATE_ALPHA_ARG2               1 << 12
#define ARG2_ALPHA_FACTOR               1 << 8
#define ARG2_ALPHA_ITERATED             3 << 8
#define ARG2_ALPHA_CURRENT              5 << 8
#define ARG2_ALPHA_TEXEL0               6 << 8
#define ARG2_ALPHA_TEXEL1               7 << 8
#define ARG2_ALPHAINVERT                1 << 6
#define UPDATE_ALPHA_OP                 1 << 5
#define DISABLE_ALPHA_OP                0
#define SELECT_ALPHA_ARG1_OP            1
#define SELECT_ALPHA_ARG2_OP            2
#define MODULATE_ALPHA_OP               3
#define MODULATE2X_ALPHA_OP             4
#define MODULATE4X_ALPHA_OP             5
#define ADD_ALPHA_OP                    6
#define ADD_SIGNED_ALPHA_OP             7
#define LINEAR_ALPHA_ITER_OP            8
#define LINEAR_ALPHA_FACTOR_OP          0x0a
#define LINEAR_TEXEL0_ALPHA_OP          0x10
#define LINEAR_TEXEL1_ALPHA_OP          0x11

/* Source-Dest Blend Mono */
#define UPDATE_MONO                     1 << 13
#define ENABLE_MONO                     1 << 12
#define DISABLE_MONO                    0
#define UPDATE_SRC_MONO_BLEND           1 << 11
#define UPDATE_DEST_MONO_BLEND          1 << 5

#define SRC_ZERO                        1 >>6
#define SRC_ONE                         2 << 6
#define SRC_SRC_COLOR                   3 << 6
#define SRC_INV_SRC_COLOR               4 << 6
#define SRC_SRC_ALPHA                   5 << 6
#define SRC_INV_SRC_ALPHA               6 << 6
#define SRC_DST_COLOR                   9 << 6
#define SRC_INV_DST_COLOR               0x0a << 6
#define SRC_BOTH_SRC_ALPHA              0x0c << 6
#define SRC_BOTH_INV_SRC_ALPHA          0x0d << 6    

#define DEST_ZERO                       1 
#define DEST_ONE                        2
#define DEST_SRC_COLOR                  3
#define DEST_INV_SRC_COLOR              4
#define DEST_SRC_ALPHA                  5
#define DEST_INV_SRC_ALPHA              6
#define DEST_DST_COLOR                  9
#define DEST_INV_DST_COLOR              0x0a
#define DEST_BOTH_SRC_ALPHA             0x0c
#define DEST_BOTH_INV_SRC_ALPHA         0x0d    

/* Destination Render Buffer */
#define RENDER_RGB8                     0
#define RENDER_RGB15                    1 << 8
#define RENDER_RGB16                    2 << 8
#define YUV_YSWAP                       4 << 8
#define YUV_NORMAL                      5 << 8
#define YUV_UVSWAP                      6 << 8
#define YUV_YUVSWAP                     7 << 8

#define ORG_XBIASMASK                   0x0F << 20
#define ORG_YBIASMASK                   0x0F << 16
#define VSTRIDE                         2
#define VSTRIDE_OFFSET                  1

/* Alpha Z-bias */
#define UPDATE_ZBIAS                    1 << 22
#define UPDATE_ALPHA_FX                 1 << 13
#define UPDATE_ALPHA_REFERENCE          1 << 8

#define ALPHAFX_NEVER                  1 << 9
#define ALPHAFX_LESS                    2 << 9
#define ALPHAFX_EQUAL                   3 << 9
#define ALPHAFX_LEQUAL                  4 << 9
#define ALPHAFX_GREATER                   5 << 9
#define ALPHAFX_NOTEQUAL                  6 << 9
#define ALPHAFX_GEQUAL                    7 << 9
#define ALPHAFX_ALWAYS                    8 << 9               

/* Scissor */
#define SCISSOR_ENABLE_UPDATE          1 << 1
#define SCISSOR_ENABLE                 1

/* Stipple */
#define STIPPLE_ENABLE                 1 << 16

/* Rendering Packets */
/* state pipelined */
#define COLOR_BLEND_STAGE               RENDER | 0x00 << 24
#define ALPHA_BLEND_STAGE               RENDER | 0x01 << 24
#define LINE_WIDTH_CULL_SHADE           RENDER | 0x02 << 24
#define BOOL_ENABLE_1                   RENDER | 0x03 << 24
#define BOOL_ENABLE_2                   RENDER | 0x04 << 24
#define VERTEX_FORMAT                   RENDER | 0x05 << 24
#define ANTIALIAS                       RENDER | 0x06 << 24
#define PVK_PIXEL_RULE                  RENDER | 0x07 << 24
#define SRC_DEST_BLEND_MONO             RENDER | 0x08 << 24
#define MAP_TEXEL                       RENDER | 0x1C << 24 
#define PRIMITIVE                       RENDER | 0x1F << 24

/* multiple dwords */
#define COLOR_FACTOR                    RENDER | 0x1D << 24 | 0x01 << 16 | 0
#define COLOR_CHROMA_KEY                RENDER | 0x1D << 24 | 0x02 << 16 | 1
#define DRAWING_RECT_INFO               RENDER | 0x1D << 24 | 0x80 << 16 | 3
#define RENDER_BUF_DEST                 RENDER | 0x1D << 24 | 0x85 << 16 | 0
#define SCISSOR_INFO                    RENDER | 0x1D << 24 | 0x81 << 16 | 1 
#define STIPPLE                         RENDER | 0x1D << 24 | 0x83 << 16 | 0

/* non-pipelined */
#define ALPHA_Z_BIAS                    RENDER | 0x14 << 24  
#define FOG_COLOR                       RENDER | 0x15 << 24
#define SCISSOR                         RENDER | 0x1C << 24 | 0x10 << 19


#define RBUFFER_START_MASK          0xFFFFF000
#define RBUFFER_SIZE_MASK           0x001FF000
#define RBUFFER_HEAD_MASK           0x001FFFFC
#define RBUFFER_TAIL_MASK           0x001FFFF8
#define RINGBUFFER_SIZE             (128 * 1024)
#define RING_SIZE_MASK              (RINGBUFFER_SIZE - 1)

#define I810RES_GART                1
#define I810RES_LRING_ACQ           2
#define I810RES_LRING_BIND          4
#define I810RES_OVL_ACQ             8
#define I810RES_OVL_BIND           16
#define I810RES_GART_ACQ           32
#define I810RES_MMAP               64
#define I810RES_STATE_SAVE        128

#ifndef AGP_NORMAL_MEMORY
#define AGP_NORMAL_MEMORY 0
#endif

#ifndef AGP_PHYSICAL_MEMORY
#define AGP_PHYSICAL_MEMORY 2
#endif

struct i810_ovl_regs {
	u32 obuf_0y;
	u32 obuf_1y;
	u32 obuf_0u;
	u32 obuf_0v;
	u32 obuf_1u;
	u32 obuf_1v;
	u32 ov0stride;
	u32 yrgb_vph;
	u32 uv_vph;
	u32 horz_ph;
	u32 init_ph;
	u32 dwinpos;
	u32 dwinsz;
	u32 swid;
	u32 swidqw;
	u32 sheight;
	u32 yrgbscale;
	u32 uvscale;
	u32 ov0clrc0;
	u32 ov0clrc1;
	u32 dclrkv;
	u32 dclrkm;
	u32 sclrkvh;
	u32 sclrkvl;
	u32 sclrkm;
	u32 ov0conf;
	u32 ov0cmd;
	u32 reserved;
	u32 awinpos;
	u32 awinsz;
};

typedef struct {
	CoreLayerRegionConfig config;
	int                   planar_bug;
} I810OverlayLayerData;


typedef struct {
     unsigned int   tail_mask;

     int            size;
     int            head;
     int            tail;
     int            space;
} I810RingBuffer;

typedef struct {
     volatile void *virt;
     unsigned int   tail_mask;
     unsigned int   outring;
} I810RingBlock;


typedef struct {
    bool                  initialized;

    I810RingBuffer        lp_ring;

    bool                  overlayOn;
    I810OverlayLayerData *iovl;

    agp_info              info;
    agp_allocate          lring_mem;
    agp_allocate          ovl_mem;
    agp_bind              lring_bind;
    agp_bind              ovl_bind;

    u32                   pattern;
    u32                   lring1;
    u32                   lring2;
    u32                   lring3;
    u32                   lring4;

    u32 i810fb_version;
    u32 cur_tail;
    int srcaddr, destaddr, srcpitch, destpitch;
    int color_value, color_value3d, pixeldepth, blit_color;
    int colorkey_bit, colorkey, render_color;
    int clip_x1, clip_x2, clip_y1, clip_y2;

    /* state validation */
	int i_src;
	int i_dst;
	int i_color;
	int i_colorkey;
	int i_clip;
        /* benchmarking */
	u32 waitfifo_sum;
	u32 waitfifo_calls;
	u32 idle_calls;
	u32 fifo_waitcycles;
	u32 idle_waitcycles;
	u32 fifo_cache_hits;
	u32 fifo_timeoutsum;
	u32 idle_timeoutsum;
} I810DeviceData;

typedef struct {
    I810DeviceData       *idev;

    volatile struct i810_ovl_regs *oregs;

    u32 flags;
	int agpgart;
	agp_info info;
	volatile u8 *aper_base;
	volatile u8 *lring_base;
	volatile u8 *ovl_base;
	volatile u8 *mmio_base;
	volatile u8 *pattern_base;
} I810DriverData;

extern DisplayLayerFuncs i810OverlayFuncs;

void i810ovlOnOff( I810DriverData       *idrv,
                   I810DeviceData       *idev,
                   bool                  on );


#define i810_readb(mmio_base, where)                     \
        *((volatile u8 *) (mmio_base + where))           \

#define i810_readw(mmio_base, where)                     \
       *((volatile u16 *) (mmio_base + where))           \

#define i810_readl(mmio_base, where)                     \
       *((volatile u32 *) (mmio_base + where))           \

#define i810_writeb(mmio_base, where, val)                              \
	*((volatile u8 *) (mmio_base + where)) = (volatile u8) val      \

#define i810_writew(mmio_base, where, val)                              \
	*((volatile u16 *) (mmio_base + where)) = (volatile u16) val    \

#define i810_writel(mmio_base, where, val)                              \
	*((volatile u32 *) (mmio_base + where)) = (volatile u32) val    \

#define PUT_LRING(val) {                                          \
        i810_writel(i810drv->lring_base, i810dev->cur_tail, val); \
	i810dev->cur_tail += 4;                                   \
	i810dev->cur_tail &= RING_SIZE_MASK;                      \
}

#define BEGIN_LRING i810_wait_for_space

#define END_LRING(i810drv) i810_writel(LRING, i810drv->mmio_base, i810dev->cur_tail)

#endif /* __I810_H__ */
