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

#include <asm/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>

#include <core/graphics_driver.h>


DFB_GRAPHICS_DRIVER( nvidia )

#include "nvidia.h"


typedef struct {
     /* for fifo/performance monitoring */
     unsigned int fifo_space;
     unsigned int waitfifo_sum;
     unsigned int waitfifo_calls;
     unsigned int fifo_waitcycles;
     unsigned int idle_waitcycles;
     unsigned int fifo_cache_hits;

     __u32        Color;
} NVidiaDeviceData;

typedef struct {
     volatile __u8             *mmio_base;
     volatile __u32            *PGRAPH;
     volatile __u32            *FIFO;
     volatile __u32            *PRAMIN;

     volatile RivaRop          *Rop;
     volatile RivaClip         *Clip;
     volatile RivaPattern      *Pattern;
     volatile RivaScreenBlt    *Blt;
     volatile RivaTriangle     *Triangle;
     volatile RivaScaledImage  *ScaledImage;
     volatile RivaRectangle    *Rectangle;
     volatile RivaLine         *Line;
     volatile RivaSurface      *Surface;
} NVidiaDriverData;

#define RIVA_FIFO_FREE(nvdev,ptr,space)                     \
{                                                           \
     (nvdev)->waitfifo_sum += (space);                      \
     (nvdev)->waitfifo_calls++;                             \
                                                            \
     if ((nvdev)->fifo_space < (space)) {                   \
          do {                                              \
               (nvdev)->fifo_space = (ptr)->FifoFree >> 2;  \
               (nvdev)->fifo_waitcycles++;                  \
          } while ((nvdev)->fifo_space < (space));          \
     }                                                      \
     else {                                                 \
          (nvdev)->fifo_cache_hits++;                       \
     }                                                      \
                                                            \
     (nvdev)->fifo_space -= (space);                        \
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
               (DSDRAW_NOFX)

#define NV4_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE  | DFXL_DRAWLINE)

#define NV4_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_NOFX)

#define NV4_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT)



static inline void nv_set_clip( NVidiaDriverData *nvdrv,
                                NVidiaDeviceData *nvdev,
                                DFBRegion        *clip )
{
     volatile RivaClip *Clip   = nvdrv->Clip;
     int                width  = clip->x2 - clip->x1 + 1;
     int                height = clip->y2 - clip->y1 + 1;

     RIVA_FIFO_FREE( nvdev, Clip, 2 );

     Clip->TopLeft     = (clip->y1 << 16) | clip->x1;
     Clip->WidthHeight = (height << 16) | width;
}


static void nvCheckState( void *drv, void *dev,
                          CardState *state, DFBAccelerationMask accel )
{
     switch (state->destination->format) {
          case DSPF_RGB16:
               break;
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          switch (state->source->format) {
               case DSPF_RGB16:
                    break;
               default:
                    return;
          }

          /* if there are no other blitting flags than the supported
             and the source and destination formats are the same */
          if (!(state->blittingflags & ~NV4_SUPPORTED_BLITTINGFLAGS)  &&
              state->source  &&  state->source->format != DSPF_RGB24)
               state->accel |= NV4_SUPPORTED_BLITTINGFUNCTIONS;
     }
     else {
          /* if there are no other drawing flags than the supported */
          if (!(state->drawingflags & ~NV4_SUPPORTED_DRAWINGFLAGS))
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

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               state->set |= DFXL_FILLTRIANGLE |
                             DFXL_FILLRECTANGLE |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE |
                             DFXL_BLIT |
                             DFXL_STRETCHBLIT;
               break;

          default:
               BUG( "unexpected drawing/blitting function" );
               break;
     }

     if (state->modified & SMF_CLIP)
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (state->modified & SMF_DESTINATION) {
          /* set offset & pitch */
#if 1
          nv_waitidle( nvdrv, nvdev );
          PGRAPH[0x640/4] = state->destination->back_buffer->video.offset & 0x1FFFFFF;
          PGRAPH[0x670/4] = state->destination->back_buffer->video.pitch;
          PGRAPH[0x724/4] = (PGRAPH[0x724/4] & ~0x000000FF) | (0x00000055);
#else
          /* What's up? Setting pitch fucks it up! */
          RIVA_FIFO_FREE( Surface, 2 );
          Surface->Pitch = (state->destination->back_buffer->video.pitch << 16) | (state->destination->back_buffer->video.pitch);
          Surface->DestOffset = state->destination->back_buffer->video.offset;
#endif
          /* attempt to get scaling working */
          //PGRAPH[0x768/4] = (PGRAPH[0x768/4] & ~0x00030000) | (0x00010000);
     }

     if (state->modified & SMF_SOURCE && state->source) {
          nv_waitidle( nvdrv, nvdev );
          PGRAPH[0x644/4] = state->source->front_buffer->video.offset & 0x1FFFFFF;
          PGRAPH[0x674/4] = state->source->front_buffer->video.pitch;
     }

     if (state->modified & (SMF_DESTINATION | SMF_COLOR)) {
          switch (state->destination->format) {
               case DSPF_RGB16:
                    nvdev->Color = PIXEL_RGB16( state->color.r,
                                                state->color.g,
                                                state->color.b );
                    break;
               default:
                    BUG( "unexpected pixelformat" );
                    break;
          }
     }

     state->modified = 0;
}

static void nvFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData       *nvdrv     = (NVidiaDriverData*) drv;
     NVidiaDeviceData       *nvdev     = (NVidiaDeviceData*) dev;
     volatile RivaRectangle *Rectangle = nvdrv->Rectangle;

     RIVA_FIFO_FREE( nvdev, Rectangle, 3 );

     Rectangle->Color = nvdev->Color;

     Rectangle->TopLeft     = (rect->y << 16) | rect->x;
     Rectangle->WidthHeight = (rect->h << 16) | rect->w;
}

static void nvFillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     NVidiaDriverData      *nvdrv    = (NVidiaDriverData*) drv;
     NVidiaDeviceData      *nvdev    = (NVidiaDeviceData*) dev;
     volatile RivaTriangle *Triangle = nvdrv->Triangle;

     RIVA_FIFO_FREE( nvdev, Triangle, 4 );

     Triangle->Color = nvdev->Color;

     Triangle->TrianglePoint0 = (tri->y1 << 16) | tri->x1;
     Triangle->TrianglePoint1 = (tri->y2 << 16) | tri->x2;
     Triangle->TrianglePoint2 = (tri->y3 << 16) | tri->x3;
}

static void nvDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     NVidiaDriverData       *nvdrv     = (NVidiaDriverData*) drv;
     NVidiaDeviceData       *nvdev     = (NVidiaDeviceData*) dev;
     volatile RivaRectangle *Rectangle = nvdrv->Rectangle;

     RIVA_FIFO_FREE( nvdev, Rectangle, 9 );

     Rectangle->Color = nvdev->Color;

     Rectangle->TopLeft     = (rect->y << 16) | rect->x;
     Rectangle->WidthHeight = (1 << 16) | rect->w;

     Rectangle->TopLeft     = ((rect->y + rect->h - 1) << 16) | rect->x;
     Rectangle->WidthHeight = (1 << 16) | rect->w;

     Rectangle->TopLeft     = ((rect->y + 1) << 16) | rect->x;
     Rectangle->WidthHeight = ((rect->h - 2) << 16) | 1;

     Rectangle->TopLeft     = ((rect->y + 1) << 16) | (rect->x + rect->w - 1);
     Rectangle->WidthHeight = ((rect->h - 2) << 16) | 1;
}

static void nvDrawLine( void *drv, void *dev, DFBRegion *line )
{
     NVidiaDriverData  *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData  *nvdev = (NVidiaDeviceData*) dev;
     volatile RivaLine *Line  = nvdrv->Line;

     RIVA_FIFO_FREE( nvdev, Line, 3 );

     Line->Color = nvdev->Color;

     Line->Lin[0].point0 = (line->y1 << 16) | line->x1;
     Line->Lin[0].point1 = (line->y2 << 16) | line->x2;
}

static void nvBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     NVidiaDriverData       *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData       *nvdev = (NVidiaDeviceData*) dev;
     volatile RivaScreenBlt *Blt   = nvdrv->Blt;

     RIVA_FIFO_FREE( nvdev, Blt, 3 );

     Blt->TopLeftSrc  = (rect->y << 16) | rect->x;
     Blt->TopLeftDst  = (dy << 16) | dx;
     Blt->WidthHeight = (rect->h << 16) | rect->w;
}

static void nvStretchBlit( void *drv, void *dev, DFBRectangle *sr, DFBRectangle *dr )
{
#if 0
     NVidiaDriverData         *nvdrv       = (NVidiaDriverData*) drv;
     NVidiaDeviceData         *nvdev       = (NVidiaDeviceData*) dev;
     volatile RivaScaledImage *ScaledImage = nvdrv->ScaledImage;

     RIVA_FIFO_FREE( nvdev, ScaledImage, 10 );

     /* does not work yet */

     ScaledImage->ClipPoint = (dr->y << 16) | dr->x;
     ScaledImage->ClipSize = (dr->h << 16) | dr->w;
     ScaledImage->ImageOutPoint = (dr->y << 16) | dr->x;
     ScaledImage->ImageOutSize = (dr->h << 16) | dr->w;
     ScaledImage->DuDx = 0x8000;
     ScaledImage->DvDy = 0x8000;
     ScaledImage->ImageInSize = (card->state->source->height << 16) | card->state->source->width;
     ScaledImage->ImageInFormat = card->state->source->front_buffer->video.pitch;
     ScaledImage->ImageInOffset = card->state->source->front_buffer->video.offset;
     ScaledImage->ImageInPoint = 0;
#endif
}

static void nvAfterSetVar( void *drv, void *dev )
{
     NVidiaDriverData *nvdrv   = (NVidiaDriverData*) drv;
     volatile __u32   *PRAMIN  = nvdrv->PRAMIN;
     volatile __u32   *FIFO    = nvdrv->FIFO;

     /* write object ids and locations */
#if 0  /* AM I NUTS? */
     PRAMIN[0x00000000] = 0x80000000;   /* Rop         */
     PRAMIN[0x00000001] = 0x80011142;

     PRAMIN[0x00000002] = 0x80000001;   /* Clip        */
     PRAMIN[0x00000003] = 0x80011143;

     PRAMIN[0x00000004] = 0x80000002;   /* Pattern     */
     PRAMIN[0x00000005] = 0x80011144;

     PRAMIN[0x00000006] = 0x80000003;   /* Triangle    */
     PRAMIN[0x00000007] = 0x80011145;

     PRAMIN[0x00000008] = 0x80000004;   /* ScaledImage */
     PRAMIN[0x00000009] = 0x80011146;

     PRAMIN[0x0000000A] = 0x80000005;   /* Rectangle   */
     PRAMIN[0x0000000B] = 0x80011147;

     PRAMIN[0x0000000C] = 0x80000006;   /* Line        */
     PRAMIN[0x0000000D] = 0x80011148;

     PRAMIN[0x0000000E] = 0x80000007;   /* Blt         */
     PRAMIN[0x0000000F] = 0x80011149;

     PRAMIN[0x00000020] = 0x00000000;
     PRAMIN[0x00000021] = 0x00000000;
     PRAMIN[0x00000022] = 0x00000000;
     PRAMIN[0x00000023] = 0x00000000;
     PRAMIN[0x00000024] = 0x00000000;
     PRAMIN[0x00000025] = 0x00000000;
     PRAMIN[0x00000026] = 0x00000000;
     PRAMIN[0x00000027] = 0x00000000;
     PRAMIN[0x00000028] = 0x00000000;
     PRAMIN[0x00000029] = 0x00000000;
     PRAMIN[0x0000002A] = 0x00000000;
     PRAMIN[0x0000002B] = 0x00000000;
     PRAMIN[0x0000002C] = 0x00000000;
     PRAMIN[0x0000002D] = 0x00000000;
#endif

     /* write object configuration */
     PRAMIN[0x00000508] = 0x01008043;   /* Rop         */
     PRAMIN[0x00000509] = 0x00000C02;
     PRAMIN[0x0000050A] = 0x00000000;
     PRAMIN[0x0000050B] = 0x00000000;

     PRAMIN[0x0000050C] = 0x01008019;   /* Clip        */
     PRAMIN[0x0000050D] = 0x00000C02;
     PRAMIN[0x0000050E] = 0x00000000;
     PRAMIN[0x0000050F] = 0x00000000;

     PRAMIN[0x00000510] = 0x01008018;   /* Pattern     */
     PRAMIN[0x00000511] = 0x00000C02;
     PRAMIN[0x00000512] = 0x00000000;
     PRAMIN[0x00000513] = 0x00000000;

     PRAMIN[0x00000514] = 0x0100A01D;   /* Triangle    */
     PRAMIN[0x00000515] = 0x00000C02;
     PRAMIN[0x00000516] = 0x11401140;
     PRAMIN[0x00000517] = 0x00000000;

     PRAMIN[0x00000518] = 0x0100A037;   /* ScaledImage */ /* Surface 58 */
     PRAMIN[0x00000519] = 0x00000C02;
     PRAMIN[0x0000051A] = 0x11401140;
     PRAMIN[0x0000051B] = 0x00000000;

     PRAMIN[0x0000051C] = 0x0100A01E;   /* Rectangle   */
     PRAMIN[0x0000051D] = 0x00000C02;
     PRAMIN[0x0000051E] = 0x11401140;
     PRAMIN[0x0000051F] = 0x00000000;

     PRAMIN[0x00000520] = 0x0100A01C;   /* Line        */
     PRAMIN[0x00000521] = 0x00000C02;
     PRAMIN[0x00000522] = 0x11401140;
     PRAMIN[0x00000523] = 0x00000000;

     PRAMIN[0x00000524] = 0x0100A01F;   /* Blt         */
     PRAMIN[0x00000525] = 0x00000C02;
     PRAMIN[0x00000526] = 0x11401140;
     PRAMIN[0x00000527] = 0x00000000;


     /* put objects into subchannels */
     FIFO[0x0000/4] = 0x80000000;  /* Rop         */
     FIFO[0x2000/4] = 0x80000001;  /* Clip        */
     FIFO[0x4000/4] = 0x80000002;  /* Pattern     */
     FIFO[0x6000/4] = 0x80000010;  /* Triangle    */
     FIFO[0x8000/4] = 0x80000011;  /* ScaledImage */
     FIFO[0xA000/4] = 0x80000012;  /* Rectangle   */
     FIFO[0xC000/4] = 0x80000013;  /* Line        */
     FIFO[0xE000/4] = 0x80000014;  /* Blt         */
}


/* exported symbols */

static int
driver_get_abi_version()
{
     return DFB_GRAPHICS_DRIVER_ABI_VERSION;
}

static int
driver_probe( GraphicsDevice *device )
{
#ifdef FB_ACCEL_NV4
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_NV4:
          case FB_ACCEL_NV5:
               return 1;
     }
#endif

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
                    void                *driver_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;

     nvdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!nvdrv->mmio_base)
          return DFB_IO;

     nvdrv->PGRAPH = (__u32*)(nvdrv->mmio_base + 0x400000);
     nvdrv->PRAMIN = (__u32*)(nvdrv->mmio_base + 0x710000);
     nvdrv->FIFO   = (__u32*)(nvdrv->mmio_base + 0x800000);

     nvdrv->Rop         = (RivaRop        *)(nvdrv->FIFO + 0x0000/4);
     nvdrv->Clip        = (RivaClip       *)(nvdrv->FIFO + 0x2000/4);
     nvdrv->Pattern     = (RivaPattern    *)(nvdrv->FIFO + 0x4000/4);
     nvdrv->Triangle    = (RivaTriangle   *)(nvdrv->FIFO + 0x6000/4);
     nvdrv->ScaledImage = (RivaScaledImage*)(nvdrv->FIFO + 0x8000/4);
     //nvdrv->Surface     = (RivaSurface    *)(nvdrv->FIFO + 0x8000/4);
     nvdrv->Rectangle   = (RivaRectangle  *)(nvdrv->FIFO + 0xA000/4);
     nvdrv->Line        = (RivaLine       *)(nvdrv->FIFO + 0xC000/4);
     nvdrv->Blt         = (RivaScreenBlt  *)(nvdrv->FIFO + 0xE000/4);


     funcs->CheckState    = nvCheckState;
     funcs->SetState      = nvSetState;
     funcs->EngineSync    = nvEngineSync;
     funcs->AfterSetVar   = nvAfterSetVar;

     funcs->FillRectangle = nvFillRectangle;
     funcs->FillTriangle  = nvFillTriangle;
     funcs->DrawRectangle = nvDrawRectangle;
     funcs->DrawLine      = nvDrawLine;
     funcs->Blit          = nvBlit;
     funcs->StretchBlit   = nvStretchBlit;

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     NVidiaDriverData     *nvdrv   = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData     *nvdev   = (NVidiaDeviceData*) device_data;
     volatile RivaPattern *Pattern = nvdrv->Pattern;
     volatile RivaRop     *Rop     = nvdrv->Rop;


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


     RIVA_FIFO_FREE( nvdev, Pattern, 5 );
     Pattern->Shape         = 0; /* 0 = 8X8, 1 = 64X1, 2 = 1X64 */
     Pattern->Color0        = 0xFFFFFFFF;
     Pattern->Color1        = 0xFFFFFFFF;
     Pattern->Monochrome[0] = 0xFFFFFFFF;
     Pattern->Monochrome[1] = 0xFFFFFFFF;

     RIVA_FIFO_FREE( nvdev, Rop, 1 );
     Rop->Rop3 = 0xCC;

     return DFB_OK;
}

static DFBResult
driver_init_layers( void *driver_data,
                    void *device_data )
{
     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;

     (void) nvdev;

     DEBUGMSG( "DirectFB/nvidia: FIFO Performance Monitoring:\n" );
     DEBUGMSG( "DirectFB/nvidia:  %9d nv_waitfifo calls\n",
               nvdev->waitfifo_calls );
     DEBUGMSG( "DirectFB/nvidia:  %9d register writes (nv_waitfifo sum)\n",
               nvdev->waitfifo_sum );
     DEBUGMSG( "DirectFB/nvidia:  %9d FIFO wait cycles (depends on CPU)\n",
               nvdev->fifo_waitcycles );
     DEBUGMSG( "DirectFB/nvidia:  %9d IDLE wait cycles (depends on CPU)\n",
               nvdev->idle_waitcycles );
     DEBUGMSG( "DirectFB/nvidia:  %9d FIFO space cache hits(depends on CPU)\n",
               nvdev->fifo_cache_hits );
     DEBUGMSG( "DirectFB/nvidia: Conclusion:\n" );
     DEBUGMSG( "DirectFB/nvidia:  Average register writes/nvidia_waitfifo"
               "call:%.2f\n",
               nvdev->waitfifo_sum/(float)(nvdev->waitfifo_calls) );
     DEBUGMSG( "DirectFB/nvidia:  Average wait cycles/nvidia_waitfifo call:"
               " %.2f\n",
               nvdev->fifo_waitcycles/(float)(nvdev->waitfifo_calls) );
     DEBUGMSG( "DirectFB/nvidia:  Average fifo space cache hits: %02d%%\n",
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

