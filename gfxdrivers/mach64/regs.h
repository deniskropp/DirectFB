/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#ifndef __MACH64_REGS_H__
#define __MACH64_REGS_H__

/* LCD indexed registers */
#define LCD_GEN_CTRL            0x01
#define LCD_VERT_STRETCHING     0x05

/* LCD_GEN_CTRL */
#define LCD_ON                  0x00000002

/* LCD_VERT_STRETCHING */
#define VERT_STRETCH_RATIO0     0x000003FF
#define VERT_STRETCH_EN         0x80000000


#define LCD_INDEX               0x0A4
#define LCD_DATA                0x0A8

#define CONFIG_CHIP_ID          0x0E0
#define CONFIG_STAT0            0x0E4

#define DST_OFF_PITCH           0x100
#define DST_X                   0x104
#define DST_Y                   0x108
#define DST_Y_X                 0x10C
#define DST_WIDTH               0x110
#define DST_HEIGHT              0x114
#define DST_HEIGHT_WIDTH        0x118
#define DST_X_WIDTH             0x11C
#define DST_BRES_LNTH           0x120
#define DST_BRES_ERR            0x124
#define DST_BRES_INC            0x128
#define DST_BRES_DEC            0x12C
#define DST_CNTL                0x130
/* #define DST_Y_X                 0x134 */
#define TRAIL_BRES_ERR          0x138
#define TRAIL_BRES_INC          0x13C
#define TRAIL_BRES_DEC          0x140
#define LEAD_BRES_LNTH          0x144
#define Z_OFF_PITCH             0x148
#define Z_CNTL                  0x14C
#define ALPHA_TEST_CNTL         0x150
/* 0x154 */
#define SECONDARY_STW_EXP       0x158
#define SECONDARY_S_X_INC       0x15C
#define SECONDARY_S_Y_INC       0x160
#define SECONDARY_S_START       0x164
#define SECONDARY_W_X_INC       0x168
#define SECONDARY_W_Y_INC       0x16C
#define SECONDARY_W_START       0x170
#define SECONDARY_T_X_INC       0x174
#define SECONDARY_T_Y_INC       0x178
#define SECONDARY_T_START       0x17C
#define SRC_OFF_PITCH           0x180
#define SRC_X                   0x184
#define SRC_Y                   0x188
#define SRC_Y_X                 0x18C
#define SRC_WIDTH1              0x190
#define SRC_HEIGHT1             0x194
#define SRC_HEIGHT1_WIDTH1      0x198
#define SRC_X_START             0x19C
#define SRC_Y_START             0x1A0
#define SRC_Y_X_START           0x1A4
#define SRC_WIDTH2              0x1A8
#define SRC_HEIGHT2             0x1AC
#define SRC_HEIGHT2_WIDTH2      0x1B0
#define SRC_CNTL                0x1B4
/* 0x1B8 - 0x1BC */
#define SCALE_OFF               0x1C0
#define SECONDARY_SCALE_OFF     0x1C4
#define TEX_0_OFF               0x1C0
#define TEX_1_OFF               0x1C4
#define TEX_2_OFF               0x1C8
#define TEX_3_OFF               0x1CC
#define TEX_4_OFF               0x1D0
#define TEX_5_OFF               0x1D4
#define TEX_6_OFF               0x1D8
#define TEX_7_OFF               0x1DC
#define SCALE_WIDTH             0x1DC
#define SCALE_HEIGHT            0x1E0
#define TEX_8_OFF               0x1E0
#define TEX_9_OFF               0x1E4
#define TEX_10_OFF              0x1E8
/* #define S_Y_INC                 0x1EC */
#define SCALE_PITCH             0x1EC
#define SCALE_X_INC             0x1F0
/* #define RED_X_INC               0x1F0 */
/* #define GREEN_X_INC             0x1F4 */
#define SCALE_Y_INC             0x1F4
#define SCALE_VACC              0x1F8
#define SCALE_3D_CNTL           0x1FC
#define HOST_DATA0              0x200
#define HOST_DATA1              0x204
#define HOST_DATA2              0x208
#define HOST_DATA3              0x20C
#define HOST_DATA4              0x210
#define HOST_DATA5              0x214
#define HOST_DATA6              0x218
#define HOST_DATA7              0x21C
#define HOST_DATA8              0x220
#define HOST_DATA9              0x224
#define HOST_DATAA              0x228
#define HOST_DATAB              0x22C
#define HOST_DATAC              0x230
#define HOST_DATAD              0x234
#define HOST_DATAE              0x238
#define HOST_DATAF              0x23C
#define HOST_CNTL               0x240
#define BM_HOSTDATA             0x244
#define BM_ADDR                 0x248
#define BM_DATA                 0x248
#define BM_GUI_TABLE_CMD        0x24C
/* 0x250 - 0x27C */
#define PAT_REG0                0x280
#define PAT_REG1                0x284
#define PAT_CNTL                0x288
/* 0x28C - 0x29C */
#define SC_LEFT                 0x2A0
#define SC_RIGHT                0x2A4
#define SC_LEFT_RIGHT           0x2A8
#define SC_TOP                  0x2AC
#define SC_BOTTOM               0x2B0
#define SC_TOP_BOTTOM           0x2B4
#define USR1_DST_OFF_PITCH      0x2B8
#define USR2_DST_OFF_PITCH      0x2BC
#define DP_BKGD_CLR             0x2C0
#define DP_FRGD_CLR             0x2C4
#define DP_WRITE_MASK           0x2C8
#define DP_CHAIN_MASK           0x2CC
#define DP_PIX_WIDTH            0x2D0
#define DP_MIX                  0x2D4
#define DP_SRC                  0x2D8
#define DP_FRGD_CLR_MIX         0x2DC
#define DP_FRGD_BKGD_CLR        0x2E0
/* 0x2E4 */
#define DST_X_Y                 0x2E8
#define DST_WIDTH_HEIGHT        0x2EC
#define USR_DST_PITCH           0x2F0
/* 0x2F4 */
#define DP_SET_GUI_ENGINE2      0x2F8
#define DP_SET_GUI_ENGINE       0x2FC
#define CLR_CMP_CLR             0x300
#define CLR_CMP_MASK            0x304
#define CLR_CMP_CNTL            0x308
/* 0x30C */
#define FIFO_STAT               0x310
/* 0x314 */
/* 0x318 */
/* 0x31C */
#define CONTEXT_MASK            0x320
/* 0x324 */
/* 0x328 */
#define CONTEXT_LOAD_CNTL       0x32C
#define GUI_TRAJ_CNTL           0x330
/* 0x334 */
#define GUI_STAT                0x338
/* 0x33C */
#define TEX_PALETTE_INDEX       0x340
#define STW_EXP                 0x344
#define LOG_MAX_INC             0x348
#define S_X_INC                 0x34C
#define S_Y_INC                 0x350
/* #define SCALE_PITCH             0x350 */
#define S_START                 0x354
#define W_X_INC                 0x358
#define W_Y_INC                 0x35C
#define W_START                 0x360
#define T_X_INC                 0x364
#define T_Y_INC                 0x368
#define SECONDARY_SCALE_PITCH   0x368
#define T_START                 0x36C
#define TEX_SIZE_PITCH          0x370
#define TEX_CNTL                0x374
#define SECONDARY_TEX_OFFSET    0x378
#define TEX_PALETTE             0x37C
#define SCALE_PITCH_BOTH        0x380
#define SECONDARY_SCALE_OFF_ACC 0x384
#define SCALE_OFF_ACC           0x388
#define SCALE_DST_Y_X           0x38C
/* 0x390 - 0x394 */
#define COMPOSITE_SHADOW_ID     0x398
#define SECONDARY_SCALE_X_INC   0x39C
#define SPECULAR_RED_X_INC      0x39C
#define SPECULAR_RED_Y_INC      0x3A0
#define SPECULAR_RED_START      0x3A4
#define SECONDARY_SCALE_HACC    0x3A4
#define SPECULAR_GREEN_X_INC    0x3A8
#define SPECULAR_GREEN_Y_INC    0x3AC
#define SPECULAR_GREEN_START    0x3B0
#define SPECULAR_BLUE_X_INC     0x3B4
#define SPECULAR_BLUE_Y_INC     0x3B8
#define SPECULAR_BLUE_START     0x3BC
/* #define SCALE_X_INC             0x3C0 */
#define RED_X_INC               0x3C0
#define RED_Y_INC               0x3C4
#define RED_START               0x3C8
#define SCALE_HACC              0x3C8
/* #define SCALE_Y_INC             0x3CC */
#define GREEN_X_INC             0x3CC
#define GREEN_Y_INC             0x3D0
#define SECONDARY_SCALE_Y_INC   0x3D0
#define SECONDARY_SCALE_VACC    0x3D4
#define GREEN_START             0x3D4
#define BLUE_X_INC              0x3D8
#define BLUE_Y_INC              0x3DC
#define BLUE_START              0x3E0
#define Z_X_INC                 0x3E4
#define Z_Y_INC                 0x3E8
#define Z_START                 0x3EC
#define ALPHA_X_INC             0x3F0
#define FOG_X_INC               0x3F0
#define ALPHA_Y_INC             0x3F4
#define FOG_Y_INC               0x3F4
#define ALPHA_START             0x3F8
#define FOG_START               0x3F8
/* 0x3FC */
#define OVERLAY_Y_X_START       0x400
#define OVERLAY_Y_X_END         0x404
#define OVERLAY_VIDEO_KEY_CLR   0x408
#define OVERLAY_VIDEO_KEY_MSK   0x40C
#define OVERLAY_GRAPHICS_KEY_CLR        0x410
#define OVERLAY_GRAPHICS_KEY_MSK        0x414
#define OVERLAY_KEY_CNTL        0x418
/* 0x41C */
#define OVERLAY_SCALE_INC       0x420
#define OVERLAY_SCALE_CNTL      0x424
#define SCALER_HEIGHT_WIDTH     0x428
#define SCALER_TEST             0x42C
/* 0x430 */
#define SCALER_BUF0_OFFSET      0x434
#define SCALER_BUF1_OFFSET      0x438
#define SCALER_BUF_PITCH        0x43C
#define CAPTURE_START_END       0x440
#define CAPTURE_X_WIDTH         0x444
#define VIDEO_FORMAT            0x448
#define VBI_START_END           0x44C
#define CAPTURE_CONFIG          0x450
#define TRIG_CNTL               0x454
#define OVERLAY_EXCLUSIVE_HORZ  0x458
#define OVERLAY_EXCLUSIVE_VERT  0x45C
#define VAL_WIDTH               0x460
#define CAPTURE_DEBUG           0x464
#define VIDEO_SYNC_TEST         0x468
/* 0x46C */
#define SNAPSHOT_VH_COUNTS      0x470
#define SNAPSHOT_F_COUNT        0x474
#define N_VIF_COUNT             0x478
#define SNAPSHOT_VIF_COUNT      0x47C
#define CAPTURE_BUF0_OFFSET     0x480
#define CAPTURE_BUF1_OFFSET     0x484
#define CAPTURE_BUF_PITCH       0x488
/* 0x48C */
#define SNAPSHOT2_VH_COUNTS     0x4B0
#define SNAPSHOT2_F_COUNT       0x4B4
#define N_VIF2_COUNT            0x4B8
#define SNAPSHOT2_VIF_COUNT     0x4BC
#define MPP_CONFIG              0x4C0
#define MPP_STROBE_SEQ          0x4C4
#define MPP_ADDR                0x4C8
#define MPP_DATA                0x4CC
#define TVO_CNTL                0x500
/* 0x504 - 0x540 */
#define CRT_HORZ_VERT_LOAD      0x544
#define AGP_BASE                0x548
#define AGP_CNTL                0x54C
#define SCALER_COLOR_CNTL       0x550
#define SCALER_H_COEFF0         0x554
#define SCALER_H_COEFF1         0x558
#define SCALER_H_COEFF2         0x55C
#define SCALER_H_COEFF3         0x560
#define SCALER_H_COEFF4         0x564
/* 0x568 - 0x56C */
#define GUI_CMDFIFO_DEBUG       0x570
#define GUI_CMDFIFO_DATA        0x574
#define GUI_CNTL                0x578
/* 0x57C */
#define BM_FRAME_BUF_OFFSET     0x580
#define BM_SYSTEM_MEM_ADDR      0x584
#define BM_COMMAND              0x588
#define BM_STATUS               0x58C
/* 0x590 - 0x5B4 */
#define BM_GUI_TABLE            0x5B8
#define BM_SYSTEM_TABLE         0x5BC
/* 0x5D0 */
#define SCALER_BUF0_OFFSET_U    0x5D4
#define SCALER_BUF0_OFFSET_V    0x5D8
#define SCALER_BUF1_OFFSET_U    0x5DC
#define SCALER_BUF1_OFFSET_V    0x5E0


/* SRC_CNTL */
#define SRC_PATTERN_EN                  0x1
#define SRC_ROTATION_EN                 0x2
#define SRC_LINEAR_EN                   0x4
#define SRC_BYTE_ALIGN                  0x8
#define SRC_LINE_X_RIGHT_TO_LEFT        0x00
#define SRC_LINE_X_LEFT_TO_RIGHT        0x10
#define FAST_FILL_EN                    0x40
#define COLOR_REG_WRITE_EN              0x2000
#define BLOCK_WRITE_EN                  0x4000

/* DST_BRES_LNTH */
#define DRAW_TRAP               0x8000
#define LINE_DIS                0x80000000

/* DST_CNTL */
#define DST_X_RIGHT_TO_LEFT     0x0
#define DST_X_LEFT_TO_RIGHT     0x1
#define DST_Y_BOTTOM_TO_TOP     0x0
#define DST_Y_TOP_TO_BOTTOM     0x2
#define DST_X_MAJOR             0x0
#define DST_Y_MAJOR             0x4
#define DST_X_TILE              0x8
#define DST_Y_TILE              0x10
#define DST_LAST_PEL            0x20
#define DST_POLYGON_EN          0x40
#define DST_24_ROTATION_EN      0x80
#define TRAIL_X_RIGHT_TO_LEFT   0x0000
#define TRAIL_X_LEFT_TO_RIGHT   0x2000
#define TRAP_FILL_RIGHT_TO_LEFT 0x0000
#define TRAP_FILL_LEFT_TO_RIGHT 0x4000

/* DP_PIX_WIDTH */
#define DST_PIX_WIDTH_MONO		0x0
#define DST_PIX_WIDTH_CI8		0x2
#define DST_PIX_WIDTH_ARGB1555		0x3
#define DST_PIX_WIDTH_RGB565		0x4
#define DST_PIX_WIDTH_RGB888		0x5
#define DST_PIX_WIDTH_ARGB8888		0x6
#define DST_PIX_WIDTH_RGB332		0x7
#define DST_PIX_WIDTH_Y8		0x8
#define DST_PIX_WIDTH_RGB8		0x9
#define DST_PIX_WIDTH_VYUY		0xB
#define DST_PIX_WIDTH_YVYU		0xC
#define DST_PIX_WIDTH_AYUV8888		0xE
#define DST_PIX_WIDTH_ARGB4444		0xF
#define SRC_PIX_WIDTH_MONO		0x000
#define SRC_PIX_WIDTH_CI8		0x200
#define SRC_PIX_WIDTH_ARGB1555		0x300
#define SRC_PIX_WIDTH_RGB565		0x400
#define SRC_PIX_WIDTH_ARGB8888		0x600
#define SRC_PIX_WIDTH_RGB332		0x700
#define SRC_PIX_WIDTH_Y8		0x800
#define SRC_PIX_WIDTH_VYUY		0xB00
#define SRC_PIX_WIDTH_YVYU		0xC00
#define SRC_PIX_WIDTH_AYUV8888		0xE00
#define SRC_PIX_WIDTH_ARGB4444		0xF00
#define SCALE_PIX_WIDTH_CI8		0x20000000
#define SCALE_PIX_WIDTH_ARGB1555	0x30000000
#define SCALE_PIX_WIDTH_RGB565		0x40000000
#define SCALE_PIX_WIDTH_ARGB8888	0x60000000
#define SCALE_PIX_WIDTH_RGB332		0x70000000
#define SCALE_PIX_WIDTH_Y8		0x80000000
#define SCALE_PIX_WIDTH_RGB8		0x90000000
#define SCALE_PIX_WIDTH_VYUY		0xB0000000
#define SCALE_PIX_WIDTH_YVYU		0xC0000000
#define SCALE_PIX_WIDTH_AYUV8888	0xE0000000
#define SCALE_PIX_WIDTH_ARGB4444	0xF0000000

/* DP_SRC */
#define BKGD_SRC_BKGD_CLR	0x0
#define BKGD_SRC_FRGD_CLR	0x1
#define BKGD_SRC_HOST		0x2
#define BKGD_SRC_BLIT		0x3
#define BKGD_SRC_PATTERN	0x4
#define BKGD_SRC_SCALE		0x5
#define FRGD_SRC_BKGD_CLR	0x000
#define FRGD_SRC_FRGD_CLR	0x100
#define FRGD_SRC_HOST		0x200
#define FRGD_SRC_BLIT		0x300
#define FRGD_SRC_PATTERN	0x400
#define FRGD_SRC_SCALE		0x500
#define MONO_SRC_ONE		0x00000
#define MONO_SRC_PATTERN	0x10000
#define MONO_SRC_HOST		0x20000
#define MONO_SRC_BLIT		0x30000

/* DP_MIX */
#define BKGD_MIX_DST            0x3
#define BKGD_MIX_SRC            0x7
#define FRGD_MIX_DST            0x30000
#define FRGD_MIX_SRC            0x70000

/* CLR_CMP_CNTL */
#define COMPARE_FALSE		0x0
#define COMPARE_TRUE		0x1
#define COMPARE_NOT_EQUAL	0x4
#define COMPARE_EQUAL		0x5
#define COMPARE_DESTINATION	0x0000000
#define COMPARE_SOURCE		0x1000000
#define COMPARE_SCALE		0x2000000

/* SCALE_3D_CNTL */
#define SCALE_3D_FCN_NOP        0x00
#define SCALE_3D_FCN_SCALE      0x40
#define SCALE_3D_FCN_TEXTURE    0x80
#define SCALE_3D_FCN_SHADE      0xC0
#define SCALE_PIX_REP           0x100
#define ALPHA_FOG_EN_ALPHA      0x0800
#define ALPHA_FOG_EN_FOG        0x1000
#define ALPHA_BLND_SAT                  0x2000
#define ALPHA_BLND_SRC_ZERO             0x00000
#define ALPHA_BLND_SRC_ONE              0x10000
#define ALPHA_BLND_SRC_DSTCOLOR         0x20000
#define ALPHA_BLND_SRC_INVDSTCOLOR      0x30000
#define ALPHA_BLND_SRC_SRCALPHA         0x40000
#define ALPHA_BLND_SRC_INVSRCALPHA      0x50000
#define ALPHA_BLND_SRC_DSTALPHA         0x60000
#define ALPHA_BLND_SRC_INVDSTALPHA      0x70000
#define ALPHA_BLND_DST_ZERO             0x000000
#define ALPHA_BLND_DST_ONE              0x080000
#define ALPHA_BLND_DST_SRCCOLOR         0x100000
#define ALPHA_BLND_DST_INVSRCCOLOR      0x180000
#define ALPHA_BLND_DST_SRCALPHA         0x200000
#define ALPHA_BLND_DST_INVSRCALPHA      0x280000
#define ALPHA_BLND_DST_DSTALPHA         0x300000
#define ALPHA_BLND_DST_INVDSTALPHA      0x380000
#define TEX_LIGHT_FCN_MODULATE          0x400000
#define MIP_MAP_DISABLE                 0x1000000
#define BILINEAR_TEX_EN                 0x2000000
#define TEX_BLEND_FCN_NEAREST_MIPMAP_NEAREST    0x0000000
#define TEX_BLEND_FCN_NEAREST_MIPMAP_LINEAR     0x4000000
#define TEX_BLEND_FCN_LINEAR_MIPMAP_NEAREST     0x8000000
#define TEX_BLEND_FCN_LINEAR_MIPMAP_LINEAR      0xC000000
#define TEX_MAP_AEN                     0x40000000

/* TEX_CNTL */
#define TEX_CACHE_FLUSH         0x800000

/* OVERLAY_SCALE_CNTL */
#define SCALE_PIX_EXPAND        0x00000001
#define SCALE_Y2R_TEMP          0x00000002
#define SCALE_HORZ_MODE         0x00000004
#define SCALE_VERT_MODE         0x00000008
#define SCALE_SIGNED_UV         0x00000010
#define SCALE_GAMMA_SEL         0x00000060
#define SCALE_BANDWITH          0x04000000
#define SCALE_DIS_LIMIT         0x08000000
#define SCALE_CLK_FORCE_ON      0x20000000
#define OVERLAY_EN              0x40000000
#define SCALE_EN                0x80000000

/* OVERLAY_Y_X_START */
#define OVERLAY_LOCK_START      0x80000000

/* OVERLAY_Y_X_END */
#define OVERLAY_LOCK_END        0x80000000

/* OVERLAY_KEY_CNTL */
#define VIDEO_MIX_FALSE         0x0
#define VIDEO_MIX_TRUE          0x1
#define VIDEO_MIX_NOT_EQUAL     0x4
#define VIDEO_MIX_EQUAL         0x5
#define GRAPHICS_MIX_FALSE      0x00
#define GRAPHICS_MIX_TRUE       0x10
#define GRAPHICS_MIX_NOT_EQUAL  0x40
#define GRAPHICS_MIX_EQUAL      0x50
#define CMP_MIX                 0x100

/* VIDEO_FORMAT */
#define VIDEO_IN_VYUY422        0x0000000B
#define VIDEO_IN_YVYU422        0x0000000C
#define VIDEO_SIGNED_UV         0x00000010
#define SCALER_IN_RGB15         0x00030000
#define SCALER_IN_RGB16         0x00040000
#define SCALER_IN_RGB32         0x00060000
#define SCALER_IN_YUV9          0x00090000
#define SCALER_IN_YUV12         0x000A0000
#define SCALER_IN_VYUY422       0x000B0000
#define SCALER_IN_YVYU422       0x000C0000

#endif
