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
               (DSBLIT_NOFX)

#define NV4_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES)

/* TNT2/GeForce(1,2,4) */
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

/* GeForce3 */
#define NV20_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_NOFX)

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
     __u32 key;

     key = ((obj >>  0) & 0x000001FF) ^
           ((obj >>  9) & 0x000001FF) ^
           ((obj >> 18) & 0x000001FF) ^
           ((obj >> 27));
     /* key should be xored with (fifo channel id << 5) too,
      * but, since we always use channel 0, it can be ignored */
     return (key << 3);
}

static inline void
nv_store_object( NVidiaDriverData *nvdrv, __u32 obj, 
                 __u32 addr, __u32 class, __u32 flags,
                 __u32 color, __u32 dma0, __u32 dma1, __u32 engine )
{
     volatile __u32 *PRAMIN = nvdrv->PRAMIN;
     volatile __u32 *PRAMHT = nvdrv->PRAMHT;
     __u32           key    = nv_hashkey( obj );
     __u32           ctx    = addr | (engine << 16) | (1 << 31);

     /* NV_PRAMIN_CTX_0 */
     nv_out32( PRAMIN, (addr << 4) +  0, class | flags );
     /* NV_PRAMIN_CTX_1 */
     nv_out32( PRAMIN, (addr << 4) +  4, color );
     /* NV_PRAMIN_CTX_2 */
     nv_out32( PRAMIN, (addr << 4) +  8, dma0 | (dma1 << 16) );
     /* NV_PRAMIN_CTX_3 */
     nv_out32( PRAMIN, (addr << 4) + 12, 0x00000000 );

     /* store object id and context */
     nv_out32( PRAMHT, key + 0, obj );
     nv_out32( PRAMHT, key + 4, ctx );
}

static inline void
nv_set_format( NVidiaDriverData      *nvdrv,
               NVidiaDeviceData      *nvdev,
               DFBSurfacePixelFormat  format )
{
     volatile __u32   *PRAMIN      = nvdrv->PRAMIN;
     NVFifoChannel    *Fifo        = nvdrv->Fifo;
     NVFifoSubChannel *SubChannel0 = &nvdrv->Fifo->sub[0];
     NVSurfaces2D     *Surfaces2D  = nvdrv->Surfaces2D;
     NVSurfaces3D     *Surfaces3D  = nvdrv->Surfaces3D;
     __u32             sformat2D   = 0;
     __u32             sformat3D   = 0;
     __u32             cformat     = 0;

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

     nv_out32( PRAMIN, (ADDR_RECTANGLE << 4) + 4, cformat );
     nv_out32( PRAMIN, (ADDR_TRIANGLE  << 4) + 4, cformat );
     nv_out32( PRAMIN, (ADDR_LINE      << 4) + 4, cformat );

     nv_waitfifo( nvdev, SubChannel0, 3 );
     Fifo->sub[2].SetObject = OBJ_RECTANGLE;
     Fifo->sub[3].SetObject = OBJ_TRIANGLE;
     Fifo->sub[4].SetObject = OBJ_LINE;

     if (nvdev->enabled_3d) {
          nv_waitfifo( nvdev, SubChannel0, 4 );
          SubChannel0->SetObject = OBJ_SURFACES2D;
          Surfaces2D->Format     = sformat2D;
          SubChannel0->SetObject = OBJ_SURFACES3D;
          Surfaces3D->Format     = sformat3D;
     } else {
          nv_waitfifo( nvdev, SubChannel0, 1 );
          Surfaces2D->Format     = sformat2D;
     }
     
}

static inline void
nv_set_clip( NVidiaDriverData *nvdrv,
             NVidiaDeviceData *nvdev,
             DFBRegion        *clip )
{
     NVClip *Clip   = nvdrv->Clip;
     int     width  = clip->x2 - clip->x1 + 1;
     int     height = clip->y2 - clip->y1 + 1;

     nv_waitfifo( nvdev, subchannelof(Clip), 2 );
     Clip->TopLeft     = (clip->y1 << 16) | (clip->x1 & 0xFFFF);
     Clip->WidthHeight = (height   << 16) | (width    & 0xFFFF);
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

          case DSPF_YUY2:
          case DSPF_UYVY:
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
nv_set_blittingflags( NVidiaDriverData        *nvdrv,
                      NVidiaDeviceData        *nvdev,
                      DFBSurfaceBlittingFlags  flags,
                      DFBColor                 color )
{
     /* we use the 5th bit of nvdev->blitfx to determine if
      * source format can be set to ARGB */
     switch (flags) {
          case DSBLIT_ALPHABLEND:
               nvdev->blitfx = 0x00000012;
               if (nvdev->alpha != color.a) {
                    nv_out32( nvdrv->PGRAPH, 0x608, color.a << 23 );
                    nvdev->alpha = color.a;
               }
               break;
          case DSBLIT_BLEND_ALPHACHANNEL:
               nvdev->blitfx = 0x00000012;
               if (nvdev->alpha != 0xFF) {
                    nv_out32( nvdrv->PGRAPH, 0x608, 0x7F800000 );
                    nvdev->alpha = 0xFF;
               }
               break;
          case DSBLIT_BLEND_COLORALPHA:
               nvdev->blitfx = 0x00000002;
               if (nvdev->alpha != color.a) {
                    nv_out32( nvdrv->PGRAPH, 0x608, color.a << 23 );
                    nvdev->alpha = color.a;
               }
               break;
          case DSBLIT_COLORIZE:
               nvdev->blitfx = 0x00000004;
               nv_out32( nvdrv->PGRAPH, 0x60C, nvdev->color3d );
               break;
          case DSBLIT_NOFX:
               nvdev->blitfx = 0x00000003;
               break;
          default:
               D_BUG( "unexpected blittingflags" );
               break;
     }
}


static void nvAfterSetVar( void *driver_data,
                           void *device_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;
     NVFifoChannel    *Fifo  = nvdrv->Fifo;
     __u32             color = 0;
 
     /* unsupported chipset */
     if (!nvdrv->arch)
          return;

     switch (dfb_primary_layer_pixelformat()) {
          case DSPF_LUT8:
          case DSPF_RGB332:
               color = 0x00000302;
               break;
          case DSPF_ARGB1555:
               color = 0x00000602;
               break;
          case DSPF_RGB16:
               color = 0x00000C02;
               break;
          case DSPF_RGB32:
               color = 0x00000E02;
               break;
          case DSPF_ARGB:
          default:
               color = 0x00000D02;
               break;
     }

#ifdef WORDS_BIGENDIAN
# define ENDIAN_FLAG 0x00080000
#else
# define ENDIAN_FLAG 0
#endif

     /* write objects configuration */
     nv_store_object( nvdrv, OBJ_SURFACES2D, ADDR_SURFACES2D,
                      0x42, 0x01008000 | ENDIAN_FLAG, 0,
                      0, 0, ENGINE_GRAPHICS );
     nv_store_object( nvdrv, OBJ_CLIP, ADDR_CLIP,
                      0x19, 0x01008000 | ENDIAN_FLAG, 0,
                      0, 0, ENGINE_GRAPHICS );
     nv_store_object( nvdrv, OBJ_RECTANGLE, ADDR_RECTANGLE,
                      0x5E, 0x0100A000 | ENDIAN_FLAG, color,
                      ADDR_DMA, ADDR_DMA, ENGINE_GRAPHICS );
     nv_store_object( nvdrv, OBJ_TRIANGLE, ADDR_TRIANGLE,
                      0x5D, 0x0100A000 | ENDIAN_FLAG, color,
                      ADDR_DMA, ADDR_DMA, ENGINE_GRAPHICS );
     nv_store_object( nvdrv, OBJ_LINE, ADDR_LINE,
                      0x5C, 0x0100A000 | ENDIAN_FLAG, color,
                      ADDR_DMA, ADDR_DMA, ENGINE_GRAPHICS );
     nv_store_object( nvdrv, OBJ_SCREENBLT, ADDR_SCREENBLT,
                      0x1F, 0x0100A000 | ENDIAN_FLAG, color,
                      ADDR_DMA, ADDR_DMA, ENGINE_GRAPHICS );

     switch (nvdrv->arch)  {
          case NV_ARCH_04:
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE,
                                0x37, 0x0100A000 | ENDIAN_FLAG, color,
                                ADDR_DMA, ADDR_DMA, ENGINE_GRAPHICS );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE,
                                0x54, 0x0300A000 | ENDIAN_FLAG, color,
                                ADDR_DMA, ADDR_DMA, ENGINE_GRAPHICS );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D,
                                0x53, 0x01008000 | ENDIAN_FLAG, 0,
                                0, 0, ENGINE_GRAPHICS );
               break;

          case NV_ARCH_05:
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE,
                                0x63, 0x0100A000 | ENDIAN_FLAG, color,
                                ADDR_DMA, ADDR_DMA, ENGINE_GRAPHICS );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE,
                                0x54, 0x0300A000 | ENDIAN_FLAG, color,
                                ADDR_DMA, ADDR_DMA, ENGINE_GRAPHICS );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D,
                                0x53, 0x01008000 | ENDIAN_FLAG, 0,
                                0, 0, ENGINE_GRAPHICS );
               break;

          case NV_ARCH_10:
          case NV_ARCH_20:
          default:
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE,
                                0x77, 0x0100A000 | ENDIAN_FLAG, color,
                                ADDR_DMA, ADDR_DMA, ENGINE_GRAPHICS );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE,
                                0x94, 0x0300A000 | ENDIAN_FLAG, color,
                                ADDR_DMA, ADDR_DMA, ENGINE_GRAPHICS );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D,
                                0x93, 0x01008000 | ENDIAN_FLAG, 0,
                                0, 0, ENGINE_GRAPHICS );
               break;
     }
     
#undef ENDIAN_FLAG
     
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
     nvdev->src_format  = DSPF_UNKNOWN;
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
          /* check unsupported blitting flags first */
          if (state->blittingflags & ~NV4_SUPPORTED_BLITTINGFLAGS)
               return;

          if (accel == DFXL_TEXTRIANGLES &&
             (source->width > 512 || source->height > 512))
               return;

          switch (source->format) {
               case DSPF_ARGB1555:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    break;
                    
               case DSPF_RGB16:
                    if (accel == DFXL_STRETCHBLIT || 
                        destination->format != DSPF_RGB16)
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
          /* check unsupported drawing flags first */
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
          /* check unsupported blitting flags first */
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
          /* check unsupported drawing flags first */
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
          /* check unsupported blitting flags first */
          if (state->blittingflags & ~NV20_SUPPORTED_BLITTINGFLAGS)
               return;

          /* TextureTriangles() is disabled for precaution,
           * we don't know if it really works on NV20 */
          if (accel == DFXL_TEXTRIANGLES)
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
          /* check unsupported drawing flags first */
          if (state->drawingflags & ~NV20_SUPPORTED_DRAWINGFLAGS)
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
     volatile __u32   *PGRAPH = nvdrv->PGRAPH;
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
               nv_waitidle( nvdrv, nvdev );

               nv_out32( PGRAPH, 0x640, offset );
               nv_out32( PGRAPH, 0x648, offset );
               nv_out32( PGRAPH, 0x670, buffer->video.pitch );
               nv_out32( PGRAPH, 0x678, buffer->video.pitch );

               if (nvdev->dst_format != buffer->format)
                    nv_set_format( nvdrv, nvdev, buffer->format );

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

               if (nvdev->src_format != buffer->format     ||
                   nvdev->src_offset != offset             ||
                   nvdev->src_pitch  != buffer->video.pitch)
               {
                    nv_waitidle( nvdrv, nvdev );

                    nv_out32( PGRAPH, 0x644, offset );
                    nv_out32( PGRAPH, 0x674, buffer->video.pitch );

                    nvdev->src_format = buffer->format;
                    nvdev->src_offset = offset; 
                    nvdev->src_pitch  = buffer->video.pitch;
               }

               nvdev->src_width  = state->source->width;
               nvdev->src_height = state->source->height;

               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
               break;

          case DFXL_TEXTRIANGLES:
               nvdev->src_width  = state->source->width;
               nvdev->src_height = state->source->height;

               nvdev->state3d.offset = nvdev->tex_offset;
               nvdev->state3d.format = 0x119915A1; // 512x512 RGB16
               nvdev->state3d.blend  = 0x12000167;

               if (state->destination->caps & DSCAPS_DEPTH) {
                    buffer = state->destination->depth_buffer;
                    offset = buffer->video.offset & nvdrv->fb_mask;

                    if (nvdev->depth_offset != offset             ||
                        nvdev->depth_pitch  != buffer->video.pitch)
                    {
                         nv_waitidle( nvdrv, nvdev );
                         
                         nv_out32( PGRAPH, 0x64C, offset );
                         nv_out32( PGRAPH, 0x67C, buffer->video.pitch );

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
     volatile __u32   *PGRAPH = nvdrv->PGRAPH;
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
               nv_waitidle( nvdrv, nvdev );

               nv_out32( PGRAPH, 0x640, offset );
               nv_out32( PGRAPH, 0x648, offset );
               nv_out32( PGRAPH, 0x670, buffer->video.pitch );
               nv_out32( PGRAPH, 0x678, buffer->video.pitch );

               if (nvdev->dst_format != buffer->format)
                    nv_set_format( nvdrv, nvdev, buffer->format );

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
          
               if (nvdev->src_format != buffer->format     ||
                   nvdev->src_offset != offset             ||
                   nvdev->src_pitch  != buffer->video.pitch)
               {
                    nv_waitidle( nvdrv, nvdev );

                    nv_out32( PGRAPH, 0x644, offset );
                    nv_out32( PGRAPH, 0x674, buffer->video.pitch );

                    nvdev->src_format = buffer->format;
                    nvdev->src_offset = offset;
                    nvdev->src_pitch  = buffer->video.pitch;
               }
               
               nvdev->src_width  = (state->source->width  + 1) & ~1;
               nvdev->src_height = (state->source->height + 1) & ~1;
               
               nv_set_blittingflags( nvdrv, nvdev,
                         state->blittingflags, state->color );
               
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
                         nv_waitidle( nvdrv, nvdev );
                         
                         nv_out32( PGRAPH, 0x64C, offset );
                         nv_out32( PGRAPH, 0x67C, buffer->video.pitch );

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
     volatile __u32   *PGRAPH = nvdrv->PGRAPH;
     __u32             modify = state->modified;
     SurfaceBuffer    *buffer;
     __u32             offset;

     if (nvdev->reloaded) {
          modify |= nvdev->reloaded;
          nvdev->reloaded = 0;
     }

     if (modify & SMF_DESTINATION) {
          buffer = state->destination->back_buffer;
          offset = buffer->video.offset;

          if (nvdrv->chip == 0x2A0) /* GeForce3 XBox */
               offset += nvdrv->fb_base;
          offset &= nvdrv->fb_mask;

          if (nvdev->dst_format != buffer->format     ||
              nvdev->dst_offset != offset             ||
              nvdev->dst_pitch  != buffer->video.pitch)
          {
               /* set offset & pitch */
               nv_waitidle( nvdrv, nvdev );

               nv_out32( PGRAPH, 0x820, offset );
               //nv_out32( PGRAPH, 0x828, offset );
               nv_out32( PGRAPH, 0x850, buffer->video.pitch );
               //nv_out32( PGRAPH, 0x858, buffer->video.pitch );

               if (nvdev->dst_format != buffer->format)
                    nv_set_format( nvdrv, nvdev, buffer->format );

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
          case DFXL_STRETCHBLIT:
               buffer = state->source->front_buffer;
               offset = buffer->video.offset;

               if (nvdrv->chip == 0x2A0) /* GeForce3 XBox */
                    offset += nvdrv->fb_base;
               offset &= nvdrv->fb_mask;

               if (nvdev->src_format != buffer->format     ||
                   nvdev->src_offset != offset             ||
                   nvdev->src_pitch  != buffer->video.pitch)
               {
                    nv_waitidle( nvdrv, nvdev );

                    nv_out32( PGRAPH, 0x824, offset );
                    nv_out32( PGRAPH, 0x854, buffer->video.pitch );

                    nvdev->src_format = buffer->format;
                    nvdev->src_offset = offset;
                    nvdev->src_pitch  = buffer->video.pitch;
               }

               nvdev->src_width  = (state->source->width  + 1) & ~1;
               nvdev->src_height = (state->source->height + 1) & ~1;
               
               nv_set_blittingflags( nvdrv, nvdev,
                         state->blittingflags, state->color );
               
               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
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
               "nVidia RIVA TNT/TNT2/GeForce Driver" );

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

     nvdrv->fb_base = (__u32) dfb_gfxcard_memory_physical( device, 0 );
     if (nvdrv->chip == 0x2A0) /* GeForce3 Xbox */
          vram += nvdrv->fb_base & 0x0FFFFFFF;
     nvdrv->fb_mask = ((1 << direct_log2( vram )) - 0x100000) | 0x000FFFC0;
     
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
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "RIVA TNT/TNT2/GeForce" );

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
          nvdev->state3d.control  = 0x40180800;
          nvdev->state3d.fog      = 0;
     }

     nvdev->alpha = 0xFF;
     
     /* NV_PGRAPH_ROP3 */
     nv_out32( nvdrv->PGRAPH, 0x604, 0x000000CC );
     /* NV_PGRAPH_BETA_AND */
     nv_out32( nvdrv->PGRAPH, 0x608, 0x7F800000 );

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

