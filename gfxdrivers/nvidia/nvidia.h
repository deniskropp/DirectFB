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

#ifndef __NVIDIA_H__
#define __NVIDIA_H__

#include <asm/types.h>

/***************************************************************************\
*                                                                           *
*                             FIFO registers.                               *
*                                                                           *
\***************************************************************************/

/*
 * Raster OPeration. Windows style ROP3.
 */
typedef struct
{
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop;
    __u32 reserved01[0x0BB];
    __u32 Rop3;
} RivaRop;
/*
 * 8X8 Monochrome pattern.
 */
typedef struct
{
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop;
    __u32 reserved01[0x0BD];
    __u32 Shape;
    __u32 reserved03[0x001];
    __u32 Color0;
    __u32 Color1;
    __u32 Monochrome[2];
} RivaPattern;
/*
 * Scissor clip rectangle.
 */
typedef struct
{
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop;
    __u32 reserved01[0x0BB];
    __u32 TopLeft;
    __u32 WidthHeight;
} RivaClip;
/*
 * 2D filled rectangle.
 */
typedef struct
{
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop[1];
    __u32 reserved01[0x03B];

    __u32 NoOperation;             /* ignored                          0100-0103*/
    __u32 Notify;                  /* NV001E_NOTIFY_*                  0104-0107*/
    __u32 Reserved00[0x01e];
    __u32 SetContextDmaNotifies;   /* NV01_CONTEXT_DMA                 0180-0183*/
    __u32 SetContextClipRectangle; /* NV01_CONTEXT_CLIP_RECTANGLE      0184-0187*/
    __u32 SetContextPattern;       /* NV01_CONTEXT_PATTERN             0188-018b*/
    __u32 SetContextRop;           /* NV03_CONTEXT_ROP                 018c-018f*/
    __u32 SetContextBeta1;         /* NV01_CONTEXT_BETA1               0190-0193*/
    __u32 SetContextSurface;       /* NV03_CONTEXT_SURFACES_2D         0194-0197*/
    __u32 Reserved01[0x059];
    __u32 SetOperation;            /* NV001E_SET_OPERATION_*           02fc-02ff*/
    __u32 SetColorFormat;          /* NV001E_SET_COLOR_FORMAT_*        0300-0303*/

    __u32 Color;                   /* source color                     0304-0307*/

    __u32 reserved03[0x03E];
    __u32 TopLeft;
    __u32 WidthHeight;
} RivaRectangle;

typedef struct {
     __u32 reserved00[4];
     __u16 FifoFree;
     __u16 Nop[1];
     __u32 reserved01[0x03B];

     __u32 NoOperation;             /* ignored                          0100-0103*/
     __u32 Notify;                  /* NV001D_NOTIFY_*                  0104-0107*/
     __u32 Reserved00[0x01e];
     __u32 SetContextDmaNotifies;   /* NV01_CONTEXT_DMA                 0180-0183*/
     __u32 SetContextClipRectangle; /* NV01_CONTEXT_CLIP_RECTANGLE      0184-0187*/
     __u32 SetContextPattern;       /* NV01_CONTEXT_PATTERN             0188-018b*/
     __u32 SetContextRop;           /* NV03_CONTEXT_ROP                 018c-018f*/
     __u32 SetContextBeta1;         /* NV01_CONTEXT_BETA1               0190-0193*/
     __u32 SetContextSurface;       /* NV03_CONTEXT_SURFACES_2D         0194-0197*/
     __u32 Reserved01[0x059];
     __u32 SetOperation;            /* NV001D_SET_OPERATION_*           02fc-02ff*/
     __u32 SetColorFormat;          /* NV001D_SET_COLOR_FORMAT_*        0300-0303*/
     __u32 Color;                   /* source color                     0304-0307*/
     __u32 Reserved02[0x002];
     __u32 TrianglePoint0;          /* y_x S16_S16 in pixels            0310-0313*/
     __u32 TrianglePoint1;          /* y_x S16_S16 in pixels            0314-0317*/
     __u32 TrianglePoint2;          /* y_x S16_S16 in pixels            0318-031b*/
     __u32 Reserved03[0x001];
     __s32 Triangle32Point0X;       /* in pixels, 0 at left             0320-0323*/
     __s32 Triangle32Point0Y;       /* in pixels, 0 at top              0324-0327*/
     __s32 Triangle32Point1X;       /* in pixels, 0 at left             0328-032b*/
     __s32 Triangle32Point1Y;       /* in pixels, 0 at top              032c-032f*/
     __s32 Triangle32Point2X;       /* in pixels, 0 at left             0330-0333*/
     __s32 Triangle32Point2Y;       /* in pixels, 0 at top              0334-0337*/
     __u32 Reserved04[0x032];
     __u32 Trimesh[32];             /* y_x S16_S16 in pixels            0400-047f*/
     struct {                       /* start aliased methods in array   0480-    */
          __s32 x;                      /* in pixels, 0 at left                0-   3*/
          __s32 y;                      /* in pixels, 0 at top                 4-   7*/
     } Trimesh32[16];               /* end of aliased methods in array      -04ff*/
     struct {                       /* start aliased methods in array   0500-    */
          __u32 color;                  /* source color                        0-   3*/
          __u32 point0;                 /* y_x S16_S16 in pixels               4-   7*/
          __u32 point1;                 /* y_x S16_S16 in pixels               8-   b*/
          __u32 point2;                 /* y_x S16_S16 in pixels               c-   f*/
     } ColorTriangle[8];            /* end of aliased methods in array      -057f*/
     struct {                       /* start aliased methods in array   0580-    */
          __u32 color;                  /* source color                        0-   3*/
          __u32 point;                  /* y_x S16_S16 in pixels               4-   7*/
     } ColorTrimesh[16];            /* end of aliased methods in array      -05ff*/
     __u32 Reserved05[0x680];
} RivaTriangle;

typedef struct {
     __u32 reserved00[4];
     __u16 FifoFree;
     __u16 Nop[1];
     __u32 reserved01[0x03B];

     __u32 NoOperation;             /* ignored                          0100-0103*/
     __u32 Notify;                  /* NV0037_NOTIFY_*                  0104-0107*/
     __u32 Reserved00[0x01e];
     __u32 SetContextDmaNotifies;   /* NV01_CONTEXT_DMA                 0180-0183*/
     __u32 SetContextDmaImage;      /* NV01_CONTEXT_DMA                 0184-0187*/
     __u32 SetContextPattern;       /* NV01_CONTEXT_PATTERN             0188-018b*/
     __u32 SetContextRop;           /* NV03_CONTEXT_ROP                 018c-018f*/
     __u32 SetContextBeta1;         /* NV01_CONTEXT_BETA1               0190-0193*/
     __u32 SetContextSurface;       /* NV03_CONTEXT_SURFACES_2D         0194-0197*/
     __u32 Reserved01[0x05a];
     __u32 SetColorFormat;          /* NV0037_SET_COLOR_FORMAT_*        0300-0303*/
     __u32 SetOperation;            /* NV0037_SET_OPERATION_*           0304-0307*/
     __u32 ClipPoint;               /* y_x S16_S16                      0308-030b*/
     __u32 ClipSize;                /* height_width U16_U16             030c-030f*/
     __u32 ImageOutPoint;           /* y_x S16_S16                      0310-0313*/
     __u32 ImageOutSize;            /* height_width U16_U16             0314-0317*/
     __u32 DuDx;                    /* S12d20 du/dx                     0318-031b*/
     __u32 DvDy;                    /* S12d20 dv/dy                     031c-031f*/
     __u32 Reserved02[0x038];
     __u32 ImageInSize;             /* height_width U16_U16             0400-0403*/
     __u32 ImageInFormat;           /* pitch U32                        0404-0407*/
     __u32 ImageInOffset;           /* byte offset of top-left texel    0408-040b*/
     __u32 ImageInPoint;            /* v_u U12d4_U12d4                  040c-040f*/
     __u32 Reserved03[0x6fc];
} RivaScaledImage;

/*
 * 2D screen-screen BLT.
 */
typedef struct
{
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop;
    __u32 reserved01[0x0BB];
    __u32 TopLeftSrc;
    __u32 TopLeftDst;
    __u32 WidthHeight;
} RivaScreenBlt;
/*
 * 2D pixel BLT.
 */
typedef struct
{
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop[1];
    __u32 reserved01[0x0BC];
    __u32 TopLeft;
    __u32 WidthHeight;
    __u32 WidthHeightIn;
    __u32 reserved02[0x03C];
    __u32 Pixels;
} RivaPixmap;
/*
 * Filled rectangle combined with monochrome expand.  Useful for glyphs.
 */
typedef struct
{
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop;
    __u32 reserved01[0x0BB];
    __u32 reserved03[(0x040)-1];
    __u32 Color1A;
    struct
    {
        __u32 TopLeft;
        __u32 WidthHeight;
    } UnclippedRectangle[64];
    __u32 reserved04[(0x080)-3];
    struct
    {
        __u32 TopLeft;
        __u32 BottomRight;
    } ClipB;
    __u32 Color1B;
    struct
    {
        __u32 TopLeft;
        __u32 BottomRight;
    } ClippedRectangle[64];
    __u32 reserved05[(0x080)-5];
    struct
    {
        __u32 TopLeft;
        __u32 BottomRight;
    } ClipC;
    __u32 Color1C;
    __u32 WidthHeightC;
    __u32 PointC;
    __u32 MonochromeData1C;
    __u32 reserved06[(0x080)+121];
    struct
    {
        __u32 TopLeft;
        __u32 BottomRight;
    } ClipD;
    __u32 Color1D;
    __u32 WidthHeightInD;
    __u32 WidthHeightOutD;
    __u32 PointD;
    __u32 MonochromeData1D;
    __u32 reserved07[(0x080)+120];
    struct
    {
        __u32 TopLeft;
        __u32 BottomRight;
    } ClipE;
    __u32 Color0E;
    __u32 Color1E;
    __u32 WidthHeightInE;
    __u32 WidthHeightOutE;
    __u32 PointE;
    __u32 MonochromeData01E;
} RivaBitmap;
/*
 * 2D line.
 */
typedef struct
{
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop[1];
    __u32 reserved01[0x0BC];
    __u32 Color;             /* source color               0304-0307*/
    __u32 Reserved02[0x03e];
    struct {                /* start aliased methods in array   0400-    */
        __u32 point0;        /* y_x S16_S16 in pixels            0-   3*/
        __u32 point1;        /* y_x S16_S16 in pixels            4-   7*/
    } Lin[16];              /* end of aliased methods in array      -047f*/
    struct {                /* start aliased methods in array   0480-    */
        __u32 point0X;       /* in pixels, 0 at left                0-   3*/
        __u32 point0Y;       /* in pixels, 0 at top                 4-   7*/
        __u32 point1X;       /* in pixels, 0 at left                8-   b*/
        __u32 point1Y;       /* in pixels, 0 at top                 c-   f*/
    } Lin32[8];             /* end of aliased methods in array      -04ff*/
    __u32 PolyLin[32];       /* y_x S16_S16 in pixels         0500-057f*/
    struct {                /* start aliased methods in array   0580-    */
        __u32 x;             /* in pixels, 0 at left                0-   3*/
        __u32 y;             /* in pixels, 0 at top                 4-   7*/
    } PolyLin32[16];        /* end of aliased methods in array      -05ff*/
    struct {                /* start aliased methods in array   0600-    */
        __u32 color;         /* source color                     0-   3*/
        __u32 point;         /* y_x S16_S16 in pixels            4-   7*/
    } ColorPolyLin[16];     /* end of aliased methods in array      -067f*/
} RivaLine;
/*
 * 3D textured, Z buffered triangle.
 */
typedef struct
{
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop;
    __u32 reserved01[0x0BC];
    __u32 TextureOffset;
    __u32 TextureFormat;
    __u32 TextureFilter;
    __u32 FogColor;
    __u32 Control;
    __u32 AlphaTest;
    __u32 reserved02[0x339];
    __u32 FogAndIndex;
    __u32 Color;
    float ScreenX;
    float ScreenY;
    float ScreenZ;
    float EyeM;
    float TextureS;
    float TextureT;
} RivaTexturedTriangle03;
/*
 * 2D/3D surfaces
 */
typedef struct
{
/*
     __u32 Synchronize;
     __u32 Reserved00[3];
     __u16 FifoFree;
     __u16 Nop;
     __u32 Reserved01[0x05e];
     __u32 SetContextDmaNotifies;
     __u32 SetContextDmaSource;
     __u32 SetContextDmaDestin;
     __u32 Reserved02[0x05d];
     __u32 SetFormat;
     __u32 SetPitch;
     __u32 SetOffsetSource;
     __u32 SetOffsetDestin;
     __u32 Reserved03[0x73c];
*/
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop;
    __u32 reserved01[0x0BC];
    __u32 Pitch;
    __u32 SourceOffset;
    __u32 DestOffset;
} RivaSurface;

typedef struct
{
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop;
    __u32 reserved01[0x0BD];
    __u32 Pitch;
    __u32 RenderBufferOffset;
    __u32 ZBufferOffset;
} RivaSurface3D;

typedef struct {
    __u32 reserved00[4];
    __u16 FifoFree;
    __u16 Nop;
    __u32 reserved01[0x014];

    __u32 NoOperation;             /* ignored                          0100-0103*/
    __u32 Notify;                  /* NV0054_NOTIFY_*                  0104-0107*/
    __u32 Reserved00[0x01e];
    __u32 SetContextDmaNotifies;   /* NV01_CONTEXT_DMA                 0180-0183*/
    __u32 SetContextDmaA;          /* NV01_CONTEXT_DMA                 0184-0187*/
    __u32 SetContextDmaB;          /* NV01_CONTEXT_DMA                 0188-018b*/
    __u32 SetContextSurfaces;      /* NV04_CONTEXT_SURFACES_3D         018c-018f*/
    __u32 Reserved01[0x05c];
    __u32 ColorKeyValue;           /* see text                         0300-0303*/
    __u32 TextureOffset;           /* texture offset in bytes          0304-0307*/
    __u32 TextureFormat;           /* see text                         0308-030b*/
    __u32 TextureFilter;           /* see text                         030c-030f*/
    __u32 Blend;                   /* see text                         0310-0313*/
    __u32 Control;                 /* see text                         0314-0317*/
    __u32 FogColor;                /* X8R8G8B8 fog color               0318-031b*/
    __u32 Reserved02[0x039];
 struct {                       /* start of methods in array        0400-    */
     float sx;                     /* screen x coordinate                00-  03*/
     float sy;                     /* screen y coordinate                04-  07*/
     float sz;                     /* screen z coordinate                08-  0b*/
     float rhw;                    /* reciprocal homogeneous W (1/W)     0c-  0f*/
     __u32 color;                  /* A8R8G8B8                           10-  13*/
     __u32 specular;               /* F8R8G8B8                           14-  17*/
     float tu;                     /* texture u coordinate               18-  1b*/
     float tv;                     /* texture v coordinate               1c-  1f*/
 } Tlvertex[16];                /* end of methods in array              -05ff*/
    __u32 DrawPrimitive[64];       /* see text                         0600-06ff*/
    __u32 Reserved03[0x640];
} RivaDx5Triangle;

#define  NV01_RENDER_SOLID_RECTANGLE                               (0x0000001E)

#define CTX_PTR(o)     (0x1144 + o)

typedef struct
{
     struct {
          __u32 ObjectID;
          __u32 ContextPointer;
     } objects[32];

     __u32 reserved[0x500-64];

     struct {
          __u32 Ctx1, Ctx2, Ctx3, Ctx4;
     } contexts[32];

} RivaInstances;

#endif

