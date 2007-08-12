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
#include <core/surface.h>

#include "nvidia.h"
#include "nvidia_regs.h"
#include "nvidia_accel.h"
#include "nvidia_2d.h"


static void
nv_copy32( volatile u32 *dst, u8 *src, int n )
{
     u32 *D = (u32*) dst;
     u32 *S = (u32*) src;
     
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
nv_copy16( volatile u32 *dst, u8 *src, int n )
{
     u32 *D = (u32*) dst;
     u16 *S = (u16*) src;

#ifdef ARCH_X86
     __asm__ __volatile__(
          "rep; movsl"
          : "=&D" (D), "=&S" (S)
          : "c" (n/2), "0" (D), "1" (S)
          : "memory" );
#else
     for (; n > 1; n -= 2) {
          *D++ = *((u32*)S);
          S += 2;
     }
#endif

     if (n & 1)
          *D = *S;
}

static inline bool
nv_clip_source( DFBRectangle *rect, u32 width, u32 height )
{
     if (rect->x >= width || rect->y >= height)
          return false;
          
     if (rect->x < 0) {
          rect->w += rect->x;
          rect->x  = 0;
     }
     if (rect->y < 0) {
          rect->h += rect->y;
          rect->y  = 0;
     }
               
     rect->w = MIN( rect->w, width  - rect->x );
     rect->h = MIN( rect->h, height - rect->y );
     
     return (rect->w > 0 && rect->h > 0);
}



bool nvFillRectangle2D( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     
     if (nvdev->dst_422) {
          rect->x /= 2;
          rect->w = (rect->w+1) >> 1;
     }
     
     nv_begin( SUBC_RECTANGLE, RECT_COLOR, 1 );
     nv_outr( nvdev->color2d );
     
     nv_begin( SUBC_RECTANGLE, RECT_TOP_LEFT, 2 );
     nv_outr( (rect->y << 16) | (rect->x & 0xFFFF) );
     nv_outr( (rect->h << 16) | (rect->w & 0xFFFF) );

     return true;
}

bool nvFillTriangle2D( void *drv, void *dev, DFBTriangle *tri )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     
     nv_begin( SUBC_TRIANGLE, TRI_COLOR, 1 );
     nv_outr( nvdev->color2d );
     
     nv_begin( SUBC_TRIANGLE, TRI_POINT0, 3 );
     nv_outr( (tri->y1 << 16) | (tri->x1 & 0xFFFF) );
     nv_outr( (tri->y2 << 16) | (tri->x2 & 0xFFFF) );
     nv_outr( (tri->y3 << 16) | (tri->x3 & 0xFFFF) );

     return true;
}

bool nvDrawRectangle2D( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     
     if (nvdev->dst_422) {
          rect->x /= 2;
          rect->w = (rect->w+1) >> 1;
     }
     
     nv_begin( SUBC_RECTANGLE, RECT_COLOR, 1 );
     nv_outr( nvdev->color2d );
     
     nv_begin( SUBC_RECTANGLE, RECT_TOP_LEFT, 8 );
     /* top */
     nv_outr( (rect->y << 16) | (rect->x & 0xFFFF) );
     nv_outr( (1       << 16) | (rect->w & 0xFFFF) );
     /* bottom */
     nv_outr( ((rect->y + rect->h - 1) << 16) | (rect->x & 0xFFFF) );
     nv_outr( (1                       << 16) | (rect->w & 0xFFFF) );
     /* left */
     nv_outr( ((rect->y + 1) << 16) | (rect->x & 0xFFFF) );
     nv_outr( ((rect->h - 2) << 16) | 1 );
     /* right */
     nv_outr( ((rect->y + 1) << 16) | ((rect->x + rect->w - 1) & 0xFFFF) );
     nv_outr( ((rect->h - 2) << 16) | 1 );

     return true;
}

bool nvDrawLine2D( void *drv, void *dev, DFBRegion *line )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     
     nv_begin( SUBC_LINE, LINE_COLOR, 1 );
     nv_outr( nvdev->color2d );
     
     nv_begin( SUBC_LINE, LINE_POINT0, 2 );
     nv_outr( (line->y1 << 16) | (line->x1 & 0xFFFF) );
     nv_outr( (line->y2 << 16) | (line->x2 & 0xFFFF) );

     return true;
}

bool nvBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;

     if (nvdev->blittingflags & DSBLIT_DEINTERLACE) {
          DFBRectangle dr = { dx, dy, rect->w, rect->h };
          return nvStretchBlit( drv, dev, rect, &dr );
     }
 
     if (nvdev->dst_422) {
          dx      /= 2;
          rect->x /= 2;
          rect->w  = (rect->w+1) >> 1;
     }
     
     if (nvdev->blittingflags || nvdev->src_format != nvdev->dst_format) {
          DFBRectangle  *clip       = &nvdev->clip;
          u32            src_width  = (nvdev->src_width  + 1) & ~1;
          u32            src_height = (nvdev->src_height + 1) & ~1;
          u32            filter     = 0;

          if (nvdev->dst_422)
               src_width >>= 1; 

          if (nvdev->arch > NV_ARCH_04)
               filter = SCALER_IN_FORMAT_ORIGIN_CORNER |
                        SCALER_IN_FORMAT_FILTER_NEAREST;

          nv_begin( SUBC_SCALEDIMAGE, SCALER_COLOR_FORMAT, 1 );
          nv_outr( nvdev->scaler_format );
          
          nv_begin( SUBC_SCALEDIMAGE, SCALER_CLIP_POINT, 6 );
          nv_outr( (clip->y << 16) | (clip->x & 0xFFFF) );
          nv_outr( (clip->h << 16) | (clip->w & 0xFFFF) );
          nv_outr( (dy      << 16) | (dx      & 0xFFFF) );
          nv_outr( (rect->h << 16) | (rect->w & 0xFFFF) );
          nv_outr( 0x100000 );
          nv_outr( 0x100000 );

          nv_begin( SUBC_SCALEDIMAGE, SCALER_IN_SIZE, 4 );
          nv_outr( (src_height << 16) | (src_width & 0xFFFF) );
          nv_outr( (nvdev->src_pitch & 0xFFFF) | filter );
          nv_outr( nvdev->src_offset );
          nv_outr( (rect->y << 20) | ((rect->x<<4) & 0xFFFF) );
     }
     else {
          nv_begin( SUBC_SCREENBLT, BLIT_TOP_LEFT_SRC, 3 );
          nv_outr( (rect->y << 16) | (rect->x & 0xFFFF) );
          nv_outr( (dy      << 16) | (dx      & 0xFFFF) );
          nv_outr( (rect->h << 16) | (rect->w & 0xFFFF) );
     }

     return true;
}   

bool nvBlitFromCPU( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     u8               *src   = nvdev->src_address;
     u32               src_w;
     u32               src_h;
     int               w, h, n;
     
     if (nvdev->blittingflags & DSBLIT_DEINTERLACE) {
          DFBRectangle dr = { dx, dy, rect->x, rect->y };
          return nvStretchBlitFromCPU( drv, dev, rect, &dr );
     }
     
     if (!nv_clip_source( rect, nvdev->src_width, nvdev->src_height ))
          return true;

     src_w = (DFB_BYTES_PER_PIXEL(nvdev->src_format) == 2)
             ? ((rect->w + 1) & ~1) : rect->w;
     src_h = rect->h;

     nv_begin( SUBC_IMAGEBLT, IBLIT_COLOR_FORMAT, 1 );
     nv_outr( nvdev->system_format );
     
     nv_begin( SUBC_IMAGEBLT, IBLIT_POINT, 3 );
     nv_outr( (dy      << 16) | (dx      & 0xFFFF) );
     nv_outr( (rect->h << 16) | (rect->w & 0xFFFF) );
     nv_outr( (src_h   << 16) | (src_w   & 0xFFFF) );

     n = nvdev->use_dma ? 256 : 128;
     
     switch (nvdev->src_format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
               src += rect->y * nvdev->src_pitch + rect->x * 2;
               for (h = rect->h; h--;) {
                    u8 *s = src;
                    
                    for (w = rect->w; w >= n*2; w -= n*2) {
                         nv_begin( SUBC_IMAGEBLT, IBLIT_PIXEL0, n );
                         direct_memcpy( (void*)nvdev->cmd_ptr, s, n*4 );
                         s += n*4;
                    }
                    if (w > 0) {
                         nv_begin( SUBC_IMAGEBLT, IBLIT_PIXEL0, (w+1)>>1 );
                         nv_copy16( nvdev->cmd_ptr, s, w );
                    }
                    
                    src += nvdev->src_pitch;
               }
               break;
               
          default:
               src += rect->y * nvdev->src_pitch + rect->x * 4;
               for (h = rect->h; h--;) {
                    u8 *s = src;
                    
                    for (w = rect->w; w >= n; w -= n) {
                         nv_begin( SUBC_IMAGEBLT, IBLIT_PIXEL0, n );
                         direct_memcpy( (void*)nvdev->cmd_ptr, s, n*4 );
                         s += n*4;
                    }
                    if (w > 0) {
                         nv_begin( SUBC_IMAGEBLT, IBLIT_PIXEL0, w );
                         nv_copy32( nvdev->cmd_ptr, s, w );
                    }
                    
                    src += nvdev->src_pitch;
               }
               break;
     }
     
     return true;
}

bool nvStretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
     NVidiaDriverData *nvdrv      = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev      = (NVidiaDeviceData*) dev;
     DFBRectangle     *cr         = &nvdev->clip;
     u32               src_width  = (nvdev->src_width  + 1) & ~1;
     u32               src_height = (nvdev->src_height + 1) & ~1;
     
     if (nvdev->dst_422) {
          sr->x /= 2;
          sr->w  = (sr->w+1) >> 1;
          dr->x /= 2;
          dr->w  = (dr->w+1) >> 1;
          src_width >>= 1;
     }

     if (nvdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h  = (sr->h+1) / 2;
     }

     nv_begin( SUBC_SCALEDIMAGE, SCALER_COLOR_FORMAT, 1 );
     nv_outr( nvdev->scaler_format );
     
     nv_begin( SUBC_SCALEDIMAGE, SCALER_CLIP_POINT, 6 );
     nv_outr( (cr->y << 16) | (cr->x & 0xFFFF) );
     nv_outr( (cr->h << 16) | (cr->w & 0xFFFF) );
     nv_outr( (dr->y << 16) | (dr->x & 0xFFFF) );
     nv_outr( (dr->h << 16) | (dr->w & 0xFFFF) );
     nv_outr( (sr->w << 20) /  dr->w );
     nv_outr( (sr->h << 20) /  dr->h );

     nv_begin( SUBC_SCALEDIMAGE, SCALER_IN_SIZE, 4 );
     nv_outr( (src_height << 16) | (src_width & 0xFFFF) );
     nv_outr( (nvdev->src_pitch & 0xFFFF) | nvdev->scaler_filter );
     nv_outr( nvdev->src_offset );
     nv_outr( (sr->y << 20) | ((sr->x << 4) & 0xFFFF) );
     
     return true;
}

bool nvStretchBlitFromCPU( void *drv, void *dev, 
                           DFBRectangle *sr, DFBRectangle *dr )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;
     DFBRectangle     *cr     = &nvdev->clip; 
     u8               *src    = nvdev->src_address;
     u32               src_w;
     u32               src_h;
     int               w, h, n;

     if (!nv_clip_source( sr, nvdev->src_width, nvdev->src_height ))
          return true;

     if (nvdev->blittingflags & DSBLIT_DEINTERLACE) {
          sr->y /= 2;
          sr->h /= 2;
     }
     
     src_w = (DFB_BYTES_PER_PIXEL(nvdev->src_format) == 2)
             ? ((sr->w + 1) & ~1) : sr->w;
     src_h = sr->h;

     nv_begin( SUBC_STRETCHEDIMAGE, ISTRETCH_COLOR_FORMAT, 1 );
     nv_outr( nvdev->system_format );
     
     nv_begin( SUBC_STRETCHEDIMAGE, ISTRETCH_IN_SIZE, 6 );
     nv_outr( (src_h << 16) | (src_w & 0xFFFF) );
     nv_outr( (dr->w << 20) /  src_w );
     nv_outr( (dr->h << 20) /  src_h );
     nv_outr( (cr->y << 16) | (cr->x & 0xFFFF) );
     nv_outr( (cr->h << 16) | (cr->w & 0xFFFF) );
     nv_outr( (dr->y << 20) | ((dr->x<<4) & 0xFFFF) );

     n = nvdev->use_dma ? 256 : 128;

     switch (nvdev->src_format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
               src += sr->y * nvdev->src_pitch + sr->x * 2;
               for (h = sr->h; h--;) {
                    u8 *s = src;
                    
                    for (w = sr->w; w >= n*2; w -= n*2) {
                         nv_begin( SUBC_STRETCHEDIMAGE, ISTRETCH_PIXEL0, n );
                         direct_memcpy( (void*)nvdev->cmd_ptr, s, n*4 );
                         s += n*4;
                    }
                    if (w > 0) {
                         nv_begin( SUBC_STRETCHEDIMAGE, ISTRETCH_PIXEL0, (w+1)>>1 );
                         nv_copy16( nvdev->cmd_ptr, s, w );
                    }

                    src += nvdev->src_pitch;
               }
               break;
               
          default:
               src += sr->y * nvdev->src_pitch + sr->x * 4;
               for (h = sr->h; h--;) {
                    u8 *s= src;
                    
                    for (w = sr->w; w >= n; w -= n) {
                         nv_begin( SUBC_STRETCHEDIMAGE, ISTRETCH_PIXEL0, n );
                         direct_memcpy( (void*)nvdev->cmd_ptr, s, n*4 );
                         s += n*4;
                    }
                    if (w > 0) {
                         nv_begin( SUBC_STRETCHEDIMAGE, ISTRETCH_PIXEL0, w );
                         nv_copy32( nvdev->cmd_ptr, s, w );
                    }

                    src += nvdev->src_pitch;
               }
               break;
     }

     return true;
}

