/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
	      Michel Dänzer <michel@daenzer.net>.

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

#ifndef REG_RADEON_H
#define REG_RADEON_H

#define RBBM_STATUS			0x0e40
#define	RBBM_FIFOCNT_MASK		 0x0000007f
#define RBBM_ACTIVE			 0x80000000

/******************************************************************************
 *                  GUI Block Memory Mapped Registers                         *
 *                     These registers are FIFOed.                            *
 *****************************************************************************/

#define DST_OFFSET                      0x1404
#define DST_PITCH                       0x1408

#define SRC_Y_X                         0x1434
#define DST_Y_X                         0x1438
#define DST_HEIGHT_WIDTH                0x143c

#define DP_GUI_MASTER_CNTL              0x146c
#define GMC_SRC_PITCH_OFFSET_NODEFAULT	 0x00000001
#define GMC_DST_PITCH_OFFSET_NODEFAULT	 0x00000002
#define GMC_SRC_CLIP_NODEFAULT		 0x00000004
#define GMC_DST_CLIP_NODEFAULT		 0x00000008
#define GMC_BRUSH_SOLIDCOLOR		 0x000000d0
#define GMC_BRUSH_NONE			 0x000000f0
#define GMC_DST_8BPP			 0x00000200
#define GMC_DST_15BPP			 0x00000300
#define GMC_DST_16BPP			 0x00000400
#define GMC_DST_32BPP			 0x00000600
#define GMC_SRC_FG			 0x00001000
#define GMC_SRC_DSTCOLOR		 0x00003000
#define GMC_ROP3_PATCOPY		 0x00f00000
#define GMC_ROP3_SRCCOPY		 0x00cc0000
#define GMC_DP_SRC_RECT			 0x02000000
#define GMC_DST_CLR_CMP_FCN_CLEAR	 0x10000000
#define GMC_WRITE_MASK_DIS		 0x40000000

#define DP_BRUSH_FRGD_CLR               0x147c

#define SRC_OFFSET                      0x15ac
#define SRC_PITCH                       0x15b0

#define CLR_CMP_CNTL                    0x15c0
#define CLR_CMP_CLR_SRC                 0x15c4

#define CLR_CMP_MASK                    0x15cc

#define DST_LINE_START                  0x1600
#define DST_LINE_END                    0x1604

#if 0
#define SC_LEFT                         0x1640
#define SC_RIGHT                        0x1644
#define SC_TOP                          0x1648
#define SC_BOTTOM                       0x164c

#define SRC_SC_RIGHT                    0x1654
#define SRC_SC_BOTTOM                   0x165c
#endif

#define DP_CNTL                         0x16c0
#define DST_X_RIGHT_TO_LEFT		 0x00000000
#define DST_X_LEFT_TO_RIGHT		 0x00000001
#define DST_Y_BOTTOM_TO_TOP		 0x00000000
#define DST_Y_TOP_TO_BOTTOM		 0x00000002
#define DST_X_MAJOR			 0x00000000
#define DST_Y_MAJOR			 0x00000004
#define DST_X_TILE			 0x00000008
#define DST_Y_TILE			 0x00000010
#define DST_LAST_PEL			 0x00000020
#define DST_TRAIL_X_RIGHT_TO_LEFT	 0x00000000
#define DST_TRAIL_X_LEFT_TO_RIGHT	 0x00000040
#define DST_TRAP_FILL_RIGHT_TO_LEFT	 0x00000000
#define DST_TRAP_FILL_LEFT_TO_RIGHT	 0x00000080
#define DST_BRES_SIGN			 0x00000100
#define DST_HOST_BIG_ENDIAN_EN		 0x00000200
#define DST_POLYLINE_NONLAST		 0x00008000
#define DST_RASTER_STALL		 0x00010000
#define DST_POLY_EDGE			 0x00040000

#define SC_TOP_LEFT                     0x16ec
#define SC_BOTTOM_RIGHT                 0x16f0
#define SRC_SC_BOTTOM_RIGHT             0x16f4


/* CONSTANTS */
#define ENGINE_IDLE                     0x0

#endif
