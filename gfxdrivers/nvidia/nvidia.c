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
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#ifdef USE_SYSFS
# include <sysfs/libsysfs.h>
#endif

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/system.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( nvidia )

#include "nvidia.h"
#include "nvidia_mmio.h"
#include "nvidia_2d.h"
#include "nvidia_3d.h"


/* TNT */
#define NV4_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV4_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE | DFXL_DRAWLINE)

#define NV4_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_BLEND_COLORALPHA | DSBLIT_BLEND_ALPHACHANNEL)

#define NV4_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES)

/* TNT2/GeForce(1,2,4MX) */
#define NV5_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV5_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE | DFXL_DRAWLINE)

#define NV5_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_BLEND_COLORALPHA | DSBLIT_BLEND_ALPHACHANNEL | \
                DSBLIT_COLORIZE)

#define NV5_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES)

/* GeForce(3,4Ti,FX) */
#define NV20_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV20_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE | DFXL_DRAWLINE)

#define NV20_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_BLEND_COLORALPHA | DSBLIT_BLEND_ALPHACHANNEL | \
                DSBLIT_COLORIZE)

#define NV20_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)


#define DSBLIT_ALPHABLEND (DSBLIT_BLEND_COLORALPHA | DSBLIT_BLEND_ALPHACHANNEL)
#define DSBLIT_MODULATE   (DSBLIT_ALPHABLEND | DSBLIT_COLORIZE)


static inline __u32
nv_hashkey( __u32 obj )
{
     return ((obj >>  0) & 0x000001FF) ^
            ((obj >>  9) & 0x000001FF) ^
            ((obj >> 18) & 0x000001FF) ^
            ((obj >> 27));
     /* key should be xored with (fifo channel id << 5) too,
      * but, since we always use channel 0, it can be ignored */
}

static inline void
nv_store_dma( NVidiaDriverData *nvdrv, __u32 obj,
              __u32 addr, __u32 class, __u32 flags,
              __u32 size, __u32 frame, __u32 access )
{ 
     volatile __u8 *PRAMIN = nvdrv->PRAMIN;
     volatile __u8 *PRAMHT = nvdrv->PRAMHT;
     __u32          key    = nv_hashkey( obj );
     __u32          ctx    = addr | (0 << 16) | (1 << 31);

     /* NV_PRAMIN_RAMRO_0 */
     nv_out32( PRAMIN, (addr << 4) +  0, class | flags );
     nv_out32( PRAMIN, (addr << 4) +  4, size - 1 );
     nv_out32( PRAMIN, (addr << 4) +  8, (frame & 0xFFFFF000) | access );
     nv_out32( PRAMIN, (addr << 4) + 12, 0xFFFFFFFF );
 
     /* store object id and context */
     nv_out32( PRAMHT, (key << 3) + 0, obj );
     nv_out32( PRAMHT, (key << 3) + 4, ctx );
}

static inline void
nv_store_object( NVidiaDriverData *nvdrv, __u32 obj, 
                 __u32 addr, __u32 class, __u32 flags,
                 __u32 color, __u32 dma0, __u32 dma1 )
{
     volatile __u8 *PRAMIN = nvdrv->PRAMIN;
     volatile __u8 *PRAMHT = nvdrv->PRAMHT;
     __u32          key    = nv_hashkey( obj );
     __u32          ctx    = addr | (1 << 16) | (1 << 31);

     /* NV_PRAMIN_CTX_0 */
     nv_out32( PRAMIN, (addr << 4) +  0, class | flags );
     /* NV_PRAMIN_CTX_1 */
     nv_out32( PRAMIN, (addr << 4) +  4, color );
     /* NV_PRAMIN_CTX_2 */
     nv_out32( PRAMIN, (addr << 4) +  8, dma0 | (dma1 << 16) );
     /* NV_PRAMIN_CTX_3 */
     nv_out32( PRAMIN, (addr << 4) + 12, 0x00000000 );

     /* store object id and context */
     nv_out32( PRAMHT, (key << 3) + 0, obj );
     nv_out32( PRAMHT, (key << 3) + 4, ctx );
}

static inline void
nv_assign_object( NVidiaDriverData *nvdrv,
                  NVidiaDeviceData *nvdev,
                  int               subchannel,
                  __u32             object )
{
     NVFifoChannel *Fifo = nvdrv->Fifo;

     if (nvdev->subchannel_object[subchannel] != object) {
          nv_waitfifo( nvdev, &Fifo->sub[subchannel], 1 );
          Fifo->sub[subchannel].SetObject      = object;
          nvdev->subchannel_object[subchannel] = object;
     }
}


static void
nv_set_destination( NVidiaDriverData  *nvdrv,
                    NVidiaDeviceData  *nvdev,
                    SurfaceBuffer     *buffer )
{
     NVSurfaces2D *Surfaces2D  = nvdrv->Surfaces2D;
     NVSurfaces3D *Surfaces3D  = nvdrv->Surfaces3D;
     __u32         dst_offset  = (buffer->video.offset + nvdrv->fb_offset) &
                                  nvdrv->fb_mask;
     __u32         dst_pitch   = buffer->video.pitch;
     __u32         src_pitch   = nvdev->src_pitch ? : 32;
     __u32         depth_pitch = nvdev->depth_pitch ? : 64;
     
     if (nvdev->dst_format != buffer->format) { 
          __u32 sformat2D = 0;
          __u32 sformat3D = 0;
          __u32 cformat   = 0;

          nvdev->dst_422 = false;

          switch (buffer->format) {
               case DSPF_ARGB1555:
                    sformat2D = 0x00000002;
                    sformat3D = 0x00000101;
                    cformat   = 0x00000602;
                    break;
               case DSPF_RGB16:
                    sformat2D = 0x00000004;
                    sformat3D = 0x00000103;
                    cformat   = 0x00000C02;
                    break;
               case DSPF_RGB32:
                    sformat2D = 0x00000006;
                    sformat3D = 0x00000106;
                    cformat   = 0x00000E02;
                    break;
               case DSPF_ARGB:
                    sformat2D = 0x0000000A;
                    sformat3D = 0x00000108;
                    cformat   = 0x00000D02;
                    break;
               case DSPF_YUY2:
                    sformat2D = 0x0000000A;
                    sformat3D = 0x00000108;
                    cformat   = 0x00001200;
                    nvdev->dst_422 = true;
                    break;
               case DSPF_UYVY:
                    sformat2D = 0x0000000A;
                    sformat3D = 0x00000108;
                    cformat   = 0x00001300;
                    nvdev->dst_422 = true;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    return;
          }

          nv_waitidle( nvdrv, nvdev );
          
          nv_out32( nvdrv->PRAMIN, (ADDR_RECTANGLE << 4) + 4, cformat );
          nv_out32( nvdrv->PRAMIN, (ADDR_TRIANGLE  << 4) + 4, cformat );
          nv_out32( nvdrv->PRAMIN, (ADDR_LINE      << 4) + 4, cformat );
          
          nv_waitfifo( nvdev, &nvdrv->Fifo->sub[0], 3 );
          nvdrv->Fifo->sub[2].SetObject = OBJ_RECTANGLE;
          nvdrv->Fifo->sub[3].SetObject = OBJ_TRIANGLE;
          nvdrv->Fifo->sub[4].SetObject = OBJ_LINE;
          
          nv_assign_object( nvdrv, nvdev, 0, OBJ_SURFACES2D );
          
          nv_waitfifo( nvdev, subchannelof(Surfaces2D), 3 );
          Surfaces2D->Format     = sformat2D;
          Surfaces2D->Pitch      = (dst_pitch << 16) | (src_pitch & 0xFFFF);
          Surfaces2D->DestOffset = dst_offset;

          if (nvdev->enabled_3d) {
               nv_assign_object( nvdrv, nvdev, 0, OBJ_SURFACES3D );
               
               nv_waitfifo( nvdev, subchannelof(Surfaces3D), 3 );
               Surfaces3D->Format       = sformat3D;
               Surfaces3D->Pitch        = (depth_pitch << 16) |
                                          (dst_pitch & 0xFFFF);
               Surfaces3D->RenderOffset = dst_offset;
          }
          
          nvdev->dst_format = buffer->format;
          nvdev->dst_offset = dst_offset;
          nvdev->dst_pitch  = dst_pitch;
     }
     else if (nvdev->dst_offset != dst_offset ||
              nvdev->dst_pitch  != dst_pitch ) 
     {
          nv_assign_object( nvdrv, nvdev, 0, OBJ_SURFACES2D );
          
          nv_waitfifo( nvdev, subchannelof(Surfaces2D), 2 );
          Surfaces2D->Pitch      = (dst_pitch << 16) | (src_pitch & 0xFFFF);
          Surfaces2D->DestOffset = dst_offset;

          if (nvdev->enabled_3d) {
               nv_assign_object( nvdrv, nvdev, 0, OBJ_SURFACES3D );
               
               nv_waitfifo( nvdev, subchannelof(Surfaces3D), 2 );
               Surfaces3D->Pitch        = (depth_pitch << 16) |
                                          (dst_pitch & 0xFFFF);
               Surfaces3D->RenderOffset = dst_offset;
          }

          nvdev->dst_offset = dst_offset;
          nvdev->dst_pitch  = dst_pitch;
     }
}

static void
nv_set_source( NVidiaDriverData *nvdrv,
               NVidiaDeviceData *nvdev,
               SurfaceBuffer    *buffer )
{
     NVSurfaces2D *Surfaces2D = nvdrv->Surfaces2D;

     if (buffer->policy == CSP_SYSTEMONLY) {
          switch (buffer->format) {
               case DSPF_ARGB1555:
                    nvdev->system_format = 2;
                    break;
               case DSPF_RGB16:
                    nvdev->system_format = 1;
                    break;
               case DSPF_RGB32:
                    nvdev->system_format = 5;
                    break;
               case DSPF_ARGB:
                    nvdev->system_format = nvdev->src_alpha ? 4 : 5;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    return;
          }

          nv_assign_object( nvdrv, nvdev, 5, OBJ_IMAGEBLT );
          nv_assign_object( nvdrv, nvdev, 6, OBJ_STRETCHEDIMAGE );
          
          nvdev->src_offset  = -1;
          nvdev->src_address = buffer->system.addr;
          nvdev->src_pitch   = buffer->system.pitch;
          nvdev->src_format  = buffer->format;
     }
     else {
          __u32 src_offset = (buffer->video.offset + nvdrv->fb_offset) &
                              nvdrv->fb_mask;
          __u32 src_pitch  = buffer->video.pitch;
     
          if (nvdev->src_offset != src_offset ||
              nvdev->src_pitch  != src_pitch )
          {
               nv_assign_object( nvdrv, nvdev, 0, OBJ_SURFACES2D );
               
               nv_waitfifo( nvdev, subchannelof(Surfaces2D), 2 );
               Surfaces2D->Pitch        = (nvdev->dst_pitch << 16) |
                                          (src_pitch & 0xFFFF);
               Surfaces2D->SourceOffset = src_offset;
          }

          switch (buffer->format) {
               case DSPF_ARGB1555:
                    nvdev->video_format = 2;
                    break;
               case DSPF_RGB16:
                    nvdev->video_format = 7;
                    break;
               case DSPF_RGB32:
                    nvdev->video_format = 4;
                    break;
               case DSPF_ARGB:
                    nvdev->video_format = nvdev->src_alpha ? 3 : 4;
                    break;
               case DSPF_YUY2:
                    nvdev->video_format = nvdev->dst_422   ? 3 : 5;
                    break;
               case DSPF_UYVY:
                    nvdev->video_format = nvdev->dst_422   ? 3 : 6;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    return;
          }

          nv_assign_object( nvdrv, nvdev, 5, OBJ_SCREENBLT );
          nv_assign_object( nvdrv, nvdev, 6, OBJ_SCALEDIMAGE );
                    
          nvdev->src_offset  = src_offset;
          nvdev->src_address = NULL;
          nvdev->src_pitch   = src_pitch;
          nvdev->src_format  = buffer->format;
          nvdev->src_width   = (buffer->surface->width  + 1) & ~1;
          nvdev->src_height  = (buffer->surface->height + 1) & ~1;

          if (nvdev->dst_422)
               nvdev->src_width = ((nvdev->src_width>>1) + 1) & ~1;
     }
}

static inline void
nv_set_depth( NVidiaDriverData *nvdrv,
              NVidiaDeviceData *nvdev,
              SurfaceBuffer    *buffer )       
{
     NVSurfaces3D *Surfaces3D   = nvdrv->Surfaces3D;
     __u32         depth_offset = (buffer->video.offset + nvdrv->fb_offset) &
                                   nvdrv->fb_mask;
     __u32         depth_pitch  = buffer->video.pitch;

     if (nvdev->depth_offset != depth_offset ||
         nvdev->depth_pitch  != depth_pitch )
     {
          nv_assign_object( nvdrv, nvdev, 0, OBJ_SURFACES3D );
          
          nv_waitfifo( nvdev, subchannelof(Surfaces3D), 2 );
          Surfaces3D->Pitch       = (depth_pitch << 16) |
                                    (nvdev->dst_pitch & 0xFFFF);
          Surfaces3D->DepthOffset = depth_offset;

          nvdev->depth_offset = depth_offset;
          nvdev->depth_pitch  = depth_pitch;
     }
}

static inline void
nv_set_clip( NVidiaDriverData *nvdrv,
             NVidiaDeviceData *nvdev,
             DFBRegion        *clip )
{
     NVClip       *Clip = nvdrv->Clip;
     DFBRectangle *cr   = &nvdev->clip;

     cr->x = clip->x1;
     cr->y = clip->y1;
     cr->w = clip->x2 - clip->x1 + 1;
     cr->h = clip->y2 - clip->y1 + 1;

     if (nvdev->dst_422) {
          cr->x =  cr->x / 2;
          cr->w = (cr->w / 2) ? : 1;
     }

     nv_waitfifo( nvdev, subchannelof(Clip), 2 );
     Clip->TopLeft     = (cr->y << 16) | (cr->x & 0xFFFF);
     Clip->WidthHeight = (cr->h << 16) | (cr->w & 0xFFFF);
}

static inline void
nv_set_color( NVidiaDriverData *nvdrv,
              NVidiaDeviceData *nvdev,
              DFBColor         *color )
{
     switch (nvdev->dst_format) {
          case DSPF_ARGB1555:
               nvdev->color = PIXEL_ARGB1555( color->a,
                                              color->r,
                                              color->g,
                                              color->b );
               break;
          case DSPF_RGB16:
               nvdev->color = PIXEL_RGB16( color->r,
                                           color->g,
                                           color->b );
               break;
          case DSPF_RGB32:
               nvdev->color = PIXEL_RGB32( color->r,
                                           color->g,
                                           color->b );
               break;
          case DSPF_ARGB:
               nvdev->color = PIXEL_ARGB( color->a,
                                          color->r,
                                          color->g,
                                          color->b );
               break;
          
          case DSPF_YUY2: {
               int y, cb, cr;
               RGB_TO_YCBCR( color->r, color->g, color->b, y, cb, cr );
               nvdev->color = PIXEL_YUY2( y, cb, cr );
          }    break;
          
          case DSPF_UYVY: {
               int y, cb, cr;
               RGB_TO_YCBCR( color->r, color->g, color->b, y, cb, cr );
               nvdev->color = PIXEL_UYVY( y, cb, cr );
          }    break;

          default:
               D_BUG( "unexpected pixelformat" );
               break;
     }

     nvdev->color3d = PIXEL_ARGB( color->a,
                                  color->r,
                                  color->g,
                                  color->b );
}

static inline void
nv_set_beta1( NVidiaDriverData *nvdrv,
              NVidiaDeviceData *nvdev,
              __u8              value )
{
     NVBeta1 *Beta1 = nvdrv->Beta1;

     if (nvdev->alpha != value) {
          nv_assign_object( nvdrv, nvdev, 0, OBJ_BETA1 );
          
          nv_waitfifo( nvdev, subchannelof(Beta1), 1 );
          Beta1->SetBeta1D31 = value << 23;

          nvdev->alpha = value;
     }
}

static inline void
nv_set_beta4( NVidiaDriverData *nvdrv,
              NVidiaDeviceData *nvdev,
              __u32             value )
{
     NVBeta4 *Beta4 = nvdrv->Beta4;
     
     nv_assign_object( nvdrv, nvdev, 0, OBJ_BETA4 );
     
     nv_waitfifo( nvdev, subchannelof(Beta4), 2 );
     Beta4->SetBetaFactor = value;
}



static void nvAfterSetVar( void *driver_data,
                           void *device_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;
     NVFifoChannel    *Fifo  = nvdrv->Fifo;
     int               i;
     
     if (nvdrv->arch == NV_ARCH_10) {
          volatile __u8* PGRAPH = nvdrv->PGRAPH;

          for (i = 0; i < 8; i++) {
               /* NV_PGRAPH_WINDOWCLIP_HORIZONTAL */
               nv_out32( PGRAPH, 0x0F00+i*4, 0x07FF0800 );
               /* NV_PGRAPH_WINDOWCLIP_VERTICAL */
               nv_out32( PGRAPH, 0x0F20+i*4, 0x07FF0800 );
          }

          /* NV_PGRAPH_XFMODE0 */
          nv_out32( PGRAPH, 0x0F40, 0x10000000 );
          /* NV_PGRAPH_XFMODE1 */
          nv_out32( PGRAPH, 0x0F44, 0x00000000 );

          /* NV_PGRAPH_PIPE_ADDRESS */
          nv_out32( PGRAPH, 0x0F50, 0x00006740 );
          /* NV_PGRAPH_PIPE_DATA */
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006750 );
          nv_out32( PGRAPH, 0x0F54, 0x40000000 );
          nv_out32( PGRAPH, 0x0F54, 0x40000000 );
          nv_out32( PGRAPH, 0x0F54, 0x40000000 );
          nv_out32( PGRAPH, 0x0F54, 0x40000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006760 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006770 );
          nv_out32( PGRAPH, 0x0F54, 0xC5000000 );
          nv_out32( PGRAPH, 0x0F54, 0xC5000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006780 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x000067A0 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006AB0 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006AC0 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006C10 );
          nv_out32( PGRAPH, 0x0F54, 0xBF800000 );
          
          for (i = 0; i < 8; i++) {
               nv_out32( PGRAPH, 0x0F50, 0x7030+i*16 );
               nv_out32( PGRAPH, 0x0F54, 0x7149F2CA );
          }

          nv_out32( PGRAPH, 0x0F50, 0x00006A80 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006AA0 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00000040 );
          nv_out32( PGRAPH, 0x0F54, 0x00000005 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006400 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x4B7FFFFF );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006410 );
          nv_out32( PGRAPH, 0x0F54, 0xC5000000 );
          nv_out32( PGRAPH, 0x0F54, 0xC5000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006420 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x00006430 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x000064C0 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          nv_out32( PGRAPH, 0x0F54, 0x477FFFFF );
          nv_out32( PGRAPH, 0x0F54, 0x3F800000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x000064D0 );
          nv_out32( PGRAPH, 0x0F54, 0xC5000000 );
          nv_out32( PGRAPH, 0x0F54, 0xC5000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x000064E0 );
          nv_out32( PGRAPH, 0x0F54, 0xC4FFF000 );
          nv_out32( PGRAPH, 0x0F54, 0xC4FFF000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          nv_out32( PGRAPH, 0x0F50, 0x000064F0 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          nv_out32( PGRAPH, 0x0F54, 0x00000000 );
          
          /* NV_PGRAPH_XFMODE0 */
          nv_out32( PGRAPH, 0x0F40, 0x30000000 );
          /* NV_PGRAPH_XFMODE1 */
          nv_out32( PGRAPH, 0x0F44, 0x00000004 );
          /* NV_PGRAPH_GLOBALSTATE0 */
          nv_out32( PGRAPH, 0x0F48, 0x10000000 );
          /* NV_PGRAPH_GLOBALSTATE1 */
          nv_out32( PGRAPH, 0x0F4C, 0x00000000 );
     }
 
     /* put objects into subchannels */
     for (i = 0; i < 8; i++)
          Fifo->sub[i].SetObject = nvdev->subchannel_object[i];
     /* reset fifo space counter */
     nvdev->fifo_space = Fifo->sub[0].Free >> 2;
     
     nvdev->reloaded        |= SMF_DESTINATION | SMF_CLIP;
     nvdev->dst_format       = DSPF_UNKNOWN;
     nvdev->src_pitch        = 0;
     nvdev->depth_pitch      = 0;
     nvdev->state3d.modified = true;
}

static void nvEngineSync( void *drv, void *dev )
{
     nv_waitidle( (NVidiaDriverData*) drv, (NVidiaDeviceData*) dev );
}


static void nv4CheckState( void *drv, void *dev,
                           CardState *state, DFBAccelerationMask accel )
{
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;

     (void) nvdev;
     
     switch (destination->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
          
          case DSPF_YUY2:
          case DSPF_UYVY:
               if (DFB_BLITTING_FUNCTION( accel )) {
                    if (accel & ~(DFXL_BLIT | DFXL_STRETCHBLIT) ||
                        state->blittingflags != DSBLIT_NOFX     || 
                        source->format != destination->format)
                         return;
               } else {
                    if (accel & (DFXL_FILLTRIANGLE | DFXL_DRAWLINE) ||
                        state->drawingflags != DSDRAW_NOFX)
                         return;
               }
               break;

          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* check unsupported blitting flags */
          if (state->blittingflags & ~NV4_SUPPORTED_BLITTINGFLAGS)
               return;

          if (accel == DFXL_TEXTRIANGLES) {
               if (source->width > 512 || source->height > 512)
                    return;
          } else
          if (state->blittingflags & DSBLIT_ALPHABLEND) {
               if (state->src_blend != DSBF_SRCALPHA   ||
                   state->dst_blend != DSBF_INVSRCALPHA)
                    return;
          }

          switch (source->format) {
               case DSPF_ARGB1555:
                    if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                         return;
                    break;
                    
               case DSPF_RGB32:
               case DSPF_ARGB:
                    break;
                    
               case DSPF_RGB16:
                    if (accel == DFXL_BLIT  && 
                       (destination->format != DSPF_RGB16 ||
                        state->blittingflags != DSBLIT_NOFX))
                         return;
                    else if (accel == DFXL_STRETCHBLIT)
                         return;
                    break;
                    
               case DSPF_YUY2:
               case DSPF_UYVY:
                    if (accel == DFXL_TEXTRIANGLES)
                         return;
                    break;
               
               default:
                    return;
          }

          state->accel |= accel;
     }
     else {
          /* check unsupported drawing flags */
          if (state->drawingflags & ~NV4_SUPPORTED_DRAWINGFLAGS)
               return;

          state->accel |= accel;
     }
}

static void nv5CheckState( void *drv, void *dev,
                           CardState *state, DFBAccelerationMask accel )
{
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;

     (void) nvdev;

     switch (destination->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32: 
          case DSPF_ARGB:
               break;

          case DSPF_YUY2:
          case DSPF_UYVY:
               if (DFB_BLITTING_FUNCTION( accel )) {
                    if (accel & ~(DFXL_BLIT | DFXL_STRETCHBLIT) ||
                        state->blittingflags != DSBLIT_NOFX     || 
                        source->format != destination->format)
                         return;
               } else {
                    if (accel & (DFXL_FILLTRIANGLE | DFXL_DRAWLINE) ||
                        state->drawingflags != DSDRAW_NOFX)
                         return;
               }
               break;
       
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* check unsupported blitting flags */
          if (state->blittingflags & ~NV5_SUPPORTED_BLITTINGFLAGS)
               return;

          if (accel == DFXL_TEXTRIANGLES) {
               if (source->width > 512 || source->height > 512)
                    return;
          } else
          if (state->blittingflags & DSBLIT_ALPHABLEND) {
               if (state->src_blend != DSBF_SRCALPHA       ||
                   state->dst_blend != DSBF_INVSRCALPHA    ||
                   (state->blittingflags & DSBLIT_COLORIZE))
                    return;
          }
          
          switch (source->format) {
               case DSPF_ARGB1555:
                    if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                         return;
                    break;
                    
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:     
                    break;
               
               case DSPF_YUY2:
               case DSPF_UYVY:
                    if (accel == DFXL_TEXTRIANGLES || 
                        source->front_buffer->policy == CSP_SYSTEMONLY)
                         return;
                    break;
               
               default:
                    return;
          }

          state->accel |= accel;
     }
     else {
          /* check unsupported drawing flags */
          if (state->drawingflags & ~NV5_SUPPORTED_DRAWINGFLAGS)
               return;

          state->accel |= accel;
     }
}

static void nv20CheckState( void *drv, void *dev,
                            CardState *state, DFBAccelerationMask accel )
{
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;

     (void) nvdev;

     switch (destination->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;

          case DSPF_YUY2:
          case DSPF_UYVY:
               if (DFB_BLITTING_FUNCTION( accel )) {
                    if (accel & ~(DFXL_BLIT | DFXL_STRETCHBLIT) ||
                        state->blittingflags != DSBLIT_NOFX     || 
                        source->format != destination->format)
                         return;
               } else {
                    if (accel & (DFXL_FILLTRIANGLE | DFXL_DRAWLINE) ||
                        state->drawingflags != DSDRAW_NOFX)
                         return;
               }
               break;
          
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* check unsupported blitting functions/flags */
          if ((accel & ~NV20_SUPPORTED_BLITTINGFUNCTIONS) ||
              (state->blittingflags & ~NV20_SUPPORTED_BLITTINGFLAGS))
               return;

          /* can't do modulation */
          if (state->blittingflags & DSBLIT_ALPHABLEND) {
               if (state->src_blend != DSBF_SRCALPHA       ||
                   state->dst_blend != DSBF_INVSRCALPHA    ||
                   (state->blittingflags & DSBLIT_COLORIZE))
                    return;
          }
          
          switch (source->format) {
               case DSPF_ARGB1555:
                    if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                         return;
                    break;
                    
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    break;
                    
               case DSPF_YUY2:
               case DSPF_UYVY:
                    if (source->front_buffer->policy == CSP_SYSTEMONLY)
                         return;
                    break;
               
               default:
                    return;
          }

          state->accel |= accel;
     }
     else {
          /* check unsupported drawing flags */
          if (state->drawingflags & ~NV20_SUPPORTED_DRAWINGFLAGS)
               return;

          if (state->drawingflags & DSDRAW_BLEND &&
              state->src_blend != DSBF_SRCALPHA  &&
              state->dst_blend != DSBF_INVSRCALPHA)
               return;

          state->accel |= accel;
     }
}


static void nv4SetState( void *drv, void *dev,
                         GraphicsDeviceFuncs *funcs,
                         CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;
     __u32             modify = state->modified;

     if (nvdev->reloaded) {
          modify |= nvdev->reloaded;
          nvdev->reloaded = 0;
     }

     if (modify & SMF_DESTINATION)
          nv_set_destination( nvdrv, nvdev,
                              state->destination->back_buffer );

     if (modify & (SMF_CLIP | SMF_DESTINATION))
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (modify & (SMF_COLOR | SMF_DESTINATION))
          nv_set_color( nvdrv, nvdev, &state->color );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               if (state->drawingflags & DSDRAW_BLEND) {
                    nvdev->state3d.modified = true;
                    nvdev->state3d.offset   = nvdev->col_offset;
                    nvdev->state3d.format   = 0x111115A1; // 2x2 RGB16
                    nvdev->state3d.blend    = (state->dst_blend << 28) |
                                              (state->src_blend << 24) |
                                              0x00100164;
                    nvdev->state3d.control &= 0xFFFFBFFF;

                    funcs->FillRectangle = nvFillRectangle3D;
                    funcs->FillTriangle  = nvFillTriangle3D;
                    funcs->DrawRectangle = nvDrawRectangle3D;
                    funcs->DrawLine      = nvDrawLine3D;
               } else {
                    funcs->FillRectangle = nvFillRectangle2D;
                    funcs->FillTriangle  = nvFillTriangle2D;
                    funcs->DrawRectangle = nvDrawRectangle2D;
                    funcs->DrawLine      = nvDrawLine2D;
               }
               
               state->set |= DFXL_FILLRECTANGLE |
                             DFXL_FILLTRIANGLE  |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT: {
               __u32 operation = 0;
               
               nvdev->src_alpha = false;

               switch (state->blittingflags) {
                    case DSBLIT_ALPHABLEND:
                         operation = 2;
                         nvdev->src_alpha = true;
                         nv_set_beta1( nvdrv, nvdev, state->color.a );
                         break;
                    case DSBLIT_BLEND_ALPHACHANNEL:
                         operation = 2;
                         nvdev->src_alpha = true;
                         nv_set_beta1( nvdrv, nvdev, 0xFF );
                         break;
                    case DSBLIT_BLEND_COLORALPHA:
                         operation = 2;
                         nv_set_beta1( nvdrv, nvdev, state->color.a );
                         break;
                    case DSBLIT_NOFX:
                         break;
                    default:
                         D_BUG( "unexpected blittingflags" );
                         break;
               }
               
               nv_set_source( nvdrv, nvdev,
                              state->source->front_buffer );
               
               if (nvdev->bop0 != operation) {
                    nv_waitfifo( nvdev, subchannelof(nvdrv->ScaledImage), 1 );
                    nvdrv->ScaledImage->SetOperation = operation;

                    nvdev->bop0 = operation;
               }

               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
          }    break;

          case DFXL_TEXTRIANGLES:
               nvdev->src_width  = state->source->width;
               nvdev->src_height = state->source->height;

               nvdev->state3d.modified = true;
               nvdev->state3d.offset   = nvdev->tex_offset;
               
               if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                    nvdev->state3d.format = 0x119914A1; // 512x512 ARGB4444
               else
                    nvdev->state3d.format = 0x119915A1; // 512x512 RGB16
               
               if (state->blittingflags != DSBLIT_NOFX) {
                    nvdev->state3d.blend  = (state->dst_blend << 28) |
                                            (state->src_blend << 24);
                    
                    if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
                         nvdev->state3d.blend |= 0x00000164;
                    else
                         nvdev->state3d.blend |= 0x00000162;

                    if (state->blittingflags & DSBLIT_ALPHABLEND)
                         nvdev->state3d.blend |= 0x00100000;
               } else
                    nvdev->state3d.blend = 0x12000167;
               
               if (state->destination->caps & DSCAPS_DEPTH) {
                    nv_set_depth( nvdrv, nvdev,
                                  state->destination->depth_buffer );
                    nvdev->state3d.control |= 0x00004000;
               } else
                    nvdev->state3d.control &= 0xFFFFBFFF;

               /* copy/convert source surface to texture buffer */
               nv_put_texture( nvdrv, nvdev, state->source->front_buffer );
               
               state->set |= DFXL_TEXTRIANGLES;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }
     
     state->modified = 0;
}

static void nv5SetState( void *drv, void *dev,
                         GraphicsDeviceFuncs *funcs,
                         CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;
     __u32             modify = state->modified;

     if (nvdev->reloaded) {
          modify |= nvdev->reloaded;
          nvdev->reloaded = 0;
     }

     if (modify & SMF_DESTINATION)
          nv_set_destination( nvdrv, nvdev,
                              state->destination->back_buffer );
     
     if (modify & (SMF_CLIP | SMF_DESTINATION))
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (modify & (SMF_COLOR | SMF_DESTINATION))
          nv_set_color( nvdrv, nvdev, &state->color );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               if (state->drawingflags & DSDRAW_BLEND) {
                    nvdev->state3d.modified = true;
                    nvdev->state3d.offset   = nvdev->col_offset;
                    nvdev->state3d.format   = 0x111115A1; // 2x2 RGB16 
                    nvdev->state3d.blend    = (state->dst_blend << 28) |
                                              (state->src_blend << 24) |
                                              0x00100164;
                    nvdev->state3d.control &= 0xFFFFBFFF;

                    funcs->FillRectangle = nvFillRectangle3D;
                    funcs->FillTriangle  = nvFillTriangle3D;
                    funcs->DrawRectangle = nvDrawRectangle3D;
                    funcs->DrawLine      = nvDrawLine3D;
               } else {
                    funcs->FillRectangle = nvFillRectangle2D;
                    funcs->FillTriangle  = nvFillTriangle2D;
                    funcs->DrawRectangle = nvDrawRectangle2D;
                    funcs->DrawLine      = nvDrawLine2D;
               }
               
               state->set |= DFXL_FILLRECTANGLE |
                             DFXL_FILLTRIANGLE  |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT: {
               __u32 operation = 3;
               
               nvdev->src_alpha = true;
               
               switch (state->blittingflags) {
                    case DSBLIT_ALPHABLEND:
                         operation = 2;
                         nv_set_beta1( nvdrv, nvdev, state->color.a );
                         break;
                    case DSBLIT_BLEND_ALPHACHANNEL:
                         operation = 2;
                         nv_set_beta1( nvdrv, nvdev, 0xFF );
                         break;
                    case DSBLIT_BLEND_COLORALPHA:
                         operation = 2;
                         nvdev->src_alpha = false;
                         nv_set_beta1( nvdrv, nvdev, state->color.a );
                         break;
                    case DSBLIT_COLORIZE:
                         operation = 4;
                         nv_set_beta4( nvdrv, nvdev, nvdev->color3d );
                         break;
                    case DSBLIT_NOFX:
                         break;
                    default:
                         D_BUG( "unexpected blittingflags" );
                         break;
               }

               nv_set_source( nvdrv, nvdev,
                              state->source->front_buffer );

               if (state->source->front_buffer->policy == CSP_SYSTEMONLY) {
                    if (nvdev->bop1 != operation) {
                         nv_waitfifo( nvdev, subchannelof(nvdrv->ImageBlt), 1 );
                         nvdrv->ImageBlt->SetOperation       = operation;

                         nv_waitfifo( nvdev, subchannelof(nvdrv->StretchedImage), 1 );
                         nvdrv->StretchedImage->SetOperation = operation;

                         nvdev->bop1 = operation;
                    }
                    
                    funcs->Blit        = nvDMABlit;
                    funcs->StretchBlit = nvDMAStretchBlit;
               }
               else {
                    if (nvdev->bop0 != operation) {
                         nv_waitfifo( nvdev, subchannelof(nvdrv->ScaledImage), 1 );
                         nvdrv->ScaledImage->SetOperation = operation;

                         nvdev->bop0 = operation;
                    }
                    
                    funcs->Blit        = nvBlit;
                    funcs->StretchBlit = nvStretchBlit;
               }
               
               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
          }    break;

          case DFXL_TEXTRIANGLES:
               nvdev->src_width  = state->source->width;
               nvdev->src_height = state->source->height;

               nvdev->state3d.modified = true;
               nvdev->state3d.offset   = nvdev->tex_offset;
               
               if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                    nvdev->state3d.format = 0x119914A1; // 512x512 ARGB4444
               else
                    nvdev->state3d.format = 0x119915A1; // 512x512 RGB16
               
               if (state->blittingflags != DSBLIT_NOFX) {
                    nvdev->state3d.blend  = (state->dst_blend << 28) |
                                            (state->src_blend << 24);
                    
                    if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
                         nvdev->state3d.blend |= 0x00000164;
                    else
                         nvdev->state3d.blend |= 0x00000162;

                    if (state->blittingflags & DSBLIT_ALPHABLEND)
                         nvdev->state3d.blend |= 0x00100000;

                    if (!(state->blittingflags & DSBLIT_COLORIZE)) {
                         nvdev->color3d  |= 0x00FFFFFF;
                         nvdev->reloaded |= SMF_COLOR;
                    }
               } else
                    nvdev->state3d.blend = 0x12000167;

               if (state->destination->caps & DSCAPS_DEPTH) {
                    nv_set_depth( nvdrv, nvdev, 
                                  state->destination->depth_buffer );
                    nvdev->state3d.control |= 0x00004000;
               } else
                    nvdev->state3d.control &= 0xFFFFBFFF;

               /* copy/convert source surface to texture buffer */
               nv_put_texture( nvdrv, nvdev, state->source->front_buffer );
 
               state->set |= DFXL_TEXTRIANGLES;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }
         
     state->modified = 0;
}

static void nv20SetState( void *drv, void *dev,
                          GraphicsDeviceFuncs *funcs,
                          CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;
     __u32             modify = state->modified;

     if (nvdev->reloaded) {
          modify |= nvdev->reloaded;
          nvdev->reloaded = 0;
     }

     if (modify & SMF_DESTINATION)
          nv_set_destination( nvdrv, nvdev,
                              state->destination->back_buffer );
          
     if (modify & (SMF_CLIP | SMF_DESTINATION))
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (modify & (SMF_COLOR | SMF_DESTINATION))
          nv_set_color( nvdrv, nvdev, &state->color );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE: {
               __u32 operation = 3;
               
               if (state->drawingflags & DSDRAW_BLEND) {
                    operation = 2;
                    nv_set_beta1( nvdrv, nvdev, state->color.a );
               }
              
               if (nvdev->dop != operation) {
                    nv_waitfifo( nvdev, subchannelof(nvdrv->Rectangle), 1 );
                    nvdrv->Rectangle->SetOperation = operation;

                    nv_waitfifo( nvdev, subchannelof(nvdrv->Triangle), 1 );
                    nvdrv->Triangle->SetOperation  = operation;

                    nv_waitfifo( nvdev, subchannelof(nvdrv->Line), 1 );
                    nvdrv->Line->SetOperation      = operation;
               
                    nvdev->dop = operation;
               }
               
               state->set |= DFXL_FILLRECTANGLE |
                             DFXL_FILLTRIANGLE  |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE;
          }    break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT: {
               __u32 operation = 3;
               
               nvdev->src_alpha = true;
               
               switch (state->blittingflags) {
                    case DSBLIT_ALPHABLEND:
                         operation = 2;
                         nv_set_beta1( nvdrv, nvdev, state->color.a );
                         break;
                    case DSBLIT_BLEND_ALPHACHANNEL:
                         operation = 2;
                         nv_set_beta1( nvdrv, nvdev, 0xFF );
                         break;
                    case DSBLIT_BLEND_COLORALPHA:
                         operation = 2;
                         nvdev->src_alpha = false;
                         nv_set_beta1( nvdrv, nvdev, state->color.a );
                         break;
                    case DSBLIT_COLORIZE:
                         operation = 4;
                         nv_set_beta4( nvdrv, nvdev, nvdev->color3d );
                         break;
                    case DSBLIT_NOFX:
                         break;
                    default:
                         D_BUG( "unexpected blittingflags" );
                         break;
               }

               nv_set_source( nvdrv, nvdev,
                              state->source->front_buffer );

               if (state->source->front_buffer->policy == CSP_SYSTEMONLY) {                    
                    if (nvdev->bop1 != operation) {
                         nv_waitfifo( nvdev, subchannelof(nvdrv->ImageBlt), 1 );
                         nvdrv->ImageBlt->SetOperation       = operation;

                         nv_waitfifo( nvdev, subchannelof(nvdrv->StretchedImage), 1 );
                         nvdrv->StretchedImage->SetOperation = operation;

                         nvdev->bop1 = operation;
                    }
                    
                    funcs->Blit        = nvDMABlit;
                    funcs->StretchBlit = nvDMAStretchBlit;
               }
               else {
                    if (nvdev->bop0 != operation) {
                         nv_waitfifo( nvdev, subchannelof(nvdrv->ScaledImage), 1 );
                         nvdrv->ScaledImage->SetOperation = operation;

                         nvdev->bop0 = operation;
                    }
                    
                    funcs->Blit        = nvBlit;
                    funcs->StretchBlit = nvStretchBlit;
               }
               
               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
          }    break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     state->modified = 0;
}


/* exported symbols */

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_NV4:
          case FB_ACCEL_NV5:
               return 1;
     }

     return 0;
}

static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "nVidia RivaTNT/RivaTNT2/GeForce Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 4;

     info->driver_data_size = sizeof (NVidiaDriverData);
     info->device_data_size = sizeof (NVidiaDeviceData);
}

static void
nv_find_architecture( NVidiaDriverData *nvdrv )
{
     char  buf[512];
     __u32 chip  = 0;
     __u32 arch  = 0;

#ifdef USE_SYSFS
     if (!sysfs_get_mnt_path( buf, 512 )) {
          struct dlist           *devices;
          struct sysfs_device    *dev;
          struct sysfs_attribute *attr;
          int                     bus;

          devices = sysfs_open_bus_devices_list( "pci" );
          if (devices) {
               dlist_for_each_data( devices, dev, struct sysfs_device ) {
                    if (sscanf( dev->name, "0000:%02x:", &bus ) < 1 || bus < 1)
                         continue;

                    dev = sysfs_open_device( "pci", dev->name );
                    if (dev) {
                         attr = sysfs_get_device_attr( dev, "vendor" );
                         if (!attr || strncasecmp( attr->value, "0x10de", 6 )) {
                              sysfs_close_device( dev );
                              continue;
                         }

                         attr = sysfs_get_device_attr( dev, "device" );
                         if (attr)
                              sscanf( attr->value, "0x%04x", &chip );

                         sysfs_close_device( dev );
                         break;
                    }
               }

               sysfs_close_list( devices );
          }
     }
#endif /* USE_SYSFS */

     /* try /proc interface */
     if (!chip) {
          FILE  *fp;
          __u32  device;
          __u32  vendor;

          fp = fopen( "/proc/bus/pci/devices", "r" );
          if (!fp) {
               D_PERROR( "DirectFB/NVidia: "
                         "couldn't access /proc/bus/pci/devices!\n" );
               return;
          }

          while (fgets( buf, 512, fp )) {
               if (sscanf( buf, "%04x\t%04x%04x",
                              &device, &vendor, &chip ) != 3)
                    continue;
               
               if (device >= 0x0100 && vendor == 0x10DE)
                    break;
               
               chip = 0;
          }

          fclose( fp );
     }
     
     switch (chip & 0x0FF0) {
          case 0x0020: /* Riva TNT/TNT2 */
               arch = (chip == 0x0020) ? NV_ARCH_04 : NV_ARCH_05;
               break;
          case 0x0100: /* GeForce */
          case 0x0110: /* GeForce2 MX */
          case 0x0150: /* GeForce2 GTS/Ti/Ultra */
          case 0x0170: /* GeForce4 MX/Go */
          case 0x0180: /* GeForce4 MX/Go AGP8X */
          //case 0x01A0: /* GeForce2 Integrated GPU */
          //case 0x01F0: /* GeForce4 MX Integrated GPU */
               arch = NV_ARCH_10;
               break;
          case 0x0200: /* GeForce3 */
          case 0x0250: /* GeForce4 Ti */
          case 0x0280: /* GeForce4 Ti AGP8X */
          case 0x02A0: /* GeForce3 Integrated GPU (XBox) */
               arch = NV_ARCH_20;
               break;
          case 0x0300: /* GeForce FX 5800 */
          case 0x0310: /* GeForce FX 5600 */
          case 0x0320: /* GeForce FX 5200 */
          case 0x0330: /* GeForce FX 5900 */
          case 0x0340: /* GeForce FX 5700 */
               arch = NV_ARCH_30;
               break;
          default:
               break;
     }

     if (arch) {
          nvdrv->chip = chip;
          nvdrv->arch = arch;
          D_INFO( "DirectFB/NVidia: "
                  "found nVidia Architecture %02x (Chipset %04x)\n",
                  arch, chip );
     } else
          D_INFO( "DirectFB/NVidia: "
                  "Unknown or Unsupported Chipset %04x (acceleration disabled)\n",
                  chip );
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     __u32             vram  = dfb_system_videoram_length();

     nvdrv->device = device;
     
     nv_find_architecture( nvdrv );
     
     if (nvdrv->chip == 0x02A0) {
          nvdrv->fb_offset  = (__u32) dfb_gfxcard_memory_physical( device, 0 );
          nvdrv->fb_offset &= 0x0FFFFFFF;
          vram             += nvdrv->fb_offset;
     }
     nvdrv->fb_mask = ((1 << direct_log2( vram )) - 1) & ~63;
     
     nvdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!nvdrv->mmio_base)
          return DFB_IO;

     nvdrv->PVIDEO  = nvdrv->mmio_base + 0x008000;
     nvdrv->PVIO    = nvdrv->mmio_base + 0x0C0000;
     nvdrv->PFB     = nvdrv->mmio_base + 0x100000;
     nvdrv->PGRAPH  = nvdrv->mmio_base + 0x400000;
     nvdrv->PCRTC   = nvdrv->mmio_base + 0x600000;
     nvdrv->PCIO    = nvdrv->mmio_base + 0x601000;
     nvdrv->PRAMIN  = nvdrv->mmio_base + 0x700000;
     nvdrv->PRAMHT  = nvdrv->mmio_base + 0x710000;

     nvdrv->Fifo           = (NVFifoChannel*) (nvdrv->mmio_base + 0x800000);
     nvdrv->Surfaces2D     = &nvdrv->Fifo->sub[0].o.Surfaces2D;
     nvdrv->Surfaces3D     = &nvdrv->Fifo->sub[0].o.Surfaces3D;
     nvdrv->Beta1          = &nvdrv->Fifo->sub[0].o.Beta1;
     nvdrv->Beta4          = &nvdrv->Fifo->sub[0].o.Beta4;
     nvdrv->Clip           = &nvdrv->Fifo->sub[1].o.Clip;
     nvdrv->Rectangle      = &nvdrv->Fifo->sub[2].o.Rectangle;
     nvdrv->Triangle       = &nvdrv->Fifo->sub[3].o.Triangle;
     nvdrv->Line           = &nvdrv->Fifo->sub[4].o.Line;
     nvdrv->ScreenBlt      = &nvdrv->Fifo->sub[5].o.ScreenBlt;
     nvdrv->ImageBlt       = &nvdrv->Fifo->sub[5].o.ImageBlt;
     nvdrv->ScaledImage    = &nvdrv->Fifo->sub[6].o.ScaledImage;
     nvdrv->StretchedImage = &nvdrv->Fifo->sub[6].o.StretchedImage;
     nvdrv->TexTriangle    = &nvdrv->Fifo->sub[7].o.TexTriangle;

     funcs->AfterSetVar   = nvAfterSetVar;
     funcs->EngineSync    = nvEngineSync;
     funcs->FillRectangle = nvFillRectangle2D; // dynamic
     funcs->FillTriangle  = nvFillTriangle2D;  // dynamic
     funcs->DrawRectangle = nvDrawRectangle2D; // dynamic
     funcs->DrawLine      = nvDrawLine2D;      // dynamic

     switch (nvdrv->arch) {
          case NV_ARCH_04:
               funcs->CheckState       = nv4CheckState;
               funcs->SetState         = nv4SetState;
               funcs->Blit             = nvBlit;
               funcs->StretchBlit      = nvStretchBlit;
               funcs->TextureTriangles = nvTextureTriangles;
               break;
          case NV_ARCH_05:
          case NV_ARCH_10:
               funcs->CheckState       = nv5CheckState;
               funcs->SetState         = nv5SetState;
               funcs->Blit             = nvBlit;
               funcs->StretchBlit      = nvStretchBlit;
               funcs->TextureTriangles = nvTextureTriangles;
               break;
          case NV_ARCH_20:
          case NV_ARCH_30:
               funcs->CheckState       = nv20CheckState;
               funcs->SetState         = nv20SetState;
               funcs->Blit             = nvBlit;
               funcs->StretchBlit      = nvStretchBlit;
               break;
          default:
               break;
     }

     dfb_screens_register_primary( device, driver_data,
                                   &nvidiaPrimaryScreenFuncs );
     
     dfb_layers_hook_primary( device, driver_data,
                              &nvidiaPrimaryLayerFuncs, 
                              &nvidiaOldPrimaryLayerFuncs,
                              &nvidiaOldPrimaryLayerDriverData );
     
     dfb_layers_register( dfb_screens_at( DSCID_PRIMARY ),
                          driver_data, &nvidiaOverlayFuncs );

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     NVidiaDriverData *nvdrv        = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev        = (NVidiaDeviceData*) device_data;
     int               ram_unusable = 0;
     int               ram_total;
     int               ram_used;

     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "RivaTNT/RivaTNT2/GeForce" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "nVidia" );

     switch (nvdrv->arch) {
          case NV_ARCH_04:
               device_info->caps.flags    = CCF_CLIPPING;
               device_info->caps.accel    = NV4_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV4_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV4_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV4_SUPPORTED_BLITTINGFLAGS;
               break;
          case NV_ARCH_05:
          case NV_ARCH_10:
               /* FIXME: random crashes on window resizing when blitting from system memory */
               device_info->caps.flags    = CCF_CLIPPING;// | CCF_READSYSMEM;
               device_info->caps.accel    = NV5_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV5_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV5_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV5_SUPPORTED_BLITTINGFLAGS;
               break;
          case NV_ARCH_20:
          case NV_ARCH_30:
               device_info->caps.flags    = CCF_CLIPPING | CCF_READSYSMEM;
               device_info->caps.accel    = NV20_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV20_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV20_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV20_SUPPORTED_BLITTINGFLAGS;
               break;
          default:
               device_info->caps.flags    = 0;
               device_info->caps.accel    = 0;
               device_info->caps.drawing  = 0;
               device_info->caps.blitting = 0;
               break;
     }

     device_info->limits.surface_byteoffset_alignment = 64; 
     device_info->limits.surface_pixelpitch_alignment = 32;

     dfb_config->pollvsync_after = 1;

     /* reserve unusable memory to avoid random crashes */
     ram_total = 1 << direct_log2( dfb_system_videoram_length() );
     ram_used  = dfb_gfxcard_memory_length();
 
     switch (nvdrv->arch) {
          case NV_ARCH_04:
          case NV_ARCH_05:
               ram_unusable = 128 * 1024;
               break;
          case NV_ARCH_10:
          case NV_ARCH_20:
          case NV_ARCH_30:
               ram_unusable = 192 * 1024;
               break;
          default:
               break;
     }
     ram_unusable -= ram_total - ram_used;
     
     if (ram_unusable > 0) {
          int offset;
          
          offset = dfb_gfxcard_reserve_memory( nvdrv->device, ram_unusable );
          if (offset < 0) {
               D_ERROR( "DirectFB/NVidia: "
                        "couldn't reserve %i bytes of video memory.\n",
                        ram_unusable );
               return DFB_NOVIDEOMEMORY;
          }

          D_DEBUG( "DirectFB/NVidia: "
                   "reserved %i bytes of unusable video memory at offset 0x%08x.\n",
                   ram_unusable, offset );
          ram_used -= ram_unusable;
     } 
    
     /* reserve memory for texture/color buffers */
     if (device_info->caps.accel & DFXL_TEXTRIANGLES) {
          __u32 len;
          int   offset;
          
          len    = 512 * 512 * 2 + 8;
          len   += (ram_used - len) & 0xFF;
          offset = dfb_gfxcard_reserve_memory( nvdrv->device, len );
          if (offset < 0) {
               D_ERROR( "DirectFB/NVidia: "
                        "couldn't reserve %i bytes of video memory.\n", len );
               return DFB_NOVIDEOMEMORY;
          }
         
          D_DEBUG( "DirectFB/NVidia: "
                   "reserved %i bytes for 3D buffers at offset 0x%08x.\n",
                    len, offset );

          nvdev->enabled_3d = true;
          nvdev->tex_offset = offset;
          nvdev->col_offset = offset + (512 * 512 * 2);

          /* clear color buffer */
          memset( dfb_gfxcard_memory_virtual( device, nvdev->col_offset ), 0xFF, 8 );
          
          /* set default 3d state */
          nvdev->state3d.modified = true;
          nvdev->state3d.colorkey = 0;
          nvdev->state3d.filter   = 0x22000000;
          nvdev->state3d.control  = 0x40580800;
          nvdev->state3d.fog      = 0;
     }

#ifdef WORDS_BIGENDIAN
# define ENDIAN_FLAG 0x00080000
#else
# define ENDIAN_FLAG 0
#endif

     /* write dma objects configuration */
     nv_store_dma( nvdrv, OBJ_DMA, ADDR_DMA,
                   0x00, 0x00003000, ram_used + nvdrv->fb_offset,
                   0, 2 );
     
     /* write graphics objects configuration */
     nv_store_object( nvdrv, OBJ_SURFACES2D, ADDR_SURFACES2D,
                      0x42, ENDIAN_FLAG, 0, 0, 0 );
     nv_store_object( nvdrv, OBJ_CLIP, ADDR_CLIP,
                      0x19, ENDIAN_FLAG, 0, 0, 0 );
     nv_store_object( nvdrv, OBJ_BETA1, ADDR_BETA1,
                      0x12, ENDIAN_FLAG, 0, 0, 0 );
     nv_store_object( nvdrv, OBJ_BETA4, ADDR_BETA4,
                      0x72, ENDIAN_FLAG, 0, 0, 0 );
    
     nv_store_object( nvdrv, OBJ_RECTANGLE, ADDR_RECTANGLE,
                      0x5E, 0x0101A000 | ENDIAN_FLAG, 0,
                      ADDR_DMA, ADDR_DMA );
     nv_store_object( nvdrv, OBJ_TRIANGLE, ADDR_TRIANGLE,
                      0x5D, 0x0101A000 | ENDIAN_FLAG, 0,
                      ADDR_DMA, ADDR_DMA );
     nv_store_object( nvdrv, OBJ_LINE, ADDR_LINE,
                      0x5C, 0x0101A000 | ENDIAN_FLAG, 0,
                      ADDR_DMA, ADDR_DMA );         

     switch (nvdrv->arch)  {
          case NV_ARCH_04:
               nv_store_object( nvdrv, OBJ_SCREENBLT, ADDR_SCREENBLT,
                                0x1F, 0x03002000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE,
                                0x37, 0x03102000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE,
                                0x54, 0x03002000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D,
                                0x53, ENDIAN_FLAG, 0, 0, 0 );
               break;

          case NV_ARCH_05:
               nv_store_object( nvdrv, OBJ_SCREENBLT, ADDR_SCREENBLT,
                                0x5F, 0x0301A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_IMAGEBLT, ADDR_IMAGEBLT,
                                0x65, 0x0311A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE,
                                0x63, 0x0311A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_STRETCHEDIMAGE, ADDR_STRETCHEDIMAGE,
                                0x66, 0x0311A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE,
                                0x54, 0x03002000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D,
                                0x53, ENDIAN_FLAG, 0, 0, 0 );
               break;

          case NV_ARCH_10:
               nv_store_object( nvdrv, OBJ_SCREENBLT, ADDR_SCREENBLT,
                                0x5F, 0x0301A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_IMAGEBLT, ADDR_IMAGEBLT,
                                0x65, 0x0311A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE,
                                0x89, 0x0311A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_STRETCHEDIMAGE, ADDR_STRETCHEDIMAGE,
                                0x66, 0x0311A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE,
                                0x94, 0x03002000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D,
                                0x93, ENDIAN_FLAG, 0, 0, 0 );
               break;

          case NV_ARCH_20:
          case NV_ARCH_30:
          default: 
               nv_store_object( nvdrv, OBJ_SCREENBLT, ADDR_SCREENBLT,
                                0x9F, 0x0301A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_IMAGEBLT, ADDR_IMAGEBLT,
                                0x65, 0x0311A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE,
                                0x89, 0x0311A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_STRETCHEDIMAGE, ADDR_STRETCHEDIMAGE,
                                0x66, 0x0311A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE,
                                0x94, 0x03002000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D,
                                0x93, ENDIAN_FLAG, 0, 0, 0 );
               break;
     }

#undef ENDIAN_FLAG
     
     /* assign default objects to subchannels */
     nvdev->subchannel_object[0] = OBJ_SURFACES2D;
     nvdev->subchannel_object[1] = OBJ_CLIP;
     nvdev->subchannel_object[2] = OBJ_RECTANGLE;
     nvdev->subchannel_object[3] = OBJ_TRIANGLE;
     nvdev->subchannel_object[4] = OBJ_LINE;
     nvdev->subchannel_object[5] = OBJ_SCREENBLT;
     nvdev->subchannel_object[6] = OBJ_SCALEDIMAGE;
     nvdev->subchannel_object[7] = OBJ_TEXTRIANGLE;

     nvdev->dop    = 3;
     nvdev->bop0   = 3;
     nvdev->bop1   = 3;
     nvdev->filter = (nvdrv->arch == NV_ARCH_04) ? 0 : 0x01010000; 
     
     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;

     (void) nvdev;
     
     D_DEBUG( "DirectFB/NVidia: FIFO Performance Monitoring:\n" );
     D_DEBUG( "DirectFB/NVidia:  %9d nv_waitfifo calls\n",
               nvdev->waitfifo_calls );
     D_DEBUG( "DirectFB/NVidia:  %9d register writes (nv_waitfifo sum)\n",
               nvdev->waitfifo_sum );
     D_DEBUG( "DirectFB/NVidia:  %9d FIFO wait cycles (depends on CPU)\n",
               nvdev->fifo_waitcycles );
     D_DEBUG( "DirectFB/NVidia:  %9d IDLE wait cycles (depends on CPU)\n",
               nvdev->idle_waitcycles );
     D_DEBUG( "DirectFB/NVidia:  %9d FIFO space cache hits(depends on CPU)\n",
               nvdev->fifo_cache_hits );
     D_DEBUG( "DirectFB/NVidia: Conclusion:\n" );
     D_DEBUG( "DirectFB/NVidia:  Average register writes/nvidia_waitfifo"
               " call:%.2f\n",
               nvdev->waitfifo_sum/(float)(nvdev->waitfifo_calls ? : 1) );
     D_DEBUG( "DirectFB/NVidia:  Average wait cycles/nvidia_waitfifo call:"
               " %.2f\n",
               nvdev->fifo_waitcycles/(float)(nvdev->waitfifo_calls ? : 1) );
     D_DEBUG( "DirectFB/NVidia:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * nvdev->fifo_cache_hits/
               (float)(nvdev->waitfifo_calls ? : 1)) );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;

     dfb_gfxcard_unmap_mmio( device, nvdrv->mmio_base, -1 );
}

