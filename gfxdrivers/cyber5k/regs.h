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

#ifndef REGS_H
#define REGS_H

/*
 *  COP MMIO definition
 *
 */

#define OPAQUE          0
#define TRANSPARENT     1

#define TRANSPARENT_SRC         1
#define TRANSPARENT_DST         2
#define SRC_FROM_SYS            4
#define PAT_IS_MONO             8

#define  R2_ZERO        0x00
#define  R2_S_AND_D     0x01
#define  R2_S_AND_ND    0x02
#define  R2_S           0x03
#define  R2_NS_AND_D    0x04
#define  R2_D           0x05
#define  R2_S_XOR_D     0x06
#define  R2_S_OR_D      0x07
#define  R2_NS_AND_ND   0x08
#define  R2_S_XOR_ND    0x09
#define  R2_ND          0x0A
#define  R2_S_OR_ND     0x0B
#define  R2_NS          0x0C
#define  R2_NS_OR_D     0x0D
#define  R2_NS_OR_ND    0x0E
#define  R2_ONE         0x0F

#define  R3_S           0xF0
#define  R3_P           0xCC


#define COP_BASE        0xBF000
#define COP_STAT        0x11 + COP_BASE
    #define HBLTW_NOTREADY      0x01
    #define HOSTFF_NOTEMPTY     0x02
    #define CMDFF_FULL          0x04
    #define SUSPEND_COP         0x08
    #define COP_STOPPED         0x10
    #define TERMINATE_COP       0x20
    #define HBLT_NOTACKZ        0x40
    #define COP_BUSY            0x80

#define SRC1WIDTH               0x18 + COP_BASE
#define COPFMT                  0x1C + COP_BASE
#define ERRORTERM               0x20 + COP_BASE
#define K1                      0x24 + COP_BASE
#define K2                      0x28 + COP_BASE
#define FMIX                    0x48 + COP_BASE
#define BMIX                    0x49 + COP_BASE
#define FCOLOR                  0x58 + COP_BASE
#define BCOLOR                  0x5C + COP_BASE
#define HEIGHTWIDTH             0x60 + COP_BASE
#define DIMW                    0x60 + COP_BASE
#define DIMH                    0x62 + COP_BASE
#define SRC1BASE                0x70 + COP_BASE
#define DSTXROT                 0x78 + COP_BASE
#define DSTYROT                 0x7A + COP_BASE
#define PATYROT                 0x7A + COP_BASE
#define PIXOP                   0x7C + COP_BASE
#define PIXOP_LO                0x7C + COP_BASE
#define PIXOP_HI                0x7E + COP_BASE

    /* Direction */
    #define YMAJOR                      0x1
    #define DEC_Y                       0x2
    #define DY_NEG                      0x2
    #define DEC_X                       0x4
    #define DX_NEG                      0x4
    #define ERRORTERM_POS               0x8

    /* Draw mode */
    #define DRAW_1ST_PIXEL_NULL         0x10
    #define DRAW_LAST_PIXEL_NULL        0x20
    #define DRAW_AREA_BOUND             0x30

    /* transparent mode */
    #define TRANS_IS_SRC1               0x0000
    #define TRANS_IS_SRC2               0x0100
    #define TRANS_IS_DST                0x0200
    #define TRANS_INVERT                0x0400
    #define TRANS_ENABLE                0x0800

    /* Cop Operation */
    #define PAT_OPAQUE_TEXTOUT          0x1000
    #define PAT_OPAQUE_TILE             0x2000
    #define PAT_OPAQUE_LINE             0x3000
    #define PAT_TRANS_TEXTOUT           0x5000
    #define PAT_TRANS_TILE              0x6000
    #define PAT_TRANS_LINE              0x7000
    #define PAT_FIXFGD                  0x8000
    #define PAT_COLOR_TILE              0x9000

    /* Host-Mem Direction */
    #define HOST_READ_SRC1              0x10000
    #define HOST_WRITE_SRC1             0x20000
    #define HOST_WRITE_SRC2             0x30000

    /* Source2 Select */
    #define SRC2_IS_COLOR               0x000000
    #define SRC2_IS_OPAQUE_MONO         0x100000
    #define SRC2_IS_FGDCOLOR            0x200000
    #define SRC2_IS_TRANS_MONO          0x500000

    /* Cop Command */
    #define COP_STEP_DRAW               0x4000000
    #define COP_LINE_DRAW               0x5000000
    #define COP_PXBLT                   0x8000000
    #define COP_INVERT_PXBLT            0x9000000
    #define COP_PXBLT256                0xB000000

    /* Fore&Back */
    #define FGD_IS_SRC1                 0x20000000
    #define FGD_IS_COLOR                0x00000000
    #define BGD_IS_SRC1                 0x80000000
    #define BGD_IS_COLOR                0x00000000

#define SRC2WIDTH               0x118 + COP_BASE
#define COPFLAGS                0x130 + COP_BASE
    #define FMONO_ENABLE        0x10
    #define FMONO_DISABLE       0xEF
    #define COP_1WS             0x4

#define FASTMONOSIZE            0x13C + COP_BASE
#define PATXROT                 0x150 + COP_BASE
#define SRC1PTR                 0x170 + COP_BASE
#define SRC2PTR                 0x174 + COP_BASE
#define DSTPTR                  0x178 + COP_BASE
#define DSTWIDTH                0x218 + COP_BASE

/* ---------------------------------------------------------------------- */

#define   PORT46E8     0x46E8 /* R   */
#define   PORT102      0x102  /* R/W */
#define   MISCREAD     0x3CC  /* R   */
#define   MISCWRITE    0x3C2  /* W   */
#define   SEQINDEX     0x3C4  /* R/W */
#define   SEQDATA      0x3C5  /* R/W */
#define   CRTINDEX     0x3D4  /* R/W */
#define   CRTDATA      0x3D5  /* R/W */
#define   ATTRRESET    0x3DA  /* R/W */
#define   ATTRINDEX    0x3C0  /* R/W */
#define   ATTRDATAW    0x3C0  /* W, Attrib write data port */
#define   ATTRDATAR    0x3C1  /* R, Attrib read data port  */
#define   GRAINDEX     0x3CE  /* R/W */
#define   GRADATA      0x3CF  /* R/W */
#define   RAMDACMASK   0x3C6  /* R/W, Mask register */
#define   RAMDACINDEXR 0x3C7  /* R/W, RAM read index port  */
#define   RAMDACINDEXW 0x3C8  /* R/W, RAM write index port */
#define   RAMDACDATA   0x3C9  /* R/W, RAM Date port */
#define   IGS3CEINDEX  0x3CE  /* R/W */
#define   IGS3CFDATA   0x3CF  /* R/W */
#define   IGS3D4INDEX  0x3D4  /* R/W */
#define   IGS3D5DATA   0x3D5  /* R/W */
#define   IGS3C4INDEX  0x3C4  /* R/W */
#define   IGS3C5DATA   0x3C5  /* R/W */

#define   SEQCOUNT  0x05
#define   MISCCOUNT 0x01
#define   CRTCOUNT  0x19
#define   ATTRCOUNT 0x15
#define   GRACOUNT  0x09
#define   EXTPARTIALCOUNT 8  /* define 8 extended regs for color depth change */

#define   SREGCOUNT SEQCOUNT+MISCCOUNT+CRTCOUNT+ATTRCOUNT+GRACOUNT
#define   EREGCOUNT EXTPARTIALCOUNT * 2 + 1


#define PIXFORMAT_8BPP		0
#define PIXFORMAT_16BPP		1
#define PIXFORMAT_24BPP		2

#define VISUALID_256		1
#define VISUALID_64K		2
#define VISUALID_16M		4
#define VISUALID_32K		6

#define FUNC_CTL		0x3c
#define FUNC_CTL_EXTREGENBL		0x80	/* enable access to 0xbcxxx		*/

#define BIU_BM_CONTROL		0x3e
#define BIU_BM_CONTROL_ENABLE		0x01	/* enable bus-master			*/
#define BIU_BM_CONTROL_BURST		0x02	/* enable burst				*/
#define BIU_BM_CONTROL_BACK2BACK	0x04	/* enable back to back			*/

#define X_V2_VID_MEM_START	0x40
#define X_V2_VID_SRC_WIDTH	0x43
#define X_V2_X_START		0x45
#define X_V2_X_END		0x47
#define X_V2_Y_START		0x49
#define X_V2_Y_END		0x4b
#define X_V2_VID_SRC_WIN_WIDTH	0x4d

#define Y_V2_DDA_X_INC		0x43
#define Y_V2_DDA_Y_INC		0x47
#define Y_V2_VID_FIFO_CTL	0x49
#define Y_V2_VID_FMT		0x4b
#define Y_V2_VID_DISP_CTL1	0x4c
#define Y_V2_VID_FIFO_CTL1	0x4d

#define J_X2_VID_MEM_START	0x40
#define J_X2_VID_SRC_WIDTH	0x43
#define J_X2_X_START		0x47
#define J_X2_X_END		0x49
#define J_X2_Y_START		0x4b
#define J_X2_Y_END		0x4d
#define J_X2_VID_SRC_WIN_WIDTH	0x4f

#define K_X2_DDA_X_INIT		0x40
#define K_X2_DDA_X_INC		0x42
#define K_X2_DDA_Y_INIT		0x44
#define K_X2_DDA_Y_INC		0x46
#define K_X2_VID_FMT		0x48
#define K_X2_VID_DISP_CTL1	0x49

#define K_CAP_X2_CTL1		0x49

#define CAP_X_START		0x60
#define CAP_X_END		0x62
#define CAP_Y_START		0x64
#define CAP_Y_END		0x66
#define CAP_DDA_X_INIT		0x68
#define CAP_DDA_X_INC		0x6a
#define CAP_DDA_Y_INIT		0x6c
#define CAP_DDA_Y_INC		0x6e

#define MEM_CTL2		0x72
#define MEM_CTL2_SIZE_2MB		0x01
#define MEM_CTL2_SIZE_4MB		0x02
#define MEM_CTL2_SIZE_MASK		0x03
#define MEM_CTL2_64BIT			0x04

#define EXT_FIFO_CTL		0x74

#define CAP_PIP_X_START		0x80
#define CAP_PIP_X_END		0x82
#define CAP_PIP_Y_START		0x84
#define CAP_PIP_Y_END		0x86

#define CAP_NEW_CTL1		0x88

#define CAP_NEW_CTL2		0x89

#define BM_CTRL0		0x9c
#define BM_CTRL1		0x9d

#define CAP_MODE1		0xa4
#define CAP_MODE1_8BIT			0x01	/* enable 8bit capture mode		*/
#define CAP_MODE1_CCIR656		0x02	/* CCIR656 mode				*/
#define CAP_MODE1_IGNOREVGT		0x04	/* ignore VGT				*/
#define CAP_MODE1_ALTFIFO		0x10	/* use alternate FIFO for capture	*/
#define CAP_MODE1_SWAPUV		0x20	/* swap UV bytes			*/
#define CAP_MODE1_MIRRORY		0x40	/* mirror vertically			*/
#define CAP_MODE1_MIRRORX		0x80	/* mirror horizontally			*/

#define CAP_MODE2		0xa5

#define Y_TV_CTL		0xae

#define EXT_MEM_START		0xc0		/* ext start address 21 bits		*/
#define HOR_PHASE_SHIFT		0xc2		/* high 3 bits				*/
#define EXT_SRC_WIDTH		0xc3		/* ext offset phase  10 bits		*/
#define EXT_SRC_HEIGHT		0xc4		/* high 6 bits				*/
#define EXT_X_START		0xc5		/* ext->screen, 16 bits			*/
#define EXT_X_END		0xc7		/* ext->screen, 16 bits			*/
#define EXT_Y_START		0xc9		/* ext->screen, 16 bits			*/
#define EXT_Y_END		0xcb		/* ext->screen, 16 bits			*/
#define EXT_SRC_WIN_WIDTH	0xcd		/* 8 bits				*/
#define EXT_COLOUR_COMPARE	0xce		/* 24 bits				*/
#define EXT_DDA_X_INIT		0xd1		/* ext->screen 16 bits			*/
#define EXT_DDA_X_INC		0xd3		/* ext->screen 16 bits			*/
#define EXT_DDA_Y_INIT		0xd5		/* ext->screen 16 bits			*/
#define EXT_DDA_Y_INC		0xd7		/* ext->screen 16 bits			*/

#define EXT_VID_FIFO_CTL	0xd9

#define EXT_VID_FMT		0xdb
#define EXT_VID_FMT_YUV422		0x00	/* formats - does this cause conversion? */
#define EXT_VID_FMT_RGB555		0x01
#define EXT_VID_FMT_RGB565		0x02
#define EXT_VID_FMT_RGB888_24		0x03
#define EXT_VID_FMT_RGB888_32		0x04
#define EXT_VID_FMT_DUP_PIX_ZOON	0x08	/* duplicate pixel zoom			*/
#define EXT_VID_FMT_MOD_3RD_PIX		0x20	/* modify 3rd duplicated pixel		*/
#define EXT_VID_FMT_DBL_H_PIX		0x40	/* double horiz pixels			*/
#define EXT_VID_FMT_UV128		0x80	/* UV data offset by 128		*/

#define EXT_VID_DISP_CTL1	0xdc 
#define EXT_VID_DISP_CTL1_INTRAM	0x01	/* video pixels go to internal RAM	*/
#define EXT_VID_DISP_CTL1_IGNORE_CCOMP	0x02	/* ignore colour compare registers	*/
#define EXT_VID_DISP_CTL1_NOCLIP	0x04	/* do not clip to 16235,16240		*/
#define EXT_VID_DISP_CTL1_UV_AVG	0x08	/* U/V data is averaged			*/
#define EXT_VID_DISP_CTL1_Y128		0x10	/* Y data offset by 128			*/
#define EXT_VID_DISP_CTL1_VINTERPOL_OFF	0x20	/* vertical interpolation off		*/
#define EXT_VID_DISP_CTL1_FULL_WIN	0x40	/* video out window full		*/
#define EXT_VID_DISP_CTL1_ENABLE_WINDOW	0x80	/* enable video window			*/

#define EXT_VID_FIFO_CTL1	0xdd

#define VFAC_CTL1		0xe8
#define VFAC_CTL1_CAPTURE		0x01	/* capture enable			*/
#define VFAC_CTL1_VFAC_ENABLE		0x02	/* vfac enable				*/
#define VFAC_CTL1_FREEZE_CAPTURE	0x04	/* freeze capture			*/
#define VFAC_CTL1_FREEZE_CAPTURE_SYNC	0x08	/* sync freeze capture			*/
#define VFAC_CTL1_VALIDFRAME_SRC	0x10	/* select valid frame source		*/
#define VFAC_CTL1_PHILIPS		0x40	/* select Philips mode			*/
#define VFAC_CTL1_MODVINTERPOLCLK	0x80	/* modify vertical interpolation clocl	*/

#define VFAC_CTL2		0xe9
#define VFAC_CTL2_INVERT_VIDDATAVALID	0x01	/* invert video data valid		*/
#define VFAC_CTL2_INVERT_GRAPHREADY	0x02	/* invert graphic ready output sig	*/
#define VFAC_CTL2_INVERT_DATACLK	0x04	/* invert data clock signal		*/
#define VFAC_CTL2_INVERT_HSYNC		0x08	/* invert hsync input			*/
#define VFAC_CTL2_INVERT_VSYNC		0x10	/* invert vsync input			*/
#define VFAC_CTL2_INVERT_FRAME		0x20	/* invert frame odd/even input		*/
#define VFAC_CTL2_INVERT_BLANK		0x40	/* invert blank output			*/
#define VFAC_CTL2_INVERT_OVSYNC		0x80	/* invert other vsync input		*/

#define VFAC_CTL3		0xea
#define VFAC_CTL3_CAP_IRQ		0x40	/* enable capture interrupt		*/

#define CAP_MEM_START		0xeb		/* 18 bits				*/
#define CAP_MAP_WIDTH		0xed		/* high 6 bits				*/
#define CAP_PITCH		0xee		/* 8 bits				*/

#define CAP_CTL_MISC		0xef
#define CAP_CTL_MISC_HDIV		0x01
#define CAP_CTL_MISC_HDIV4		0x02
#define CAP_CTL_MISC_ODDEVEN		0x04
#define CAP_CTL_MISC_HSYNCDIV2		0x08
#define CAP_CTL_MISC_SYNCTZHIGH		0x10
#define CAP_CTL_MISC_SYNCTZOR		0x20
#define CAP_CTL_MISC_DISPUSED		0x80

#define REG_BANK		0xfa
#define REG_BANK_X			0x00
#define REG_BANK_Y			0x01
#define REG_BANK_W			0x02
#define REG_BANK_T			0x03
#define REG_BANK_J			0x04
#define REG_BANK_K			0x05

/*
 * Bus-master
 */
#define BM_VID_ADDR_LOW		0xbc040
#define BM_VID_ADDR_HIGH	0xbc044
#define BM_ADDRESS_LOW		0xbc080
#define BM_ADDRESS_HIGH		0xbc084
#define BM_LENGTH		0xbc088
#define BM_CONTROL		0xbc08c
#define BM_CONTROL_ENABLE		0x01	/* enable transfer			*/
#define BM_CONTROL_IRQEN		0x02	/* enable IRQ at end of transfer	*/
#define BM_CONTROL_INIT			0x04	/* initialise status & count		*/
#define BM_COUNT		0xbc090		/* read-only				*/

/*
 * Graphics Co-processor
 */
#define CO_CMD_L_PATTERN_FGCOL	0x8000
#define CO_CMD_L_INC_LEFT	0x0004
#define CO_CMD_L_INC_UP		0x0002

#define CO_CMD_H_SRC_PIXMAP	0x2000
#define CO_CMD_H_BLITTER	0x0800

#define CO_REG_CONTROL		0xbf011
#define CO_REG_SRC_WIDTH	0xbf018
#define CO_REG_PIX_FORMAT	0xbf01c
#define CO_REG_FORE_MIX		0xbf048
#define CO_REG_FOREGROUND	0xbf058
#define CO_REG_WIDTH		0xbf060
#define CO_REG_HEIGHT		0xbf062
#define CO_REG_X_PHASE		0xbf078
#define CO_REG_CMD_L		0xbf07c
#define CO_REG_CMD_H		0xbf07e
#define CO_REG_SRC_PTR		0xbf170
#define CO_REG_DEST_PTR		0xbf178
#define CO_REG_DEST_WIDTH	0xbf218

#endif

