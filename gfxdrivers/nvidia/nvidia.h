/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan82@cheapnet.it>.

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

#ifndef __NVIDIA_H__
#define __NVIDIA_H__

#include <dfb_types.h>

#include <core/state.h>
#include <core/screens.h>
#include <core/layers.h>

/***************************************************************************\
*                                                                           *
*                             FIFO registers.                               *
*                                                                           *
\***************************************************************************/

/*
 * 2D surfaces
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 Reserved00[0x01E];
     __u32 SetContextDmaNotifies;  /* 0180-0183 */
     __u32 SetContextDmaSource;    /* 0184-0187 */
     __u32 SetContextDmaDestin;    /* 0188-018B */
     __u32 Reserved01[0x05D];
     __u32 Format;                 /* 0300-0303 */
     __u32 Pitch;                  /* 0304-0307 */
     __u32 SourceOffset;           /* 0308-030B */
     __u32 DestOffset;             /* 030C-030F */
     __u32 Reserved02[0x73C];
} NVSurfaces2D;

/*
 * 3D surfaces
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 Reserved00[0x01E];
     __u32 SetContextDmaNotify;    /* 0180-0183 */
     __u32 SetContextDmaColor;     /* 0184-0187 */
     __u32 SetContextDmaZeta;      /* 0188-018B */
     __u32 Reserved01[0x05B];
     __u32 ClipHorizontal;         /* 02F8-02FB */          
     __u32 ClipVertical;           /* 02FC-02FF */
     __u32 Format;                 /* 0300-0303 */
     __u32 ClipSize;               /* 0304-0307 */
     __u32 Pitch;                  /* 0308-030B */
     __u32 RenderOffset;           /* 030C-030F */
     __u32 DepthOffset;            /* 0310-0313 */
     __u32 Reserved02[0x73B];
} NVSurfaces3D;

/*
 * Scissor clip rectangle
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 Reserved00[0x01E];
     __u32 SetContextDmaNotify;    /* 0180-0183 */
     __u32 SetContextDmaImage;     /* 0184-0187 */
     __u32 Reserved01[0x05E];
     __u32 TopLeft;                /* 0300-0303 */
     __u32 WidthHeight;            /* 0304-0307 */
     __u32 Reserved02[0x73E];
} NVClip;

/*
 * Fixed alpha value (Alphablend)
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 Reserved00[0x01E];
     __u32 SetContextDmaNotify;    /* 0180-0183 */
     __u32 Reserved01[0x05F];
     __u32 SetBeta1D31;            /* 0300-0303 */
     __u32 Reserved02[0x73F];
} NVBeta1;

/*
 * ARGB color (Colorizing)
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 Reserved00[0x01E];
     __u32 SetContextDmaNotify;    /* 0180-0183 */
     __u32 Reserved01[0x05F];
     __u32 SetBetaFactor;          /* 0300-0303 */
     __u32 Reserved02[0x73F];
} NVBeta4;

/*
 * 2D solid rectangle
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 Reserved00[0x01E];
     __u32 SetContextDmaNotifies;  /* 0180-0183 */
     __u32 SetContextClip;         /* 0184-0187 */
     __u32 SetContextPattern;      /* 0188-018B */
     __u32 SetContextRop;          /* 018C-018F */
     __u32 SetContextBeta1;        /* 0190-0193 */
     __u32 SetContextSurface;      /* 0194-0197 */
     __u32 Reserved01[0x059];
     __u32 SetOperation;           /* 02FC-02FF */
     __u32 SetColorFormat;         /* 0300-0303 */
     __u32 Color;                  /* 0304-0307 */
     __u32 Reserved02[0x03E];
     __u32 TopLeft;                /* 0400-0403 */
     __u32 WidthHeight;            /* 0404-0407 */
     __u32 Reserved03[0x6FE];
} NVRectangle;

/*
 * 2D solid triangle
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 Reserved00[0x01E];
     __u32 SetContextDmaNotifies;  /* 0180-0183 */
     __u32 SetContextClip;         /* 0184-0187 */
     __u32 SetContextPattern;      /* 0188-018B */
     __u32 SetContextRop;          /* 018C-018F */
     __u32 SetContextBeta1;        /* 0190-0193 */
     __u32 SetContextSurface;      /* 0194-0197 */
     __u32 Reserved01[0x059];
     __u32 SetOperation;           /* 02FC-02FF */
     __u32 SetColorFormat;         /* 0300-0303 */
     __u32 Color;                  /* 0304-0307 */
     __u32 Reserved02[0x002];
     __u32 TrianglePoint0;         /* 0310-0313 */
     __u32 TrianglePoint1;         /* 0314-0317 */
     __u32 TrianglePoint2;         /* 0318-031B */
     __u32 Reserved03[0x001];
     __s32 Triangle32Point0X;      /* 0320-0323 */
     __s32 Triangle32Point0Y;      /* 0324-0327 */
     __s32 Triangle32Point1X;      /* 0328-032B */
     __s32 Triangle32Point1Y;      /* 032C-032F */
     __s32 Triangle32Point2X;      /* 0330-0333 */
     __s32 Triangle32Point2Y;      /* 0334-0337 */
     __u32 Reserved04[0x032];
     __u32 Trimesh[32];            /* 0400-047F */
     struct {                      /* 0480-     */
          __s32 x;                 /*    0-   3 */
          __s32 y;                 /*    4-   7 */
     } Trimesh32[16];              /*     -04FF */
     struct {                      /* 0500-     */
          __u32 color;             /*    0-   3 */
          __u32 point0;            /*    4-   7 */
          __u32 point1;            /*    8-   B */
          __u32 point2;            /*    C-   F */
     } ColorTriangle[8];           /*     -057F */
     struct {                      /* 0580-     */
          __u32 color;             /*    0-   3 */
          __u32 point;             /*    4-   7 */
     } ColorTrimesh[16];           /*     -05FF */
     __u32 Reserved05[0x680];
} NVTriangle;

/*
 * 2D solid line
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 Reserved00[0x01E];
     __u32 SetContextDmaNotify;    /* 0180-0183 */
     __u32 SetContextClip;         /* 0184-0187 */
     __u32 SetContextPattern;      /* 0188-018B */
     __u32 SetContextRop;          /* 018C-018F */
     __u32 SetContextBeta1;        /* 0190-0193 */
     __u32 SetContextSurface;      /* 0194-0197 */
     __u32 Reserved01[0x059];
     __u32 SetOperation;           /* 02FC-02FF */
     __u32 SetColorFormat;         /* 0300-0303 */
     __u32 Color;                  /* 0304-0307 */
     __u32 Reserved02[0x03E];
     struct {                      /* 0400-     */
          __u32 point0;            /*    0-   3 */
          __u32 point1;            /*    4-   7 */
     } Lin[16];                    /*     -047F */
     struct {                      /* 0480-     */
          __u32 point0X;           /*    0-   3 */
          __u32 point0Y;           /*    4-   7 */
          __u32 point1X;           /*    8-   B */
          __u32 point1Y;           /*    C-   F */
     } Lin32[8];                   /*     -04FF */
     __u32 PolyLin[32];            /* 0500-057F */
     struct {                      /* 0580-     */
          __u32 x;                 /*    0-   3 */
          __u32 y;                 /*    4-   7 */
     } PolyLin32[16];              /*     -05FF */
     struct {                      /* 0600-     */
          __u32 color;             /*    0-   3 */
          __u32 point;             /*    4-   7 */
     } ColorPolyLin[16];           /*     -067F */
     __u32 Reserved03[0x660];
} NVLine;

/*
 * 2D screen-screen BLT
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 WaitForIdle;            /* 0108-010B (NV_09F_WAIT_FOR_IDLE) */
     __u32 WaitForSync;            /* 010C-010F (NV_09F_WAIT_FOR_CRTC) */
     __u32 Reserved00[0x01C];
     __u32 SetContextDmaNotify;    /* 0180-0183 */
     __u32 SetContextColorKey;     /* 0184-0187 */
     __u32 SetContextClip;         /* 0188-018B */
     __u32 SetContextPattern;      /* 018C-018F */
     __u32 SetContextRop;          /* 0190-0193 */
     __u32 SetContextBeta1;        /* 0194-0197 */
     __u32 SetContextBeta4;        /* 0198-019B */
     __u32 SetContextSurface;      /* 019C-019F */
     __u32 Reserved01[0x057];
     __u32 SetOperation;           /* 02FC-02FF */
     __u32 TopLeftSrc;             /* 0300-0303 */
     __u32 TopLeftDst;             /* 0304-0307 */
     __u32 WidthHeight;            /* 0308-030B */
     __u32 Reserved02[0x73D];
} NVScreenBlt;

/*
 * 2D scaled image BLT
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 Reserved00[0x01E];
     __u32 SetContextDmaNotifies;  /* 0180-0183 */
     __u32 SetContextDmaImage;     /* 0184-0187 */
     __u32 SetContextPattern;      /* 0188-018B */
     __u32 SetContextRop;          /* 018C-018F */
     __u32 SetContextBeta1;        /* 0190-0193 */
     __u32 SetContextBeta4;        /* 0194-0197 */
     __u32 SetContextSurface;      /* 0198-019C */
     __u32 Reserved01[0x058];
     __u32 SetColorConversion;     /* 02FC-02FF */
     __u32 SetColorFormat;         /* 0300-0303 */
     __u32 SetOperation;           /* 0304-0307 */
     __u32 ClipPoint;              /* 0308-030B */
     __u32 ClipSize;               /* 030C-030F */
     __u32 ImageOutPoint;          /* 0310-0313 */
     __u32 ImageOutSize;           /* 0314-0317 */
     __u32 DuDx;                   /* 0318-031B */
     __u32 DvDy;                   /* 031C-031F */
     __u32 Reserved02[0x038];
     __u32 ImageInSize;            /* 0400-0403 */
     __u32 ImageInFormat;          /* 0404-0407 */
     __u32 ImageInOffset;          /* 0408-040B */
     __u32 ImageInPoint;           /* 040C-040F */
     __u32 Reserved03[0x6FC];
} NVScaledImage;

/*
 * 3D textured, Z buffered triangle
 */
typedef volatile struct {
     __u32 NoOperation;            /* 0100-0103 */
     __u32 Notify;                 /* 0104-0107 */
     __u32 Reserved00[0x01E];
     __u32 SetContextDmaNotify;    /* 0180-0183 */
     __u32 SetContextDmaA;         /* 0184-0187 */
     __u32 SetContextDmaB;         /* 0188-018B */
     __u32 SetContextSurfaces;     /* 018C-018F */
     __u32 Reserved01[0x05C];
     __u32 ColorKey;               /* 0300-0303 */
     __u32 TextureOffset;          /* 0304-0307 */
     __u32 TextureFormat;          /* 0308-030B */
     __u32 TextureFilter;          /* 030C-030F */
     __u32 Blend;                  /* 0310-0313 */
     __u32 Control;                /* 0314-0317 */
     __u32 FogColor;               /* 0318-031B */
     __u32 Reserved02[0x039];
     struct {                      /* 0400-     */
          float sx;                /*   00-  03 */
          float sy;                /*   04-  07 */
          float sz;                /*   08-  0B */
          float rhw;               /*   0C-  0F */
          __u32 color;             /*   10-  13 */
          __u32 specular;          /*   14-  17 */
          float ts;                /*   18-  1B */
          float tt;                /*   1C-  1F */
     } Tlvertex[16];               /*     -05FF */
     __u32 DrawPrimitives[64];     /* 0600-063F */
     __u32 Reserved03[0x640];
} NVTexturedTriangleDx5;


typedef volatile struct {
     __u32 SetObject;              /* 0000-0003 */
     __u32 Reserved00[0x003];
#ifdef WORDS_BIGENDIAN
     __u32 Free;                   /* 0010-0013 */
#else
     __u16 Free;                   /* 0010-0011 */
     __u16 Nop;                    /* 0012-0013 */
#endif
     __u32 Reserved01[0x00B];
     __u32 DmaPut;                 /* 0040-0043 */
     __u32 DmaGet;                 /* 0044-0047 */
     __u32 Reserved02[0x02E];
     union {
          NVSurfaces2D          Surfaces2D;
          NVSurfaces3D          Surfaces3D;
          NVClip                Clip;
          NVBeta1               Beta1;
          NVBeta4               Beta4;
          NVRectangle           Rectangle;
          NVTriangle            Triangle;
          NVLine                Line;
          NVScreenBlt           Blt;
          NVScaledImage         ScaledImage;
          NVTexturedTriangleDx5 TexTriangle;
     } o;
} NVFifoSubChannel;

typedef volatile struct {
     NVFifoSubChannel sub[8];
} NVFifoChannel;


/*
 * Objects IDs;
 * used for RAMHT offset calculation
 */
enum {
     OBJ_DMA          = 0x00800000,
     OBJ_SURFACES2D   = 0x00800001,
     OBJ_CLIP         = 0x00800002,
     OBJ_BETA1        = 0x00800003,
     OBJ_BETA4        = 0x00800004,
     OBJ_RECTANGLE    = 0x00800010,
     OBJ_TRIANGLE     = 0x00800011,
     OBJ_LINE         = 0x00800012,
     OBJ_SCREENBLT    = 0x00800013,
     OBJ_SCALEDIMAGE  = 0x00800014,
     OBJ_TEXTRIANGLE  = 0x00800015,
     OBJ_SURFACES3D   = 0x00800016
};

/*
 * Objects addresses into context table [PRAMIN + (address)*16]
 */
enum {
     ADDR_DMA         = 0x1160,
     ADDR_SURFACES2D  = 0x1162,
     ADDR_CLIP        = 0x1163,
     ADDR_BETA1       = 0x1164,
     ADDR_BETA4       = 0x1165,
     ADDR_RECTANGLE   = 0x1166,
     ADDR_TRIANGLE    = 0x1167,
     ADDR_LINE        = 0x1168,
     ADDR_SCREENBLT   = 0x1169,
     ADDR_SCALEDIMAGE = 0x116A,
     ADDR_TEXTRIANGLE = 0x116B,
     ADDR_SURFACES3D  = 0x116C
};



typedef struct {
     StateModificationFlags  reloaded;
     
     DFBSurfacePixelFormat   dst_format;
     __u32                   dst_offset;
     __u32                   dst_pitch;

     DFBSurfacePixelFormat   src_format;
     __u32                   src_offset;
     __u32                   src_pitch;
     __u32                   src_width;
     __u32                   src_height;

     __u32                   depth_offset;
     __u32                   depth_pitch;

     __u32                   color;
     __u8                    alpha;
     __u32                   operation;
     bool                    argb_src;

     /* 3D stuff */
     bool                    enabled_3d; /* 3d engine enabled     */
     __u32                   tex_offset; /* texture buffer offset */
     __u32                   col_offset; /* color buffer offset   */
     __u32                   color3d;
     
     struct {
          __u32              colorkey;
          __u32              offset;
          __u32              format;
          __u32              filter;
          __u32              blend;
          __u32              control;
          __u32              fog;
     } state3d;

     /* for fifo/performance monitoring */
     unsigned int            fifo_space;
     unsigned int            waitfifo_sum;
     unsigned int            waitfifo_calls;
     unsigned int            fifo_waitcycles;
     unsigned int            idle_waitcycles;
     unsigned int            fifo_cache_hits;
} NVidiaDeviceData;


enum {
     NV_ARCH_04 = 0x04,
     NV_ARCH_05 = 0x05,
     NV_ARCH_10 = 0x10,
     NV_ARCH_20 = 0x20,
     NV_ARCH_30 = 0x30
};

typedef struct {
     GraphicsDevice         *device;

     __u32                   chip;
     __u32                   arch; /* NV_ARCH_* */
     
     __u32                   fb_offset;
     __u32                   fb_mask;

     volatile __u8          *mmio_base;
     volatile __u32         *PVIDEO;
     volatile __u32         *PFB;
     volatile __u32         *PGRAPH;
     volatile __u32         *PCRTC;
     volatile __u8          *PCIO;
     volatile __u8          *PVIO;
     volatile __u32         *PRAMIN;
     volatile __u32         *PRAMHT;

     NVFifoChannel          *Fifo;
     NVSurfaces2D           *Surfaces2D;
     NVSurfaces3D           *Surfaces3D;
     NVClip                 *Clip;
     NVBeta1                *Beta1;
     NVBeta4                *Beta4;
     NVRectangle            *Rectangle;
     NVTriangle             *Triangle;
     NVLine                 *Line;
     NVScreenBlt            *Blt;
     NVScaledImage          *ScaledImage;
     NVTexturedTriangleDx5  *TexTriangle;
} NVidiaDriverData;


extern ScreenFuncs        nvidiaPrimaryScreenFuncs;

extern DisplayLayerFuncs  nvidiaPrimaryLayerFuncs;
extern DisplayLayerFuncs  nvidiaOldPrimaryLayerFuncs;
extern void              *nvidiaOldPrimaryLayerDriverData;

extern DisplayLayerFuncs  nvidiaOverlayFuncs;


#endif /* __NVIDIA_H__ */

