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

#ifndef __SAVAGE_H__
#define __SAVAGE_H__

#include <core/gfxcard.h>

#define SRC_BASE             0xa4d4
#define DEST_BASE            0xa4d8
#define CLIP_L_R             0xa4dc
#define CLIP_T_B             0xa4e0
#define DEST_SRC_STR         0xa4e4
#define MONO_PAT_0           0xa4e8
#define MONO_PAT_1           0xa4ec


#define BCI_BUFFER_OFFSET    0x10000

#define MAXFIFO              0x7f00

typedef struct {
     unsigned int accel_id;

     unsigned int waitfifo_sum;
     unsigned int waitfifo_calls;
     unsigned int waitidle_calls;
     unsigned int fifo_waitcycles;
     unsigned int idle_waitcycles;
     unsigned int fifo_cache_hits;

     unsigned int fifo_space;

     unsigned int bci_ptr;
} SavageDeviceData;

typedef struct {
     volatile __u8  *mmio_base;
     volatile __u32 *bci_base;
} SavageDriverData;


#if 0
typedef struct S3SAVDRAWCTRLtag {
 hwUI32 uED:1; // Enable Dithering
 hwUI32 uUVO:1; // UV Offset Enable (add 0.5 to u and v 
 hwUI32 uBCM:2; // Backface Cull Mode
                // 00 - reserved
                // 01 - disable culling
                // 10 - cull clockwise
                // 11 - cull counterclockwise
 hwUI32 uTVC:1; // vertex counter reset 1 - reset it
 hwUI32 uSM:1; // Shade Mode 0 - gouraud, 1 - flat (color at vertex 0)
 hwUI32 uESS:1; // Enable Specular
 hwUI32 uDABM:3; // Destination Alpha Blend Mode look below 
 hwUI32 uSABM:3; // Source Alpha Blend Mode look below
 hwUI32 uReserved1:1;
 hwUI32 uATC:3; // Alpha Test Compare look below
 hwUI32 uEAT:1; // Enable Alpha Test
 hwUI32 uAlphaRef:8; // Alpha Reference Value 
 hwUI32 uTBC:3; // Texture Blending Control (look below)
 hwUI32 uFDW:1; // Flush Destination Writes 
 hwUI32 uFZW:1; // Flush Z Writes
 hwUI32 uIM:1; // Interpolaton Mode 1 - linear color and fog interpolation
} S3SAVDRAWCTRL, *PS3SAVDRAWCTRL;
#endif

#define DRAWCTRL_ENABLE_DITHERING            0x00000001
#define DRAWCTRL_ENABLE_UV_OFFSET            0x00000002
#define DRAWCTRL_CULL_REVERSED               0x00000000
#define DRAWCTRL_CULL_NONE                   0x00000004
#define DRAWCTRL_CULL_CLOCKWISE              0x00000008
#define DRAWCTRL_CULL_COUNTERCLOCKWISE       0x0000000C
#define DRAWCTRL_VERTEX_COUNTER_RESET        0x00000010
#define DRAWCTRL_SHADE_GOURAUD               0x00000000
#define DRAWCTRL_SHADE_FLAT                  0x00000020
#define DRAWCTRL_ENABLE_SPECULAR             0x00000040
#define DRAWCTRL_ENABLE_ALPHA_TEST           0x00020000
#define DRAWCTRL_FLUSH_DESTINATION_WRITES    0x20000000
#define DRAWCTRL_FLUSH_Z_WRITES              0x40000000
#define DRAWCTRL_COLOR_AND_FOG_INTERPOLATION 0x80000000

#define DRAWCTRL_DABM_ZERO                   (0 << 7)
#define DRAWCTRL_DABM_ONE                    (1 << 7)
#define DRAWCTRL_DABM_SOURCE_COLOR           (2 << 7)
#define DRAWCTRL_DABM_ONE_MINUS_SOURCE_COLOR (3 << 7)
#define DRAWCTRL_DABM_SOURCE_ALPHA           (4 << 7)
#define DRAWCTRL_DABM_ONE_MINUS_SOURCE_ALPHA (5 << 7)
#define DRAWCTRL_DABM_6                      (6 << 7)
#define DRAWCTRL_DABM_7                      (7 << 7)

#define DRAWCTRL_SABM_ZERO                   (0 << 10)
#define DRAWCTRL_SABM_ONE                    (1 << 10)
#define DRAWCTRL_SABM_DEST_COLOR             (2 << 10)
#define DRAWCTRL_SABM_ONE_MINUS_DEST_COLOR   (3 << 10)
#define DRAWCTRL_SABM_SOURCE_ALPHA           (4 << 10)
#define DRAWCTRL_SABM_ONE_MINUS_SOURCE_ALPHA (5 << 10)
#define DRAWCTRL_SABM_6                      (6 << 10)
#define DRAWCTRL_SABM_7                      (7 << 10)

#define DRAWCTRL_ATC_NEVER                   (0 << 14)
#define DRAWCTRL_ATC_LESS                    (1 << 14)
#define DRAWCTRL_ATC_EQUAL                   (2 << 14)
#define DRAWCTRL_ATC_LEQUAL                  (3 << 14)
#define DRAWCTRL_ATC_GREATER                 (4 << 14)
#define DRAWCTRL_ATC_NOTEQUAL                (5 << 14)
#define DRAWCTRL_ATC_GEQUAL                  (6 << 14)
#define DRAWCTRL_ATC_ALWAYS                  (7 << 14)

#define DRAWCTRL_TBC_DECAL                   (0 << 26)
#define DRAWCTRL_TBC_MODULATE                (1 << 26)
#define DRAWCTRL_TBC_DECALALPHA              (2 << 26)
#define DRAWCTRL_TBC_MODULATEALPHA           (3 << 26)
#define DRAWCTRL_TBC_4                       (4 << 26)
#define DRAWCTRL_TBC_5                       (5 << 26)
#define DRAWCTRL_TBC_COPY                    (6 << 26)
#define DRAWCTRL_TBC_7                       (7 << 26)


#endif
