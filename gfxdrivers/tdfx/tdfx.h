/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <dfb_types.h>


#define S12_4(val)    (((u32)((s32)((val) << 4))) & 0xffff)
#define S12_4_(a,b)   (((u32)((s32)(((a) << 4) + (b)))) & 0xffff)


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
    u32 status;
    u32 intrCtrl;

    u32 vertexAx;
    u32 vertexAy;
    u32 vertexBx;
    u32 vertexBy;
    u32 vertexCx;
    u32 vertexCy;

    s32 startR;
    s32 startG;
    s32 startB;
    s32 startZ;
    s32 startA;
    s32 startS;
    s32 startT;
    s32 startW;

    s32 dRdX;
    s32 dGdX;
    s32 dBdX;
    s32 dZdX;
    s32 dAdX;
    s32 dSdX;
    s32 dTdX;
    s32 dWdX;

    s32 dRdY;
    s32 dGdY;
    s32 dBdY;
    s32 dZdY;
    s32 dAdY;
    s32 dSdY;
    s32 dTdY;
    s32 dWdY;

    u32 triangleCMD;
    u32 reserved0;

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

    u32 ftriangleCMD;
    u32 fbzColorPath;
    u32 fogMode;
    u32 alphaMode;
    u32 fbzMode;
    u32 lfbMode;
    u32 clipLeftRight;
    u32 clipTopBottom;

    u32 nopCMD;
    u32 fastfillCMD;
    u32 swapbufferCMD;
    u32 fogColor;
    u32 zaColor;
    u32 chromaKey;
    u32 chromaRange;
    u32 userIntrCMD;
    u32 stipple;
    u32 color0;
    u32 color1;

    u32 fbiPixelsIn;
    u32 fbiChromaFail;
    u32 fbiZfuncFail;
    u32 fbiAfuncFail;
    u32 fbiPixelsOut;

    u32 fogTable[32];

    u32 reserved1[3];

    u32 colBufferAddr;
    u32 colBufferStride;
    u32 auxBufferAddr;
    u32 auxBufferStride;

    u32 reserved2;

    u32 clipLeftRight1;
    u32 clipTopBottom1;

    u32 reserved3[17];
    u32 swapPending;
    u32 leftOverlayBuf;
    u32 rightOverlayBuf;
    u32 fbiSwapHistory;
    u32 fbiTrianglesOut;
    u32 sSetupMode;
    float sVx;
    float sVy;
    u32 sARGB;
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
    u32 sDrawTriCMD;
    u32 sBeginTriCMD;

    u32 reserved[22];
    u32 textureMode;
    u32 tLOD;
    u32 tDetail;
    u32 texBaseAddr;
    u32 texBaseAddr1;
    u32 texBaseAddr2;
    u32 texBaseAddr3_8;
    u32 texStride;
    u32 trexInit1;

    u32 nccTable0[12];
    u32 nccTable1[12];

    u32 reserved4[31];
} Voodoo3D;


typedef volatile struct {
     u32 status;
     u32 intCtrl;
     u32 clip0Min;
     u32 clip0Max;
     u32 dstBaseAddr;
     u32 dstFormat;
     u32 srcColorkeyMin;
     u32 srcColorkeyMax;
     u32 dstColorkeyMin;
     u32 dstColorkeyMax;
     u32 bresError0;
     u32 bresError1;
     u32 rop;
     u32 srcBaseAddr;
     u32 commandExtra;
     u32 lineStipple;
     u32 lineStyle;
     u32 pattern0Alias;
     u32 pattern1Alias;
     u32 clip1Min;
     u32 clip1Max;
     u32 srcFormat;
     u32 srcSize;
     u32 srcXY;
     u32 colorBack;
     u32 colorFore;
     u32 dstSize;
     u32 dstXY;
     u32 command;

     u32 reserved[3];

     u32 launchArea[32];

     u32 colorPattern[64];
} Voodoo2D;

#endif
