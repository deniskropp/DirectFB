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
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>

#include <fbdev/fb.h>

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/system.h>
#include <core/gfxcard.h>
#include <core/surface.h>
#include <core/palette.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/conf.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( nvidia )

#include "nvidia.h"
#include "nvidia_regs.h"
#include "nvidia_accel.h"
#include "nvidia_objects.h"
#include "nvidia_state.h"
#include "nvidia_2d.h"
#include "nvidia_3d.h"


/* Riva TNT */
#define NV4_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV4_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE  | DFXL_DRAWLINE)

#define NV4_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_BLEND_COLORALPHA | DSBLIT_BLEND_ALPHACHANNEL | \
                DSBLIT_DEINTERLACE)

#define NV4_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES)

/* Riva TNT2 */
#define NV5_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV5_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE  | DFXL_DRAWLINE)

#define NV5_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_BLEND_COLORALPHA | DSBLIT_BLEND_ALPHACHANNEL | \
                DSBLIT_COLORIZE         | DSBLIT_DEINTERLACE)

#define NV5_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES)

/* GeForce1/GeForce2/GeForce4 */
#define NV10_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV10_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE  | DFXL_DRAWLINE)

#define NV10_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_BLEND_COLORALPHA | DSBLIT_BLEND_ALPHACHANNEL | \
                DSBLIT_COLORIZE         | DSBLIT_SRC_PREMULTCOLOR   | \
                DSBLIT_DEINTERLACE)

#define NV10_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES)

/* GeForce3/GeForce4Ti */
#define NV20_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV20_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE  | DFXL_DRAWLINE)

#define NV20_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_BLEND_COLORALPHA | DSBLIT_BLEND_ALPHACHANNEL | \
                DSBLIT_COLORIZE         | DSBLIT_SRC_PREMULTCOLOR   | \
                DSBLIT_DEINTERLACE)

#define NV20_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)
               
/* GeForceFX */
#define NV30_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV30_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE  | DFXL_DRAWLINE)

#define NV30_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_NOFX)

#define NV30_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT)


#define DSBLIT_MODULATE_ALPHA \
     (DSBLIT_BLEND_COLORALPHA | DSBLIT_BLEND_ALPHACHANNEL)

#define DSBLIT_MODULATE_COLOR \
     (DSBLIT_COLORIZE | DSBLIT_SRC_PREMULTCOLOR)

#define DSBLIT_MODULATE       \
     (DSBLIT_MODULATE_ALPHA | DSBLIT_MODULATE_COLOR)




static void nvAfterSetVar( void *driver_data,
                           void *device_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;
     volatile u8      *mmio  = nvdrv->mmio_base;
     int               i;

     nv_waitidle( nvdrv, nvdev );
     
     /* reconfigure PFIFO for selected mode (DMA/PIO) */
     nv_out32( mmio, PFIFO_CACHES, PFIFO_CACHES_REASSIGN_DISABLED );
     if (nvdev->use_dma) {
          nv_out32( mmio, PFIFO_MODE, PFIFO_MODE_CHANNEL_0_DMA );
          nv_out32( mmio, PFIFO_NEXT_CHANNEL, PFIFO_NEXT_CHANNEL_MODE_DMA );
     } else {
          nv_out32( mmio, PFIFO_MODE, PFIFO_MODE_CHANNEL_0_PIO );
          nv_out32( mmio, PFIFO_NEXT_CHANNEL, PFIFO_NEXT_CHANNEL_MODE_PIO );
     }
     nv_out32( mmio, PFIFO_CACHE1_PUSH0, PFIFO_CACHE1_PULL1_ENGINE_SW );
     nv_out32( mmio, PFIFO_CACHE1_PULL0, PFIFO_CACHE1_PULL0_ACCESS_DISABLED );
     if (nvdev->use_dma) {
          nv_out32( mmio, PFIFO_CACHE1_PUSH1, PFIFO_CACHE1_PUSH1_MODE_DMA );
     } else {
          nv_out32( mmio, PFIFO_CACHE1_PUSH1, PFIFO_CACHE1_PUSH1_MODE_PIO );
     }
     nv_out32( mmio, PFIFO_CACHE1_DMA_PUT, 0 );
     nv_out32( mmio, PFIFO_CACHE1_DMA_GET, 0 );
     if (nvdev->use_dma) {
          nv_out32( mmio, PFIFO_CACHE1_DMA_INSTANCE, ADDR_DMA_OUT );
     } else {
          nv_out32( mmio, PFIFO_CACHE1_DMA_INSTANCE, 0 );
     }
     nv_out32( mmio, PFIFO_CACHE0_PUSH0, PFIFO_CACHE0_PUSH0_ACCESS_DISABLED );
     nv_out32( mmio, PFIFO_CACHE0_PULL0, PFIFO_CACHE0_PULL0_ACCESS_DISABLED );
     nv_out32( mmio, PFIFO_RAMHT, 0x00000100             |
                                  PFIFO_RAMHT_SIZE_4K    |
                                  PFIFO_RAMHT_SEARCH_128 );
     nv_out32( mmio, PFIFO_RAMFC, 0x00000110 );
     nv_out32( mmio, PFIFO_RAMRO, 0x00000112 | PFIFO_RAMRO_SIZE_512 );
     nv_out32( mmio, PFIFO_SIZE, 0x0000FFFF );
     nv_out32( mmio, PFIFO_CACHE1_HASH, 0x0000FFFF );
     nv_out32( mmio, PFIFO_INTR_EN, PFIFO_INTR_EN_DISABLED );
     nv_out32( mmio, PFIFO_INTR, PFIFO_INTR_RESET );
     nv_out32( mmio, PFIFO_CACHE0_PULL1, PFIFO_CACHE0_PULL1_ENGINE_GRAPHICS );
     if (nvdev->use_dma) {
          if (nvdev->use_agp) {
               nv_out32( mmio, PFIFO_CACHE1_DMA_CTL, 
                                        PFIFO_CACHE1_DMA_CTL_PAGE_TABLE_PRESENT |
                                        PFIFO_CACHE1_DMA_CTL_PAGE_ENTRY_LINEAR  |
                                        PFIFO_CACHE1_DMA_CTL_TARGET_NODE_AGP );
          } else {
               nv_out32( mmio, PFIFO_CACHE1_DMA_CTL,
                                        PFIFO_CACHE1_DMA_CTL_PAGE_TABLE_PRESENT |
                                        PFIFO_CACHE1_DMA_CTL_PAGE_ENTRY_LINEAR  |
                                        PFIFO_CACHE1_DMA_CTL_TARGET_NODE_NVM );
          }
          nv_out32( mmio, PFIFO_CACHE1_DMA_LIMIT, nvdev->dma_size - 4 );
          nv_out32( mmio, PFIFO_CACHE1_ENGINE, PFIFO_CACHE1_ENGINE_0_SW );
#ifdef WORDS_BIGENDIAN
          nv_out32( mmio, PFIFO_CACHE1_DMA_FETCH,
                                        PFIFO_CACHE1_DMA_FETCH_TRIG_128_BYTES |
                                        PFIFO_CACHE1_DMA_FETCH_MAX_REQS_15    |
                                        PFIFO_CACHE1_BIG_ENDIAN );
#else
          nv_out32( mmio, PFIFO_CACHE1_DMA_FETCH,
                                        PFIFO_CACHE1_DMA_FETCH_TRIG_128_BYTES |
                                        PFIFO_CACHE1_DMA_FETCH_MAX_REQS_15    |
                                        PFIFO_CACHE1_LITTLE_ENDIAN );
#endif
          nv_out32( mmio, PFIFO_CACHE1_DMA_PUSH, 
                                        PFIFO_CACHE1_DMA_PUSH_ACCESS_ENABLED );
     } else {
          nv_out32( mmio, PFIFO_CACHE1_DMA_PUSH, 
                                        PFIFO_CACHE1_DMA_PUSH_ACCESS_DISABLED );
     }
     nv_out32( mmio, PFIFO_CACHE1_PUSH0, PFIFO_CACHE1_PUSH0_ACCESS_ENABLED );
     nv_out32( mmio, PFIFO_CACHE1_PULL0, PFIFO_CACHE1_PULL0_ACCESS_ENABLED );
     nv_out32( mmio, PFIFO_CACHE1_PULL1, PFIFO_CACHE1_PULL1_ENGINE_GRAPHICS );
     nv_out32( mmio, PFIFO_CACHES, PFIFO_CACHES_REASSIGN_ENABLED );
     nv_out32( mmio, PFIFO_INTR_EN, PFIFO_INTR_EN_CACHE_ERROR_ENABLED );

     if (nvdev->arch == NV_ARCH_10) {
          nv_out32( mmio, PGRAPH_DEBUG_1, 0x00118701 );
          nv_out32( mmio, PGRAPH_DEBUG_2, 0x24F82AD9 );

          for (i = 0; i < 8; i++) {
               nv_out32( mmio, NV10_PGRAPH_WINDOWCLIP_HORIZONTAL+i*4, 0x07FF0800 );
               nv_out32( mmio, NV10_PGRAPH_WINDOWCLIP_VERTICAL  +i*4, 0x07FF0800 );
          }

          nv_out32( mmio, NV10_PGRAPH_XFMODE0, 0x10000000 );
          nv_out32( mmio, NV10_PGRAPH_XFMODE1, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006740 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006750 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x40000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x40000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x40000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x40000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006760 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006770 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0xC5000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0xC5000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006780 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x000067A0 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006AB0 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006AC0 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006C10 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0xBF800000 );

          for (i = 0; i < 8; i++) {
               nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x7030+i*16 );
               nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x7149F2CA );
          }

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006A80 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006AA0 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00000040 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000005 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006400 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x4B7FFFFF );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006410 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0xC5000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0xC5000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006420 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x00006430 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x000064C0 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x477FFFFF );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x3F800000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x000064D0 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0xC5000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0xC5000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x000064E0 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0xC4FFF000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0xC4FFF000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_PIPE_ADDRESS, 0x000064F0 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );
          nv_out32( mmio, NV10_PGRAPH_PIPE_DATA, 0x00000000 );

          nv_out32( mmio, NV10_PGRAPH_XFMODE0, 0x30000000 );
          nv_out32( mmio, NV10_PGRAPH_XFMODE1, 0x00000004 );
          nv_out32( mmio, NV10_PGRAPH_GLOBALSTATE0, 0x10000000 );
          nv_out32( mmio, NV10_PGRAPH_GLOBALSTATE1, 0x00000000 );
     }
     
     nvdev->dma_max   = nvdev->dma_size/4 - 1;
     nvdev->dma_cur   = 0;
     nvdev->dma_free  = nvdev->dma_max;
     nvdev->dma_put   = 0;
     nvdev->dma_get   = 0;
     nvdev->fifo_free = 0;
}

static void nvEngineReset( void *drv, void *dev )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     int               i;

     /* reput objects into subchannels */
     for (i = 0; i < 8; i++) {
          nv_assign_object( nvdrv, nvdev, i, 
                            nvdev->subchannel_object[i], true );
     }

     nvdev->set        = 0;
     nvdev->dst_format = DSPF_UNKNOWN;
     nvdev->dst_pitch  = 0;
     nvdev->src_pitch  = 0;
     nvdev->beta1_set  = false;
     nvdev->beta4_set  = false;
}

static DFBResult nvEngineSync( void *drv, void *dev )
{
     nv_waitidle( (NVidiaDriverData*)drv, (NVidiaDeviceData*)dev );

     return DFB_OK;
}

static void nvFlushTextureCache( void *drv, void *dev )
{
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;

     /* invalidate source texture */
     nvdev->set &= ~SMF_SOURCE_TEXTURE;
}

static void nvEmitCommands( void *drv, void *dev )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) dev;
     
     if (nvdev->use_dma)
          nv_emitdma( nvdrv, nvdev );
}

static void nv4CheckState( void *drv, void *dev,
                           CardState *state, DFBAccelerationMask accel )
{
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;

     switch (destination->format) { 
          case DSPF_A8:
          case DSPF_LUT8:
          case DSPF_ALUT44:
          case DSPF_RGB332:
               if (DFB_BLITTING_FUNCTION( accel )) {
                    if (accel != DFXL_BLIT || state->blittingflags ||
                        source->format != destination->format)
                         return;
               } else {
                    if (state->drawingflags != DSDRAW_NOFX)
                         return;
               }
               break;

          case DSPF_RGB555:
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
               u32 size = 1 << (direct_log2( source->width  ) +
                                direct_log2( source->height ));
               
               if (size > nvdev->max_texture_size)
                    return;
          } 
          else if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL) {
               if (state->src_blend != DSBF_SRCALPHA   ||
                   state->dst_blend != DSBF_INVSRCALPHA)
                    return;
          }

          switch (source->format) { 
               case DSPF_LUT8:
               case DSPF_ALUT44:
                    if (destination->format != source->format ||
                        !dfb_palette_equal( source->palette,
                                            destination->palette ))
                         return;
                    break;
                    
               case DSPF_A8:
               case DSPF_RGB332:
                    if (destination->format != source->format)
                         return;
                    break;
               
               case DSPF_RGB555:     
               case DSPF_ARGB1555:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    break;

               case DSPF_RGB16:
                    switch (accel) {
                         case DFXL_BLIT:
                              if (state->blittingflags != DSBLIT_NOFX ||
                                  destination->format  != DSPF_RGB16)
                                   return;
                              break;
                         case DFXL_STRETCHBLIT:
                              return;
                         default:
                              break;
                    }
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

          state->accel |= NV4_SUPPORTED_DRAWINGFUNCTIONS;
     }
}

static void nv5CheckState( void *drv, void *dev,
                           CardState *state, DFBAccelerationMask accel )
{
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;

     switch (destination->format) {
          case DSPF_A8:
          case DSPF_LUT8:
          case DSPF_ALUT44:
          case DSPF_RGB332:
               if (DFB_BLITTING_FUNCTION( accel )) {
                    if (accel != DFXL_BLIT || state->blittingflags ||
                        source->format != destination->format)
                         return;
               } else {
                    if (state->drawingflags != DSDRAW_NOFX)
                         return;
               }
               break;

          case DSPF_RGB555:
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
               u32 size = 1 << (direct_log2( source->width  ) +
                                direct_log2( source->height ));
               
               if (size > nvdev->max_texture_size)
                    return;
          }
          else if (state->blittingflags & DSBLIT_MODULATE) {
               if (state->blittingflags & DSBLIT_MODULATE_ALPHA &&
                   state->blittingflags & DSBLIT_MODULATE_COLOR)
                    return;
               
               if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL) {
                    if (state->src_blend != DSBF_SRCALPHA  || 
                        state->dst_blend != DSBF_INVSRCALPHA)
                         return;
               }
          }

          switch (source->format) {
               case DSPF_LUT8:
               case DSPF_ALUT44:
                    if (destination->format != source->format          ||
                        source->front_buffer->policy == CSP_SYSTEMONLY ||
                        !dfb_palette_equal( source->palette,
                                            destination->palette ))
                         return;
                    break;
                    
               case DSPF_A8:
               case DSPF_RGB332:
                    if (destination->format != source->format ||
                        source->front_buffer->policy == CSP_SYSTEMONLY)
                         return;
                    break;

               case DSPF_RGB555:
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    /* disable host-to-video blit for simple blits */
                    if (source->front_buffer->policy == CSP_SYSTEMONLY &&
                        accel == DFXL_BLIT && !state->blittingflags    &&
                        source->format == destination->format)
                         return;
                    break;

               case DSPF_YUY2:
               case DSPF_UYVY:
                    if (accel & ~(DFXL_BLIT | DFXL_STRETCHBLIT) ||
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

          state->accel |= NV5_SUPPORTED_DRAWINGFUNCTIONS;
     }
}

static void nv10CheckState( void *drv, void *dev,
                            CardState *state, DFBAccelerationMask accel )
{
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;

     switch (destination->format) {
          case DSPF_A8:
          case DSPF_LUT8:
          case DSPF_ALUT44:
          case DSPF_RGB332:
               if (DFB_BLITTING_FUNCTION( accel )) {
                    if (accel != DFXL_BLIT || state->blittingflags ||
                        source->format != destination->format)
                         return;
               } else {
                    if (state->drawingflags != DSDRAW_NOFX)
                         return;
               }
               break;

          case DSPF_RGB555:
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
          if (state->blittingflags & ~NV10_SUPPORTED_BLITTINGFLAGS)
               return;

          if (accel == DFXL_TEXTRIANGLES) {
               u32 size = 1 << (direct_log2( source->width  ) + 
                                direct_log2( source->height ));
               
               if (size > nvdev->max_texture_size)
                    return;
          } 
          else if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL) {
               if (state->blittingflags & DSBLIT_MODULATE_COLOR) {
                    if (source->format == DSPF_ARGB && state->src_blend != DSBF_ONE)
                         return;
               }

               if (state->src_blend != DSBF_ONE &&
                   state->src_blend != DSBF_SRCALPHA)
                    return;

               if (state->dst_blend != DSBF_INVSRCALPHA)
                    return;
          }

          switch (source->format) {
               case DSPF_A8:
                    if (DFB_BYTES_PER_PIXEL(destination->format) != 4 ||
                        source->front_buffer->policy == CSP_SYSTEMONLY)
                         return;
                    break;

               case DSPF_LUT8:
               case DSPF_ALUT44:
                    if (destination->format != source->format          ||
                        source->front_buffer->policy == CSP_SYSTEMONLY ||
                        !dfb_palette_equal( source->palette,
                                            destination->palette ))
                         return;
                    break;
                    
               case DSPF_RGB332:
                    if (destination->format != source->format ||
                        source->front_buffer->policy == CSP_SYSTEMONLY)
                         return;
                    break;

               case DSPF_RGB555:
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    /* disable host-to-video blit for simple blits */
                    if (source->front_buffer->policy == CSP_SYSTEMONLY &&
                        accel == DFXL_BLIT && !state->blittingflags    &&
                        source->format == destination->format)
                         return;
                    break;

               case DSPF_YUY2:
               case DSPF_UYVY:
                    if (accel & ~(DFXL_BLIT | DFXL_STRETCHBLIT) ||
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
          if (state->drawingflags & ~NV10_SUPPORTED_DRAWINGFLAGS)
               return;

          state->accel |= NV10_SUPPORTED_DRAWINGFUNCTIONS;
     }
}

static void nv20CheckState( void *drv, void *dev,
                            CardState *state, DFBAccelerationMask accel )
{
     CoreSurface *destination = state->destination;
     CoreSurface *source      = state->source;

     switch (destination->format) {
          case DSPF_A8:
          case DSPF_LUT8:
          case DSPF_ALUT44:
          case DSPF_RGB332:
               if (DFB_BLITTING_FUNCTION( accel )) {
                    if (state->blittingflags != DSBLIT_NOFX  ||
                        source->format != destination->format)
                         return;
               } else {
                    if (state->drawingflags != DSDRAW_NOFX)
                         return;
               }
               break;

          case DSPF_RGB555:
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

          if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL) { 
               if (state->blittingflags & DSBLIT_MODULATE_COLOR) {
                    if (source->format == DSPF_ARGB && state->src_blend != DSBF_ONE)
                         return;
               }

               if (state->src_blend != DSBF_ONE &&
                   state->src_blend != DSBF_SRCALPHA)
                    return;
                    
               if (state->dst_blend != DSBF_INVSRCALPHA)
                    return;
          }

          switch (source->format) {
               case DSPF_A8:
                    if (source->front_buffer->policy == CSP_SYSTEMONLY)
                         return;
                    break;

               case DSPF_LUT8:
               case DSPF_ALUT44:
                    if (destination->format != source->format          ||
                        source->front_buffer->policy == CSP_SYSTEMONLY ||
                        !dfb_palette_equal( source->palette,
                                            destination->palette ))
                         return;
                    break;
                    
               case DSPF_RGB332:
                    if (destination->format != source->format ||
                        source->front_buffer->policy == CSP_SYSTEMONLY)
                         return;
                    break;

               case DSPF_RGB555:
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    /* disable host-to-video blit for simple blits */
                    if (source->front_buffer->policy == CSP_SYSTEMONLY &&
                        accel == DFXL_BLIT && !state->blittingflags    &&
                        source->format == destination->format)
                         return;
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

          state->accel |= NV20_SUPPORTED_DRAWINGFUNCTIONS;
     }
}

static void nv30CheckState( void *drv, void *dev,
                            CardState *state, DFBAccelerationMask accel )
{
     CoreSurface *destination = state->destination;
     CoreSurface *source      = state->source;

     switch (destination->format) {
          case DSPF_A8:
          case DSPF_LUT8:
          case DSPF_ALUT44:
          case DSPF_RGB332:
               if (DFB_DRAWING_FUNCTION( accel ) &&
                   state->drawingflags != DSDRAW_NOFX)
                    return;
               break;

          case DSPF_RGB555:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;

          case DSPF_YUY2:
          case DSPF_UYVY:
               if (accel & (DFXL_FILLTRIANGLE | DFXL_DRAWLINE) ||
                   state->drawingflags != DSDRAW_NOFX)
                    return;
               break;

          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* check unsupported blitting functions/flags */
          if ((accel & ~NV30_SUPPORTED_BLITTINGFUNCTIONS) ||
              (state->blittingflags & ~NV30_SUPPORTED_BLITTINGFLAGS))
               return;

          switch (source->format) {
               case DSPF_LUT8:
               case DSPF_ALUT44:
                    if (!dfb_palette_equal( source->palette,
                                            destination->palette ))
                         return;
               case DSPF_A8:
               case DSPF_RGB332:
               case DSPF_RGB555:
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_YUY2:
               case DSPF_UYVY:
                    if (source->front_buffer->policy == CSP_SYSTEMONLY ||
                        source->format != destination->format)
                         return;
                    break;

               default:
                    return;
          }

          state->accel |= accel;
     }
     else {
          /* check unsupported drawing flags */
          if (state->drawingflags & ~NV30_SUPPORTED_DRAWINGFLAGS)
               return;

          if (state->drawingflags & DSDRAW_BLEND &&
              state->src_blend != DSBF_SRCALPHA  &&
              state->dst_blend != DSBF_INVSRCALPHA)
               return;

          state->accel |= NV30_SUPPORTED_DRAWINGFUNCTIONS;
     }
}


static void nv4SetState( void *drv, void *dev,
                         GraphicsDeviceFuncs *funcs,
                         CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;

     nvdev->set &= ~state->modified;
     if (state->modified & SMF_COLOR)
          nvdev->set &= ~(SMF_DRAWING_COLOR | SMF_BLITTING_COLOR);

     nv_set_destination( nvdrv, nvdev, state );
     nv_set_clip( nvdrv, nvdev, state );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               nv_set_drawing_color( nvdrv, nvdev, state );
               nv_set_drawingflags( nvdrv, nvdev, state );
               
               if (state->drawingflags & DSDRAW_BLEND && nvdev->enabled_3d) {
                    nvdev->state3d[0].modified = true;
                    nv_set_blend_function( nvdrv, nvdev, state );

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

               state->set = DFXL_FILLRECTANGLE |
                            DFXL_FILLTRIANGLE  |
                            DFXL_DRAWRECTANGLE |
                            DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
          case DFXL_TEXTRIANGLES:
               nv_set_source( nvdrv, nvdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE_ALPHA) {
                    nv_set_blend_function( nvdrv, nvdev, state );
                    nv_set_blitting_color( nvdrv, nvdev, state );
               }

               nv_set_blittingflags( nvdrv, nvdev, state );
               
               if (accel == DFXL_TEXTRIANGLES) {
                    if (nvdev->src_texture != state->source->front_buffer)
                         nvdev->set &= ~SMF_SOURCE_TEXTURE;
                    
                    nvdev->src_texture = state->source->front_buffer; 
                    nvdev->state3d[1].modified = true;
                    
                    state->set = DFXL_TEXTRIANGLES;
               } else
                    state->set = DFXL_BLIT |
                                 DFXL_STRETCHBLIT;
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
     
     nvdev->set &= ~state->modified;
     if (state->modified & SMF_COLOR)
          nvdev->set &= ~(SMF_DRAWING_COLOR | SMF_BLITTING_COLOR);

     nv_set_destination( nvdrv, nvdev, state );
     nv_set_clip( nvdrv, nvdev, state );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               nv_set_drawing_color( nvdrv, nvdev, state );
               nv_set_drawingflags( nvdrv, nvdev, state );
               
               if (state->drawingflags & DSDRAW_BLEND && nvdev->enabled_3d) {
                    nvdev->state3d[0].modified = true;                  
                    nv_set_blend_function( nvdrv, nvdev, state );

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

               state->set = DFXL_FILLRECTANGLE |
                            DFXL_FILLTRIANGLE  |
                            DFXL_DRAWRECTANGLE |
                            DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
          case DFXL_TEXTRIANGLES:
               nv_set_source( nvdrv, nvdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE) {
                    nv_set_blend_function( nvdrv, nvdev, state );
                    nv_set_blitting_color( nvdrv, nvdev, state );
               }

               nv_set_blittingflags( nvdrv, nvdev, state );
               
               if (accel == DFXL_TEXTRIANGLES) {
                    if (nvdev->src_texture != state->source->front_buffer)
                         nvdev->set &= ~SMF_SOURCE_TEXTURE;
                    
                    nvdev->src_texture = state->source->front_buffer; 
                    nvdev->state3d[1].modified = true;
                    
                    state->set = DFXL_TEXTRIANGLES;
               } else {
                    if (nvdev->src_system) {
                         funcs->Blit        = nvBlitFromCPU;
                         funcs->StretchBlit = nvStretchBlitFromCPU;
                    } else {
                         funcs->Blit        = nvBlit;
                         funcs->StretchBlit = nvStretchBlit;
                    }
                    
                    state->set = DFXL_BLIT |
                                 DFXL_STRETCHBLIT;
               }
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     state->modified = 0;
}

static void nv10SetState( void *drv, void *dev,
                          GraphicsDeviceFuncs *funcs,
                          CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;

     nvdev->set &= ~state->modified;
     if (state->modified & SMF_COLOR)
          nvdev->set &= ~(SMF_DRAWING_COLOR | SMF_BLITTING_COLOR);

     nv_set_destination( nvdrv, nvdev, state );
     nv_set_clip( nvdrv, nvdev, state );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               nv_set_drawing_color( nvdrv, nvdev, state );
               nv_set_drawingflags( nvdrv, nvdev, state );
               
               if (state->drawingflags & DSDRAW_BLEND && nvdev->enabled_3d) {
                    nvdev->state3d[0].modified = true;
                    nv_set_blend_function( nvdrv, nvdev, state );

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

               state->set = DFXL_FILLRECTANGLE |
                            DFXL_FILLTRIANGLE  |
                            DFXL_DRAWRECTANGLE |
                            DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
          case DFXL_TEXTRIANGLES:
               nv_set_source( nvdrv, nvdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE) {
                    nv_set_blend_function( nvdrv, nvdev, state );
                    nv_set_blitting_color( nvdrv, nvdev, state );
               }

               nv_set_blittingflags( nvdrv, nvdev, state );
               
               if (accel == DFXL_TEXTRIANGLES) { 
                    if (nvdev->src_texture != state->source->front_buffer)
                         nvdev->set &= ~SMF_SOURCE_TEXTURE;
                    
                    nvdev->src_texture = state->source->front_buffer; 
                    nvdev->state3d[1].modified = true;
                    
                    state->set = DFXL_TEXTRIANGLES;
               } else {
                    if (nvdev->src_system) {
                         funcs->Blit        = nvBlitFromCPU;
                         funcs->StretchBlit = nvStretchBlitFromCPU;
                    } else {
                         funcs->Blit        = nvBlit;
                         funcs->StretchBlit = nvStretchBlit;
                    }
                    
                    state->set = DFXL_BLIT |
                                 DFXL_STRETCHBLIT;
               }
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

     nvdev->set &= ~state->modified;
     if (state->modified & SMF_COLOR)
          nvdev->set &= ~(SMF_DRAWING_COLOR | SMF_BLITTING_COLOR);

     nv_set_destination( nvdrv, nvdev, state );
     nv_set_clip( nvdrv, nvdev, state );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               nv_set_drawing_color( nvdrv, nvdev, state );
               nv_set_drawingflags( nvdrv, nvdev, state );

               state->set = DFXL_FILLRECTANGLE |
                            DFXL_FILLTRIANGLE  |
                            DFXL_DRAWRECTANGLE |
                            DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               nv_set_source( nvdrv, nvdev, state );
               
               if (state->blittingflags & DSBLIT_MODULATE) {
                    if (state->modified & SMF_SRC_BLEND)
                         nvdev->set &= ~SMF_BLITTING_FLAGS;
                    nv_set_blitting_color( nvdrv, nvdev, state );
               }

               nv_set_blittingflags( nvdrv, nvdev, state );

               if (nvdev->src_system) {
                    funcs->Blit        = nvBlitFromCPU;
                    funcs->StretchBlit = nvStretchBlitFromCPU;
               }
               else {
                    if (DFB_BITS_PER_PIXEL(nvdev->dst_format) == 8)
                         nvdev->scaler_filter = SCALER_IN_FORMAT_ORIGIN_CORNER |
                                                SCALER_IN_FORMAT_FILTER_NEAREST;
                    else
                         nvdev->scaler_filter = SCALER_IN_FORMAT_ORIGIN_CENTER |
                                                SCALER_IN_FORMAT_FILTER_LINEAR;

                    funcs->Blit        = nvBlit;
                    funcs->StretchBlit = nvStretchBlit;
               }

               state->set = DFXL_BLIT |
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

     nvdev->set &= ~state->modified;
     if (!nvdev->set & SMF_COLOR)
          nvdev->set &= ~(SMF_DRAWING_COLOR | SMF_BLITTING_COLOR);

     nv_set_destination( nvdrv, nvdev, state );
     nv_set_clip( nvdrv, nvdev, state );

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               nv_set_drawing_color( nvdrv, nvdev, state );
               nv_set_drawingflags( nvdrv, nvdev, state );

               state->set = DFXL_FILLRECTANGLE |
                            DFXL_FILLTRIANGLE  |
                            DFXL_DRAWRECTANGLE |
                            DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
               nv_set_source( nvdrv, nvdev, state );

               state->set = DFXL_BLIT;
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
          case FB_ACCEL_NV_10:
          case FB_ACCEL_NV_20:
          case FB_ACCEL_NV_30:
          case FB_ACCEL_NV_40:
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
               "nVidia NV4/NV5/NV10/NV20/NV30 Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "directfb.org" );
               
     snprintf( info->url,
               DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH,
               "http://www.directfb.org" );
               
     snprintf( info->license,
               DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH,
               "LGPL" );

     info->version.major = 0;
     info->version.minor = 6;

     info->driver_data_size = sizeof(NVidiaDriverData);
     info->device_data_size = sizeof(NVidiaDeviceData);
}

static void
nv_find_architecture( NVidiaDriverData *nvdrv, u32 *ret_chip, u32 *ret_arch )
{
     unsigned int vendor_id;
     unsigned int device_id;
     unsigned int arch = 0;
    
     device_id = nv_in32( nvdrv->mmio_base, 0x00 ) >> 16; /* id:rev */
     if (!device_id) {
          dfb_system_get_deviceid( &vendor_id, &device_id );
          if (vendor_id != 0x10DE) {
               D_ERROR( "DirectFB/NVidia: Could not detect device id!\n"
                        "     -> Please, specify the bus location of"
                        "        the card by using the 'busid' option.\n" );
               vendor_id = device_id = 0;
          }
     }
     
     switch (device_id & 0xFFF0) {
          case 0x0020: /* Riva TNT/TNT2 */
               arch = (device_id == 0x0020) ? NV_ARCH_04 : NV_ARCH_05;
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

     if (ret_chip)
          *ret_chip = device_id;
     if (ret_arch)
          *ret_arch = arch;
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;
     u32               arch  = 0;
    
     nvdrv->device      = device;
     nvdrv->device_data = device_data;
     
     nvdrv->fb_base = (volatile void*) dfb_gfxcard_memory_virtual( device, 0 );
     nvdrv->agp_base = (volatile void*) dfb_gfxcard_auxmemory_virtual( device, 0 );

     nvdrv->mmio_base = (volatile void*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!nvdrv->mmio_base)
          return DFB_IO;
   
     if (nvdev->use_dma) {
          nvdrv->dma_base = nvdev->use_agp ? nvdrv->agp_base : nvdrv->fb_base;
          nvdrv->dma_base += nvdev->dma_offset;
     }

     funcs->AfterSetVar   = nvAfterSetVar;
     funcs->EngineReset   = nvEngineReset;
     funcs->EngineSync    = nvEngineSync;
     funcs->EmitCommands  = nvEmitCommands;
     funcs->FillRectangle = nvFillRectangle2D; // dynamic
     funcs->FillTriangle  = nvFillTriangle2D;  // dynamic
     funcs->DrawRectangle = nvDrawRectangle2D; // dynamic
     funcs->DrawLine      = nvDrawLine2D;      // dynamic
     funcs->Blit          = nvBlit;            // dynamic

     nv_find_architecture( nvdrv, NULL, &arch );

     switch (arch) {
          case NV_ARCH_04: 
               funcs->FlushTextureCache = nvFlushTextureCache;
               funcs->CheckState        = nv4CheckState;
               funcs->SetState          = nv4SetState;
               funcs->StretchBlit       = nvStretchBlit;
               funcs->TextureTriangles  = nvTextureTriangles;
               break;
          case NV_ARCH_05:
               funcs->FlushTextureCache = nvFlushTextureCache;
               funcs->CheckState        = nv5CheckState;
               funcs->SetState          = nv5SetState;
               funcs->StretchBlit       = nvStretchBlit;
               funcs->TextureTriangles  = nvTextureTriangles;
               break;
          case NV_ARCH_10:
               funcs->FlushTextureCache = nvFlushTextureCache;
               funcs->CheckState        = nv10CheckState;
               funcs->SetState          = nv10SetState;
               funcs->StretchBlit       = nvStretchBlit;
               funcs->TextureTriangles  = nvTextureTriangles;
               break;
          case NV_ARCH_20:
               funcs->CheckState        = nv20CheckState;
               funcs->SetState          = nv20SetState;
               funcs->StretchBlit       = nvStretchBlit;
               break;
          case NV_ARCH_30:
               funcs->CheckState        = nv30CheckState;
               funcs->SetState          = nv30SetState;
               break;
          default:
               funcs->EngineReset       = NULL;
               break;
     }

     dfb_screens_hook_primary( device, driver_data,
                               &nvidiaPrimaryScreenFuncs,
                               &OldPrimaryScreenFuncs,
                               &OldPrimaryScreenDriverData );

     dfb_layers_hook_primary( device, driver_data,
                              &nvidiaPrimaryLayerFuncs,
                              &OldPrimaryLayerFuncs,
                              &OldPrimaryLayerDriverData );

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
     NVidiaDriverData *nvdrv     = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev     = (NVidiaDeviceData*) device_data;
     int               ram_total = dfb_system_videoram_length();
     int               ram_used  = dfb_gfxcard_memory_length();
     
     nv_find_architecture( nvdrv, &nvdev->chip, &nvdev->arch );

     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,
               "NV%02X (%04x)", (nvdev->chip >> 4) & 0xFF, nvdev->chip );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "nVidia" );

     switch (nvdev->arch) {
          case NV_ARCH_04:
               device_info->caps.flags    = CCF_CLIPPING;
               device_info->caps.accel    = NV4_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV4_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV4_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV4_SUPPORTED_BLITTINGFLAGS;
               break;
          case NV_ARCH_05:
               device_info->caps.flags    = CCF_CLIPPING | CCF_READSYSMEM;
               device_info->caps.accel    = NV5_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV5_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV5_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV5_SUPPORTED_BLITTINGFLAGS;
               break;
          case NV_ARCH_10:
               device_info->caps.flags    = CCF_CLIPPING | CCF_READSYSMEM;
               device_info->caps.accel    = NV10_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV10_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV10_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV10_SUPPORTED_BLITTINGFLAGS;
               break;
          case NV_ARCH_20:
               device_info->caps.flags    = CCF_CLIPPING /* | CCF_READSYSMEM*/; /* Crash reported when the flag is on. */
               device_info->caps.accel    = NV20_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV20_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV20_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV20_SUPPORTED_BLITTINGFLAGS;
               break;
          case NV_ARCH_30:
               device_info->caps.flags    = CCF_CLIPPING;
               device_info->caps.accel    = NV30_SUPPORTED_DRAWINGFUNCTIONS |
                                            NV30_SUPPORTED_BLITTINGFUNCTIONS;
               device_info->caps.drawing  = NV30_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = NV30_SUPPORTED_BLITTINGFLAGS;
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
     
     /* GeForce3 Intergrated GPU (XBox) */
     if (nvdev->chip == 0x02A0) {
          nvdev->fb_offset  = (long)nvdrv->fb_base & 0x0FFFFFFF;
          ram_total        += nvdev->fb_offset;
     }
     
     nvdev->fb_size = 1 << direct_log2( ram_total );

     nvdev->agp_offset = dfb_gfxcard_auxmemory_physical( nvdrv->device, 0 );

     if (dfb_config->dma) {
          int offset = -1;
          
          if (dfb_gfxcard_auxmemory_length() >= 64*1024) {
               offset = dfb_gfxcard_reserve_auxmemory( nvdrv->device, 64*1024 );
               if (offset < 0) {
                    D_ERROR( "DirectFB/NVidia: "
                             "couldn't reserve 64Kb of agp memory!\n" );
               }
               else {
                    nvdev->use_agp    = true;
                    nvdev->use_dma    = true;
                    nvdev->dma_size   = 64*1024;
                    nvdev->dma_offset = offset;
                    nvdrv->dma_base   = nvdrv->agp_base + offset; 
               }
          }
          
          if (offset < 0) {
               int len;
               
               len    = 32*1024 + ((ram_used - 32*1024) & 0x1FFFF);
               offset = dfb_gfxcard_reserve_memory( nvdrv->device, len );
               if (offset < 0) {
                    D_ERROR( "DirectFB/NVidia: "
                             "couldn't reserve %d bytes of video memory!\n", len );
               }
               else {
                    nvdev->use_dma    = true;
                    nvdev->dma_size   = 32*1024;
                    nvdev->dma_offset = offset;
                    nvdrv->dma_base   = nvdrv->fb_base + offset;
          
                    ram_used -= len;
               }
          }

          D_INFO ( "DirectFB/NVidia: DMA acceleration %s.\n",
                   nvdev->use_dma ? "enabled" : "disabled" );
          D_DEBUG( "DirectFB/NVidia: DMA target is %s.\n",
                   nvdev->use_agp ? "AGP" : "NVM" );
     }

     /* reserve memory for textures/color buffers */
     if (device_info->caps.accel & DFXL_TEXTRIANGLES) {
          unsigned tex_size;
          int      len, offset;

          /* if we have more than 32MB of video memory, use a 1024x1024 texture */
          if (ram_used > (32 << 20))
               tex_size = 1024*1024;
          /* if we have more than 16MB of video memory, use a 1024x512 texture */
          else if (ram_used > (16 << 20))
               tex_size = 1024*512;
          /* otherwise use a 512x512 texture */
          else
               tex_size = 512*512;

          len    = tex_size*2 + 8;
          len   += (ram_used - len) & 0xFF;
          offset = dfb_gfxcard_reserve_memory( nvdrv->device, len );
          
          if (offset < 0) {
               /* if video memory allocation failed, disable 3d acceleration */
               D_ERROR( "DirectFB/NVidia: "
                        "couldn't reserve %d bytes of video memory!\n", len );
               D_INFO(  "DirectFB/NVidia: 3D acceleration disabled.\n" );
               device_info->caps.accel &= ~DFXL_TEXTRIANGLES;
          }
          else {
               D_DEBUG( "DirectFB/NVidia: "
                        "reserved %d bytes for 3D buffers at offset 0x%08x.\n",
                         len, offset );
               
               nvdev->enabled_3d = true;
               nvdev->buf_offset[0] = offset + tex_size*2; // color
               nvdev->buf_offset[1] = offset;              // texture
               nvdev->max_texture_size = tex_size;
          }
     }
          
     if (nvdev->enabled_3d) {
          /* set default 3d state for drawing functions */
          nvdev->state3d[0].modified = true;
          nvdev->state3d[0].colorkey = 0;
          nvdev->state3d[0].offset   = nvdev->fb_offset + nvdev->buf_offset[0];
          nvdev->state3d[0].format   = TXTRI_FORMAT_CONTEXT_DMA_A     |
                                       TXTRI_FORMAT_ORIGIN_ZOH_CORNER |
                                       TXTRI_FORMAT_ORIGIN_FOH_CORNER |
                                       TXTRI_FORMAT_COLOR_R5G6B5      |
                                       TXTRI_FORMAT_U_WRAP            |
                                       TXTRI_FORMAT_V_WRAP            |
                                       0x00111000; // 2x2
          nvdev->state3d[0].filter   = TXTRI_FILTER_TEXTUREMIN_NEAREST |
                                       TXTRI_FILTER_TEXTUREMAG_NEAREST;
          nvdev->state3d[0].blend    = TXTRI_BLEND_TEXTUREMAPBLEND_MODULATEALPHA |
                                       TXTRI_BLEND_OPERATION_MUX_TALPHAMSB       |
                                       TXTRI_BLEND_SHADEMODE_FLAT                |
                                       TXTRI_BLEND_ALPHABLEND_ENABLE             |
                                       TXTRI_BLEND_SRCBLEND_SRCALPHA             |
                                       TXTRI_BLEND_DESTBLEND_INVSRCALPHA;
          nvdev->state3d[0].control  = TXTRI_CONTROL_ALPHAFUNC_ALWAYS |
                                       TXTRI_CONTROL_ORIGIN_CORNER    |
                                       TXTRI_CONTROL_ZFUNC_ALWAYS     |
                                       TXTRI_CONTROL_CULLMODE_NONE    |
                                       TXTRI_CONTROL_Z_FORMAT_FIXED;
          nvdev->state3d[0].fog      = 0;
          
          /* set default 3d state for blitting functions */
          nvdev->state3d[1].modified = true;
          nvdev->state3d[1].colorkey = 0;
          nvdev->state3d[1].offset   = nvdev->fb_offset + nvdev->buf_offset[1];
          nvdev->state3d[1].format   = TXTRI_FORMAT_CONTEXT_DMA_A     |
                                       TXTRI_FORMAT_ORIGIN_ZOH_CORNER |
                                       TXTRI_FORMAT_ORIGIN_FOH_CORNER |
                                       TXTRI_FORMAT_COLOR_R5G6B5      |
                                       TXTRI_FORMAT_U_CLAMP           |
                                       TXTRI_FORMAT_V_CLAMP           |
                                       0x00001000;
          nvdev->state3d[1].filter   = TXTRI_FILTER_TEXTUREMIN_LINEAR |
                                       TXTRI_FILTER_TEXTUREMAG_LINEAR;
          nvdev->state3d[1].blend    = TXTRI_BLEND_TEXTUREMAPBLEND_COPY      |
                                       TXTRI_BLEND_OPERATION_MUX_TALPHAMSB   |
                                       TXTRI_BLEND_SHADEMODE_GOURAUD         |
                                       TXTRI_BLEND_TEXTUREPERSPECTIVE_ENABLE |
                                       TXTRI_BLEND_SRCBLEND_ONE              |
                                       TXTRI_BLEND_DESTBLEND_ZERO;
          nvdev->state3d[1].control  = TXTRI_CONTROL_ALPHAFUNC_ALWAYS |
                                       TXTRI_CONTROL_ORIGIN_CENTER    |
                                       TXTRI_CONTROL_ZFUNC_ALWAYS     |
                                       TXTRI_CONTROL_CULLMODE_NONE    |
                                       TXTRI_CONTROL_DITHER_ENABLE    |
                                       TXTRI_CONTROL_Z_FORMAT_FIXED;
          nvdev->state3d[1].fog      = 0;

          /* clear color buffer */
          memset( dfb_gfxcard_memory_virtual( device,
                                              nvdev->buf_offset[0] ), 0xFF, 8 );
     }

     /* write dma objects configuration */
     nv_store_dma( nvdrv, OBJ_DMA_IN, ADDR_DMA_IN, 0x00,
                   DMA_FLAG_PAGE_TABLE  | DMA_FLAG_PAGE_ENTRY_LINEAR |
                   DMA_FLAG_ACCESS_RDWR | DMA_FLAG_TARGET_NVM,
                   nvdev->fb_size, 0x00000000, DMA_FRAME_ACCESS_RDWR );
     
     if (nvdev->use_dma) {
          if (nvdev->use_agp) {
               nv_store_dma( nvdrv, OBJ_DMA_OUT, ADDR_DMA_OUT, 0x02,
                             DMA_FLAG_PAGE_TABLE  | DMA_FLAG_PAGE_ENTRY_LINEAR |
                             DMA_FLAG_ACCESS_RDWR | DMA_FLAG_TARGET_AGP,
                             nvdev->dma_size, nvdev->agp_offset+nvdev->dma_offset,
                             DMA_FRAME_ACCESS_RDWR );
          }
          else {
               nv_store_dma( nvdrv, OBJ_DMA_OUT, ADDR_DMA_OUT, 0x02,
                             DMA_FLAG_PAGE_TABLE  | DMA_FLAG_PAGE_ENTRY_LINEAR |
                             DMA_FLAG_ACCESS_RDWR | DMA_FLAG_TARGET_NVM,
                             nvdev->dma_size, nvdev->fb_offset+nvdev->dma_offset,
                             DMA_FRAME_ACCESS_RDWR );
          }
     }

     /* write graphics objects configuration */
     nv_store_object( nvdrv, OBJ_SURFACES2D, ADDR_SURFACES2D, 0x42, 0, 0, 0 );
     nv_store_object( nvdrv, OBJ_CLIP, ADDR_CLIP, 0x19, 0, 0, 0 );
     nv_store_object( nvdrv, OBJ_BETA1, ADDR_BETA1, 0x12, 0, 0, 0 );
     nv_store_object( nvdrv, OBJ_BETA4, ADDR_BETA4, 0x72, 0, 0, 0 );

     nv_store_object( nvdrv, OBJ_RECTANGLE, ADDR_RECTANGLE, 0x5E,
                      CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY | 
                      CTX_FLAG_PATCH, ADDR_DMA_IN, ADDR_DMA_IN );
     nv_store_object( nvdrv, OBJ_TRIANGLE, ADDR_TRIANGLE, 0x5D,
                      CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY |
                      CTX_FLAG_PATCH, ADDR_DMA_IN, ADDR_DMA_IN );
     nv_store_object( nvdrv, OBJ_LINE, ADDR_LINE, 0x5C,
                      CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY |
                      CTX_FLAG_PATCH, ADDR_DMA_IN, ADDR_DMA_IN );

     switch (nvdev->arch)  {
          case NV_ARCH_04:
               nv_store_object( nvdrv, OBJ_SCREENBLT, ADDR_SCREENBLT, 0x1F,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY |
                                CTX_FLAG_PATCH     | CTX_FLAG_CTX_SURFACE0,
                                ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE, 0x37,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_COPY        | 
                                CTX_FLAG_PATCH     | CTX_FLAG_CONVERSION_DITHER |
                                CTX_FLAG_CTX_SURFACE0, ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE, 0x54,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_COPY |
                                CTX_FLAG_PATCH     | CTX_FLAG_CTX_SURFACE0,
                                ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D, 0x53, 0, 0, 0 );
               break;

          case NV_ARCH_05:
               nv_store_object( nvdrv, OBJ_SCREENBLT, ADDR_SCREENBLT, 0x5F,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY |
                                CTX_FLAG_PATCH     | CTX_FLAG_CTX_SURFACE0,
                                ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_IMAGEBLT, ADDR_IMAGEBLT, 0x65,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY     |
                                CTX_FLAG_PATCH     | CTX_FLAG_CONVERSION_DITHER |
                                CTX_FLAG_CTX_SURFACE0, ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE, 0x63,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY     |
                                CTX_FLAG_PATCH     | CTX_FLAG_CONVERSION_DITHER |
                                CTX_FLAG_CTX_SURFACE0, ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_STRETCHEDIMAGE, ADDR_STRETCHEDIMAGE, 0x66,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY     |
                                CTX_FLAG_PATCH     | CTX_FLAG_CONVERSION_DITHER |
                                CTX_FLAG_CTX_SURFACE0, ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE, 0x54,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY |
                                CTX_FLAG_PATCH     | CTX_FLAG_CTX_SURFACE0,
                                ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D, 0x53, 0, 0, 0 );
               break;

          case NV_ARCH_10:
               nv_store_object( nvdrv, OBJ_SCREENBLT, ADDR_SCREENBLT, 0x5F,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY |
                                CTX_FLAG_PATCH     | CTX_FLAG_CTX_SURFACE0,
                                ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_IMAGEBLT, ADDR_IMAGEBLT, 0x65,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY     |
                                CTX_FLAG_PATCH     | CTX_FLAG_CONVERSION_DITHER |
                                CTX_FLAG_CTX_SURFACE0, ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE, 0x89,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY     |
                                CTX_FLAG_PATCH     | CTX_FLAG_CONVERSION_DITHER |
                                CTX_FLAG_CTX_SURFACE0, ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_STRETCHEDIMAGE, ADDR_STRETCHEDIMAGE, 0x66,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY     |
                                CTX_FLAG_PATCH     | CTX_FLAG_CONVERSION_DITHER |
                                CTX_FLAG_CTX_SURFACE0, ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE, 0x94,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY |
                                CTX_FLAG_PATCH     | CTX_FLAG_CTX_SURFACE0,
                                ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D, 0x93, 0, 0, 0 );
               break;

          case NV_ARCH_20:
          case NV_ARCH_30:
          default:
               nv_store_object( nvdrv, OBJ_SCREENBLT, ADDR_SCREENBLT, 0x9F,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY |
                                CTX_FLAG_PATCH     | CTX_FLAG_CTX_SURFACE0,
                                ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_IMAGEBLT, ADDR_IMAGEBLT, 0x65,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY     |
                                CTX_FLAG_PATCH     | CTX_FLAG_CONVERSION_DITHER |
                                CTX_FLAG_CTX_SURFACE0, ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_SCALEDIMAGE, ADDR_SCALEDIMAGE, 0x89,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY     |
                                CTX_FLAG_PATCH     | CTX_FLAG_CONVERSION_DITHER |
                                CTX_FLAG_CTX_SURFACE0, ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_STRETCHEDIMAGE, ADDR_STRETCHEDIMAGE, 0x66,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY     |
                                CTX_FLAG_PATCH     | CTX_FLAG_CONVERSION_DITHER |
                                CTX_FLAG_CTX_SURFACE0, ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_TEXTRIANGLE, ADDR_TEXTRIANGLE, 0x94,
                                CTX_FLAG_USER_CLIP | CTX_FLAG_PATCH_SRCCOPY |
                                CTX_FLAG_PATCH     | CTX_FLAG_CTX_SURFACE0,
                                ADDR_DMA_IN, ADDR_DMA_IN );
               nv_store_object( nvdrv, OBJ_SURFACES3D, ADDR_SURFACES3D, 0x93, 0, 0, 0 );
               break;
     }

     /* assign default objects to subchannels */
     nvdev->subchannel_object[SUBC_SURFACES2D]  = OBJ_SURFACES2D;
     nvdev->subchannel_object[SUBC_CLIP]        = OBJ_CLIP;
     nvdev->subchannel_object[SUBC_RECTANGLE]   = OBJ_RECTANGLE;
     nvdev->subchannel_object[SUBC_TRIANGLE]    = OBJ_TRIANGLE;
     nvdev->subchannel_object[SUBC_LINE]        = OBJ_LINE;
     nvdev->subchannel_object[SUBC_SCREENBLT]   = OBJ_SCREENBLT;
     nvdev->subchannel_object[SUBC_SCALEDIMAGE] = OBJ_SCALEDIMAGE;
     nvdev->subchannel_object[SUBC_TEXTRIANGLE] = OBJ_TEXTRIANGLE;
    
     if (nvdev->arch == NV_ARCH_04) {
          nvdev->drawing_operation = OPERATION_COPY;
          nvdev->scaler_operation  = OPERATION_COPY;
          nvdev->scaler_filter     = 0;
          nvdev->system_operation  = OPERATION_COPY;
     } else {
          nvdev->drawing_operation = OPERATION_SRCCOPY;
          nvdev->scaler_operation  = OPERATION_SRCCOPY;
          nvdev->scaler_filter     = SCALER_IN_FORMAT_ORIGIN_CENTER |
                                     SCALER_IN_FORMAT_FILTER_LINEAR;
          nvdev->system_operation  = OPERATION_SRCCOPY;
     }

     nvAfterSetVar( driver_data, device_data );

     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;

     D_DEBUG( "DirectFB/NVidia: Performance Monitoring:\n" );
     D_DEBUG( "DirectFB/NVidia:  %9d nv_wait* calls\n",
               nvdev->waitfree_calls );
     D_DEBUG( "DirectFB/NVidia:  %9d register writes\n",
               nvdev->waitfree_sum );
     D_DEBUG( "DirectFB/NVidia:  %9d FIFO/DMA wait cycles (depends on CPU)\n",
               nvdev->free_waitcycles );
     D_DEBUG( "DirectFB/NVidia:  %9d IDLE wait cycles (depends on CPU)\n",
               nvdev->idle_waitcycles );
     D_DEBUG( "DirectFB/NVidia:  %9d FIFO/DMA space cache hits (depends on CPU)\n",
               nvdev->cache_hits );
     D_DEBUG( "DirectFB/NVidia: Conclusion:\n" );
     D_DEBUG( "DirectFB/NVidia:  Average register writes/nv_wait* call:%.2f\n",
               nvdev->waitfree_sum/(float)(nvdev->waitfree_calls ? : 1) );
     D_DEBUG( "DirectFB/NVidia:  Average wait cycles/nv_wait* call: %.2f\n",
               nvdev->free_waitcycles/(float)(nvdev->waitfree_calls ? : 1) );
     D_DEBUG( "DirectFB/NVidia:  Average FIFO/DMA space cache hits: %02d%%\n",
               (int)(100 * nvdev->cache_hits/
               (float)(nvdev->waitfree_calls ? : 1)) );
     
     /* reset channel mode to PIO to avoid crash in rivafb */
     if (nvdev->use_dma) {
          nvdev->use_dma = false;
          nvAfterSetVar( driver_data, device_data );
     }
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;

     dfb_gfxcard_unmap_mmio( device, nvdrv->mmio_base, -1 );
}

