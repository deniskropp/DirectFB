/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <directfb.h>

#include <direct/messages.h>
#include <direct/memcpy.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/surfaces.h>

#include "nvidia.h"
#include "nvidia_mmio.h"
#include "nvidia_2d.h"


static void
nv_copy32( volatile __u32 *dst, __u8 *src, int n )
{
     __u32 *D = (__u32*) dst;
     __u32 *S = (__u32*) src;
     
#ifdef ARCH_X86
     __asm__ __volatile__(
          "rep; movsl"
          : "=&D" (D), "=&S" (S)
          : "c" (n), "0" (D), "1" (S)
          : "memory" );
#else
     do {
          *D++ = *S++;
     } while (--n);
#endif
}

static void
nv_copy16( volatile __u32 *dst, __u8 *src, int n )
{
     __u32 *D = (__u32*) dst;
     __u16 *S = (__u16*) src;

#ifdef ARCH_X86
     __asm__ __volatile__(
          "rep; movsl"
          : "=&D" (D), "=&S" (S)
          : "c" (n/2), "0" (D), "1" (S)
          : "memory" );
#else
     int n2;
     for (n2 = n/2; n2--;) {
          *D++ = *((__u32*)S);
          S += 2;
     }
#endif

     if (n & 1)
          *D = *S;
}


bool nvFillRectangle2D( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData *nvdrv     = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev     = (NVidiaDeviceData*) dev;
     NVRectangle      *Rectangle = nvdrv->Rectangle;
     
     if (nvdev->dst_422) {
          rect->x =  rect->x    >> 1;
          rect->w = (rect->w+1) >> 1;
     }

     nv_waitfifo( nvdev, subchannelof(Rectangle), 3 );
     Rectangle->Color       = nvdev->color;
     Rectangle->TopLeft     = (rect->y << 16) | (rect->x & 0xFFFF);
     Rectangle->WidthHeight = (rect->h << 16) | (rect->w & 0xFFFF);

     return true;
}

bool nvFillTriangle2D( void *drv, void *dev, DFBTriangle *tri )
{
     NVidiaDriverData *nvdrv    = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev    = (NVidiaDeviceData*) dev;
     NVTriangle       *Triangle = nvdrv->Triangle;
     
     nv_waitfifo( nvdev, subchannelof(Triangle), 4 );
     Triangle->Color          = nvdev->color;
     Triangle->TrianglePoint0 = (tri->y1 << 16) | (tri->x1 & 0xFFFF);
     Triangle->TrianglePoint1 = (tri->y2 << 16) | (tri->x2 & 0xFFFF);
     Triangle->TrianglePoint2 = (tri->y3 << 16) | (tri->x3 & 0xFFFF);

     return true;
}

bool nvDrawRectangle2D( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData *nvdrv     = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev     = (NVidiaDeviceData*) dev;
     NVRectangle      *Rectangle = nvdrv->Rectangle;
     
     if (nvdev->dst_422) {
          rect->x =  rect->x    >> 1;
          rect->w = (rect->w+1) >> 1;
     }
     
     nv_waitfifo( nvdev, subchannelof(Rectangle), 9 );
     Rectangle->Color       = nvdev->color;
     /* top */
     Rectangle->TopLeft     = (rect->y << 16) | (rect->x & 0xFFFF);
     Rectangle->WidthHeight = (1       << 16) | (rect->w & 0xFFFF);
     /* bottom */
     Rectangle->TopLeft     = ((rect->y + rect->h - 1) << 16) | (rect->x & 0xFFFF);
     Rectangle->WidthHeight = (1                       << 16) | (rect->w & 0xFFFF);
     /* left */
     Rectangle->TopLeft     = ((rect->y + 1) << 16) | (rect->x & 0xFFFF);
     Rectangle->WidthHeight = ((rect->h - 2) << 16) | 1;
     /* right */
     Rectangle->TopLeft     = ((rect->y + 1) << 16) | ((rect->x + rect->w - 1) & 0xFFFF);
     Rectangle->WidthHeight = ((rect->h - 2) << 16) | 1;

     return true;
}

bool nvDrawLine2D( void *drv, void *dev, DFBRegion *line )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     NVLine           *Line  = nvdrv->Line;
     
     nv_waitfifo( nvdev, subchannelof(Line), 3 );
     Line->Color         = nvdev->color;
     Line->Lin[0].point0 = (line->y1 << 16) | (line->x1 & 0xFFFF);
     Line->Lin[0].point1 = (line->y2 << 16) | (line->x2 & 0xFFFF);

     return true;
}

bool nvBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
 
     if (nvdev->dst_422) {
          dx      =  dx         >> 1;
          rect->x =  rect->x    >> 1;
          rect->w = (rect->w+1) >> 1;
     }
     
     if (nvdev->bop0 == 2 || nvdev->bop0 == 4 ||
         nvdev->src_format != nvdev->dst_format)
     {
          NVScaledImage *ScaledImage = nvdrv->ScaledImage;
          DFBRectangle  *clip        = &nvdev->clip;
         
          nv_waitfifo( nvdev, subchannelof(ScaledImage), 1 );
          ScaledImage->SetColorFormat = nvdev->video_format;
          
          nv_waitfifo( nvdev, subchannelof(ScaledImage), 6 );
          ScaledImage->ClipPoint     = (clip->y << 16) | (clip->x & 0xFFFF);
          ScaledImage->ClipSize      = (clip->h << 16) | (clip->w & 0xFFFF);
          ScaledImage->ImageOutPoint = (dy      << 16) | (dx      & 0xFFFF);
          ScaledImage->ImageOutSize  = (rect->h << 16) | (rect->w & 0xFFFF);
          ScaledImage->DuDx          = 0x100000;
          ScaledImage->DvDy          = 0x100000;

          nv_waitfifo( nvdev, subchannelof(ScaledImage), 4 );
          ScaledImage->ImageInSize   = (nvdev->src_height << 16)   |
                                       (nvdev->src_width & 0xffff);
          ScaledImage->ImageInFormat = (nvdev->src_pitch & 0xFFFF) |
                                        nvdev->filter;
          ScaledImage->ImageInOffset = nvdev->src_offset;
          ScaledImage->ImageInPoint  = (rect->y << 20) | ((rect->x<<4) & 0xFFFF);
     }
     else {
          NVScreenBlt *ScreenBlt = nvdrv->ScreenBlt;

          nv_waitfifo( nvdev, subchannelof(ScreenBlt), 3 );
          ScreenBlt->TopLeftSrc  = (rect->y << 16) | (rect->x & 0xFFFF);
          ScreenBlt->TopLeftDst  = (dy      << 16) | (dx      & 0xFFFF);
          ScreenBlt->WidthHeight = (rect->h << 16) | (rect->w & 0xFFFF);
     }

     return true;
}

bool nvDMABlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     NVidiaDriverData *nvdrv    = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev    = (NVidiaDeviceData*) dev;
     NVImageBlt       *ImageBlt = nvdrv->ImageBlt;
     __u8             *src      = nvdev->src_address;
     __u32             src_w    = rect->w;
     __u32             src_h    = rect->h;
     int               w, h;

     if (DFB_BYTES_PER_PIXEL(nvdev->src_format) == 2)
          src_w = (src_w + 1) & ~1;

     nv_waitfifo( nvdev, subchannelof(ImageBlt), 1 );
     ImageBlt->SetColorFormat = nvdev->system_format;
     
     nv_waitfifo( nvdev, subchannelof(ImageBlt), 3 );
     ImageBlt->Point   = (dy      << 16) | (dx      & 0xffff);
     ImageBlt->SizeOut = (rect->h << 16) | (rect->w & 0xffff);
     ImageBlt->SizeIn  = (src_h   << 16) | (src_w   & 0xffff);
     
     switch (nvdev->src_format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
               src += rect->y * nvdev->src_pitch + rect->x * 2;
               for (h = rect->h; h--;) {
                    __u8 *S = src;
                    
                    for (w = rect->w; w > 255; w -= 256) {
                         nv_waitfifo( nvdev, subchannelof(ImageBlt), 128 );
                         direct_memcpy( (void*)&ImageBlt->Pixel[0], S, 128*4 );
                         S += 128*4;
                    }
                    if (w > 0) {
                         nv_waitfifo( nvdev, subchannelof(ImageBlt), (w+1)>>1 );
                         nv_copy16( &ImageBlt->Pixel[0], S, w );
                    }
                    
                    src += nvdev->src_pitch;
               }
               break;
               
          default:
               src += rect->y * nvdev->src_pitch + rect->x * 4;
               for (h = rect->h; h--;) {
                    __u8 *S = src;
                    
                    for (w = rect->w; w > 127; w -= 128) {
                         nv_waitfifo( nvdev, subchannelof(ImageBlt), 128 );
                         direct_memcpy( (void*)&ImageBlt->Pixel[0], S, 128*4 );
                         S += 128*4;
                    }
                    if (w > 0) {
                         nv_waitfifo( nvdev, subchannelof(ImageBlt), w );
                         nv_copy32( &ImageBlt->Pixel[0], S, w );
                    }
                    
                    src += nvdev->src_pitch;
               }
               break;
     }
     
     return true;
}

bool nvStretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     NVidiaDriverData *nvdrv       = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     NVScaledImage    *ScaledImage = nvdrv->ScaledImage;
     DFBRectangle     *cr          = &nvdev->clip;
     
     if (nvdev->dst_422) {
          sr->x =  sr->x    >> 1;
          sr->w = (sr->w+1) >> 1;
          dr->x =  dr->x    >> 1;
          dr->w = (dr->w+1) >> 1;
     } 

     nv_waitfifo( nvdev, subchannelof(ScaledImage), 1 );
     ScaledImage->SetColorFormat = nvdev->video_format;
     
     nv_waitfifo( nvdev, subchannelof(ScaledImage), 6 );
     ScaledImage->ClipPoint     = (cr->y << 16) | (cr->x & 0xFFFF);
     ScaledImage->ClipSize      = (cr->h << 16) | (cr->w & 0xFFFF);
     ScaledImage->ImageOutPoint = (dr->y << 16) | (dr->x & 0xFFFF);
     ScaledImage->ImageOutSize  = (dr->h << 16) | (dr->w & 0xFFFF);
     ScaledImage->DuDx          = (sr->w << 20) /  dr->w;
     ScaledImage->DvDy          = (sr->h << 20) /  dr->h;

     nv_waitfifo( nvdev, subchannelof(ScaledImage), 4 );
     ScaledImage->ImageInSize   = (nvdev->src_height << 16) | nvdev->src_width;
     ScaledImage->ImageInFormat = (nvdev->src_pitch & 0xFFFF) | nvdev->filter;
     ScaledImage->ImageInOffset = nvdev->src_offset;
     ScaledImage->ImageInPoint  = (sr->y << 20) | ((sr->x<<4) & 0xFFFF);
     
     return true;
}

bool nvDMAStretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     NVidiaDriverData *nvdrv          = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev          = (NVidiaDeviceData*) dev;
     NVStretchedImage *StretchedImage = nvdrv->StretchedImage;
     DFBRectangle     *cr             = &nvdev->clip; 
     __u8             *src            = nvdev->src_address;
     __u32             src_w          = sr->w;
     __u32             src_h          = sr->h;
     int               h, w;

     if (DFB_BYTES_PER_PIXEL(nvdev->src_format) == 2)
          src_w = (src_w + 1) & ~1;

     nv_waitfifo( nvdev, subchannelof(StretchedImage), 1 );
     StretchedImage->SetColorFormat = nvdev->system_format;
     
     nv_waitfifo( nvdev, subchannelof(StretchedImage), 4 );
     StretchedImage->ImageInSize   = (src_h << 16) | (src_w & 0xffff);
     StretchedImage->DxDu          = (dr->w << 20) /  src_w;
     StretchedImage->DyDv          = (dr->h << 20) /  src_h;
     StretchedImage->ClipPoint     = (cr->y << 16) | (cr->x & 0xffff);
     StretchedImage->ClipSize      = (cr->h << 16) | (cr->w & 0xffff);
     StretchedImage->ImageOutPoint = (dr->y << 20) | ((dr->x<<4) & 0xffff);

     switch (nvdev->src_format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
               src += sr->y * nvdev->src_pitch + sr->x * 2;
               for (h = sr->h; h--;) {
                    __u8 *S = src;
                    
                    for (w = sr->w; w > 255; w -= 256) {
                         nv_waitfifo( nvdev, subchannelof(StretchedImage), 128 );
                         direct_memcpy( (void*)&StretchedImage->Pixel[0], S, 128*4 );
                         S += 128*4;
                    }
                    if (w > 0) {
                         nv_waitfifo( nvdev, subchannelof(StretchedImage), (w+1)>>1 );
                         nv_copy16( &StretchedImage->Pixel[0], S, w );
                    }

                    src += nvdev->src_pitch;
               }
               break;
               
          default:
               src += sr->y * nvdev->src_pitch + sr->x * 4;
               for (h = sr->h; h--;) {
                    __u8 *S = src;
                    
                    for (w = sr->w; w > 127; w -= 128) {
                         nv_waitfifo( nvdev, subchannelof(StretchedImage), 128 );
                         direct_memcpy( (void*)&StretchedImage->Pixel[0], S, 128*4 );
                         S += 128*4;
                    }
                    if (w > 0) {
                         nv_waitfifo( nvdev, subchannelof(StretchedImage), w );
                         nv_copy32( &StretchedImage->Pixel[0], S, w );
                    }

                    src += nvdev->src_pitch;
               }
               break;
     }

     return true;
}

