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

#ifndef __TDFX_H__
#define __TDFX_H__

#include <asm/types.h>


#define S12_4(val)    (((__u32)((__s32)((val) << 4))) & 0xffff)
#define S12_4_(a,b)   (((__u32)((__s32)(((a) << 4) + (b)))) & 0xffff)


#define TDFX_LFBMODE_RGB565                   0
#define TDFX_LFBMODE_RGB555                   1
#define TDFX_LFBMODE_RGB0888                  4
#define TDFX_LFBMODE_ARGB8888                 5
#define TDFX_LFBMODE_PIXEL_PIPELINE_ENABLE   (1 << 8)

#define TDFX_CLIP_ENABLE                     (1 << 31)


#define TDFX_ALPHAMODE_BLEND_ENABLE          (1 << 4)

#define TDFX_FBZCOLORPATH_RGBSELECT_COLOR1    2
#define TDFX_FBZCOLORPATH_ASELECT_COLOR1     (2 << 2)

typedef volatile struct {
    __u32 status;
    __u32 intrCtrl;

    __u32 vertexAx;
    __u32 vertexAy;
    __u32 vertexBx;
    __u32 vertexBy;
    __u32 vertexCx;
    __u32 vertexCy;

    __s32 startR;
    __s32 startG;
    __s32 startB;
    __s32 startZ;
    __s32 startA;
    __s32 startS;
    __s32 startT;
    __s32 startW;

    __s32 dRdX;
    __s32 dGdX;
    __s32 dBdX;
    __s32 dZdX;
    __s32 dAdX;
    __s32 dSdX;
    __s32 dTdX;
    __s32 dWdX;

    __s32 dRdY;
    __s32 dGdY;
    __s32 dBdY;
    __s32 dZdY;
    __s32 dAdY;
    __s32 dSdY;
    __s32 dTdY;
    __s32 dWdY;

    __u32 triangleCMD;
    __u32 reserved0;

    float fvertexAx;
    float fvertexAy;
    float fvertexBx;
    float fvertexBy;
    float fvertexCx;
    float fvertexCy;

    float fstartR;
    float fstartG;
    float fstartB;
    float fstartZ;
    float fstartA;
    float fstartS;
    float fstartT;
    float fstartW;

    float fdRdX;
    float fdGdX;
    float fdBdX;
    float fdZdX;
    float fdAdX;
    float fdSdX;
    float fdTdX;
    float fdWdX;

    float fdRdY;
    float fdGdY;
    float fdBdY;
    float fdZdY;
    float fdAdY;
    float fdSdY;
    float fdTdY;
    float fdWdY;

    __u32 ftriangleCMD;
    __u32 fbzColorPath;
    __u32 fogMode;
    __u32 alphaMode;
    __u32 fbzMode;
    __u32 lfbMode;
    __u32 clipLeftRight;
    __u32 clipTopBottom;

    __u32 nopCMD;
    __u32 fastfillCMD;
    __u32 swapbufferCMD;
    __u32 fogColor;
    __u32 zaColor;
    __u32 chromaKey;
    __u32 chromaRange;
    __u32 userIntrCMD;
    __u32 stipple;
    __u32 color0;
    __u32 color1;

    __u32 fbiPixelsIn;
    __u32 fbiChromaFail;
    __u32 fbiZfuncFail;
    __u32 fbiAfuncFail;
    __u32 fbiPixelsOut;

    __u32 fogTable[32];

    __u32 reserved1[3];

    __u32 colBufferAddr;
    __u32 colBufferStride;
    __u32 auxBufferAddr;
    __u32 auxBufferStride;

    __u32 reserved2;

    __u32 clipLeftRight1;
    __u32 clipTopBottom1;

    __u32 reserved3[17];
    __u32 swapPending;
    __u32 leftOverlayBuf;
    __u32 rightOverlayBuf;
    __u32 fbiSwapHistory;
    __u32 fbiTrianglesOut;
    __u32 sSetupMode;
    float sVx;
    float sVy;
    __u32 sARGB;
    float sRed;
    float sGreen;
    float sBlue;
    float sAlpha;
    float sVz;
    float sWb;
    float sWtmu0;
    float sS_W0;
    float sT_W0;
    float sWtmu1;
    float sS_Wtmu1;
    float sT_Wtmu1;
    __u32 sDrawTriCMD;
    __u32 sBeginTriCMD;

    __u32 reserved[22];
    __u32 textureMode;
    __u32 tLOD;
    __u32 tDetail;
    __u32 texBaseAddr;
    __u32 texBaseAddr1;
    __u32 texBaseAddr2;
    __u32 texBaseAddr3_8;
    __u32 texStride;
    __u32 trexInit1;

    __u32 nccTable0[12];
    __u32 nccTable1[12];

    __u32 reserved4[31];
} Voodoo3D;


typedef volatile struct {
     __u32 status;
     __u32 intCtrl;
     __u32 clip0Min;
     __u32 clip0Max;
     __u32 dstBaseAddr;
     __u32 dstFormat;
     __u32 srcColorkeyMin;
     __u32 srcColorkeyMax;
     __u32 dstColorkeyMin;
     __u32 dstColorkeyMax;
     __u32 bresError0;
     __u32 bresError1;
     __u32 rop;
     __u32 srcBaseAddr;
     __u32 commandExtra;
     __u32 lineStipple;
     __u32 lineStyle;
     __u32 pattern0Alias;
     __u32 pattern1Alias;
     __u32 clip1Min;
     __u32 clip1Max;
     __u32 srcFormat;
     __u32 srcSize;
     __u32 srcXY;
     __u32 colorBack;
     __u32 colorFore;
     __u32 dstSize;
     __u32 dstXY;
     __u32 command;

     __u32 reserved[3];

     __u32 launchArea[32];

     __u32 colorPattern[64];
} Voodoo2D;

#endif
