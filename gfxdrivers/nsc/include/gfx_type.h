/*
 * $Workfile: gfx_type.h $
 *
 * This header file defines the pneumonics used when calling Durango routines. 
 * This file is automatically included by gfx_rtns.h
 */

/* NSC_LIC_ALTERNATIVE_PREAMBLE
 *
 * Revision 1.0
 *
 * National Semiconductor Alternative GPL-BSD License
 *
 * National Semiconductor Corporation licenses this software 
 * ("Software"):
 *
 * National Xfree frame buffer driver
 *
 * under one of the two following licenses, depending on how the 
 * Software is received by the Licensee.
 * 
 * If this Software is received as part of the Linux Framebuffer or
 * other GPL licensed software, then the GPL license designated 
 * NSC_LIC_GPL applies to this Software; in all other circumstances 
 * then the BSD-style license designated NSC_LIC_BSD shall apply.
 *
 * END_NSC_LIC_ALTERNATIVE_PREAMBLE */

/* NSC_LIC_BSD
 *
 * National Semiconductor Corporation Open Source License for 
 *
 * National Xfree frame buffer driver
 *
 * (BSD License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 *   * Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer. 
 *
 *   * Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the following 
 *     disclaimer in the documentation and/or other materials provided 
 *     with the distribution. 
 *
 *   * Neither the name of the National Semiconductor Corporation nor 
 *     the names of its contributors may be used to endorse or promote 
 *     products derived from this software without specific prior 
 *     written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE,
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * END_NSC_LIC_BSD */

/* NSC_LIC_GPL
 *
 * National Semiconductor Corporation Gnu General Public License for 
 *
 * National Xfree frame buffer driver
 *
 * (GPL License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted under the terms of the GNU General 
 * Public License as published by the Free Software Foundation; either 
 * version 2 of the License, or (at your option) any later version  
 *
 * In addition to the terms of the GNU General Public License, neither 
 * the name of the National Semiconductor Corporation nor the names of 
 * its contributors may be used to endorse or promote products derived 
 * from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE, 
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE. See the GNU General Public License for more details. 
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with this file; if not, write to the Free Software Foundation, 
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 *
 * END_NSC_LIC_GPL */


#ifndef _gfx_type_h
#define _gfx_type_h

/* MSR DEFINITIONS */

typedef enum DevStatus { FOUND, NOT_KNOWN, REQ_NOT_FOUND, REQ_NOT_INSTALLED } DEV_STATUS;

typedef struct msr {
	DEV_STATUS Present;		   /* Node enumeration status                              */
	unsigned char Id;		   /* Device ID (from MSR specs)                           */
	unsigned long Address;	   /* Address - 32-bit MBus address at which 'Id' is found */
} MSR;

typedef struct mValue {
	unsigned long high;
	unsigned long low;
} Q_WORD;

typedef struct mbusNode {
	unsigned long address;
	unsigned int  deviceId;
	unsigned int  claimed;
} MBUS_NODE;

/* MSR ARRAY INDEXES */
/* These are indexes into the array of MBus devices. These     */
/* should not be confused with the class codes at MSR register */
/* 0x2000.                                                     */

#define RC_ID_MBIU0 0x00
#define RC_ID_MBIU1 0x01
#define RC_ID_MCP   0x02
#define RC_ID_MPCI  0x03
#define RC_ID_MC    0x04
#define RC_ID_GP    0x05
#define RC_ID_VG    0x06
#define RC_ID_DF    0x07
#define RC_ID_FG    0x08
#define RC_ID_VA    0x09
#define CP_ID_MBIU	0x0A
#define CP_ID_MPCI	0x0B
#define CP_ID_USB2	0x0C
#define CP_ID_ATAC	0x0D
#define CP_ID_MDD 	0x0E
#define CP_ID_ACC 	0x0F
#define CP_ID_USB1	0x10
#define CP_ID_MCP 	0x11
 
/* MBUS DEVICE CLASS CODES */
/* These are the device ids for the known Redcloud MBus devices. */

#define RC_CC_MBIU  0x01
#define RC_CC_MCP   0x02
#define RC_CC_MPCI  0x05
#define RC_CC_MC    0x20
#define RC_CC_GP    0x3D
#define RC_CC_VG    0x3E
#define RC_CC_DF    0x3F
#define RC_CC_FG    0xF0
#define RC_CC_VA    0x86
#define CP_CC_MBIU	0x01
#define CP_CC_MPCI	0x05
#define CP_CC_USB2	0x42
#define CP_CC_ATAC	0x47
#define CP_CC_MDD 	0xDF
#define CP_CC_ACC 	0x33
#define CP_CC_USB1	0x42
#define CP_CC_MCP 	0x02

/* VAIL AND MBIUS ARE AT KNOWN ADDRESSES */
/* We can initialize the addresses of these devices in advance,  */
/* as their location should never change.                        */

#define RC_MB0_MBIU0	0x10000000
#define RC_MB0_MBIU1	0x40000000
#define CP_MB0_MBIU0	0x51010000
#define RC_MB0_CPU 	  	0x00000000
#define FAKE_ADDRESS    0xFFFFFFFF

/* MSR PORT DESCRIPTORS */

#define NOT_POPULATED 	0
#define NOT_INSTALLED   0xFFFE
#define REFLECTIVE		0xFFFF

/* CRC DATA SOURCES */

#define CRC_SOURCE_GFX_DATA  0x00
#define CRC_SOURCE_CRT_RGB   0x01
#define CRC_SOURCE_FP_DATA   0x02


/* TV DEFINITIONS */

typedef enum TVStandardType {
	TV_STANDARD_NTSC = 1,
    TV_STANDARD_PAL
} TVStandardType;

typedef enum GfxOnTVType {
	GFX_ON_TV_SQUARE_PIXELS = 1,
	GFX_ON_TV_NO_SCALING
} GfxOnTVType;

#define CRT_DISABLE 0x00
#define CRT_ENABLE  0x01
#define CRT_STANDBY 0x02
#define CRT_SUSPEND 0x03

#define TV_OUTPUT_COMPOSITE	0x01
#define TV_OUTPUT_S_VIDEO	0x02
#define TV_OUTPUT_YUV   	0x03
#define TV_OUTPUT_SCART   	0x04

#define TV_FLICKER_FILTER_NONE	      0x01
#define TV_FLICKER_FILTER_NORMAL      0x02
#define TV_FLICKER_FILTER_INTERLACED  0x03

#define TV_YC_DELAY_NONE	          0x01
#define TV_Y_DELAY_ONE_PIXEL          0x02
#define TV_C_DELAY_ONE_PIXEL          0x03
#define TV_C_DELAY_TWO_PIXELS         0x04

#define TV_SUB_CARRIER_RESET_NEVER              0x01
#define TV_SUB_CARRIER_RESET_EVERY_TWO_LINES    0x02
#define TV_SUB_CARRIER_RESET_EVERY_TWO_FRAMES   0x03
#define TV_SUB_CARRIER_RESET_EVERY_FOUR_FRAMES  0x04

#define TVENC_RESET_EVERY_ODD_FIELD     0x01
#define TVENC_RESET_EVERY_EVEN_FIELD    0x02
#define TVENC_RESET_NEXT_ODD_FIELD      0x03
#define TVENC_RESET_NEXT_EVEN_FIELD     0x04
#define TVENC_RESET_EVERY_FIELD         0x05
#define TVENC_RESET_EVERY_X_ODD_FIELDS  0x06
#define TVENC_RESET_EVERY_X_EVEN_FIELDS 0x07

/* VBI FORMATS */

#define VBI_FORMAT_VIDEO	0x1
#define VBI_FORMAT_RAW		0x2
#define VBI_FORMAT_CC		0x4
#define VBI_FORMAT_NABTS	0x8

/* VIDEO DEFINITIONS */

#define VIDEO_FORMAT_UYVY            0x0
#define VIDEO_FORMAT_Y2YU            0x1
#define VIDEO_FORMAT_YUYV            0x2
#define VIDEO_FORMAT_YVYU            0x3
#define VIDEO_FORMAT_Y0Y1Y2Y3        0x4
#define VIDEO_FORMAT_Y3Y2Y1Y0        0x5
#define VIDEO_FORMAT_Y1Y0Y3Y2        0x6
#define VIDEO_FORMAT_Y1Y2Y3Y0        0x7
#define VIDEO_FORMAT_RGB             0x8
#define VIDEO_FORMAT_P2M_P2L_P1M_P1L 0x9
#define VIDEO_FORMAT_P1M_P1L_P2M_P2L 0xA
#define VIDEO_FORMAT_P1M_P2L_P2M_P1L 0xB

#define VIDEO_DOWNSCALE_KEEP_1_OF 0x1
#define VIDEO_DOWNSCALE_DROP_1_OF 0x2

typedef enum VideoSourceType { /* The source from which the video processor shows full screen video */
	VIDEO_SOURCE_MEMORY = 1,
	VIDEO_SOURCE_DVIP
} VideoSourceType;

typedef enum VbiSourceType { /* The source from which the video processor takes VBI */
	VBI_SOURCE_MEMORY = 1,
	VBI_SOURCE_DVIP
} VbiSourceType;

/* GENLOCK DEFINITIONS */

#define GENLOCK_SINGLE                     0x001
#define GENLOCK_FIELD_SYNC                 0x001
#define GENLOCK_CONTINUOUS                 0x002
#define GENLOCK_SYNCED_EDGE_FALLING        0x004
#define GENLOCK_SYNCING_EDGE_FALLING       0x008
#define GENLOCK_TIMEOUT                    0x010
#define GENLOCK_TVENC_RESET_EVEN_FIELD     0x020
#define GENLOCK_TVENC_RESET_BEFORE_DELAY   0x040
#define GENLOCK_TVENC_RESET                0x080
#define GENLOCK_SYNC_TO_TVENC              0x100

/* VIP DEFINITIONS */

#define VIP_MODE_C          0x1

#define VIP_CAPTURE_STOP_LINE	0x1
#define VIP_CAPTURE_STOP_FIELD	0x2
#define VIP_CAPTURE_START_FIELD	0x4

#define VBI_ANCILLARY       0x1
#define VBI_TASK_A          0x2
#define VBI_TASK_B          0x4

/* VGA STRUCTURE */

#define GFX_STD_CRTC_REGS 25
#define GFX_EXT_CRTC_REGS 16

#define GFX_VGA_FLAG_MISC_OUTPUT	0x00000001
#define GFX_VGA_FLAG_STD_CRTC		0x00000002
#define GFX_VGA_FLAG_EXT_CRTC		0x00000004

/* FS450 TV Standard flags */

#define GFX_TV_STANDARD_NTSC_M 0x0001
#define GFX_TV_STANDARD_NTSC_M_J 0x0002
#define GFX_TV_STANDARD_PAL_B 0x0004
#define GFX_TV_STANDARD_PAL_D 0x0008
#define GFX_TV_STANDARD_PAL_H 0x0010
#define GFX_TV_STANDARD_PAL_I 0x0020
#define GFX_TV_STANDARD_PAL_M 0x0040
#define GFX_TV_STANDARD_PAL_N 0x0080
#define GFX_TV_STANDARD_PAL_G 0x0100

/* FS450 VGA Mode flags */

#define GFX_VGA_MODE_UNKNOWN 0
#define GFX_VGA_MODE_640X480 0x0001
#define GFX_VGA_MODE_720X487 0x0002
#define GFX_VGA_MODE_720X576 0x0004
#define GFX_VGA_MODE_800X600 0x0008
#define GFX_VGA_MODE_1024X768 0x0010

/* FS450 TVout mode flags */

#define GFX_TVOUT_MODE_CVBS 0x0001
#define GFX_TVOUT_MODE_YC 0x0002
#define GFX_TVOUT_MODE_RGB 0x0004
#define GFX_TVOUT_MODE_CVBS_YC (GFX_TVOUT_MODE_CVBS | GFX_TVOUT_MODE_YC)

/* FS450 Luma and Chroma Filters */

#define GFX_LUMA_FILTER 0x0001
#define GFX_CHROMA_FILTER 0x0002

/* APS Trigger Bits */

#define GFX_APS_TRIGGER_OFF 0
#define GFX_APS_TRIGGER_AGC_ONLY 1
#define GFX_APS_TRIGGER_AGC_2_LINE 2
#define GFX_APS_TRIGGER_AGC_4_LINE 3

typedef struct {
	int xsize;
	int ysize;
	int hz;
	int clock;
	unsigned char miscOutput;
	unsigned char stdCRTCregs[GFX_STD_CRTC_REGS];
	unsigned char extCRTCregs[GFX_EXT_CRTC_REGS];
} gfx_vga_struct;

/* POSSIBLE STATUS VALUES */

#define GFX_STATUS_UNSUPPORTED		(-3)
#define GFX_STATUS_BAD_PARAMETER	(-2)
#define GFX_STATUS_ERROR            (-1)
#define GFX_STATUS_OK				0

/* CPU AND VIDEO TYPES */

#define GFX_CPU_GXLV		1
#define GFX_CPU_SC1200		2
#define GFX_CPU_REDCLOUD    3
#define GFX_CPU_PYRAMID		0x20801	
	

#define GFX_VID_CS5530		1
#define GFX_VID_SC1200		2
#define GFX_VID_REDCLOUD    3

/* CHIP NAME AND REVISION */

typedef enum ChipType {
	CHIP_NOT_DETECTED,
	SC1200_REV_A,
	SC1200_REV_B1_B2,
	SC1200_REV_B3,
	SC1200_REV_C1,
	SC1200_REV_D1,
	SC1200_REV_D1_1,
	SC1200_REV_D2_MVD,	/* Macrovision disabled */
	SC1200_REV_D2_MVE,	/* Macrovision enabled  */
	SC1200_FUTURE_REV
} ChipType;

#endif /* !_gfx_type_h */
