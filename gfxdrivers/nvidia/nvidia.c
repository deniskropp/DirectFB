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

#include <core/system.h>
#include <core/state.h>
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
#include "nvidia_tables.h"


#define NV_LOAD_STATE( dest, table )                             \
{                                                                \
     int _i = 0;                                                 \
     for (; _i < sizeof(nv##table) / sizeof(nv##table[0]); _i++) \
          dest[nv##table[_i][0]] = nv##table[_i][1];             \
}


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

/* GeForce(3,4Ti) */
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

/* GeForceFX */
#define NV30_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_NOFX)

#define NV30_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE | DFXL_DRAWLINE)

#define NV30_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_NOFX)

#define NV30_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT)


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
     volatile __u32 *PRAMIN = nvdrv->PRAMIN;
     volatile __u32 *PRAMHT = nvdrv->PRAMHT;
     __u32           key    = nv_hashkey( obj );
     __u32           ctx    = addr | (0 << 16) | (1 << 31);

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
     volatile __u32 *PRAMIN = nvdrv->PRAMIN;
     volatile __u32 *PRAMHT = nvdrv->PRAMHT;
     __u32           key    = nv_hashkey( obj );
     __u32           ctx    = addr | (1 << 16) | (1 << 31);

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

static void
nv_set_destination( NVidiaDriverData      *nvdrv,
                    NVidiaDeviceData      *nvdev,
                    __u32                  offset,
                    __u32                  pitch,
                    DFBSurfacePixelFormat  format )
{
     volatile __u32   *PRAMIN      = nvdrv->PRAMIN;
     NVFifoChannel    *Fifo        = nvdrv->Fifo;
     NVFifoSubChannel *SubChannel  = subchannelof(nvdrv->Surfaces2D);
     __u32             src_pitch   = nvdev->src_pitch ? : 32;
     __u32             depth_pitch = nvdev->depth_pitch ? : 64;
     
     if (nvdev->dst_format != format) { 
          __u32 sformat2D = 0;
          __u32 sformat3D = 0;
          __u32 cformat   = 0;

          switch (format) {
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
               default:
                    D_BUG( "unexpected pixelformat" );
                    return;
          }

          nv_waitidle( nvdrv, nvdev );
          
          nv_out32( PRAMIN, (ADDR_RECTANGLE << 4) + 4, cformat );
          nv_out32( PRAMIN, (ADDR_TRIANGLE  << 4) + 4, cformat );
          nv_out32( PRAMIN, (ADDR_LINE      << 4) + 4, cformat );
        
          nv_waitfifo( nvdev, SubChannel, 3 );
          Fifo->sub[2].SetObject = OBJ_RECTANGLE;
          Fifo->sub[3].SetObject = OBJ_TRIANGLE;
          Fifo->sub[4].SetObject = OBJ_LINE;
             
          nv_waitfifo( nvdev, SubChannel, 4 );
          SubChannel->SetObject               = OBJ_SURFACES2D;
          SubChannel->o.Surfaces2D.Format     = sformat2D;
          SubChannel->o.Surfaces2D.Pitch      = (pitch << 16) | 
                                                (src_pitch & 0xFFFF);
          SubChannel->o.Surfaces2D.DestOffset = offset;

          if (nvdev->enabled_3d) {
               nv_waitfifo( nvdev, SubChannel, 4 );
               SubChannel->SetObject                 = OBJ_SURFACES3D;
               SubChannel->o.Surfaces3D.Format       = sformat3D;
               SubChannel->o.Surfaces3D.Pitch        = (depth_pitch << 16) |
                                                       (pitch & 0xFFFF);
               SubChannel->o.Surfaces3D.RenderOffset = offset;
          }
     } 
     else {          
          nv_waitfifo( nvdev, SubChannel, 3 );
          SubChannel->SetObject               = OBJ_SURFACES2D;
          SubChannel->o.Surfaces2D.Pitch      = (pitch << 16) | 
                                                (src_pitch & 0xFFFF);
          SubChannel->o.Surfaces2D.DestOffset = offset;

          if (nvdev->enabled_3d) {
               nv_waitfifo( nvdev, SubChannel, 3 );
               SubChannel->SetObject                 = OBJ_SURFACES3D;
               SubChannel->o.Surfaces3D.Pitch        = (depth_pitch << 16) |
                                                       (pitch & 0xFFFF);
               SubChannel->o.Surfaces3D.RenderOffset = offset;
          }
     }
}

static inline void
nv_set_source( NVidiaDriverData *nvdrv,
               NVidiaDeviceData *nvdev,
               __u32             offset,
               __u32             pitch )
{
     NVFifoSubChannel *SubChannel = subchannelof(nvdrv->Surfaces2D);

     nv_waitfifo( nvdev, SubChannel, 3 );
     SubChannel->SetObject                 = OBJ_SURFACES2D;
     SubChannel->o.Surfaces2D.Pitch        = (nvdev->dst_pitch << 16) |
                                             (pitch & 0xFFFF);
     SubChannel->o.Surfaces2D.SourceOffset = offset;
}

static inline void
nv_set_depth( NVidiaDriverData *nvdrv,
              NVidiaDeviceData *nvdev,
              __u32             offset,
              __u32             pitch )
{
     NVFifoSubChannel *SubChannel = subchannelof(nvdrv->Surfaces3D);

     nv_waitfifo( nvdev, SubChannel, 3 );
     SubChannel->SetObject                 = OBJ_SURFACES3D;
     SubChannel->o.Surfaces3D.Pitch        = (pitch << 16) |
                                             (nvdev->dst_pitch & 0xFFFF);
     SubChannel->o.Surfaces3D.DepthOffset = offset;
}

static inline void
nv_set_clip( NVidiaDriverData *nvdrv,
             NVidiaDeviceData *nvdev,
             DFBRegion        *clip )
{
     NVClip       *Clip = nvdrv->Clip;
     DFBRectangle *cr   = &nvdev->clip;

     nvdev->clip.x = clip->x1;
     nvdev->clip.y = clip->y1;
     nvdev->clip.w = clip->x2 - clip->x1 + 1;
     nvdev->clip.h = clip->y2 - clip->y1 + 1;

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
     NVFifoSubChannel *SubChannel = subchannelof(nvdrv->Beta1);

     nv_waitfifo( nvdev, SubChannel, 2 );
     SubChannel->SetObject           = OBJ_BETA1;
     SubChannel->o.Beta1.SetBeta1D31 = value << 23;
}

static inline void
nv_set_beta4( NVidiaDriverData *nvdrv,
              NVidiaDeviceData *nvdev,
              __u32             value )
{
     NVFifoSubChannel *SubChannel = subchannelof(nvdrv->Beta4);
     
     nv_waitfifo( nvdev, SubChannel, 2 );
     SubChannel->SetObject             = OBJ_BETA4;
     SubChannel->o.Beta4.SetBetaFactor = value;
}



static void nvAfterSetVar( void *driver_data,
                           void *device_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;
     NVFifoChannel    *Fifo  = nvdrv->Fifo;   
     
     if (nvdrv->arch == NV_ARCH_10)
          NV_LOAD_STATE( nvdrv->PGRAPH, PGRAPH10 )
 
     /* put objects into subchannels */
     Fifo->sub[0].SetObject = OBJ_SURFACES2D;
     Fifo->sub[1].SetObject = OBJ_CLIP;
     Fifo->sub[2].SetObject = OBJ_RECTANGLE;
     Fifo->sub[3].SetObject = OBJ_TRIANGLE;
     Fifo->sub[4].SetObject = OBJ_LINE;
     Fifo->sub[5].SetObject = OBJ_SCREENBLT;
     Fifo->sub[6].SetObject = OBJ_SCALEDIMAGE;
     Fifo->sub[7].SetObject = OBJ_TEXTRIANGLE;

     nvdev->reloaded   |= SMF_DESTINATION | SMF_CLIP;
     nvdev->dst_format  = DSPF_UNKNOWN;
     nvdev->src_pitch   = 0;
     nvdev->depth_pitch = 0;
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
               case DSPF_YUY2:
               case DSPF_UYVY:
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

static void nv30CheckState( void *drv, void *dev,
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
          
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* check unsupported blitting functions/flags */
          if ((accel & ~NV30_SUPPORTED_BLITTINGFUNCTIONS) ||
              (state->blittingflags & ~NV30_SUPPORTED_BLITTINGFLAGS))
               return;

          if (source->format != destination->format)
               return;

          state->accel |= accel;
     }
     else {
          /* check unsupported drawing flags */
          if (state->drawingflags & ~NV30_SUPPORTED_DRAWINGFLAGS)
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
     SurfaceBuffer    *buffer;
     __u32             offset;

     if (nvdev->reloaded) {
          modify |= nvdev->reloaded;
          nvdev->reloaded = 0;
     }

     if (modify & SMF_DESTINATION) {
          buffer = state->destination->back_buffer;
          offset = buffer->video.offset & nvdrv->fb_mask;

          if (nvdev->dst_format != buffer->format     ||
              nvdev->dst_offset != offset             ||
              nvdev->dst_pitch  != buffer->video.pitch)
          {
               /* set offset & pitch */
               nv_set_destination( nvdrv, nvdev, offset,
                                   buffer->video.pitch, buffer->format );

               nvdev->dst_format = buffer->format;
               nvdev->dst_offset = offset;
               nvdev->dst_pitch  = buffer->video.pitch;
          }
     }

     if (modify & SMF_CLIP)
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (modify & (SMF_COLOR | SMF_DESTINATION))
          nv_set_color( nvdrv, nvdev, &state->color );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               if (state->drawingflags & DSDRAW_BLEND) {
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
          case DFXL_STRETCHBLIT:
               buffer = state->source->front_buffer;
               offset = buffer->video.offset & nvdrv->fb_mask;

               if (nvdev->src_offset != offset             ||
                   nvdev->src_pitch  != buffer->video.pitch)
               {
                    nv_set_source( nvdrv, nvdev, offset,
                                   buffer->video.pitch );
                    
                    nvdev->src_offset = offset; 
                    nvdev->src_pitch  = buffer->video.pitch;
               }

               nvdev->src_format = state->source->format;
               nvdev->src_width  = state->source->width;
               nvdev->src_height = state->source->height;
               nvdev->argb_src   = false;

               switch (state->blittingflags) {
                    case DSBLIT_ALPHABLEND:
                         nvdev->bop = 0x00000002;
                         nvdev->argb_src = true;
                         if (nvdev->alpha != state->color.a) {
                              nv_set_beta1( nvdrv, nvdev, state->color.a );
                              nvdev->alpha = state->color.a;
                         }
                         break;
                    case DSBLIT_BLEND_ALPHACHANNEL:
                         nvdev->bop = 0x00000002;
                         nvdev->argb_src = true;
                         if (nvdev->alpha != 0xFF) {
                              nv_set_beta1( nvdrv, nvdev, 0xFF );
                              nvdev->alpha = 0xFF;
                         }
                         break;
                    case DSBLIT_BLEND_COLORALPHA:
                         nvdev->bop = 0x00000002;
                         if (nvdev->alpha != state->color.a) {
                              nv_set_beta1( nvdrv, nvdev, state->color.a );
                              nvdev->alpha    = state->color.a;
                         }
                         break;
                    case DSBLIT_NOFX:
                         nvdev->bop = 0;
                         break;
                    default:
                         D_BUG( "unexpected blittingflags" );
                         break;
               }

               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
               break;

          case DFXL_TEXTRIANGLES:
               nvdev->src_width  = state->source->width;
               nvdev->src_height = state->source->height;

               nvdev->state3d.offset = nvdev->tex_offset;
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
                    buffer = state->destination->depth_buffer;
                    offset = buffer->video.offset & nvdrv->fb_mask;

                    if (nvdev->depth_offset != offset             ||
                        nvdev->depth_pitch  != buffer->video.pitch)
                    {
                         nv_set_depth( nvdrv, nvdev, offset,
                                       buffer->video.pitch );
                         
                         nvdev->depth_offset = offset;
                         nvdev->depth_pitch  = buffer->video.pitch;
                    }

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
     SurfaceBuffer    *buffer;
     __u32             offset;

     if (nvdev->reloaded) {
          modify |= nvdev->reloaded;
          nvdev->reloaded = 0;
     }

     if (modify & SMF_DESTINATION) {
          buffer = state->destination->back_buffer;
          offset = buffer->video.offset & nvdrv->fb_mask;

          if (nvdev->dst_format != buffer->format     ||
              nvdev->dst_offset != offset             ||
              nvdev->dst_pitch  != buffer->video.pitch)
          {
               /* set offset & pitch */
               nv_set_destination( nvdrv, nvdev, offset,
                                   buffer->video.pitch, buffer->format );

               nvdev->dst_format = buffer->format;
               nvdev->dst_offset = offset;
               nvdev->dst_pitch  = buffer->video.pitch;
          }
     }

     if (modify & SMF_CLIP)
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (modify & (SMF_COLOR | SMF_DESTINATION))
          nv_set_color( nvdrv, nvdev, &state->color );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               if (state->drawingflags & DSDRAW_BLEND) {
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
          case DFXL_STRETCHBLIT:
               buffer = state->source->front_buffer;
               offset = buffer->video.offset & nvdrv->fb_mask;
          
               if (nvdev->src_offset != offset             ||
                   nvdev->src_pitch  != buffer->video.pitch)
               {
                    nv_set_source( nvdrv, nvdev, offset,
                                   buffer->video.pitch );
                    
                    nvdev->src_offset = offset;
                    nvdev->src_pitch  = buffer->video.pitch;
               }
              
               nvdev->src_format = state->source->format;
               nvdev->src_width  = (state->source->width  + 1) & ~1;
               nvdev->src_height = (state->source->height + 1) & ~1;
               nvdev->argb_src   = true;
               
               switch (state->blittingflags) {
                    case DSBLIT_ALPHABLEND:
                         nvdev->bop = 0x00000002;
                         if (nvdev->alpha != state->color.a) {
                              nv_set_beta1( nvdrv, nvdev, state->color.a );
                              nvdev->alpha = state->color.a;
                         }
                         break;
                    case DSBLIT_BLEND_ALPHACHANNEL:
                         nvdev->bop = 0x00000002;
                         if (nvdev->alpha != 0xFF) {
                              nv_set_beta1( nvdrv, nvdev, 0xFF );
                              nvdev->alpha = 0xFF;
                         }
                         break;
                    case DSBLIT_BLEND_COLORALPHA:
                         nvdev->bop = 0x00000002;
                         nvdev->argb_src = false;
                         if (nvdev->alpha != state->color.a) {
                              nv_set_beta1( nvdrv, nvdev, state->color.a );
                              nvdev->alpha = state->color.a;
                         }
                         break;
                    case DSBLIT_COLORIZE:
                         nvdev->bop = 0x00000004;
                         nv_set_beta4( nvdrv, nvdev, nvdev->color3d );
                         break;
                    case DSBLIT_NOFX:
                         nvdev->bop = 0x00000003;
                         break;
                    default:
                         D_BUG( "unexpected blittingflags" );
                         break;
               }
               
               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
               break;

          case DFXL_TEXTRIANGLES:
               nvdev->src_width  = state->source->width;
               nvdev->src_height = state->source->height;

               nvdev->state3d.offset = nvdev->tex_offset;
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
                    buffer = state->destination->depth_buffer;
                    offset = buffer->video.offset & nvdrv->fb_mask;

                    if (nvdev->depth_offset != offset             ||
                        nvdev->depth_pitch  != buffer->video.pitch)
                    {
                         nv_set_depth( nvdrv, nvdev, offset,
                                       buffer->video.pitch );
                         
                         nvdev->depth_offset = offset;
                         nvdev->depth_pitch  = buffer->video.pitch;
                    }

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
     SurfaceBuffer    *buffer;
     __u32             offset;

     if (nvdev->reloaded) {
          modify |= nvdev->reloaded;
          nvdev->reloaded = 0;
     }

     if (modify & SMF_DESTINATION) {
          buffer = state->destination->back_buffer;
          offset = (buffer->video.offset + nvdrv->fb_offset) &
                    nvdrv->fb_mask;

          if (nvdev->dst_format != buffer->format     ||
              nvdev->dst_offset != offset             ||
              nvdev->dst_pitch  != buffer->video.pitch)
          {
               /* set offset & pitch */
               nv_set_destination( nvdrv, nvdev, offset,
                                   buffer->video.pitch, buffer->format );
               
               nvdev->dst_format = buffer->format;
               nvdev->dst_offset = offset;
               nvdev->dst_pitch  = buffer->video.pitch;
          }
     }

     if (modify & SMF_CLIP)
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (modify & (SMF_COLOR | SMF_DESTINATION))
          nv_set_color( nvdrv, nvdev, &state->color );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               if (state->drawingflags & DSDRAW_BLEND) {
                    nvdev->dop = 0x00000002;
                    if (nvdev->alpha != state->color.a) {
                         nv_set_beta1( nvdrv, nvdev, state->color.a );
                         nvdev->alpha = state->color.a;
                    }
               } else
                    nvdev->dop = 0x00000003;
               
               state->set |= DFXL_FILLRECTANGLE |
                             DFXL_FILLTRIANGLE  |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               buffer = state->source->front_buffer;
               offset = (buffer->video.offset + nvdrv->fb_offset) &
                         nvdrv->fb_mask;

               if (nvdev->src_offset != offset             ||
                   nvdev->src_pitch  != buffer->video.pitch)
               {
                    nv_set_source( nvdrv, nvdev, offset,
                                   buffer->video.pitch );

                    nvdev->src_offset = offset;
                    nvdev->src_pitch  = buffer->video.pitch;
               }

               nvdev->src_format = state->source->format;
               nvdev->src_width  = (state->source->width  + 1) & ~1;
               nvdev->src_height = (state->source->height + 1) & ~1;
               nvdev->argb_src   = true;
               
               switch (state->blittingflags) {
                    case DSBLIT_ALPHABLEND:
                         nvdev->bop = 0x00000002;
                         if (nvdev->alpha != state->color.a) {
                              nv_set_beta1( nvdrv, nvdev, state->color.a );
                              nvdev->alpha = state->color.a;
                         }
                         break;
                    case DSBLIT_BLEND_ALPHACHANNEL:
                         nvdev->bop = 0x00000002;
                         if (nvdev->alpha != 0xFF) {
                              nv_set_beta1( nvdrv, nvdev, 0xFF );
                              nvdev->alpha = 0xFF;
                         }
                         break;
                    case DSBLIT_BLEND_COLORALPHA:
                         nvdev->bop = 0x00000002;
                         nvdev->argb_src = false;
                         if (nvdev->alpha != state->color.a) {
                              nv_set_beta1( nvdrv, nvdev, state->color.a );
                              nvdev->alpha = state->color.a;
                         }
                         break;
                    case DSBLIT_COLORIZE:
                         nvdev->bop = 0x00000004;
                         nv_set_beta4( nvdrv, nvdev, nvdev->color3d );
                         break;
                    case DSBLIT_NOFX:
                         nvdev->bop = 0x00000003;
                         break;
                    default:
                         D_BUG( "unexpected blittingflags" );
                         break;
               }
               
               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     state->modified = 0;
}

static void nv30SetState( void *drv, void *dev,
                          GraphicsDeviceFuncs *funcs,
                          CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;
     __u32             modify = state->modified;
     SurfaceBuffer    *buffer;
     __u32             offset;

     if (nvdev->reloaded) {
          modify |= nvdev->reloaded;
          nvdev->reloaded = 0;
     }

     if (modify & SMF_DESTINATION) {
          buffer = state->destination->back_buffer;
          offset = buffer->video.offset & nvdrv->fb_mask;

          if (nvdev->dst_format != buffer->format     ||
              nvdev->dst_offset != offset             ||
              nvdev->dst_pitch  != buffer->video.pitch)
          {
               /* set offset & pitch */
               nv_set_destination( nvdrv, nvdev, offset,
                                   buffer->video.pitch, buffer->format );
               
               nvdev->dst_format = buffer->format;
               nvdev->dst_offset = offset;
               nvdev->dst_pitch  = buffer->video.pitch;
          }
     }

     if (modify & SMF_CLIP)
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (modify & (SMF_COLOR | SMF_DESTINATION))
          nv_set_color( nvdrv, nvdev, &state->color );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               state->set |= DFXL_FILLRECTANGLE |
                             DFXL_FILLTRIANGLE  |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
               buffer = state->source->front_buffer;
               offset = buffer->video.offset & nvdrv->fb_mask;

               if (nvdev->src_offset != offset             ||
                   nvdev->src_pitch  != buffer->video.pitch)
               {
                    nv_set_source( nvdrv, nvdev, offset,
                                   buffer->video.pitch );
                    
                    nvdev->src_offset = offset;
                    nvdev->src_pitch  = buffer->video.pitch;
               }

               nvdev->src_format = state->source->format;
               nvdev->src_width  = (state->source->width  + 1) & ~1;
               nvdev->src_height = (state->source->height + 1) & ~1;
               nvdev->argb_src   = true;
               nvdev->bop        = 0x00000003; 
               
               state->set |= DFXL_BLIT;
               break;

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
     info->version.minor = 3;

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

     nvdrv->PVIDEO      = (volatile __u32*) (nvdrv->mmio_base + 0x008000);
     nvdrv->PVIO        = (volatile __u8 *) (nvdrv->mmio_base + 0x0C0000);
     nvdrv->PFB         = (volatile __u32*) (nvdrv->mmio_base + 0x100000);
     nvdrv->PGRAPH      = (volatile __u32*) (nvdrv->mmio_base + 0x400000);
     nvdrv->PCRTC       = (volatile __u32*) (nvdrv->mmio_base + 0x600000);
     nvdrv->PCIO        = (volatile __u8 *) (nvdrv->mmio_base + 0x601000);
     nvdrv->PRAMIN      = (volatile __u32*) (nvdrv->mmio_base + 0x700000);
     nvdrv->PRAMHT      = (volatile __u32*) (nvdrv->mmio_base + 0x710000);

     nvdrv->Fifo        = (NVFifoChannel*) (nvdrv->mmio_base + 0x800000);
     nvdrv->Surfaces2D  = &nvdrv->Fifo->sub[0].o.Surfaces2D;
     nvdrv->Surfaces3D  = &nvdrv->Fifo->sub[0].o.Surfaces3D;
     nvdrv->Beta1       = &nvdrv->Fifo->sub[0].o.Beta1;
     nvdrv->Beta4       = &nvdrv->Fifo->sub[0].o.Beta4;
     nvdrv->Clip        = &nvdrv->Fifo->sub[1].o.Clip;
     nvdrv->Rectangle   = &nvdrv->Fifo->sub[2].o.Rectangle;
     nvdrv->Triangle    = &nvdrv->Fifo->sub[3].o.Triangle;
     nvdrv->Line        = &nvdrv->Fifo->sub[4].o.Line;
     nvdrv->Blt         = &nvdrv->Fifo->sub[5].o.Blt;
     nvdrv->ScaledImage = &nvdrv->Fifo->sub[6].o.ScaledImage;
     nvdrv->TexTriangle = &nvdrv->Fifo->sub[7].o.TexTriangle;

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
               funcs->Blit             = nv4Blit;
               funcs->StretchBlit      = nv4StretchBlit;
               funcs->TextureTriangles = nvTextureTriangles;
               break;
          case NV_ARCH_05:
          case NV_ARCH_10:
               funcs->CheckState       = nv5CheckState;
               funcs->SetState         = nv5SetState;
               funcs->Blit             = nv5Blit;
               funcs->StretchBlit      = nv5StretchBlit;
               funcs->TextureTriangles = nvTextureTriangles;
               break;
          case NV_ARCH_20:
               funcs->CheckState       = nv20CheckState;
               funcs->SetState         = nv20SetState;
               funcs->Blit             = nv5Blit;
               funcs->StretchBlit      = nv5StretchBlit;
               break;
          case NV_ARCH_30:
               funcs->CheckState       = nv30CheckState;
               funcs->SetState         = nv30SetState;
               funcs->Blit             = nv5Blit;
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

     device_info->caps.flags = CCF_CLIPPING;

     switch (nvdrv->arch) {
          case NV_ARCH_04:     
               device_info->caps.accel    = NV4_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV4_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV4_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV4_SUPPORTED_BLITTINGFLAGS;
               break;
          case NV_ARCH_05:
          case NV_ARCH_10:
               device_info->caps.accel    = NV5_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV5_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV5_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV5_SUPPORTED_BLITTINGFLAGS;
               break;
          case NV_ARCH_20:
               device_info->caps.accel    = NV20_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV20_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV20_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV20_SUPPORTED_BLITTINGFLAGS;
               break;
          case NV_ARCH_30:
               device_info->caps.accel    = NV30_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV30_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV30_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV30_SUPPORTED_BLITTINGFLAGS;
               break;
          default:
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
          memset( dfb_system_video_memory_virtual( nvdev->col_offset ), 0xFF, 8 );
          
          /* set default 3d state */
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
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE,
                                0x63, 0x0311A000 | ENDIAN_FLAG, 0,
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
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE,
                                0x89, 0x0311A000 | ENDIAN_FLAG, 0,
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
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE,
                                0x89, 0x0311A000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE,
                                0x94, 0x03002000 | ENDIAN_FLAG, 0,
                                ADDR_DMA, ADDR_DMA );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D,
                                0x93, ENDIAN_FLAG, 0, 0, 0 );
               break;
     }
     
#undef ENDIAN_FLAG

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

