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

/* SRC_CNTL */
#define SRC_PATTERN_EN                  0x1
#define SRC_ROTATION_EN                 0x2
#define SRC_LINEAR_EN                   0x4
#define SRC_BYTE_ALIGN                  0x8
#define SRC_LINE_X_RIGHT_TO_LEFT        0x00
#define SRC_LINE_X_LEFT_TO_RIGHT        0x10

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

/* DP_CHAIN_MASK */
#define DP_CHAIN_1BPP		0x0000
#define DP_CHAIN_4BPP		0x8888
#define DP_CHAIN_7BPP		0xD2D2
#define DP_CHAIN_8BPP		0x8080
#define DP_CHAIN_8BPP_RGB	0x9292
#define DP_CHAIN_15BPP		0x4210
#define DP_CHAIN_16BPP		0x8410
#define DP_CHAIN_24BPP		0x8080
#define DP_CHAIN_32BPP		0x8080

/* DP_PIX_WIDTH */
#define DST_1BPP		0x0
#define DST_4BPP		0x1
#define DST_8BPP		0x2
#define DST_15BPP		0x3
#define DST_16BPP		0x4
#define DST_32BPP		0x6
#define SRC_1BPP		0x000
#define SRC_4BPP		0x100
#define SRC_8BPP		0x200
#define SRC_15BPP		0x300
#define SRC_16BPP		0x400
#define SRC_32BPP		0x600
#define HOST_TRIPLE_EN		0x2000
#define HOST_1BPP		0x00000
#define HOST_4BPP		0x10000
#define HOST_8BPP		0x20000
#define HOST_15BPP		0x30000
#define HOST_16BPP		0x40000
#define HOST_32BPP		0x60000
#define BYTE_ORDER_MSB_TO_LSB	0x0000000
#define BYTE_ORDER_LSB_TO_MSB	0x1000000

/* DP_SRC */
#define BKGD_SRC_BKGD_CLR	0x0
#define BKGD_SRC_FRGD_CLR	0x1
#define BKGD_SRC_HOST		0x2
#define BKGD_SRC_BLIT		0x3
#define BKGD_SRC_PATTERN	0x4
#define FRGD_SRC_BKGD_CLR	0x000
#define FRGD_SRC_FRGD_CLR	0x100
#define FRGD_SRC_HOST		0x200
#define FRGD_SRC_BLIT		0x300
#define FRGD_SRC_PATTERN	0x400
#define MONO_SRC_ONE		0x00000
#define MONO_SRC_PATTERN	0x10000
#define MONO_SRC_HOST		0x20000
#define MONO_SRC_BLIT		0x30000

/* CLR_CMP_CNTL */
#define COMPARE_FALSE		0x0
#define COMPARE_TRUE		0x1
#define COMPARE_NOT_EQUAL	0x4
#define COMPARE_EQUAL		0x5
#define COMPARE_DESTINATION	0x0000000
#define COMPARE_SOURCE		0x1000000
#define COMPARE_TEXEL		0x2000000

#endif
