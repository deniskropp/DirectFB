/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include "nvidia.h"

/* for fifo/performance monitoring */
unsigned int nv_fifo_space      = 0;
unsigned int nv_waitfifo_sum    = 0;
unsigned int nv_waitfifo_calls  = 0;
unsigned int nv_fifo_waitcycles = 0;
unsigned int nv_idle_waitcycles = 0;
unsigned int nv_fifo_cache_hits = 0;


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

__u32 nv_Color = 0;

static inline void nv_waitidle()
{
     while (PGRAPH[0x1C0] & 1) {
          nv_idle_waitcycles++;
     }
}


static void nvEngineSync()
{
     nv_waitidle( mmio_base );
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



static void nv_set_clip()
{
     DFBRegion *clip = &card->state->clip;
     int width  = clip->x2 - clip->x1 + 1;
     int height = clip->y2 - clip->y1 + 1;

     RIVA_FIFO_FREE( Clip, 2 );
     Clip->TopLeft     = (clip->y1 << 16) | clip->x1;
     Clip->WidthHeight = (height << 16) | width;
}


static void nvCheckState( CardState *state, DFBAccelerationMask accel )
{
     switch (state->destination->format) {
          case DSPF_RGB16:
               break;
          default:
               return;
     }

     if (accel & 0xFFFF0000) {
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

static void nvSetState( CardState *state, DFBAccelerationMask accel )
{
     if (state != card->state) {
          state->modified |= SMF_ALL;
          state->set = 0;
          card->state = state;
     }

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
          nv_set_clip();

     if (state->modified & SMF_DESTINATION) {
          /* set offset & pitch */
#if 1
          nv_waitidle();
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
          nv_waitidle();
          PGRAPH[0x644/4] = card->state->source->front_buffer->video.offset & 0x1FFFFFF;
          PGRAPH[0x674/4] = card->state->source->front_buffer->video.pitch;
     }

     if (state->modified & (SMF_DESTINATION | SMF_COLOR)) {
          switch (state->destination->format) {
               case DSPF_RGB16:
                    nv_Color = PIXEL_RGB16( state->color.r,
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

static void nvFillRectangle( DFBRectangle *rect )
{
     RIVA_FIFO_FREE( Rectangle, 3 );

     Rectangle->Color = nv_Color;

     Rectangle->TopLeft     = (rect->y << 16) | rect->x;
     Rectangle->WidthHeight = (rect->h << 16) | rect->w;
}

static void nvFillTriangle( DFBTriangle *tri )
{
     RIVA_FIFO_FREE( Triangle, 4 );

     Triangle->Color = nv_Color;

     Triangle->TrianglePoint0 = (tri->y1 << 16) | tri->x1;
     Triangle->TrianglePoint1 = (tri->y2 << 16) | tri->x2;
     Triangle->TrianglePoint2 = (tri->y3 << 16) | tri->x3;
}

static void nvDrawRectangle( DFBRectangle *rect )
{
     RIVA_FIFO_FREE( Rectangle, 9 );

     Rectangle->Color = nv_Color;

     Rectangle->TopLeft     = (rect->y << 16) | rect->x;
     Rectangle->WidthHeight = (1 << 16) | rect->w;

     Rectangle->TopLeft     = ((rect->y + rect->h - 1) << 16) | rect->x;
     Rectangle->WidthHeight = (1 << 16) | rect->w;

     Rectangle->TopLeft     = ((rect->y + 1) << 16) | rect->x;
     Rectangle->WidthHeight = ((rect->h - 2) << 16) | 1;

     Rectangle->TopLeft     = ((rect->y + 1) << 16) | (rect->x + rect->w - 1);
     Rectangle->WidthHeight = ((rect->h - 2) << 16) | 1;
}

static void nvDrawLine( DFBRegion *line )
{
     RIVA_FIFO_FREE( Line, 3 );

     Line->Color = nv_Color;

     Line->Lin[0].point0 = (line->y1 << 16) | line->x1;
     Line->Lin[0].point1 = (line->y2 << 16) | line->x2;
}

static void nvBlit( DFBRectangle *rect, int dx, int dy )
{
     RIVA_FIFO_FREE( Blt, 3 );
     Blt->TopLeftSrc  = (rect->y << 16) | rect->x;
     Blt->TopLeftDst  = (dy << 16) | dx;
     Blt->WidthHeight = (rect->h << 16) | rect->w;
}

static void nvStretchBlit( DFBRectangle *sr, DFBRectangle *dr )
{
     RIVA_FIFO_FREE( ScaledImage, 10 );

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
}

static void nvAfterSetVar()
{
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


     RIVA_FIFO_FREE( Pattern, 5 );
     Pattern->Shape         = 0; /* 0 = 8X8, 1 = 64X1, 2 = 1X64 */
     Pattern->Color0        = 0xFFFFFFFF;
     Pattern->Color1        = 0xFFFFFFFF;
     Pattern->Monochrome[0] = 0xFFFFFFFF;
     Pattern->Monochrome[1] = 0xFFFFFFFF;

     RIVA_FIFO_FREE( Rop, 1 );
     Rop->Rop3 = 0xCC;
}


/* exported symbols */

int driver_probe( int fd, GfxCard *card )
{
#ifdef FB_ACCEL_NV4
     switch (card->fix.accel) {
          case FB_ACCEL_NV4:
          case FB_ACCEL_NV5:
               return 1;
     }
#endif
     return 0;
}

int driver_init( int fd, GfxCard *card )
{
     mmio_base = (__u8*)mmap(NULL, card->fix.mmio_len, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, card->fix.smem_len);

     if (mmio_base == MAP_FAILED) {
          PERRORMSG("DirectFB/nvidia: Unable to map mmio region!\n");
          return DFB_IO;
     }

     PGRAPH = (__u32*)(mmio_base + 0x400000);
     PRAMIN = (__u32*)(mmio_base + 0x710000);
     FIFO   = (__u32*)(mmio_base + 0x800000);

     Rop         = (RivaRop        *)(FIFO + 0x0000/4);
     Clip        = (RivaClip       *)(FIFO + 0x2000/4);
     Pattern     = (RivaPattern    *)(FIFO + 0x4000/4);
     Triangle    = (RivaTriangle   *)(FIFO + 0x6000/4);
     ScaledImage = (RivaScaledImage*)(FIFO + 0x8000/4);
     //Surface     = (RivaSurface    *)(FIFO + 0x8000/4);
     Rectangle   = (RivaRectangle  *)(FIFO + 0xA000/4);
     Line        = (RivaLine       *)(FIFO + 0xC000/4);
     Blt         = (RivaScreenBlt  *)(FIFO + 0xE000/4);

     sprintf( card->info.driver_name, "nVidia RIVA TNT/TNT2/GeForce" );
     sprintf( card->info.driver_vendor, "convergence integrated media GmbH" );

     card->info.driver_version.major = 0;
     card->info.driver_version.minor = 1;

     card->caps.flags    = CCF_CLIPPING;
     card->caps.accel    = NV4_SUPPORTED_DRAWINGFUNCTIONS |
                           NV4_SUPPORTED_BLITTINGFUNCTIONS;
     card->caps.drawing  = NV4_SUPPORTED_DRAWINGFLAGS;
     card->caps.blitting = NV4_SUPPORTED_BLITTINGFLAGS;

     card->CheckState    = nvCheckState;
     card->SetState      = nvSetState;
     card->EngineSync    = nvEngineSync;
     card->AfterSetVar   = nvAfterSetVar;

     card->FillRectangle = nvFillRectangle;
     card->FillTriangle  = nvFillTriangle;
     card->DrawRectangle = nvDrawRectangle;
     card->DrawLine      = nvDrawLine;
     card->Blit          = nvBlit;
     card->StretchBlit   = nvStretchBlit;

     card->byteoffset_align = 32 * 4;
     card->pixelpitch_align = 32;


     dfb_config->pollvsync_after = 1;

     return DFB_OK;
}

void driver_init_layers()
{
}

void driver_deinit()
{
     DEBUGMSG( "DirectFB/nvidia: FIFO Performance Monitoring:\n" );
     DEBUGMSG( "DirectFB/nvidia:  %9d nv_waitfifo calls\n",
               nv_waitfifo_calls );
     DEBUGMSG( "DirectFB/nvidia:  %9d register writes (nv_waitfifo sum)\n",
               nv_waitfifo_sum );
     DEBUGMSG( "DirectFB/nvidia:  %9d FIFO wait cycles (depends on CPU)\n",
               nv_fifo_waitcycles );
     DEBUGMSG( "DirectFB/nvidia:  %9d IDLE wait cycles (depends on CPU)\n",
               nv_idle_waitcycles );
     DEBUGMSG( "DirectFB/nvidia:  %9d FIFO space cache hits(depends on CPU)\n",
               nv_fifo_cache_hits );
     DEBUGMSG( "DirectFB/nvidia: Conclusion:\n" );
     DEBUGMSG( "DirectFB/nvidia:  Average register writes/nvidia_waitfifo"
               "call:%.2f\n",
               nv_waitfifo_sum/(float)(nv_waitfifo_calls) );
     DEBUGMSG( "DirectFB/nvidia:  Average wait cycles/nvidia_waitfifo call:"
               " %.2f\n",
               nv_fifo_waitcycles/(float)(nv_waitfifo_calls) );
     DEBUGMSG( "DirectFB/nvidia:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * nv_fifo_cache_hits/
               (float)(nv_waitfifo_calls)) );


     munmap( (void*)mmio_base, card->fix.mmio_len);
}

