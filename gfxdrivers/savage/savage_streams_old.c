/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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



#include <dfb_types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/io.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <math.h>

#include <directfb.h>

#include <fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/surface.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/windows.h>
#include <core/screen.h>
#include <core/screens.h>
#include <core/system.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>
#include <direct/mem.h>

#include "savage.h"
#include "savage_streams_old.h"
#include "mmio.h"
#include "savage_bci.h"

/* #define SAVAGE_DEBUG */
#ifdef SAVAGE_DEBUG
     #define SVGDBG(x...) fprintf(stderr, "savage_streams_old:" x)
#else
     #define SVGDBG(x...)
#endif

typedef struct {
     DFBRectangle          dest;
     CoreLayerRegionConfig config;
     int video_pitch;

     struct {
          /* secondary stream registers */
          u32 SSTREAM_CTRL;
          u32 SSTREAM_H_SCALE;
          u32 BLEND_CTRL;
          u32 SSTREAM_MULTIBUF;
          u32 SSTREAM_FB_ADDR0;
          u32 SSTREAM_FB_ADDR1;
          u32 SSTREAM_STRIDE;
          u32 SSTREAM_V_SCALE;
          u32 SSTREAM_V_INIT_VALUE;
          u32 SSTREAM_SRC_LINE_COUNT;
          u32 SSTREAM_WIN_START;
          u32 SSTREAM_WIN_SIZE;
          u32 SSTREAM_FB_CB_ADDR;
          u32 SSTREAM_FB_CR_ADDR;
          u32 SSTREAM_CBCR_STRIDE;
          u32 SSTREAM_FB_SIZE;
          u32 SSTREAM_FB_ADDR2;
          u32 CHROMA_KEY_CONTROL;
          u32 CHROMA_KEY_UPPER_BOUND;
     } regs;
} SavageSecondaryLayerData;

typedef struct {
     CoreLayerRegionConfig config;
     CoreSurfaceBufferLock *lock;
     bool init;

     struct {
          /* primary stream registers */
          u32 PSTREAM_CTRL;
          u32 PSTREAM_FB_ADDR0;
          u32 PSTREAM_FB_ADDR1;
          u32 PSTREAM_STRIDE;
          u32 PSTREAM_WIN_START;
          u32 PSTREAM_WIN_SIZE;
          u32 PSTREAM_FB_SIZE;
     } regs;
} SavagePrimaryLayerData;

DisplayLayerFuncs savage_pfuncs;
void *savage_pdriver_data;

/* function prototypes */
static void
secondary_set_regs (SavageDriverData         *sdrv,
                    SavageSecondaryLayerData *slay);
static void
secondary_calc_regs(SavageDriverData         *sdrv,
                    SavageSecondaryLayerData *slay,
                    CoreLayer                *layer,
                    CoreLayerRegionConfig    *config,
                    CoreSurface              *surface,
                    CoreSurfaceBufferLock    *lock);

static DFBResult
savage_secondary_calc_colorkey( SavageDriverData         *sdrv,
                                SavageSecondaryLayerData *slay,
                                CoreLayerRegionConfig    *config,
                                const DFBColorKey        *key,
                                DFBSurfacePixelFormat     format );
static void
primary_set_regs   (SavageDriverData         *sdrv,
                    SavagePrimaryLayerData   *play);
static void
primary_calc_regs  (SavageDriverData         *sdrv,
                    SavagePrimaryLayerData   *play,
                    CoreLayer                *layer,
                    CoreLayerRegionConfig    *config,
                    CoreSurface              *surface,
                    CoreSurfaceBufferLock    *lock);

static inline
void waitretrace (void)
{
     iopl(3);
     while ((inb (0x3da) & 0x8))
          ;

     while (!(inb (0x3da) & 0x8))
          ;
}

static void
streamOnOff(SavageDriverData * sdrv, int on)
{
     volatile u8 *mmio = sdrv->mmio_base;

     waitretrace();

     if (on) {
          vga_out8( mmio, 0x3d4, 0x23 );
          vga_out8( mmio, 0x3d5, 0x00 );

          vga_out8( mmio, 0x3d4, 0x26 );
          vga_out8( mmio, 0x3d5, 0x00 );

          /* turn on stream operation */
          vga_out8( mmio, 0x3d4, 0x67 );
          vga_out8( mmio, 0x3d5, 0x0c );
     }
     else {
          /* turn off stream operation */
          vga_out8( mmio, 0x3d4, 0x67 );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) & ~0x0c );
     }
}

/* secondary layer functions */
static int
savageSecondaryLayerDataSize( void )
{
     SVGDBG("savageSecondaryLayerDataSize\n");
     return sizeof(SavageSecondaryLayerData);
}

static DFBResult
savageSecondaryInitLayer( CoreLayer                  *layer,
                          void                       *driver_data,
                          void                       *layer_data,
                          DFBDisplayLayerDescription *description,
                          DFBDisplayLayerConfig      *default_config,
                          DFBColorAdjustment         *default_adj )
{
     SVGDBG("savageSecondaryInitLayer\n");

     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE | DLCAPS_SCREEN_LOCATION |
                         DLCAPS_BRIGHTNESS | DLCAPS_CONTRAST |
                         DLCAPS_OPACITY | DLCAPS_HUE | DLCAPS_SATURATION |
                         DLCAPS_ALPHACHANNEL | DLCAPS_SRC_COLORKEY |
                         DLCAPS_DST_COLORKEY;
     description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf(description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH,
              "Savage Secondary Stream");

     /* fill out the default configuration */
     default_config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                                   DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                                   DLCONF_OPTIONS;
     default_config->width       = 640;
     default_config->height      = 480;
     default_config->pixelformat = DSPF_YUY2;
     default_config->buffermode  = DLBM_FRONTONLY;
     default_config->options     = DLOP_NONE;

     /* fill out default color adjustment,
        only fields set in flags will be accepted from applications */
     default_adj->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST |
                               DCAF_HUE | DCAF_SATURATION;
     default_adj->brightness = 0x8000;
     default_adj->contrast   = 0x8000;
     default_adj->hue        = 0x8000;
     default_adj->saturation = 0x8000;

     return DFB_OK;
}

static DFBResult
savageSecondaryRemoveRegion( CoreLayer *layer,
                             void      *driver_data,
                             void      *layer_data,
                             void      *region_data )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     volatile u8 *mmio = sdrv->mmio_base;

     SVGDBG("savageSecondaryRemoveRegion\n");

     /* put primary stream on top of secondary stream */
     savage_out32(mmio, SAVAGE_BLEND_CONTROL,
                  SAVAGE_BLEND_CONTROL_COMP_PSTREAM);

     return DFB_OK;
}

static DFBResult
savageSecondaryTestRegion( CoreLayer                  *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           CoreLayerRegionConfig      *config,
                           CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     SVGDBG("savageSecondaryTestRegion\n");

     /* check for unsupported options */
     /* savage only supports one option at a time */
     switch (config->options) {
          case DLOP_NONE:
          case DLOP_ALPHACHANNEL:
          case DLOP_SRC_COLORKEY:
          case DLOP_DST_COLORKEY:
          case DLOP_OPACITY:
               break;
          default:
               fail |= CLRCF_OPTIONS;
               break;
     }

     /* check pixel format */
     switch (config->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               break;
          default:
               fail |= CLRCF_FORMAT;
     }

     /* check width */
     if (config->width > 2048 || config->width < 1)
          fail |= CLRCF_WIDTH;

     /* check height */
     if (config->height > 2048 || config->height < 1)
          fail |= CLRCF_HEIGHT;

     switch (config->format) {
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               /* secondary is in YUV format */
               if (config->dest.w < (config->source.w / 2))
                    fail |= CLRCF_SOURCE | CLRCF_DEST;
               if (config->dest.h < (config->source.h / 32))
                    fail |= CLRCF_SOURCE | CLRCF_DEST;
               break;
          default:
               /* secondary is in RGB format */
               if (config->dest.w < config->source.w)
                    fail |= CLRCF_SOURCE | CLRCF_DEST;
               if (config->dest.h < config->source.h)
                    fail |= CLRCF_SOURCE | CLRCF_DEST;
               break;
     }

     /* write back failing fields */
     if (failed)
          *failed = fail;

     /* return failure if any field failed */
     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
savageSecondarySetRegion( CoreLayer                  *layer,
                          void                       *driver_data,
                          void                       *layer_data,
                          void                       *region_data,
                          CoreLayerRegionConfig      *config,
                          CoreLayerRegionConfigFlags  updated,
                          CoreSurface                *surface,
                          CorePalette                *palette,
                          CoreSurfaceBufferLock      *lock )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;

     SVGDBG("savageSecondarySetConfiguration w:%i h:%i bpp:%i\n",
            config->width, config->height,
            DFB_BYTES_PER_PIXEL(config->pixelformat) * 8);

     /* remember configuration */
     slay->config = *config;

     switch (config->options & (DLOP_SRC_COLORKEY | DLOP_DST_COLORKEY)) {
          case DLOP_SRC_COLORKEY:
               savage_secondary_calc_colorkey(sdrv, slay, config, &config->src_key,
                                              config->format);
               break;
          case DLOP_DST_COLORKEY:
               savage_secondary_calc_colorkey(sdrv, slay, config, &config->dst_key,
                                              dfb_primary_layer_pixelformat());
               break;
          default:
               slay->regs.CHROMA_KEY_CONTROL = 0;
               slay->regs.CHROMA_KEY_UPPER_BOUND = 0;
               break;
     }

     secondary_calc_regs(sdrv, slay, layer, config, surface, lock);

     secondary_set_regs(sdrv, slay);

     return DFB_OK;
}

static void
savage_secondary_calc_opacity( SavageDriverData         *sdrv,
                               SavageSecondaryLayerData *slay,
                               CoreLayerRegionConfig    *config )
{
     u8 opacity = config->opacity;

     switch (opacity) {
          case 0:
               /* put primary stream on top of secondary stream */
               slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_PSTREAM;
               break;
          case 0xFF:
               /* put secondary stream on top of primary stream */
               slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_SSTREAM;
               break;
          default:
               /* reverse opacity */
               opacity = 7 - (opacity >> 5);

               /* for some reason opacity can not be zero */
               if (opacity == 0)
                    opacity = 1;

               /* dissolve primary and secondary stream */
               slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_DISSOLVE | KP_KS(opacity,0);
               break;
     }
}

static DFBResult
savage_secondary_calc_colorkey( SavageDriverData         *sdrv,
                                SavageSecondaryLayerData *slay,
                                CoreLayerRegionConfig    *config,
                                const DFBColorKey        *key,
                                DFBSurfacePixelFormat     format )
{
     u32 reg;

     switch (format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
               reg = 0x14000000;
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
               reg = 0x17000000;
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     slay->regs.CHROMA_KEY_CONTROL = reg | (key->r << 16) | (key->g << 8) | (key->b);
     slay->regs.CHROMA_KEY_UPPER_BOUND = 0x00000000 | (key->r << 16) | (key->g << 8) | (key->b);

     return DFB_OK;
}


static DFBResult
savageSecondaryFlipRegion( CoreLayer             *layer,
                           void                  *driver_data,
                           void                  *layer_data,
                           void                  *region_data,
                           CoreSurface           *surface,
                           DFBSurfaceFlipFlags    flags,
                           const DFBRegion       *left_update,
                           CoreSurfaceBufferLock *left_lock,
                           const DFBRegion       *right_update,
                           CoreSurfaceBufferLock *right_lock )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;

     SVGDBG("savageSecondaryFlipRegion\n");

     dfb_surface_flip( surface, false );

     secondary_calc_regs(sdrv, slay, layer, &slay->config, surface, left_lock);
     secondary_set_regs(sdrv, slay);

     if (flags & DSFLIP_WAIT)
          dfb_screen_wait_vsync( dfb_screens_at( DSCID_PRIMARY ) );

     return DFB_OK;
}


static DFBResult
savageSecondarySetColorAdjustment( CoreLayer          *layer,
                                   void               *driver_data,
                                   void               *layer_data,
                                   DFBColorAdjustment *adj )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;
     volatile u8      *mmio = sdrv->mmio_base;

     SVGDBG("savageSecondaryColorAdjustment b:%i c:%i h:%i s:%i\n",
            adj->brightness, adj->contrast, adj->hue, adj->saturation);

     if ((slay->regs.SSTREAM_FB_SIZE & 0x00400000) == 0) {
          /* secondary is in YUV format */
          u32 reg;
          long sat = adj->saturation * 16 / 65536;
          double hue = (adj->hue - 0x8000) * 3.141592654 / 32768.0;
          unsigned char hs1 = ((char)(sat * cos(hue))) & 0x1f;
          unsigned char hs2 = ((char)(sat * sin(hue))) & 0x1f;

          reg = 0x80008000 | (adj->brightness >> 8) |
                ((adj->contrast & 0xf800) >> 3) | (hs1 << 16) | (hs2 << 24);

          savage_out32(mmio, SAVAGE_COLOR_ADJUSTMENT, reg);

          return DFB_OK;
     }
     else {
          /* secondary is in RGB format */
          return DFB_UNSUPPORTED;
     }
}

DisplayLayerFuncs savageSecondaryFuncs = {
     .LayerDataSize      = savageSecondaryLayerDataSize,
     .InitLayer          = savageSecondaryInitLayer,
     .RemoveRegion       = savageSecondaryRemoveRegion,
     .TestRegion         = savageSecondaryTestRegion,
     .SetRegion          = savageSecondarySetRegion,
     .FlipRegion         = savageSecondaryFlipRegion,
     .SetColorAdjustment = savageSecondarySetColorAdjustment,
};

/* secondary internal */
static void
secondary_set_regs(SavageDriverData *sdrv, SavageSecondaryLayerData *slay)
{
     volatile u8 *mmio = sdrv->mmio_base;

     SVGDBG("secondary_set_regs\n");

     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_CONTROL,
                  slay->regs.SSTREAM_CTRL);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_HORIZONTAL_SCALING,
                  slay->regs.SSTREAM_H_SCALE);
     savage_out32(mmio, SAVAGE_BLEND_CONTROL,
                  slay->regs.BLEND_CTRL);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_MULTIPLE_BUFFER_SUPPORT,
                  slay->regs.SSTREAM_MULTIBUF);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS0,
                  slay->regs.SSTREAM_FB_ADDR0);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS1,
                  slay->regs.SSTREAM_FB_ADDR1);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS2,
                  slay->regs.SSTREAM_FB_ADDR2);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_SIZE,
                  slay->regs.SSTREAM_FB_SIZE);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_STRIDE,
                  slay->regs.SSTREAM_STRIDE);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_SCALING,
                  slay->regs.SSTREAM_V_SCALE);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_SOURCE_LINE_COUNT,
                  slay->regs.SSTREAM_SRC_LINE_COUNT);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_INITIAL_VALUE,
                  slay->regs.SSTREAM_V_INIT_VALUE);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_WINDOW_START,
                  slay->regs.SSTREAM_WIN_START);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_WINDOW_SIZE,
                  slay->regs.SSTREAM_WIN_SIZE);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FB_CB_ADDRESS,
                  slay->regs.SSTREAM_FB_CB_ADDR);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FB_CR_ADDRESS,
                  slay->regs.SSTREAM_FB_CR_ADDR);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_CBCR_STRIDE,
                  slay->regs.SSTREAM_CBCR_STRIDE);

     savage_out32(mmio, SAVAGE_CHROMA_KEY_CONTROL,
                  slay->regs.CHROMA_KEY_CONTROL);
     savage_out32(mmio, SAVAGE_CHROMA_KEY_UPPER_BOUND,
                  slay->regs.CHROMA_KEY_UPPER_BOUND);

     /* Set FIFO L2 on second stream. */
     {
          int pitch = slay->video_pitch;
          unsigned char cr92;

          SVGDBG("FIFO L2 pitch:%i\n", pitch);
          pitch = (pitch + 7) / 8;
          vga_out8(mmio, 0x3d4, 0x92);
          cr92 = vga_in8( mmio, 0x3d5);
          vga_out8(mmio, 0x3d5, (cr92 & 0x40) | (pitch >> 8) | 0x80);
          vga_out8(mmio, 0x3d4, 0x93);
          vga_out8(mmio, 0x3d5, pitch);
     }
}

static void
secondary_calc_regs(SavageDriverData         *sdrv,
                    SavageSecondaryLayerData *slay,
                    CoreLayer                *layer,
                    CoreLayerRegionConfig    *config,
                    CoreSurface              *surface,
                    CoreSurfaceBufferLock    *lock)
{
     DFBRectangle *source = &config->source;
     DFBRectangle *dest = &config->dest;

     /* source size */
     const int src_w = source->w;
     const int src_h = source->h;
     /* destination size */
     const int drw_w = dest->w;
     const int drw_h = dest->h;

     SVGDBG("secondary_calc_regs x:%i y:%i w:%i h:%i\n",
            dest->x, dest->y, dest->w, dest->h);
     SVGDBG("w:%i h:%i pitch:%i video.offset:%x\n",
            source->w, source->h, lock->pitch, lock->offset);

     slay->video_pitch = 1;
     slay->regs.SSTREAM_FB_SIZE = (((lock->pitch *
                                     surface->config.size.h) / 8) - 1) & 0x003fffff;

     switch (surface->config.format) {
          case DSPF_ARGB1555:
               SVGDBG("secondary set to DSPF_ARGB1555\n");
               slay->regs.SSTREAM_FB_SIZE |= 0x00400000;
               slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_KRGB16;
               break;
          case DSPF_RGB16:
               SVGDBG("secondary set to DSPF_RGB16\n");
               slay->regs.SSTREAM_FB_SIZE |= 0x00400000;
               slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB16;
               break;
          case DSPF_RGB24:
               SVGDBG("secondary set to DSPF_RGB24\n");
               slay->regs.SSTREAM_FB_SIZE |= 0x00400000;
               slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB24;
               break;
          case DSPF_RGB32:
               SVGDBG("secondary set to DSPF_RGB32\n");
               slay->regs.SSTREAM_FB_SIZE |= 0x00400000;
               slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB32;
               break;
          case DSPF_YUY2:
               SVGDBG("secondary set to DSPF_YUY2\n");
               slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YCbCr422;
               break;
          case DSPF_UYVY:
               SVGDBG("secondary set to DSPF_UYVY\n");
               slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_CbYCrY422;
               break;
          case DSPF_I420:
               SVGDBG("secondary set to DSPF_I420\n");
               slay->video_pitch = 2;
               slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YCbCr420;
               slay->regs.SSTREAM_FB_CB_ADDR = lock->offset +
                                               (surface->config.size.h * lock->pitch);
               slay->regs.SSTREAM_FB_CR_ADDR = slay->regs.SSTREAM_FB_CB_ADDR +
                                               ((surface->config.size.h * lock->pitch)/4);
               slay->regs.SSTREAM_CBCR_STRIDE = ((lock->pitch/2)
                                                 & 0x00001fff);
               break;
          case DSPF_YV12:
               SVGDBG("secondary set to DSPF_YV12\n");
               slay->video_pitch = 2;
               slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YCbCr420;
               slay->regs.SSTREAM_FB_CR_ADDR = lock->offset +
                                               surface->config.size.h * lock->pitch;
               slay->regs.SSTREAM_FB_CB_ADDR = slay->regs.SSTREAM_FB_CR_ADDR +
                                               (surface->config.size.h * lock->pitch)/4;
               slay->regs.SSTREAM_CBCR_STRIDE = ((lock->pitch/2)
                                                 & 0x00001fff);
               break;
          default:
               D_BUG("unexpected secondary pixelformat");
               return;
     }

     slay->regs.SSTREAM_CTRL |= src_w;

     switch (config->options) {
          case DLOP_ALPHACHANNEL:
               SVGDBG("secondary option DLOP_ALPHACHANNEL\n");
               slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_ALPHA;
               break;
          case DLOP_SRC_COLORKEY:
               SVGDBG("secondary option DLOP_SRC_COLORKEY\n");
               slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_SCOLORKEY;
               break;
          case DLOP_DST_COLORKEY:
               SVGDBG("secondary option DLOP_DST_COLORKEY\n");
               slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_PCOLORKEY;
               break;
          case DLOP_OPACITY:
               SVGDBG("secondary option DLOP_OPACITY\n");
               savage_secondary_calc_opacity( sdrv, slay, config );
               break;
          case DLOP_NONE:
               SVGDBG("secondary option default\n");
               slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_SSTREAM;
               break;
          default:
               D_BUG("unexpected layer option");
     }

     slay->regs.SSTREAM_H_SCALE = ((32768 * src_w) / drw_w) & 0x0000FFFF;
     slay->regs.SSTREAM_V_SCALE = ((32768 * src_h) / drw_h) & 0x000FFFFF;
     slay->regs.SSTREAM_V_INIT_VALUE = 0;
     slay->regs.SSTREAM_SRC_LINE_COUNT = src_h & 0x7ff;
     slay->regs.SSTREAM_MULTIBUF = 0;
     slay->regs.SSTREAM_FB_ADDR0 = lock->offset & 0x01ffffff;
     slay->regs.SSTREAM_FB_ADDR1 = 0;
     slay->regs.SSTREAM_FB_ADDR2 = 0;
     slay->regs.SSTREAM_STRIDE = lock->pitch & 0x00001fff;
     slay->regs.SSTREAM_WIN_START = OS_XY(dest->x, dest->y);
     slay->regs.SSTREAM_WIN_SIZE = OS_WH(drw_w, drw_h);

     /* remember pitch */
     slay->video_pitch *= lock->pitch;
}

/* primary layer functions */
static int
savagePrimaryLayerDataSize( void )
{
     SVGDBG("savagePrimaryLayerDataSize\n");
     return sizeof(SavagePrimaryLayerData);
}

static DFBResult
savagePrimaryInitLayer( CoreLayer                  *layer,
                        void                       *driver_data,
                        void                       *layer_data,
                        DFBDisplayLayerDescription *description,
                        DFBDisplayLayerConfig      *default_config,
                        DFBColorAdjustment         *default_adj )
{
     SavagePrimaryLayerData *play = (SavagePrimaryLayerData*) layer_data;
     DFBResult ret;

     SVGDBG("savagePrimaryInitLayer w:%i h:%i bpp:%i\n",
            dfb_config->mode.width, dfb_config->mode.height,
            dfb_config->mode.depth);

     /* call the original initialization function first */
     ret = savage_pfuncs.InitLayer (layer, driver_data, layer_data,
                                    description, default_config, default_adj);
     if (ret)
          return ret;

     /* set name */
     snprintf(description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH,
              "Savage Primary Stream");

     /* add support for options */
     default_config->flags |= DLCONF_OPTIONS;
     default_config->options = DLOP_NONE;

     /* add capabilities */
     description->caps |= DLCAPS_SCREEN_LOCATION;

     play->init = false;

     return DFB_OK;
}

static DFBResult
savagePrimarySetRegion( CoreLayer                  *layer,
                        void                       *driver_data,
                        void                       *layer_data,
                        void                       *region_data,
                        CoreLayerRegionConfig      *config,
                        CoreLayerRegionConfigFlags  updated,
                        CoreSurface                *surface,
                        CorePalette                *palette,
                        CoreSurfaceBufferLock      *lock )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavagePrimaryLayerData *play = (SavagePrimaryLayerData*) layer_data;
     DFBResult ret;

     SVGDBG("savagePrimarySetConfiguration w:%i h:%i bpp:%i\n",
            config->width, config->height,
            DFB_BYTES_PER_PIXEL(config->format) * 8);

     ret = savage_pfuncs.SetRegion(layer, driver_data, layer_data, region_data,
                                   config, updated, surface, palette, lock);
     if (ret != DFB_OK)
          return ret;

     /* remember configuration */
     play->config = *config;
     play->lock = lock;

     primary_calc_regs(sdrv, play, layer, config, surface, lock);
     primary_set_regs(sdrv, play);

     return DFB_OK;
}

DisplayLayerFuncs savagePrimaryFuncs = {
     .LayerDataSize = savagePrimaryLayerDataSize,
     .InitLayer     = savagePrimaryInitLayer,
     .SetRegion     = savagePrimarySetRegion,
};

/* primary internal */
static void
primary_set_regs(SavageDriverData *sdrv, SavagePrimaryLayerData *play)
{
     volatile u8 *mmio = sdrv->mmio_base;

     SVGDBG("primary_set_regs\n");

     /* turn streams on */
     streamOnOff(sdrv, 1);

     /* setup primary stream */
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_WINDOW_START,
                  play->regs.PSTREAM_WIN_START);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_WINDOW_SIZE,
                  play->regs.PSTREAM_WIN_SIZE);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_ADDRESS0,
                  play->regs.PSTREAM_FB_ADDR0);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_ADDRESS1,
                  play->regs.PSTREAM_FB_ADDR1);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_STRIDE,
                  play->regs.PSTREAM_STRIDE);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_CONTROL,
                  play->regs.PSTREAM_CTRL);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_SIZE,
                  play->regs.PSTREAM_FB_SIZE);

     if (!play->init) {
          /* tweak */
          /* fifo fetch delay register */
          vga_out8( mmio, 0x3d4, 0x85 );
          SVGDBG( "cr85: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, 0x00 );

          /* force high priority for display channel memory */
          vga_out8( mmio, 0x3d4, 0x88 );
          SVGDBG( "cr88: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) & ~0x01 );

          /* primary stream timeout register */
          vga_out8( mmio, 0x3d4, 0x71 );
          SVGDBG( "cr71: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );

          /* secondary stream timeout register */
          vga_out8( mmio, 0x3d4, 0x73 );
          SVGDBG( "cr73: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );

          /* set primary stream to use memory mapped io */
          vga_out8( mmio, 0x3d4, 0x69 );
          SVGDBG( "cr69: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) | 0x80 );

          /* enable certain registers to be loaded on vsync */
          vga_out8( mmio, 0x3d4, 0x51 );
          SVGDBG( "cr51: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) | 0x80 );

          /* setup secondary stream */
          savage_out32(mmio, SAVAGE_CHROMA_KEY_CONTROL, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_CONTROL, 0);
          savage_out32(mmio, SAVAGE_CHROMA_KEY_UPPER_BOUND, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_HORIZONTAL_SCALING, 0);
          savage_out32(mmio, SAVAGE_COLOR_ADJUSTMENT, 0);
          savage_out32(mmio, SAVAGE_BLEND_CONTROL, 1 << 24);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_MULTIPLE_BUFFER_SUPPORT, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS0, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS1, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS2, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_SIZE, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_STRIDE, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_SCALING, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_SOURCE_LINE_COUNT, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_INITIAL_VALUE, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_WINDOW_START,
                       OS_XY(0xfffe, 0xfffe));
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_WINDOW_SIZE,
                       OS_WH(10,2));

          play->init = true;
     }
}

static void
primary_calc_regs(SavageDriverData       *sdrv,
                  SavagePrimaryLayerData *play,
                  CoreLayer              *layer,
                  CoreLayerRegionConfig  *config,
                  CoreSurface            *surface,
                  CoreSurfaceBufferLock  *lock)
{
     DFBRectangle *dest = &config->dest;

     SVGDBG("primary_calc_regs w:%i h:%i pitch:%i video.offset:%x\n",
            surface->config.size.w, surface->config.size.h, lock->pitch, lock->offset);

     switch (surface->config.format) {
          case DSPF_ARGB1555:
               SVGDBG("primary set to DSPF_ARGB1555\n");
               play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_KRGB16;
               break;
          case DSPF_RGB16:
               SVGDBG("primary set to DSPF_RGB16\n");
               play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB16;
               break;
          case DSPF_RGB24:
               SVGDBG("primary set to DSPF_RGB24 (unaccelerated)\n");
               play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB24;
               break;
          case DSPF_RGB32:
               SVGDBG("primary set to DSPF_RGB32\n");
               play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB32;
               break;
          case DSPF_ARGB:
               SVGDBG("primary set to DSPF_ARGB\n");
               play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_ARGB;
               break;
          case DSPF_RGB332:
               SVGDBG("primary set to DSPF_RGB332\n");
               play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_CLUT;
               break;
          default:
               D_BUG("unexpected primary pixelformat");
               return;
     }

     play->regs.PSTREAM_FB_ADDR0 = lock->offset & 0x01ffffff;
     play->regs.PSTREAM_FB_ADDR1 = 0;
     play->regs.PSTREAM_STRIDE = lock->pitch & 0x00001fff;
     play->regs.PSTREAM_WIN_START = OS_XY(dest->x, dest->y);
     play->regs.PSTREAM_WIN_SIZE = OS_WH(dest->w, dest->h);
     play->regs.PSTREAM_FB_SIZE = (((lock->pitch *
                                     surface->config.size.h) / 8) - 1) & 0x003fffff;
}
/* end of code */
