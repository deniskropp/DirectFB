/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de> and
	      Alex Song <alexsong@comports.com>.

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

#ifndef __SAVAGE_STREAMS_OLD_H__
#define __SAVAGE_STREAMS_OLD_H__

#include "savage.h"
#include <core/layers.h>

extern DisplayLayerFuncs savageSecondaryFuncs;
extern DisplayLayerFuncs savagePrimaryFuncs;
extern DisplayLayerFuncs pfuncs;
extern void *pdriver_data;

/* Streams Processor Registers */
#define SAVAGE_PRIMARY_STREAM_CONTROL                       0x8180
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_CLUT            0x00000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_ARGB            0x01000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_KRGB16          0x03000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB16           0x05000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB24           0x06000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB32           0x07000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSFC_NOT_FILTERED     0x00000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSFC_REP_BOTH         0x10000000
#define SAVAGE_PRIMARY_STREAM_CONTROL_PSFC_HOR_INTERPOLATE  0x20000000

#define SAVAGE_CHROMA_KEY_CONTROL                           0x8184

#define SAVAGE_GENLOCK_CONTROL                              0x8188

#define SAVAGE_SECONDARY_STREAM_CONTROL                     0x8190
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_CbYCrY422     0x00000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YCbCr422      0x01000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YUV422        0x02000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_KRGB16        0x03000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YCbCr420      0x04000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB16         0x05000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB24         0x06000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB32         0x07000000
#define SAVAGE_SECONDARY_STREAM_CONTROL_H_DOWNSCALE4        0x00020000
#define SAVAGE_SECONDARY_STREAM_CONTROL_H_DOWNSCALE8        0x00030000
#define SAVAGE_SECONDARY_STREAM_CONTROL_H_DOWNSCALE16       0x00040000
#define SAVAGE_SECONDARY_STREAM_CONTROL_H_DOWNSCALE32       0x00050000
#define SAVAGE_SECONDARY_STREAM_CONTROL_H_DOWNSCALE64       0x00060000
#define SAVAGE_SECONDARY_STREAM_CONTROL_LUMA_ONLY_INTERPOL  0x80000000

#define SAVAGE_CHROMA_KEY_UPPER_BOUND                       0x8194

#define SAVAGE_SECONDARY_STREAM_HORIZONTAL_SCALING          0x8198

#define SAVAGE_COLOR_ADJUSTMENT                             0x819C

#define SAVAGE_BLEND_CONTROL                                0x81a0
#define SAVAGE_BLEND_CONTROL_COMP_SSTREAM                   0x00000000
#define SAVAGE_BLEND_CONTROL_COMP_PSTREAM                   0x01000000
#define SAVAGE_BLEND_CONTROL_COMP_DISSOLVE                  0x02000000
#define SAVAGE_BLEND_CONTROL_COMP_FADE                      0x03000000
#define SAVAGE_BLEND_CONTROL_COMP_ALPHA                     0x04000000
#define SAVAGE_BLEND_CONTROL_COMP_PCOLORKEY                 0x05000000
#define SAVAGE_BLEND_CONTROL_COMP_SCOLORKEY                 0x06000000
#define KP_KS(kp,ks) ((kp<<10)|(ks<<2))

#define SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_ADDRESS0         0x81c0

#define SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_ADDRESS1         0x81c4

#define SAVAGE_PRIMARY_STREAM_STRIDE                        0x81c8

#define SAVAGE_SECONDARY_STREAM_MULTIPLE_BUFFER_SUPPORT     0x81cc

#define SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS0       0x81d0

#define SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS1       0x81d4

#define SAVAGE_SECONDARY_STREAM_STRIDE                      0x81d8

#define SAVAGE_SECONDARY_STREAM_VERTICAL_SCALING            0x81e0

#define SAVAGE_SECONDARY_STREAM_VERTICAL_INITIAL_VALUE      0x81e4

#define SAVAGE_SECONDARY_STREAM_SOURCE_LINE_COUNT           0x81e8

#define SAVAGE_STREAMS_FIFO                                 0x81ec

#define SAVAGE_PRIMARY_STREAM_WINDOW_START                  0x81f0

#define SAVAGE_PRIMARY_STREAM_WINDOW_SIZE                   0x81f4

#define SAVAGE_SECONDARY_STREAM_WINDOW_START                0x81f8

#define SAVAGE_SECONDARY_STREAM_WINDOW_SIZE                 0x81fc

#define SAVAGE_PRIMARY_STREAM_FIFO_MONITOR0                 0x8200

#define SAVAGE_SECONDARY_STREAM_FIFO_MONITOR0               0x8204

#define SAVAGE_SECONDARY_STREAM_FB_CB_ADDRESS               0x8208

#define SAVAGE_SECONDARY_STREAM_FB_CR_ADDRESS               0x820C

#define SAVAGE_PRIMARY_STREAM_FIFO_MONITOR1                 0x8210

#define SAVAGE_SECONDARY_STREAM_FIFO_MONITOR1               0x8214

#define SAVAGE_SECONDARY_STREAM_CBCR_STRIDE                 0x8218

#define SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_SIZE             0x8300

#define SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_SIZE           0x8304

#define SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS2       0x8308

/* macros */
#define OS_XY(x,y) (((x+1)<<16)|(y+1))
#define OS_WH(x,y) (((x-1)<<16)|(y))

#endif /* __SAVAGE_STREAMS_OLD_H__ */
