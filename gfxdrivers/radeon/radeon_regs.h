/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *   Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
 * References:
 *
 * !!!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 * !!!! FIXME !!!!
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 */

/* !!!! FIXME !!!!  NOTE: THIS FILE HAS BEEN CONVERTED FROM r128_reg.h
 * AND CONTAINS REGISTERS AND REGISTER DEFINITIONS THAT ARE NOT CORRECT
 * ON THE RADEON.  A FULL AUDIT OF THIS CODE IS NEEDED!  */

#ifndef __RADEON_REGS_H__
#define __RADEON_REGS_H__

#ifdef ROP_XOR
#undef ROP_XOR
#endif

#ifdef ROP_COPY
#undef ROP_COPY
#endif


				/* Registers for 2D/Video/Overlay */
#define CONFIG_VENDOR_ID             0x0f00 /* PCI */
#define CONFIG_DEVICE_ID             0x0f02 /* PCI */
#define CONFIG_ADAPTER_ID            0x0f2c /* PCI */

#define AGP_BASE                     0x0170
#define AGP_CNTL                     0x0174
#       define AGP_APER_SIZE_256MB   (0x00 << 0)
#       define AGP_APER_SIZE_128MB   (0x20 << 0)
#       define AGP_APER_SIZE_64MB    (0x30 << 0)
#       define AGP_APER_SIZE_32MB    (0x38 << 0)
#       define AGP_APER_SIZE_16MB    (0x3c << 0)
#       define AGP_APER_SIZE_8MB     (0x3e << 0)
#       define AGP_APER_SIZE_4MB     (0x3f << 0)
#       define AGP_APER_SIZE_MASK    (0x3f << 0)
#define AGP_COMMAND                  0x0f60 /* PCI */
#define AGP_COMMAND_PCI_CONFIG       0x0060 /* offset in PCI config*/
#       define AGP_ENABLE            (1<<8)
#define AGP_PLL_CNTL                 0x000b /* PLL */
#define AGP_STATUS                   0x0f5c /* PCI */
#       define AGP_1X_MODE           0x01
#       define AGP_2X_MODE           0x02
#       define AGP_4X_MODE           0x04
#       define AGP_FW_MODE           0x10
#       define AGP_MODE_MASK         0x17
#define ATTRDR                       0x03c1 /* VGA */
#define ATTRDW                       0x03c0 /* VGA */
#define ATTRX                        0x03c0 /* VGA */
#define AUX_SC_CNTL                  0x1660
#       define AUX1_SC_EN            (1 << 0)
#       define AUX1_SC_MODE_OR       (0 << 1)
#       define AUX1_SC_MODE_NAND     (1 << 1)
#       define AUX2_SC_EN            (1 << 2)
#       define AUX2_SC_MODE_OR       (0 << 3)
#       define AUX2_SC_MODE_NAND     (1 << 3)
#       define AUX3_SC_EN            (1 << 4)
#       define AUX3_SC_MODE_OR       (0 << 5)
#       define AUX3_SC_MODE_NAND     (1 << 5)
#define AUX1_SC_BOTTOM               0x1670
#define AUX1_SC_LEFT                 0x1664
#define AUX1_SC_RIGHT                0x1668
#define AUX1_SC_TOP                  0x166c
#define AUX2_SC_BOTTOM               0x1680
#define AUX2_SC_LEFT                 0x1674
#define AUX2_SC_RIGHT                0x1678
#define AUX2_SC_TOP                  0x167c
#define AUX3_SC_BOTTOM               0x1690
#define AUX3_SC_LEFT                 0x1684
#define AUX3_SC_RIGHT                0x1688
#define AUX3_SC_TOP                  0x168c
#define AUX_WINDOW_HORZ_CNTL         0x02d8
#define AUX_WINDOW_VERT_CNTL         0x02dc

#define BASE_CODE                    0x0f0b
#define BIOS_0_SCRATCH               0x0010
#define BIOS_1_SCRATCH               0x0014
#define BIOS_2_SCRATCH               0x0018
#define BIOS_3_SCRATCH               0x001c
#define BIOS_4_SCRATCH               0x0020
#define BIOS_5_SCRATCH               0x0024
#define BIOS_6_SCRATCH               0x0028
#define BIOS_7_SCRATCH               0x002c
#define BIOS_ROM                     0x0f30 /* PCI */
#define BIST                         0x0f0f /* PCI */
#define BRUSH_DATA0                  0x1480
#define BRUSH_DATA1                  0x1484
#define BRUSH_DATA10                 0x14a8
#define BRUSH_DATA11                 0x14ac
#define BRUSH_DATA12                 0x14b0
#define BRUSH_DATA13                 0x14b4
#define BRUSH_DATA14                 0x14b8
#define BRUSH_DATA15                 0x14bc
#define BRUSH_DATA16                 0x14c0
#define BRUSH_DATA17                 0x14c4
#define BRUSH_DATA18                 0x14c8
#define BRUSH_DATA19                 0x14cc
#define BRUSH_DATA2                  0x1488
#define BRUSH_DATA20                 0x14d0
#define BRUSH_DATA21                 0x14d4
#define BRUSH_DATA22                 0x14d8
#define BRUSH_DATA23                 0x14dc
#define BRUSH_DATA24                 0x14e0
#define BRUSH_DATA25                 0x14e4
#define BRUSH_DATA26                 0x14e8
#define BRUSH_DATA27                 0x14ec
#define BRUSH_DATA28                 0x14f0
#define BRUSH_DATA29                 0x14f4
#define BRUSH_DATA3                  0x148c
#define BRUSH_DATA30                 0x14f8
#define BRUSH_DATA31                 0x14fc
#define BRUSH_DATA32                 0x1500
#define BRUSH_DATA33                 0x1504
#define BRUSH_DATA34                 0x1508
#define BRUSH_DATA35                 0x150c
#define BRUSH_DATA36                 0x1510
#define BRUSH_DATA37                 0x1514
#define BRUSH_DATA38                 0x1518
#define BRUSH_DATA39                 0x151c
#define BRUSH_DATA4                  0x1490
#define BRUSH_DATA40                 0x1520
#define BRUSH_DATA41                 0x1524
#define BRUSH_DATA42                 0x1528
#define BRUSH_DATA43                 0x152c
#define BRUSH_DATA44                 0x1530
#define BRUSH_DATA45                 0x1534
#define BRUSH_DATA46                 0x1538
#define BRUSH_DATA47                 0x153c
#define BRUSH_DATA48                 0x1540
#define BRUSH_DATA49                 0x1544
#define BRUSH_DATA5                  0x1494
#define BRUSH_DATA50                 0x1548
#define BRUSH_DATA51                 0x154c
#define BRUSH_DATA52                 0x1550
#define BRUSH_DATA53                 0x1554
#define BRUSH_DATA54                 0x1558
#define BRUSH_DATA55                 0x155c
#define BRUSH_DATA56                 0x1560
#define BRUSH_DATA57                 0x1564
#define BRUSH_DATA58                 0x1568
#define BRUSH_DATA59                 0x156c
#define BRUSH_DATA6                  0x1498
#define BRUSH_DATA60                 0x1570
#define BRUSH_DATA61                 0x1574
#define BRUSH_DATA62                 0x1578
#define BRUSH_DATA63                 0x157c
#define BRUSH_DATA7                  0x149c
#define BRUSH_DATA8                  0x14a0
#define BRUSH_DATA9                  0x14a4
#define BRUSH_SCALE                  0x1470
#define BRUSH_Y_X                    0x1474
#define BUS_CNTL                     0x0030
#       define BUS_MASTER_DIS         (1 << 6)
#       define BUS_RD_DISCARD_EN      (1 << 24)
#       define BUS_RD_ABORT_EN        (1 << 25)
#       define BUS_MSTR_DISCONNECT_EN (1 << 28)
#       define BUS_WRT_BURST          (1 << 29)
#       define BUS_READ_BURST         (1 << 30)
#define BUS_CNTL1                    0x0034
#       define BUS_WAIT_ON_LOCK_EN    (1 << 4)

#define CACHE_CNTL                   0x1724
#define CACHE_LINE                   0x0f0c /* PCI */
#define CAP0_TRIG_CNTL               0x0950 /* ? */
#define CAP1_TRIG_CNTL               0x09c0 /* ? */
#define CAPABILITIES_ID              0x0f50 /* PCI */
#define CAPABILITIES_PTR             0x0f34 /* PCI */
#define CLK_PIN_CNTL                 0x0001 /* PLL */
#       define SCLK_DYN_START_CNTL   (1 << 15)
#define CLOCK_CNTL_DATA              0x000c
#define CLOCK_CNTL_INDEX             0x0008
#       define PLL_WR_EN             (1 << 7)
#       define PLL_DIV_SEL           (3 << 8)
#       define PLL2_DIV_SEL_MASK     ~(3 << 8)
#define CLK_PWRMGT_CNTL              0x0014
#       define ENGIN_DYNCLK_MODE     (1 << 12)
#       define ACTIVE_HILO_LAT_MASK  (3 << 13)
#       define ACTIVE_HILO_LAT_SHIFT 13
#       define DISP_DYN_STOP_LAT_MASK (1 << 12)
#       define DYN_STOP_MODE_MASK    (7 << 21)
#define PLL_PWRMGT_CNTL              0x0015
#       define TCL_BYPASS_DISABLE    (1 << 20)
#define CLR_CMP_CLR_3D               0x1a24
#define CLR_CMP_CLR_DST              0x15c8
#define CLR_CMP_CLR_SRC              0x15c4
#define CLR_CMP_CNTL                 0x15c0
#       define SRC_CMP_EQ_COLOR      (4 <<  0)
#       define SRC_CMP_NEQ_COLOR     (5 <<  0)
#       define CLR_CMP_SRC_SOURCE    (1 << 24)
#define CLR_CMP_MASK                 0x15cc
#       define CLR_CMP_MSK           0xffffffff
#define CLR_CMP_MASK_3D              0x1A28
#define COMMAND                      0x0f04 /* PCI */
#define COMPOSITE_SHADOW_ID          0x1a0c
#define CONFIG_APER_0_BASE           0x0100
#define CONFIG_APER_1_BASE           0x0104
#define CONFIG_APER_SIZE             0x0108
#define CONFIG_BONDS                 0x00e8
#define CONFIG_CNTL                  0x00e0
#       define CFG_ATI_REV_A11       (0   << 16)
#       define CFG_ATI_REV_A12       (1   << 16)
#       define CFG_ATI_REV_A13       (2   << 16)
#       define CFG_ATI_REV_ID_MASK   (0xf << 16)
#define CONFIG_MEMSIZE               0x00f8
#define CONFIG_MEMSIZE_EMBEDDED      0x0114
#define CONFIG_REG_1_BASE            0x010c
#define CONFIG_REG_APER_SIZE         0x0110
#define CONFIG_XSTRAP                0x00e4
#define CONSTANT_COLOR_C             0x1d34
#       define CONSTANT_COLOR_MASK   0x00ffffff
#       define CONSTANT_COLOR_ONE    0x00ffffff
#       define CONSTANT_COLOR_ZERO   0x00000000
#define CRC_CMDFIFO_ADDR             0x0740
#define CRC_CMDFIFO_DOUT             0x0744
#define GRPH_BUFFER_CNTL             0x02f0
#       define GRPH_START_REQ_MASK          (0x7f)
#       define GRPH_START_REQ_SHIFT         0
#       define GRPH_STOP_REQ_MASK           (0x7f<<8)
#       define GRPH_STOP_REQ_SHIFT          8
#       define GRPH_CRITICAL_POINT_MASK     (0x7f<<16)
#       define GRPH_CRITICAL_POINT_SHIFT    16
#       define GRPH_CRITICAL_CNTL           (1<<28)
#       define GRPH_BUFFER_SIZE             (1<<29)
#       define GRPH_CRITICAL_AT_SOF         (1<<30)
#       define GRPH_STOP_CNTL               (1<<31)
#define GRPH2_BUFFER_CNTL            0x03f0
#       define GRPH2_START_REQ_MASK         (0x7f)
#       define GRPH2_START_REQ_SHIFT         0
#       define GRPH2_STOP_REQ_MASK          (0x7f<<8)
#       define GRPH2_STOP_REQ_SHIFT         8
#       define GRPH2_CRITICAL_POINT_MASK    (0x7f<<16)
#       define GRPH2_CRITICAL_POINT_SHIFT   16
#       define GRPH2_CRITICAL_CNTL          (1<<28)
#       define GRPH2_BUFFER_SIZE            (1<<29)
#       define GRPH2_CRITICAL_AT_SOF        (1<<30)
#       define GRPH2_STOP_CNTL              (1<<31)
#define CRTC_CRNT_FRAME              0x0214
#define CRTC_EXT_CNTL                0x0054
#       define CRTC_VGA_XOVERSCAN    (1 <<  0)
#       define VGA_ATI_LINEAR        (1 <<  3)
#       define XCRT_CNT_EN           (1 <<  6)
#       define CRTC_HSYNC_DIS        (1 <<  8)
#       define CRTC_VSYNC_DIS        (1 <<  9)
#       define CRTC_DISPLAY_DIS      (1 << 10)
#       define CRTC_SYNC_TRISTAT     (1 << 11)
#       define CRTC_CRT_ON           (1 << 15)
#define CRTC_EXT_CNTL_DPMS_BYTE      0x0055
#       define CRTC_HSYNC_DIS_BYTE   (1 <<  0)
#       define CRTC_VSYNC_DIS_BYTE   (1 <<  1)
#       define CRTC_DISPLAY_DIS_BYTE (1 <<  2)
#define CRTC_GEN_CNTL                0x0050
#       define CRTC_DBL_SCAN_EN      (1 <<  0)
#       define CRTC_INTERLACE_EN     (1 <<  1)
#       define CRTC_CSYNC_EN         (1 <<  4)
#       define CRTC_CUR_EN           (1 << 16)
#       define CRTC_CUR_MODE_MASK    (7 << 17)
#       define CRTC_ICON_EN          (1 << 20)
#       define CRTC_EXT_DISP_EN      (1 << 24)
#       define CRTC_EN               (1 << 25)
#       define CRTC_DISP_REQ_EN_B    (1 << 26)
#define CRTC2_GEN_CNTL               0x03f8
#       define CRTC2_DBL_SCAN_EN     (1 <<  0)
#       define CRTC2_INTERLACE_EN    (1 <<  1)
#       define CRTC2_SYNC_TRISTAT    (1 <<  4)
#       define CRTC2_HSYNC_TRISTAT   (1 <<  5)
#       define CRTC2_VSYNC_TRISTAT   (1 <<  6)
#       define CRTC2_CRT2_ON         (1 <<  7)
#       define CRTC2_ICON_EN         (1 << 15)
#       define CRTC2_CUR_EN          (1 << 16)
#       define CRTC2_CUR_MODE_MASK   (7 << 20)
#       define CRTC2_DISP_DIS        (1 << 23)
#       define CRTC2_EN              (1 << 25)
#       define CRTC2_DISP_REQ_EN_B   (1 << 26)
#       define CRTC2_CSYNC_EN        (1 << 27)
#       define CRTC2_HSYNC_DIS       (1 << 28)
#       define CRTC2_VSYNC_DIS       (1 << 29)
#define CRTC_MORE_CNTL               0x27c
#       define CRTC_H_CUTOFF_ACTIVE_EN (1<<4)   
#       define CRTC_V_CUTOFF_ACTIVE_EN (1<<5)   
#define CRTC_GUI_TRIG_VLINE          0x0218
#define CRTC_H_SYNC_STRT_WID         0x0204
#       define CRTC_H_SYNC_STRT_PIX        (0x07  <<  0)
#       define CRTC_H_SYNC_STRT_CHAR       (0x3ff <<  3)
#       define CRTC_H_SYNC_STRT_CHAR_SHIFT 3
#       define CRTC_H_SYNC_WID             (0x3f  << 16)
#       define CRTC_H_SYNC_WID_SHIFT       16
#       define CRTC_H_SYNC_POL             (1     << 23)
#define CRTC2_H_SYNC_STRT_WID        0x0304
#       define CRTC2_H_SYNC_STRT_PIX        (0x07  <<  0)
#       define CRTC2_H_SYNC_STRT_CHAR       (0x3ff <<  3)
#       define CRTC2_H_SYNC_STRT_CHAR_SHIFT 3
#       define CRTC2_H_SYNC_WID             (0x3f  << 16)
#       define CRTC2_H_SYNC_WID_SHIFT       16
#       define CRTC2_H_SYNC_POL             (1     << 23)
#define CRTC_H_TOTAL_DISP            0x0200
#       define CRTC_H_TOTAL          (0x03ff << 0)
#       define CRTC_H_TOTAL_SHIFT    0
#       define CRTC_H_DISP           (0x01ff << 16)
#       define CRTC_H_DISP_SHIFT     16
#define CRTC2_H_TOTAL_DISP           0x0300
#       define CRTC2_H_TOTAL         (0x03ff << 0)
#       define CRTC2_H_TOTAL_SHIFT   0
#       define CRTC2_H_DISP          (0x01ff << 16)
#       define CRTC2_H_DISP_SHIFT    16
#define CRTC_OFFSET                  0x0224
#define CRTC2_OFFSET                 0x0324
#define CRTC_OFFSET_CNTL             0x0228
#       define CRTC_TILE_EN          (1 << 15)
#       define CRTC_HSYNC_EN         (1 << 16)
#define CRTC2_OFFSET_CNTL            0x0328
#       define CRTC2_TILE_EN         (1 << 15)
#define CRTC_PITCH                   0x022c
#define CRTC2_PITCH                  0x032c
#define CRTC_STATUS                  0x005c
#       define CRTC_VBLANK_SAVE      (1 <<  1)
#       define CRTC_VBLANK_SAVE_CLEAR  (1 <<  1)
#define CRTC2_STATUS                  0x03fc
#       define CRTC2_VBLANK_SAVE      (1 <<  1)
#       define CRTC2_VBLANK_SAVE_CLEAR  (1 <<  1)
#define CRTC_V_SYNC_STRT_WID         0x020c
#       define CRTC_V_SYNC_STRT        (0x7ff <<  0)
#       define CRTC_V_SYNC_STRT_SHIFT  0
#       define CRTC_V_SYNC_WID         (0x1f  << 16)
#       define CRTC_V_SYNC_WID_SHIFT   16
#       define CRTC_V_SYNC_POL         (1     << 23)
#define CRTC2_V_SYNC_STRT_WID        0x030c
#       define CRTC2_V_SYNC_STRT       (0x7ff <<  0)
#       define CRTC2_V_SYNC_STRT_SHIFT 0
#       define CRTC2_V_SYNC_WID        (0x1f  << 16)
#       define CRTC2_V_SYNC_WID_SHIFT  16
#       define CRTC2_V_SYNC_POL        (1     << 23)
#define CRTC_V_TOTAL_DISP            0x0208
#       define CRTC_V_TOTAL          (0x07ff << 0)
#       define CRTC_V_TOTAL_SHIFT    0
#       define CRTC_V_DISP           (0x07ff << 16)
#       define CRTC_V_DISP_SHIFT     16
#define CRTC2_V_TOTAL_DISP           0x0308
#       define CRTC2_V_TOTAL         (0x07ff << 0)
#       define CRTC2_V_TOTAL_SHIFT   0
#       define CRTC2_V_DISP          (0x07ff << 16)
#       define CRTC2_V_DISP_SHIFT    16
#define CRTC_VLINE_CRNT_VLINE        0x0210
#       define CRTC_CRNT_VLINE_MASK  (0x7ff << 16)
#define CRTC2_CRNT_FRAME             0x0314
#define CRTC2_GUI_TRIG_VLINE         0x0318
#define CRTC2_STATUS                 0x03fc
#define CRTC2_VLINE_CRNT_VLINE       0x0310
#define CRTC8_DATA                   0x03d5 /* VGA, 0x3b5 */
#define CRTC8_IDX                    0x03d4 /* VGA, 0x3b4 */
#define CUR_CLR0                     0x026c
#define CUR_CLR1                     0x0270
#define CUR_HORZ_VERT_OFF            0x0268
#define CUR_HORZ_VERT_POSN           0x0264
#define CUR_OFFSET                   0x0260
#       define CUR_LOCK              (1 << 31)
#define CUR2_CLR0                    0x036c
#define CUR2_CLR1                    0x0370
#define CUR2_HORZ_VERT_OFF           0x0368
#define CUR2_HORZ_VERT_POSN          0x0364
#define CUR2_OFFSET                  0x0360
#       define CUR2_LOCK             (1 << 31)

#define DAC_CNTL                     0x0058
#       define DAC_RANGE_CNTL        (3 <<  0)
#       define DAC_RANGE_CNTL_MASK   0x03
#       define DAC_BLANKING          (1 <<  2)
#       define DAC_CMP_EN            (1 <<  3)
#       define DAC_CMP_OUTPUT        (1 <<  7)
#       define DAC_8BIT_EN           (1 <<  8)
#       define DAC_TVO_EN            (1 << 10)
#       define DAC_VGA_ADR_EN        (1 << 13)
#       define DAC_PDWN              (1 << 15)
#       define DAC_MASK_ALL          (0xff << 24)
#define DAC_CNTL2                    0x007c
#       define DAC2_DAC_CLK_SEL      (1 <<  0)
#       define DAC2_DAC2_CLK_SEL     (1 <<  1)
#       define DAC2_PALETTE_ACC_CTL  (1 <<  5)
#define DAC_EXT_CNTL                 0x0280
#       define DAC_FORCE_BLANK_OFF_EN (1 << 4)
#       define DAC_FORCE_DATA_EN      (1 << 5)
#       define DAC_FORCE_DATA_SEL_MASK (3 << 6)
#       define DAC_FORCE_DATA_MASK   0x0003ff00
#       define DAC_FORCE_DATA_SHIFT  8
#define DAC_MACRO_CNTL               0x0d04
#       define DAC_PDWN_R            (1 << 16)
#       define DAC_PDWN_G            (1 << 17)
#       define DAC_PDWN_B            (1 << 18)
#define DISP_HW_DEBUG                0x0d14
#       define CRT2_DISP1_SEL        (1 <<  5)
#define DISP_OUTPUT_CNTL             0x0d64
#       define DISP_DAC_SOURCE_MASK    0x03
#       define DISP_DAC2_SOURCE_MASK   0x0c
#       define DISP_DAC_SOURCE_CRTC2   0x01
#       define DISP_DAC2_SOURCE_CRTC2  0x04
#       define DISP_TV_SOURCE          (1 << 16)
#       define DISP_TV_MODE_MASK       (3 << 17)
#       define DISP_TV_MODE_888        (0 << 17)
#       define DISP_TV_MODE_565        (1 << 17)
#       define DISP_TV_YG_DITH_EN      (1 << 19)
#       define DISP_TV_CBB_CRR_DITH_EN (1 << 20)
#       define DISP_TV_BIT_WIDTH       (1 << 21)
#       define DISP_TV_SYNC_MODE_MASK  (3 << 22)
#       define DISP_TV_SYNC_COLOR_MASK (3 << 25)
#define DAC_CRC_SIG                  0x02cc
#define DAC_DATA                     0x03c9 /* VGA */
#define DAC_MASK                     0x03c6 /* VGA */
#define DAC_R_INDEX                  0x03c7 /* VGA */
#define DAC_W_INDEX                  0x03c8 /* VGA */
#define DDA_CONFIG                   0x02e0
#define DDA_ON_OFF                   0x02e4
#define DEFAULT_OFFSET               0x16e0
#define DEFAULT_PITCH                0x16e4
#define DEFAULT_SC_BOTTOM_RIGHT      0x16e8
#       define DEFAULT_SC_RIGHT_MAX  (0x1fff <<  0)
#       define DEFAULT_SC_BOTTOM_MAX (0x1fff << 16)
#define DESTINATION_3D_CLR_CMP_VAL   0x1820
#define DESTINATION_3D_CLR_CMP_MSK   0x1824
#define DISP_MISC_CNTL               0x0d00
#       define SOFT_RESET_GRPH_PP    (1 << 0)
#define DISP_MERGE_CNTL		  0x0d60
#       define DISP_ALPHA_MODE_MASK  0x03
#       define DISP_ALPHA_MODE_KEY   0
#       define DISP_ALPHA_MODE_PER_PIXEL 1
#       define DISP_ALPHA_MODE_GLOBAL 2
#       define DISP_RGB_OFFSET_EN    (1<<8)
#       define DISP_GRPH_ALPHA_MASK  (0xff << 16)
#       define DISP_OV0_ALPHA_MASK   (0xff << 24)
#	    define DISP_LIN_TRANS_BYPASS (0x01 << 9)
#define DISP2_MERGE_CNTL		    0x0d68
#       define DISP2_RGB_OFFSET_EN   (1<<8)
#define DISP_LIN_TRANS_GRPH_A        0x0d80
#define DISP_LIN_TRANS_GRPH_B        0x0d84
#define DISP_LIN_TRANS_GRPH_C        0x0d88
#define DISP_LIN_TRANS_GRPH_D        0x0d8c
#define DISP_LIN_TRANS_GRPH_E        0x0d90
#define DISP_LIN_TRANS_GRPH_F        0x0d98
#define DP_BRUSH_BKGD_CLR            0x1478
#define DP_BRUSH_FRGD_CLR            0x147c
#define DP_CNTL                      0x16c0
#       define DST_X_LEFT_TO_RIGHT   (1 <<  0)
#       define DST_Y_TOP_TO_BOTTOM   (1 <<  1)
#define DP_CNTL_XDIR_YDIR_YMAJOR     0x16d0
#       define DST_Y_MAJOR             (1 <<  2)
#       define DST_Y_DIR_TOP_TO_BOTTOM (1 << 15)
#       define DST_X_DIR_LEFT_TO_RIGHT (1 << 31)
#define DP_DATATYPE                  0x16c4
#       define DST_8BPP	                0x00000002
#       define DST_15BPP                0x00000003
#       define DST_16BPP                0x00000004
#       define DST_24BPP                0x00000005
#       define DST_32BPP                0x00000006
#       define DST_8BPP_RGB332          0x00000007
#       define DST_8BPP_Y8              0x00000008
#       define DST_8BPP_RGB8            0x00000009
#       define DST_16BPP_VYUY422        0x0000000b
#       define DST_16BPP_YVYU422        0x0000000c
#       define DST_32BPP_AYUV444        0x0000000e
#       define DST_16BPP_ARGB4444       0x0000000f
#       define BRUSH_SOLIDCOLOR	        0x00000d00
#       define SRC_MONO	                0x00000000
#       define SRC_MONO_LBKGD           0x00010000
#       define SRC_DSTCOLOR             0x00030000
#       define BYTE_ORDER_MSB_TO_LSB    0x00000000
#       define BYTE_ORDER_LSB_TO_MSB    0x40000000
#       define DP_CONVERSION_TEMP       0x80000000
#       define HOST_BIG_ENDIAN_EN       (1 << 29)
#define DP_GUI_MASTER_CNTL           0x146c
#       define GMC_SRC_PITCH_OFFSET_CNTL   (1    <<  0)
#       define GMC_DST_PITCH_OFFSET_CNTL   (1    <<  1)
#       define GMC_SRC_CLIPPING            (1    <<  2)
#       define GMC_DST_CLIPPING            (1    <<  3)
#       define GMC_BRUSH_DATATYPE_MASK     (0x0f <<  4)
#       define GMC_BRUSH_8X8_MONO_FG_BG    (0    <<  4)
#       define GMC_BRUSH_8X8_MONO_FG_LA    (1    <<  4)
#       define GMC_BRUSH_1X8_MONO_FG_BG    (4    <<  4)
#       define GMC_BRUSH_1X8_MONO_FG_LA    (5    <<  4)
#       define GMC_BRUSH_32x1_MONO_FG_BG   (6    <<  4)
#       define GMC_BRUSH_32x1_MONO_FG_LA   (7    <<  4)
#       define GMC_BRUSH_32x32_MONO_FG_BG  (8    <<  4)
#       define GMC_BRUSH_32x32_MONO_FG_LA  (9    <<  4)
#       define GMC_BRUSH_8x8_COLOR         (10   <<  4)
#       define GMC_BRUSH_1X8_COLOR         (12   <<  4)
#       define GMC_BRUSH_SOLID_COLOR       (13   <<  4)
#       define GMC_BRUSH_NONE              (15   <<  4)
#       define GMC_DST_8BPP                (2    <<  8)
#       define GMC_DST_15BPP               (3    <<  8)
#       define GMC_DST_16BPP               (4    <<  8)
#       define GMC_DST_24BPP               (5    <<  8)
#       define GMC_DST_32BPP               (6    <<  8)
#       define GMC_DST_8BPP_RGB            (7    <<  8)
#       define GMC_DST_Y8                  (8    <<  8)
#       define GMC_DST_RGB8                (9    <<  8)
#       define GMC_DST_VYUY                (11   <<  8)
#       define GMC_DST_YVYU                (12   <<  8)
#       define GMC_DST_AYUV444             (14   <<  8)
#       define GMC_DST_ARGB4444            (15   <<  8)
#       define GMC_DST_DATATYPE_MASK       (0x0f <<  8)
#       define GMC_DST_DATATYPE_SHIFT      8
#       define GMC_SRC_DATATYPE_MASK       (3    << 12)
#       define GMC_SRC_DATATYPE_MONO_FG_BG (0    << 12)
#       define GMC_SRC_DATATYPE_MONO_FG_LA (1    << 12)
#       define GMC_SRC_DATATYPE_COLOR      (3    << 12)
#       define GMC_BYTE_PIX_ORDER          (1    << 14)
#       define GMC_BYTE_MSB_TO_LSB         (0    << 14)
#       define GMC_BYTE_LSB_TO_MSB         (1    << 14)
#       define GMC_CONVERSION_TEMP         (1    << 15)
#       define GMC_CONVERSION_TEMP_6500    (0    << 15)
#       define GMC_CONVERSION_TEMP_9300    (1    << 15)
#       define GMC_ROP3_MASK               (0xff << 16)
#       define GMC_ROP3_PATCOPY		   0x00f00000
#       define GMC_ROP3_SRCCOPY		   0x00cc0000
#       define GMC_ROP3_PATXOR             0x005a0000
#       define GMC_ROP3_XOR                0x00660000
#       define GMC_DP_SRC_SOURCE_MASK      (7    << 24)
#       define GMC_DP_SRC_SOURCE_MEMORY    (2    << 24)
#       define GMC_DP_SRC_SOURCE_HOST_DATA (3    << 24)
#       define GMC_3D_FCN_EN               (1    << 27)
#       define GMC_CLR_CMP_CNTL_DIS        (1    << 28)
#       define GMC_AUX_CLIP_DIS            (1    << 29)
#       define GMC_WR_MSK_DIS              (1    << 30)
#       define GMC_LD_BRUSH_Y_X            (1    << 31)
#define DP_GUI_MASTER_CNTL_C         0x1c84
#define DP_MIX                       0x16c8
#define DP_SRC_BKGD_CLR              0x15dc
#define DP_SRC_FRGD_CLR              0x15d8
#define DP_WRITE_MASK                0x16cc
#define DST_BRES_DEC                 0x1630
#define DST_BRES_ERR                 0x1628
#define DST_BRES_INC                 0x162c
#define DST_BRES_LNTH                0x1634
#define DST_BRES_LNTH_SUB            0x1638
#define DST_HEIGHT                   0x1410
#define DST_HEIGHT_WIDTH             0x143c
#define DST_HEIGHT_WIDTH_8           0x158c
#define DST_HEIGHT_WIDTH_BW          0x15b4
#define DST_HEIGHT_Y                 0x15a0
#define DST_LINE_START               0x1600
#define DST_LINE_END                 0x1604
#define DST_LINE_PATCOUNT            0x1608
#       define BRES_CNTL_SHIFT       8
#define DST_OFFSET                   0x1404
#define DST_PITCH                    0x1408
#define DST_PITCH_OFFSET             0x142c
#define DST_PITCH_OFFSET_C           0x1c80
#       define PITCH_SHIFT           21
#       define DST_TILE_LINEAR       (0 << 30)
#       define DST_TILE_MACRO        (1 << 30)
#       define DST_TILE_MICRO        (2 << 30)
#       define DST_TILE_BOTH         (3 << 30)
#define DST_WIDTH                    0x140c
#define DST_WIDTH_HEIGHT             0x1598
#define DST_WIDTH_X                  0x1588
#define DST_WIDTH_X_INCY             0x159c
#define DST_X                        0x141c
#define DST_X_SUB                    0x15a4
#define DST_X_Y                      0x1594
#define DST_Y                        0x1420
#define DST_Y_SUB                    0x15a8
#define DST_Y_X                      0x1438

#define FCP_CNTL                     0x0910
#      define FCP0_SRC_PCICLK             0
#      define FCP0_SRC_PCLK               1
#      define FCP0_SRC_PCLKb              2
#      define FCP0_SRC_HREF               3
#      define FCP0_SRC_GND                4
#      define FCP0_SRC_HREFb              5
#define FLUSH_1                      0x1704
#define FLUSH_2                      0x1708
#define FLUSH_3                      0x170c
#define FLUSH_4                      0x1710
#define FLUSH_5                      0x1714
#define FLUSH_6                      0x1718
#define FLUSH_7                      0x171c
#define FOG_3D_TABLE_START           0x1810
#define FOG_3D_TABLE_END             0x1814
#define FOG_3D_TABLE_DENSITY         0x181c
#define FOG_TABLE_INDEX              0x1a14
#define FOG_TABLE_DATA               0x1a18
#define FP_CRTC_H_TOTAL_DISP         0x0250
#define FP_CRTC_V_TOTAL_DISP         0x0254
#define FP_CRTC2_H_TOTAL_DISP        0x0350
#define FP_CRTC2_V_TOTAL_DISP        0x0354
#       define FP_CRTC_H_TOTAL_MASK      0x000003ff
#       define FP_CRTC_H_DISP_MASK       0x01ff0000
#       define FP_CRTC_V_TOTAL_MASK      0x00000fff
#       define FP_CRTC_V_DISP_MASK       0x0fff0000
#       define FP_H_SYNC_STRT_CHAR_MASK  0x00001ff8
#       define FP_H_SYNC_WID_MASK        0x003f0000
#       define FP_V_SYNC_STRT_MASK       0x00000fff
#       define FP_V_SYNC_WID_MASK        0x001f0000
#       define FP_CRTC_H_TOTAL_SHIFT     0x00000000
#       define FP_CRTC_H_DISP_SHIFT      0x00000010
#       define FP_CRTC_V_TOTAL_SHIFT     0x00000000
#       define FP_CRTC_V_DISP_SHIFT      0x00000010
#       define FP_H_SYNC_STRT_CHAR_SHIFT 0x00000003
#       define FP_H_SYNC_WID_SHIFT       0x00000010
#       define FP_V_SYNC_STRT_SHIFT      0x00000000
#       define FP_V_SYNC_WID_SHIFT       0x00000010
#define FP_GEN_CNTL                  0x0284
#       define FP_FPON                  (1 <<  0)
#       define FP_BLANK_EN              (1 <<  1)
#       define FP_TMDS_EN               (1 <<  2)
#       define FP_PANEL_FORMAT          (1 <<  3)
#       define FP_EN_TMDS               (1 <<  7)
#       define FP_DETECT_SENSE          (1 <<  8)
#       define R200_FP_SOURCE_SEL_MASK         (3 <<  10)
#       define R200_FP_SOURCE_SEL_CRTC1        (0 <<  10)
#       define R200_FP_SOURCE_SEL_CRTC2        (1 <<  10)
#       define R200_FP_SOURCE_SEL_RMX          (2 <<  10)
#       define R200_FP_SOURCE_SEL_TRANS        (3 <<  10)
#       define FP_SEL_CRTC1             (0 << 13)
#       define FP_SEL_CRTC2             (1 << 13)
#       define FP_CRTC_DONT_SHADOW_HPAR (1 << 15)
#       define FP_CRTC_DONT_SHADOW_VPAR (1 << 16)
#       define FP_CRTC_DONT_SHADOW_HEND (1 << 17)
#       define FP_CRTC_USE_SHADOW_VEND  (1 << 18)
#       define FP_RMX_HVSYNC_CONTROL_EN (1 << 20)
#       define FP_DFP_SYNC_SEL          (1 << 21)
#       define FP_CRTC_LOCK_8DOT        (1 << 22)
#       define FP_CRT_SYNC_SEL          (1 << 23)
#       define FP_USE_SHADOW_EN         (1 << 24)
#       define FP_CRT_SYNC_ALT          (1 << 26)
#define FP2_GEN_CNTL                 0x0288
#       define FP2_BLANK_EN             (1 <<  1)
#       define FP2_ON                   (1 <<  2)
#       define FP2_PANEL_FORMAT         (1 <<  3)
#       define R200_FP2_SOURCE_SEL_MASK        (3 << 10)
#       define R200_FP2_SOURCE_SEL_CRTC1       (0 <<  10)
#       define R200_FP2_SOURCE_SEL_CRTC2       (1 << 10)
#       define R200_FP2_SOURCE_SEL_RMX         (2 << 10)
#       define FP2_SRC_SEL_MASK         (3 << 13)
#       define FP2_SRC_SEL_CRTC2        (1 << 13)
#       define FP2_FP_POL               (1 << 16)
#       define FP2_LP_POL               (1 << 17)
#       define FP2_SCK_POL              (1 << 18)
#       define FP2_LCD_CNTL_MASK        (7 << 19)
#       define FP2_PAD_FLOP_EN          (1 << 22)
#       define FP2_CRC_EN               (1 << 23)
#       define FP2_CRC_READ_EN          (1 << 24)
#       define FP2_DVO_EN               (1 << 25)
#       define FP2_DVO_RATE_SEL_SDR     (1 << 26)
#define FP_H_SYNC_STRT_WID           0x02c4
#define FP2_H_SYNC_STRT_WID          0x03c4
#define FP_HORZ_STRETCH              0x028c
#define FP_HORZ2_STRETCH             0x038c
#       define HORZ_STRETCH_RATIO_MASK 0xffff
#       define HORZ_STRETCH_RATIO_MAX  4096
#       define HORZ_PANEL_SIZE         (0x1ff   << 16)
#       define HORZ_PANEL_SHIFT        16
#       define HORZ_STRETCH_PIXREP     (0      << 25)
#       define HORZ_STRETCH_BLEND      (1      << 26)
#       define HORZ_STRETCH_ENABLE     (1      << 25)
#       define HORZ_AUTO_RATIO         (1      << 27)
#       define HORZ_FP_LOOP_STRETCH    (0x7    << 28)
#       define HORZ_AUTO_RATIO_INC     (1      << 31)
#define FP_V_SYNC_STRT_WID           0x02c8
#define FP_VERT_STRETCH              0x0290
#define FP2_V_SYNC_STRT_WID          0x03c8
#define FP_VERT2_STRETCH             0x0390
#       define VERT_PANEL_SIZE          (0xfff << 12)
#       define VERT_PANEL_SHIFT         12
#       define VERT_STRETCH_RATIO_MASK  0xfff
#       define VERT_STRETCH_RATIO_SHIFT 0
#       define VERT_STRETCH_RATIO_MAX   4096
#       define VERT_STRETCH_ENABLE      (1     << 25)
#       define VERT_STRETCH_LINEREP     (0     << 26)
#       define VERT_STRETCH_BLEND       (1     << 26)
#       define VERT_AUTO_RATIO_EN       (1     << 27)
#       define VERT_STRETCH_RESERVED    0xf1000000

#define GEN_INT_CNTL                 0x0040
#define GEN_INT_STATUS               0x0044
#       define VSYNC_INT_AK          (1 <<  2)
#       define VSYNC_INT             (1 <<  2)
#       define VSYNC2_INT_AK         (1 <<  6)
#       define VSYNC2_INT            (1 <<  6)
#define GENENB                       0x03c3 /* VGA */
#define GENFC_RD                     0x03ca /* VGA */
#define GENFC_WT                     0x03da /* VGA, 0x03ba */
#define GENMO_RD                     0x03cc /* VGA */
#define GENMO_WT                     0x03c2 /* VGA */
#define GENS0                        0x03c2 /* VGA */
#define GENS1                        0x03da /* VGA, 0x03ba */
#define GPIO_MONID                   0x0068 /* DDC interface via I2C */
#define GPIO_MONIDB                  0x006c
#define GPIO_CRT2_DDC                0x006c
#define GPIO_DVI_DDC                 0x0064
#define GPIO_VGA_DDC                 0x0060
#       define GPIO_A_0              (1 <<  0)
#       define GPIO_A_1              (1 <<  1)
#       define GPIO_Y_0              (1 <<  8)
#       define GPIO_Y_1              (1 <<  9)
#       define GPIO_Y_SHIFT_0        8
#       define GPIO_Y_SHIFT_1        9
#       define GPIO_EN_0             (1 << 16)
#       define GPIO_EN_1             (1 << 17)
#       define GPIO_MASK_0           (1 << 24) /*??*/
#       define GPIO_MASK_1           (1 << 25) /*??*/
#define GRPH8_DATA                   0x03cf /* VGA */
#define GRPH8_IDX                    0x03ce /* VGA */
#define GUI_SCRATCH_REG0             0x15e0
#define GUI_SCRATCH_REG1             0x15e4
#define GUI_SCRATCH_REG2             0x15e8
#define GUI_SCRATCH_REG3             0x15ec
#define GUI_SCRATCH_REG4             0x15f0
#define GUI_SCRATCH_REG5             0x15f4

#define HEADER                       0x0f0e /* PCI */
#define HOST_DATA0                   0x17c0
#define HOST_DATA1                   0x17c4
#define HOST_DATA2                   0x17c8
#define HOST_DATA3                   0x17cc
#define HOST_DATA4                   0x17d0
#define HOST_DATA5                   0x17d4
#define HOST_DATA6                   0x17d8
#define HOST_DATA7                   0x17dc
#define HOST_DATA_LAST               0x17e0
#define HOST_PATH_CNTL               0x0130
#       define HDP_SOFT_RESET        (1 << 26)
#define HTOTAL_CNTL                  0x0009 /* PLL */
#define HTOTAL2_CNTL                 0x002e /* PLL */

#define I2C_CNTL_1                   0x0094 /* ? */
#define DVI_I2C_CNTL_1               0x02e4 /* ? */
#define INTERRUPT_LINE               0x0f3c /* PCI */
#define INTERRUPT_PIN                0x0f3d /* PCI */
#define IO_BASE                      0x0f14 /* PCI */

#define LATENCY                      0x0f0d /* PCI */
#define LEAD_BRES_DEC                0x1608
#define LEAD_BRES_LNTH               0x161c
#define LEAD_BRES_LNTH_SUB           0x1624
#define LVDS_GEN_CNTL                0x02d0
#       define LVDS_ON               (1   <<  0)
#       define LVDS_DISPLAY_DIS      (1   <<  1)
#       define LVDS_PANEL_TYPE       (1   <<  2)
#       define LVDS_PANEL_FORMAT     (1   <<  3)
#       define LVDS_EN               (1   <<  7)
#       define LVDS_DIGON            (1   << 18)
#       define LVDS_BLON             (1   << 19)
#       define LVDS_SEL_CRTC2        (1   << 23)
#define LVDS_PLL_CNTL                0x02d4
#       define HSYNC_DELAY_SHIFT     28
#       define HSYNC_DELAY_MASK      (0xf << 28)

#define MAX_LATENCY                  0x0f3f /* PCI */
#define MC_AGP_LOCATION              0x014c
#define MC_FB_LOCATION               0x0148
#define CRTC_BASE_ADDR               0x023c
#define CRTC2_BASE_ADDR              0x033c
#define DISPLAY_TEST_DEBUG_CNTL      0x0d10
#define NB_TOM                       0x015c
#define MCLK_CNTL                    0x0012 /* PLL */
#       define FORCEON_MCLKA         (1 << 16)
#       define FORCEON_MCLKB         (1 << 17)
#       define FORCEON_YCLKA         (1 << 18)
#       define FORCEON_YCLKB         (1 << 19)
#       define FORCEON_MC            (1 << 20)
#       define FORCEON_AIC           (1 << 21)
#       define R300_DISABLE_MC_MCLKA        (1 << 21)
#       define R300_DISABLE_MC_MCLKB        (1 << 21)
#define MCLK_MISC                    0x001f /* PLL */
#       define MC_MCLK_MAX_DYN_STOP_LAT (1<<12)
#       define IO_MCLK_MAX_DYN_STOP_LAT (1<<13)
#       define MC_MCLK_DYN_ENABLE    (1 << 14)
#       define IO_MCLK_DYN_ENABLE    (1 << 14)
#define MDGPIO_A_REG                 0x01ac
#define MDGPIO_EN_REG                0x01b0
#define MDGPIO_MASK                  0x0198
#define MDGPIO_Y_REG                 0x01b4
#define MEM_ADDR_CONFIG              0x0148
#define MEM_BASE                     0x0f10 /* PCI */
#define MEM_CNTL                     0x0140
#       define MEM_NUM_CHANNELS_MASK 0x01
#       define MEM_USE_B_CH_ONLY     (1<<1)
#       define RV100_HALF_MODE              (1<<3)
#       define R300_MEM_NUM_CHANNELS_MASK   0x03
#       define R300_MEM_USE_CD_CH_ONLY      (1<<2)
#define MEM_TIMING_CNTL              0x0144 /* EXT_MEM_CNTL */
#define MEM_INIT_LAT_TIMER           0x0154
#define MEM_INTF_CNTL                0x014c
#define MEM_SDRAM_MODE_REG           0x0158
#define MEM_STR_CNTL                 0x0150
#define MEM_VGA_RP_SEL               0x003c
#define MEM_VGA_WP_SEL               0x0038
#define MIN_GRANT                    0x0f3e /* PCI */
#define MM_DATA                      0x0004
#define MM_INDEX                     0x0000
#define MPLL_CNTL                    0x000e /* PLL */
#define MPP_TB_CONFIG                0x01c0 /* ? */
#define MPP_GP_CONFIG                0x01c8 /* ? */
#define R300_MC_IND_INDEX                   0x01f8
#       define R300_MC_IND_ADDR_MASK        0x3f
#define R300_MC_IND_DATA                    0x01fc
#define R300_MC_READ_CNTL_AB                0x017c
#       define R300_MEM_RBS_POSITION_A_MASK 0x03
#define R300_MC_READ_CNTL_CD_mcind	    0x24
#       define R300_MEM_RBS_POSITION_C_MASK 0x03

#define N_VIF_COUNT                  0x0248

/* overlay */
#define	OV0_Y_X_START                0x0400
#define	OV0_Y_X_END                  0x0404
#define	OV0_PIPELINE_CNTL            0x0408
#define	OV0_EXCLUSIVE_HORZ           0x0408
#	define EXCL_HORZ_START_MASK         0x000000ff
#	define EXCL_HORZ_END_MASK           0x0000ff00
#	define EXCL_HORZ_BACK_PORCH_MASK    0x00ff0000
#	define EXCL_HORZ_EXCLUSIVE_EN       0x80000000
#define	OV0_EXCLUSIVE_VERT           0x040C
#	define EXCL_VERT_START_MASK	        0x000003ff
#	define EXCL_VERT_END_MASK           0x03ff0000
#define	OV0_REG_LOAD_CNTL            0x0410
#	define REG_LD_CTL_LOCK                  0x00000001
#	define REG_LD_CTL_VBLANK_DURING_LOCK    0x00000002
#	define REG_LD_CTL_STALL_GUI_UNTIL_FLIP  0x00000004
#	define REG_LD_CTL_LOCK_READBACK         0x00000008
#define	OV0_SCALE_CNTL               0x0420
#	define SCALER_PIX_EXPAND           0x00000001
#	define SCALER_Y2R_TEMP             0x00000002
#	define SCALER_HORZ_PICK_NEAREST    0x00000004
#	define SCALER_VERT_PICK_NEAREST    0x00000008
#	define SCALER_SIGNED_UV	           0x00000010
#	define SCALER_GAMMA_SEL_MASK       0x00000060
#	define SCALER_GAMMA_SEL_BRIGHT     0x00000000
#	define SCALER_GAMMA_SEL_G22        0x00000020
#	define SCALER_GAMMA_SEL_G18        0x00000040
#	define SCALER_GAMMA_SEL_G14        0x00000060
#	define SCALER_COMCORE_SHIFT_UP_ONE 0x00000080
#	define SCALER_SURFAC_FORMAT        0x00000f00
#	define SCALER_SOURCE_UNK0          0x00000000 /* 2 bpp ??? */
#	define SCALER_SOURCE_UNK1          0x00000100 /* 4 bpp ??? */
#	define SCALER_SOURCE_UNK2          0x00000200 /* 8 bpp ??? */
#	define SCALER_SOURCE_15BPP         0x00000300
#	define SCALER_SOURCE_16BPP         0x00000400
/*#	define SCALER_SOURCE_24BPP         0x00000500*/
#	define SCALER_SOURCE_32BPP         0x00000600
#	define SCALER_SOURCE_UNK3          0x00000700 /* 8BPP_RGB332 ??? */
#	define SCALER_SOURCE_UNK4          0x00000800 /* 8BPP_Y8 ??? */
#	define SCALER_SOURCE_YUV9          0x00000900 /* 8BPP_RGB8 */
#	define SCALER_SOURCE_YUV12         0x00000A00
#	define SCALER_SOURCE_VYUY422       0x00000B00
#	define SCALER_SOURCE_YVYU422       0x00000C00
#	define SCALER_SOURCE_UNK5          0x00000D00 /* ??? */
#	define SCALER_SOURCE_UNK6          0x00000E00 /* 32BPP_AYUV444 */
#	define SCALER_SOURCE_UNK7          0x00000F00 /* 16BPP_ARGB4444 */
#	define SCALER_ADAPTIVE_DEINT       0x00001000
#	define R200_SCALER_TEMPORAL_DEINT  0x00002000
#	define SCALER_UNKNOWN_FLAG1        0x00004000 /* ??? */
#	define SCALER_SMART_SWITCH         0x00008000
#	define SCALER_BURST_PER_PLANE      0x007f0000
#	define SCALER_DOUBLE_BUFFER        0x01000000
#	define SCALER_UNKNOWN_FLAG3        0x02000000 /* ??? */
#	define SCALER_UNKNOWN_FLAG4        0x04000000 /* ??? */
#	define SCALER_DIS_LIMIT            0x08000000
#	define SCALER_PRG_LOAD_START       0x10000000
#	define SCALER_INT_EMU              0x20000000
#	define SCALER_ENABLE               0x40000000
#	define SCALER_SOFT_RESET           0x80000000
#define	OV0_V_INC                    0x0424
#define	OV0_P1_V_ACCUM_INIT          0x0428
#	define OV0_P1_MAX_LN_IN_PER_LN_OUT  0x00000003
#	define OV0_P1_V_ACCUM_INIT_MASK     0x01ff8000
#define	OV0_P23_V_ACCUM_INIT         0x042C
#	define OV0_P23_MAX_LN_IN_PER_LN_OUT 0x00000003
#	define OV0_P23_V_ACCUM_INIT_MASK    0x01ff8000
#define	OV0_P1_BLANK_LINES_AT_TOP    0x0430
#	define P1_BLNK_LN_AT_TOP_M1_MASK    0x00000fff
#	define P1_ACTIVE_LINES_M1           0x0fff0000
#define	OV0_P23_BLANK_LINES_AT_TOP   0x0434
#	define P23_BLNK_LN_AT_TOP_M1_MASK   0x000007ff
#	define P23_ACTIVE_LINES_M1          0x07ff0000
#define	OV0_BASE_ADDR                0x043C
#define	OV0_VID_BUF0_BASE_ADRS       0x0440
#	define VIF_BUF0_PITCH_SEL           0x00000001
#	define VIF_BUF0_TILE_ADRS           0x00000002
#	define VIF_BUF0_BASE_ADRS_MASK      0xfffffff0
#	define VIF_BUF0_1ST_LINE_LSBS_MASK  0x48000000
#define	OV0_VID_BUF1_BASE_ADRS       0x0444
#	define VIF_BUF1_PITCH_SEL           0x00000001
#	define VIF_BUF1_TILE_ADRS           0x00000002
#	define VIF_BUF1_BASE_ADRS_MASK      0xfffffff0
#	define VIF_BUF1_1ST_LINE_LSBS_MASK  0x48000000
#define	OV0_VID_BUF2_BASE_ADRS       0x0448
#	define VIF_BUF2_PITCH_SEL           0x00000001
#	define VIF_BUF2_TILE_ADRS           0x00000002
#	define VIF_BUF2_BASE_ADRS_MASK      0xfffffff0
#	define VIF_BUF2_1ST_LINE_LSBS_MASK  0x48000000
#define	OV0_VID_BUF3_BASE_ADRS       0x044C
#	define VIF_BUF3_PITCH_SEL           0x00000001
#	define VIF_BUF3_TILE_ADRS           0x00000002
#	define VIF_BUF3_BASE_ADRS_MASK      0xfffffff0
#	define VIF_BUF3_1ST_LINE_LSBS_MASK  0x48000000
#define	OV0_VID_BUF4_BASE_ADRS       0x0450
#	define VIF_BUF4_PITCH_SEL           0x00000001
#	define VIF_BUF4_TILE_ADRS           0x00000002
#	define VIF_BUF4_BASE_ADRS_MASK      0xfffffff0
#	define VIF_BUF4_1ST_LINE_LSBS_MASK  0x48000000
#define	OV0_VID_BUF5_BASE_ADRS       0x0454
#	define VIF_BUF5_PITCH_SEL           0x00000001
#	define VIF_BUF5_TILE_ADRS           0x00000002
#	define VIF_BUF5_BASE_ADRS_MASK      0xfffffff0
#	define VIF_BUF5_1ST_LINE_LSBS_MASK  0x48000000
#define	OV0_VID_BUF_PITCH0_VALUE     0x0460
#define	OV0_VID_BUF_PITCH1_VALUE     0x0464
#define	OV0_AUTO_FLIP_CNTL           0x0470
#	define OV0_AUTO_FLIP_CNTL_SOFT_BUF_NUM        0x00000007
#	define OV0_AUTO_FLIP_CNTL_SOFT_REPEAT_FIELD   0x00000008
#	define OV0_AUTO_FLIP_CNTL_SOFT_BUF_ODD        0x00000010
#	define OV0_AUTO_FLIP_CNTL_IGNORE_REPEAT_FIELD 0x00000020
#	define OV0_AUTO_FLIP_CNTL_SOFT_EOF_TOGGLE     0x00000040
#	define OV0_AUTO_FLIP_CNTL_VID_PORT_SELECT     0x00000300
#	define OV0_AUTO_FLIP_CNTL_P1_FIRST_LINE_EVEN  0x00010000
#	define OV0_AUTO_FLIP_CNTL_SHIFT_EVEN_DOWN     0x00040000
#	define OV0_AUTO_FLIP_CNTL_SHIFT_ODD_DOWN      0x00080000
#	define OV0_AUTO_FLIP_CNTL_FIELD_POL_SOURCE    0x00800000
#define	OV0_DEINTERLACE_PATTERN      0x0474
#define	OV0_SUBMIT_HISTORY           0x0478
#define	OV0_H_INC                    0x0480
#define	OV0_STEP_BY                  0x0484
#define	OV0_P1_H_ACCUM_INIT          0x0488
#define	OV0_P23_H_ACCUM_INIT         0x048C
#define	OV0_P1_X_START_END           0x0494
#define	OV0_P2_X_START_END           0x0498
#define	OV0_P3_X_START_END	         0x049C
#define	OV0_FILTER_CNTL	             0x04A0
#	define FILTER_PROGRAMMABLE_COEF     0x00000000
#	define FILTER_HARD_SCALE_HORZ_Y		0x00000001
#	define FILTER_HARD_SCALE_HORZ_UV    0x00000002
#	define FILTER_HARD_SCALE_VERT_Y     0x00000004
#	define FILTER_HARD_SCALE_VERT_UV    0x00000008
#	define FILTER_HARDCODED_COEF        0x0000000F
#	define FILTER_COEF_MASK	            0x0000000F
#define	OV0_FOUR_TAP_COEF_0          0x04B0
#	define OV0_FOUR_TAP_PHASE_0_TAP_0   0x0000000F
#	define OV0_FOUR_TAP_PHASE_0_TAP_1   0x00007F00
#	define OV0_FOUR_TAP_PHASE_0_TAP_2   0x007F0000
#	define OV0_FOUR_TAP_PHASE_0_TAP_3   0x0F000000
#define	OV0_FOUR_TAP_COEF_1	         0x04B4
#	define OV0_FOUR_TAP_PHASE_1_5_TAP_0 0x0000000F
#	define OV0_FOUR_TAP_PHASE_1_5_TAP_1	0x00007F00
#	define OV0_FOUR_TAP_PHASE_1_5_TAP_2	0x007F0000
#	define OV0_FOUR_TAP_PHASE_1_5_TAP_3	0x0F000000
#define	OV0_FOUR_TAP_COEF_2	         0x04B8
#	define OV0_FOUR_TAP_PHASE_2_6_TAP_0	0x0000000F
#	define OV0_FOUR_TAP_PHASE_2_6_TAP_1	0x00007F00
#	define OV0_FOUR_TAP_PHASE_2_6_TAP_2	0x007F0000
#	define OV0_FOUR_TAP_PHASE_2_6_TAP_3	0x0F000000
#define	OV0_FOUR_TAP_COEF_3	         0x04BC
#	define OV0_FOUR_TAP_PHASE_3_7_TAP_0	0x0000000F
#	define OV0_FOUR_TAP_PHASE_3_7_TAP_1	0x00007F00
#	define OV0_FOUR_TAP_PHASE_3_7_TAP_2	0x007F0000
#	define OV0_FOUR_TAP_PHASE_3_7_TAP_3	0x0F000000
#define	OV0_FOUR_TAP_COEF_4	         0x04C0
#	define OV0_FOUR_TAP_PHASE_4_TAP_0	0x0000000F
#	define OV0_FOUR_TAP_PHASE_4_TAP_1	0x00007F00
#	define OV0_FOUR_TAP_PHASE_4_TAP_2	0x007F0000
#	define OV0_FOUR_TAP_PHASE_4_TAP_3	0x0F000000
#define	OV0_FLAG_CNTL                0x04DC
#define	OV0_SLICE_CNTL               0x04E0
#	define SLICE_CNTL_DISABLE       0x40000000
#define	OV0_VID_KEY_CLR_LOW	         0x04E4
#define	OV0_VID_KEY_CLR_HIGH         0x04E8
#define	OV0_GRPH_KEY_CLR_LOW         0x04EC
#define	OV0_GRPH_KEY_CLR_HIGH        0x04F0
#define	OV0_KEY_CNTL                 0x04F4
#	define VIDEO_KEY_FN_MASK        0x00000003
#	define VIDEO_KEY_FN_FALSE       0x00000000
#	define VIDEO_KEY_FN_TRUE        0x00000001
#	define VIDEO_KEY_FN_EQ          0x00000002
#	define VIDEO_KEY_FN_NE          0x00000003
#	define GRAPHIC_KEY_FN_MASK      0x00000030
#	define GRAPHIC_KEY_FN_FALSE     0x00000000
#	define GRAPHIC_KEY_FN_TRUE      0x00000010
#	define GRAPHIC_KEY_FN_EQ        0x00000020
#	define GRAPHIC_KEY_FN_NE        0x00000030
#	define CMP_MIX_MASK             0x00000100
#	define CMP_MIX_OR               0x00000000
#	define CMP_MIX_AND              0x00000100
#define	OV0_TEST                     0x04F8
#	define OV0_SCALER_Y2R_DISABLE   0x00000001
#	define OV0_SUBPIC_ONLY          0x00000008
#	define OV0_EXTENSE              0x00000010
#	define OV0_SWAP_UV              0x00000020
#define OV0_COL_CONV				 0x04FC
#	define OV0_CB_TO_B              0x0000007F
#	define OV0_CB_TO_G              0x0000FF00
#	define OV0_CR_TO_G              0x00FF0000
#	define OV0_CR_TO_R              0x7F000000
#	define OV0_NEW_COL_CONV			0x80000000
#define OV1_Y_X_START                0x0600
#define OV1_Y_X_END                  0x0604
#define	OV0_LIN_TRANS_A              0x0D20
#define	OV0_LIN_TRANS_B              0x0D24
#define	OV0_LIN_TRANS_C              0x0D28
#define	OV0_LIN_TRANS_D	             0x0D2C
#define	OV0_LIN_TRANS_E              0x0D30
#define	OV0_LIN_TRANS_F              0x0D34
#define OV0_GAMMA_000_00F            0x0d40
#define OV0_GAMMA_010_01F            0x0d44
#define OV0_GAMMA_020_03F            0x0d48
#define OV0_GAMMA_040_07F            0x0d4c
#define OV0_GAMMA_080_0BF            0x0e00
#define OV0_GAMMA_0C0_0FF            0x0e04
#define OV0_GAMMA_100_13F            0x0e08
#define OV0_GAMMA_140_17F            0x0e0c
#define OV0_GAMMA_180_1BF            0x0e10
#define OV0_GAMMA_1C0_1FF            0x0e14
#define OV0_GAMMA_200_23F            0x0e18
#define OV0_GAMMA_240_27F            0x0e1c
#define OV0_GAMMA_280_2BF            0x0e20
#define OV0_GAMMA_2C0_2FF            0x0e24
#define OV0_GAMMA_300_33F            0x0e28
#define OV0_GAMMA_340_37F            0x0e2c
#define OV0_GAMMA_380_3BF            0x0d50
#define OV0_GAMMA_3C0_3FF            0x0d54

#define OVR_CLR                      0x0230
#define OVR_WID_LEFT_RIGHT           0x0234
#define OVR_WID_TOP_BOTTOM           0x0238

/* subpicture */
#define	SUBPIC_CNTL	                 0x0540
#define	SUBPIC_DEFCOLCON             0x0544
#define	SUBPIC_Y_X_START             0x054C
#define	SUBPIC_Y_X_END               0x0550
#define	SUBPIC_V_INC                 0x0554
#define	SUBPIC_H_INC                 0x0558
#define	SUBPIC_BUF0_OFFSET           0x055C
#define	SUBPIC_BUF1_OFFSET           0x0560
#define	SUBPIC_LC0_OFFSET            0x0564
#define	SUBPIC_LC1_OFFSET            0x0568
#define	SUBPIC_PITCH                 0x056C
#define	SUBPIC_BTN_HLI_COLCON        0x0570
#define	SUBPIC_BTN_HLI_Y_X_START     0x0574
#define	SUBPIC_BTN_HLI_Y_X_END       0x0578
#define	SUBPIC_PALETTE_INDEX         0x057C
#define	SUBPIC_PALETTE_DATA	         0x0580
#define	SUBPIC_H_ACCUM_INIT          0x0584
#define	SUBPIC_V_ACCUM_INIT          0x0588

#define P2PLL_CNTL                   0x002a /* P2PLL */
#       define P2PLL_RESET                (1 <<  0)
#       define P2PLL_SLEEP                (1 <<  1)
#       define P2PLL_ATOMIC_UPDATE_EN     (1 << 16)
#       define P2PLL_VGA_ATOMIC_UPDATE_EN (1 << 17)
#       define P2PLL_ATOMIC_UPDATE_VSYNC  (1 << 18)
#define P2PLL_DIV_0                  0x002c
#       define P2PLL_FB0_DIV_MASK    0x07ff
#       define P2PLL_POST0_DIV_MASK  0x00070000
#define P2PLL_REF_DIV                0x002B /* PLL */
#       define P2PLL_REF_DIV_MASK    0x03ff
#       define P2PLL_ATOMIC_UPDATE_R (1 << 15) /* same as _W */
#       define P2PLL_ATOMIC_UPDATE_W (1 << 15) /* same as _R */
#       define R300_PPLL_REF_DIV_ACC_MASK   (0x3ff << 18)
#       define R300_PPLL_REF_DIV_ACC_SHIFT  18
#define PALETTE_DATA                 0x00b4
#define PALETTE_30_DATA              0x00b8
#define PALETTE_INDEX                0x00b0
#define PCI_GART_PAGE                0x017c
#define PIXCLKS_CNTL                 0x002d
#       define PIX2CLK_SRC_SEL_MASK     0x03
#       define PIX2CLK_SRC_SEL_CPUCLK   0x00
#       define PIX2CLK_SRC_SEL_PSCANCLK 0x01
#       define PIX2CLK_SRC_SEL_BYTECLK  0x02
#       define PIX2CLK_SRC_SEL_P2PLLCLK 0x03
#       define PIX2CLK_ALWAYS_ONb       (1<<6)
#       define PIX2CLK_DAC_ALWAYS_ONb   (1<<7)
#       define PIXCLK_TV_SRC_SEL        (1 << 8)
#       define DISP_TVOUT_PIXCLK_TV_ALWAYS_ONb (1 << 9)
#       define R300_DVOCLK_ALWAYS_ONb          (1 << 10)
#       define PIXCLK_BLEND_ALWAYS_ONb  (1 << 11)
#       define PIXCLK_GV_ALWAYS_ONb     (1 << 12)
#       define PIXCLK_DIG_TMDS_ALWAYS_ONb (1 << 13)
#       define R300_PIXCLK_DVO_ALWAYS_ONb      (1 << 13)
#       define PIXCLK_LVDS_ALWAYS_ONb   (1 << 14)
#       define PIXCLK_TMDS_ALWAYS_ONb   (1 << 15)
#       define R300_PIXCLK_TRANS_ALWAYS_ONb    (1 << 16)
#       define R300_PIXCLK_TVO_ALWAYS_ONb      (1 << 17)
#       define R300_P2G2CLK_ALWAYS_ONb         (1 << 18)
#       define R300_P2G2CLK_DAC_ALWAYS_ONb     (1 << 19)
#       define R300_DISP_DAC_PIXCLK_DAC2_BLANK_OFF (1 << 23)
#define PLANE_3D_MASK_C              0x1d44
#define PLL_TEST_CNTL                0x0013 /* PLL */
#define PMI_CAP_ID                   0x0f5c /* PCI */
#define PMI_DATA                     0x0f63 /* PCI */
#define PMI_NXT_CAP_PTR              0x0f5d /* PCI */
#define PMI_PMC_REG                  0x0f5e /* PCI */
#define PMI_PMCSR_REG                0x0f60 /* PCI */
#define PMI_REGISTER                 0x0f5c /* PCI */
#define PPLL_CNTL                    0x0002 /* PLL */
#       define PPLL_RESET                (1 <<  0)
#       define PPLL_SLEEP                (1 <<  1)
#       define PPLL_ATOMIC_UPDATE_EN     (1 << 16)
#       define PPLL_VGA_ATOMIC_UPDATE_EN (1 << 17)
#       define PPLL_ATOMIC_UPDATE_VSYNC  (1 << 18)
#define PPLL_DIV_0                   0x0004 /* PLL */
#define PPLL_DIV_1                   0x0005 /* PLL */
#define PPLL_DIV_2                   0x0006 /* PLL */
#define PPLL_DIV_3                   0x0007 /* PLL */
#       define PPLL_FB3_DIV_MASK     0x07ff
#       define PPLL_POST3_DIV_MASK   0x00070000
#define PPLL_REF_DIV                 0x0003 /* PLL */
#       define PPLL_REF_DIV_MASK     0x03ff
#       define PPLL_ATOMIC_UPDATE_R  (1 << 15) /* same as _W */
#       define PPLL_ATOMIC_UPDATE_W  (1 << 15) /* same as _R */
#define PWR_MNGMT_CNTL_STATUS        0x0f60 /* PCI */

/* ERT registers */
#define TV_MASTER_CNTL                    0x0800
#       define TV_MASTER_TVCLK_ALWAYS_ONb       (1 << 30)
#       define TV_MASTER_TV_ON                  (1 << 31)
#define TV_RGB_CNTL                       0x0804
#       define TV_RGB_CNTL_INI	         0x007b0004
#define TV_SYNC_CNTL                      0x0808
#define TV_HTOTAL                         0x080c
#define TV_HDISP                          0x0810
#define TV_HSTART                         0x0818
#define TV_HCOUNT                         0x081c
#define TV_VTOTAL                         0x0820
#define TV_VDISP                          0x0824
#define TV_VCOUNT                         0x0828
#define TV_FTOTAL                         0x082c
#define TV_FCOUNT                         0x0830
#define TV_FRESTART                       0x0834
#define TV_HRESTART                       0x0838
#define TV_VRESTART                       0x083c
#define TV_HOST_READ_DATA                 0x0840
#define TV_HOST_WRITE_DATA                0x0844
#define TV_HOST_RD_WT_CNTL                0x0848
#define TV_VSCALER_CNTL1                  0x084c
#       define TV_VSCALER_RESTART_FIELD	        (1 << 29)
#define TV_TIMING_CNTL                    0x0850
#define TV_VSCALER_CNTL2                  0x0854
#define TV_Y_FALL_CNTL                    0x0858
#define TV_Y_RISE_CNTL                    0x085c
#define TV_Y_SAWTOOTH_CNTL                0x0860
#define TV_UPSAMP_AND_GAIN_CNTL           0x0864
#define TV_GAIN_LIMIT_SETTINGS            0x0868
#define TV_LINEAR_GAIN_SETTINGS           0x086c
#define TV_MODULATOR_CNTL1                0x0870
#define TV_MODULATOR_CNTL2                0x0874
#define TV_PRE_DAC_MUX_CNTL               0x0888
#       define TV_PRE_DAC_Y_RED_EN              (1 << 0)
#       define TV_PRE_DAC_C_GRN_EN              (1 << 1)
#       define TV_PRE_DAC_CMP_BLU_EN            (1 << 2)
#       define TV_PRE_DAC_RED_MX_FORCE_DAC_DATA (6 << 4)
#       define TV_PRE_DAC_GRN_MX_FORCE_DAC_DATA (6 << 8)
#       define TV_PRE_DAC_BLU_MX_FORCE_DAC_DATA (6 << 12)
#       define TV_FORCE_DAC_DATA_SHIFT           16
#define TV_DAC_CNTL                       0x088c
#       define TV_DAC_NBLANK	                  (1 << 0)
#       define	TV_DAC_NHOLD                     (1 << 1)
#       define TV_DAC_CMPOUT                    (1 << 5)
#       define	TV_DAC_BGSLEEP                   (1 << 6)
#       define	TV_DAC_RDACPD                    (1 << 24)
#       define	TV_DAC_GDACPD                    (1 << 25)
#       define	TV_DAC_BDACPD                    (1 << 26)
#define TV_CRC_CNTL                       0x0890
#define TV_UV_ADR                         0x08ac
/* ERT PLL registers */
#define TV_PLL_CNTL	                      0x21
#define TV_PLL_CNTL1                      0x22
#       define	TV_PLL_CNTL1_TVPLL_RESET	        (1 << 1)
#       define	TV_PLL_CNTL1_TVPLL_SLEEP         (1 << 3)
#       define	TV_PLL_CNTL1_TVPDC_SHIFT          14
#       define	TV_PLL_CNTL1_TVPDC_MASK          (3 << 14)
#       define	TV_PLL_CNTL1_TVCLK_SRC_SEL       (1 << 30)


#define RBBM_GUICNTL                 0x172c
#       define HOST_DATA_SWAP_NONE   (0 << 0)
#       define HOST_DATA_SWAP_16BIT  (1 << 0)
#       define HOST_DATA_SWAP_32BIT  (2 << 0)
#       define HOST_DATA_SWAP_HDW    (3 << 0)
#define RBBM_SOFT_RESET              0x00f0
#       define SOFT_RESET_CP         (1 <<  0)
#       define SOFT_RESET_HI         (1 <<  1)
#       define SOFT_RESET_SE         (1 <<  2)
#       define SOFT_RESET_RE         (1 <<  3)
#       define SOFT_RESET_PP         (1 <<  4)
#       define SOFT_RESET_E2         (1 <<  5)
#       define SOFT_RESET_RB         (1 <<  6)
#       define SOFT_RESET_HDP        (1 <<  7)
#define RBBM_STATUS                  0x0e40
#       define RBBM_FIFOCNT_MASK     0x007f
#       define RBBM_ACTIVE           (1 << 31)
#define RB2D_DSTCACHE_MODE           0x3428
#       define RB2D_DC_CACHE_ENABLE           (0)
#       define RB2D_DC_2D_CACHE_DISABLE       (1)
#       define RB2D_DC_3D_CACHE_DISABLE       (2)
#       define RB2D_DC_CACHE_DISABLE		 (3)
#       define RB2D_DC_2D_CACHE_LINESIZE_128	 (1 << 2)
#       define RB2D_DC_3D_CACHE_LINESIZE_128  (2 << 2)
#       define RB2D_DC_2D_CACHE_AUTOFLUSH     (1 << 8)
#       define RB2D_DC_3D_CACHE_AUTOFLUSH     (2 << 8)
#       define R200_RB2D_DC_2D_CACHE_AUTOFREE (1 << 10)
#       define R200_RB2D_DC_3D_CACHE_AUTOFREE (2 << 10)
#       define RB2D_DC_FORCE_RMW              (1 << 16)
#       define R300_RB2D_DC_ENABLE            (1 << 17)
#       define RB2D_DC_DISABLE_RI_FILL        (1 << 24)
#       define RB2D_DC_DISABLE_RI_READ        (1 << 25)
#       define RB2D_DC_DISABLE_MASK_CHK       (1 << 26)
#define RB2D_DSTCACHE_CTLSTAT        0x342c
#       define RB2D_DC_FLUSH         (3 << 0)
#       define RB2D_DC_FREE          (3 << 2)
#       define RB2D_DC_FLUSH_ALL     0xf
#       define RB2D_DC_BUSY          (1 << 31)
#define RB3D_DSTCACHE_MODE           0x3258
#       define RB3D_DC_CACHE_ENABLE           (0)
#       define RB3D_DC_2D_CACHE_DISABLE       (1)
#       define RB3D_DC_3D_CACHE_DISABLE       (2)
#       define RB3D_DC_CACHE_DISABLE		 (3)
#       define RB3D_DC_2D_CACHE_LINESIZE_128	 (1 << 2)
#       define RB3D_DC_3D_CACHE_LINESIZE_128  (2 << 2)
#       define RB3D_DC_2D_CACHE_AUTOFLUSH     (1 << 8)
#       define RB3D_DC_3D_CACHE_AUTOFLUSH     (2 << 8)
#       define R200_RB3D_DC_2D_CACHE_AUTOFREE (1 << 10)
#       define R200_RB3D_DC_3D_CACHE_AUTOFREE (2 << 10)
#       define RB3D_DC_FORCE_RMW              (1 << 16)
#       define R300_RB3D_DC_ENABLE            (1 << 17)
#       define RB3D_DC_DISABLE_RI_FILL        (1 << 24)
#       define RB3D_DC_DISABLE_RI_READ        (1 << 25)
#       define RB3D_DC_DISABLE_MASK_CHK       (1 << 26)
#define RB3D_DSTCACHE_CTLSTAT        0x325C
#       define RB3D_DC_FLUSH                  (3 << 0)
#       define RB3D_DC_FREE                   (3 << 2)
#       define RB3D_DC_FLUSH_ALL               0xf
#       define RB3D_DC_BUSY                   (1 << 31)
#define REG_BASE                     0x0f18 /* PCI */
#define REGPROG_INF                  0x0f09 /* PCI */
#define REVISION_ID                  0x0f08 /* PCI */

#define SC_BOTTOM                    0x164c
#define SC_BOTTOM_RIGHT              0x16f0
#define SC_BOTTOM_RIGHT_C            0x1c8c
#define SC_LEFT                      0x1640
#define SC_RIGHT                     0x1644
#define SC_TOP                       0x1648
#define SC_TOP_LEFT                  0x16ec
#define SC_TOP_LEFT_C                0x1c88
#       define SC_SIGN_MASK_LO       0x8000
#       define SC_SIGN_MASK_HI       0x80000000
#define SCLK_CNTL                    0x000d /* PLL */
#       define SCLK_SRC_SEL_MASK     0x0007
#       define DYN_STOP_LAT_MASK     0x00007ff8
#       define CP_MAX_DYN_STOP_LAT   0x0008
#       define SCLK_FORCEON_MASK     0xffff8000
#       define SCLK_FORCE_DISP2      (1<<15)
#       define SCLK_FORCE_CP         (1<<16)
#       define SCLK_FORCE_HDP        (1<<17)
#       define SCLK_FORCE_DISP1      (1<<18)
#       define SCLK_FORCE_TOP        (1<<19)
#       define SCLK_FORCE_E2         (1<<20)
#       define SCLK_FORCE_SE         (1<<21)
#       define SCLK_FORCE_IDCT       (1<<22)
#       define SCLK_FORCE_VIP        (1<<23)
#       define SCLK_FORCE_RE         (1<<24)
#       define SCLK_FORCE_PB         (1<<25)
#       define SCLK_FORCE_TAM        (1<<26)
#       define SCLK_FORCE_TDM        (1<<27)
#       define SCLK_FORCE_RB         (1<<28)
#       define SCLK_FORCE_TV_SCLK    (1<<29)
#       define SCLK_FORCE_SUBPIC     (1<<30)
#       define SCLK_FORCE_OV0        (1<<31)
#       define R300_SCLK_FORCE_VAP   (1<<21)
#       define R300_SCLK_FORCE_SR    (1<<25)
#       define R300_SCLK_FORCE_PX    (1<<26)
#       define R300_SCLK_FORCE_TX    (1<<27)
#       define R300_SCLK_FORCE_US    (1<<28)
#       define R300_SCLK_FORCE_SU    (1<<30)
#define R300_SCLK_CNTL2                     0x1e   /* PLL */
#       define R300_SCLK_TCL_MAX_DYN_STOP_LAT (1<<10)
#       define R300_SCLK_GA_MAX_DYN_STOP_LAT  (1<<11)
#       define R300_SCLK_CBA_MAX_DYN_STOP_LAT (1<<12)
#       define R300_SCLK_FORCE_TCL          (1<<13)
#       define R300_SCLK_FORCE_CBA          (1<<14)
#       define R300_SCLK_FORCE_GA           (1<<15)
#define SCLK_MORE_CNTL               0x0035 /* PLL */
#       define SCLK_MORE_MAX_DYN_STOP_LAT 0x0007
#       define SCLK_MORE_FORCEON     0x0700
#define SDRAM_MODE_REG               0x0158
#define SEQ8_DATA                    0x03c5 /* VGA */
#define SEQ8_IDX                     0x03c4 /* VGA */
#define SNAPSHOT_F_COUNT             0x0244
#define SNAPSHOT_VH_COUNTS           0x0240
#define SNAPSHOT_VIF_COUNT           0x024c
#define SRC_OFFSET                   0x15ac
#define SRC_PITCH                    0x15b0
#define SRC_PITCH_OFFSET             0x1428
#define SRC_SC_BOTTOM                0x165c
#define SRC_SC_BOTTOM_RIGHT          0x16f4
#define SRC_SC_RIGHT                 0x1654
#define SRC_X                        0x1414
#define SRC_X_Y                      0x1590
#define SRC_Y                        0x1418
#define SRC_Y_X                      0x1434
#define STATUS                       0x0f06 /* PCI */
#define SUB_CLASS                    0x0f0a /* PCI */
#define SURFACE_CNTL                 0x0b00
#       define SURF_TRANSLATION_DIS  (1 << 8)
#       define NONSURF_AP0_SWP_16BPP (1 << 20)
#       define NONSURF_AP0_SWP_32BPP (1 << 21)
#       define NONSURF_AP1_SWP_16BPP (1 << 22)
#       define NONSURF_AP1_SWP_32BPP (1 << 23)
#define SURFACE0_INFO                0x0b0c
#define SURFACE0_LOWER_BOUND         0x0b04
#define SURFACE0_UPPER_BOUND         0x0b08
#define SURFACE1_INFO                0x0b1c
#define SURFACE1_LOWER_BOUND         0x0b14
#define SURFACE1_UPPER_BOUND         0x0b18
#define SURFACE2_INFO                0x0b2c
#define SURFACE2_LOWER_BOUND         0x0b24
#define SURFACE2_UPPER_BOUND         0x0b28
#define SURFACE3_INFO                0x0b3c
#define SURFACE3_LOWER_BOUND         0x0b34
#define SURFACE3_UPPER_BOUND         0x0b38
#define SURFACE4_INFO                0x0b4c
#define SURFACE4_LOWER_BOUND         0x0b44
#define SURFACE4_UPPER_BOUND         0x0b48
#define SURFACE5_INFO                0x0b5c
#define SURFACE5_LOWER_BOUND         0x0b54
#define SURFACE5_UPPER_BOUND         0x0b58
#define SURFACE6_INFO                0x0b6c
#define SURFACE6_LOWER_BOUND         0x0b64
#define SURFACE6_UPPER_BOUND         0x0b68
#define SURFACE7_INFO                0x0b7c
#define SURFACE7_LOWER_BOUND         0x0b74
#define SURFACE7_UPPER_BOUND         0x0b78
#define SW_SEMAPHORE                 0x013c

#define TEST_DEBUG_CNTL              0x0120
#define TEST_DEBUG_MUX               0x0124
#define TEST_DEBUG_OUT               0x012c
#define TMDS_PLL_CNTL                0x02a8
#define TMDS_TRANSMITTER_CNTL        0x02a4
#       define TMDS_TRANSMITTER_PLLEN  1
#       define TMDS_TRANSMITTER_PLLRST 2
#define TRAIL_BRES_DEC               0x1614
#define TRAIL_BRES_ERR               0x160c
#define TRAIL_BRES_INC               0x1610
#define TRAIL_X                      0x1618
#define TRAIL_X_SUB                  0x1620

#define VCLK_ECP_CNTL                0x0008 /* PLL */
#       define VCLK_SRC_SEL_MASK     0x03
#       define VCLK_SRC_SEL_CPUCLK   0x00
#       define VCLK_SRC_SEL_PSCANCLK 0x01
#       define VCLK_SRC_SEL_BYTECLK  0x02
#       define VCLK_SRC_SEL_PPLLCLK  0x03
#       define PIXCLK_ALWAYS_ONb     (1<<6)
#       define PIXCLK_DAC_ALWAYS_ONb (1<<7)
#       define R300_DISP_DAC_PIXCLK_DAC_BLANK_OFF (1<<23)

#define VGA_DDA_CONFIG               0x02e8
#define VGA_DDA_ON_OFF               0x02ec
#define VID_BUFFER_CONTROL           0x0900
#define VIDEOMUX_CNTL                0x0190
#define VIPH_CONTROL                 0x0c40 /* ? */

#define WAIT_UNTIL                   0x1720
#       define WAIT_CRTC_PFLIP        (1 << 0)
#       define R300_WAIT_2D_IDLE      (1 << 0)
#       define R300_WAIT_3D_IDLE      (2 << 0)
#       define R300_WAIT_2D_IDLECLEAN (3 << 0)
#       define R300_WAIT_3D_IDLECLEAN (4 << 0)
#       define WAIT_2D_IDLE           (1 << 14)
#       define WAIT_3D_IDLE           (1 << 15)
#       define WAIT_2D_IDLECLEAN      (1 << 16)
#       define WAIT_3D_IDLECLEAN      (1 << 17)
#       define WAIT_HOST_IDLECLEAN    (1 << 18)

#define ISYNC_CNTL                   0x1724
#	define ISYNC_ANY2D_IDLE3D        (1 << 0)
#	define ISYNC_ANY3D_IDLE2D        (1 << 1)
#	define ISYNC_TRIG2D_IDLE3D       (1 << 2)
#	define ISYNC_TRIG3D_IDLE2D       (1 << 3)
#	define ISYNC_WAIT_IDLEGUI	   (1 << 4)
#	define ISYNC_CPSCRATCH_IDLEGUI   (1 << 5)

#define X_MPLL_REF_FB_DIV            0x000a /* PLL */
#define XCLK_CNTL                    0x000d /* PLL */
#define XDLL_CNTL                    0x000c /* PLL */
#define XPLL_CNTL                    0x000b /* PLL */



				/* Registers for 3D/TCL */
#define PP_BORDER_COLOR_0            0x1d40
#define PP_BORDER_COLOR_1            0x1d44
#define PP_BORDER_COLOR_2            0x1d48
#define PP_CNTL                      0x1c38
#       define STIPPLE_ENABLE        (1 <<  0)
#       define SCISSOR_ENABLE        (1 <<  1)
#       define PATTERN_ENABLE        (1 <<  2)
#       define SHADOW_ENABLE         (1 <<  3)
#       define TEX_ENABLE_MASK       (0xf << 4)
#       define TEX_0_ENABLE          (1 <<  4)
#       define TEX_1_ENABLE          (1 <<  5)
#       define TEX_2_ENABLE          (1 <<  6)
#       define TEX_3_ENABLE          (1 <<  7)
#       define TEX_BLEND_ENABLE_MASK (0xf << 12)
#       define TEX_BLEND_0_ENABLE    (1 << 12)
#       define TEX_BLEND_1_ENABLE    (1 << 13)
#       define TEX_BLEND_2_ENABLE    (1 << 14)
#       define TEX_BLEND_3_ENABLE    (1 << 15)
#       define PLANAR_YUV_ENABLE     (1 << 20)
#       define SPECULAR_ENABLE       (1 << 21)
#       define FOG_ENABLE            (1 << 22)
#       define ALPHA_TEST_ENABLE     (1 << 23)
#       define ANTI_ALIAS_NONE       (0 << 24)
#       define ANTI_ALIAS_LINE       (1 << 24)
#       define ANTI_ALIAS_POLY       (2 << 24)
#       define ANTI_ALIAS_LINE_POLY  (3 << 24)
#       define BUMP_MAP_ENABLE       (1 << 26)
#       define BUMPED_MAP_T0         (0 << 27)
#       define BUMPED_MAP_T1         (1 << 27)
#       define BUMPED_MAP_T2         (2 << 27)
#       define TEX_3D_ENABLE_0       (1 << 29)
#       define TEX_3D_ENABLE_1       (1 << 30)
#       define MC_ENABLE             (1 << 31)
#define PP_FOG_COLOR                 0x1c18
#       define FOG_COLOR_MASK        0x00ffffff
#       define FOG_VERTEX            (0 << 24)
#       define FOG_TABLE             (1 << 24)
#       define FOG_USE_DEPTH         (0 << 25)
#       define FOG_USE_DIFFUSE_ALPHA (2 << 25)
#       define FOG_USE_SPEC_ALPHA    (3 << 25)
#define PP_LUM_MATRIX                0x1d00
#define PP_MISC                      0x1c14
#       define REF_ALPHA_MASK        0x000000ff
#       define ALPHA_TEST_FAIL       (0 << 8)
#       define ALPHA_TEST_LESS       (1 << 8)
#       define ALPHA_TEST_LEQUAL     (2 << 8)
#       define ALPHA_TEST_EQUAL      (3 << 8)
#       define ALPHA_TEST_GEQUAL     (4 << 8)
#       define ALPHA_TEST_GREATER    (5 << 8)
#       define ALPHA_TEST_NEQUAL     (6 << 8)
#       define ALPHA_TEST_PASS       (7 << 8)
#       define ALPHA_TEST_OP_MASK    (7 << 8)
#       define CHROMA_FUNC_FAIL      (0 << 16)
#       define CHROMA_FUNC_PASS      (1 << 16)
#       define CHROMA_FUNC_NEQUAL    (2 << 16)
#       define CHROMA_FUNC_EQUAL     (3 << 16)
#       define CHROMA_KEY_NEAREST    (0 << 18)
#       define CHROMA_KEY_ZERO       (1 << 18)
#       define SHADOW_ID_AUTO_INC    (1 << 20)
#       define SHADOW_FUNC_EQUAL     (0 << 21)
#       define SHADOW_FUNC_NEQUAL    (1 << 21)
#       define SHADOW_PASS_1         (0 << 22)
#       define SHADOW_PASS_2         (1 << 22)
#       define RIGHT_HAND_CUBE_D3D   (0 << 24)
#       define RIGHT_HAND_CUBE_OGL   (1 << 24)
#define PP_ROT_MATRIX_0              0x1d58
#define PP_ROT_MATRIX_1              0x1d5c
#define PP_TXFILTER_0                0x1c54
#define PP_TXFILTER_1                0x1c6c
#define PP_TXFILTER_2                0x1c84
#       define MAG_FILTER_NEAREST                   (0  <<  0)
#       define MAG_FILTER_LINEAR                    (1  <<  0)
#       define MAG_FILTER_MASK                      (1  <<  0)
#       define MIN_FILTER_NEAREST                   (0  <<  1)
#       define MIN_FILTER_LINEAR                    (1  <<  1)
#       define MIN_FILTER_NEAREST_MIP_NEAREST       (2  <<  1)
#       define MIN_FILTER_NEAREST_MIP_LINEAR        (3  <<  1)
#       define MIN_FILTER_LINEAR_MIP_NEAREST        (6  <<  1)
#       define MIN_FILTER_LINEAR_MIP_LINEAR         (7  <<  1)
#       define MIN_FILTER_ANISO_NEAREST             (8  <<  1)
#       define MIN_FILTER_ANISO_LINEAR              (9  <<  1)
#       define MIN_FILTER_ANISO_NEAREST_MIP_NEAREST (10 <<  1)
#       define MIN_FILTER_ANISO_NEAREST_MIP_LINEAR  (11 <<  1)
#       define MIN_FILTER_MASK                      (15 <<  1)
#       define MAX_ANISO_1_TO_1                     (0  <<  5)
#       define MAX_ANISO_2_TO_1                     (1  <<  5)
#       define MAX_ANISO_4_TO_1                     (2  <<  5)
#       define MAX_ANISO_8_TO_1                     (3  <<  5)
#       define MAX_ANISO_16_TO_1                    (4  <<  5)
#       define MAX_ANISO_MASK                       (7  <<  5)
#       define LOD_BIAS_MASK                        (0xff <<  8)
#       define LOD_BIAS_SHIFT                       8
#       define MAX_MIP_LEVEL_MASK                   (0x0f << 16)
#       define MAX_MIP_LEVEL_SHIFT                  16
#       define YUV_TO_RGB                           (1  << 20)
#       define YUV_TEMPERATURE_COOL                 (0  << 21)
#       define YUV_TEMPERATURE_HOT                  (1  << 21)
#       define YUV_TEMPERATURE_MASK                 (1  << 21)
#       define WRAPEN_S                             (1  << 22)
#       define CLAMP_S_WRAP                         (0  << 23)
#       define CLAMP_S_MIRROR                       (1  << 23)
#       define CLAMP_S_CLAMP_LAST                   (2  << 23)
#       define CLAMP_S_MIRROR_CLAMP_LAST            (3  << 23)
#       define CLAMP_S_CLAMP_BORDER                 (4  << 23)
#       define CLAMP_S_MIRROR_CLAMP_BORDER          (5  << 23)
#       define CLAMP_S_CLAMP_GL                     (6  << 23)
#       define CLAMP_S_MIRROR_CLAMP_GL              (7  << 23)
#       define CLAMP_S_MASK                         (7  << 23)
#       define WRAPEN_T                             (1  << 26)
#       define CLAMP_T_WRAP                         (0  << 27)
#       define CLAMP_T_MIRROR                       (1  << 27)
#       define CLAMP_T_CLAMP_LAST                   (2  << 27)
#       define CLAMP_T_MIRROR_CLAMP_LAST            (3  << 27)
#       define CLAMP_T_CLAMP_BORDER                 (4  << 27)
#       define CLAMP_T_MIRROR_CLAMP_BORDER          (5  << 27)
#       define CLAMP_T_CLAMP_GL                     (6  << 27)
#       define CLAMP_T_MIRROR_CLAMP_GL              (7  << 27)
#       define CLAMP_T_MASK                         (7  << 27)
#       define BORDER_MODE_OGL                      (0  << 31)
#       define BORDER_MODE_D3D                      (1  << 31)
#define PP_TXFORMAT_0                0x1c58
#define PP_TXFORMAT_1                0x1c70
#define PP_TXFORMAT_2                0x1c88
#       define TXFORMAT_I8                 (0  <<  0)
#       define TXFORMAT_AI88               (1  <<  0)
#       define TXFORMAT_RGB332             (2  <<  0)
#       define TXFORMAT_ARGB1555           (3  <<  0)
#       define TXFORMAT_RGB565             (4  <<  0)
#       define TXFORMAT_ARGB4444           (5  <<  0)
#       define TXFORMAT_ARGB8888           (6  <<  0)
#       define TXFORMAT_RGBA8888           (7  <<  0)
#       define TXFORMAT_Y8                 (8  <<  0)
#       define TXFORMAT_VYUY422            (10 <<  0)
#       define TXFORMAT_YVYU422            (11 <<  0)
#       define TXFORMAT_DXT1               (12 <<  0)
#       define TXFORMAT_DXT23              (14 <<  0)
#       define TXFORMAT_DXT45              (15 <<  0)
#       define TXFORMAT_FORMAT_MASK        (31 <<  0)
#       define TXFORMAT_FORMAT_SHIFT       0
#       define TXFORMAT_APPLE_YUV_MODE     (1  <<  5)
#       define TXFORMAT_ALPHA_IN_MAP       (1  <<  6)
#       define TXFORMAT_NON_POWER2         (1  <<  7)
#       define TXFORMAT_WIDTH_MASK         (15 <<  8)
#       define TXFORMAT_WIDTH_SHIFT        8
#       define TXFORMAT_HEIGHT_MASK        (15 << 12)
#       define TXFORMAT_HEIGHT_SHIFT       12
#       define TXFORMAT_F5_WIDTH_MASK      (15 << 16)
#       define TXFORMAT_F5_WIDTH_SHIFT     16
#       define TXFORMAT_F5_HEIGHT_MASK     (15 << 20)
#       define TXFORMAT_F5_HEIGHT_SHIFT    20
#       define TXFORMAT_ST_ROUTE_STQ0      (0  << 24)
#       define TXFORMAT_ST_ROUTE_MASK      (3  << 24)
#       define TXFORMAT_ST_ROUTE_STQ1      (1  << 24)
#       define TXFORMAT_ST_ROUTE_STQ2      (2  << 24)
#       define TXFORMAT_ENDIAN_NO_SWAP     (0  << 26)
#       define TXFORMAT_ENDIAN_16BPP_SWAP  (1  << 26)
#       define TXFORMAT_ENDIAN_32BPP_SWAP  (2  << 26)
#       define TXFORMAT_ENDIAN_HALFDW_SWAP (3  << 26)
#       define TXFORMAT_ALPHA_MASK_ENABLE  (1  << 28)
#       define TXFORMAT_CHROMA_KEY_ENABLE  (1  << 29)
#       define TXFORMAT_CUBIC_MAP_ENABLE   (1  << 30)
#       define TXFORMAT_PERSPECTIVE_ENABLE (1  << 31)
#define PP_CUBIC_FACES_0             0x1d24
#define PP_CUBIC_FACES_1             0x1d28
#define PP_CUBIC_FACES_2             0x1d2c
#       define FACE_WIDTH_1_SHIFT          0
#       define FACE_HEIGHT_1_SHIFT         4
#       define FACE_WIDTH_1_MASK           (0xf << 0)
#       define FACE_HEIGHT_1_MASK          (0xf << 4)
#       define FACE_WIDTH_2_SHIFT          8
#       define FACE_HEIGHT_2_SHIFT         12
#       define FACE_WIDTH_2_MASK           (0xf << 8)
#       define FACE_HEIGHT_2_MASK          (0xf << 12)
#       define FACE_WIDTH_3_SHIFT          16
#       define FACE_HEIGHT_3_SHIFT         20
#       define FACE_WIDTH_3_MASK           (0xf << 16)
#       define FACE_HEIGHT_3_MASK          (0xf << 20)
#       define FACE_WIDTH_4_SHIFT          24
#       define FACE_HEIGHT_4_SHIFT         28
#       define FACE_WIDTH_4_MASK           (0xf << 24)
#       define FACE_HEIGHT_4_MASK          (0xf << 28)

#define PP_TXOFFSET_0                0x1c5c
#define PP_TXOFFSET_1                0x1c74
#define PP_TXOFFSET_2                0x1c8c
#       define TXO_ENDIAN_NO_SWAP     (0 << 0)
#       define TXO_ENDIAN_BYTE_SWAP   (1 << 0)
#       define TXO_ENDIAN_WORD_SWAP   (2 << 0)
#       define TXO_ENDIAN_HALFDW_SWAP (3 << 0)
#       define TXO_MACRO_LINEAR       (0 << 2)
#       define TXO_MACRO_TILE         (1 << 2)
#       define TXO_MICRO_LINEAR       (0 << 3)
#       define TXO_MICRO_TILE_X2      (1 << 3)
#       define TXO_MICRO_TILE_OPT     (2 << 3)
#       define TXO_OFFSET_MASK        0xffffffe0
#       define TXO_OFFSET_SHIFT       5

#define PP_CUBIC_OFFSET_T0_0         0x1dd0  /* bits [31:5] */
#define PP_CUBIC_OFFSET_T0_1         0x1dd4
#define PP_CUBIC_OFFSET_T0_2         0x1dd8
#define PP_CUBIC_OFFSET_T0_3         0x1ddc
#define PP_CUBIC_OFFSET_T0_4         0x1de0
#define PP_CUBIC_OFFSET_T1_0         0x1e00
#define PP_CUBIC_OFFSET_T1_1         0x1e04
#define PP_CUBIC_OFFSET_T1_2         0x1e08
#define PP_CUBIC_OFFSET_T1_3         0x1e0c
#define PP_CUBIC_OFFSET_T1_4         0x1e10
#define PP_CUBIC_OFFSET_T2_0         0x1e14
#define PP_CUBIC_OFFSET_T2_1         0x1e18
#define PP_CUBIC_OFFSET_T2_2         0x1e1c
#define PP_CUBIC_OFFSET_T2_3         0x1e20
#define PP_CUBIC_OFFSET_T2_4         0x1e24

#define PP_TEX_SIZE_0                0x1d04  /* NPOT */
#define PP_TEX_SIZE_1                0x1d0c
#define PP_TEX_SIZE_2                0x1d14
#       define TEX_USIZE_MASK        (0x7ff << 0)
#       define TEX_USIZE_SHIFT       0
#       define TEX_VSIZE_MASK        (0x7ff << 16)
#       define TEX_VSIZE_SHIFT       16
#       define SIGNED_RGB_MASK       (1 << 30)
#       define SIGNED_RGB_SHIFT      30
#       define SIGNED_ALPHA_MASK     (1 << 31)
#       define SIGNED_ALPHA_SHIFT    31
#define PP_TEX_PITCH_0               0x1d08  /* NPOT */
#define PP_TEX_PITCH_1               0x1d10  /* NPOT */
#define PP_TEX_PITCH_2               0x1d18  /* NPOT */
/* note: bits 13-5: 32 byte aligned stride of texture map */

#define PP_TXCBLEND_0                0x1c60
#define PP_TXCBLEND_1                0x1c78
#define PP_TXCBLEND_2                0x1c90
#       define COLOR_ARG_A_SHIFT          0
#       define COLOR_ARG_A_MASK           (0x1f << 0)
#       define COLOR_ARG_A_ZERO           (0    << 0)
#       define COLOR_ARG_A_CURRENT_COLOR  (2    << 0)
#       define COLOR_ARG_A_CURRENT_ALPHA  (3    << 0)
#       define COLOR_ARG_A_DIFFUSE_COLOR  (4    << 0)
#       define COLOR_ARG_A_DIFFUSE_ALPHA  (5    << 0)
#       define COLOR_ARG_A_SPECULAR_COLOR (6    << 0)
#       define COLOR_ARG_A_SPECULAR_ALPHA (7    << 0)
#       define COLOR_ARG_A_TFACTOR_COLOR  (8    << 0)
#       define COLOR_ARG_A_TFACTOR_ALPHA  (9    << 0)
#       define COLOR_ARG_A_T0_COLOR       (10   << 0)
#       define COLOR_ARG_A_T0_ALPHA       (11   << 0)
#       define COLOR_ARG_A_T1_COLOR       (12   << 0)
#       define COLOR_ARG_A_T1_ALPHA       (13   << 0)
#       define COLOR_ARG_A_T2_COLOR       (14   << 0)
#       define COLOR_ARG_A_T2_ALPHA       (15   << 0)
#       define COLOR_ARG_A_T3_COLOR       (16   << 0)
#       define COLOR_ARG_A_T3_ALPHA       (17   << 0)
#       define COLOR_ARG_B_SHIFT          5
#       define COLOR_ARG_B_MASK           (0x1f << 5)
#       define COLOR_ARG_B_ZERO           (0    << 5)
#       define COLOR_ARG_B_CURRENT_COLOR  (2    << 5)
#       define COLOR_ARG_B_CURRENT_ALPHA  (3    << 5)
#       define COLOR_ARG_B_DIFFUSE_COLOR  (4    << 5)
#       define COLOR_ARG_B_DIFFUSE_ALPHA  (5    << 5)
#       define COLOR_ARG_B_SPECULAR_COLOR (6    << 5)
#       define COLOR_ARG_B_SPECULAR_ALPHA (7    << 5)
#       define COLOR_ARG_B_TFACTOR_COLOR  (8    << 5)
#       define COLOR_ARG_B_TFACTOR_ALPHA  (9    << 5)
#       define COLOR_ARG_B_T0_COLOR       (10   << 5)
#       define COLOR_ARG_B_T0_ALPHA       (11   << 5)
#       define COLOR_ARG_B_T1_COLOR       (12   << 5)
#       define COLOR_ARG_B_T1_ALPHA       (13   << 5)
#       define COLOR_ARG_B_T2_COLOR       (14   << 5)
#       define COLOR_ARG_B_T2_ALPHA       (15   << 5)
#       define COLOR_ARG_B_T3_COLOR       (16   << 5)
#       define COLOR_ARG_B_T3_ALPHA       (17   << 5)
#       define COLOR_ARG_C_SHIFT          10
#       define COLOR_ARG_C_MASK           (0x1f << 10)
#       define COLOR_ARG_C_ZERO           (0    << 10)
#       define COLOR_ARG_C_CURRENT_COLOR  (2    << 10)
#       define COLOR_ARG_C_CURRENT_ALPHA  (3    << 10)
#       define COLOR_ARG_C_DIFFUSE_COLOR  (4    << 10)
#       define COLOR_ARG_C_DIFFUSE_ALPHA  (5    << 10)
#       define COLOR_ARG_C_SPECULAR_COLOR (6    << 10)
#       define COLOR_ARG_C_SPECULAR_ALPHA (7    << 10)
#       define COLOR_ARG_C_TFACTOR_COLOR  (8    << 10)
#       define COLOR_ARG_C_TFACTOR_ALPHA  (9    << 10)
#       define COLOR_ARG_C_T0_COLOR       (10   << 10)
#       define COLOR_ARG_C_T0_ALPHA       (11   << 10)
#       define COLOR_ARG_C_T1_COLOR       (12   << 10)
#       define COLOR_ARG_C_T1_ALPHA       (13   << 10)
#       define COLOR_ARG_C_T2_COLOR       (14   << 10)
#       define COLOR_ARG_C_T2_ALPHA       (15   << 10)
#       define COLOR_ARG_C_T3_COLOR       (16   << 10)
#       define COLOR_ARG_C_T3_ALPHA       (17   << 10)
#       define COMP_ARG_A                 (1 << 15)
#       define COMP_ARG_A_SHIFT           15
#       define COMP_ARG_B                 (1 << 16)
#       define COMP_ARG_B_SHIFT           16
#       define COMP_ARG_C                 (1 << 17)
#       define COMP_ARG_C_SHIFT           17
#       define BLEND_CTL_MASK             (7 << 18)
#       define BLEND_CTL_ADD              (0 << 18)
#       define BLEND_CTL_SUBTRACT         (1 << 18)
#       define BLEND_CTL_ADDSIGNED        (2 << 18)
#       define BLEND_CTL_BLEND            (3 << 18)
#       define BLEND_CTL_DOT3             (4 << 18)
#       define SCALE_SHIFT                21
#       define SCALE_MASK                 (3 << 21)
#       define SCALE_1X                   (0 << 21)
#       define SCALE_2X                   (1 << 21)
#       define SCALE_4X                   (2 << 21)
#       define CLAMP_TX                   (1 << 23)
#       define T0_EQ_TCUR                 (1 << 24)
#       define T1_EQ_TCUR                 (1 << 25)
#       define T2_EQ_TCUR                 (1 << 26)
#       define T3_EQ_TCUR                 (1 << 27)
#       define COLOR_ARG_MASK             0x1f
#       define COMP_ARG_SHIFT             15
#define PP_TXABLEND_0                0x1c64
#define PP_TXABLEND_1                0x1c7c
#define PP_TXABLEND_2                0x1c94
#       define ALPHA_ARG_A_SHIFT          0
#       define ALPHA_ARG_A_MASK           (0xf << 0)
#       define ALPHA_ARG_A_ZERO           (0   << 0)
#       define ALPHA_ARG_A_CURRENT_ALPHA  (1   << 0)
#       define ALPHA_ARG_A_DIFFUSE_ALPHA  (2   << 0)
#       define ALPHA_ARG_A_SPECULAR_ALPHA (3   << 0)
#       define ALPHA_ARG_A_TFACTOR_ALPHA  (4   << 0)
#       define ALPHA_ARG_A_T0_ALPHA       (5   << 0)
#       define ALPHA_ARG_A_T1_ALPHA       (6   << 0)
#       define ALPHA_ARG_A_T2_ALPHA       (7   << 0)
#       define ALPHA_ARG_A_T3_ALPHA       (8   << 0)
#       define ALPHA_ARG_B_SHIFT          4
#       define ALPHA_ARG_B_MASK           (0xf << 4)
#       define ALPHA_ARG_B_ZERO           (0   << 4)
#       define ALPHA_ARG_B_CURRENT_ALPHA  (1   << 4)
#       define ALPHA_ARG_B_DIFFUSE_ALPHA  (2   << 4)
#       define ALPHA_ARG_B_SPECULAR_ALPHA (3   << 4)
#       define ALPHA_ARG_B_TFACTOR_ALPHA  (4   << 4)
#       define ALPHA_ARG_B_T0_ALPHA       (5   << 4)
#       define ALPHA_ARG_B_T1_ALPHA       (6   << 4)
#       define ALPHA_ARG_B_T2_ALPHA       (7   << 4)
#       define ALPHA_ARG_B_T3_ALPHA       (8   << 4)
#       define ALPHA_ARG_C_SHIFT          8
#       define ALPHA_ARG_C_MASK           (0xf << 8)
#       define ALPHA_ARG_C_ZERO           (0   << 8)
#       define ALPHA_ARG_C_CURRENT_ALPHA  (1   << 8)
#       define ALPHA_ARG_C_DIFFUSE_ALPHA  (2   << 8)
#       define ALPHA_ARG_C_SPECULAR_ALPHA (3   << 8)
#       define ALPHA_ARG_C_TFACTOR_ALPHA  (4   << 8)
#       define ALPHA_ARG_C_T0_ALPHA       (5   << 8)
#       define ALPHA_ARG_C_T1_ALPHA       (6   << 8)
#       define ALPHA_ARG_C_T2_ALPHA       (7   << 8)
#       define ALPHA_ARG_C_T3_ALPHA       (8   << 8)
#       define DOT_ALPHA_DONT_REPLICATE   (1   << 9)
#       define ALPHA_ARG_MASK             0xf

#define PP_TFACTOR_0                 0x1c68
#define PP_TFACTOR_1                 0x1c80
#define PP_TFACTOR_2                 0x1c98

#define RB3D_BLENDCNTL               0x1c20
#       define COMB_FCN_MASK                    (3  << 12)
#       define COMB_FCN_ADD_CLAMP               (0  << 12)
#       define COMB_FCN_ADD_NOCLAMP             (1  << 12)
#       define COMB_FCN_SUB_CLAMP               (2  << 12)
#       define COMB_FCN_SUB_NOCLAMP             (3  << 12)
#       define SRC_BLEND_GL_ZERO                (32 << 16)
#       define SRC_BLEND_GL_ONE                 (33 << 16)
#       define SRC_BLEND_GL_SRC_COLOR           (34 << 16)
#       define SRC_BLEND_GL_ONE_MINUS_SRC_COLOR (35 << 16)
#       define SRC_BLEND_GL_DST_COLOR           (36 << 16)
#       define SRC_BLEND_GL_ONE_MINUS_DST_COLOR (37 << 16)
#       define SRC_BLEND_GL_SRC_ALPHA           (38 << 16)
#       define SRC_BLEND_GL_ONE_MINUS_SRC_ALPHA (39 << 16)
#       define SRC_BLEND_GL_DST_ALPHA           (40 << 16)
#       define SRC_BLEND_GL_ONE_MINUS_DST_ALPHA (41 << 16)
#       define SRC_BLEND_GL_SRC_ALPHA_SATURATE  (42 << 16)
#       define SRC_BLEND_MASK                   (63 << 16)
#       define DST_BLEND_GL_ZERO                (32 << 24)
#       define DST_BLEND_GL_ONE                 (33 << 24)
#       define DST_BLEND_GL_SRC_COLOR           (34 << 24)
#       define DST_BLEND_GL_ONE_MINUS_SRC_COLOR (35 << 24)
#       define DST_BLEND_GL_DST_COLOR           (36 << 24)
#       define DST_BLEND_GL_ONE_MINUS_DST_COLOR (37 << 24)
#       define DST_BLEND_GL_SRC_ALPHA           (38 << 24)
#       define DST_BLEND_GL_ONE_MINUS_SRC_ALPHA (39 << 24)
#       define DST_BLEND_GL_DST_ALPHA           (40 << 24)
#       define DST_BLEND_GL_ONE_MINUS_DST_ALPHA (41 << 24)
#       define DST_BLEND_MASK                   (63 << 24)
#define RB3D_CNTL                    0x1c3c
#       define ALPHA_BLEND_ENABLE       (1  <<  0)
#       define PLANE_MASK_ENABLE        (1  <<  1)
#       define DITHER_ENABLE            (1  <<  2)
#       define ROUND_ENABLE             (1  <<  3)
#       define SCALE_DITHER_ENABLE      (1  <<  4)
#       define DITHER_INIT              (1  <<  5)
#       define ROP_ENABLE               (1  <<  6)
#       define STENCIL_ENABLE           (1  <<  7)
#       define Z_ENABLE                 (1  <<  8)
#       define DEPTH_XZ_OFFEST_ENABLE   (1  <<  9)
#       define COLOR_FORMAT_ARGB1555    (3  << 10)
#       define COLOR_FORMAT_RGB565      (4  << 10)
#       define COLOR_FORMAT_ARGB8888    (6  << 10)
#       define COLOR_FORMAT_RGB332      (7  << 10)
#       define COLOR_FORMAT_Y8          (8  << 10)
#       define COLOR_FORMAT_RGB8        (9  << 10)
#       define COLOR_FORMAT_YUV422_VYUY (11 << 10)
#       define COLOR_FORMAT_YUV422_YVYU (12 << 10)
#       define COLOR_FORMAT_AVYU        (14 << 10)
#       define COLOR_FORMAT_ARGB4444    (15 << 10)
#       define CLRCMP_FLIP_ENABLE       (1  << 14)
#       define SEPARATE_ALPHA_ENABLE    (1  << 16)
#define RB3D_COLOROFFSET             0x1c40
#       define COLOROFFSET_MASK      0xfffffff0
#define RB3D_COLORPITCH              0x1c48
#       define COLORPITCH_MASK         0x000001ff8
#       define COLOR_TILE_ENABLE       (1 << 16)
#       define COLOR_MICROTILE_ENABLE  (1 << 17)
#       define COLOR_ENDIAN_NO_SWAP    (0 << 18)
#       define COLOR_ENDIAN_WORD_SWAP  (1 << 18)
#       define COLOR_ENDIAN_DWORD_SWAP (2 << 18)
#define RB3D_DEPTHOFFSET             0x1c24
#define RB3D_DEPTHPITCH              0x1c28
#       define DEPTHPITCH_MASK         0x00001ff8
#       define DEPTH_ENDIAN_NO_SWAP    (0 << 18)
#       define DEPTH_ENDIAN_WORD_SWAP  (1 << 18)
#       define DEPTH_ENDIAN_DWORD_SWAP (2 << 18)
#define RB3D_PLANEMASK               0x1d84
#define RB3D_ROPCNTL                 0x1d80
#       define ROP_MASK              (15 << 8)
#       define ROP_CLEAR             (0  << 8)
#       define ROP_NOR               (1  << 8)
#       define ROP_AND_INVERTED      (2  << 8)
#       define ROP_COPY_INVERTED     (3  << 8)
#       define ROP_AND_REVERSE       (4  << 8)
#       define ROP_INVERT            (5  << 8)
#       define ROP_XOR               (6  << 8)
#       define ROP_NAND              (7  << 8)
#       define ROP_AND               (8  << 8)
#       define ROP_EQUIV             (9  << 8)
#       define ROP_NOOP              (10 << 8)
#       define ROP_OR_INVERTED       (11 << 8)
#       define ROP_COPY              (12 << 8)
#       define ROP_OR_REVERSE        (13 << 8)
#       define ROP_OR                (14 << 8)
#       define ROP_SET               (15 << 8)
#define RB3D_STENCILREFMASK          0x1d7c
#       define STENCIL_REF_SHIFT       0
#       define STENCIL_REF_MASK        (0xff << 0)
#       define STENCIL_MASK_SHIFT      16
#       define STENCIL_VALUE_MASK      (0xff << 16)
#       define STENCIL_WRITEMASK_SHIFT 24
#       define STENCIL_WRITE_MASK      (0xff << 24)
#define RB3D_ZSTENCILCNTL            0x1c2c
#       define DEPTH_FORMAT_MASK          (0xf << 0)
#       define DEPTH_FORMAT_16BIT_INT_Z   (0  <<  0)
#       define DEPTH_FORMAT_24BIT_INT_Z   (2  <<  0)
#       define DEPTH_FORMAT_24BIT_FLOAT_Z (3  <<  0)
#       define DEPTH_FORMAT_32BIT_INT_Z   (4  <<  0)
#       define DEPTH_FORMAT_32BIT_FLOAT_Z (5  <<  0)
#       define DEPTH_FORMAT_16BIT_FLOAT_W (7  <<  0)
#       define DEPTH_FORMAT_24BIT_FLOAT_W (9  <<  0)
#       define DEPTH_FORMAT_32BIT_FLOAT_W (11 <<  0)
#       define Z_TEST_NEVER               (0  <<  4)
#       define Z_TEST_LESS                (1  <<  4)
#       define Z_TEST_LEQUAL              (2  <<  4)
#       define Z_TEST_EQUAL               (3  <<  4)
#       define Z_TEST_GEQUAL              (4  <<  4)
#       define Z_TEST_GREATER             (5  <<  4)
#       define Z_TEST_NEQUAL              (6  <<  4)
#       define Z_TEST_ALWAYS              (7  <<  4)
#       define Z_TEST_MASK                (7  <<  4)
#       define STENCIL_TEST_NEVER         (0  << 12)
#       define STENCIL_TEST_LESS          (1  << 12)
#       define STENCIL_TEST_LEQUAL        (2  << 12)
#       define STENCIL_TEST_EQUAL         (3  << 12)
#       define STENCIL_TEST_GEQUAL        (4  << 12)
#       define STENCIL_TEST_GREATER       (5  << 12)
#       define STENCIL_TEST_NEQUAL        (6  << 12)
#       define STENCIL_TEST_ALWAYS        (7  << 12)
#       define STENCIL_TEST_MASK          (0x7 << 12)
#       define STENCIL_FAIL_KEEP          (0  << 16)
#       define STENCIL_FAIL_ZERO          (1  << 16)
#       define STENCIL_FAIL_REPLACE       (2  << 16)
#       define STENCIL_FAIL_INC           (3  << 16)
#       define STENCIL_FAIL_DEC           (4  << 16)
#       define STENCIL_FAIL_INVERT        (5  << 16)
#       define STENCIL_FAIL_MASK          (0x7 << 16)
#       define STENCIL_ZPASS_KEEP         (0  << 20)
#       define STENCIL_ZPASS_ZERO         (1  << 20)
#       define STENCIL_ZPASS_REPLACE      (2  << 20)
#       define STENCIL_ZPASS_INC          (3  << 20)
#       define STENCIL_ZPASS_DEC          (4  << 20)
#       define STENCIL_ZPASS_INVERT       (5  << 20)
#       define STENCIL_ZPASS_MASK         (0x7 << 20)
#       define STENCIL_ZFAIL_KEEP         (0  << 24)
#       define STENCIL_ZFAIL_ZERO         (1  << 24)
#       define STENCIL_ZFAIL_REPLACE      (2  << 24)
#       define STENCIL_ZFAIL_INC          (3  << 24)
#       define STENCIL_ZFAIL_DEC          (4  << 24)
#       define STENCIL_ZFAIL_INVERT       (5  << 24)
#       define STENCIL_ZFAIL_MASK         (0x7 << 24)
#       define Z_COMPRESSION_ENABLE       (1  << 28)
#       define FORCE_Z_DIRTY              (1  << 29)
#       define Z_WRITE_ENABLE             (1  << 30)
#define RE_LINE_PATTERN              0x1cd0
#       define LINE_PATTERN_MASK             0x0000ffff
#       define LINE_REPEAT_COUNT_SHIFT       16
#       define LINE_PATTERN_START_SHIFT      24
#       define LINE_PATTERN_LITTLE_BIT_ORDER (0 << 28)
#       define LINE_PATTERN_BIG_BIT_ORDER    (1 << 28)
#       define LINE_PATTERN_AUTO_RESET       (1 << 29)
#define RE_LINE_STATE                0x1cd4
#       define LINE_CURRENT_PTR_SHIFT   0
#       define LINE_CURRENT_COUNT_SHIFT 8
#define RE_MISC                      0x26c4
#       define STIPPLE_COORD_MASK       0x1f
#       define STIPPLE_X_OFFSET_SHIFT   0
#       define STIPPLE_X_OFFSET_MASK    (0x1f << 0)
#       define STIPPLE_Y_OFFSET_SHIFT   8
#       define STIPPLE_Y_OFFSET_MASK    (0x1f << 8)
#       define STIPPLE_LITTLE_BIT_ORDER (0 << 16)
#       define STIPPLE_BIG_BIT_ORDER    (1 << 16)
#define RE_SOLID_COLOR               0x1c1c
#define RE_POINTSIZE                 0x2648
#       define RE_POINTSIZE_SHIFT    0
#       define RE_MAXPOINTSIZE_SHIFT 16
#define RE_TOP_LEFT                  0x26c0
#       define RE_LEFT_SHIFT         0
#       define RE_TOP_SHIFT          16
#define RE_BOTTOM_RIGHT              0x1c44
#       define RE_RIGHT_SHIFT        0
#       define RE_BOTTOM_SHIFT       16

#define SE_CNTL                      0x1c4c
#       define FFACE_CULL_CW          (0 <<  0)
#       define FFACE_CULL_CCW         (1 <<  0)
#       define FFACE_CULL_DIR_MASK    (1 <<  0)
#       define BFACE_CULL             (0 <<  1)
#       define BFACE_SOLID            (3 <<  1)
#       define FFACE_CULL             (0 <<  3)
#       define FFACE_SOLID            (3 <<  3)
#       define FFACE_CULL_MASK        (3 <<  3)
#       define BADVTX_CULL_DISABLE    (1 <<  5)
#       define FLAT_SHADE_VTX_0       (0 <<  6)
#       define FLAT_SHADE_VTX_1       (1 <<  6)
#       define FLAT_SHADE_VTX_2       (2 <<  6)
#       define FLAT_SHADE_VTX_LAST    (3 <<  6)
#       define DIFFUSE_SHADE_SOLID    (0 <<  8)
#       define DIFFUSE_SHADE_FLAT     (1 <<  8)
#       define DIFFUSE_SHADE_GOURAUD  (2 <<  8)
#       define DIFFUSE_SHADE_MASK     (3 <<  8)
#       define ALPHA_SHADE_SOLID      (0 << 10)
#       define ALPHA_SHADE_FLAT       (1 << 10)
#       define ALPHA_SHADE_GOURAUD    (2 << 10)
#       define ALPHA_SHADE_MASK       (3 << 10)
#       define SPECULAR_SHADE_SOLID   (0 << 12)
#       define SPECULAR_SHADE_FLAT    (1 << 12)
#       define SPECULAR_SHADE_GOURAUD (2 << 12)
#       define SPECULAR_SHADE_MASK    (3 << 12)
#       define FOG_SHADE_SOLID        (0 << 14)
#       define FOG_SHADE_FLAT         (1 << 14)
#       define FOG_SHADE_GOURAUD      (2 << 14)
#       define FOG_SHADE_MASK         (3 << 14)
#       define ZBIAS_ENABLE_POINT     (1 << 16)
#       define ZBIAS_ENABLE_LINE      (1 << 17)
#       define ZBIAS_ENABLE_TRI       (1 << 18)
#       define WIDELINE_ENABLE        (1 << 20)
#       define VPORT_XY_XFORM_ENABLE  (1 << 24)
#       define VPORT_Z_XFORM_ENABLE   (1 << 25)
#       define VTX_PIX_CENTER_D3D     (0 << 27)
#       define VTX_PIX_CENTER_OGL     (1 << 27)
#       define ROUND_MODE_TRUNC       (0 << 28)
#       define ROUND_MODE_ROUND       (1 << 28)
#       define ROUND_MODE_ROUND_EVEN  (2 << 28)
#       define ROUND_MODE_ROUND_ODD   (3 << 28)
#       define ROUND_PREC_16TH_PIX    (0 << 30)
#       define ROUND_PREC_8TH_PIX     (1 << 30)
#       define ROUND_PREC_4TH_PIX     (2 << 30)
#       define ROUND_PREC_HALF_PIX    (3 << 30)
#define R200_RE_CNTL				0x1c50 
#       define R200_STIPPLE_ENABLE		0x1
#       define R200_SCISSOR_ENABLE		0x2
#       define R200_PATTERN_ENABLE		0x4
#       define R200_PERSPECTIVE_ENABLE		0x8
#       define R200_POINT_SMOOTH		0x20
#       define R200_VTX_STQ0_D3D		0x00010000
#       define R200_VTX_STQ1_D3D		0x00040000
#       define R200_VTX_STQ2_D3D		0x00100000
#       define R200_VTX_STQ3_D3D		0x00400000
#       define R200_VTX_STQ4_D3D		0x01000000
#       define R200_VTX_STQ5_D3D		0x04000000
#define SE_CNTL_STATUS               0x2140
#       define VC_NO_SWAP            (0 << 0)
#       define VC_16BIT_SWAP         (1 << 0)
#       define VC_32BIT_SWAP         (2 << 0)
#       define VC_HALF_DWORD_SWAP    (3 << 0)
#       define TCL_BYPASS            (1 << 8)
#define SE_COORD_FMT                 0x1c50
#       define VTX_XY_PRE_MULT_1_OVER_W0  (1 <<  0)
#       define VTX_Z_PRE_MULT_1_OVER_W0   (1 <<  1)
#       define VTX_ST0_NONPARAMETRIC      (1 <<  8)
#       define VTX_ST1_NONPARAMETRIC      (1 <<  9)
#       define VTX_ST2_NONPARAMETRIC      (1 << 10)
#       define VTX_ST3_NONPARAMETRIC      (1 << 11)
#       define VTX_W0_NORMALIZE           (1 << 12)
#       define VTX_W0_IS_NOT_1_OVER_W0    (1 << 16)
#       define VTX_ST0_PRE_MULT_1_OVER_W0 (1 << 17)
#       define VTX_ST1_PRE_MULT_1_OVER_W0 (1 << 19)
#       define VTX_ST2_PRE_MULT_1_OVER_W0 (1 << 21)
#       define VTX_ST3_PRE_MULT_1_OVER_W0 (1 << 23)
#       define TEX1_W_ROUTING_USE_W0      (0 << 26)
#       define TEX1_W_ROUTING_USE_Q1      (1 << 26)
#define SE_LINE_WIDTH                0x1db8
#define SE_TCL_LIGHT_MODEL_CTL       0x226c
#       define LIGHTING_ENABLE              (1 << 0)
#       define LIGHT_IN_MODELSPACE          (1 << 1)
#       define LOCAL_VIEWER                 (1 << 2)
#       define NORMALIZE_NORMALS            (1 << 3)
#       define RESCALE_NORMALS              (1 << 4)
#       define SPECULAR_LIGHTS              (1 << 5)
#       define DIFFUSE_SPECULAR_COMBINE     (1 << 6)
#       define LIGHT_ALPHA                  (1 << 7)
#       define LOCAL_LIGHT_VEC_GL           (1 << 8)
#       define LIGHT_NO_NORMAL_AMBIENT_ONLY (1 << 9)
#       define LM_SOURCE_STATE_PREMULT      0
#       define LM_SOURCE_STATE_MULT         1
#       define LM_SOURCE_VERTEX_DIFFUSE     2
#       define LM_SOURCE_VERTEX_SPECULAR    3
#       define EMISSIVE_SOURCE_SHIFT        16
#       define AMBIENT_SOURCE_SHIFT         18
#       define DIFFUSE_SOURCE_SHIFT         20
#       define SPECULAR_SOURCE_SHIFT        22
#define SE_TCL_MATERIAL_AMBIENT_RED     0x2220
#define SE_TCL_MATERIAL_AMBIENT_GREEN   0x2224
#define SE_TCL_MATERIAL_AMBIENT_BLUE    0x2228
#define SE_TCL_MATERIAL_AMBIENT_ALPHA   0x222c
#define SE_TCL_MATERIAL_DIFFUSE_RED     0x2230
#define SE_TCL_MATERIAL_DIFFUSE_GREEN   0x2234
#define SE_TCL_MATERIAL_DIFFUSE_BLUE    0x2238
#define SE_TCL_MATERIAL_DIFFUSE_ALPHA   0x223c
#define SE_TCL_MATERIAL_EMMISSIVE_RED   0x2210
#define SE_TCL_MATERIAL_EMMISSIVE_GREEN 0x2214
#define SE_TCL_MATERIAL_EMMISSIVE_BLUE  0x2218
#define SE_TCL_MATERIAL_EMMISSIVE_ALPHA 0x221c
#define SE_TCL_MATERIAL_SPECULAR_RED    0x2240
#define SE_TCL_MATERIAL_SPECULAR_GREEN  0x2244
#define SE_TCL_MATERIAL_SPECULAR_BLUE   0x2248
#define SE_TCL_MATERIAL_SPECULAR_ALPHA  0x224c
#define SE_TCL_MATRIX_SELECT_0       0x225c
#       define MODELVIEW_0_SHIFT        0
#       define MODELVIEW_1_SHIFT        4
#       define MODELVIEW_2_SHIFT        8
#       define MODELVIEW_3_SHIFT        12
#       define IT_MODELVIEW_0_SHIFT     16
#       define IT_MODELVIEW_1_SHIFT     20
#       define IT_MODELVIEW_2_SHIFT     24
#       define IT_MODELVIEW_3_SHIFT     28
#define SE_TCL_MATRIX_SELECT_1       0x2260
#       define MODELPROJECT_0_SHIFT     0
#       define MODELPROJECT_1_SHIFT     4
#       define MODELPROJECT_2_SHIFT     8
#       define MODELPROJECT_3_SHIFT     12
#       define TEXMAT_0_SHIFT           16
#       define TEXMAT_1_SHIFT           20
#       define TEXMAT_2_SHIFT           24
#       define TEXMAT_3_SHIFT           28


#define SE_TCL_OUTPUT_VTX_FMT        0x2254
#       define TCL_VTX_W0                 (1 <<  0)
#       define TCL_VTX_FP_DIFFUSE         (1 <<  1)
#       define TCL_VTX_FP_ALPHA           (1 <<  2)
#       define TCL_VTX_PK_DIFFUSE         (1 <<  3)
#       define TCL_VTX_FP_SPEC            (1 <<  4)
#       define TCL_VTX_FP_FOG             (1 <<  5)
#       define TCL_VTX_PK_SPEC            (1 <<  6)
#       define TCL_VTX_ST0                (1 <<  7)
#       define TCL_VTX_ST1                (1 <<  8)
#       define TCL_VTX_Q1                 (1 <<  9)
#       define TCL_VTX_ST2                (1 << 10)
#       define TCL_VTX_Q2                 (1 << 11)
#       define TCL_VTX_ST3                (1 << 12)
#       define TCL_VTX_Q3                 (1 << 13)
#       define TCL_VTX_Q0                 (1 << 14)
#       define TCL_VTX_WEIGHT_COUNT_SHIFT 15
#       define TCL_VTX_NORM0              (1 << 18)
#       define TCL_VTX_XY1                (1 << 27)
#       define TCL_VTX_Z1                 (1 << 28)
#       define TCL_VTX_W1                 (1 << 29)
#       define TCL_VTX_NORM1              (1 << 30)
#       define TCL_VTX_Z0                 (1 << 31)

#define SE_TCL_OUTPUT_VTX_SEL        0x2258
#       define TCL_COMPUTE_XYZW           (1 << 0)
#       define TCL_COMPUTE_DIFFUSE        (1 << 1)
#       define TCL_COMPUTE_SPECULAR       (1 << 2)
#       define TCL_FORCE_NAN_IF_COLOR_NAN (1 << 3)
#       define TCL_FORCE_INORDER_PROC     (1 << 4)
#       define TCL_TEX_INPUT_TEX_0        0
#       define TCL_TEX_INPUT_TEX_1        1
#       define TCL_TEX_INPUT_TEX_2        2
#       define TCL_TEX_INPUT_TEX_3        3
#       define TCL_TEX_COMPUTED_TEX_0     8
#       define TCL_TEX_COMPUTED_TEX_1     9
#       define TCL_TEX_COMPUTED_TEX_2     10
#       define TCL_TEX_COMPUTED_TEX_3     11
#       define TCL_TEX_0_OUTPUT_SHIFT     16
#       define TCL_TEX_1_OUTPUT_SHIFT     20
#       define TCL_TEX_2_OUTPUT_SHIFT     24
#       define TCL_TEX_3_OUTPUT_SHIFT     28

#define SE_TCL_PER_LIGHT_CTL_0       0x2270
#       define LIGHT_0_ENABLE               (1 <<  0)
#       define LIGHT_0_ENABLE_AMBIENT       (1 <<  1)
#       define LIGHT_0_ENABLE_SPECULAR      (1 <<  2)
#       define LIGHT_0_IS_LOCAL             (1 <<  3)
#       define LIGHT_0_IS_SPOT              (1 <<  4)
#       define LIGHT_0_DUAL_CONE            (1 <<  5)
#       define LIGHT_0_ENABLE_RANGE_ATTEN   (1 <<  6)
#       define LIGHT_0_CONSTANT_RANGE_ATTEN (1 <<  7)
#       define LIGHT_0_SHIFT                0
#       define LIGHT_1_ENABLE               (1 << 16)
#       define LIGHT_1_ENABLE_AMBIENT       (1 << 17)
#       define LIGHT_1_ENABLE_SPECULAR      (1 << 18)
#       define LIGHT_1_IS_LOCAL             (1 << 19)
#       define LIGHT_1_IS_SPOT              (1 << 20)
#       define LIGHT_1_DUAL_CONE            (1 << 21)
#       define LIGHT_1_ENABLE_RANGE_ATTEN   (1 << 22)
#       define LIGHT_1_CONSTANT_RANGE_ATTEN (1 << 23)
#       define LIGHT_1_SHIFT                16
#define SE_TCL_PER_LIGHT_CTL_1       0x2274
#       define LIGHT_2_SHIFT            0
#       define LIGHT_3_SHIFT            16
#define SE_TCL_PER_LIGHT_CTL_2       0x2278
#       define LIGHT_4_SHIFT            0
#       define LIGHT_5_SHIFT            16
#define SE_TCL_PER_LIGHT_CTL_3       0x227c
#       define LIGHT_6_SHIFT            0
#       define LIGHT_7_SHIFT            16

#define SE_TCL_SHININESS             0x2250

#define SE_TCL_TEXTURE_PROC_CTL      0x2268
#       define TEXGEN_TEXMAT_0_ENABLE      (1 << 0)
#       define TEXGEN_TEXMAT_1_ENABLE      (1 << 1)
#       define TEXGEN_TEXMAT_2_ENABLE      (1 << 2)
#       define TEXGEN_TEXMAT_3_ENABLE      (1 << 3)
#       define TEXMAT_0_ENABLE             (1 << 4)
#       define TEXMAT_1_ENABLE             (1 << 5)
#       define TEXMAT_2_ENABLE             (1 << 6)
#       define TEXMAT_3_ENABLE             (1 << 7)
#       define TEXGEN_INPUT_MASK           0xf
#       define TEXGEN_INPUT_TEXCOORD_0     0
#       define TEXGEN_INPUT_TEXCOORD_1     1
#       define TEXGEN_INPUT_TEXCOORD_2     2
#       define TEXGEN_INPUT_TEXCOORD_3     3
#       define TEXGEN_INPUT_OBJ            4
#       define TEXGEN_INPUT_EYE            5
#       define TEXGEN_INPUT_EYE_NORMAL     6
#       define TEXGEN_INPUT_EYE_REFLECT    7
#       define TEXGEN_INPUT_EYE_NORMALIZED 8
#       define TEXGEN_0_INPUT_SHIFT        16
#       define TEXGEN_1_INPUT_SHIFT        20
#       define TEXGEN_2_INPUT_SHIFT        24
#       define TEXGEN_3_INPUT_SHIFT        28

#define SE_TCL_UCP_VERT_BLEND_CTL    0x2264
#       define UCP_IN_CLIP_SPACE            (1 <<  0)
#       define UCP_IN_MODEL_SPACE           (1 <<  1)
#       define UCP_ENABLE_0                 (1 <<  2)
#       define UCP_ENABLE_1                 (1 <<  3)
#       define UCP_ENABLE_2                 (1 <<  4)
#       define UCP_ENABLE_3                 (1 <<  5)
#       define UCP_ENABLE_4                 (1 <<  6)
#       define UCP_ENABLE_5                 (1 <<  7)
#       define TCL_FOG_MASK                 (3 <<  8)
#       define TCL_FOG_DISABLE              (0 <<  8)
#       define TCL_FOG_EXP                  (1 <<  8)
#       define TCL_FOG_EXP2                 (2 <<  8)
#       define TCL_FOG_LINEAR               (3 <<  8)
#       define RNG_BASED_FOG                (1 << 10)
#       define LIGHT_TWOSIDE                (1 << 11)
#       define BLEND_OP_COUNT_MASK          (7 << 12)
#       define BLEND_OP_COUNT_SHIFT         12
#       define POSITION_BLEND_OP_ENABLE     (1 << 16)
#       define NORMAL_BLEND_OP_ENABLE       (1 << 17)
#       define VERTEX_BLEND_SRC_0_PRIMARY   (1 << 18)
#       define VERTEX_BLEND_SRC_0_SECONDARY (1 << 18)
#       define VERTEX_BLEND_SRC_1_PRIMARY   (1 << 19)
#       define VERTEX_BLEND_SRC_1_SECONDARY (1 << 19)
#       define VERTEX_BLEND_SRC_2_PRIMARY   (1 << 20)
#       define VERTEX_BLEND_SRC_2_SECONDARY (1 << 20)
#       define VERTEX_BLEND_SRC_3_PRIMARY   (1 << 21)
#       define VERTEX_BLEND_SRC_3_SECONDARY (1 << 21)
#       define VERTEX_BLEND_WGT_MINUS_ONE   (1 << 22)
#       define CULL_FRONT_IS_CW             (0 << 28)
#       define CULL_FRONT_IS_CCW            (1 << 28)
#       define CULL_FRONT                   (1 << 29)
#       define CULL_BACK                    (1 << 30)
#       define FORCE_W_TO_ONE               (1 << 31)

#define SE_VPORT_XSCALE              0x1d98
#define SE_VPORT_XOFFSET             0x1d9c
#define SE_VPORT_YSCALE              0x1da0
#define SE_VPORT_YOFFSET             0x1da4
#define SE_VPORT_ZSCALE              0x1da8
#define SE_VPORT_ZOFFSET             0x1dac
#define SE_ZBIAS_FACTOR              0x1db0
#define SE_ZBIAS_CONSTANT            0x1db4

#define SE_VTX_FMT                   0x2080
#       define SE_VTX_FMT_XY         0x00000000
#       define SE_VTX_FMT_W0         0x00000001
#       define SE_VTX_FMT_FPCOLOR    0x00000002
#       define SE_VTX_FMT_FPALPHA    0x00000004
#       define SE_VTX_FMT_PKCOLOR    0x00000008
#       define SE_VTX_FMT_FPSPEC     0x00000010
#       define SE_VTX_FMT_FPFOG      0x00000020
#       define SE_VTX_FMT_PKSPEC     0x00000040
#       define SE_VTX_FMT_ST0        0x00000080
#       define SE_VTX_FMT_ST1        0x00000100
#       define SE_VTX_FMT_Q1         0x00000200
#       define SE_VTX_FMT_ST2        0x00000400
#       define SE_VTX_FMT_Q2         0x00000800
#       define SE_VTX_FMT_ST3        0x00001000
#       define SE_VTX_FMT_Q3         0x00002000
#       define SE_VTX_FMT_Q0         0x00004000
#       define SE_VTX_FMT_BLND_WEIGHT_CNT_MASK  0x00038000
#       define SE_VTX_FMT_N0         0x00040000
#       define SE_VTX_FMT_XY1        0x08000000
#       define SE_VTX_FMT_Z1         0x10000000
#       define SE_VTX_FMT_W1         0x20000000
#       define SE_VTX_FMT_N1         0x40000000
#       define SE_VTX_FMT_Z          0x80000000

#define SE_VF_CNTL                             0x2084
#       define VF_PRIM_TYPE_POINT_LIST         1
#       define VF_PRIM_TYPE_LINE_LIST          2
#       define VF_PRIM_TYPE_LINE_STRIP         3
#       define VF_PRIM_TYPE_TRIANGLE_LIST      4
#       define VF_PRIM_TYPE_TRIANGLE_FAN       5
#       define VF_PRIM_TYPE_TRIANGLE_STRIP     6
#       define VF_PRIM_TYPE_TRIANGLE_FLAG      7
#       define VF_PRIM_TYPE_RECTANGLE_LIST     8
#       define VF_PRIM_TYPE_POINT_LIST_3       9
#       define VF_PRIM_TYPE_LINE_LIST_3        10
#       define VF_PRIM_TYPE_SPIRIT_LIST        11
#       define VF_PRIM_TYPE_LINE_LOOP          12
#       define VF_PRIM_TYPE_QUAD_LIST          13
#       define VF_PRIM_TYPE_QUAD_STRIP         14
#       define VF_PRIM_TYPE_POLYGON            15
#       define VF_PRIM_WALK_STATE              (0<<4)
#       define VF_PRIM_WALK_INDEX              (1<<4)
#       define VF_PRIM_WALK_LIST               (2<<4)
#       define VF_PRIM_WALK_DATA               (3<<4)
#       define VF_COLOR_ORDER_RGBA             (1<<6)
#       define VF_RADEON_MODE                  (1<<8)
#       define VF_TCL_OUTPUT_CTL_ENA           (1<<9)
#       define VF_PROG_STREAM_ENA              (1<<10)
#       define VF_INDEX_SIZE_SHIFT             11
#       define VF_NUM_VERTICES_SHIFT           16

#define SE_PORT_DATA0			0x2000
 
#define R200_SE_VAP_CNTL			0x2080
#       define R200_VAP_TCL_ENABLE		0x00000001
#       define R200_VAP_SINGLE_BUF_STATE_ENABLE	0x00000010
#       define R200_VAP_FORCE_W_TO_ONE		0x00010000
#       define R200_VAP_D3D_TEX_DEFAULT		0x00020000
#       define R200_VAP_VF_MAX_VTX_NUM__SHIFT	18
#       define R200_VAP_VF_MAX_VTX_NUM		(9 << 18)
#       define R200_VAP_DX_CLIP_SPACE_DEF	0x00400000
#define R200_VF_MAX_VTX_INDX			0x210c
#define R200_VF_MIN_VTX_INDX			0x2110
#define R200_SE_VTE_CNTL			0x20b0
#       define R200_VPORT_X_SCALE_ENA			0x00000001
#       define R200_VPORT_X_OFFSET_ENA			0x00000002
#       define R200_VPORT_Y_SCALE_ENA			0x00000004
#       define R200_VPORT_Y_OFFSET_ENA			0x00000008
#       define R200_VPORT_Z_SCALE_ENA			0x00000010
#       define R200_VPORT_Z_OFFSET_ENA			0x00000020
#       define R200_VTX_XY_FMT				0x00000100
#       define R200_VTX_Z_FMT				0x00000200
#       define R200_VTX_W0_FMT				0x00000400
#       define R200_VTX_W0_NORMALIZE			0x00000800
#       define R200_VTX_ST_DENORMALIZED		0x00001000
#define R200_SE_VAP_CNTL_STATUS			0x2140
#       define R200_VC_NO_SWAP			(0 << 0)
#       define R200_VC_16BIT_SWAP            (1 << 0)
#       define R200_VC_32BIT_SWAP            (2 << 0)
#       define R200_TCL_BYPASS               (1 << 8)
#define R200_PP_TXFILTER_0			0x2c00
#define R200_PP_TXFILTER_1              0x2c20
#       define R200_MAG_FILTER_NEAREST		(0  <<  0)
#       define R200_MAG_FILTER_LINEAR		(1  <<  0)
#       define R200_MAG_FILTER_MASK		(1  <<  0)
#       define R200_MIN_FILTER_NEAREST		(0  <<  1)
#       define R200_MIN_FILTER_LINEAR		(1  <<  1)
#       define R200_MIN_FILTER_NEAREST_MIP_NEAREST (2  <<  1)
#       define R200_MIN_FILTER_NEAREST_MIP_LINEAR (3  <<  1)
#       define R200_MIN_FILTER_LINEAR_MIP_NEAREST (6  <<  1)
#       define R200_MIN_FILTER_LINEAR_MIP_LINEAR (7  <<  1)
#       define R200_MIN_FILTER_ANISO_NEAREST	(8  <<  1)
#       define R200_MIN_FILTER_ANISO_LINEAR	(9  <<  1)
#       define R200_MIN_FILTER_ANISO_NEAREST_MIP_NEAREST (10 <<  1)
#       define R200_MIN_FILTER_ANISO_NEAREST_MIP_LINEAR (11 <<  1)
#       define R200_MIN_FILTER_MASK		(15 <<  1)
#       define R200_MAX_ANISO_1_TO_1		(0  <<  5)
#       define R200_MAX_ANISO_2_TO_1		(1  <<  5)
#       define R200_MAX_ANISO_4_TO_1		(2  <<  5)
#       define R200_MAX_ANISO_8_TO_1		(3  <<  5)
#       define R200_MAX_ANISO_16_TO_1		(4  <<  5)
#       define R200_MAX_ANISO_MASK		(7  <<  5)
#       define R200_MAX_MIP_LEVEL_MASK		(0x0f << 16)
#       define R200_MAX_MIP_LEVEL_SHIFT		16
#       define R200_YUV_TO_RGB			(1  << 20)
#       define R200_YUV_TEMPERATURE_COOL	(0  << 21)
#       define R200_YUV_TEMPERATURE_HOT		(1  << 21)
#       define R200_YUV_TEMPERATURE_MASK	(1  << 21)
#       define R200_WRAPEN_S			(1  << 22)
#       define R200_CLAMP_S_WRAP		(0  << 23)
#       define R200_CLAMP_S_MIRROR		(1  << 23)
#       define R200_CLAMP_S_CLAMP_LAST		(2  << 23)
#       define R200_CLAMP_S_MIRROR_CLAMP_LAST	(3  << 23)
#       define R200_CLAMP_S_CLAMP_BORDER	(4  << 23)
#       define R200_CLAMP_S_MIRROR_CLAMP_BORDER	(5  << 23)
#       define R200_CLAMP_S_CLAMP_GL		(6  << 23)
#       define R200_CLAMP_S_MIRROR_CLAMP_GL	(7  << 23)
#       define R200_CLAMP_S_MASK		(7  << 23)
#       define R200_WRAPEN_T			(1  << 26)
#       define R200_CLAMP_T_WRAP		(0  << 27)
#       define R200_CLAMP_T_MIRROR		(1  << 27)
#       define R200_CLAMP_T_CLAMP_LAST		(2  << 27)
#       define R200_CLAMP_T_MIRROR_CLAMP_LAST	(3  << 27)
#       define R200_CLAMP_T_CLAMP_BORDER	(4  << 27)
#       define R200_CLAMP_T_MIRROR_CLAMP_BORDER	(5  << 27)
#       define R200_CLAMP_T_CLAMP_GL		(6  << 27)
#       define R200_CLAMP_T_MIRROR_CLAMP_GL	(7  << 27)
#       define R200_CLAMP_T_MASK		(7  << 27)
#       define R200_KILL_LT_ZERO		(1  << 30)
#       define R200_BORDER_MODE_OGL		(0  << 31)
#       define R200_BORDER_MODE_D3D		(1  << 31)
#define R200_PP_TXFORMAT_0             0x2c04
#define R200_PP_TXFORMAT_1             0x2c24
#       define R200_TXFORMAT_I8			(0 << 0)
#       define R200_TXFORMAT_AI88	          (1 << 0)
#       define R200_TXFORMAT_RGB332		(2 << 0)
#       define R200_TXFORMAT_ARGB1555		(3 << 0)
#       define R200_TXFORMAT_RGB565		(4 << 0)
#       define R200_TXFORMAT_ARGB4444		(5 << 0)
#       define R200_TXFORMAT_ARGB8888		(6 << 0)
#       define R200_TXFORMAT_RGBA8888		(7 << 0)
#       define R200_TXFORMAT_Y8              (8 << 0)
#       define R200_TXFORMAT_AVYU            (9 << 0)
#       define R200_TXFORMAT_VYUY422		(10 << 0)
#       define R200_TXFORMAT_YVYU422		(11 << 0)
#       define R200_TXFORMAT_DXT1	          (12 << 0)
#       define R200_TXFORMAT_DXT23           (14 << 0)
#       define R200_TXFORMAT_DXT45           (15 << 0)
#       define R200_TXFORMAT_FORMAT_MASK	(31 << 0)
#       define R200_TXFORMAT_FORMAT_SHIFT	0
#       define R200_TXFORMAT_ALPHA_IN_MAP	(1 << 6)
#       define R200_TXFORMAT_NON_POWER2		(1 << 7)
#       define R200_TXFORMAT_WIDTH_MASK		(15 <<	8)
#       define R200_TXFORMAT_WIDTH_SHIFT	8
#       define R200_TXFORMAT_HEIGHT_MASK	(15 << 12)
#       define R200_TXFORMAT_HEIGHT_SHIFT	12
#       define R200_TXFORMAT_F5_WIDTH_MASK	(15 << 16)	/* cube face 5 */
#       define R200_TXFORMAT_F5_WIDTH_SHIFT	16
#       define R200_TXFORMAT_F5_HEIGHT_MASK	(15 << 20)
#       define R200_TXFORMAT_F5_HEIGHT_SHIFT	20
#       define R200_TXFORMAT_ST_ROUTE_STQ0	(0 << 24)
#       define R200_TXFORMAT_ST_ROUTE_STQ1	(1 << 24)
#       define R200_TXFORMAT_ST_ROUTE_STQ2	(2 << 24)
#       define R200_TXFORMAT_ST_ROUTE_STQ3	(3 << 24)
#       define R200_TXFORMAT_ST_ROUTE_STQ4	(4 << 24)
#       define R200_TXFORMAT_ST_ROUTE_STQ5	(5 << 24)
#       define R200_TXFORMAT_ST_ROUTE_MASK	(7 << 24)
#       define R200_TXFORMAT_ST_ROUTE_SHIFT	24
#       define R200_TXFORMAT_ALPHA_MASK_ENABLE	(1 << 28)
#       define R200_TXFORMAT_CHROMA_KEY_ENABLE	(1 << 29)
#       define R200_TXFORMAT_CUBIC_MAP_ENABLE		(1 << 30)
#define R200_PP_TXFORMAT_X_0            0x2c08
#define R200_PP_TXFORMAT_X_1            0x2c28
#define     R200_DEPTH_LOG2_MASK                      (0xf << 0)
#define     R200_DEPTH_LOG2_SHIFT                     0
#define     R200_VOLUME_FILTER_SHIFT                  4
#define     R200_VOLUME_FILTER_MASK                   (1 << 4)
#define     R200_VOLUME_FILTER_NEAREST                (0 << 4)
#define     R200_VOLUME_FILTER_LINEAR                 (1 << 4)
#define     R200_WRAPEN_Q                             (1  << 8)
#define     R200_CLAMP_Q_WRAP                         (0  << 9)
#define     R200_CLAMP_Q_MIRROR                       (1  << 9)
#define     R200_CLAMP_Q_CLAMP_LAST                   (2  << 9)
#define     R200_CLAMP_Q_MIRROR_CLAMP_LAST            (3  << 9)
#define     R200_CLAMP_Q_CLAMP_BORDER                 (4  << 9)
#define     R200_CLAMP_Q_MIRROR_CLAMP_BORDER          (5  << 9)
#define     R200_CLAMP_Q_CLAMP_GL                     (6  << 9)
#define     R200_CLAMP_Q_MIRROR_CLAMP_GL              (7  << 9)
#define     R200_CLAMP_Q_MASK                         (7  << 9)
#define     R200_MIN_MIP_LEVEL_MASK                   (0xff << 12)
#define     R200_MIN_MIP_LEVEL_SHIFT                  12
#define     R200_TEXCOORD_NONPROJ                     (0  << 16)
#define     R200_TEXCOORD_CUBIC_ENV                   (1  << 16)
#define     R200_TEXCOORD_VOLUME                      (2  << 16)
#define     R200_TEXCOORD_PROJ                        (3  << 16)
#define     R200_TEXCOORD_DEPTH                       (4  << 16)
#define     R200_TEXCOORD_1D_PROJ                     (5  << 16)
#define     R200_TEXCOORD_1D                          (6  << 16)
#define     R200_TEXCOORD_ZERO                        (7  << 16)
#define     R200_TEXCOORD_MASK                        (7  << 16)
#define     R200_LOD_BIAS_MASK                        (0xfff80000)
#define     R200_LOD_BIAS_SHIFT                       19
#define R200_PP_TXSIZE_0	               0x2c0c /* NPOT only */
#define R200_PP_TXSIZE_1                0x2c2c
#define R200_PP_TXPITCH_0               0x2c10 /* NPOT only */
#define R200_PP_TXPITCH_1               0x2c30
#define R200_PP_TXOFFSET_0			0x2d00
#define R200_PP_TXOFFSET_1              0x2d18
#       define R200_TXO_ENDIAN_NO_SWAP		(0 << 0)
#       define R200_TXO_ENDIAN_BYTE_SWAP	(1 << 0)
#       define R200_TXO_ENDIAN_WORD_SWAP	(2 << 0)
#       define R200_TXO_ENDIAN_HALFDW_SWAP	(3 << 0)
#       define R200_TXO_OFFSET_MASK		0xffffffe0
#       define R200_TXO_OFFSET_SHIFT		5


#define R200_PP_TFACTOR_0			0x2ee0
#define R200_PP_TFACTOR_1			0x2ee4
#define R200_PP_TFACTOR_2			0x2ee8
#define R200_PP_TFACTOR_3			0x2eec
#define R200_PP_TFACTOR_4			0x2ef0
#define R200_PP_TFACTOR_5			0x2ef4

#define R200_PP_TXCBLEND_0			0x2f00
#define R200_PP_TXCBLEND_1              0x2f10
#       define R200_TXC_ARG_A_ZERO		(0)
#       define R200_TXC_ARG_A_CURRENT_COLOR	(2)
#       define R200_TXC_ARG_A_CURRENT_ALPHA	(3)
#       define R200_TXC_ARG_A_DIFFUSE_COLOR	(4)
#       define R200_TXC_ARG_A_DIFFUSE_ALPHA	(5)
#       define R200_TXC_ARG_A_SPECULAR_COLOR	(6)
#       define R200_TXC_ARG_A_SPECULAR_ALPHA	(7)
#       define R200_TXC_ARG_A_TFACTOR_COLOR	(8)
#       define R200_TXC_ARG_A_TFACTOR_ALPHA	(9)
#       define R200_TXC_ARG_A_R0_COLOR		(10)
#       define R200_TXC_ARG_A_R0_ALPHA		(11)
#       define R200_TXC_ARG_A_R1_COLOR		(12)
#       define R200_TXC_ARG_A_R1_ALPHA		(13)
#       define R200_TXC_ARG_A_R2_COLOR		(14)
#       define R200_TXC_ARG_A_R2_ALPHA		(15)
#       define R200_TXC_ARG_A_R3_COLOR		(16)
#       define R200_TXC_ARG_A_R3_ALPHA		(17)
#       define R200_TXC_ARG_A_R4_COLOR		(18)
#       define R200_TXC_ARG_A_R4_ALPHA		(19)
#       define R200_TXC_ARG_A_R5_COLOR		(20)
#       define R200_TXC_ARG_A_R5_ALPHA		(21)
#       define R200_TXC_ARG_A_TFACTOR1_COLOR	(26)
#       define R200_TXC_ARG_A_TFACTOR1_ALPHA	(27)
#       define R200_TXC_ARG_A_MASK		(31 << 0)
#       define R200_TXC_ARG_A_SHIFT		0
#       define R200_TXC_ARG_B_ZERO		(0 << 5)
#       define R200_TXC_ARG_B_CURRENT_COLOR	(2 << 5)
#       define R200_TXC_ARG_B_CURRENT_ALPHA	(3 << 5)
#       define R200_TXC_ARG_B_DIFFUSE_COLOR	(4 << 5)
#       define R200_TXC_ARG_B_DIFFUSE_ALPHA	(5 << 5)
#       define R200_TXC_ARG_B_SPECULAR_COLOR	(6 << 5)
#       define R200_TXC_ARG_B_SPECULAR_ALPHA	(7 << 5)
#       define R200_TXC_ARG_B_TFACTOR_COLOR	(8 << 5)
#       define R200_TXC_ARG_B_TFACTOR_ALPHA	(9 << 5)
#       define R200_TXC_ARG_B_R0_COLOR		(10 << 5)
#       define R200_TXC_ARG_B_R0_ALPHA		(11 << 5)
#       define R200_TXC_ARG_B_R1_COLOR		(12 << 5)
#       define R200_TXC_ARG_B_R1_ALPHA		(13 << 5)
#       define R200_TXC_ARG_B_R2_COLOR		(14 << 5)
#       define R200_TXC_ARG_B_R2_ALPHA		(15 << 5)
#       define R200_TXC_ARG_B_R3_COLOR		(16 << 5)
#       define R200_TXC_ARG_B_R3_ALPHA		(17 << 5)
#       define R200_TXC_ARG_B_R4_COLOR		(18 << 5)
#       define R200_TXC_ARG_B_R4_ALPHA		(19 << 5)
#       define R200_TXC_ARG_B_R5_COLOR		(20 << 5)
#       define R200_TXC_ARG_B_R5_ALPHA		(21 << 5)
#       define R200_TXC_ARG_B_TFACTOR1_COLOR	(26 << 5)
#       define R200_TXC_ARG_B_TFACTOR1_ALPHA	(27 << 5)
#       define R200_TXC_ARG_B_MASK		(31 << 5)
#       define R200_TXC_ARG_B_SHIFT		5
#       define R200_TXC_ARG_C_ZERO		(0 << 10)
#       define R200_TXC_ARG_C_CURRENT_COLOR	(2 << 10)
#       define R200_TXC_ARG_C_CURRENT_ALPHA	(3 << 10)
#       define R200_TXC_ARG_C_DIFFUSE_COLOR	(4 << 10)
#       define R200_TXC_ARG_C_DIFFUSE_ALPHA	(5 << 10)
#       define R200_TXC_ARG_C_SPECULAR_COLOR	(6 << 10)
#       define R200_TXC_ARG_C_SPECULAR_ALPHA	(7 << 10)
#       define R200_TXC_ARG_C_TFACTOR_COLOR	(8 << 10)
#       define R200_TXC_ARG_C_TFACTOR_ALPHA	(9 << 10)
#       define R200_TXC_ARG_C_R0_COLOR		(10 << 10)
#       define R200_TXC_ARG_C_R0_ALPHA		(11 << 10)
#       define R200_TXC_ARG_C_R1_COLOR		(12 << 10)
#       define R200_TXC_ARG_C_R1_ALPHA		(13 << 10)
#       define R200_TXC_ARG_C_R2_COLOR		(14 << 10)
#       define R200_TXC_ARG_C_R2_ALPHA		(15 << 10)
#       define R200_TXC_ARG_C_R3_COLOR		(16 << 10)
#       define R200_TXC_ARG_C_R3_ALPHA		(17 << 10)
#       define R200_TXC_ARG_C_R4_COLOR		(18 << 10)
#       define R200_TXC_ARG_C_R4_ALPHA		(19 << 10)
#       define R200_TXC_ARG_C_R5_COLOR		(20 << 10)
#       define R200_TXC_ARG_C_R5_ALPHA		(21 << 10)
#       define R200_TXC_ARG_C_TFACTOR1_COLOR	(26 << 10)
#       define R200_TXC_ARG_C_TFACTOR1_ALPHA	(27 << 10)
#       define R200_TXC_ARG_C_MASK		(31 << 10)
#       define R200_TXC_ARG_C_SHIFT		10
#       define R200_TXC_COMP_ARG_A		(1 << 16)
#       define R200_TXC_COMP_ARG_A_SHIFT	(16)
#       define R200_TXC_BIAS_ARG_A		(1 << 17)
#       define R200_TXC_SCALE_ARG_A		(1 << 18)
#       define R200_TXC_NEG_ARG_A		(1 << 19)
#       define R200_TXC_COMP_ARG_B		(1 << 20)
#       define R200_TXC_COMP_ARG_B_SHIFT	(20)
#       define R200_TXC_BIAS_ARG_B		(1 << 21)
#       define R200_TXC_SCALE_ARG_B		(1 << 22)
#       define R200_TXC_NEG_ARG_B		(1 << 23)
#       define R200_TXC_COMP_ARG_C		(1 << 24)
#       define R200_TXC_COMP_ARG_C_SHIFT	(24)
#       define R200_TXC_BIAS_ARG_C		(1 << 25)
#       define R200_TXC_SCALE_ARG_C		(1 << 26)
#       define R200_TXC_NEG_ARG_C		(1 << 27)
#       define R200_TXC_OP_MADD			(0 << 28)
#       define R200_TXC_OP_CND0			(2 << 28)
#       define R200_TXC_OP_LERP			(3 << 28)
#       define R200_TXC_OP_DOT3			(4 << 28)
#       define R200_TXC_OP_DOT4			(5 << 28)
#       define R200_TXC_OP_CONDITIONAL		(6 << 28)
#       define R200_TXC_OP_DOT2_ADD		(7 << 28)
#       define R200_TXC_OP_MASK			(7 << 28)
#define R200_PP_TXCBLEND2_0             0x2f04
#define R200_PP_TXCBLEND2_1             0x2f14
#       define R200_TXC_TFACTOR_SEL_SHIFT	0
#       define R200_TXC_TFACTOR_SEL_MASK	0x7
#       define R200_TXC_TFACTOR1_SEL_SHIFT	4
#       define R200_TXC_TFACTOR1_SEL_MASK	(0x7 << 4)
#       define R200_TXC_SCALE_SHIFT		8
#       define R200_TXC_SCALE_MASK		(7 << 8)
#       define R200_TXC_SCALE_1X		(0 << 8)
#       define R200_TXC_SCALE_2X		(1 << 8)
#       define R200_TXC_SCALE_4X		(2 << 8)
#       define R200_TXC_SCALE_8X		(3 << 8)
#       define R200_TXC_SCALE_INV2		(5 << 8)
#       define R200_TXC_SCALE_INV4		(6 << 8)
#       define R200_TXC_SCALE_INV8		(7 << 8)
#       define R200_TXC_CLAMP_SHIFT		12
#       define R200_TXC_CLAMP_MASK		(3 << 12)
#       define R200_TXC_CLAMP_WRAP		(0 << 12)
#       define R200_TXC_CLAMP_0_1		(1 << 12)
#       define R200_TXC_CLAMP_8_8		(2 << 12)
#       define R200_TXC_OUTPUT_REG_MASK		(7 << 16)
#       define R200_TXC_OUTPUT_REG_NONE		(0 << 16)
#       define R200_TXC_OUTPUT_REG_R0		(1 << 16)
#       define R200_TXC_OUTPUT_REG_R1		(2 << 16)
#       define R200_TXC_OUTPUT_REG_R2		(3 << 16)
#       define R200_TXC_OUTPUT_REG_R3		(4 << 16)
#       define R200_TXC_OUTPUT_REG_R4		(5 << 16)
#       define R200_TXC_OUTPUT_REG_R5		(6 << 16)
#       define R200_TXC_OUTPUT_MASK_MASK	(7 << 20)
#       define R200_TXC_OUTPUT_MASK_RGB		(0 << 20)
#       define R200_TXC_OUTPUT_MASK_RG		(1 << 20)
#       define R200_TXC_OUTPUT_MASK_RB		(2 << 20)
#       define R200_TXC_OUTPUT_MASK_R		(3 << 20)
#       define R200_TXC_OUTPUT_MASK_GB		(4 << 20)
#       define R200_TXC_OUTPUT_MASK_G		(5 << 20)
#       define R200_TXC_OUTPUT_MASK_B		(6 << 20)
#       define R200_TXC_OUTPUT_MASK_NONE	(7 << 20)
#       define R200_TXC_REPL_NORMAL		0
#       define R200_TXC_REPL_RED		1
#       define R200_TXC_REPL_GREEN		2
#       define R200_TXC_REPL_BLUE		3
#       define R200_TXC_REPL_ARG_A_SHIFT	26
#       define R200_TXC_REPL_ARG_A_MASK		(3 << 26)
#       define R200_TXC_REPL_ARG_B_SHIFT	28
#       define R200_TXC_REPL_ARG_B_MASK		(3 << 28)
#       define R200_TXC_REPL_ARG_C_SHIFT	30
#       define R200_TXC_REPL_ARG_C_MASK		(3 << 30)
#define R200_PP_TXABLEND_0              0x2f08
#define R200_PP_TXABLEND_1              0x2f18
#       define R200_TXA_ARG_A_ZERO		(0)
#       define R200_TXA_ARG_A_CURRENT_ALPHA	(2) /* guess */
#       define R200_TXA_ARG_A_CURRENT_BLUE	(3) /* guess */
#       define R200_TXA_ARG_A_DIFFUSE_ALPHA	(4)
#       define R200_TXA_ARG_A_DIFFUSE_BLUE	(5)
#       define R200_TXA_ARG_A_SPECULAR_ALPHA	(6)
#       define R200_TXA_ARG_A_SPECULAR_BLUE	(7)
#       define R200_TXA_ARG_A_TFACTOR_ALPHA	(8)
#       define R200_TXA_ARG_A_TFACTOR_BLUE	(9)
#       define R200_TXA_ARG_A_R0_ALPHA		(10)
#       define R200_TXA_ARG_A_R0_BLUE		(11)
#       define R200_TXA_ARG_A_R1_ALPHA		(12)
#       define R200_TXA_ARG_A_R1_BLUE		(13)
#       define R200_TXA_ARG_A_R2_ALPHA		(14)
#       define R200_TXA_ARG_A_R2_BLUE		(15)
#       define R200_TXA_ARG_A_R3_ALPHA		(16)
#       define R200_TXA_ARG_A_R3_BLUE		(17)
#       define R200_TXA_ARG_A_R4_ALPHA		(18)
#       define R200_TXA_ARG_A_R4_BLUE		(19)
#       define R200_TXA_ARG_A_R5_ALPHA		(20)
#       define R200_TXA_ARG_A_R5_BLUE		(21)
#       define R200_TXA_ARG_A_TFACTOR1_ALPHA	(26)
#       define R200_TXA_ARG_A_TFACTOR1_BLUE	(27)
#       define R200_TXA_ARG_A_MASK		(31 << 0)
#       define R200_TXA_ARG_A_SHIFT		0
#       define R200_TXA_ARG_B_ZERO		(0 << 5)
#       define R200_TXA_ARG_B_CURRENT_ALPHA	(2 << 5) /* guess */
#       define R200_TXA_ARG_B_CURRENT_BLUE	(3 << 5) /* guess */
#       define R200_TXA_ARG_B_DIFFUSE_ALPHA	(4 << 5)
#       define R200_TXA_ARG_B_DIFFUSE_BLUE	(5 << 5)
#       define R200_TXA_ARG_B_SPECULAR_ALPHA	(6 << 5)
#       define R200_TXA_ARG_B_SPECULAR_BLUE	(7 << 5)
#       define R200_TXA_ARG_B_TFACTOR_ALPHA	(8 << 5)
#       define R200_TXA_ARG_B_TFACTOR_BLUE	(9 << 5)
#       define R200_TXA_ARG_B_R0_ALPHA		(10 << 5)
#       define R200_TXA_ARG_B_R0_BLUE		(11 << 5)
#       define R200_TXA_ARG_B_R1_ALPHA		(12 << 5)
#       define R200_TXA_ARG_B_R1_BLUE		(13 << 5)
#       define R200_TXA_ARG_B_R2_ALPHA		(14 << 5)
#       define R200_TXA_ARG_B_R2_BLUE		(15 << 5)
#       define R200_TXA_ARG_B_R3_ALPHA		(16 << 5)
#       define R200_TXA_ARG_B_R3_BLUE		(17 << 5)
#       define R200_TXA_ARG_B_R4_ALPHA		(18 << 5)
#       define R200_TXA_ARG_B_R4_BLUE		(19 << 5)
#       define R200_TXA_ARG_B_R5_ALPHA		(20 << 5)
#       define R200_TXA_ARG_B_R5_BLUE		(21 << 5)
#       define R200_TXA_ARG_B_TFACTOR1_ALPHA	(26 << 5)
#       define R200_TXA_ARG_B_TFACTOR1_BLUE	(27 << 5)
#       define R200_TXA_ARG_B_MASK		(31 << 5)
#       define R200_TXA_ARG_B_SHIFT			5
#       define R200_TXA_ARG_C_ZERO		(0 << 10)
#       define R200_TXA_ARG_C_CURRENT_ALPHA	(2 << 10) /* guess */
#       define R200_TXA_ARG_C_CURRENT_BLUE	(3 << 10) /* guess */
#       define R200_TXA_ARG_C_DIFFUSE_ALPHA	(4 << 10)
#       define R200_TXA_ARG_C_DIFFUSE_BLUE	(5 << 10)
#       define R200_TXA_ARG_C_SPECULAR_ALPHA	(6 << 10)
#       define R200_TXA_ARG_C_SPECULAR_BLUE	(7 << 10)
#       define R200_TXA_ARG_C_TFACTOR_ALPHA	(8 << 10)
#       define R200_TXA_ARG_C_TFACTOR_BLUE	(9 << 10)
#       define R200_TXA_ARG_C_R0_ALPHA		(10 << 10)
#       define R200_TXA_ARG_C_R0_BLUE		(11 << 10)
#       define R200_TXA_ARG_C_R1_ALPHA		(12 << 10)
#       define R200_TXA_ARG_C_R1_BLUE		(13 << 10)
#       define R200_TXA_ARG_C_R2_ALPHA		(14 << 10)
#       define R200_TXA_ARG_C_R2_BLUE		(15 << 10)
#       define R200_TXA_ARG_C_R3_ALPHA		(16 << 10)
#       define R200_TXA_ARG_C_R3_BLUE		(17 << 10)
#       define R200_TXA_ARG_C_R4_ALPHA		(18 << 10)
#       define R200_TXA_ARG_C_R4_BLUE		(19 << 10)
#       define R200_TXA_ARG_C_R5_ALPHA		(20 << 10)
#       define R200_TXA_ARG_C_R5_BLUE		(21 << 10)
#       define R200_TXA_ARG_C_TFACTOR1_ALPHA	(26 << 10)
#       define R200_TXA_ARG_C_TFACTOR1_BLUE	(27 << 10)
#       define R200_TXA_ARG_C_MASK		(31 << 10)
#       define R200_TXA_ARG_C_SHIFT		10
#       define R200_TXA_COMP_ARG_A		(1 << 16)
#       define R200_TXA_COMP_ARG_A_SHIFT	(16)
#       define R200_TXA_BIAS_ARG_A		(1 << 17)
#       define R200_TXA_SCALE_ARG_A		(1 << 18)
#       define R200_TXA_NEG_ARG_A		(1 << 19)
#       define R200_TXA_COMP_ARG_B		(1 << 20)
#       define R200_TXA_COMP_ARG_B_SHIFT	(20)
#       define R200_TXA_BIAS_ARG_B		(1 << 21)
#       define R200_TXA_SCALE_ARG_B		(1 << 22)
#       define R200_TXA_NEG_ARG_B		(1 << 23)
#       define R200_TXA_COMP_ARG_C		(1 << 24)
#       define R200_TXA_COMP_ARG_C_SHIFT	(24)
#       define R200_TXA_BIAS_ARG_C		(1 << 25)
#       define R200_TXA_SCALE_ARG_C		(1 << 26)
#       define R200_TXA_NEG_ARG_C		(1 << 27)
#       define R200_TXA_OP_MADD			(0 << 28)
#       define R200_TXA_OP_CND0			(2 << 28)
#       define R200_TXA_OP_LERP			(3 << 28)
#       define R200_TXA_OP_CONDITIONAL		(6 << 28)
#       define R200_TXA_OP_MASK			(7 << 28)
#define R200_PP_TXABLEND2_0             0x2f0c
#define R200_PP_TXABLEND2_1             0x2f1c
#       define R200_TXA_TFACTOR_SEL_SHIFT	0
#       define R200_TXA_TFACTOR_SEL_MASK	0x7
#       define R200_TXA_TFACTOR1_SEL_SHIFT	4
#       define R200_TXA_TFACTOR1_SEL_MASK	(0x7 << 4)
#       define R200_TXA_SCALE_SHIFT		8
#       define R200_TXA_SCALE_MASK		(7 << 8)
#       define R200_TXA_SCALE_1X		(0 << 8)
#       define R200_TXA_SCALE_2X		(1 << 8)
#       define R200_TXA_SCALE_4X		(2 << 8)
#       define R200_TXA_SCALE_8X		(3 << 8)
#       define R200_TXA_SCALE_INV2		(5 << 8)
#       define R200_TXA_SCALE_INV4		(6 << 8)
#       define R200_TXA_SCALE_INV8		(7 << 8)
#       define R200_TXA_CLAMP_SHIFT		12
#       define R200_TXA_CLAMP_MASK		(3 << 12)
#       define R200_TXA_CLAMP_WRAP		(0 << 12)
#       define R200_TXA_CLAMP_0_1		(1 << 12)
#       define R200_TXA_CLAMP_8_8		(2 << 12)
#       define R200_TXA_OUTPUT_REG_MASK		(7 << 16)
#       define R200_TXA_OUTPUT_REG_NONE		(0 << 16)
#       define R200_TXA_OUTPUT_REG_R0		(1 << 16)
#       define R200_TXA_OUTPUT_REG_R1		(2 << 16)
#       define R200_TXA_OUTPUT_REG_R2		(3 << 16)
#       define R200_TXA_OUTPUT_REG_R3		(4 << 16)
#       define R200_TXA_OUTPUT_REG_R4		(5 << 16)
#       define R200_TXA_OUTPUT_REG_R5		(6 << 16)
#       define R200_TXA_DOT_ALPHA		(1 << 20)
#       define R200_TXA_REPL_NORMAL		0
#       define R200_TXA_REPL_RED		1
#       define R200_TXA_REPL_GREEN		2
#       define R200_TXA_REPL_ARG_A_SHIFT	26
#       define R200_TXA_REPL_ARG_A_MASK		(3 << 26)
#       define R200_TXA_REPL_ARG_B_SHIFT	28
#       define R200_TXA_REPL_ARG_B_MASK		(3 << 28)
#       define R200_TXA_REPL_ARG_C_SHIFT	30
#       define R200_TXA_REPL_ARG_C_MASK		(3 << 30)
#define R200_RB3D_BLENDCOLOR            0x3218 /* ARGB 8888 */
#define R200_RB3D_ABLENDCNTL            0x321C /* see BLENDCNTL */
#define R200_RB3D_CBLENDCNTL            0x3220 /* see BLENDCNTL */

#define R200_SE_VTX_FMT_0			0x2088
#       define R200_VTX_XY			0 /* always have xy */
#       define R200_VTX_Z0			(1<<0)
#       define R200_VTX_W0			(1<<1)
#       define R200_VTX_WEIGHT_COUNT_SHIFT	(2)
#       define R200_VTX_PV_MATRIX_SEL		(1<<5)
#       define R200_VTX_N0			(1<<6)
#       define R200_VTX_POINT_SIZE		(1<<7)
#       define R200_VTX_DISCRETE_FOG		(1<<8)
#       define R200_VTX_SHININESS_0		(1<<9)
#       define R200_VTX_SHININESS_1		(1<<10)
#       define   R200_VTX_COLOR_NOT_PRESENT	0
#       define   R200_VTX_PK_RGBA		1
#       define   R200_VTX_FP_RGB		2
#       define   R200_VTX_FP_RGBA		3
#       define   R200_VTX_COLOR_MASK		3
#       define R200_VTX_COLOR_0_SHIFT		11
#       define R200_VTX_COLOR_1_SHIFT		13
#       define R200_VTX_COLOR_2_SHIFT		15
#       define R200_VTX_COLOR_3_SHIFT		17
#       define R200_VTX_COLOR_4_SHIFT		19
#       define R200_VTX_COLOR_5_SHIFT		21
#       define R200_VTX_COLOR_6_SHIFT		23
#       define R200_VTX_COLOR_7_SHIFT		25
#       define R200_VTX_XY1			(1<<28)
#       define R200_VTX_Z1			(1<<29)
#       define R200_VTX_W1			(1<<30)
#       define R200_VTX_N1			(1<<31)
#define R200_SE_VTX_FMT_1			0x208c
#       define R200_VTX_TEX0_COMP_CNT_SHIFT	0
#       define R200_VTX_TEX1_COMP_CNT_SHIFT	3
#       define R200_VTX_TEX2_COMP_CNT_SHIFT	6
#       define R200_VTX_TEX3_COMP_CNT_SHIFT	9
#       define R200_VTX_TEX4_COMP_CNT_SHIFT	12
#       define R200_VTX_TEX5_COMP_CNT_SHIFT	15

#define R200_SE_TCL_OUTPUT_VTX_FMT_0		0x2090
#define R200_SE_TCL_OUTPUT_VTX_FMT_1		0x2094
#define R200_SE_TCL_OUTPUT_VTX_COMP_SEL		0x2250
#       define R200_OUTPUT_XYZW			(1<<0)
#       define R200_OUTPUT_COLOR_0		(1<<8)
#       define R200_OUTPUT_COLOR_1		(1<<9)
#       define R200_OUTPUT_TEX_0		(1<<16)
#       define R200_OUTPUT_TEX_1		(1<<17)
#       define R200_OUTPUT_TEX_2		(1<<18)
#       define R200_OUTPUT_TEX_3		(1<<19)
#       define R200_OUTPUT_TEX_4		(1<<20)
#       define R200_OUTPUT_TEX_5		(1<<21)
#       define R200_OUTPUT_TEX_MASK		(0x3f<<16)
#       define R200_OUTPUT_DISCRETE_FOG		(1<<24)
#       define R200_OUTPUT_PT_SIZE		(1<<25)
#       define R200_FORCE_INORDER_PROC		(1<<31)
#define R200_PP_CNTL_X				0x2cc4
#define R200_PP_TXMULTI_CTL_0			0x2c1c
#define R200_SE_VTX_STATE_CNTL			0x2180
#       define R200_UPDATE_USER_COLOR_0_ENA_MASK (1<<16)


               /* R300 3D registers */
#define R300_MC_INIT_MISC_LAT_TIMER	0x180
#	define R300_MC_MISC__MC_CPR_INIT_LAT_SHIFT	0
#	define R300_MC_MISC__MC_VF_INIT_LAT_SHIFT	4
#	define R300_MC_MISC__MC_DISP0R_INIT_LAT_SHIFT	8
#	define R300_MC_MISC__MC_DISP1R_INIT_LAT_SHIFT	12
#	define R300_MC_MISC__MC_FIXED_INIT_LAT_SHIFT	16
#	define R300_MC_MISC__MC_E2R_INIT_LAT_SHIFT	20
#	define R300_MC_MISC__MC_SAME_PAGE_PRIO_SHIFT	24
#	define R300_MC_MISC__MC_GLOBW_INIT_LAT_SHIFT	28


#define R300_MC_INIT_GFX_LAT_TIMER	0x154
#	define R300_MC_MISC__MC_G3D0R_INIT_LAT_SHIFT	0
#	define R300_MC_MISC__MC_G3D1R_INIT_LAT_SHIFT	4
#	define R300_MC_MISC__MC_G3D2R_INIT_LAT_SHIFT	8
#	define R300_MC_MISC__MC_G3D3R_INIT_LAT_SHIFT	12
#	define R300_MC_MISC__MC_TX0R_INIT_LAT_SHIFT	16
#	define R300_MC_MISC__MC_TX1R_INIT_LAT_SHIFT	20
#	define R300_MC_MISC__MC_GLOBR_INIT_LAT_SHIFT	24
#	define R300_MC_MISC__MC_GLOBW_FULL_LAT_SHIFT	28

/*
This file contains registers and constants for the R300. They have been
found mostly by examining command buffers captured using glxtest, as well
as by extrapolating some known registers and constants from the R200.

I am fairly certain that they are correct unless stated otherwise in comments.
*/

#define R300_SE_VPORT_XSCALE                0x1D98
#define R300_SE_VPORT_XOFFSET               0x1D9C
#define R300_SE_VPORT_YSCALE                0x1DA0
#define R300_SE_VPORT_YOFFSET               0x1DA4
#define R300_SE_VPORT_ZSCALE                0x1DA8
#define R300_SE_VPORT_ZOFFSET               0x1DAC

/* BEGIN: Wild guesses */
#define R300_VAP_OUTPUT_VTX_FMT_0           0x2090
#       define R300_VAP_OUTPUT_VTX_FMT_0__POS_PRESENT     (1<<0)
#       define R300_VAP_OUTPUT_VTX_FMT_0__COLOR_PRESENT   (1<<1)
#       define R300_VAP_OUTPUT_VTX_FMT_0__COLOR_1_PRESENT (1<<2) /* GUESS */
#       define R300_VAP_OUTPUT_VTX_FMT_0__COLOR_2_PRESENT (1<<3) /* GUESS */
#       define R300_VAP_OUTPUT_VTX_FMT_0__COLOR_3_PRESENT (1<<4) /* GUESS */
#       define R300_VAP_OUTPUT_VTX_FMT_0__PT_SIZE_PRESENT (1<<16) /* GUESS */

#define R300_VAP_OUTPUT_VTX_FMT_1           0x2094
#       define R300_VAP_OUTPUT_VTX_FMT_1__TEX_0_COMP_CNT_SHIFT 0
#       define R300_VAP_OUTPUT_VTX_FMT_1__TEX_1_COMP_CNT_SHIFT 3
#       define R300_VAP_OUTPUT_VTX_FMT_1__TEX_2_COMP_CNT_SHIFT 6
#       define R300_VAP_OUTPUT_VTX_FMT_1__TEX_3_COMP_CNT_SHIFT 9
#       define R300_VAP_OUTPUT_VTX_FMT_1__TEX_4_COMP_CNT_SHIFT 12
#       define R300_VAP_OUTPUT_VTX_FMT_1__TEX_5_COMP_CNT_SHIFT 15
#       define R300_VAP_OUTPUT_VTX_FMT_1__TEX_6_COMP_CNT_SHIFT 18
#       define R300_VAP_OUTPUT_VTX_FMT_1__TEX_7_COMP_CNT_SHIFT 21
/* END */

#define R300_SE_VTE_CNTL                  0x20b0
#	define     R300_VPORT_X_SCALE_ENA                0x00000001
#	define     R300_VPORT_X_OFFSET_ENA               0x00000002
#	define     R300_VPORT_Y_SCALE_ENA                0x00000004
#	define     R300_VPORT_Y_OFFSET_ENA               0x00000008
#	define     R300_VPORT_Z_SCALE_ENA                0x00000010
#	define     R300_VPORT_Z_OFFSET_ENA               0x00000020
#	define     R300_VTX_XY_FMT                       0x00000100
#	define     R300_VTX_Z_FMT                        0x00000200
#	define     R300_VTX_W0_FMT                       0x00000400
#	define     R300_VTX_W0_NORMALIZE                 0x00000800
#	define     R300_VTX_ST_DENORMALIZED              0x00001000

/* BEGIN: Vertex data assembly - lots of uncertainties */
/* gap */
/* Where do we get our vertex data?
//
// Vertex data either comes either from immediate mode registers or from
// vertex arrays.
// There appears to be no mixed mode (though we can force the pitch of
// vertex arrays to 0, effectively reusing the same element over and over
// again).
//
// Immediate mode is controlled by the INPUT_CNTL registers. I am not sure
// if these registers influence vertex array processing.
//
// Vertex arrays are controlled via the 3D_LOAD_VBPNTR packet3.
//
// In both cases, vertex attributes are then passed through INPUT_ROUTE.

// Beginning with INPUT_ROUTE_0_0 is a list of WORDs that route vertex data
// into the vertex processor's input registers.
// The first word routes the first input, the second word the second, etc.
// The corresponding input is routed into the register with the given index.
// The list is ended by a word with INPUT_ROUTE_END set.
//
// Always set COMPONENTS_4 in immediate mode. */

#define R300_VAP_INPUT_ROUTE_0_0            0x2150
#       define R300_INPUT_ROUTE_COMPONENTS_1     (0 << 0)
#       define R300_INPUT_ROUTE_COMPONENTS_2     (1 << 0)
#       define R300_INPUT_ROUTE_COMPONENTS_3     (2 << 0)
#       define R300_INPUT_ROUTE_COMPONENTS_4     (3 << 0)
#       define R300_INPUT_ROUTE_COMPONENTS_RGBA  (4 << 0) /* GUESS */
#       define R300_VAP_INPUT_ROUTE_IDX_SHIFT    8
#       define R300_VAP_INPUT_ROUTE_IDX_MASK     (31 << 8) /* GUESS */
#       define R300_VAP_INPUT_ROUTE_END          (1 << 13)
#       define R300_INPUT_ROUTE_IMMEDIATE_MODE   (0 << 14) /* GUESS */
#       define R300_INPUT_ROUTE_FLOAT            (1 << 14) /* GUESS */
#       define R300_INPUT_ROUTE_UNSIGNED_BYTE    (2 << 14) /* GUESS */
#       define R300_INPUT_ROUTE_FLOAT_COLOR      (3 << 14) /* GUESS */
#define R300_VAP_INPUT_ROUTE_0_1            0x2154
#define R300_VAP_INPUT_ROUTE_0_2            0x2158
#define R300_VAP_INPUT_ROUTE_0_3            0x215C
#define R300_VAP_INPUT_ROUTE_0_4            0x2160
#define R300_VAP_INPUT_ROUTE_0_5            0x2164
#define R300_VAP_INPUT_ROUTE_0_6            0x2168
#define R300_VAP_INPUT_ROUTE_0_7            0x216C

/* gap */
/* Notes:
//  - always set up to produce at least two attributes:
//    if vertex program uses only position, fglrx will set normal, too
//  - INPUT_CNTL_0_COLOR and INPUT_CNTL_COLOR bits are always equal */
#define R300_VAP_INPUT_CNTL_0               0x2180
#       define R300_INPUT_CNTL_0_COLOR           0x00000001
#define R300_VAP_INPUT_CNTL_1               0x2184
#       define R300_INPUT_CNTL_POS               0x00000001
#       define R300_INPUT_CNTL_NORMAL            0x00000002
#       define R300_INPUT_CNTL_COLOR             0x00000004
#       define R300_INPUT_CNTL_TC0               0x00000400
#       define R300_INPUT_CNTL_TC1               0x00000800
#       define R300_INPUT_CNTL_TC2               0x00001000 /* GUESS */
#       define R300_INPUT_CNTL_TC3               0x00002000 /* GUESS */
#       define R300_INPUT_CNTL_TC4               0x00004000 /* GUESS */
#       define R300_INPUT_CNTL_TC5               0x00008000 /* GUESS */
#       define R300_INPUT_CNTL_TC6               0x00010000 /* GUESS */
#       define R300_INPUT_CNTL_TC7               0x00020000 /* GUESS */

/* gap */
/* Words parallel to INPUT_ROUTE_0; All words that are active in INPUT_ROUTE_0
// are set to a swizzling bit pattern, other words are 0.
//
// In immediate mode, the pattern is always set to xyzw. In vertex array
// mode, the swizzling pattern is e.g. used to set zw components in texture
// coordinates with only tweo components. */
#define R300_VAP_INPUT_ROUTE_1_0            0x21E0
#       define R300_INPUT_ROUTE_SELECT_X    0
#       define R300_INPUT_ROUTE_SELECT_Y    1
#       define R300_INPUT_ROUTE_SELECT_Z    2
#       define R300_INPUT_ROUTE_SELECT_W    3
#       define R300_INPUT_ROUTE_SELECT_ZERO 4
#       define R300_INPUT_ROUTE_SELECT_ONE  5
#       define R300_INPUT_ROUTE_SELECT_MASK 7
#       define R300_INPUT_ROUTE_X_SHIFT          0
#       define R300_INPUT_ROUTE_Y_SHIFT          3
#       define R300_INPUT_ROUTE_Z_SHIFT          6
#       define R300_INPUT_ROUTE_W_SHIFT          9
#       define R300_INPUT_ROUTE_ENABLE           (15 << 12)
#define R300_VAP_INPUT_ROUTE_1_1            0x21E4
#define R300_VAP_INPUT_ROUTE_1_2            0x21E8
#define R300_VAP_INPUT_ROUTE_1_3            0x21EC
#define R300_VAP_INPUT_ROUTE_1_4            0x21F0
#define R300_VAP_INPUT_ROUTE_1_5            0x21F4
#define R300_VAP_INPUT_ROUTE_1_6            0x21F8
#define R300_VAP_INPUT_ROUTE_1_7            0x21FC

/* END */

/* gap */
/* BEGIN: Upload vertex program and data
// The programmable vertex shader unit has a memory bank of unknown size
// that can be written to in 16 byte units by writing the address into
// UPLOAD_ADDRESS, followed by data in UPLOAD_DATA (multiples of 4 DWORDs).
//
// Pointers into the memory bank are always in multiples of 16 bytes.
//
// The memory bank is divided into areas with fixed meaning.
//
// Starting at address UPLOAD_PROGRAM: Vertex program instructions.
// Native limits reported by drivers from ATI suggest size 256 (i.e. 4KB),
// whereas the difference between known addresses suggests size 512.
//
// Starting at address UPLOAD_PARAMETERS: Vertex program parameters.
// Native reported limits and the VPI layout suggest size 256, whereas
// difference between known addresses suggests size 512.
//
// At address UPLOAD_POINTSIZE is a vector (0, 0, ps, 0), where ps is the
// floating point pointsize. The exact purpose of this state is uncertain,
// as there is also the R300_RE_POINTSIZE register.
//
// Multiple vertex programs and parameter sets can be loaded at once,
// which could explain the size discrepancy. */
#define R300_VAP_PVS_UPLOAD_ADDRESS         0x2200
#       define R300_PVS_UPLOAD_PROGRAM           0x00000000
#       define R300_PVS_UPLOAD_PARAMETERS        0x00000200
#       define R300_PVS_UPLOAD_POINTSIZE         0x00000406
/* gap */
#define R300_VAP_PVS_UPLOAD_DATA            0x2208
/* END */

/* gap */
/* I do not know the purpose of this register. However, I do know that
// it is set to 221C_CLEAR for clear operations and to 221C_NORMAL
// for normal rendering. */
#define R300_VAP_UNKNOWN_221C               0x221C
#       define R300_221C_NORMAL                  0x00000000
#       define R300_221C_CLEAR                   0x0001C000

/* gap */
/* Sometimes, END_OF_PKT and 0x2284=0 are the only commands sent between
// rendering commands and overwriting vertex program parameters.
// Therefore, I suspect writing zero to 0x2284 synchronizes the engine and
// avoids bugs caused by still running shaders reading bad data from memory. */
#define R300_VAP_PVS_WAITIDLE               0x2284 /* GUESS */

/* Absolutely no clue what this register is about. */
#define R300_VAP_UNKNOWN_2288               0x2288
#       define R300_2288_R300                    0x00750000 /* -- nh */
#       define R300_2288_RV350                   0x0000FFFF /* -- Vladimir */

/* gap */
/* Addresses are relative to the vertex program instruction area of the
// memory bank. PROGRAM_END points to the last instruction of the active
// program
//
// The meaning of the two UNKNOWN fields is obviously not known. However,
// experiments so far have shown that both *must* point to an instruction
// inside the vertex program, otherwise the GPU locks up.
// fglrx usually sets CNTL_3_UNKNOWN to the end of the program and
// CNTL_1_UNKNOWN points to instruction where last write to position takes place. 
// Most likely this is used to ignore rest of the program in cases where group of verts arent visible.
// For some reason this "section" is sometimes accepted other instruction that have
// no relationship with position calculations. 
*/
#define R300_VAP_PVS_CNTL_1                 0x22D0
#       define R300_PVS_CNTL_1_PROGRAM_START_SHIFT   0
#       define R300_PVS_CNTL_1_POS_END_SHIFT         10
#       define R300_PVS_CNTL_1_PROGRAM_END_SHIFT     20
/* Addresses are relative the the vertex program parameters area. */
#define R300_VAP_PVS_CNTL_2                 0x22D4
#       define R300_PVS_CNTL_2_PARAM_OFFSET_SHIFT 0
#       define R300_PVS_CNTL_2_PARAM_COUNT_SHIFT  16
#define R300_VAP_PVS_CNTL_3	           0x22D8
#       define R300_PVS_CNTL_3_PROGRAM_UNKNOWN_SHIFT 10
#       define R300_PVS_CNTL_3_PROGRAM_UNKNOWN2_SHIFT 0

/* The entire range from 0x2300 to 0x2AC inclusive seems to be used for
// immediate vertices */
#define R300_VAP_VTX_COLOR_R                0x2464
#define R300_VAP_VTX_COLOR_G                0x2468
#define R300_VAP_VTX_COLOR_B                0x246C
#define R300_VAP_VTX_POS_0_X_1              0x2490 /* used for glVertex2*() */
#define R300_VAP_VTX_POS_0_Y_1              0x2494
#define R300_VAP_VTX_COLOR_PKD              0x249C /* RGBA */
#define R300_VAP_VTX_POS_0_X_2              0x24A0 /* used for glVertex3*() */
#define R300_VAP_VTX_POS_0_Y_2              0x24A4
#define R300_VAP_VTX_POS_0_Z_2              0x24A8
#define R300_VAP_VTX_END_OF_PKT             0x24AC /* write 0 to indicate end of packet? */

/* gap */

/* These are values from r300_reg/r300_reg.h - they are known to be correct
   and are here so we can use one register file instead of several
   - Vladimir */
#define R300_GB_VAP_RASTER_VTX_FMT_0	0x4000
#	define R300_GB_VAP_RASTER_VTX_FMT_0__POS_PRESENT	(1<<0)
#	define R300_GB_VAP_RASTER_VTX_FMT_0__COLOR_0_PRESENT	(1<<1)
#	define R300_GB_VAP_RASTER_VTX_FMT_0__COLOR_1_PRESENT	(1<<2)
#	define R300_GB_VAP_RASTER_VTX_FMT_0__COLOR_2_PRESENT	(1<<3)
#	define R300_GB_VAP_RASTER_VTX_FMT_0__COLOR_3_PRESENT	(1<<4)
#	define R300_GB_VAP_RASTER_VTX_FMT_0__COLOR_SPACE	(0xf<<5)
#	define R300_GB_VAP_RASTER_VTX_FMT_0__PT_SIZE_PRESENT	(0x1<<16)

#define R300_GB_VAP_RASTER_VTX_FMT_1	0x4004
	/* each of the following is 3 bits wide, specifies number
	   of components */
#	define R300_GB_VAP_RASTER_VTX_FMT_1__TEX_0_COMP_CNT_SHIFT	0
#	define R300_GB_VAP_RASTER_VTX_FMT_1__TEX_1_COMP_CNT_SHIFT	3
#	define R300_GB_VAP_RASTER_VTX_FMT_1__TEX_2_COMP_CNT_SHIFT	6
#	define R300_GB_VAP_RASTER_VTX_FMT_1__TEX_3_COMP_CNT_SHIFT	9
#	define R300_GB_VAP_RASTER_VTX_FMT_1__TEX_4_COMP_CNT_SHIFT	12
#	define R300_GB_VAP_RASTER_VTX_FMT_1__TEX_5_COMP_CNT_SHIFT	15
#	define R300_GB_VAP_RASTER_VTX_FMT_1__TEX_6_COMP_CNT_SHIFT	18
#	define R300_GB_VAP_RASTER_VTX_FMT_1__TEX_7_COMP_CNT_SHIFT	21

/* UNK30 seems to enables point to quad transformation on textures
   (or something closely related to that).
   This bit is rather fatal at the time being due to lackings at pixel shader side */
#define R300_GB_ENABLE	0x4008
#	define R300_GB_POINT_STUFF_ENABLE	(1<<0)
#	define R300_GB_LINE_STUFF_ENABLE	(1<<1)
#	define R300_GB_TRIANGLE_STUFF_ENABLE	(1<<2)
#	define R300_GB_STENCIL_AUTO_ENABLE	(1<<4)
#	define R300_GB_UNK30			(1<<30)
	/* each of the following is 2 bits wide */
#define R300_GB_TEX_REPLICATE	0
#define R300_GB_TEX_ST		1
#define R300_GB_TEX_STR		2
#	define R300_GB_TEX0_SOURCE_SHIFT	16
#	define R300_GB_TEX1_SOURCE_SHIFT	18
#	define R300_GB_TEX2_SOURCE_SHIFT	20
#	define R300_GB_TEX3_SOURCE_SHIFT	22
#	define R300_GB_TEX4_SOURCE_SHIFT	24
#	define R300_GB_TEX5_SOURCE_SHIFT	26
#	define R300_GB_TEX6_SOURCE_SHIFT	28
#	define R300_GB_TEX7_SOURCE_SHIFT	30

/* MSPOS - positions for multisample antialiasing (?) */
#define R300_GB_MSPOS0	0x4010
	/* shifts - each of the fields is 4 bits */
#	define R300_GB_MSPOS0__MS_X0_SHIFT	0
#	define R300_GB_MSPOS0__MS_Y0_SHIFT	4
#	define R300_GB_MSPOS0__MS_X1_SHIFT	8
#	define R300_GB_MSPOS0__MS_Y1_SHIFT	12
#	define R300_GB_MSPOS0__MS_X2_SHIFT	16
#	define R300_GB_MSPOS0__MS_Y2_SHIFT	20
#	define R300_GB_MSPOS0__MSBD0_Y		24
#	define R300_GB_MSPOS0__MSBD0_X		28

#define R300_GB_MSPOS1	0x4014
#	define R300_GB_MSPOS1__MS_X3_SHIFT	0
#	define R300_GB_MSPOS1__MS_Y3_SHIFT	4
#	define R300_GB_MSPOS1__MS_X4_SHIFT	8
#	define R300_GB_MSPOS1__MS_Y4_SHIFT	12
#	define R300_GB_MSPOS1__MS_X5_SHIFT	16
#	define R300_GB_MSPOS1__MS_Y5_SHIFT	20
#	define R300_GB_MSPOS1__MSBD1		24


#define R300_GB_TILE_CONFIG	0x4018
#	define R300_GB_TILE_ENABLE	(1<<0)
#	define R300_GB_TILE_PIPE_COUNT_RV300	0
#	define R300_GB_TILE_PIPE_COUNT_R300	(3<<1)
#	define R300_GB_TILE_PIPE_COUNT_R420	(7<<1)
#	define R300_GB_TILE_SIZE_8		0
#	define R300_GB_TILE_SIZE_16		(1<<4)
#	define R300_GB_TILE_SIZE_32		(2<<4)
#	define R300_GB_SUPER_SIZE_1		(0<<6)
#	define R300_GB_SUPER_SIZE_2		(1<<6)
#	define R300_GB_SUPER_SIZE_4		(2<<6)
#	define R300_GB_SUPER_SIZE_8		(3<<6)
#	define R300_GB_SUPER_SIZE_16		(4<<6)
#	define R300_GB_SUPER_SIZE_32		(5<<6)
#	define R300_GB_SUPER_SIZE_64		(6<<6)
#	define R300_GB_SUPER_SIZE_128		(7<<6)
#	define R300_GB_SUPER_X_SHIFT		9	/* 3 bits wide */
#	define R300_GB_SUPER_Y_SHIFT		12	/* 3 bits wide */
#	define R300_GB_SUPER_TILE_A		0
#	define R300_GB_SUPER_TILE_B		(1<<15)
#	define R300_GB_SUBPIXEL_1_12		0
#	define R300_GB_SUBPIXEL_1_16		(1<<16)

#define R300_GB_FIFO_SIZE	0x4024
	/* each of the following is 2 bits wide */
#define R300_GB_FIFO_SIZE_32	0
#define R300_GB_FIFO_SIZE_64	1
#define R300_GB_FIFO_SIZE_128	2
#define R300_GB_FIFO_SIZE_256	3
#	define R300_SC_IFIFO_SIZE_SHIFT	0
#	define R300_SC_TZFIFO_SIZE_SHIFT	2
#	define R300_SC_BFIFO_SIZE_SHIFT	4

#	define R300_US_OFIFO_SIZE_SHIFT	12
#	define R300_US_WFIFO_SIZE_SHIFT	14
	/* the following use the same constants as above, but meaning is
	   is times 2 (i.e. instead of 32 words it means 64 */
#	define R300_RS_TFIFO_SIZE_SHIFT	6
#	define R300_RS_CFIFO_SIZE_SHIFT	8
#	define R300_US_RAM_SIZE_SHIFT		10
	/* watermarks, 3 bits wide */
#	define R300_RS_HIGHWATER_COL_SHIFT	16
#	define R300_RS_HIGHWATER_TEX_SHIFT	19
#	define R300_OFIFO_HIGHWATER_SHIFT	22	/* two bits only */
#	define R300_CUBE_FIFO_HIGHWATER_COL_SHIFT	24

#define R300_GB_SELECT	0x401C
#	define R300_GB_FOG_SELECT_C0A		0
#	define R300_GB_FOG_SELECT_C1A		1
#	define R300_GB_FOG_SELECT_C2A		2
#	define R300_GB_FOG_SELECT_C3A		3
#	define R300_GB_FOG_SELECT_1_1_W	4
#	define R300_GB_FOG_SELECT_Z		5
#	define R300_GB_DEPTH_SELECT_Z		0
#	define R300_GB_DEPTH_SELECT_1_1_W	(1<<3)
#	define R300_GB_W_SELECT_1_W		0
#	define R300_GB_W_SELECT_1		(1<<4)

#define R300_GB_AA_CONFIG		0x4020
#	define R300_AA_ENABLE			0x01
#	define R300_AA_SUBSAMPLES_2		0
#	define R300_AA_SUBSAMPLES_3		(1<<1)
#	define R300_AA_SUBSAMPLES_4		(2<<1)
#	define R300_AA_SUBSAMPLES_6		(3<<1)

/* END */

/* gap */
/* Zero to flush caches. */
#define R300_TX_CNTL                        0x4100

/* The upper enable bits are guessed, based on fglrx reported limits. */
#define R300_TX_ENABLE                      0x4104
#       define R300_TX_ENABLE_0                  (1 << 0)
#       define R300_TX_ENABLE_1                  (1 << 1)
#       define R300_TX_ENABLE_2                  (1 << 2)
#       define R300_TX_ENABLE_3                  (1 << 3)
#       define R300_TX_ENABLE_4                  (1 << 4)
#       define R300_TX_ENABLE_5                  (1 << 5)
#       define R300_TX_ENABLE_6                  (1 << 6)
#       define R300_TX_ENABLE_7                  (1 << 7)
#       define R300_TX_ENABLE_8                  (1 << 8)
#       define R300_TX_ENABLE_9                  (1 << 9)
#       define R300_TX_ENABLE_10                 (1 << 10)
#       define R300_TX_ENABLE_11                 (1 << 11)
#       define R300_TX_ENABLE_12                 (1 << 12)
#       define R300_TX_ENABLE_13                 (1 << 13)
#       define R300_TX_ENABLE_14                 (1 << 14)
#       define R300_TX_ENABLE_15                 (1 << 15)

/* The pointsize is given in multiples of 6. The pointsize can be
// enormous: Clear() renders a single point that fills the entire
// framebuffer. */
#define R300_RE_POINTSIZE                   0x421C
#       define R300_POINTSIZE_Y_SHIFT            0
#       define R300_POINTSIZE_Y_MASK             (0xFFFF << 0) /* GUESS */
#       define R300_POINTSIZE_X_SHIFT            16
#       define R300_POINTSIZE_X_MASK             (0xFFFF << 16) /* GUESS */
#       define R300_POINTSIZE_MAX             (R300_POINTSIZE_Y_MASK / 6)

/* The line width is given in multiples of 6.
   In default mode lines are classified as vertical lines.
   HO: horizontal
   VE: vertical or horizontal
   HO & VE: no classification
*/
#define R300_RE_LINE_CNT                      0x4234
#       define R300_LINESIZE_SHIFT            0
#       define R300_LINESIZE_MASK             (0xFFFF << 0) /* GUESS */
#       define R300_LINESIZE_MAX             (R300_LINESIZE_MASK / 6)
#       define R300_LINE_CNT_HO               (1 << 16)
#       define R300_LINE_CNT_VE               (1 << 17)

/* Some sort of scale or clamp value for texcoordless textures. */
#define R300_RE_UNK4238                       0x4238

#define R300_RE_SHADE_MODEL                   0x4278
#	define R300_RE_SHADE_MODEL_SMOOTH     0x3aaaa
#	define R300_RE_SHADE_MODEL_FLAT       0x39595

/* Dangerous */
#define R300_RE_POLYGON_MODE                  0x4288
#	define R300_PM_ENABLED                (1 << 0)
#	define R300_PM_FRONT_POINT            (0 << 0)
#	define R300_PM_BACK_POINT             (0 << 0)
#	define R300_PM_FRONT_LINE             (1 << 4)
#	define R300_PM_FRONT_FILL             (1 << 5)
#	define R300_PM_BACK_LINE              (1 << 7)
#	define R300_PM_BACK_FILL              (1 << 8)

/* Not sure why there are duplicate of factor and constant values. 
   My best guess so far is that there are seperate zbiases for test and write. 
   Ordering might be wrong.
   Some of the tests indicate that fgl has a fallback implementation of zbias
   via pixel shaders. */
#define R300_RE_ZBIAS_T_FACTOR                0x42A4
#define R300_RE_ZBIAS_T_CONSTANT              0x42A8
#define R300_RE_ZBIAS_W_FACTOR                0x42AC
#define R300_RE_ZBIAS_W_CONSTANT              0x42B0

/* This register needs to be set to (1<<1) for RV350 to correctly
   perform depth test (see --vb-triangles in r300_demo)
   Don't know about other chips. - Vladimir
   This is set to 3 when GL_POLYGON_OFFSET_FILL is on.
   My guess is that there are two bits for each zbias primitive (FILL, LINE, POINT).
   One to enable depth test and one for depth write.
   Yet this doesnt explain why depth writes work ...
    */
#define R300_RE_OCCLUSION_CNTL		    0x42B4
#	define R300_OCCLUSION_ON		(1<<1)

#define R300_RE_CULL_CNTL                   0x42B8
#       define R300_CULL_FRONT                   (1 << 0)
#       define R300_CULL_BACK                    (1 << 1)
#       define R300_FRONT_FACE_CCW               (0 << 2)
#       define R300_FRONT_FACE_CW                (1 << 2)


/* BEGIN: Rasterization / Interpolators - many guesses
// 0_UNKNOWN_18 has always been set except for clear operations.
// TC_CNT is the number of incoming texture coordinate sets (i.e. it depends
// on the vertex program, *not* the fragment program) */
#define R300_RS_CNTL_0                      0x4300
#       define R300_RS_CNTL_TC_CNT_SHIFT         2
#       define R300_RS_CNTL_TC_CNT_MASK          (7 << 2)
#       define R300_RS_CNTL_CI_CNT_SHIFT         7 /* number of color interpolators used */
#       define R300_RS_CNTL_0_UNKNOWN_18         (1 << 18)
/* Guess: RS_CNTL_1 holds the index of the highest used RS_ROUTE_n register. */
#define R300_RS_CNTL_1                      0x4304

/* gap */
/* Only used for texture coordinates.
// Use the source field to route texture coordinate input from the vertex program
// to the desired interpolator. Note that the source field is relative to the
// outputs the vertex program *actually* writes. If a vertex program only writes
// texcoord[1], this will be source index 0.
// Set INTERP_USED on all interpolators that produce data used by the
// fragment program. INTERP_USED looks like a swizzling mask, but
// I haven't seen it used that way.
//
// Note: The _UNKNOWN constants are always set in their respective register.
// I don't know if this is necessary. */
#define R300_RS_INTERP_0                    0x4310
#define R300_RS_INTERP_1                    0x4314
#       define R300_RS_INTERP_1_UNKNOWN          0x40
#define R300_RS_INTERP_2                    0x4318
#       define R300_RS_INTERP_2_UNKNOWN          0x80
#define R300_RS_INTERP_3                    0x431C
#       define R300_RS_INTERP_3_UNKNOWN          0xC0
#define R300_RS_INTERP_4                    0x4320
#define R300_RS_INTERP_5                    0x4324
#define R300_RS_INTERP_6                    0x4328
#define R300_RS_INTERP_7                    0x432C
#       define R300_RS_INTERP_SRC_SHIFT          2
#       define R300_RS_INTERP_SRC_MASK           (7 << 2)
#       define R300_RS_INTERP_USED               0x00D10000

/* These DWORDs control how vertex data is routed into fragment program
// registers, after interpolators. */
#define R300_RS_ROUTE_0                     0x4330
#define R300_RS_ROUTE_1                     0x4334
#define R300_RS_ROUTE_2                     0x4338
#define R300_RS_ROUTE_3                     0x433C /* GUESS */
#define R300_RS_ROUTE_4                     0x4340 /* GUESS */
#define R300_RS_ROUTE_5                     0x4344 /* GUESS */
#define R300_RS_ROUTE_6                     0x4348 /* GUESS */
#define R300_RS_ROUTE_7                     0x434C /* GUESS */
#       define R300_RS_ROUTE_SOURCE_INTERP_0     0
#       define R300_RS_ROUTE_SOURCE_INTERP_1     1
#       define R300_RS_ROUTE_SOURCE_INTERP_2     2
#       define R300_RS_ROUTE_SOURCE_INTERP_3     3
#       define R300_RS_ROUTE_SOURCE_INTERP_4     4
#       define R300_RS_ROUTE_SOURCE_INTERP_5     5 /* GUESS */
#       define R300_RS_ROUTE_SOURCE_INTERP_6     6 /* GUESS */
#       define R300_RS_ROUTE_SOURCE_INTERP_7     7 /* GUESS */
#       define R300_RS_ROUTE_ENABLE              (1 << 3) /* GUESS */
#       define R300_RS_ROUTE_DEST_SHIFT          6
#       define R300_RS_ROUTE_DEST_MASK           (31 << 6) /* GUESS */

/* Special handling for color: When the fragment program uses color,
// the ROUTE_0_COLOR bit is set and ROUTE_0_COLOR_DEST contains the
// color register index. */
#       define R300_RS_ROUTE_0_COLOR             (1 << 14)
#       define R300_RS_ROUTE_0_COLOR_DEST_SHIFT  17
#       define R300_RS_ROUTE_0_COLOR_DEST_MASK   (31 << 17) /* GUESS */
/* As above, but for secondary color */
#       define R300_RS_ROUTE_1_COLOR1            (1 << 14)
#       define R300_RS_ROUTE_1_COLOR1_DEST_SHIFT 17
#       define R300_RS_ROUTE_1_COLOR1_DEST_MASK  (31 << 17)
#       define R300_RS_ROUTE_1_UNKNOWN11         (1 << 11)
/* END */

/* BEGIN: Scissors and cliprects
// There are four clipping rectangles. Their corner coordinates are inclusive.
// Every pixel is assigned a number from 0 and 15 by setting bits 0-3 depending
// on whether the pixel is inside cliprects 0-3, respectively. For example,
// if a pixel is inside cliprects 0 and 1, but outside 2 and 3, it is assigned
// the number 3 (binary 0011).
// Iff the bit corresponding to the pixel's number in RE_CLIPRECT_CNTL is set,
// the pixel is rasterized.
//
// In addition to this, there is a scissors rectangle. Only pixels inside the
// scissors rectangle are drawn. (coordinates are inclusive)
//
// For some reason, the top-left corner of the framebuffer is at (1440, 1440)
// for the purpose of clipping and scissors. */
#define R300_RE_CLIPRECT_TL_0               0x43B0
#define R300_RE_CLIPRECT_BR_0               0x43B4
#define R300_RE_CLIPRECT_TL_1               0x43B8
#define R300_RE_CLIPRECT_BR_1               0x43BC
#define R300_RE_CLIPRECT_TL_2               0x43C0
#define R300_RE_CLIPRECT_BR_2               0x43C4
#define R300_RE_CLIPRECT_TL_3               0x43C8
#define R300_RE_CLIPRECT_BR_3               0x43CC
#       define R300_CLIPRECT_OFFSET              1440
#       define R300_CLIPRECT_MASK                0x1FFF
#       define R300_CLIPRECT_X_SHIFT             0
#       define R300_CLIPRECT_X_MASK              (0x1FFF << 0)
#       define R300_CLIPRECT_Y_SHIFT             13
#       define R300_CLIPRECT_Y_MASK              (0x1FFF << 13)
#define R300_RE_CLIPRECT_CNTL               0x43D0
#       define R300_CLIP_OUT                     (1 << 0)
#       define R300_CLIP_0                       (1 << 1)
#       define R300_CLIP_1                       (1 << 2)
#       define R300_CLIP_10                      (1 << 3)
#       define R300_CLIP_2                       (1 << 4)
#       define R300_CLIP_20                      (1 << 5)
#       define R300_CLIP_21                      (1 << 6)
#       define R300_CLIP_210                     (1 << 7)
#       define R300_CLIP_3                       (1 << 8)
#       define R300_CLIP_30                      (1 << 9)
#       define R300_CLIP_31                      (1 << 10)
#       define R300_CLIP_310                     (1 << 11)
#       define R300_CLIP_32                      (1 << 12)
#       define R300_CLIP_320                     (1 << 13)
#       define R300_CLIP_321                     (1 << 14)
#       define R300_CLIP_3210                    (1 << 15)

/* gap */
#define R300_RE_SCISSORS_TL                 0x43E0
#define R300_RE_SCISSORS_BR                 0x43E4
#       define R300_SCISSORS_OFFSET              1440
#       define R300_SCISSORS_X_SHIFT             0
#       define R300_SCISSORS_X_MASK              (0x1FFF << 0)
#       define R300_SCISSORS_Y_SHIFT             13
#       define R300_SCISSORS_Y_MASK              (0x1FFF << 13)
/* END */

/* BEGIN: Texture specification
// The texture specification dwords are grouped by meaning and not by texture unit.
// This means that e.g. the offset for texture image unit N is found in register
// TX_OFFSET_0 + (4*N) */
#define R300_TX_FILTER_0                    0x4400
#define R300_TX_FILTER_1                    0x4404
#       define R300_TX_REPEAT                    0
#       define R300_TX_MIRRORED                  1
#       define R300_TX_CLAMP                     4
#       define R300_TX_CLAMP_TO_EDGE             2
#       define R300_TX_CLAMP_TO_BORDER           6
#       define R300_TX_WRAP_S_SHIFT              0
#       define R300_TX_WRAP_S_MASK               (7 << 0)
#       define R300_TX_WRAP_T_SHIFT              3
#       define R300_TX_WRAP_T_MASK               (7 << 3)
#       define R300_TX_WRAP_Q_SHIFT              6
#       define R300_TX_WRAP_Q_MASK               (7 << 6)
#       define R300_TX_MAG_FILTER_NEAREST        (1 << 9)
#       define R300_TX_MAG_FILTER_LINEAR         (2 << 9)
#       define R300_TX_MAG_FILTER_MASK           (3 << 9)
#       define R300_TX_MIN_FILTER_NEAREST        (1 << 11)
#       define R300_TX_MIN_FILTER_LINEAR         (2 << 11)
#       define R300_TX_MIN_FILTER_NEAREST_MIP_NEAREST       (5  <<  11)
#       define R300_TX_MIN_FILTER_NEAREST_MIP_LINEAR        (9  <<  11)
#       define R300_TX_MIN_FILTER_LINEAR_MIP_NEAREST        (6  <<  11)
#       define R300_TX_MIN_FILTER_LINEAR_MIP_LINEAR         (10 <<  11)
/* NOTE: NEAREST doesnt seem to exist.
   Im not seting MAG_FILTER_MASK and (3 << 11) on for all
   anisotropy modes because that would void selected mag filter */
#       define R300_TX_MIN_FILTER_ANISO_NEAREST             ((0 << 13) /*|R300_TX_MAG_FILTER_MASK|(3<<11)*/)
#       define R300_TX_MIN_FILTER_ANISO_LINEAR              ((0 << 13) /*|R300_TX_MAG_FILTER_MASK|(3<<11)*/)
#       define R300_TX_MIN_FILTER_ANISO_NEAREST_MIP_NEAREST ((1 << 13) /*|R300_TX_MAG_FILTER_MASK|(3<<11)*/)
#       define R300_TX_MIN_FILTER_ANISO_NEAREST_MIP_LINEAR  ((2 << 13) /*|R300_TX_MAG_FILTER_MASK|(3<<11)*/)
#       define R300_TX_MIN_FILTER_MASK           ( (15 << 11) | (3 << 13) )
#       define R300_TX_MAX_ANISO_1_TO_1  (0 << 21)
#       define R300_TX_MAX_ANISO_2_TO_1  (2 << 21)
#       define R300_TX_MAX_ANISO_4_TO_1  (4 << 21)
#       define R300_TX_MAX_ANISO_8_TO_1  (6 << 21)
#       define R300_TX_MAX_ANISO_16_TO_1 (8 << 21)
#       define R300_TX_MAX_ANISO_MASK    (14 << 21)

#define R300_TX_FILTER1_0                      0x4440
#define R300_TX_FILTER1_1                      0x4444
#	define R300_CHROMA_KEY_MODE_DISABLE    0
#	define R300_CHROMA_KEY_FORCE	       1
#	define R300_CHROMA_KEY_BLEND           2
#	define R300_MC_ROUND_NORMAL            (0<<2)
#	define R300_MC_ROUND_MPEG4             (1<<2)
#	define R300_LOD_BIAS_MASK	    0x1fff
#	define R300_EDGE_ANISO_EDGE_DIAG       (0<<13)
#	define R300_EDGE_ANISO_EDGE_ONLY       (1<<13)
#	define R300_MC_COORD_TRUNCATE_DISABLE  (0<<14)
#	define R300_MC_COORD_TRUNCATE_MPEG     (1<<14)
#	define R300_TX_TRI_PERF_0_8            (0<<15)
#	define R300_TX_TRI_PERF_1_8            (1<<15)
#	define R300_TX_TRI_PERF_1_4            (2<<15)
#	define R300_TX_TRI_PERF_3_8            (3<<15)
#	define R300_ANISO_THRESHOLD_MASK       (7<<17)

#define R300_TX_SIZE_0                      0x4480
#define R300_TX_SIZE_1                      0x4484
#       define R300_TX_WIDTH_SHIFT               0
#       define R300_TX_WIDTH_MASK               (2047 << 0)
#       define R300_TX_HEIGHT_SHIFT              11
#       define R300_TX_HEIGHT_MASK               (2047 << 11)
#       define R300_TX_UNK23                     (1 << 23)
#       define R300_TX_SIZE_SHIFT                26 /* largest of width, height */
#       define R300_TX_SIZE_MASK                 (15 << 26)
#       define R300_TX_SIZE_PROJECTED            (1<<30)
#       define R300_TX_SIZE_TXPITCH_EN           (1<<31)

#define R300_TX_FORMAT_0                    0x44C0
#define R300_TX_FORMAT_1                    0x44C4
	/* The interpretation of the format word by Wladimir van der Laan */
	/* The X, Y, Z and W refer to the layout of the components.
	   They are given meanings as R, G, B and Alpha by the swizzle
	   specification */
#	define R300_TX_FORMAT_X8		    0x0
#	define R300_TX_FORMAT_X16		    0x1
#	define R300_TX_FORMAT_Y4X4		    0x2
#	define R300_TX_FORMAT_Y8X8		    0x3
#	define R300_TX_FORMAT_Y16X16		    0x4
#	define R300_TX_FORMAT_Z3Y3X2		    0x5
#	define R300_TX_FORMAT_Z5Y6X5		    0x6
#	define R300_TX_FORMAT_Z6Y5X5		    0x7
#	define R300_TX_FORMAT_Z11Y11X10		    0x8
#	define R300_TX_FORMAT_Z10Y11X11		    0x9
#	define R300_TX_FORMAT_W4Z4Y4X4		    0xA
#	define R300_TX_FORMAT_W1Z5Y5X5		    0xB
#	define R300_TX_FORMAT_W8Z8Y8X8		    0xC
#	define R300_TX_FORMAT_W2Z10Y10X10	    0xD
#	define R300_TX_FORMAT_W16Z16Y16X16	    0xE
#	define R300_TX_FORMAT_DXT1	    	    0xF
#	define R300_TX_FORMAT_DXT3	    	    0x10
#	define R300_TX_FORMAT_DXT5	    	    0x11
#	define R300_TX_FORMAT_D3DMFT_CxV8U8	    0x12     /* no swizzle */
#	define R300_TX_FORMAT_A8R8G8B8	    	    0x13     /* no swizzle */
#	define R300_TX_FORMAT_B8G8_B8G8	    	    0x14     /* no swizzle */
#	define R300_TX_FORMAT_G8R8_G8B8	    	    0x15     /* no swizzle */
						  /* 0x16 - some 16 bit green format.. ?? */
#	define R300_TX_FORMAT_UNK25		   (1 << 25) /* no swizzle */
#	define R300_TX_FORMAT_CUBIC_MAP		   (1 << 26)
	/* gap */
	/* Floating point formats */
	/* Note - hardware supports both 16 and 32 bit floating point */
#	define R300_TX_FORMAT_FL_I16	    	    0x18
#	define R300_TX_FORMAT_FL_I16A16	    	    0x19
#	define R300_TX_FORMAT_FL_R16G16B16A16	    0x1A
#	define R300_TX_FORMAT_FL_I32	    	    0x1B
#	define R300_TX_FORMAT_FL_I32A32	    	    0x1C
#	define R300_TX_FORMAT_FL_R32G32B32A32	    0x1D
	/* alpha modes, convenience mostly */
	/* if you have alpha, pick constant appropriate to the
	   number of channels (1 for I8, 2 for I8A8, 4 for R8G8B8A8, etc */
# 	define R300_TX_FORMAT_ALPHA_1CH		    0x000
# 	define R300_TX_FORMAT_ALPHA_2CH		    0x200
# 	define R300_TX_FORMAT_ALPHA_4CH		    0x600
# 	define R300_TX_FORMAT_ALPHA_NONE	    0xA00
	/* Swizzling */
	/* constants */
#	define R300_TX_FORMAT_X		0
#	define R300_TX_FORMAT_Y		1
#	define R300_TX_FORMAT_Z		2
#	define R300_TX_FORMAT_W		3
#	define R300_TX_FORMAT_ZERO	4
#	define R300_TX_FORMAT_ONE	5
#	define R300_TX_FORMAT_CUT_Z	6		/* 2.0*Z, everything above 1.0 is set to 0.0 */
#	define R300_TX_FORMAT_CUT_W	7		/* 2.0*W, everything above 1.0 is set to 0.0 */
#	define R300_TX_FORMAT_B_SHIFT	18
#	define R300_TX_FORMAT_G_SHIFT	15
#	define R300_TX_FORMAT_R_SHIFT	12
#	define R300_TX_FORMAT_A_SHIFT	9
	/* Convenience macro to take care of layout and swizzling */
#	define R300_EASY_TXFORMAT(B, G, R, A, FMT)	(\
	  ((R300_TX_FORMAT_##B)<<R300_TX_FORMAT_B_SHIFT) \
	| ((R300_TX_FORMAT_##G)<<R300_TX_FORMAT_G_SHIFT) \
	| ((R300_TX_FORMAT_##R)<<R300_TX_FORMAT_R_SHIFT) \
	| ((R300_TX_FORMAT_##A)<<R300_TX_FORMAT_A_SHIFT) \
	| (R300_TX_FORMAT_##FMT) \
	  )
	/* These can be ORed with result of R300_EASY_TX_FORMAT() */
	/* We don't really know what they do. Take values from a constant color ? */
#	define R300_TX_FORMAT_CONST_X		(1<<5)
#	define R300_TX_FORMAT_CONST_Y		(2<<5)
#	define R300_TX_FORMAT_CONST_Z		(4<<5)
#	define R300_TX_FORMAT_CONST_W		(8<<5)
#	define R300_TX_FORMAT_YUV_MODE     0x00800000
     /* Precalculated formats */
#    define R300_TXFORMAT_ARGB8888     R300_EASY_TXFORMAT(X, Y, Z, W, W8Z8Y8X8)
#    define R300_TXFORMAT_RGB565       R300_EASY_TXFORMAT(X, Y, Z, ONE, Z5Y6X5)
#    define R300_TXFORMAT_ARGB4444     R300_EASY_TXFORMAT(X, Y, Z, W, W4Z4Y4X4)
#    define R300_TXFORMAT_ARGB1555     R300_EASY_TXFORMAT(X, Y, Z, W, W1Z5Y5X5)
#    define R300_TXFORMAT_RGB332       R300_EASY_TXFORMAT(X, Y, Z, ONE, Z3Y3X2)
#    define R300_TXFORMAT_A8           R300_EASY_TXFORMAT(ZERO, ZERO, ZERO, X, X8)
#    define R300_TXFORMAT_I8           R300_EASY_TXFORMAT(X, X, X, X, X8)
#    define R300_TXFORMAT_VYUY422      R300_EASY_TXFORMAT(X, Y, Z, ONE, G8R8_G8B8)|R300_TX_FORMAT_YUV_MODE
#    define R300_TXFORMAT_YVYU422      R300_EASY_TXFORMAT(X, Y, Z, ONE, B8G8_B8G8)|R300_TX_FORMAT_YUV_MODE


#define R300_TX_PITCH_0			    0x4500 /* obvious missing in gap */
#define R300_TX_PITCH_1                0x4504

#define R300_TX_OFFSET_0               0x4540
#define R300_TX_OFFSET_1               0x4544
/* BEGIN: Guess from R200 */
#       define R300_TXO_ENDIAN_NO_SWAP           (0 << 0)
#       define R300_TXO_ENDIAN_BYTE_SWAP         (1 << 0)
#       define R300_TXO_ENDIAN_WORD_SWAP         (2 << 0)
#       define R300_TXO_ENDIAN_HALFDW_SWAP       (3 << 0)
#       define R300_TXO_MACRO_TILE               (1 << 2)
#       define R300_TXO_MICRO_TILE               (1 << 3)
#       define R300_TXO_OFFSET_MASK              0xffffffe0
/* END */

#define R300_TX_CHROMA_KEY_0                0x4580 /* 32 bit chroma key */

#define R300_TX_BORDER_COLOR_0              0x45C0 //ff00ff00 == { 0, 1.0, 0, 1.0 }

/* END */

/* BEGIN: Fragment program instruction set
// Fragment programs are written directly into register space.
// There are separate instruction streams for texture instructions and ALU
// instructions.
// In order to synchronize these streams, the program is divided into up
// to 4 nodes. Each node begins with a number of TEX operations, followed
// by a number of ALU operations.
// The first node can have zero TEX ops, all subsequent nodes must have at least
// one TEX ops.
// All nodes must have at least one ALU op.
//
// The index of the last node is stored in PFS_CNTL_0: A value of 0 means
// 1 node, a value of 3 means 4 nodes.
// The total amount of instructions is defined in PFS_CNTL_2. The offsets are
// offsets into the respective instruction streams, while *_END points to the
// last instruction relative to this offset. */
#define R300_PFS_CNTL_0                     0x4600
#       define R300_PFS_CNTL_LAST_NODES_SHIFT    0
#       define R300_PFS_CNTL_LAST_NODES_MASK     (3 << 0)
#       define R300_PFS_CNTL_FIRST_NODE_HAS_TEX  (1 << 3)
#define R300_PFS_CNTL_1                     0x4604
/* There is an unshifted value here which has so far always been equal to the
// index of the highest used temporary register. */
#define R300_PFS_CNTL_2                     0x4608
#       define R300_PFS_CNTL_ALU_OFFSET_SHIFT    0
#       define R300_PFS_CNTL_ALU_OFFSET_MASK     (63 << 0)
#       define R300_PFS_CNTL_ALU_END_SHIFT       6
#       define R300_PFS_CNTL_ALU_END_MASK        (63 << 0)
#       define R300_PFS_CNTL_TEX_OFFSET_SHIFT    12
#       define R300_PFS_CNTL_TEX_OFFSET_MASK     (31 << 12) /* GUESS */
#       define R300_PFS_CNTL_TEX_END_SHIFT       18
#       define R300_PFS_CNTL_TEX_END_MASK        (31 << 18) /* GUESS */

/* gap */
/* Nodes are stored backwards. The last active node is always stored in
// PFS_NODE_3.
// Example: In a 2-node program, NODE_0 and NODE_1 are set to 0. The
// first node is stored in NODE_2, the second node is stored in NODE_3.
//
// Offsets are relative to the master offset from PFS_CNTL_2.
// LAST_NODE is set for the last node, and only for the last node. */
#define R300_PFS_NODE_0                     0x4610
#define R300_PFS_NODE_1                     0x4614
#define R300_PFS_NODE_2                     0x4618
#define R300_PFS_NODE_3                     0x461C
#       define R300_PFS_NODE_ALU_OFFSET_SHIFT    0
#       define R300_PFS_NODE_ALU_OFFSET_MASK     (63 << 0)
#       define R300_PFS_NODE_ALU_END_SHIFT       6
#       define R300_PFS_NODE_ALU_END_MASK        (63 << 6)
#       define R300_PFS_NODE_TEX_OFFSET_SHIFT    12
#       define R300_PFS_NODE_TEX_OFFSET_MASK     (31 << 12)
#       define R300_PFS_NODE_TEX_END_SHIFT       17
#       define R300_PFS_NODE_TEX_END_MASK        (31 << 17)
/*#       define R300_PFS_NODE_LAST_NODE           (1 << 22) */
#		define R300_PFS_NODE_OUTPUT_COLOR        (1 << 22)
#		define R300_PFS_NODE_OUTPUT_DEPTH        (1 << 23)

/* TEX
// As far as I can tell, texture instructions cannot write into output
// registers directly. A subsequent ALU instruction is always necessary,
// even if it's just MAD o0, r0, 1, 0 */
#define R300_PFS_TEXI_0                     0x4620
#       define R300_FPITX_SRC_SHIFT              0
#       define R300_FPITX_SRC_MASK               (31 << 0)
#       define R300_FPITX_SRC_CONST              (1 << 5) /* GUESS */
#       define R300_FPITX_DST_SHIFT              6
#       define R300_FPITX_DST_MASK               (31 << 6)
#       define R300_FPITX_IMAGE_SHIFT            11
#       define R300_FPITX_IMAGE_MASK             (15 << 11) /* GUESS based on layout and native limits */
/* Unsure if these are opcodes, or some kind of bitfield, but this is how
 * they were set when I checked
 */
#       define R300_FPITX_OPCODE_SHIFT           15
#       define R300_FPITX_OP_TEX                 (1 << 15)
#       define R300_FPITX_OP_KIL                 (2 << 15)
#       define R300_FPITX_OP_TXP                 (3 << 15)
#       define R300_FPITX_OP_TXB                 (4 << 15)

/* ALU
// The ALU instructions register blocks are enumerated according to the order
// in which fglrx. I assume there is space for 64 instructions, since
// each block has space for a maximum of 64 DWORDs, and this matches reported
// native limits.
//
// The basic functional block seems to be one MAD for each color and alpha,
// and an adder that adds all components after the MUL.
//  - ADD, MUL, MAD etc.: use MAD with appropriate neutral operands
//  - DP4: Use OUTC_DP4, OUTA_DP4
//  - DP3: Use OUTC_DP3, OUTA_DP4, appropriate alpha operands
//  - DPH: Use OUTC_DP4, OUTA_DP4, appropriate alpha operands
//  - CMP: If ARG2 < 0, return ARG1, else return ARG0
//  - FLR: use FRC+MAD
//  - XPD: use MAD+MAD
//  - SGE, SLT: use MAD+CMP
//  - RSQ: use ABS modifier for argument
//  - Use OUTC_REPL_ALPHA to write results of an alpha-only operation (e.g. RCP)
//    into color register
//  - apparently, there's no quick DST operation
//  - fglrx set FPI2_UNKNOWN_31 on a "MAD fragment.color, tmp0, tmp1, tmp2"
//  - fglrx set FPI2_UNKNOWN_31 on a "MAX r2, r1, c0"
//  - fglrx once set FPI0_UNKNOWN_31 on a "FRC r1, r1"
//
// Operand selection
// First stage selects three sources from the available registers and
// constant parameters. This is defined in INSTR1 (color) and INSTR3 (alpha).
// fglrx sorts the three source fields: Registers before constants,
// lower indices before higher indices; I do not know whether this is necessary.
// fglrx fills unused sources with "read constant 0"
// According to specs, you cannot select more than two different constants.
//
// Second stage selects the operands from the sources. This is defined in
// INSTR0 (color) and INSTR2 (alpha). You can also select the special constants
// zero and one.
// Swizzling and negation happens in this stage, as well.
//
// Important: Color and alpha seem to be mostly separate, i.e. their sources
// selection appears to be fully independent (the register storage is probably
// physically split into a color and an alpha section).
// However (because of the apparent physical split), there is some interaction
// WRT swizzling. If, for example, you want to load an R component into an
// Alpha operand, this R component is taken from a *color* source, not from
// an alpha source. The corresponding register doesn't even have to appear in
// the alpha sources list. (I hope this alll makes sense to you)
//
// Destination selection
// The destination register index is in FPI1 (color) and FPI3 (alpha) together
// with enable bits.
// There are separate enable bits for writing into temporary registers
// (DSTC_REG_* /DSTA_REG) and and program output registers (DSTC_OUTPUT_* /DSTA_OUTPUT).
// You can write to both at once, or not write at all (the same index
// must be used for both).
//
// Note: There is a special form for LRP
//  - Argument order is the same as in ARB_fragment_program.
//  - Operation is MAD
//  - ARG1 is set to ARGC_SRC1C_LRP/ARGC_SRC1A_LRP
//  - Set FPI0/FPI2_SPECIAL_LRP
// Arbitrary LRP (including support for swizzling) requires vanilla MAD+MAD */
#define R300_PFS_INSTR1_0                   0x46C0
#       define R300_FPI1_SRC0C_SHIFT             0
#       define R300_FPI1_SRC0C_MASK              (31 << 0)
#       define R300_FPI1_SRC0C_CONST             (1 << 5)
#       define R300_FPI1_SRC1C_SHIFT             6
#       define R300_FPI1_SRC1C_MASK              (31 << 6)
#       define R300_FPI1_SRC1C_CONST             (1 << 11)
#       define R300_FPI1_SRC2C_SHIFT             12
#       define R300_FPI1_SRC2C_MASK              (31 << 12)
#       define R300_FPI1_SRC2C_CONST             (1 << 17)
#       define R300_FPI1_DSTC_SHIFT              18
#       define R300_FPI1_DSTC_MASK               (31 << 18)
#       define R300_FPI1_DSTC_REG_MASK_SHIFT     23
#       define R300_FPI1_DSTC_REG_X              (1 << 23)
#       define R300_FPI1_DSTC_REG_Y              (1 << 24)
#       define R300_FPI1_DSTC_REG_Z              (1 << 25)
#       define R300_FPI1_DSTC_OUTPUT_MASK_SHIFT  26
#       define R300_FPI1_DSTC_OUTPUT_X           (1 << 26)
#       define R300_FPI1_DSTC_OUTPUT_Y           (1 << 27)
#       define R300_FPI1_DSTC_OUTPUT_Z           (1 << 28)

#define R300_PFS_INSTR3_0                   0x47C0
#       define R300_FPI3_SRC0A_SHIFT             0
#       define R300_FPI3_SRC0A_MASK              (31 << 0)
#       define R300_FPI3_SRC0A_CONST             (1 << 5)
#       define R300_FPI3_SRC1A_SHIFT             6
#       define R300_FPI3_SRC1A_MASK              (31 << 6)
#       define R300_FPI3_SRC1A_CONST             (1 << 11)
#       define R300_FPI3_SRC2A_SHIFT             12
#       define R300_FPI3_SRC2A_MASK              (31 << 12)
#       define R300_FPI3_SRC2A_CONST             (1 << 17)
#       define R300_FPI3_DSTA_SHIFT              18
#       define R300_FPI3_DSTA_MASK               (31 << 18)
#       define R300_FPI3_DSTA_REG                (1 << 23)
#       define R300_FPI3_DSTA_OUTPUT             (1 << 24)
#       define R300_FPI3_DSTA_DEPTH              (1 << 27)

#define R300_PFS_INSTR0_0                   0x48C0
#       define R300_FPI0_ARGC_SRC0C_XYZ          0
#       define R300_FPI0_ARGC_SRC0C_XXX          1
#       define R300_FPI0_ARGC_SRC0C_YYY          2
#       define R300_FPI0_ARGC_SRC0C_ZZZ          3
#       define R300_FPI0_ARGC_SRC1C_XYZ          4
#       define R300_FPI0_ARGC_SRC1C_XXX          5
#       define R300_FPI0_ARGC_SRC1C_YYY          6
#       define R300_FPI0_ARGC_SRC1C_ZZZ          7
#       define R300_FPI0_ARGC_SRC2C_XYZ          8
#       define R300_FPI0_ARGC_SRC2C_XXX          9
#       define R300_FPI0_ARGC_SRC2C_YYY          10
#       define R300_FPI0_ARGC_SRC2C_ZZZ          11
#       define R300_FPI0_ARGC_SRC0A              12
#       define R300_FPI0_ARGC_SRC1A              13
#       define R300_FPI0_ARGC_SRC2A              14
#       define R300_FPI0_ARGC_SRC1C_LRP          15
#       define R300_FPI0_ARGC_ZERO               20
#       define R300_FPI0_ARGC_ONE                21
#       define R300_FPI0_ARGC_HALF               22 /* GUESS */
#       define R300_FPI0_ARGC_SRC0C_YZX          23
#       define R300_FPI0_ARGC_SRC1C_YZX          24
#       define R300_FPI0_ARGC_SRC2C_YZX          25
#       define R300_FPI0_ARGC_SRC0C_ZXY          26
#       define R300_FPI0_ARGC_SRC1C_ZXY          27
#       define R300_FPI0_ARGC_SRC2C_ZXY          28
#       define R300_FPI0_ARGC_SRC0CA_WZY         29
#       define R300_FPI0_ARGC_SRC1CA_WZY         30
#       define R300_FPI0_ARGC_SRC2CA_WZY         31

#       define R300_FPI0_ARG0C_SHIFT             0
#       define R300_FPI0_ARG0C_MASK              (31 << 0)
#       define R300_FPI0_ARG0C_NEG               (1 << 5)
#       define R300_FPI0_ARG0C_ABS               (1 << 6)
#       define R300_FPI0_ARG1C_SHIFT             7
#       define R300_FPI0_ARG1C_MASK              (31 << 7)
#       define R300_FPI0_ARG1C_NEG               (1 << 12)
#       define R300_FPI0_ARG1C_ABS               (1 << 13)
#       define R300_FPI0_ARG2C_SHIFT             14
#       define R300_FPI0_ARG2C_MASK              (31 << 14)
#       define R300_FPI0_ARG2C_NEG               (1 << 19)
#       define R300_FPI0_ARG2C_ABS               (1 << 20)
#       define R300_FPI0_SPECIAL_LRP             (1 << 21)
#       define R300_FPI0_OUTC_MAD                (0 << 23)
#       define R300_FPI0_OUTC_DP3                (1 << 23)
#       define R300_FPI0_OUTC_DP4                (2 << 23)
#       define R300_FPI0_OUTC_MIN                (4 << 23)
#       define R300_FPI0_OUTC_MAX                (5 << 23)
#       define R300_FPI0_OUTC_CMP                (8 << 23)
#       define R300_FPI0_OUTC_FRC                (9 << 23)
#       define R300_FPI0_OUTC_REPL_ALPHA         (10 << 23)
#       define R300_FPI0_OUTC_SAT                (1 << 30)
#       define R300_FPI0_INSERT_NOP              (1 << 31)

#define R300_PFS_INSTR2_0                   0x49C0
#       define R300_FPI2_ARGA_SRC0C_X            0
#       define R300_FPI2_ARGA_SRC0C_Y            1
#       define R300_FPI2_ARGA_SRC0C_Z            2
#       define R300_FPI2_ARGA_SRC1C_X            3
#       define R300_FPI2_ARGA_SRC1C_Y            4
#       define R300_FPI2_ARGA_SRC1C_Z            5
#       define R300_FPI2_ARGA_SRC2C_X            6
#       define R300_FPI2_ARGA_SRC2C_Y            7
#       define R300_FPI2_ARGA_SRC2C_Z            8
#       define R300_FPI2_ARGA_SRC0A              9
#       define R300_FPI2_ARGA_SRC1A              10
#       define R300_FPI2_ARGA_SRC2A              11
#       define R300_FPI2_ARGA_SRC1A_LRP          15
#       define R300_FPI2_ARGA_ZERO               16
#       define R300_FPI2_ARGA_ONE                17
#       define R300_FPI2_ARGA_HALF               18 /* GUESS */

#       define R300_FPI2_ARG0A_SHIFT             0
#       define R300_FPI2_ARG0A_MASK              (31 << 0)
#       define R300_FPI2_ARG0A_NEG               (1 << 5)
#       define R300_FPI2_ARG0A_ABS               (1 << 6) /* GUESS */
#       define R300_FPI2_ARG1A_SHIFT             7
#       define R300_FPI2_ARG1A_MASK              (31 << 7)
#       define R300_FPI2_ARG1A_NEG               (1 << 12)
#       define R300_FPI2_ARG1A_ABS	              (1 << 13) /* GUESS */
#       define R300_FPI2_ARG2A_SHIFT             14
#       define R300_FPI2_ARG2A_MASK              (31 << 14)
#       define R300_FPI2_ARG2A_NEG               (1 << 19)
#       define R300_FPI2_ARG2A_ABS               (1 << 20) /* GUESS */
#       define R300_FPI2_SPECIAL_LRP             (1 << 21)
#       define R300_FPI2_OUTA_MAD                (0 << 23)
#       define R300_FPI2_OUTA_DP4                (1 << 23)
#       define R300_FPI2_OUTA_MIN                (2 << 23)
#       define R300_FPI2_OUTA_MAX                (3 << 23)
#       define R300_FPI2_OUTA_CMP                (6 << 23)
#       define R300_FPI2_OUTA_FRC                (7 << 23)
#       define R300_FPI2_OUTA_EX2                (8 << 23)
#       define R300_FPI2_OUTA_LG2                (9 << 23)
#       define R300_FPI2_OUTA_RCP                (10 << 23)
#       define R300_FPI2_OUTA_RSQ                (11 << 23)
#       define R300_FPI2_OUTA_SAT                (1 << 30)
#       define R300_FPI2_UNKNOWN_31              (1 << 31)
/* END */

/* gap */
#define R300_PP_ALPHA_TEST                  0x4BD4
#       define R300_REF_ALPHA_MASK               0x000000ff
#       define R300_ALPHA_TEST_FAIL              (0 << 8)
#       define R300_ALPHA_TEST_LESS              (1 << 8)
#       define R300_ALPHA_TEST_LEQUAL            (3 << 8)
#       define R300_ALPHA_TEST_EQUAL             (2 << 8)
#       define R300_ALPHA_TEST_GEQUAL            (6 << 8)
#       define R300_ALPHA_TEST_GREATER           (4 << 8)
#       define R300_ALPHA_TEST_NEQUAL            (5 << 8)
#       define R300_ALPHA_TEST_PASS              (7 << 8)
#       define R300_ALPHA_TEST_OP_MASK           (7 << 8)
#       define R300_ALPHA_TEST_ENABLE            (1 << 11)

/* gap */
/* Fragment program parameters in 7.16 floating point */
#define R300_PFS_PARAM_0_X                  0x4C00
#define R300_PFS_PARAM_0_Y                  0x4C04
#define R300_PFS_PARAM_0_Z                  0x4C08
#define R300_PFS_PARAM_0_W                  0x4C0C
/* GUESS: PARAM_31 is last, based on native limits reported by fglrx */
#define R300_PFS_PARAM_31_X                 0x4DF0
#define R300_PFS_PARAM_31_Y                 0x4DF4
#define R300_PFS_PARAM_31_Z                 0x4DF8
#define R300_PFS_PARAM_31_W                 0x4DFC

/* Notes:
// - AFAIK fglrx always sets BLEND_UNKNOWN when blending is used in the application
// - AFAIK fglrx always sets BLEND_NO_SEPARATE when CBLEND and ABLEND are set to the same
//   function (both registers are always set up completely in any case)
// - Most blend flags are simply copied from R200 and not tested yet */
#define R300_RB3D_CBLEND                    0x4E04
#define R300_RB3D_ABLEND                    0x4E08
 /* the following only appear in CBLEND */
#       define R300_BLEND_ENABLE                     (1 << 0)
#       define R300_BLEND_UNKNOWN                    (3 << 1)
#       define R300_BLEND_NO_SEPARATE                (1 << 3)
 /* the following are shared between CBLEND and ABLEND */
#       define R300_FCN_MASK                         (3  << 12)
#       define R300_COMB_FCN_ADD_CLAMP               (0  << 12)
#       define R300_COMB_FCN_ADD_NOCLAMP             (1  << 12)
#       define R300_COMB_FCN_SUB_CLAMP               (2  << 12)
#       define R300_COMB_FCN_SUB_NOCLAMP             (3  << 12)
#       define R300_SRC_BLEND_GL_ZERO                (32 << 16)
#       define R300_SRC_BLEND_GL_ONE                 (33 << 16)
#       define R300_SRC_BLEND_GL_SRC_COLOR           (34 << 16)
#       define R300_SRC_BLEND_GL_ONE_MINUS_SRC_COLOR (35 << 16)
#       define R300_SRC_BLEND_GL_DST_COLOR           (36 << 16)
#       define R300_SRC_BLEND_GL_ONE_MINUS_DST_COLOR (37 << 16)
#       define R300_SRC_BLEND_GL_SRC_ALPHA           (38 << 16)
#       define R300_SRC_BLEND_GL_ONE_MINUS_SRC_ALPHA (39 << 16)
#       define R300_SRC_BLEND_GL_DST_ALPHA           (40 << 16)
#       define R300_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA (41 << 16)
#       define R300_SRC_BLEND_GL_SRC_ALPHA_SATURATE  (42 << 16)
#       define R300_SRC_BLEND_MASK                   (63 << 16)
#       define R300_DST_BLEND_GL_ZERO                (32 << 24)
#       define R300_DST_BLEND_GL_ONE                 (33 << 24)
#       define R300_DST_BLEND_GL_SRC_COLOR           (34 << 24)
#       define R300_DST_BLEND_GL_ONE_MINUS_SRC_COLOR (35 << 24)
#       define R300_DST_BLEND_GL_DST_COLOR           (36 << 24)
#       define R300_DST_BLEND_GL_ONE_MINUS_DST_COLOR (37 << 24)
#       define R300_DST_BLEND_GL_SRC_ALPHA           (38 << 24)
#       define R300_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA (39 << 24)
#       define R300_DST_BLEND_GL_DST_ALPHA           (40 << 24)
#       define R300_DST_BLEND_GL_ONE_MINUS_DST_ALPHA (41 << 24)
#       define R300_DST_BLEND_MASK                   (63 << 24)
#define R300_RB3D_COLORMASK                 0x4E0C
#       define R300_COLORMASK0_B                 (1<<0)
#       define R300_COLORMASK0_G                 (1<<1)
#       define R300_COLORMASK0_R                 (1<<2)
#       define R300_COLORMASK0_A                 (1<<3)

#define R300_RB3D_BLENDCOLOR                0x4E10 /* ARGB */

/* gap */
#define R300_RB3D_COLOROFFSET0              0x4E28
#       define R300_COLOROFFSET_MASK             0xFFFFFFF0 /* GUESS */
#define R300_RB3D_COLOROFFSET1              0x4E2C /* GUESS */
#define R300_RB3D_COLOROFFSET2              0x4E30 /* GUESS */
#define R300_RB3D_COLOROFFSET3              0x4E34 /* GUESS */
/* gap */
/* Bit 16: Larger tiles
// Bit 17: 4x2 tiles
// Bit 18: Extremely weird tile like, but some pixels duplicated? */
#define R300_RB3D_COLORPITCH0               0x4E38
#       define R300_COLORPITCH_MASK              0x00001FF8 /* GUESS */
#       define R300_COLOR_TILE_ENABLE            (1 << 16) /* GUESS */
#       define R300_COLOR_MICROTILE_ENABLE       (1 << 17) /* GUESS */
#       define R300_COLOR_ENDIAN_NO_SWAP         (0 << 18) /* GUESS */
#       define R300_COLOR_ENDIAN_WORD_SWAP       (1 << 18) /* GUESS */
#       define R300_COLOR_ENDIAN_DWORD_SWAP      (2 << 18) /* GUESS */
#       define R300_COLOR_FORMAT_RGB565          (2 << 22)
#       define R300_COLOR_FORMAT_ARGB8888        (3 << 22)
#       define R300_COLOR_FORMAT_RGB8            (4 << 22)
#       define R300_COLOR_FORMAT_MASK            (7 << 22)
#define R300_RB3D_COLORPITCH1               0x4E3C /* GUESS */
#define R300_RB3D_COLORPITCH2               0x4E40 /* GUESS */
#define R300_RB3D_COLORPITCH3               0x4E44 /* GUESS */

#define R300_RB3D_DSTCACHE_MODE             0x4E48
/* gap */
/* Guess by Vladimir.
// Set to 0A before 3D operations, set to 02 afterwards. */
#define R300_RB3D_DSTCACHE_CTLSTAT          0x4E4C
#       define R300_RB3D_DSTCACHE_02             0x00000002
#       define R300_RB3D_DSTCACHE_0A             0x0000000A

/* gap */
/* There seems to be no "write only" setting, so use Z-test = ALWAYS for this. */
/* Bit (1<<8) is the "test" bit. so plain write is 6  - vd */
#define R300_RB3D_ZSTENCIL_CNTL_0                   0x4F00
#       define R300_RB3D_Z_DISABLED_1            0x00000010 /* GUESS */
#       define R300_RB3D_Z_DISABLED_2            0x00000014 /* GUESS */
#       define R300_RB3D_Z_TEST                  0x00000012
#       define R300_RB3D_Z_TEST_AND_WRITE        0x00000016
#       define R300_RB3D_Z_WRITE_ONLY            0x00000006
#       define R300_RB3D_STENCIL_ENABLE	         0x00000001

#define R300_RB3D_ZSTENCIL_CNTL_1                   0x4F04
		/* functions */
#	define R300_ZS_NEVER			0
#	define R300_ZS_LESS	               1
#	define R300_ZS_LEQUAL			2
#	define R300_ZS_EQUAL			3
#	define R300_ZS_GEQUAL			4
#	define R300_ZS_GREATER			5
#	define R300_ZS_NOTEQUAL			6
#	define R300_ZS_ALWAYS			7
#    define R300_ZS_MASK                7
		/* operations */
#	define R300_ZS_KEEP			0
#	define R300_ZS_ZERO			1
#	define R300_ZS_REPLACE			2
#	define R300_ZS_INCR			3
#	define R300_ZS_DECR			4
#	define R300_ZS_INVERT			5
#	define R300_ZS_INCR_WRAP		6
#	define R300_ZS_DECR_WRAP		7

       /* front and back refer to operations done for front
          and back faces, i.e. separate stencil function support */
#	define R300_RB3D_ZS1_DEPTH_FUNC_SHIFT		0
#	define R300_RB3D_ZS1_FRONT_FUNC_SHIFT		3
#	define R300_RB3D_ZS1_FRONT_FAIL_OP_SHIFT	6
#	define R300_RB3D_ZS1_FRONT_ZPASS_OP_SHIFT	9
#	define R300_RB3D_ZS1_FRONT_ZFAIL_OP_SHIFT      12
#	define R300_RB3D_ZS1_BACK_FUNC_SHIFT           15
#	define R300_RB3D_ZS1_BACK_FAIL_OP_SHIFT        18
#	define R300_RB3D_ZS1_BACK_ZPASS_OP_SHIFT       21
#	define R300_RB3D_ZS1_BACK_ZFAIL_OP_SHIFT       24



#define R300_RB3D_ZSTENCIL_CNTL_2                   0x4F08
#	define R300_RB3D_ZS2_STENCIL_REF_SHIFT		0
#	define R300_RB3D_ZS2_STENCIL_MASK		0xFF
#	define R300_RB3D_ZS2_STENCIL_MASK_SHIFT	        8
#	define R300_RB3D_ZS2_STENCIL_WRITE_MASK_SHIFT	16

/* gap */

#define R300_RB3D_ZSTENCIL_FORMAT                   0x4F10
#	define R300_DEPTH_FORMAT_16BIT_INT_Z     (0 << 0)
#	define R300_DEPTH_FORMAT_24BIT_INT_Z     (2 << 0)
 /* 16 bit format or some aditional bit ? */
#	define R300_DEPTH_FORMAT_UNK32          (32 << 0)

/* gap */
#define R300_RB3D_DEPTHOFFSET               0x4F20
#define R300_RB3D_DEPTHPITCH                0x4F24
#       define R300_DEPTHPITCH_MASK              0x00001FF8 /* GUESS */
#       define R300_DEPTH_TILE_ENABLE            (1 << 16) /* GUESS */
#       define R300_DEPTH_MICROTILE_ENABLE       (1 << 17) /* GUESS */
#       define R300_DEPTH_ENDIAN_NO_SWAP         (0 << 18) /* GUESS */
#       define R300_DEPTH_ENDIAN_WORD_SWAP       (1 << 18) /* GUESS */
#       define R300_DEPTH_ENDIAN_DWORD_SWAP      (2 << 18) /* GUESS */

/* BEGIN: Vertex program instruction set
// Every instruction is four dwords long:
//  DWORD 0: output and opcode
//  DWORD 1: first argument
//  DWORD 2: second argument
//  DWORD 3: third argument
//
// Notes:
//  - ABS r, a is implemented as MAX r, a, -a
//  - MOV is implemented as ADD to zero
//  - XPD is implemented as MUL + MAD
//  - FLR is implemented as FRC + ADD
//  - apparently, fglrx tries to schedule instructions so that there is at least
//    one instruction between the write to a temporary and the first read
//    from said temporary; however, violations of this scheduling are allowed
//  - register indices seem to be unrelated with OpenGL aliasing to conventional state
//  - only one attribute and one parameter can be loaded at a time; however, the
//    same attribute/parameter can be used for more than one argument
//  - the second software argument for POW is the third hardware argument (no idea why)
//  - MAD with only temporaries as input seems to use VPI_OUT_SELECT_MAD_2
//
// There is some magic surrounding LIT:
//  The single argument is replicated across all three inputs, but swizzled:
//   First argument: xyzy
//   Second argument: xyzx
//   Third argument: xyzw
//  Whenever the result is used later in the fragment program, fglrx forces x and w
//  to be 1.0 in the input selection; I don't know whether this is strictly necessary */
#define R300_VPI_OUT_OP_DOT                     (1 << 0)
#define R300_VPI_OUT_OP_MUL                     (2 << 0)
#define R300_VPI_OUT_OP_ADD                     (3 << 0)
#define R300_VPI_OUT_OP_MAD                     (4 << 0)
#define R300_VPI_OUT_OP_DST                     (5 << 0)
#define R300_VPI_OUT_OP_FRC                     (6 << 0)
#define R300_VPI_OUT_OP_MAX                     (7 << 0)
#define R300_VPI_OUT_OP_MIN                     (8 << 0)
#define R300_VPI_OUT_OP_SGE                     (9 << 0)
#define R300_VPI_OUT_OP_SLT                     (10 << 0)
#define R300_VPI_OUT_OP_UNK12                   (12 << 0) /* Used in GL_POINT_DISTANCE_ATTENUATION_ARB, vector(scalar, vector) */
#define R300_VPI_OUT_OP_EXP                     (65 << 0)
#define R300_VPI_OUT_OP_LOG                     (66 << 0)
#define R300_VPI_OUT_OP_UNK67                   (67 << 0) /* Used in fog computations, scalar(scalar) */
#define R300_VPI_OUT_OP_LIT                     (68 << 0)
#define R300_VPI_OUT_OP_POW                     (69 << 0)
#define R300_VPI_OUT_OP_RCP                     (70 << 0)
#define R300_VPI_OUT_OP_RSQ                     (72 << 0)
#define R300_VPI_OUT_OP_UNK73                   (73 << 0) /* Used in GL_POINT_DISTANCE_ATTENUATION_ARB, scalar(scalar) */
#define R300_VPI_OUT_OP_EX2                     (75 << 0)
#define R300_VPI_OUT_OP_LG2                     (76 << 0)
#define R300_VPI_OUT_OP_MAD_2                   (128 << 0)
#define R300_VPI_OUT_OP_UNK129                  (129 << 0) /* all temps, vector(scalar, vector, vector) */

#define R300_VPI_OUT_REG_CLASS_TEMPORARY        (0 << 8)
#define R300_VPI_OUT_REG_CLASS_RESULT           (2 << 8)
#define R300_VPI_OUT_REG_CLASS_MASK             (31 << 8)

#define R300_VPI_OUT_REG_INDEX_SHIFT            13
#define R300_VPI_OUT_REG_INDEX_MASK             (31 << 13) /* GUESS based on fglrx native limits */

#define R300_VPI_OUT_WRITE_X                    (1 << 20)
#define R300_VPI_OUT_WRITE_Y                    (1 << 21)
#define R300_VPI_OUT_WRITE_Z                    (1 << 22)
#define R300_VPI_OUT_WRITE_W                    (1 << 23)

#define R300_VPI_IN_REG_CLASS_TEMPORARY         (0 << 0)
#define R300_VPI_IN_REG_CLASS_ATTRIBUTE         (1 << 0)
#define R300_VPI_IN_REG_CLASS_PARAMETER         (2 << 0)
#define R300_VPI_IN_REG_CLASS_NONE              (9 << 0)
#define R300_VPI_IN_REG_CLASS_MASK              (31 << 0) /* GUESS */

#define R300_VPI_IN_REG_INDEX_SHIFT             5
#define R300_VPI_IN_REG_INDEX_MASK              (255 << 5) /* GUESS based on fglrx native limits */

/* The R300 can select components from the input register arbitrarily.
// Use the following constants, shifted by the component shift you
// want to select */
#define R300_VPI_IN_SELECT_X    0
#define R300_VPI_IN_SELECT_Y    1
#define R300_VPI_IN_SELECT_Z    2
#define R300_VPI_IN_SELECT_W    3
#define R300_VPI_IN_SELECT_ZERO 4
#define R300_VPI_IN_SELECT_ONE  5
#define R300_VPI_IN_SELECT_MASK 7

#define R300_VPI_IN_X_SHIFT                     13
#define R300_VPI_IN_Y_SHIFT                     16
#define R300_VPI_IN_Z_SHIFT                     19
#define R300_VPI_IN_W_SHIFT                     22

#define R300_VPI_IN_NEG_X                       (1 << 25)
#define R300_VPI_IN_NEG_Y                       (1 << 26)
#define R300_VPI_IN_NEG_Z                       (1 << 27)
#define R300_VPI_IN_NEG_W                       (1 << 28)
/* END */

//BEGIN: Packet 3 commands

// A primitive emission dword.
#define R300_PRIM_TYPE_NONE                     (0 << 0)
#define R300_PRIM_TYPE_POINT                    (1 << 0)
#define R300_PRIM_TYPE_LINE                     (2 << 0)
#define R300_PRIM_TYPE_LINE_STRIP               (3 << 0)
#define R300_PRIM_TYPE_TRI_LIST                 (4 << 0)
#define R300_PRIM_TYPE_TRI_FAN                  (5 << 0)
#define R300_PRIM_TYPE_TRI_STRIP                (6 << 0)
#define R300_PRIM_TYPE_TRI_TYPE2                (7 << 0)
#define R300_PRIM_TYPE_RECT_LIST                (8 << 0)
#define R300_PRIM_TYPE_3VRT_POINT_LIST          (9 << 0)
#define R300_PRIM_TYPE_3VRT_LINE_LIST           (10 << 0)
#define R300_PRIM_TYPE_POINT_SPRITES            (11 << 0) // GUESS (based on r200)
#define R300_PRIM_TYPE_LINE_LOOP                (12 << 0)
#define R300_PRIM_TYPE_QUADS                    (13 << 0)
#define R300_PRIM_TYPE_QUAD_STRIP               (14 << 0)
#define R300_PRIM_TYPE_POLYGON                  (15 << 0)
#define R300_PRIM_TYPE_MASK                     0xF
#define R300_PRIM_WALK_IND                      (1 << 4)
#define R300_PRIM_WALK_LIST                     (2 << 4)
#define R300_PRIM_WALK_RING                     (3 << 4)
#define R300_PRIM_WALK_MASK                     (3 << 4)
#define R300_PRIM_COLOR_ORDER_BGRA              (0 << 6) // GUESS (based on r200)
#define R300_PRIM_COLOR_ORDER_RGBA              (1 << 6) // GUESS
#define R300_PRIM_NUM_VERTICES_SHIFT            16

               /* Registers for CP and Microcode Engine */
#define CP_ME_RAM_ADDR               0x07d4
#define CP_ME_RAM_RADDR              0x07d8
#define CP_ME_RAM_DATAH              0x07dc
#define CP_ME_RAM_DATAL              0x07e0

#define CP_RB_BASE                   0x0700
#define CP_RB_CNTL                   0x0704
#define CP_RB_RPTR_ADDR              0x070c
#define CP_RB_RPTR                   0x0710
#define CP_RB_WPTR                   0x0714

#define CP_IB_BASE                   0x0738
#define CP_IB_BUFSZ                  0x073c

#define CP_CSQ_CNTL                  0x0740
#       define CSQ_CNT_PRIMARY_MASK     (0xff << 0)
#       define CSQ_PRIDIS_INDDIS        (0    << 28)
#       define CSQ_PRIPIO_INDDIS        (1    << 28)
#       define CSQ_PRIBM_INDDIS         (2    << 28)
#       define CSQ_PRIPIO_INDBM         (3    << 28)
#       define CSQ_PRIBM_INDBM          (4    << 28)
#       define CSQ_PRIPIO_INDPIO        (15   << 28)
#define CP_CSQ_STAT                  0x07f8
#       define CSQ_RPTR_PRIMARY_MASK    (0xff <<  0)
#       define CSQ_WPTR_PRIMARY_MASK    (0xff <<  8)
#       define CSQ_RPTR_INDIRECT_MASK   (0xff << 16)
#       define CSQ_WPTR_INDIRECT_MASK   (0xff << 24)
#define CP_CSQ_ADDR                  0x07f0
#define CP_CSQ_DATA                  0x07f4
#define CP_CSQ_APER_PRIMARY          0x1000
#define CP_CSQ_APER_INDIRECT         0x1300

#define CP_RB_WPTR_DELAY             0x0718
#       define PRE_WRITE_TIMER_SHIFT    0
#       define PRE_WRITE_LIMIT_SHIFT    23

#define AIC_CNTL                     0x01d0
#       define PCIGART_TRANSLATE_EN     (1 << 0)
#define AIC_LO_ADDR                  0x01dc



				/* Constants */
#define LAST_FRAME_REG               GUI_SCRATCH_REG0
#define LAST_CLEAR_REG               GUI_SCRATCH_REG2



				/* CP packet types */
#define CP_PACKET0                           0x00000000
#define CP_PACKET1                           0x40000000
#define CP_PACKET2                           0x80000000
#define CP_PACKET3                           0xC0000000
#       define CP_PACKET_MASK                0xC0000000
#       define CP_PACKET_COUNT_MASK          0x3fff0000
#       define CP_PACKET_MAX_DWORDS          (1 << 12)
#       define CP_PACKET0_REG_MASK           0x000007ff
#       define CP_PACKET1_REG0_MASK          0x000007ff
#       define CP_PACKET1_REG1_MASK          0x003ff800

#define CP_PACKET0_ONE_REG_WR                0x00008000

#define CP_PACKET3_NOP                       0xC0001000
#define CP_PACKET3_NEXT_CHAR                 0xC0001900
#define CP_PACKET3_PLY_NEXTSCAN              0xC0001D00
#define CP_PACKET3_SET_SCISSORS              0xC0001E00
#define CP_PACKET3_3D_RNDR_GEN_INDX_PRIM     0xC0002300
#define CP_PACKET3_LOAD_MICROCODE            0xC0002400
#define CP_PACKET3_WAIT_FOR_IDLE             0xC0002600
#define CP_PACKET3_3D_DRAW_VBUF              0xC0002800
#define CP_PACKET3_3D_DRAW_IMMD              0xC0002900
#define CP_PACKET3_3D_DRAW_INDX              0xC0002A00
#define CP_PACKET3_LOAD_PALETTE              0xC0002C00
#define R200_CP_PACKET3_3D_DRAW_IMMD_2              0xc0003500
#define CP_PACKET3_3D_LOAD_VBPNTR            0xC0002F00
#define CP_PACKET3_CNTL_PAINT                0xC0009100
#define CP_PACKET3_CNTL_BITBLT               0xC0009200
#define CP_PACKET3_CNTL_SMALLTEXT            0xC0009300
#define CP_PACKET3_CNTL_HOSTDATA_BLT         0xC0009400
#define CP_PACKET3_CNTL_POLYLINE             0xC0009500
#define CP_PACKET3_CNTL_POLYSCANLINES        0xC0009800
#define CP_PACKET3_CNTL_PAINT_MULTI          0xC0009A00
#define CP_PACKET3_CNTL_BITBLT_MULTI         0xC0009B00
#define CP_PACKET3_CNTL_TRANS_BITBLT         0xC0009C00


#define CP_VC_FRMT_XY                        0x00000000
#define CP_VC_FRMT_W0                        0x00000001
#define CP_VC_FRMT_FPCOLOR                   0x00000002
#define CP_VC_FRMT_FPALPHA                   0x00000004
#define CP_VC_FRMT_PKCOLOR                   0x00000008
#define CP_VC_FRMT_FPSPEC                    0x00000010
#define CP_VC_FRMT_FPFOG                     0x00000020
#define CP_VC_FRMT_PKSPEC                    0x00000040
#define CP_VC_FRMT_ST0                       0x00000080
#define CP_VC_FRMT_ST1                       0x00000100
#define CP_VC_FRMT_Q1                        0x00000200
#define CP_VC_FRMT_ST2                       0x00000400
#define CP_VC_FRMT_Q2                        0x00000800
#define CP_VC_FRMT_ST3                       0x00001000
#define CP_VC_FRMT_Q3                        0x00002000
#define CP_VC_FRMT_Q0                        0x00004000
#define CP_VC_FRMT_BLND_WEIGHT_CNT_MASK      0x00038000
#define CP_VC_FRMT_N0                        0x00040000
#define CP_VC_FRMT_XY1                       0x08000000
#define CP_VC_FRMT_Z1                        0x10000000
#define CP_VC_FRMT_W1                        0x20000000
#define CP_VC_FRMT_N1                        0x40000000
#define CP_VC_FRMT_Z                         0x80000000

#define CP_VC_CNTL_PRIM_TYPE_NONE            0x00000000
#define CP_VC_CNTL_PRIM_TYPE_POINT           0x00000001
#define CP_VC_CNTL_PRIM_TYPE_LINE            0x00000002
#define CP_VC_CNTL_PRIM_TYPE_LINE_STRIP      0x00000003
#define CP_VC_CNTL_PRIM_TYPE_TRI_LIST        0x00000004
#define CP_VC_CNTL_PRIM_TYPE_TRI_FAN         0x00000005
#define CP_VC_CNTL_PRIM_TYPE_TRI_STRIP       0x00000006
#define CP_VC_CNTL_PRIM_TYPE_TRI_TYPE_2      0x00000007
#define CP_VC_CNTL_PRIM_TYPE_RECT_LIST       0x00000008
#define CP_VC_CNTL_PRIM_TYPE_3VRT_POINT_LIST 0x00000009
#define CP_VC_CNTL_PRIM_TYPE_3VRT_LINE_LIST  0x0000000a
#define CP_VC_CNTL_PRIM_WALK_IND             0x00000010
#define CP_VC_CNTL_PRIM_WALK_LIST            0x00000020
#define CP_VC_CNTL_PRIM_WALK_RING            0x00000030
#define CP_VC_CNTL_COLOR_ORDER_BGRA          0x00000000
#define CP_VC_CNTL_COLOR_ORDER_RGBA          0x00000040
#define CP_VC_CNTL_MAOS_ENABLE               0x00000080
#define CP_VC_CNTL_VTX_FMT_NON_MODE   0x00000000
#define CP_VC_CNTL_VTX_FMT_MODE       0x00000100
#define CP_VC_CNTL_TCL_DISABLE               0x00000000
#define CP_VC_CNTL_TCL_ENABLE                0x00000200
#define CP_VC_CNTL_NUM_SHIFT                 16

#define VS_MATRIX_0_ADDR                   0
#define VS_MATRIX_1_ADDR                   4
#define VS_MATRIX_2_ADDR                   8
#define VS_MATRIX_3_ADDR                  12
#define VS_MATRIX_4_ADDR                  16
#define VS_MATRIX_5_ADDR                  20
#define VS_MATRIX_6_ADDR                  24
#define VS_MATRIX_7_ADDR                  28
#define VS_MATRIX_8_ADDR                  32
#define VS_MATRIX_9_ADDR                  36
#define VS_MATRIX_10_ADDR                 40
#define VS_MATRIX_11_ADDR                 44
#define VS_MATRIX_12_ADDR                 48
#define VS_MATRIX_13_ADDR                 52
#define VS_MATRIX_14_ADDR                 56
#define VS_MATRIX_15_ADDR                 60
#define VS_LIGHT_AMBIENT_ADDR             64
#define VS_LIGHT_DIFFUSE_ADDR             72
#define VS_LIGHT_SPECULAR_ADDR            80
#define VS_LIGHT_DIRPOS_ADDR              88
#define VS_LIGHT_HWVSPOT_ADDR             96
#define VS_LIGHT_ATTENUATION_ADDR        104
#define VS_MATRIX_EYE2CLIP_ADDR          112
#define VS_UCP_ADDR                      116
#define VS_GLOBAL_AMBIENT_ADDR           122
#define VS_FOG_PARAM_ADDR                123
#define VS_EYE_VECTOR_ADDR               124

#define SS_LIGHT_DCD_ADDR                  0
#define SS_LIGHT_SPOT_EXPONENT_ADDR        8
#define SS_LIGHT_SPOT_CUTOFF_ADDR         16
#define SS_LIGHT_SPECULAR_THRESH_ADDR     24
#define SS_LIGHT_RANGE_CUTOFF_ADDR        32
#define SS_VERT_GUARD_CLIP_ADJ_ADDR       48
#define SS_VERT_GUARD_DISCARD_ADJ_ADDR    49
#define SS_HORZ_GUARD_CLIP_ADJ_ADDR       50
#define SS_HORZ_GUARD_DISCARD_ADJ_ADDR    51
#define SS_SHININESS                      60


#endif /* __RADEON_REGS_H__ */
