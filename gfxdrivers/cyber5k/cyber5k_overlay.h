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

#ifndef CYBER5KFB_OVERLAY_H
#define CYBER5KFB_OVERLAY_H

#include "regs.h"

#define OVERLAY_YUV422		0	   /*captured data is YUV 422 format*/
#define OVERLAY_RGB555		1
#define OVERLAY_RGB565		2
#define OVERLAY_RGB888		3
#define OVERLAY_RGB8888		4
#define OVERLAY_RGB8		5
#define OVERLAY_RGB4444		6
#define OVERLAY_RGB8T		7

#define OVERLAY_COLORKEY	0	   /*Overlayed window is of color keying*/
#define OVERLAY_WINDOWKEY	1	   /*Overlayed window is of window keying*/

#define OVERLAY_WEAVEMODE	0
#define OVERLAY_BOBMODE		1


#define MEMORY_START_L			0xC0
#define MEMORY_START_M			0xC1
#define MEMORY_START_H			0xC2
#define MEMORY_PITCH_L			0xC3
#define MEMORY_PITCH_H			0xC4
#define DEST_RECT_LEFT_L		0xC5
#define DEST_RECT_LEFT_H		0xC6
#define DEST_RECT_RIGHT_L		0xC7
#define DEST_RECT_RIGHT_H		0xC8
#define DEST_RECT_TOP_L			0xC9
#define DEST_RECT_TOP_H			0xCA
#define DEST_RECT_BOTTOM_L		0xCB
#define DEST_RECT_BOTTOM_H		0xCC
#define MEMORY_OFFSET_PHASE		0xCD
#define COLOR_CMP_RED			0xCE
#define COLOR_CMP_GREEN			0xCF
#define COLOR_CMP_BLUE			0xD0
#define DDA_X_INIT_L			0xD1
#define DDA_X_INIT_H			0xD2
#define DDA_X_INC_L				0xD3
#define DDA_X_INC_H				0xD4
#define DDA_Y_INIT_L			0xD5
#define DDA_Y_INIT_H			0xD6
#define DDA_Y_INC_L				0xD7
#define DDA_Y_INC_H				0xD8
#define FIFO_TIMING_CTL_L		0xD9
#define FIFO_TIMING_CTL_H		0xDA
#define VIDEO_FORMAT			0xDB
#define DISP_CTL_I				0xDC
#define FIFO_CTL_I				0xDD
#define MISC_CTL_I				0xDE

void cyber_cleanup_overlay(void);
void cyber_init_overlay(void);
void cyber_enable_overlay(int enable);
void cyber_change_overlay_fifo(void);
void cyber_set_overlay_format(int format);
void cyber_set_overlay_mode(int mode);
void cyber_set_overlay_srcaddr(int addr, int x, int y, int width, int pitch);
void cyber_set_overlay_window(int left, int top, int right, int bottom);
void cyber_set_overlay_scale( unsigned char bEnableBob, int wSrcXExt, int wDstXExt, int wSrcYExt, int wDstYExt );

#endif /* CYBER5KFB_OVERLAY_H */
