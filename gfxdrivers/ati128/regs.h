/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef REG_RAGE128_H
#define REG_RAGE128_H

#define CLOCK_CNTL_INDEX                0x0008
#define CLOCK_CNTL_DATA                 0x000c
#define BIOS_0_SCRATCH                  0x0010
#define BUS_CNTL                        0x0030
#define GEN_INT_CNTL                    0x0040
#define CRTC_GEN_CNTL                   0x0050
#define CRTC_EXT_CNTL                   0x0054
#define DAC_CNTL                        0x0058
#define I2C_CNTL_1                      0x0094
#define PALETTE_INDEX                   0x00b0
#define PALETTE_DATA                    0x00b4
#define CONFIG_CNTL                     0x00e0
#define GEN_RESET_CNTL                  0x00f0
#define CONFIG_MEMSIZE                  0x00f8
#define MEM_CNTL                        0x0140
#define AGP_BASE                        0x0170
#define AGP_CNTL                        0x0174
#define AGP_APER_OFFSET                 0x0178
#define PCI_GART_PAGE                   0x017c
#define PC_NGUI_MODE                    0x0180
#define PC_NGUI_CTLSTAT                 0x0184
#define MPP_TB_CONFIG                   0x01C0
#define MPP_GP_CONFIG                   0x01C8
#define VIPH_CONTROL                    0x01D0
#define CRTC_H_TOTAL_DISP               0x0200
#define CRTC_H_SYNC_STRT_WID            0x0204
#define CRTC_V_TOTAL_DISP               0x0208
#define CRTC_V_SYNC_STRT_WID            0x020c
#define CRTC_OFFSET                     0x0224
#define CRTC_OFFSET_CNTL                0x0228
#define CRTC_PITCH                      0x022c
#define OVR_CLR                         0x0230
#define OVR_WID_LEFT_RIGHT              0x0234
#define OVR_WID_TOP_BOTTOM              0x0238
#define LVDS_GEN_CNTL                   0x02d0
#define DDA_CONFIG                      0x02e0
#define DDA_ON_OFF                      0x02e4
#define VGA_DDA_CONFIG                  0x02e8
#define VGA_DDA_ON_OFF                  0x02ec
#define OV0_SCALE_CNTL                  0x0420
#define SUBPIC_CNTL                     0x0540
#define PM4_BUFFER_OFFSET               0x0700
#define PM4_BUFFER_CNTL                 0x0704
#define PM4_BUFFER_WM_CNTL              0x0708
#define PM4_BUFFER_DL_RPTR_ADDR         0x070c
#define PM4_BUFFER_DL_RPTR              0x0710
#define PM4_BUFFER_DL_WPTR              0x0714
#define PM4_VC_FPU_SETUP                0x071c
#define PM4_FPU_CNTL                    0x0720
#define PM4_VC_FORMAT                   0x0724
#define PM4_VC_CNTL                     0x0728
#define PM4_VC_I01                      0x072c
#define PM4_VC_VLOFF                    0x0730
#define PM4_VC_VLSIZE                   0x0734
#define PM4_IW_INDOFF                   0x0738
#define PM4_IW_INDSIZE                  0x073c
#define PM4_FPU_FPX0                    0x0740
#define PM4_FPU_FPY0                    0x0744
#define PM4_FPU_FPX1                    0x0748
#define PM4_FPU_FPY1                    0x074c
#define PM4_FPU_FPX2                    0x0750
#define PM4_FPU_FPY2                    0x0754
#define PM4_FPU_FPY3                    0x0758
#define PM4_FPU_FPY4                    0x075c
#define PM4_FPU_FPY5                    0x0760
#define PM4_FPU_FPY6                    0x0764
#define PM4_FPU_FPR                     0x0768
#define PM4_FPU_FPG                     0x076c
#define PM4_FPU_FPB                     0x0770
#define PM4_FPU_FPA                     0x0774
#define PM4_FPU_INTXY0                  0x0780
#define PM4_FPU_INTXY1                  0x0784
#define PM4_FPU_INTXY2                  0x0788
#define PM4_FPU_INTARGB                 0x078c
#define PM4_FPU_FPTWICEAREA             0x0790
#define PM4_FPU_DMAJOR01                0x0794
#define PM4_FPU_DMAJOR12                0x0798
#define PM4_FPU_DMAJOR02                0x079c
#define PM4_FPU_STAT                    0x07a0
#define PM4_STAT                        0x07b8
#define PM4_TEST_CNTL                   0x07d0
#define PM4_MICROCODE_ADDR              0x07d4
#define PM4_MICROCODE_RADDR             0x07d8
#define PM4_MICROCODE_DATAH             0x07dc
#define PM4_MICROCODE_DATAL             0x07e0
#define PM4_CMDFIFO_ADDR                0x07e4
#define PM4_CMDFIFO_DATAH               0x07e8
#define PM4_CMDFIFO_DATAL               0x07ec
#define PM4_BUFFER_ADDR                 0x07f0
#define PM4_BUFFER_DATAH                0x07f4
#define PM4_BUFFER_DATAL                0x07f8
#define PM4_MICRO_CNTL                  0x07fc
#define CAP0_TRIG_CNTL                  0x0950
#define CAP1_TRIG_CNTL                  0x09c0

/******************************************************************************
 *                  GUI Block Memory Mapped Registers                         *
 *                     These registers are FIFOed.                            *
 *****************************************************************************/
#define PM4_FIFO_DATA_EVEN              0x1000
#define PM4_FIFO_DATA_ODD               0x1004

#define DST_OFFSET                      0x1404
#define DST_PITCH                       0x1408
#define DST_WIDTH                       0x140c
#define DST_HEIGHT                      0x1410
#define SRC_X                           0x1414
#define SRC_Y                           0x1418
#define DST_X                           0x141c
#define DST_Y                           0x1420
#define SRC_PITCH_OFFSET                0x1428
#define DST_PITCH_OFFSET                0x142c
#define SRC_Y_X                         0x1434
#define DST_Y_X                         0x1438
#define DST_HEIGHT_WIDTH                0x143c
#define DP_GUI_MASTER_CNTL              0x146c
#define BRUSH_SCALE                     0x1470
#define BRUSH_Y_X                       0x1474
#define DP_BRUSH_BKGD_CLR               0x1478
#define DP_BRUSH_FRGD_CLR               0x147c
#define DST_WIDTH_X                     0x1588
#define DST_HEIGHT_WIDTH_8              0x158c
#define SRC_X_Y                         0x1590
#define DST_X_Y                         0x1594
#define DST_WIDTH_HEIGHT                0x1598
#define DST_WIDTH_X_INCY                0x159c
#define DST_HEIGHT_Y                    0x15a0
#define DST_X_SUB                       0x15a4
#define DST_Y_SUB                       0x15a8
#define SRC_OFFSET                      0x15ac
#define SRC_PITCH                       0x15b0
#define DST_HEIGHT_WIDTH_BW             0x15b4
#define CLR_CMP_CNTL                    0x15c0
#define CLR_CMP_CLR_SRC                 0x15c4
#define CLR_CMP_CLR_DST                 0x15c8
#define CLR_CMP_MASK                    0x15cc
#define DP_SRC_FRGD_CLR                 0x15d8
#define DP_SRC_BKGD_CLR                 0x15dc
#define DST_BRES_ERR                    0x1628
#define DST_BRES_INC                    0x162c
#define DST_BRES_DEC                    0x1630
#define DST_BRES_LNTH                   0x1634
#define DST_BRES_LNTH_SUB               0x1638
#define SC_LEFT                         0x1640
#define SC_RIGHT                        0x1644
#define SC_TOP                          0x1648
#define SC_BOTTOM                       0x164c
#define SRC_SC_RIGHT                    0x1654
#define SRC_SC_BOTTOM                   0x165c
#define GUI_DEBUG0                      0x16a0
#define GUI_DEBUG1                      0x16a4
#define GUI_TIMEOUT                     0x16b0
#define GUI_TIMEOUT0                    0x16b4
#define GUI_TIMEOUT1                    0x16b8
#define GUI_PROBE                       0x16bc
#define DP_CNTL                         0x16c0
#define DP_DATATYPE                     0x16c4
#define DP_MIX                          0x16c8
#define DP_WRITE_MASK                   0x16cc
#define DP_CNTL_XDIR_YDIR_YMAJOR        0x16d0
#define DEFAULT_OFFSET                  0x16e0
#define DEFAULT_PITCH                   0x16e4
#define DEFAULT_SC_BOTTOM_RIGHT         0x16e8
#define SC_TOP_LEFT                     0x16ec
#define SC_BOTTOM_RIGHT                 0x16f0
#define SRC_SC_BOTTOM_RIGHT             0x16f4
#define WAIT_UNTIL                      0x1720
#define CACHE_CNTL                      0x1724
#define GUI_STAT                        0x1740
#define PC_GUI_MODE                     0x1744
#define PC_GUI_CTLSTAT                  0x1748
#define PC_DEBUG_MODE                   0x1760
#define BRES_DST_ERR_DEC                0x1780
#define TRAIL_BRES_T12_ERR_DEC          0x1784
#define TRAIL_BRES_T12_INC              0x1788
#define DP_T12_CNTL                     0x178c
#define DST_BRES_T1_LNTH                0x1790
#define DST_BRES_T2_LNTH                0x1794
#define SCALE_SRC_HEIGHT_WIDTH          0x1994
#define SCALE_OFFSET_0                  0x1998
#define SCALE_PITCH                     0x199c
#define SCALE_X_INC                     0x19a0
#define SCALE_Y_INC                     0x19a4
#define SCALE_HACC                      0x19a8
#define SCALE_VACC                      0x19ac
#define SCALE_DST_X_Y                   0x19b0
#define SCALE_DST_HEIGHT_WIDTH          0x19b4
#define SCALE_3D_CNTL                   0x1a00
#define SCALE_3D_DATATYPE               0x1a20
#define SETUP_CNTL                      0x1bc4
#define SOLID_COLOR                     0x1bc8
#define WINDOW_XY_OFFSET                0x1bcc
#define DRAW_LINE_POINT                 0x1bd0
#define SETUP_CNTL_PM4                  0x1bd4
#define DST_PITCH_OFFSET_C              0x1c80
#define DP_GUI_MASTER_CNTL_C            0x1c84
#define SC_TOP_LEFT_C                   0x1c88
#define SC_BOTTOM_RIGHT_C               0x1c8c

#define CLR_CMP_MASK_3D                 0x1A28
#define MISC_3D_STATE_CNTL_REG          0x1CA0
#define MC_SRC1_CNTL                    0x19D8
#define TEX_CNTL                        0x1800

/* CONSTANTS */
#define ENG_3D_BUSY                     0x02000000
#define GUI_ACTIVE                      0x80000000


#define ENGINE_IDLE                     0x0

#define PLL_WR_EN                       0x00000080

#define CLK_PIN_CNTL                    0x0001
#define PPLL_CNTL                       0x0002
#define PPLL_REF_DIV                    0x0003
#define PPLL_DIV_0                      0x0004
#define PPLL_DIV_1                      0x0005
#define PPLL_DIV_2                      0x0006
#define PPLL_DIV_3                      0x0007
#define VCLK_ECP_CNTL                   0x0008
#define HTOTAL_CNTL                     0x0009
#define X_MPLL_REF_FB_DIV               0x000a
#define XPLL_CNTL                       0x000b
#define XDLL_CNTL                       0x000c
#define XCLK_CNTL                       0x000d
#define MPLL_CNTL                       0x000e
#define MCLK_CNTL                       0x000f
#define AGP_PLL_CNTL                    0x0010
#define FCP_CNTL                        0x0012
#define PLL_TEST_CNTL                   0x0013

#define PPLL_RESET                      0x01
#define PPLL_ATOMIC_UPDATE_EN           0x10000
#define PPLL_VGA_ATOMIC_UPDATE_EN       0x20000
#define PPLL_REF_DIV_MASK               0x3FF
#define PPLL_FB3_DIV_MASK               0x7FF
#define PPLL_POST3_DIV_MASK             0x70000
#define PPLL_ATOMIC_UPDATE_R            0x8000
#define PPLL_ATOMIC_UPDATE_W            0x8000
#define MEM_CFG_TYPE_MASK               0x3
#define XCLK_SRC_SEL_MASK               0x7
#define XPLL_FB_DIV_MASK                0xFF00
#define X_MPLL_REF_DIV_MASK             0xFF

/* CRTC control values (CRTC_GEN_CNTL) */
#define CRTC_CSYNC_EN                   0x00000010

#define CRTC_PIX_WIDTH_MASK             0x00000700
#define CRTC_PIX_WIDTH_4BPP             0x00000100
#define CRTC_PIX_WIDTH_8BPP             0x00000200
#define CRTC_PIX_WIDTH_15BPP            0x00000300
#define CRTC_PIX_WIDTH_16BPP            0x00000400
#define CRTC_PIX_WIDTH_24BPP            0x00000500
#define CRTC_PIX_WIDTH_32BPP            0x00000600

/* DAC_CNTL bit constants */
#define DAC_8BIT_EN                     0x00000100
#define DAC_MASK                        0xFF000000
#define DAC_BLANKING                    0x00000004
#define DAC_RANGE_CNTL                  0x00000003
#define DAC_RANGE_CNTL                  0x00000003
#define DAC_PALETTE_ACCESS_CNTL         0x00000020
#define DAC_PDWN                        0x00008000

/* GEN_RESET_CNTL bit constants */
#define SOFT_RESET_GUI                  0x00000001
#define SOFT_RESET_VCLK                 0x00000100
#define SOFT_RESET_PCLK                 0x00000200
#define SOFT_RESET_ECP                  0x00000400
#define SOFT_RESET_DISPENG_XCLK         0x00000800

/* PC_GUI_CTLSTAT bit constants */
#define PC_BUSY_INIT                    0x10000000
#define PC_BUSY_GUI                     0x20000000
#define PC_BUSY_NGUI                    0x40000000
#define PC_BUSY                         0x80000000

#define BUS_MASTER_DIS                  0x00000040
#define PM4_BUFFER_CNTL_NONPM4          0x00000000

/* DP_DATATYPE bit constants */
#define DST_8BPP                        0x00000002
#define DST_15BPP                       0x00000003
#define DST_16BPP                       0x00000004
#define DST_24BPP                       0x00000005
#define DST_32BPP                       0x00000006
#define DST_8BPP_RGB332                 0x00000007
#define DST_8BPP_Y8                     0x00000008
#define DST_8BPP_RGB8                   0x00000009
#define DST_16BPP_VYUY422               0x0000000b
#define DST_16BPP_YVYU422               0x0000000c
#define DST_32BPP_AYUV444               0x0000000e
#define DST_16BPP_ARGB4444              0x0000000f
#define BRUSH_8x8MONO                   0x00000000
#define BRUSH_8x8MONO_LBKGD             0x00000100
#define BRUSH_8x1MONO                   0x00000200
#define BRUSH_8x1MONO_LBKGD             0x00000300
#define BRUSH_1x8MONO                   0x00000400
#define BRUSH_1x8MONO_LBKGD             0x00000500
#define BRUSH_32x1MONO                  0x00000600
#define BRUSH_32x1MONO_LBKGD            0x00000700
#define BRUSH_32x32MONO                 0x00000800
#define BRUSH_32x32MONO_LBKGD           0x00000900
#define BRUSH_8x8COLOR                  0x00000a00
#define BRUSH_8x1COLOR                  0x00000b00
#define BRUSH_1x8COLOR                  0x00000c00
#define BRUSH_SOLIDCOLOR                0x00000d00
#define SRC_MONO                        0x00000000
#define SRC_MONO_LBKGD                  0x00010000
#define SRC_DSTCOLOR                    0x00030000
#define BYTE_ORDER_MSB_TO_LSB           0x00000000
#define BYTE_ORDER_LSB_TO_MSB           0x40000000
#define DP_CONVERSION_TEMP              0x80000000

/* DP_GUI_MASTER_CNTL bit constants */
#define GMC_SRC_PITCH_OFFSET_DEFAULT    0x00000000
#define GMC_DST_PITCH_OFFSET_DEFAULT    0x00000000
#define GMC_SRC_CLIP_DEFAULT            0x00000000
#define GMC_DST_CLIP_DEFAULT            0x00000000
#define GMC_BRUSH_SOLIDCOLOR            0x000000d0
#define GMC_SRC_DSTCOLOR                0x00003000
#define GMC_BYTE_ORDER_MSB_TO_LSB       0x00000000
#define GMC_DP_SRC_RECT                 0x02000000
#define GMC_3D_FCN_EN_CLR               0x00000000
#define GMC_AUX_CLIP_CLEAR              0x20000000
#define GMC_DST_CLR_CMP_FCN_CLEAR       0x10000000
#define GMC_WRITE_MASK_SET              0x40000000
#define GMC_DP_CONVERSION_TEMP_6500     0x00000000

/* DP_GUI_MASTER_CNTL ROP3 named constants */
#define ROP3_PATCOPY                    0x00f00000
#define ROP3_SRCCOPY                    0x00cc0000

#define SRC_DSTCOLOR                    0x00030000

/* DP_CNTL bit constants */
#define DST_X_RIGHT_TO_LEFT             0x00000000
#define DST_X_LEFT_TO_RIGHT             0x00000001
#define DST_Y_BOTTOM_TO_TOP             0x00000000
#define DST_Y_TOP_TO_BOTTOM             0x00000002
#define DST_X_MAJOR                     0x00000000
#define DST_Y_MAJOR                     0x00000004
#define DST_X_TILE                      0x00000008
#define DST_Y_TILE                      0x00000010
#define DST_LAST_PEL                    0x00000020
#define DST_TRAIL_X_RIGHT_TO_LEFT       0x00000000
#define DST_TRAIL_X_LEFT_TO_RIGHT       0x00000040
#define DST_TRAP_FILL_RIGHT_TO_LEFT     0x00000000
#define DST_TRAP_FILL_LEFT_TO_RIGHT     0x00000080
#define DST_BRES_SIGN                   0x00000100
#define DST_HOST_BIG_ENDIAN_EN          0x00000200
#define DST_POLYLINE_NONLAST            0x00008000
#define DST_RASTER_STALL                0x00010000
#define DST_POLY_EDGE                   0x00040000

/* DP_MIX bit constants */
#define DP_SRC_RECT                     0x00000200
#define DP_SRC_HOST                     0x00000300
#define DP_SRC_HOST_BYTEALIGN           0x00000400

/* LVDS_GEN_CNTL constants */
#define LVDS_BL_MOD_LEVEL_MASK          0x0000ff00
#define LVDS_BL_MOD_LEVEL_SHIFT         0x8
#define LVDS_BL_MOD_EN                  0x00010000
#define LVDS_DIGION                     0x00040000
#define LVDS_BLON                       0x00080000


/* from the ati128ddk */

#define FOG_3D_TABLE_START              0x1810
#define FOG_3D_TABLE_END                0x1814
#define FOG_3D_TABLE_DENSITY            0x181c

#define FOG_TABLE_INDEX                 0x1a14
#define FOG_TABLE_DATA                  0x1a18

/* MISC_3D_STATE */
#define MISC_3D_STATE_SCALE_3D_FN_NOP                         (0x00000000 << 8)
#define MISC_3D_STATE_SCALE_3D_FN_SCALE                       (0x00000001 << 8)
#define MISC_3D_STATE_SCALE_3D_FN_TMAP_SHADE                  (0x00000002 << 8)
#define MISC_3D_STATE_SCALE_PIX_REP_BLEND                     (0x00000000 << 10)
#define MISC_3D_STATE_SCALE_PIX_REP_REPLICATE                 (0x00000001 << 10)
#define MISC_3D_STATE_ALPHA_COMB_FNC_ADD_CLAMP                (0x00000000 << 12)
#define MISC_3D_STATE_ALPHA_COMB_FNC_ADD_NO_CLAMP             (0x00000001 << 12)
#define MISC_3D_STATE_ALPHA_COMB_FNC_SUB_SRC_DST_CLAMP        (0x00000002 << 12)
#define MISC_3D_STATE_ALPHA_COMB_FNC_SUB_SRC_DST_NO_CLAMP     (0x00000003 << 12)
#define MISC_3D_STATE_FOG_TABLE_EN_VERTEX_FOG                 (0x00000000 << 14)
#define MISC_3D_STATE_FOG_TABLE_EN_TABLE_FOG                  (0x00000001 << 14)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_ZERO                    (0x00000000 << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_ONE                     (0x00000001 << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_SRCCOLOR                (0x00000002 << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_INVSRCCOLOR             (0x00000003 << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_SRCALPHA                (0x00000004 << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_INVSRCALPHA             (0x00000005 << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_DESTALPHA               (0x00000006 << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_INVDESTALPHA            (0x00000007 << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_DESTCOLOR               (0x00000008 << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_INVDESTCOLOR            (0x00000009 << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_SRCALPHASAT             (0x0000000a << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_BOTHSRCALPHA            (0x0000000b << 16)
#define MISC_3D_STATE_ALPHA_BLEND_SRC_BOTHINVSRCALPHA         (0x0000000c << 16)
#define MISC_3D_STATE_ALPHA_BLEND_DST_ZERO                    (0x00000000 << 20)
#define MISC_3D_STATE_ALPHA_BLEND_DST_ONE                     (0x00000001 << 20)
#define MISC_3D_STATE_ALPHA_BLEND_DST_SRCCOLOR                (0x00000002 << 20)
#define MISC_3D_STATE_ALPHA_BLEND_DST_INVSRCCOLOR             (0x00000003 << 20)
#define MISC_3D_STATE_ALPHA_BLEND_DST_SRCALPHA                (0x00000004 << 20)
#define MISC_3D_STATE_ALPHA_BLEND_DST_INVSRCALPHA             (0x00000005 << 20)
#define MISC_3D_STATE_ALPHA_BLEND_DST_DESTALPHA               (0x00000006 << 20)
#define MISC_3D_STATE_ALPHA_BLEND_DST_INVDESTALPHA            (0x00000007 << 20)
#define MISC_3D_STATE_ALPHA_BLEND_DST_DESTCOLOR               (0x00000008 << 20)
#define MISC_3D_STATE_ALPHA_BLEND_DST_INVDESTCOLOR            (0x00000009 << 20)
#define MISC_3D_STATE_ALPHA_BLEND_DST_SRCALPHASAT             (0x0000000a << 20)
#define MISC_3D_STATE_ALPHA_TEST_OP_NEVER                     (0x00000000 << 24)
#define MISC_3D_STATE_ALPHA_TEST_OP_LESS                      (0x00000001 << 24)
#define MISC_3D_STATE_ALPHA_TEST_OP_LESSEQUAL                 (0x00000002 << 24)
#define MISC_3D_STATE_ALPHA_TEST_OP_EQUAL                     (0x00000003 << 24)
#define MISC_3D_STATE_ALPHA_TEST_OP_GREATEREQUAL              (0x00000004 << 24)
#define MISC_3D_STATE_ALPHA_TEST_OP_GREATER                   (0x00000005 << 24)
#define MISC_3D_STATE_ALPHA_TEST_OP_NEQUAL                    (0x00000006 << 24)
#define MISC_3D_STATE_ALPHA_TEST_OP_ALWAYS                    (0x00000007 << 24)



/* Z_STEN_CNTL */
#define Z_STEN_CNTL_Z_PIX_WIDTH_16                            (0x00000000 <<  1)
#define Z_STEN_CNTL_Z_PIX_WIDTH_24                            (0x00000001 <<  1)
#define Z_STEN_CNTL_Z_PIX_WIDTH_32                            (0x00000002 <<  1)
#define Z_STEN_CNTL_Z_TEST_NEVER                              (0x00000000 <<  4)
#define Z_STEN_CNTL_Z_TEST_LESS                               (0x00000001 <<  4)
#define Z_STEN_CNTL_Z_TEST_LESSEQUAL                          (0x00000002 <<  4)
#define Z_STEN_CNTL_Z_TEST_EQUAL                              (0x00000003 <<  4)
#define Z_STEN_CNTL_Z_TEST_GREATEREQUAL                       (0x00000004 <<  4)
#define Z_STEN_CNTL_Z_TEST_GREATER                            (0x00000005 <<  4)
#define Z_STEN_CNTL_Z_TEST_NEQUAL                             (0x00000006 <<  4)
#define Z_STEN_CNTL_Z_TEST_ALWAYS                             (0x00000007 <<  4)
#define Z_STEN_CNTL_STENCIL_TEST_NEVER                        (0x00000000 << 12)
#define Z_STEN_CNTL_STENCIL_TEST_LESS                         (0x00000001 << 12)
#define Z_STEN_CNTL_STENCIL_TEST_LESSEQUAL                    (0x00000002 << 12)
#define Z_STEN_CNTL_STENCIL_TEST_EQUAL                        (0x00000003 << 12)
#define Z_STEN_CNTL_STENCIL_TEST_GREATEREQUAL                 (0x00000004 << 12)
#define Z_STEN_CNTL_STENCIL_TEST_GREATER                      (0x00000005 << 12)
#define Z_STEN_CNTL_STENCIL_TEST_NEQUAL                       (0x00000006 << 12)
#define Z_STEN_CNTL_STENCIL_TEST_ALWAYS                       (0x00000007 << 12)
#define Z_STEN_CNTL_STENCIL_S_FAIL_OP_KEEP                    (0x00000000 << 16)
#define Z_STEN_CNTL_STENCIL_S_FAIL_OP_ZERO                    (0x00000001 << 16)
#define Z_STEN_CNTL_STENCIL_S_FAIL_OP_REPLACE                 (0x00000002 << 16)
#define Z_STEN_CNTL_STENCIL_S_FAIL_OP_INC                     (0x00000003 << 16)
#define Z_STEN_CNTL_STENCIL_S_FAIL_OP_DEC                     (0x00000004 << 16)
#define Z_STEN_CNTL_STENCIL_S_FAIL_OP_INV                     (0x00000005 << 16)
#define Z_STEN_CNTL_STENCIL_ZPASS_OP_KEEP                     (0x00000000 << 20)
#define Z_STEN_CNTL_STENCIL_ZPASS_OP_ZERO                     (0x00000001 << 20)
#define Z_STEN_CNTL_STENCIL_ZPASS_OP_REPLACE                  (0x00000002 << 20)
#define Z_STEN_CNTL_STENCIL_ZPASS_OP_INC                      (0x00000003 << 20)
#define Z_STEN_CNTL_STENCIL_ZPASS_OP_DEC                      (0x00000004 << 20)
#define Z_STEN_CNTL_STENCIL_ZPASS_OP_INV                      (0x00000005 << 20)
#define Z_STEN_CNTL_STENCIL_ZFAIL_OP_KEEP                     (0x00000000 << 24)
#define Z_STEN_CNTL_STENCIL_ZFAIL_OP_ZERO                     (0x00000001 << 24)
#define Z_STEN_CNTL_STENCIL_ZFAIL_OP_REPLACE                  (0x00000002 << 24)
#define Z_STEN_CNTL_STENCIL_ZFAIL_OP_INC                      (0x00000003 << 24)
#define Z_STEN_CNTL_STENCIL_ZFAIL_OP_DEC                      (0x00000004 << 24)
#define Z_STEN_CNTL_STENCIL_ZFAIL_OP_INV                      (0x00000005 << 24)

/* TEX_CNTL */
#define TEX_CNTL_Z_EN_OFF                                     (0x00000000 <<  0)
#define TEX_CNTL_Z_EN_ON                                      (0x00000001 <<  0)
#define TEX_CNTL_Z_MASK_DIS                                   (0x00000000 <<  1)
#define TEX_CNTL_Z_MASK_EN                                    (0x00000001 <<  1)
#define TEX_CNTL_STENCIL_EN_OFF                               (0x00000000 <<  3)
#define TEX_CNTL_STENCIL_EN_ON                                (0x00000001 <<  3)
#define TEX_CNTL_TEX_EN_SHADE                                 (0x00000000 <<  4)
#define TEX_CNTL_TEX_EN_TMAP                                  (0x00000001 <<  4)
#define TEX_CNTL_SECONDARY_TEX_EN_OFF                         (0x00000000 <<  5)
#define TEX_CNTL_SECONDARY_TEX_EN_ON                          (0x00000001 <<  5)
#define TEX_CNTL_FOG_EN_OFF                                   (0x00000000 <<  7)
#define TEX_CNTL_FOG_EN_ON                                    (0x00000001 <<  7)
#define TEX_CNTL_DITHRE_EN_OFF                                (0x00000000 <<  8)
#define TEX_CNTL_DITHRE_EN_ON                                 (0x00000001 <<  8)
#define TEX_CNTL_ALPHA_EN_OFF                                 (0x00000000 <<  9)
#define TEX_CNTL_ALPHA_EN_ON                                  (0x00000001 <<  9)
#define TEX_CNTL_ALPHA_TEST_EN_OFF                            (0x00000000 << 10)
#define TEX_CNTL_ALPHA_TEST_EN_ON                             (0x00000001 << 10)
#define TEX_CNTL_SPEC_LIGHT_EN_OFF                            (0x00000000 << 11)
#define TEX_CNTL_SPEC_LIGHT_EN_ON                             (0x00000001 << 11)
#define TEX_CNTL_TEX_CHROMA_KEY_EN_OFF                        (0x00000000 << 12)
#define TEX_CNTL_TEX_CHROMA_KEY_EN_ON                         (0x00000001 << 12)
#define TEX_CNTL_AMASK_EN_OFF                                 (0x00000000 << 13)
#define TEX_CNTL_AMASK_EN_ON                                  (0x00000001 << 13)
#define TEX_CNTL_LIGHT_FN_DIS                                 (0x00000000 << 14)
#define TEX_CNTL_LIGHT_FN_COPY                                (0x00000001 << 14)
#define TEX_CNTL_LIGHT_FN_MODULATE                            (0x00000002 << 14)
#define TEX_CNTL_LIGHT_FN_ADD                                 (0x00000003 << 14)
#define TEX_CNTL_LIGHT_FN_BLEND_CONSTANT                      (0x00000004 << 14)
#define TEX_CNTL_LIGHT_FN_BLEND_TEXTURE                       (0x00000005 << 14)
#define TEX_CNTL_LIGHT_FN_BLEND_VERTEX                        (0x00000006 << 14)
#define TEX_CNTL_LIGHT_FN_BLEND_CONST_COLOR                   (0x00000007 << 14)
#define TEX_CNTL_ALPHA_LIGHT_FN_DIS                           (0x00000000 << 18)
#define TEX_CNTL_ALPHA_LIGHT_FN_COPY                          (0x00000001 << 18)
#define TEX_CNTL_ALPHA_LIGHT_FN_MODULATE                      (0x00000002 << 18)
#define TEX_CNTL_ALPHA_LIGHT_FN_ADD                           (0x00000003 << 18)
//#define TEX_CNTL_ANTI_ALIAS_FN
#define TEX_CNTL_TEX_CACHE_FLUSH_OFF                          (0x00000000 << 23)
#define TEX_CNTL_TEX_CACHE_FLUSH_ON                           (0x00000001 << 23)
//#define TEX_CNTL_LOD_BIAS


/* PRIM_TEX_CNTL */
#define PRIM_TEX_CNTL_PRIM_MIN_BLEND_FN_NEAREST               (0x00000000 <<  0)
#define PRIM_TEX_CNTL_PRIM_MIN_BLEND_FN_LINEAR                (0x00000001 <<  0)
#define PRIM_TEX_CNTL_PRIM_MIN_BLEND_FN_MIPNEAREST            (0x00000002 <<  0)
#define PRIM_TEX_CNTL_PRIM_MIN_BLEND_FN_MIPLINEAR             (0x00000003 <<  0)
#define PRIM_TEX_CNTL_PRIM_MIN_BLEND_FN_LINEARMIPNEAREST      (0x00000004 <<  0)
#define PRIM_TEX_CNTL_PRIM_MIN_BLEND_FN_LINEARMIPLINEAR       (0x00000005 <<  0)
#define PRIM_TEX_CNTL_PRIM_MAG_BLEND_FN_NEAREST               (0x00000000 <<  4)
#define PRIM_TEX_CNTL_PRIM_MAG_BLEND_FN_LINEAR                (0x00000001 <<  4)
#define PRIM_TEX_CNTL_MIP_MAP_DIS_OFF                         (0x00000000 <<  7)
#define PRIM_TEX_CNTL_MIP_MAP_DIS_ON                          (0x00000001 <<  7)
#define PRIM_TEX_CNTL_PRIM_TEX_CLAMP_MODE_S_WRAP              (0x00000000 <<  8)
#define PRIM_TEX_CNTL_PRIM_TEX_CLAMP_MODE_S_MIRROR            (0x00000001 <<  8)
#define PRIM_TEX_CNTL_PRIM_TEX_CLAMP_MODE_S_CLAMP             (0x00000002 <<  8)
#define PRIM_TEX_CNTL_PRIM_TEX_CLAMP_MODE_S_BORDER_COLOR      (0x00000003 <<  8)
#define PRIM_TEX_CNTL_PRIM_TEX_WRAP_S_OFF                     (0x00000000 << 10)
#define PRIM_TEX_CNTL_PRIM_TEX_WRAP_S_ON                      (0x00000001 << 10)
#define PRIM_TEX_CNTL_PRIM_TEX_CLAMP_MODE_T_WRAP              (0x00000000 << 11)
#define PRIM_TEX_CNTL_PRIM_TEX_CLAMP_MODE_T_MIRROR            (0x00000001 << 11)
#define PRIM_TEX_CNTL_PRIM_TEX_CLAMP_MODE_T_CLAMP             (0x00000002 << 11)
#define PRIM_TEX_CNTL_PRIM_TEX_CLAMP_MODE_T_BORDER_COLOR      (0x00000003 << 11)
#define PRIM_TEX_CNTL_PRIM_TEX_WRAP_T_OFF                     (0x00000000 << 13)
#define PRIM_TEX_CNTL_PRIM_TEX_WRAP_T_ON                      (0x00000001 << 13)
#define PRIM_TEX_CNTL_PRIM_TEX_PERSPECTIVE_DIS_OFF            (0x00000000 << 14)
#define PRIM_TEX_CNTL_PRIM_TEX_PERSPECTIVE_DIS_ON             (0x00000001 << 14)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_VQ                        (0x00000000 << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_CI4                       (0x00000001 << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_CI8                       (0x00000002 << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_ARGB1555                  (0x00000003 << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_RGB565                    (0x00000004 << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_RGB888                    (0x00000005 << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_ARGB8888                  (0x00000006 << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_RGB332                    (0x00000007 << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_Y8                        (0x00000008 << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_RGB8                      (0x00000009 << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_CI16                      (0x0000000a << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_YUV422                    (0x0000000b << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_YUV422_2                  (0x0000000c << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_AYUV444                   (0x0000000d << 16)
#define PRIM_TEX_CNTL_PRIM_DATATYPE_ARGB4444                  (0x0000000e << 16)
//#define PRIM_TEX_CNTL_PRIM_PALETTE_OFF_
//#define PRIM_TEX_CNTL_PRIM_PSEUDOCOLOR_DATATYPE_


/* SETP_CNTL */
#define SETUP_CNTL_DONT_START_TRI_OFF                         (0x00000000 <<  0)
#define SETUP_CNTL_DONT_START_TRI_ON                          (0x00000001 <<  0)
#define SETUP_CNTL_Z_BIAS                                     (0x00000000 <<  1)
#define SETUP_CNTL_DONT_START_ANY_OFF                         (0x00000000 <<  2)
#define SETUP_CNTL_DONT_START_ANY_ON                          (0x00000001 <<  2)
#define SETUP_CNTL_COLOR_FNC_SOLID_COLOR                      (0x00000000 <<  3)
#define SETUP_CNTL_COLOR_FNC_FLAT_VERT_1                      (0x00000001 <<  3)
#define SETUP_CNTL_COLOR_FNC_FLAT_VERT_2                      (0x00000002 <<  3)
#define SETUP_CNTL_COLOR_FNC_FLAT_VERT_3                      (0x00000003 <<  3)
#define SETUP_CNTL_COLOR_FNC_GOURAUD                          (0x00000004 <<  3)
#define SETUP_CNTL_PRIM_TYPE_SELECT_TRI                       (0x00000000 <<  7)
#define SETUP_CNTL_PRIM_TYPE_SELECT_LINE                      (0x00000001 <<  7)
#define SETUP_CNTL_PRIM_TYPE_SELECT_POINT                     (0x00000002 <<  7)
#define SETUP_CNTL_PRIM_TYPE_SELECT_POLY_EDGE                 (0x00000003 <<  7)
#define SETUP_CNTL_TEXTURE_ST_FORMAT_MULT_W                   (0x00000000 <<  9)
#define SETUP_CNTL_TEXTURE_ST_FORMAT_DIRECT                   (0x00000001 <<  9)
#define SETUP_CNTL_STARTING_VERTEX_SELECT_1                   (0x00000001 << 14)
#define SETUP_CNTL_STARTING_VERTEX_SELECT_2                   (0x00000002 << 14)
#define SETUP_CNTL_STARTING_VERTEX_SELECT_3                   (0x00000003 << 14)
#define SETUP_CNTL_ENDING_VERTEX_SELECT_1                     (0x00000001 << 16)
#define SETUP_CNTL_ENDING_VERTEX_SELECT_2                     (0x00000002 << 16)
#define SETUP_CNTL_ENDING_VERTEX_SELECT_3                     (0x00000003 << 16)
#define SETUP_CNTL_SU_POLY_LINE_LAST                          (0x00000000 << 18)
#define SETUP_CNTL_SU_POLY_LINE_NOT_LAST                      (0x00000001 << 18)
#define SETUP_CNTL_SUB_PIX_AMOUNT_2BITS                       (0x00000000 << 19)
#define SETUP_CNTL_SUB_PIX_AMOUNT_4BITS                       (0x00000001 << 19)
//#define SETUP_CNTL_SU_POLY_EDGE
//#define SETUP_CNTL_SU_EDGE_DST_Y_MAJOR
//#define SETUP_CNTL_SU_STATE
#define SETUP_CNTL_SET_UP_CONTINUE                            (0x00000001 << 31)

/* PM4_VC_FPU_SETUP */
#define PM4_VC_FPU_SETUP_FRONT_DIR_CW                         (0x00000000 <<  0)
#define PM4_VC_FPU_SETUP_FRONT_DIR_CCW                        (0x00000001 <<  0)
#define PM4_VC_FPU_SETUP_BACKFACE_CULLING_FN_CULL             (0x00000000 <<  1)
#define PM4_VC_FPU_SETUP_BACKFACE_CULLING_FN_POINT            (0x00000001 <<  1)
#define PM4_VC_FPU_SETUP_BACKFACE_CULLING_FN_LINE             (0x00000002 <<  1)
#define PM4_VC_FPU_SETUP_BACKFACE_CULLING_FN_REV_SOLID        (0x00000003 <<  1)
#define PM4_VC_FPU_SETUP_FRONTFACE_CULLING_FN_CULL            (0x00000000 <<  3)
#define PM4_VC_FPU_SETUP_FRONTFACE_CULLING_FN_POINT           (0x00000001 <<  3)
#define PM4_VC_FPU_SETUP_FRONTFACE_CULLING_FN_LINE            (0x00000002 <<  3)
#define PM4_VC_FPU_SETUP_FRONTFACE_CULLING_FN_REV_SOLID       (0x00000003 <<  3)
#define PM4_VC_FPU_SETUP_PM4_COLOR_FCN_SOLID                  (0x00000000 <<  5)
#define PM4_VC_FPU_SETUP_PM4_COLOR_FCN_FLAT                   (0x00000001 <<  5)
#define PM4_VC_FPU_SETUP_PM4_COLOR_FCN_GOURAUD                (0x00000002 <<  5)
#define PM4_VC_FPU_SETUP_PM4_COLOR_FCN_GOURAUD2               (0x00000003 <<  5)
#define PM4_VC_FPU_SETUP_PM4_SUB_PIX_AMOUNT_2BITS             (0x00000000 <<  7)
#define PM4_VC_FPU_SETUP_PM4_SUB_PIX_AMOUNT_4BITS             (0x00000001 <<  7)
#define PM4_VC_FPU_SETUP_FPU_MODE_2D                          (0x00000000 <<  8)
#define PM4_VC_FPU_SETUP_FPU_MODE_3D                          (0x00000001 <<  8)
#define PM4_VC_FPU_SETUP_TRAP_DISABLE_OFF                     (0x00000000 <<  9)
#define PM4_VC_FPU_SETUP_TRAP_DISABLE_ON                      (0x00000001 <<  9)
#define PM4_VC_FPU_SETUP_EDGE_ANTIALIAS_OFF                   (0x00000000 << 10)
#define PM4_VC_FPU_SETUP_EDGE_ANTIALIAS_ON                    (0x00000001 << 10)
#define PM4_VC_FPU_SETUP_SUPERSAMPLE_OFF                      (0x00000000 << 11)
#define PM4_VC_FPU_SETUP_SUPERSAMPLE_ON                       (0x00000001 << 11)
#define PM4_VC_FPU_SETUP_XFACTOR_2                            (0x00000000 << 12)
#define PM4_VC_FPU_SETUP_XFACTOR_4                            (0x00000001 << 12)
#define PM4_VC_FPU_SETUP_YFACTOR_2                            (0x00000000 << 13)
#define PM4_VC_FPU_SETUP_YFACTOR_4                            (0x00000001 << 13)
#define PM4_VC_FPU_SETUP_FLAT_SHADE_VERTEX_D3D                (0x00000000 << 14)
#define PM4_VC_FPU_SETUP_FLAT_SHADE_VERTEX_OPENGL             (0x00000001 << 14)
#define PM4_VC_FPU_SETUP_FPU_ROUND_EN_OFF                     (0x00000000 << 15)
#define PM4_VC_FPU_SETUP_FPU_ROUND_EN_ON                      (0x00000001 << 15)
#define PM4_VC_FPU_SETUP_VC_WM_SEL_8DW                        (0x00000000 << 16)
#define PM4_VC_FPU_SETUP_VC_WM_SEL_16DW                       (0x00000001 << 16)
#define PM4_VC_FPU_SETUP_VC_WM_SEL_32DW                       (0x00000002 << 16)

/* SEC_TEX_CNTL */
#define SEC_TEX_CNTL_SEC_SRC_SEL_ST_0                         (0x00000000 <<  0)
#define SEC_TEX_CNTL_SEC_SRC_SEL_ST_1                         (0x00000001 <<  0)

/* [PRIM_ | SEC_] SEC_TEX_COMBINE_CNTL */
#define TEX_COMBINE_CNTL_COMB_FNC_DIS                         (0x00000000 << 0)
#define TEX_COMBINE_CNTL_COMB_FNC_COPY                        (0x00000001 <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_COPY_INP                    (0x00000002 <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_MODULATE                    (0x00000003 <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_MODULATE2X                  (0x00000004 <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_MODULATE4X                  (0x00000005 <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_ADD                         (0x00000006 <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_ADD_SIGNED                  (0x00000007 <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_BLEND_VERTEX                (0x00000008 <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_BLEND_TEXTURE               (0x00000009 <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_BLEND_CONST                 (0x0000000a <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_BLEND_PREMULT               (0x0000000b <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_BLEND_PREV                  (0x0000000c <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_BLEND_PREMULT_INV           (0x0000000d <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_ADD_SIGNED2X                (0x0000000e <<  0)
#define TEX_COMBINE_CNTL_COMB_FNC_BLEND_CONST_COLOR           (0x0000000f <<  0)
#define TEX_COMBINE_CNTL_COLOR_FACTOR_TEX                     (0x00000004 <<  4)
#define TEX_COMBINE_CNTL_COLOR_FACTOR_NTEX                    (0x00000005 <<  4)
#define TEX_COMBINE_CNTL_COLOR_FACTOR_ALPHA                   (0x00000006 <<  4)
#define TEX_COMBINE_CNTL_COLOR_FACTOR_NALPHA                  (0x00000007 <<  4)
#define TEX_COMBINE_CNTL_INPUT_FACTOR_CONST_COLOR             (0x00000002 << 10)
#define TEX_COMBINE_CNTL_INPUT_FACTOR_CONST_ALPHA             (0x00000003 << 10)
#define TEX_COMBINE_CNTL_INPUT_FACTOR_INT_COLOR               (0x00000004 << 10)
#define TEX_COMBINE_CNTL_INPUT_FACTOR_INT_ALPHA               (0x00000005 << 10)
#define TEX_COMBINE_CNTL_INPUT_FACTOR_PREV_COLOR              (0x00000008 << 10)
#define TEX_COMBINE_CNTL_INPUT_FACTOR_PREV_ALPHA              (0x00000009 << 10)
#define TEX_COMBINE_CNTL_COMB_FNC_ALPHA_DIS                   (0x00000000 << 14)
#define TEX_COMBINE_CNTL_COMB_FNC_ALPHA_COPY                  (0x00000001 << 14)
#define TEX_COMBINE_CNTL_COMB_FNC_ALPHA_COPY_INP              (0x00000002 << 14)
#define TEX_COMBINE_CNTL_COMB_FNC_ALPHA_MODULATE              (0x00000003 << 14)
#define TEX_COMBINE_CNTL_COMB_FNC_ALPHA_MODULATE2X            (0x00000004 << 14)
#define TEX_COMBINE_CNTL_COMB_FNC_ALPHA_MODULATE4X            (0x00000005 << 14)
#define TEX_COMBINE_CNTL_COMB_FNC_ALPHA_ADD                   (0x00000006 << 14)
#define TEX_COMBINE_CNTL_COMB_FNC_ALPHA_ADD_SIGNED            (0x00000007 << 14)
#define TEX_COMBINE_CNTL_COMB_FNC_ALPHA_ADD_SIGNED2X          (0x0000000e << 14)
#define TEX_COMBINE_CNTL_ALPHA_FACTOR_TEX_ALPHA               (0x00000006 << 18)
#define TEX_COMBINE_CNTL_ALPHA_FACTOR_NTEX_ALPHA              (0x00000007 << 18)
#define TEX_COMBINE_CNTL_INP_FACTOR_ALPHA_CONST_ALPHA         (0x00000001 << 25)
#define TEX_COMBINE_CNTL_INP_FACTOR_ALPHA_INT_ALPHA           (0x00000002 << 25)
#define TEX_COMBINE_CNTL_INP_FACTOR_ALPHA_PREV_ALPHA          (0x00000004 << 25)


/* SCALE_3D_CNTL */
#define SCALE_3D_CNTL_SCALE_DITHER_ERR_DIFF                   (0x00000000 <<  1)
#define SCALE_3D_CNTL_SCALE_DITHER_TABLE                      (0x00000001 <<  1)
#define SCALE_3D_CNTL_TEX_CACHE_SIZE_FULL                     (0x00000000 <<  2)
#define SCALE_3D_CNTL_TEX_CACHE_SIZE_HALF                     (0x00000001 <<  2)
#define SCALE_3D_CNTL_DITHER_INIT_CURR                        (0x00000000 <<  3)
#define SCALE_3D_CNTL_DITHER_INIT_RESET                       (0x00000001 <<  3)
#define SCALE_3D_CNTL_ROUND_EN_OFF                            (0x00000000 <<  4)
#define SCALE_3D_CNTL_ROUND_EN_ON                             (0x00000001 <<  4)
#define SCALE_3D_CNTL_TEX_CACHE_DIS_OFF                       (0x00000000 <<  5)
#define SCALE_3D_CNTL_TEX_CACHE_DIS_ON                        (0x00000001 <<  5)
#define SCALE_3D_CNTL_SCALE_3D_FN_NONE                        (0x00000000 <<  6)
#define SCALE_3D_CNTL_SCALE_3D_FN_SCALE                       (0x00000001 <<  6)
#define SCALE_3D_CNTL_SCALE_3D_FN_TMAP_SHADE                  (0x00000002 <<  6)
#define SCALE_3D_CNTL_SCALE_PIX_REP_BLEND                     (0x00000000 <<  8)
#define SCALE_3D_CNTL_SCALE_PIX_REP_REP                       (0x00000001 <<  8)
#define SCALE_3D_CNTL_TEX_CACHE_SPLIT_OFF                     (0x00000000 <<  9)
#define SCALE_3D_CNTL_TEX_CACHE_SPLIT_ON                      (0x00000001 <<  9)
#define SCALE_3D_CNTL_APPLE_YUV_MODE_OFF                      (0x00000000 << 10)
#define SCALE_3D_CNTL_APPLE_YUV_MODE_ON                       (0x00000001 << 10)
#define SCALE_3D_CNTL_TEX_CACHE_PAL_MODE_OFF                  (0x00000000 << 11)
#define SCALE_3D_CNTL_TEX_CACHE_PAL_MODE_ON                   (0x00000001 << 11)
#define SCALE_3D_CNTL_ALPHA_COMB_FNC_ADD_CLAMP                (0x00000000 << 12)
#define SCALE_3D_CNTL_ALPHA_COMB_FNC_ADD_NCLAMP               (0x00000001 << 12)
#define SCALE_3D_CNTL_ALPHA_COMB_FNC_SUB_DST_SRC_CLAMP        (0x00000002 << 12)
#define SCALE_3D_CNTL_ALPHA_COMB_FNC_SUB_DST_SRC_NCLAMP       (0x00000003 << 12)
#define SCALE_3D_CNTL_FOG_TABLE_EN_OFF                        (0x00000000 << 14)
#define SCALE_3D_CNTL_FOG_TABLE_EN_ON                         (0x00000001 << 14)
#define SCALE_3D_CNTL_SIGNED_DST_CLAMP_OFF                    (0x00000000 << 15)
#define SCALE_3D_CNTL_SIGNED_DST_CLAMP_ON                     (0x00000001 << 15)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_ZERO                    (0x00000000 << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_ONE                     (0x00000001 << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_SRCCOLOR                (0x00000002 << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_INVSRCCOLOR             (0x00000003 << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_SRCALPHA                (0x00000004 << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_INVSRCALPHA             (0x00000005 << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_DSTALPHA                (0x00000006 << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_INVDSTALPHA             (0x00000007 << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_DSTCOLOR                (0x00000008 << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_INVDSTCOLOR             (0x00000009 << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_SAT                     (0x0000000a << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_BLEND                   (0x0000000b << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_SRC_INVBLEND                (0x0000000c << 16)
#define SCALE_3D_CNTL_ALPHA_BLEND_DST_ZERO                    (0x00000000 << 20)
#define SCALE_3D_CNTL_ALPHA_BLEND_DST_ONE                     (0x00000001 << 20)
#define SCALE_3D_CNTL_ALPHA_BLEND_DST_SRCCOLOR                (0x00000002 << 20)
#define SCALE_3D_CNTL_ALPHA_BLEND_DST_INVSRCCOLOR             (0x00000003 << 20)
#define SCALE_3D_CNTL_ALPHA_BLEND_DST_SRCALPHA                (0x00000004 << 20)
#define SCALE_3D_CNTL_ALPHA_BLEND_DST_INVSRCALPHA             (0x00000005 << 20)
#define SCALE_3D_CNTL_ALPHA_BLEND_DST_DSTALPHA                (0x00000006 << 20)
#define SCALE_3D_CNTL_ALPHA_BLEND_DST_INVDSTALPHA             (0x00000007 << 20)
#define SCALE_3D_CNTL_ALPHA_BLEND_DST_DSTCOLOR                (0x00000008 << 20)
#define SCALE_3D_CNTL_ALPHA_BLEND_DST_INVDSTCOLOR             (0x00000009 << 20)
#define SCALE_3D_CNTL_ALPHA_TEST_OP_NEVER                     (0x00000000 << 24)
#define SCALE_3D_CNTL_ALPHA_TEST_OP_LESS                      (0x00000001 << 24)
#define SCALE_3D_CNTL_ALPHA_TEST_OP_LESSEQUAL                 (0x00000002 << 24)
#define SCALE_3D_CNTL_ALPHA_TEST_OP_EQUAL                     (0x00000003 << 24)
#define SCALE_3D_CNTL_ALPHA_TEST_OP_GREATEREQUAL              (0x00000004 << 24)
#define SCALE_3D_CNTL_ALPHA_TEST_OP_GREATER                   (0x00000005 << 24)
#define SCALE_3D_CNTL_ALPHA_TEST_OP_NEQUAL                    (0x00000006 << 24)
#define SCALE_3D_CNTL_ALPHA_TEST_OP_ALWAYS                    (0x00000007 << 24)
#define SCALE_3D_CNTL_COMPOSITE_SHADOW_CMP_EQUAL              (0x00000000 << 28)
#define SCALE_3D_CNTL_COMPOSITE_SHADOW_CMP_NEQUAL             (0x00000001 << 28)
#define SCALE_3D_CNTL_COMPOSITE_SHADOW_EN_OFF                 (0x00000000 << 29)
#define SCALE_3D_CNTL_COMPOSITE_SHADOW_EN_ON                  (0x00000001 << 29)
#define SCALE_3D_CNTL_TEX_MAP_AEN_OFF                         (0x00000000 << 30)
#define SCALE_3D_CNTL_TEX_MAP_AEN_ON                          (0x00000001 << 30)
#define SCALE_3D_CNTL_TEX_CACHE_LINE_SIZE_8QW                 (0x00000000 << 31)
#define SCALE_3D_CNTL_TEX_CACHE_LINE_SIZE_4QW                 (0x00000001 << 31)




#define SCALE_3D_DATATYPE                          0x1a20
#define SETUP_CNTL                                 0x1bc4
#define SOLID_COLOR                                0x1bc8
#define WINDOW_XY_OFFSET                           0x1bcc
#define DRAW_LINE_POINT                            0x1bd0
#define SETUP_CNTL_PM4                             0x1bd4
#define DST_PITCH_OFFSET_C                         0x1c80
#define DP_GUI_MASTER_CNTL_C                       0x1c84
#define SC_TOP_LEFT_C                              0x1c88
#define SC_BOTTOM_RIGHT_C                          0x1c8c

#define Z_OFFSET_C                                 0x1c90
#define Z_PITCH_C                                  0x1c94
#define Z_STEN_CNTL_C                              0x1c98
#define TEX_CNTL_C                                 0x1c9c
#define TEXTURE_CLR_CMP_CLR_C                      0x1CA4
#define TEXTURE_CLR_CMP_MSK_C                      0x1CA8
#define FOG_COLOR_C                                0x1CAC
#define PRIM_TEX_CNTL_C                            0x1CB0
#define PRIM_TEX_COMBINE_CNTL_C                    0x1CB4
#define TEX_SIZE_PITCH_C                           0x1CB8
#define PRIM_TEX_0_OFFSET_C                        0x1CBC
#define PRIM_TEX_1_OFFSET_C                        0x1CC0
#define PRIM_TEX_2_OFFSET_C                        0x1CC4
#define PRIM_TEX_3_OFFSET_C                        0x1CC8
#define PRIM_TEX_4_OFFSET_C                        0x1CCC
#define PRIM_TEX_5_OFFSET_C                        0x1CD0
#define PRIM_TEX_6_OFFSET_C                        0x1CD4
#define PRIM_TEX_7_OFFSET_C                        0x1CD8
#define PRIM_TEX_8_OFFSET_C                        0x1CDC
#define PRIM_TEX_9_OFFSET_C                        0x1CE0
#define PRIM_TEX_10_OFFSET_C                       0x1CE4
#define SEC_TEX_CNTL_C                             0x1D00
#define SEC_TEX_COMBINE_CNTL_C                     0x1D04
#define SEC_TEX_0_OFFSET_C                         0x1D08
#define SEC_TEX_1_OFFSET_C                         0x1D0C
#define SEC_TEX_2_OFFSET_C                         0x1D10
#define SEC_TEX_3_OFFSET_C                         0x1D14
#define SEC_TEX_4_OFFSET_C                         0x1D18
#define SEC_TEX_5_OFFSET_C                         0x1D1C
#define SEC_TEX_6_OFFSET_C                         0x1D20
#define SEC_TEX_7_OFFSET_C                         0x1D24
#define SEC_TEX_8_OFFSET_C                         0x1D28
#define SEC_TEX_9_OFFSET_C                         0x1D2C
#define SEC_TEX_10_OFFSET_C                        0x1D30
#define CONSTANT_COLOR_C                           0x1D34
#define PRIM_TEXTURE_BORDER_COLOR_C                0x1D38
#define SEC_TEXTURE_BORDER_COLOR_C                 0x1D3C
#define STEN_REF_MASK_C                            0x1D40
#define PLANE_3D_MASK_C                            0x1D44

#define CLR_CMP_MASK_3D                            0x1A28
#define MC_SRC1_CNTL                               0x19D8
#define TEX_CNTL                                   0x1800
#define CLR_CMP_CLR_3D 0x1A24


/* first overlay unit (there is only one) */

#define OV0_Y_X_START                0x0400
#define OV0_Y_X_END                  0x0404
#define OV0_EXCLUSIVE_HORZ           0x0408
#       define  R128_EXCL_HORZ_START_MASK        0x000000ff
#       define  R128_EXCL_HORZ_END_MASK          0x0000ff00
#       define  R128_EXCL_HORZ_BACK_PORCH_MASK   0x00ff0000
#       define  R128_EXCL_HORZ_EXCLUSIVE_EN      0x80000000
#define OV0_EXCLUSIVE_VERT           0x040C
#       define  R128_EXCL_VERT_START_MASK        0x000003ff
#       define  R128_EXCL_VERT_END_MASK          0x03ff0000
#define OV0_REG_LOAD_CNTL            0x0410
#       define  R128_REG_LD_CTL_LOCK                 0x00000001L
#       define  R128_REG_LD_CTL_VBLANK_DURING_LOCK   0x00000002L
#       define  R128_REG_LD_CTL_STALL_GUI_UNTIL_FLIP 0x00000004L
#       define  R128_REG_LD_CTL_LOCK_READBACK        0x00000008L
#define OV0_SCALE_CNTL               0x0420
#       define  R128_SCALER_PIX_EXPAND           0x00000001L
#       define  R128_SCALER_Y2R_TEMP             0x00000002L
#       define  R128_SCALER_HORZ_PICK_NEAREST    0x00000003L
#       define  R128_SCALER_VERT_PICK_NEAREST    0x00000004L
#       define  R128_SCALER_SIGNED_UV            0x00000010L
#       define  R128_SCALER_GAMMA_SEL_MASK       0x00000060L
#       define  R128_SCALER_GAMMA_SEL_BRIGHT     0x00000000L
#       define  R128_SCALER_GAMMA_SEL_G22        0x00000020L
#       define  R128_SCALER_GAMMA_SEL_G18        0x00000040L
#       define  R128_SCALER_GAMMA_SEL_G14        0x00000060L
#       define  R128_SCALER_COMCORE_SHIFT_UP_ONE 0x00000080L
#       define  R128_SCALER_SURFAC_FORMAT        0x00000f00L
#       define  R128_SCALER_SOURCE_15BPP         0x00000300L
#       define  R128_SCALER_SOURCE_16BPP         0x00000400L
#       define  R128_SCALER_SOURCE_32BPP         0x00000600L
#       define  R128_SCALER_SOURCE_YUV9          0x00000900L
#       define  R128_SCALER_SOURCE_YUV12         0x00000A00L
#       define  R128_SCALER_SOURCE_VYUY422       0x00000B00L
#       define  R128_SCALER_SOURCE_YVYU422       0x00000C00L
#       define  R128_SCALER_SMART_SWITCH         0x00008000L
#       define  R128_SCALER_BURST_PER_PLANE      0x00ff0000L
#       define  R128_SCALER_DOUBLE_BUFFER        0x01000000L
#       define  R128_SCALER_DIS_LIMIT            0x08000000L
#       define  R128_SCALER_PRG_LOAD_START       0x10000000L
#       define  R128_SCALER_INT_EMU              0x20000000L
#       define  R128_SCALER_ENABLE               0x40000000L
#       define  R128_SCALER_SOFT_RESET           0x80000000L
#define OV0_V_INC                    0x0424
#define OV0_P1_V_ACCUM_INIT          0x0428
#       define  OV0_P1_MAX_LN_IN_PER_LN_OUT        0x00000003L
#       define  OV0_P1_V_ACCUM_INIT_MASK           0x01ff8000L
#define OV0_P23_V_ACCUM_INIT         0x042C
#define OV0_P1_BLANK_LINES_AT_TOP    0x0430
#       define  R128_P1_BLNK_LN_AT_TOP_M1_MASK   0x00000fffL
#       define  R128_P1_ACTIVE_LINES_M1          0x0fff0000L
#define OV0_P23_BLANK_LINES_AT_TOP   0x0434
#       define  R128_P23_BLNK_LN_AT_TOP_M1_MASK  0x000007ffL
#       define  R128_P23_ACTIVE_LINES_M1         0x07ff0000L
#define OV0_VID_BUF0_BASE_ADRS       0x0440
#       define  R128_VIF_BUF0_PITCH_SEL          0x00000001L
#       define  R128_VIF_BUF0_TILE_ADRS          0x00000002L
#       define  R128_VIF_BUF0_BASE_ADRS_MASK     0x03fffff0L
#       define  R128_VIF_BUF0_1ST_LINE_LSBS_MASK 0x48000000L
#define OV0_VID_BUF1_BASE_ADRS       0x0444
#       define  R128_VIF_BUF1_PITCH_SEL          0x00000001L
#       define  R128_VIF_BUF1_TILE_ADRS          0x00000002L
#       define  R128_VIF_BUF1_BASE_ADRS_MASK     0x03fffff0L
#       define  R128_VIF_BUF1_1ST_LINE_LSBS_MASK 0x48000000L
#define OV0_VID_BUF2_BASE_ADRS       0x0448
#       define  R128_VIF_BUF2_PITCH_SEL          0x00000001L
#       define  R128_VIF_BUF2_TILE_ADRS          0x00000002L
#       define  R128_VIF_BUF2_BASE_ADRS_MASK     0x03fffff0L
#       define  R128_VIF_BUF2_1ST_LINE_LSBS_MASK 0x48000000L
#define OV0_VID_BUF3_BASE_ADRS       0x044C
#define OV0_VID_BUF4_BASE_ADRS       0x0450
#define OV0_VID_BUF5_BASE_ADRS       0x0454
#define OV0_VID_BUF_PITCH0_VALUE     0x0460
#define OV0_VID_BUF_PITCH1_VALUE     0x0464
#define OV0_AUTO_FLIP_CNTL           0x0470
#define OV0_DEINTERLACE_PATTERN      0x0474
#define OV0_H_INC                    0x0480
#define OV0_STEP_BY                  0x0484
#define OV0_P1_H_ACCUM_INIT          0x0488
#define OV0_P23_H_ACCUM_INIT         0x048C
#define OV0_P1_X_START_END           0x0494
#define OV0_P2_X_START_END           0x0498
#define OV0_P3_X_START_END           0x049C
#define OV0_FILTER_CNTL              0x04A0
#define OV0_FOUR_TAP_COEF_0          0x04B0
#define OV0_FOUR_TAP_COEF_1          0x04B4
#define OV0_FOUR_TAP_COEF_2          0x04B8
#define OV0_FOUR_TAP_COEF_3          0x04BC
#define OV0_FOUR_TAP_COEF_4          0x04C0
#define OV0_COLOR_CNTL               0x04E0
#define OV0_VIDEO_KEY_CLR            0x04E4
#define OV0_VIDEO_KEY_MSK            0x04E8
#define OV0_GRAPHICS_KEY_CLR         0x04EC
#define OV0_GRAPHICS_KEY_MSK         0x04F0
#define OV0_KEY_CNTL                 0x04F4
#       define  R128_VIDEO_KEY_FN_MASK           0x00000007L
#       define  R128_VIDEO_KEY_FN_FALSE          0x00000000L
#       define  R128_VIDEO_KEY_FN_TRUE           0x00000001L
#       define  R128_VIDEO_KEY_FN_EQ             0x00000004L
#       define  R128_VIDEO_KEY_FN_NE             0x00000005L
#       define  R128_GRAPHIC_KEY_FN_MASK         0x00000070L
#       define  R128_GRAPHIC_KEY_FN_FALSE        0x00000000L
#       define  R128_GRAPHIC_KEY_FN_TRUE         0x00000010L
#       define  R128_GRAPHIC_KEY_FN_EQ           0x00000040L
#       define  R128_GRAPHIC_KEY_FN_NE           0x00000050L
#       define  R128_CMP_MIX_MASK                0x00000100L
#       define  R128_CMP_MIX_OR                  0x00000000L
#       define  R128_CMP_MIX_AND                 0x00000100L
#define OV0_TEST                     0x04F8


/* added by DirectFB programmers */
#define CRTC_OFFSET_FLIP_CNTL                      0x00010000
#define MEM_ADDR_CONFIG                            0x0148

#endif

