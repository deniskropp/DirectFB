/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/surfaces.h>

#include "nvidia.h"
#include "nvidia_mmio.h"
#include "nvidia_2d.h"


bool nvFillRectangle2D( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData *nvdrv     = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev     = (NVidiaDeviceData*) dev;
     NVRectangle      *Rectangle = nvdrv->Rectangle;
     
     if (nvdrv->arch == NV_ARCH_20 && nvdev->modified & SMF_DRAWING_FLAGS) {
          nv_waitfifo( nvdev, subchannelof(Rectangle), 1 );
          Rectangle->SetOperation = nvdev->dop;
          
          nvdev->modified ^= SMF_DRAWING_FLAGS;
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
     
     if (nvdrv->arch == NV_ARCH_20 && nvdev->modified & SMF_DRAWING_FLAGS) {
          nv_waitfifo( nvdev, subchannelof(Triangle), 1 );
          Triangle->SetOperation = nvdev->dop;

          nvdev->modified ^= SMF_DRAWING_FLAGS;
     }
     
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
     
     if (nvdrv->arch == NV_ARCH_20 && nvdev->modified & SMF_DRAWING_FLAGS) {
          nv_waitfifo( nvdev, subchannelof(Rectangle), 1 );
          Rectangle->SetOperation = nvdev->dop;
          
          nvdev->modified ^= SMF_DRAWING_FLAGS;
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
     
     if (nvdrv->arch == NV_ARCH_20 && nvdev->modified & SMF_DRAWING_FLAGS) {
          nv_waitfifo( nvdev, subchannelof(Line), 1 );
          Line->SetOperation = nvdev->dop;

          nvdev->modified ^= SMF_DRAWING_FLAGS;
     }

     nv_waitfifo( nvdev, subchannelof(Line), 3 );
     Line->Color         = nvdev->color;
     Line->Lin[0].point0 = (line->y1 << 16) | (line->x1 & 0xFFFF);
     Line->Lin[0].point1 = (line->y2 << 16) | (line->x2 & 0xFFFF);

     return true;
}

bool nv4Blit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     NVScreenBlt      *Blt   = nvdrv->Blt;
     
     if (nvdev->bop || nvdev->src_format != nvdev->dst_format) {
          DFBRectangle dr = { dx, dy, rect->w, rect->h };
          return nv4StretchBlit( drv, dev, rect, &dr );
     }

     nv_waitfifo( nvdev, subchannelof(Blt), 3 );
     Blt->TopLeftSrc  = (rect->y << 16) | (rect->x & 0xFFFF);
     Blt->TopLeftDst  = (dy      << 16) | (dx      & 0xFFFF);
     Blt->WidthHeight = (rect->h << 16) | (rect->w & 0xFFFF);

     return true;
}

bool nv5Blit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     NVScreenBlt      *Blt   = nvdrv->Blt;
 
     if (nvdev->bop != 3 || nvdev->src_format != nvdev->dst_format) {
          DFBRectangle dr = { dx, dy, rect->w, rect->h };
          return nv5StretchBlit( drv, dev, rect, &dr );
     }

     nv_waitfifo( nvdev, subchannelof(Blt), 3 );
     Blt->TopLeftSrc   = (rect->y << 16) | (rect->x & 0xFFFF);
     Blt->TopLeftDst   = (dy      << 16) | (dx      & 0xFFFF);
     Blt->WidthHeight  = (rect->h << 16) | (rect->w & 0xFFFF);

     return true;
}

bool nv4StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{

     NVidiaDriverData *nvdrv       = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     NVScaledImage    *ScaledImage = nvdrv->ScaledImage;
     DFBRectangle     *cr          = &nvdev->clip;
     __u32             format      = 0;
     
     switch (nvdev->src_format) {
          case DSPF_ARGB1555:
               format = 0x00000002;
               break;
          case DSPF_RGB32:
               format = 0x00000004;
               break;
          case DSPF_ARGB:
               format = (nvdev->argb_src) ? 0x00000003 : 0x00000004;
               break;
          case DSPF_YUY2:
               format = 0x00000005;
               break;
          case DSPF_UYVY:
               format = 0x00000006;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               return false;
     }

     if (nvdev->modified & SMF_BLITTING_FLAGS) {
          nv_waitfifo( nvdev, subchannelof(ScaledImage), 2 );
          ScaledImage->SetColorFormat = format;
          ScaledImage->SetOperation   = nvdev->bop;

          nvdev->modified ^= SMF_BLITTING_FLAGS;
     } else {
          nv_waitfifo( nvdev, subchannelof(ScaledImage), 1 );
          ScaledImage->SetColorFormat = format;
     }

     nv_waitfifo( nvdev, subchannelof(ScaledImage), 6 );
     ScaledImage->ClipPoint      = (cr->y << 16) | (cr->x & 0xFFFF);
     ScaledImage->ClipSize       = (cr->h << 16) | (cr->w & 0xFFFF);
     ScaledImage->ImageOutPoint  = (dr->y << 16) | (dr->x & 0xFFFF);
     ScaledImage->ImageOutSize   = (dr->h << 16) | (dr->w & 0xFFFF);
     ScaledImage->DuDx           = (sr->w << 20) /  dr->w;
     ScaledImage->DvDy           = (sr->h << 20) /  dr->h;

     nv_waitfifo( nvdev, subchannelof(ScaledImage), 4 );
     ScaledImage->ImageInSize    = (nvdev->src_height << 16) | nvdev->src_width;
     ScaledImage->ImageInFormat  = nvdev->src_pitch;
     ScaledImage->ImageInOffset  = nvdev->src_offset;
     ScaledImage->ImageInPoint   = (sr->y << 20) | ((sr->x << 4) & 0xFFFF);
     
     return true;
}

bool nv5StretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{

     NVidiaDriverData *nvdrv       = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     NVScaledImage    *ScaledImage = nvdrv->ScaledImage;
     DFBRectangle     *cr          = &nvdev->clip;
     __u32             format      = 0;

     switch (nvdev->src_format) {
          case DSPF_ARGB1555:
               format = 0x00000002;
               break;
          case DSPF_RGB16:
               format = 0x00000007;
               break;
          case DSPF_RGB32:
               format = 0x00000004;
               break;
          case DSPF_ARGB:
               format = (nvdev->argb_src) ? 0x00000003 : 0x00000004;
               break;
          case DSPF_YUY2:
               format = 0x00000005;
               break;
          case DSPF_UYVY:
               format = 0x00000006;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               return false;
     }
  
     if (nvdev->modified & SMF_BLITTING_FLAGS) {
          nv_waitfifo( nvdev, subchannelof(ScaledImage), 2 );
          ScaledImage->SetColorFormat = format;
          ScaledImage->SetOperation   = nvdev->bop;
          
          nvdev->modified ^= SMF_BLITTING_FLAGS;
     } else {
          nv_waitfifo( nvdev, subchannelof(ScaledImage), 1 );
          ScaledImage->SetColorFormat = format;
     }

     nv_waitfifo( nvdev, subchannelof(ScaledImage), 6 );
     ScaledImage->ClipPoint      = (cr->y << 16) | (cr->x & 0xFFFF);
     ScaledImage->ClipSize       = (cr->h << 16) | (cr->w & 0xFFFF);
     ScaledImage->ImageOutPoint  = (dr->y << 16) | (dr->x & 0xFFFF);
     ScaledImage->ImageOutSize   = (dr->h << 16) | (dr->w & 0xFFFF);
     ScaledImage->DuDx           = (sr->w << 20) /  dr->w;
     ScaledImage->DvDy           = (sr->h << 20) /  dr->h;

     nv_waitfifo( nvdev, subchannelof(ScaledImage), 4 );
     ScaledImage->ImageInSize    = (nvdev->src_height << 16) | nvdev->src_width;
     ScaledImage->ImageInFormat  = 0x01010000 | (nvdev->src_pitch & 0xFFFF);
     ScaledImage->ImageInOffset  = nvdev->src_offset;
     ScaledImage->ImageInPoint   = (sr->y << 20) | ((sr->x << 4) & 0xFFFF);
     
     return true;
}

