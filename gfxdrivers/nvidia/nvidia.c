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

#include <dfb_types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/screens.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>

#include <core/graphics_driver.h>
#include <core/layers.h>

#include "config.h"

DFB_GRAPHICS_DRIVER( nvidia )

#include "nvidia.h"
#include "nvidia_tables.h"



#define NV_FIFO_FREE( nvdev, ptr, space )                        \
{                                                                \
     (nvdev)->waitfifo_sum += (space);                           \
     (nvdev)->waitfifo_calls++;                                  \
                                                                 \
     if ((nvdev)->fifo_space < (space)) {                        \
          do {                                                   \
               (nvdev)->fifo_space = (ptr)->FifoFree >> 2;       \
               (nvdev)->fifo_waitcycles++;                       \
          } while ((nvdev)->fifo_space < (space));               \
     }                                                           \
     else {                                                      \
          (nvdev)->fifo_cache_hits++;                            \
     }                                                           \
                                                                 \
     (nvdev)->fifo_space -= (space);                             \
}

#define NV_LOAD_TABLE( dest, table )                             \
{                                                                \
     int _i = 0;                                                 \
     for (; _i < sizeof(nv##table) / sizeof(nv##table[0]); _i++) \
          dest[nv##table[_i][0]] = nv##table[_i][1];             \
}


static inline void nv_waitidle( NVidiaDriverData *nvdrv,
                                NVidiaDeviceData *nvdev )
{
     while (nvdrv->PGRAPH[0x1C0] & 1) {
          nvdev->idle_waitcycles++;
     }
}


static void nvEngineSync( void *drv, void *dev )
{
     nv_waitidle( (NVidiaDriverData*) drv, (NVidiaDeviceData*) dev );
}

#define NV4_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV4_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE | DFXL_DRAWLINE)

#define NV4_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_NOFX)

#define NV4_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)



static inline void nv_set_clip( NVidiaDriverData *nvdrv,
                                NVidiaDeviceData *nvdev,
                                DFBRegion        *clip )
{
     volatile NVClip *Clip   = nvdrv->Clip;
     int              width  = clip->x2 - clip->x1 + 1;
     int              height = clip->y2 - clip->y1 + 1;

     NV_FIFO_FREE( nvdev, Clip, 2 );

     Clip->TopLeft     = (clip->y1 << 16) | clip->x1;
     Clip->WidthHeight = (height << 16) | width;
}

static inline void nv_set_rop( NVidiaDriverData        *nvdrv,
                               NVidiaDeviceData        *nvdev,
                               DFBSurfaceBlendFunction  src,
                               DFBSurfaceBlendFunction  dst )
{
     volatile NVRop *Rop = nvdrv->Rop;
     __u32           op  = 0;
     
     switch (src) {
          case 0:
               op |= 0xC0;
               break;
          case DSBF_ZERO:
               op |= 0x00;
               break;
          default:
               return;
     }

     switch (dst) {
          case 0:
               op |= 0x0C;
               break;
          case DSBF_ZERO:
               op |= 0x00;
               break;
          default:
               return;
     }

     NV_FIFO_FREE( nvdev, Rop, 1 )
     Rop->Rop3 = op;
}


static void nvCheckState( void *drv, void *dev,
                          CardState *state, DFBAccelerationMask accel )
{
     switch (state->destination->format) {
       case DSPF_ARGB1555:
       case DSPF_RGB16:
       case DSPF_RGB32:
       case DSPF_ARGB:
               break;
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->source->format != state->destination->format)
               return;

          /* currently strechblit works only at 32bpp */
	  if (accel == DFXL_STRETCHBLIT && 
              DFB_BITS_PER_PIXEL( state->destination->format ) != 32 )
               return;

          /* if there are no other blitting flags than the supported */
          if (!(state->blittingflags & ~NV4_SUPPORTED_BLITTINGFLAGS))
               state->accel |= NV4_SUPPORTED_BLITTINGFUNCTIONS;
     }
     else {
          /* if there are no other drawing flags than the supported */
          if (state->drawingflags & ~NV4_SUPPORTED_DRAWINGFLAGS)
               return;

          if (state->drawingflags & DSDRAW_BLEND) {
               switch (state->src_blend) {
                    case DSBF_ZERO:
                         break;
                    default:
                         return;
               }

               switch (state->dst_blend) {
                    case DSBF_ZERO:
                         break;
                    default:
                         return;
               }
          }
            
          state->accel |= NV4_SUPPORTED_DRAWINGFUNCTIONS;
     }
}

static void nvSetState( void *drv, void *dev,
                        GraphicsDeviceFuncs *funcs,
                        CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;
     volatile __u32   *PGRAPH = nvdrv->PGRAPH;
     volatile __u32   *PRAMIN = nvdrv->PRAMIN;
     volatile __u32   *FIFO   = nvdrv->FIFO;

     nvdev->state = state;
     
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
          case DFXL_TEXTRIANGLES: /* doesn't work yet */
               state->set |= DFXL_FILLTRIANGLE |
                             DFXL_FILLRECTANGLE |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE |
                             DFXL_BLIT |
                             DFXL_STRETCHBLIT |
                    DFXL_TEXTRIANGLES;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     if (state->modified & SMF_CLIP)
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (state->modified & SMF_DESTINATION) {
          /* set offset & pitch */
          nv_waitidle( nvdrv, nvdev );
          PGRAPH[0x640/4] = state->destination->back_buffer->video.offset & 0x1FFFFFF;
          PGRAPH[0x670/4] = state->destination->back_buffer->video.pitch;

          if (state->destination->format != nvdev->format) {
               /* change pixelformat */
               switch (state->destination->format) {
                    case DSPF_ARGB1555:
                         NV_LOAD_TABLE( PGRAPH, PGRAPH_ARGB1555 )
                         NV_LOAD_TABLE( PRAMIN, PRAMIN_ARGB1555 )
                         break;
            
                    case DSPF_RGB16:
                         NV_LOAD_TABLE( PGRAPH, PGRAPH_RGB16 )
                         NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB16 )
                         break;
            
                    case DSPF_RGB32:
                         NV_LOAD_TABLE( PGRAPH, PGRAPH_RGB32 )
                         NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB32 )
                         break;

                    case DSPF_ARGB:
                         NV_LOAD_TABLE( PGRAPH, PGRAPH_ARGB )
                         NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB32 )
                         break;
            
                    default:
                         D_BUG( "unexpected pixelformat" );
                         break;
               }

               /* update fifo subchannels */
               NV_LOAD_TABLE( FIFO, FIFO )

               nvdev->format = state->destination->format;
          }
     }

     if (state->modified & SMF_SOURCE && state->source) {
          nv_waitidle( nvdrv, nvdev );
          PGRAPH[0x644/4] = state->source->front_buffer->video.offset & 0x1FFFFFF;
          PGRAPH[0x674/4] = state->source->front_buffer->video.pitch;
     }

     if (state->modified & (SMF_DESTINATION | SMF_COLOR)) {
          switch (state->destination->format) {
               case DSPF_ARGB1555:
                    nvdev->color = PIXEL_ARGB1555( state->color.a,
                                                   state->color.r,
                                                   state->color.g,
                                                   state->color.b );
                    break;
              
               case DSPF_RGB16:
                    nvdev->color = PIXEL_RGB16( state->color.r,
                                                state->color.g,
                                                state->color.b );
                    break;
              
               case DSPF_RGB32:
                    nvdev->color = PIXEL_RGB32( state->color.r,
                                                state->color.g,
                                                state->color.b );
                    break;

               case DSPF_ARGB:
                    nvdev->color = PIXEL_ARGB( state->color.a,
                                               state->color.r,
                                               state->color.g,
                                               state->color.b );
                    break;
            
               default:
                    D_BUG( "unexpected pixelformat" );
                    break;
          }
     }

     if (state->modified & SMF_DRAWING_FLAGS) {
          if (state->drawingflags == DSDRAW_BLEND)
               nv_set_rop( nvdrv, nvdev, state->src_blend, state->dst_blend );
          else
               nv_set_rop( nvdrv, nvdev, 0, 0 );
     }

     state->modified = 0;
}

static bool nvFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData     *nvdrv     = (NVidiaDriverData*) drv;
     NVidiaDeviceData     *nvdev     = (NVidiaDeviceData*) dev;
     volatile NVRectangle *Rectangle = nvdrv->Rectangle;
     
     NV_FIFO_FREE( nvdev, Rectangle, 3 );

     Rectangle->Color = nvdev->color;

     Rectangle->TopLeft     = (rect->y << 16) | rect->x;
     Rectangle->WidthHeight = (rect->h << 16) | rect->w;

     return true;
}

static bool nvFillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     NVidiaDriverData    *nvdrv    = (NVidiaDriverData*) drv;
     NVidiaDeviceData    *nvdev    = (NVidiaDeviceData*) dev;
     volatile NVTriangle *Triangle = nvdrv->Triangle;

     NV_FIFO_FREE( nvdev, Triangle, 4 );

     Triangle->Color = nvdev->color;

     Triangle->TrianglePoint0 = (tri->y1 << 16) | tri->x1;
     Triangle->TrianglePoint1 = (tri->y2 << 16) | tri->x2;
     Triangle->TrianglePoint2 = (tri->y3 << 16) | tri->x3;

     return true;
}

static bool nvDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData     *nvdrv     = (NVidiaDriverData*) drv;
     NVidiaDeviceData     *nvdev     = (NVidiaDeviceData*) dev;
     volatile NVRectangle *Rectangle = nvdrv->Rectangle;

     NV_FIFO_FREE( nvdev, Rectangle, 9 );

     Rectangle->Color = nvdev->color;

     Rectangle->TopLeft     = (rect->y << 16) | rect->x;
     Rectangle->WidthHeight = (1 << 16) | rect->w;

     Rectangle->TopLeft     = ((rect->y + rect->h - 1) << 16) | rect->x;
     Rectangle->WidthHeight = (1 << 16) | rect->w;

     Rectangle->TopLeft     = ((rect->y + 1) << 16) | rect->x;
     Rectangle->WidthHeight = ((rect->h - 2) << 16) | 1;

     Rectangle->TopLeft     = ((rect->y + 1) << 16) | (rect->x + rect->w - 1);
     Rectangle->WidthHeight = ((rect->h - 2) << 16) | 1;

     return true;
}

static bool nvDrawLine( void *drv, void *dev, DFBRegion *line )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     volatile NVLine  *Line  = nvdrv->Line;

     NV_FIFO_FREE( nvdev, Line, 4 );

     Line->Color = nvdev->color;

     Line->Lin[0].point0 = (line->y1 << 16) | line->x1;
     Line->Lin[0].point1 = (line->y2 << 16) | line->x2;

     return true;
}

static bool nvBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     NVidiaDriverData     *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData     *nvdev = (NVidiaDeviceData*) dev;
     volatile NVScreenBlt *Blt   = nvdrv->Blt;

     NV_FIFO_FREE( nvdev, Blt, 3 );

     Blt->TopLeftSrc  = (rect->y << 16) | rect->x;
     Blt->TopLeftDst  = (dy << 16) | dx;
     Blt->WidthHeight = (rect->h << 16) | rect->w;

     return true;
}


static bool nvStretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{

     NVidiaDriverData       *nvdrv       = (NVidiaDriverData*) drv;
     NVidiaDeviceData       *nvdev       = (NVidiaDeviceData*) dev;
     CardState              *state       = nvdev->state;
     volatile NVScaledImage *ScaledImage = nvdrv->ScaledImage;

     NV_FIFO_FREE( nvdev, ScaledImage, 6 )
     ScaledImage->ClipPoint     = (dr->y << 16) | dr->x;
     ScaledImage->ClipSize      = (dr->h << 16) | dr->w;
     ScaledImage->ImageOutPoint = (dr->y << 16) | dr->x;
     ScaledImage->ImageOutSize  = (dr->h << 16) | dr->w;
     ScaledImage->DuDx          = (sr->w << 20) / dr->w;
     ScaledImage->DvDy          = (sr->h << 20) / dr->h;

     NV_FIFO_FREE( nvdev, ScaledImage, 4 )
     ScaledImage->ImageInSize   = (state->source->height << 16) | state->source->width;
     ScaledImage->ImageInFormat = state->source->front_buffer->video.pitch;
     ScaledImage->ImageInOffset = state->source->front_buffer->video.offset & 0x1FFFFFF;
     ScaledImage->ImageInPoint  = 0;
     
     return true;
}


#if 0
#define NV_VERTEX3D( v, x, y, z, w, col, spc, s, t ) \
{\
     NV_FIFO_FREE( nvdev, TexTri, 8 );\
     TexTri->Vertex[v].sx       = x;\
     TexTri->Vertex[v].sy       = y;\
     TexTri->Vertex[v].sz       = z;\
     TexTri->Vertex[v].rhw      = w;\
     TexTri->Vertex[v].color    = col;\
     TexTri->Vertex[v].specular = spc;\
     TexTri->Vertex[v].ts       = s;\
     TexTri->Vertex[v].tt       = t;\
}

static void
TextureTriangle( NVidiaDriverData *nvdrv, NVidiaDeviceData *nvdev,
           DFBVertex *v0, DFBVertex *v1, DFBVertex *v2 )
{
     volatile RivaTexturedTriangle05 *TexTri = nvdrv->TexTri;

     NV_FIFO_FREE( nvdev, TexTri, 8 );

     TexTri->Vertex[0].sx       = v0->x;
     TexTri->Vertex[0].sy       = v0->y;
     TexTri->Vertex[0].sz       = v0->z;
     TexTri->Vertex[0].rhw      = 1.0 / v0->w;
     TexTri->Vertex[0].color    = 0;
     TexTri->Vertex[0].specular = 0;
     TexTri->Vertex[0].ts       = v0->s;
     TexTri->Vertex[0].tt       = v0->t;

     TexTri->Vertex[1].sx       = v1->x;
     TexTri->Vertex[1].sy       = v1->y;
     TexTri->Vertex[1].sz       = v1->z;
     TexTri->Vertex[1].rhw      = 1.0 / v1->w;
     TexTri->Vertex[1].color    = 0;
     TexTri->Vertex[1].specular = 0;
     TexTri->Vertex[1].ts       = v1->s;
     TexTri->Vertex[1].tt       = v1->t;

     TexTri->Vertex[2].sx       = v2->x;
     TexTri->Vertex[2].sy       = v2->y;
     TexTri->Vertex[2].sz       = v2->z;
     TexTri->Vertex[2].rhw      = 1.0 / v2->w;
     TexTri->Vertex[2].color    = 0;
     TexTri->Vertex[2].specular = 0;
     TexTri->Vertex[2].ts       = v2->s;
     TexTri->Vertex[2].tt       = v2->t;

     NV_FIFO_FREE( nvdev, TexTri, 1 );

     TexTri->DrawTriangle3D     = (2 << 8) | (1 << 4) | 0;
}
#endif

static bool
nvTextureTriangles( void *drv, void *dev, DFBVertex *vertices,
                    int num, DFBTriangleFormation formation )
{
#if 0
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     CardState        *state = nvdev->state;
     volatile RivaTexturedTriangle05 *TexTri = nvdrv->TexTri;
     int               i;

     for ( i = 0; i < num ; i++ )
     {
          vertices[i].x -= 0.5;
          vertices[i].y -= 0.5;
          vertices[i].w  = 1.0 / vertices[i].w;
     }

     NV_FIFO_FREE( nvdev, TexTri, 8 );
     
     TexTri->TextureOffset = state->source->front_buffer->video.offset;
     TexTri->TextureFormat = state->source->front_buffer->video.pitch;
     TexTri->TextureFilter = 0;
     TexTri->Blend         = 0;
     TexTri->Control       = 0;
     TexTri->FogColor      = 0xffffffff;

     switch (formation)
     {
          case DTTF_LIST:
               for (i = 0; i < num; i += 3)
                    TextureTriangle( nvdrv, nvdev,
                         &vertices[i], &vertices[i+1], &vertices[i+2] );
          break;

          case DTTF_STRIP:
               TextureTriangle( nvdrv, nvdev, 
                         &vertices[0], &vertices[1], &vertices[2] );
               
               for (i = 0; i < num; i++)
                    TextureTriangle( nvdrv, nvdev, 
                         &vertices[i-2], &vertices[i-1], &vertices[i] );
          break;

          case DTTF_FAN:
               TextureTriangle( nvdrv, nvdev,
                         &vertices[0], &vertices[1], &vertices[2] );

               for (i = 0; i < num; i++)
                    TextureTriangle( nvdrv, nvdev,
                         &vertices[0], &vertices[i-1], &vertices[i] );
          break;

          default:
          return false;
     }

     nv_waitidle( nvdrv, nvdev );
#endif
     return true;
}
     

static void nvAfterSetVar( void *drv, void *dev )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     volatile __u32   *PRAMIN = nvdrv->PRAMIN;
     volatile __u32   *FIFO   = nvdrv->FIFO;

     /* write object configuration */
     NV_LOAD_TABLE( PRAMIN, PRAMIN )

     /* set pixelformat */
     switch (dfb_primary_layer_pixelformat()) {
          case DSPF_ARGB1555:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_ARGB1555 )
               break;
          case DSPF_RGB16:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB16 )
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB32 )
               break;
          default:
               break;
     }

     /* put objects into subchannels */
     NV_LOAD_TABLE( FIFO, FIFO )
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
     info->version.minor = 2;

     info->driver_data_size = sizeof (NVidiaDriverData);
     info->device_data_size = sizeof (NVidiaDeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;

     nvdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!nvdrv->mmio_base)
          return DFB_IO;

     nvdrv->device = device;

     nvdrv->PGRAPH = (volatile __u32*) (nvdrv->mmio_base + 0x400000);
     nvdrv->PRAMIN = (volatile __u32*) (nvdrv->mmio_base + 0x710000);
     nvdrv->FIFO   = (volatile __u32*) (nvdrv->mmio_base + 0x800000);
     nvdrv->PMC    = (volatile __u32*) (nvdrv->mmio_base + 0x000000);

     nvdrv->Rop         = (volatile NVRop                *) &nvdrv->FIFO[0x0000/4];
     nvdrv->Clip        = (volatile NVClip               *) &nvdrv->FIFO[0x2000/4];
     nvdrv->Pattern     = (volatile NVPattern            *) &nvdrv->FIFO[0x4000/4];
     nvdrv->Triangle    = (volatile NVTriangle           *) &nvdrv->FIFO[0x6000/4];
     nvdrv->Blt         = (volatile NVScreenBlt          *) &nvdrv->FIFO[0x8000/4];
     nvdrv->Rectangle   = (volatile NVRectangle          *) &nvdrv->FIFO[0xA000/4];
     nvdrv->Line        = (volatile NVLine               *) &nvdrv->FIFO[0xC000/4];
     nvdrv->ScaledImage = (volatile NVScaledImage        *) &nvdrv->FIFO[0xE000/4];
     //nvdrv->TexTri      = (volatile NVTexturedTriangle05 *) &nvdrv->FIFO[0xE000/4];
     
     funcs->CheckState    = nvCheckState;
     funcs->SetState      = nvSetState;
     funcs->EngineSync    = nvEngineSync;
     funcs->AfterSetVar   = nvAfterSetVar;

     funcs->FillRectangle    = nvFillRectangle;
     funcs->FillTriangle     = nvFillTriangle;
     funcs->DrawRectangle    = nvDrawRectangle;
     funcs->DrawLine         = nvDrawLine;
     funcs->Blit             = nvBlit;
     funcs->StretchBlit      = nvStretchBlit;
     funcs->TextureTriangles = nvTextureTriangles;

     dfb_layers_register( dfb_screens_at(DSCID_PRIMARY),
                          driver_data, &nvidiaOverlayFuncs );

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     NVidiaDriverData   *nvdrv   = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData   *nvdev   = (NVidiaDeviceData*) device_data;
     volatile NVPattern *Pattern = nvdrv->Pattern;
     volatile NVRop     *Rop     = nvdrv->Rop;


     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "RIVA TNT/TNT2/GeForce" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "nVidia" );


     device_info->caps.flags    = CCF_CLIPPING;
     device_info->caps.accel    = NV4_SUPPORTED_DRAWINGFUNCTIONS |
                                  NV4_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = NV4_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = NV4_SUPPORTED_BLITTINGFLAGS;

     device_info->limits.surface_byteoffset_alignment = 32 * 4;
     device_info->limits.surface_pixelpitch_alignment = 32;

     dfb_config->pollvsync_after = 1;


     NV_FIFO_FREE( nvdev, Pattern, 5 );
     Pattern->Shape         = 0; /* 0 = 8X8, 1 = 64X1, 2 = 1X64 */
     Pattern->Color0        = 0xFFFFFFFF;
     Pattern->Color1        = 0xFFFFFFFF;
     Pattern->Monochrome[0] = 0xFFFFFFFF;
     Pattern->Monochrome[1] = 0xFFFFFFFF;

     NV_FIFO_FREE( nvdev, Rop, 1 );
     Rop->Rop3 = 0xCC;

     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;

     (void) nvdev;

     D_DEBUG( "DirectFB/nvidia: FIFO Performance Monitoring:\n" );
     D_DEBUG( "DirectFB/nvidia:  %9d nv_waitfifo calls\n",
               nvdev->waitfifo_calls );
     D_DEBUG( "DirectFB/nvidia:  %9d register writes (nv_waitfifo sum)\n",
               nvdev->waitfifo_sum );
     D_DEBUG( "DirectFB/nvidia:  %9d FIFO wait cycles (depends on CPU)\n",
               nvdev->fifo_waitcycles );
     D_DEBUG( "DirectFB/nvidia:  %9d IDLE wait cycles (depends on CPU)\n",
               nvdev->idle_waitcycles );
     D_DEBUG( "DirectFB/nvidia:  %9d FIFO space cache hits(depends on CPU)\n",
               nvdev->fifo_cache_hits );
     D_DEBUG( "DirectFB/nvidia: Conclusion:\n" );
     D_DEBUG( "DirectFB/nvidia:  Average register writes/nvidia_waitfifo"
               "call:%.2f\n",
               nvdev->waitfifo_sum/(float)(nvdev->waitfifo_calls) );
     D_DEBUG( "DirectFB/nvidia:  Average wait cycles/nvidia_waitfifo call:"
               " %.2f\n",
               nvdev->fifo_waitcycles/(float)(nvdev->waitfifo_calls) );
     D_DEBUG( "DirectFB/nvidia:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * nvdev->fifo_cache_hits/
               (float)(nvdev->waitfifo_calls)) );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;

     dfb_gfxcard_unmap_mmio( device, nvdrv->mmio_base, -1 );
}

