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

/* LCD Panel registers */
#define CONFIG_PANEL            0x00
#define LCD_GEN_CTRL            0x01
#define DSTN_CONTROL            0x02
#define HFB_PITCH_ADDR          0x03
#define HORZ_STRETCHING         0x04
#define VERT_STRETCHING         0x05
#define EXT_VERT_STRETCH        0x06
#define LT_GIO                  0x07
#define POWER_MANAGEMENT        0x08
#define ZVGPIO                  0x09
#define ICON_CLR0               0x0A
#define ICON_CLR1               0x0B
#define ICON_OFFSET             0x0C
#define ICON_HORZ_VERT_POSN     0x0D
#define ICON_HORZ_VERT_OFF      0x0E
#define ICON2_CLR0              0x0F
#define ICON2_CLR1              0x10
#define ICON2_OFFSET            0x11
#define ICON2_HORZ_VERT_POSN    0x12
#define ICON2_HORZ_VERT_OFF     0x13
#define LCD_MISC_CNTL           0x14
#define TMDS_CNTL               0x15
#define SCRATCH_PAD_4           0x15
#define TMDS_SYNC_CHAR_SETA     0x16
#define SCRATCH_PAD_5           0x16
#define TMDS_SYNC_CHAR_SETB     0x17
#define SCRATCH_PAD_6           0x17
#define TMDS_CRC                0x18
#define SCRATCH_PAD_7           0x18
#define PLTSTBLK_GEN_SEED       0x19
#define SCRATCH_PAD_8           0x19
#define SYNC_GEN_CNTL           0x1A
#define PATTERN_GEN_SEED        0x1B
#define APC_CNTL                0x1C
#define POWER_MANAGEMENT_2      0x1D
#define PRI_ERR_PATTERN         0x1E
#define CUR_ERR_PATTERN         0x1F
#define PLTSTBLK_RPT            0x20
#define SYNC_RPT                0x21
#define CRC_PATTERN_RPT         0x22
#define PL_TRANSMITTER_CNTL     0x23
#define PL_PLL_CNTL             0x24
#define ALPHA_BLENDING          0x25
#define PORTRAIT_GEN_CNTL       0x26
#define APC_CTRL_IO             0x27
#define TEST_IO                 0x28
#define TEST_OUTPUTS            0x29
#define DP1_MEM_ACCESS          0x2A
#define DP0_MEM_ACCESS          0x2B
#define DP0_DEBUG_A             0x2C
#define DP0_DEBUG_B             0x2D
#define DP1_DEBUG_A             0x2E
#define DP1_DEBUG_B             0x2F
#define DPCTRL_DEBUG_A          0x30
#define DPCTRL_DEBUG_B          0x31
#define MEMBLK_DEBUG            0x32
/* #define SCRATCH_PAD_4           0x33 */
#define APC_LUT_AB              0x33
/* #define SCRATCH_PAD_5           0x34 */
#define APC_LUT_CD              0x34
/* #define SCRATCH_PAD_6           0x35 */
#define APC_LUT_EF              0x25
/* #define SCRATCH_PAD_7           0x36 */
#define APC_LUT_GH              0x36
/* #define SCRATCH_PAD_8           0x37 */
#define APC_LUT_IJ              0x37
#define APC_LUT_KL              0x38
#define APC_LUT_MN              0x39
#define APC_LUT_OP              0x3A

/* LCD_GEN_CTRL */
#define LCD_ON                          0x00000002

/* VERT_STRETCHING */
#define VERT_STRETCH_RATIO0             0x000003FF
#define VERT_STRETCH_EN                 0x80000000


/* PLL registers */
#define MPLL_CNTL               0x00
#define VPLL_CNTL               0x01
#define PLL_REF_DIV             0x02
#define PLL_GEN_CNTL            0x03
#define MCLK_FB_DIV             0x04
#define PLL_VCLK_CNTL           0x05
#define VCLK_POST_DIV           0x06
#define VCLK0_FB_DIV            0x07
#define VCLK1_FB_DIV            0x08
#define VCLK2_FB_DIV            0x09
#define VCLK3_FB_DIV            0x0A
#define PLL_EXT_CNTL            0x0B
#define DLL_CNTL                0x0C
#define DLL1_CNTL               0x0C
#define VFC_CNTL                0x0D
#define PLL_TEST_CNTL           0x0E
#define PLL_TEST_COUNT          0x0F
#define LVDSPLL_CNTL0           0x10
#define LVDS_CNTL0              0x10
#define LVDSPLL_CNTL1           0x11
#define LVDS_CNTL1              0x11
#define AGP1_CNTL               0x12
#define AGP2_CNTL               0x13
#define DLL2_CNTL               0x14
#define SCLK_FB_DIV             0x15
#define SPLL_CNTL1              0x16
#define SPLL_CNTL2              0x17
#define APLL_STRAPS             0x18
#define EXT_VPLL_CNTL           0x19
#define EXT_VPLL_REF_DIV        0x1A
#define EXT_VPLL_FB_DIV         0x1B
#define EXT_VPLL_MSB            0x1C
#define HTOTAL_CNTL             0x1D
#define BYTE_CLK_CNTL           0x1E
#define TV_PLL_CNTL1            0x1F
#define TV_PLL_CNTL2            0x20
#define TV_PLL_CNTL             0x21
#define EXT_TV_PLL              0x22
#define V2PLL_CNTL              0x23
#define PLL_V2CLK_CNTL          0x24
#define EXT_V2PLL_REF_DIV       0x25
#define EXT_V2PLL_FB_DIV        0x26
#define EXT_V2PLL_MSB           0x27
#define HTOTAL2_CNTL            0x28
#define PLL_YCLK_CNTL           0x29
#define PM_DYN_CLK_CNTL         0x2A

/* PLL_VCLK_CNTL */
#define ECP_DIV                         0x30


/* TV Out registers */
/* 0x00 - 0x0F */
#define TV_MASTER_CNTL          0x10
/* 0x11 */
#define TV_RGB_CNTL             0x12
/* 0x13 */
#define TV_SYNC_CNTL            0x14
/* 0x15 - 1F */
#define TV_HTOTAL               0x20
#define TV_HDISP                0x21
#define TV_HSIZE                0x22
#define TV_HSTART               0x23
#define TV_HCOUNT               0x24
#define TV_VTOTAL               0x25
#define TV_VDISP                0x26
#define TV_VCOUNT               0x27
#define TV_FTOTAL               0x28
#define TV_FCOUNT               0x29
#define TV_FRESTART             0x2A
#define TV_HRESTART             0x2B
#define TV_VRESTART             0x2C
/* 0x2D - 0x5F */
#define TV_HOST_READ_DATA       0x60
#define TV_HOST_WRITE_DATA      0x61
#define TV_HOST_RD_WT_CNTL      0x62
/* 0x63 - 0x6F */
#define TV_VSCALER_CNTL         0x70
#define TV_TIMING_CNTL          0x71
#define TV_GAMMA_CNTL           0x72
#define TV_Y_FALL_CNTL          0x73
#define TV_Y_RISE_CNTL          0x74
#define TV_Y_SAW_TOOTH_CNTL     0x75
/* 0x76 - 0x7F */
#define TV_MODULATOR_CNTL1      0x80
#define TV_MODULATOR_CNTL2      0x81
/* 0x82 - 0x8F */
#define TV_PRE_DAC_MUX_CNTL     0x90
/* 0x91 - 0x9F */
#define TV_DAC_CNTL             0xA0
/* 0xA1 - 0xAF */
#define TV_CRC_CNTL             0xB0
#define TV_VIDEO_PORT_SIG       0xB1
/* 0xB2 - 0xB7 */
#define TV_VBI_CC_CNTL          0xB8
#define TV_VBI_EDS_CNTL         0xB9
#define TV_VBI_20BIT_CNTL       0xBA
/* 0xBB - 0xBC */
#define TV_VBI_DTO_CNTL         0xBD
#define TV_VBI_LEVEL_CNTL       0xBE
/* 0xBF */
#define TV_UV_ADR               0xC0
#define TV_FIFO_TEST_CNTL       0xC1
/* 0xC2 - 0xFF */


/* Main registers */
#define CRTC_H_TOTAL_DISP       0x000
#define CRTC2_H_TOTAL_DISP      0x000
#define CRTC_H_SYNC_STRT_WID    0x004
#define CRTC2_H_SYNC_STRT_WID   0x004
#define CRTC_V_TOTAL_DISP       0x008
#define CRTC2_V_TOTAL_DISP      0x008
#define CRTC_V_SYNC_STRT_WID    0x00C
#define CRTC2_V_SYNC_STRT_WID   0x00C
#define CRTC_VLINE_CRNT_VLINE   0x010
#define CRTC2_VLINE_CRNT_VLINE  0x010
#define CRTC_OFF_PITCH          0x014
#define CRTC_INT_CNTL           0x018
#define CRTC_GEN_CNTL           0x01C
#define TV_OUT_INDEX            0x01D
#define DSP_CONFIG              0x020
#define PM_DSP_CONFIG           0x020
#define DSP_ON_OFF              0x024
#define PM_DSP_ON_OFF           0x024
#define TV_OUT_DATA             0x01D
#define TIMER_CONFIG            0x028
#define MEM_BUF_CNTL            0x02C
#define SHARED_CNTL             0x030
#define SHARED_MEM_CONFIG       0x034
#define MEM_ADDR_CONFIG         0x034
#define CRT_TRAP                0x038
#define I2C_CNTL_0              0x03C
#define DSTN_CONTROL_LT         0x03C
#define OVR_CLR                 0x040
#define OVR2_CLR                0x040
#define OVR_WID_LEFT_RIGHT      0x044
#define OVR2_WID_LEFT_RIGHT     0x044
#define OVR_WID_TOP_BOTTOM      0x048
#define OVR2_WID_TOP_BOTTOM     0x048
#define VGA_DSP_CONFIG          0x04C
#define PM_VGA_DSP_CONFIG       0x04C
#define VGA_DSP_ON_OFF          0x050
#define PM_VGA_DSP_ON_OFF       0x050
#define DSP2_CONFIG             0x054
#define PM_DSP2_CONFIG          0x054
#define DSP2_ON_OFF             0x058
#define PM_DSP2_ON_OFF          0x058
#define CRTC2_OFF_PITCH         0x05C
#define CUR_CLR0                0x060
#define CUR2_CLR0               0x060
#define CUR_CLR1                0x064
#define CUR2_CLR1               0x064
#define CUR_OFFSET              0x068
#define CUR2_OFFSET             0x068
#define CUR_HORZ_VERT_POSN      0x06C
#define CUR2_HORZ_VERT_POSN     0x06C
#define CUR_HORZ_VERT_OFF       0x070
#define CUR2_HORZ_VERT_OFF      0x070
#define CONFIG_PANEL_LT         0x074
#define GP_IO                   0x078
#define HW_DEBUG                0x07C
#define SCRATCH_REG0            0x080
#define SCRATCH_REG1            0x084
#define SCRATCH_REG2            0x088
#define SCRATCH_REG3            0x08C
#define CLOCK_CNTL              0x090
#define CLOCK_CNTL0             0x090
#define CLOCK_CNTL1             0x091
#define CLOCK_CNTL2             0x092
#define CLOCK_CNTL3             0x093
#define CONFIG_STAT1            0x094
#define CONFIG_STAT2            0x098
/* 0x09C */
#define BUS_CNTL                0x0A0
#define LCD_INDEX               0x0A4
#define LCD_DATA                0x0A8
#define HFB_PITCH_ADDR_LT       0x0A8
#define EXT_MEM_CNTL            0x0AC
#define MEM_CNTL                0x0B0
#define MEM_VGA_WP_SEL          0x0B4
#define MEM_VGA_RP_SEL          0x0B8
#define I2C_CNTL_1              0x0BC
#define LT_GIO_LT               0x0BC
#define DAC_REGS                0x0C0
#define DAC_CNTL                0x0C4
#define EXT_DAC_REGS            0x0C8
#define HORZ_STRETCHING_LT      0x0C8
#define VERT_STRETCHING_LT      0x0CC
#define GEN_TEST_CNTL           0x0D0
#define CUSTOM_MACRO_CNTL       0x0D4
#define LCD_GEN_CTRL_LT         0x0D4
#define POWER_MANAGEMENT_LT     0x0D8
#define CONFIG_CNTL             0x0DC
#define CONFIG_CHIP_ID          0x0E0
#define CONFIG_STAT0            0x0E4
#define CRC_SIG                 0x0E8
#define CRC2_SIG                0x0E8
/* 0x0EC - 0x0FC */
#define DST_OFF_PITCH           0x100
#define DST_X                   0x104
#define DST_Y                   0x108
#define DST_Y_X                 0x10C
#define DST_WIDTH               0x110
#define DST_HEIGHT              0x114
#define DST_HEIGHT_WIDTH        0x118
#define DST_X_WIDTH             0x11C
#define DST_BRES_LNTH           0x120
/* #define LEAD_BRES_LNTH          0x120 */
#define DST_BRES_ERR            0x124
#define LEAD_BRES_ERR           0x124
#define DST_BRES_INC            0x128
#define LEAD_BRES_INC           0x128
#define DST_BRES_DEC            0x12C
#define LEAD_BRES_DEC           0x12C
#define DST_CNTL                0x130
/* #define DST_Y_X                 0x134 */
#define TRAIL_BRES_ERR          0x138
#define TRAIL_BRES_INC          0x13C
#define TRAIL_BRES_DEC          0x140
#define LEAD_BRES_LNTH          0x144
#define Z_OFF_PITCH             0x148
#define Z_CNTL                  0x14C
#define ALPHA_TST_CNTL          0x150
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
#define SCALE_Y_OFF             0x1C0
#define SCALE_OFF               0x1C0
#define TEX_0_OFF               0x1C0
#define SECONDARY_SCALE_OFF     0x1C4
#define TEX_1_OFF               0x1C4
#define TEX_2_OFF               0x1C8
#define TEX_3_OFF               0x1CC
#define TEX_4_OFF               0x1D0
#define TEX_5_OFF               0x1D4
#define TEX_6_OFF               0x1D8
#define SCALE_WIDTH             0x1DC
#define TEX_7_OFF               0x1DC
#define SCALE_HEIGHT            0x1E0
#define TEX_8_OFF               0x1E0
#define TEX_9_OFF               0x1E4
#define TEX_10_OFF              0x1E8
#define SCALE_Y_PITCH           0x1EC
#define SCALE_PITCH             0x1EC
/* #define S_Y_INC                 0x1EC */
#define SCALE_X_INC             0x1F0
/* #define RED_X_INC               0x1F0 */
#define SCALE_Y_INC             0x1F4
/* #define GREEN_X_INC             0x1F4 */
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
#define DP_FOG_CLR              0x2C4
#define DP_WRITE_MSK            0x2C8
#define DP_CHAIN_MSK            0x2CC
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
#define CLR_CMP_MSK             0x304
#define CLR_CMP_CNTL            0x308
/* 0x30C */
#define FIFO_STAT               0x310
/* 0x314 - 0x31C */
#define CONTEXT_MSK             0x320
/* 0x324 */
/* 0x328 */
#define CONTEXT_LOAD_CNTL       0x32C
#define GUI_TRAJ_CNTL           0x330
/* 0x334 */
#define GUI_STAT                0x338
/* 0x33C */
#define S_X_INC2                0x340
#define TEX_PALETTE_INDEX       0x340
#define S_Y_INC2                0x344
#define STW_EXP                 0x344
#define S_XY_INC2               0x348
#define LOG_MAX_INC             0x348
#define S_X_INC_START           0x34C
#define S_X_INC                 0x34C
#define S_Y_INC                 0x350
/* #define SCALE_Y_PITCH           0x350 */
/* #define SCALE_PITCH             0x350 */
#define S_START                 0x354
#define T_X_INC2                0x358
#define W_X_INC                 0x358
#define T_Y_INC2                0x35C
#define W_Y_INC                 0x35C
#define T_XY_INC2               0x360
#define W_START                 0x360
#define T_X_INC_START           0x364
#define T_X_INC                 0x364
#define SECONDARY_SCALE_PITCH   0x368
#define T_Y_INC                 0x368
#define T_START                 0x36C
#define TEX_SIZE_PITCH          0x370
#define TEX_CNTL                0x374
#define SECONDARY_TEX_OFFSET    0x378
#define TEX_PAL_WR              0x37C
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
#define SECONDARY_SCALE_HACC    0x3A4
#define SPECULAR_RED_START      0x3A4
#define SPECULAR_GREEN_X_INC    0x3A8
#define SPECULAR_GREEN_Y_INC    0x3AC
#define SPECULAR_GREEN_START    0x3B0
#define SPECULAR_BLUE_X_INC     0x3B4
#define SPECULAR_BLUE_Y_INC     0x3B8
#define SPECULAR_BLUE_START     0x3BC
/* #define SCALE_X_INC             0x3C0 */
#define RED_X_INC               0x3C0
#define RED_Y_INC               0x3C4
#define SCALE_HACC              0x3C8
#define RED_START               0x3C8
/* #define SCALE_Y_INC             0x3CC */
#define GREEN_X_INC             0x3CC
#define SECONDARY_SCALE_Y_INC   0x3D0
#define GREEN_Y_INC             0x3D0
#define SECONDARY_SCALE_VACC    0x3D4
#define GREEN_START             0x3D4
#define SCALE_XUV_INC           0x3D8
#define BLUE_X_INC              0x3D8
#define BLUE_Y_INC              0x3DC
#define SCALE_UV_HACC           0x3E0
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
#define VBI_WIDTH               0x460
#define CAPTURE_DEBUG           0x464
#define VIDEO_SYNC_TEST         0x468
/* 0x46C */
#define SNAPSHOT_VH_COUNTS      0x470
#define SNAPSHOT_F_COUNT        0x474
#define N_VIF_COUNT             0x478
#define SNAPSHOT_VIF_COUNT      0x47C
#define BUF0_OFFSET             0x480
#define CAPTURE_BUF0_OFFSET     0x480
#define CAPTURE_BUF1_OFFSET     0x484
#define ONESHOT_BUF_OFFSET      0x488
#define CAPTURE_BUF_PITCH       0x488
#define BUF0_PITCH              0x48C
/* 0x490 - 0x494 */
#define BUF1_OFFSET             0x498
/* 0x49C - 0x4A0 */
#define BUF1_PITCH              0x4A4
/* 0x4A8 */
#define BUF0_CAP_OFFSET         0x4AC
#define BUF1_CAP_OFFSET         0x4B0
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
#define SCALER_COLOUR_CNTL      0x550
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
/* 0x5E4 - 0x63C */
#define VERTEX_1_S              0x640
#define VERTEX_1_T              0x644
#define VERTEX_1_W              0x648
#define VERTEX_1_SPEC_ARGB      0x64C
#define VERTEX_1_Z              0x650
#define VERTEX_1_ARGB           0x654
#define VERTEX_1_X_Y            0x658
/* #define ONE_OVER_AREA           0x65C */
#define VERTEX_2_S              0x660
#define VERTEX_2_T              0x664
#define VERTEX_2_W              0x668
#define VERTEX_2_SPEC_ARGB      0x66C
#define VERTEX_2_Z              0x670
#define VERTEX_2_ARGB           0x674
#define VERTEX_2_X_Y            0x678
/* #define ONE_OVER_AREA           0x67C */
#define VERTEX_3_S              0x680
#define VERTEX_3_T              0x684
#define VERTEX_3_W              0x688
#define VERTEX_3_SPEC_ARGB      0x68C
#define VERTEX_3_Z              0x690
#define VERTEX_3_ARGB           0x694
#define VERTEX_3_X_Y            0x698
#define ONE_OVER_AREA           0x69C
#define VERTEX_3_SECONDARY_S    0x6A0
#define VERTEX_3_SECONDARY_T    0x6A4
#define VERTEX_3_SECONDARY_W    0x6A8
/* #define VERTEX_1_S              0x6AC */
/* #define VERTEX_1_T              0x6B0 */
/* #define VERTEX_1_W              0x6B4 */
/* #define VERTEX_2_S              0x6B8 */
/* #define VERTEX_2_T              0x6BC */
/* #define VERTEX_2_W              0x6C0 */
/* #define VERTEX_3_S              0x6C4 */
/* #define VERTEX_3_T              0x6C8 */
/* #define VERTEX_3_W              0x6CC */
/* #define VERTEX_1_SPEC_ARGB      0x6D0 */
/* #define VERTEX_2_SPEC_ARGB      0x6D4 */
/* #define VERTEX_3_SPEC_ARGB      0x6D8 */
/* #define VERTEX_1_Z              0x6DC */
/* #define VERTEX_2_Z              0x6E0 */
/* #define VERTEX_3_Z              0x6E4 */
/* #define VERTEX_1_ARGB           0x6E8 */
/* #define VERTEX_2_ARGB           0x6EC */
/* #define VERTEX_3_ARGB           0x6F0 */
/* #define VERTEX_1_X_Y            0x6F4 */
/* #define VERTEX_2_X_Y            0x6F8 */
/* #define VERTEX_3_X_Y            0x6FC */
#define ONE_OVER_AREA_UC        0x700
#define SETUP_CNTL              0x704
/* 0x708 - 0x724 */
#define VERTEX_1_SECONDARY_S    0x728
#define VERTEX_1_SECONDARY_T    0x72C
#define VERTEX_1_SECONDARY_W    0x730
#define VERTEX_2_SECONDARY_S    0x734
#define VERTEX_2_SECONDARY_T    0x738
#define VERTEX_2_SECONDARY_W    0x73C
/* 0x740 - 0x7FC */


/* HW_DEBUG */
#define INTER_PRIM_DIS                  0x00000040
#define AUTO_BLKWRT_COLOR_DIS           0x00000100
#define AUTO_FF_DIS                     0x00001000
#define AUTO_BLKWRT_DIS                 0x00002000

/* CLOCK_CNTL1 */
#define PLL_WR_EN                       0x02

/* CONFIG_CHIP_ID */
#define CFG_CHIP_TYPE                   0x0000FFFF
#define CFG_CHIP_CLASS                  0x00FF0000
#define CFG_CHIP_MAJOR                  0x07000000
#define CFG_CHIP_FND_ID                 0x38000000
#define CFG_CHIP_MINOR                  0xC0000000

/* CONFIG_STAT0 */
#define CFG_MEM_TYPE                    0x00000007
#define CFG_MEM_TYPE_SGRAM              0x00000005

/* DST_BRES_LNTH */
#define DRAW_TRAP                       0x00008000
#define LINE_DIS                        0x80000000

/* DST_CNTL */
#define DST_X_DIR                       0x00000001
#define DST_Y_DIR                       0x00000002
#define DST_Y_MAJOR                     0x00000004
#define DST_X_TILE                      0x00000008
#define DST_Y_TILE                      0x00000010
#define DST_LAST_PEL                    0x00000020
#define DST_POLYGON_EN                  0x00000040
#define DST_24_ROTATION_EN              0x00000080
#define TRAIL_X_DIR                     0x00002000
#define TRAP_FILL_DIR                   0x00004000

/* ALPHA_TST_CNTL */
#define ALPHA_DST_SEL_ZERO              0x00000000
#define ALPHA_DST_SEL_ONE               0x00000100
#define ALPHA_DST_SEL_SRCALPHA          0x00000400
#define ALPHA_DST_SEL_INVSRCALPHA       0x00000500
#define ALPHA_DST_SEL_DSTALPHA          0x00000600
#define ALPHA_DST_SEL_INVDSTALPHA       0x00000700

/* SRC_CNTL */
#define SRC_PATTERN_EN                  0x00000001
#define SRC_ROTATION_EN                 0x00000002
#define SRC_LINEAR_EN                   0x00000004
#define SRC_BYTE_ALIGN                  0x00000008
#define SRC_LINE_X_DIR                  0x00000010
#define FAST_FILL_EN                    0x00000040
#define COLOR_REG_WRITE_EN              0x00002000
#define BLOCK_WRITE_EN                  0x00004000

/* DP_PIX_WIDTH (GT) */
#define DST_PIX_WIDTH_MONO              0x00000000
#define DST_PIX_WIDTH_CI8               0x00000002
#define DST_PIX_WIDTH_ARGB1555          0x00000003
#define DST_PIX_WIDTH_RGB565            0x00000004
#define DST_PIX_WIDTH_RGB888            0x00000005
#define DST_PIX_WIDTH_ARGB8888          0x00000006
#define DST_PIX_WIDTH_RGB332            0x00000007
#define DST_PIX_WIDTH_Y8                0x00000008
#define DST_PIX_WIDTH_RGB8              0x00000009
#define DST_PIX_WIDTH_VYUY              0x0000000B
#define DST_PIX_WIDTH_YVYU              0x0000000C
#define DST_PIX_WIDTH_AYUV8888          0x0000000E
#define DST_PIX_WIDTH_ARGB4444          0x0000000F
#define SRC_PIX_WIDTH_MONO              0x00000000
#define SRC_PIX_WIDTH_CI8               0x00000200
#define SRC_PIX_WIDTH_ARGB1555          0x00000300
#define SRC_PIX_WIDTH_RGB565            0x00000400
#define SRC_PIX_WIDTH_ARGB8888          0x00000600
#define SRC_PIX_WIDTH_RGB332            0x00000700
#define SRC_PIX_WIDTH_Y8                0x00000800
#define SRC_PIX_WIDTH_VYUY              0x00000B00
#define SRC_PIX_WIDTH_YVYU              0x00000C00
#define SRC_PIX_WIDTH_AYUV8888          0x00000E00
#define SRC_PIX_WIDTH_ARGB4444          0x00000F00
#define SCALE_PIX_WIDTH_CI8             0x20000000
#define SCALE_PIX_WIDTH_ARGB1555        0x30000000
#define SCALE_PIX_WIDTH_RGB565          0x40000000
#define SCALE_PIX_WIDTH_ARGB8888        0x60000000
#define SCALE_PIX_WIDTH_RGB332          0x70000000
#define SCALE_PIX_WIDTH_Y8              0x80000000
#define SCALE_PIX_WIDTH_RGB8            0x90000000
#define SCALE_PIX_WIDTH_VYUY            0xB0000000
#define SCALE_PIX_WIDTH_YVYU            0xC0000000
#define SCALE_PIX_WIDTH_AYUV8888        0xE0000000
#define SCALE_PIX_WIDTH_ARGB4444        0xF0000000

/* DP_PIX_WIDTH (GX/CT/VT) */
#define DST_PIX_WIDTH_8BPP              0x00000002
#define DST_PIX_WIDTH_15BPP             0x00000003
#define DST_PIX_WIDTH_16BPP             0x00000004
#define DST_PIX_WIDTH_32BPP             0x00000006
#define SRC_PIX_WIDTH_8BPP              0x00000200
#define SRC_PIX_WIDTH_15BPP             0x00000300
#define SRC_PIX_WIDTH_16BPP             0x00000400
#define SRC_PIX_WIDTH_32BPP             0x00000600

/* DP_PIX_WIDTH masks */
#define DST_PIX_WIDTH                   0x0000000F
#define SRC_PIX_WIDTH                   0x00000F00
#define SCALE_PIX_WIDTH                 0xF0000000

/* DP_MIX */
#define BKGD_MIX_DST                    0x00000003
#define BKGD_MIX_SRC                    0x00000007
#define FRGD_MIX_DST                    0x00030000
#define FRGD_MIX_SRC                    0x00070000

/* DP_SRC */
#define BKGD_SRC_BKGD_CLR               0x00000000
#define BKGD_SRC_FRGD_CLR               0x00000001
#define BKGD_SRC_HOST                   0x00000002
#define BKGD_SRC_BLIT                   0x00000003
#define BKGD_SRC_PATTERN                0x00000004
#define BKGD_SRC_SCALE                  0x00000005
#define FRGD_SRC_BKGD_CLR               0x00000000
#define FRGD_SRC_FRGD_CLR               0x00000100
#define FRGD_SRC_HOST                   0x00000200
#define FRGD_SRC_BLIT                   0x00000300
#define FRGD_SRC_PATTERN                0x00000400
#define FRGD_SRC_SCALE                  0x00000500
#define MONO_SRC_ONE                    0x00000000
#define MONO_SRC_PATTERN                0x00010000
#define MONO_SRC_HOST                   0x00020000
#define MONO_SRC_BLIT                   0x00030000

/* CLR_CMP_CNTL */
#define CLR_CMP_FN_FALSE                0x00000000
#define CLR_CMP_FN_TRUE                 0x00000001
#define CLR_CMP_FN_NOT_EQUAL            0x00000004
#define CLR_CMP_FN_EQUAL                0x00000005
#define CLR_CMP_SRC_DEST                0x00000000
#define CLR_CMP_SRC_2D                  0x01000000
#define CLR_CMP_SRC_SCALE               0x02000000

/* GUI_STAT */
#define GUI_ACTIVE                      0x00000001

/* SCALE_3D_CNTL */
#define SCALE_PIX_EXPAND                0x00000001
#define SCALE_DITHER                    0x00000002
#define DITHER_EN                       0x00000004
#define DITHER_INIT                     0x00000008
#define ROUND_EN                        0x00000010
#define TEX_CACHE_DIS                   0x00000020
#define SCALE_3D_FCN_NOP                0x00000000
#define SCALE_3D_FCN_SCALE              0x00000040
#define SCALE_3D_FCN_TEXTURE            0x00000080
#define SCALE_3D_FCN_SHADE              0x000000C0
#define SCALE_PIX_REP                   0x00000100
#define NEAREST_TEX_VIS                 0x00000200
#define TEX_CACHE_SPLIT                 0x00000200
#define APPLE_YUV_MODE                  0x00000400
#define ALPHA_FOG_EN_DIS                0x00000000
#define ALPHA_FOG_EN_ALPHA              0x00000800
#define ALPHA_FOG_EN_FOG                0x00001000
#define COLOR_OVERRIDE                  0x00002000
#define ALPHA_BLND_SAT                  0x00002000
#define RED_DITHER_MAX                  0x00004000
#define SIGNED_DST_CLAMP                0x00008000
#define ALPHA_BLND_SRC_ZERO             0x00000000
#define ALPHA_BLND_SRC_ONE              0x00010000
#define ALPHA_BLND_SRC_DSTCOLOR         0x00020000
#define ALPHA_BLND_SRC_INVDSTCOLOR      0x00030000
#define ALPHA_BLND_SRC_SRCALPHA         0x00040000
#define ALPHA_BLND_SRC_INVSRCALPHA      0x00050000
#define ALPHA_BLND_SRC_DSTALPHA         0x00060000
#define ALPHA_BLND_SRC_INVDSTALPHA      0x00070000
#define ALPHA_BLND_DST_ZERO             0x00000000
#define ALPHA_BLND_DST_ONE              0x00080000
#define ALPHA_BLND_DST_SRCCOLOR         0x00100000
#define ALPHA_BLND_DST_INVSRCCOLOR      0x00180000
#define ALPHA_BLND_DST_SRCALPHA         0x00200000
#define ALPHA_BLND_DST_INVSRCALPHA      0x00280000
#define ALPHA_BLND_DST_DSTALPHA         0x00300000
#define ALPHA_BLND_DST_INVDSTALPHA      0x00380000
#define TEX_LIGHT_FCN_REPLACE           0x00000000
#define TEX_LIGHT_FCN_MODULATE          0x00400000
#define TEX_LIGHT_FCN_ALPHA_DECAL       0x00800000
#define MIP_MAP_DISABLE                 0x01000000
#define BILINEAR_TEX_EN                 0x02000000
#define TEX_BLEND_FCN_NEAREST_MIPMAP_NEAREST    0x00000000
#define TEX_BLEND_FCN_NEAREST_MIPMAP_LINEAR     0x04000000
#define TEX_BLEND_FCN_LINEAR_MIPMAP_NEAREST     0x08000000
#define TEX_BLEND_FCN_LINEAR_MIPMAP_LINEAR      0x0C000000
#define TEX_AMASK_AEN                   0x10000000
#define TEX_AMASK_MODE                  0x20000000
#define TEX_MAP_AEN                     0x40000000
#define SRC_3D_SEL                      0x80000000

/* TEX_CNTL */
#define TEX_CACHE_FLUSH                 0x00800000

/* OVERLAY_Y_X_START */
#define OVERLAY_LOCK_START              0x80000000

/* OVERLAY_Y_X_END */
#define OVERLAY_LOCK_END                0x80000000

/* OVERLAY_KEY_CNTL */
#define VIDEO_KEY_FN_FALSE              0x00000000
#define VIDEO_KEY_FN_TRUE               0x00000001
#define VIDEO_KEY_FN_NOT_EQUAL          0x00000004
#define VIDEO_KEY_FN_EQUAL              0x00000005
#define GRAPHICS_KEY_FN_FALSE           0x00000000
#define GRAPHICS_KEY_FN_TRUE            0x00000010
#define GRAPHICS_KEY_FN_NOT_EQUAL       0x00000040
#define GRAPHICS_KEY_FN_EQUAL           0x00000050
#define OVERLAY_CMP_MIX_OR              0x00000000
#define OVERLAY_CMP_MIX_AND             0x00000100

/* OVERLAY_SCALE_CNTL */
/* #define SCALE_PIX_EXPAND                0x00000001 */
#define SCALE_Y2R_TEMP                  0x00000002
#define SCALE_HORZ_MODE                 0x00000004
#define SCALE_VERT_MODE                 0x00000008
#define SCALE_SIGNED_UV                 0x00000010
#define SCALE_GAMMA_SEL                 0x00000060
#define SCALE_BANDWITH                  0x04000000
#define SCALE_DIS_LIMIT                 0x08000000
#define SCALE_CLK_FORCE_ON              0x20000000
#define OVERLAY_EN                      0x40000000
#define SCALE_EN                        0x80000000

/* VIDEO_FORMAT */
#define VIDEO_IN_VYUY422                0x0000000B
#define VIDEO_IN_YVYU422                0x0000000C
#define VIDEO_SIGNED_UV                 0x00000010
#define SCALER_IN_RGB15                 0x00030000
#define SCALER_IN_RGB16                 0x00040000
#define SCALER_IN_RGB32                 0x00060000
#define SCALER_IN_YUV9                  0x00090000
#define SCALER_IN_YUV12                 0x000A0000
#define SCALER_IN_VYUY422               0x000B0000
#define SCALER_IN_YVYU422               0x000C0000

/* CAPTURE_CONFIG */
#define OVL_BUF_MODE_SINGLE             0x00000000
#define OVL_BUF_MODE_DOUBLE             0x10000000
#define OVL_BUF_NEXT_BUF0               0x00000000
#define OVL_BUF_NEXT_BUF1               0x20000000

#endif
