/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <stdio.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include "regs.h"
#include "mmio.h"
#include "matrox.h"

typedef struct {
     CoreLayerRegionConfig config;

     /* Stored registers */
     struct {
          /* CRTC2 sub picture */
          __u32 c2DATACTL;

          __u32 c2SPICSTARTADD0;
          __u32 c2SPICSTARTADD1;
          __u32 c2SUBPICLUT;
     } regs;
} MatroxSpicLayerData;

static void spic_calc_buffer( MatroxDriverData    *mdrv,
                              MatroxSpicLayerData *mspic,
                              CoreSurface         *surface,
                              bool                 front );
static void spic_set_buffer( MatroxDriverData    *mdrv,
                             MatroxSpicLayerData *mspic );

#define SPIC_SUPPORTED_OPTIONS   (DLOP_ALPHACHANNEL | DLOP_OPACITY)

/**********************/

static int
spicLayerDataSize()
{
     return sizeof(MatroxSpicLayerData);
}

static DFBResult
spicInitLayer( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               DFBDisplayLayerDescription *description,
               DFBDisplayLayerConfig      *config,
               DFBColorAdjustment         *adjustment )
{
     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE | DLCAPS_ALPHACHANNEL | DLCAPS_OPACITY;
     description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Matrox CRTC2 Sub-Picture" );

     /* fill out the default configuration */
     config->flags        = DLCONF_WIDTH | DLCONF_HEIGHT |
                            DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                            DLCONF_OPTIONS | DLCONF_SURFACE_CAPS;

     config->width        = 720;
     config->height       = (dfb_config->matrox_tv_std != DSETV_PAL) ? 480 : 576;
     config->pixelformat  = DSPF_ALUT44;
     config->buffermode   = DLBM_FRONTONLY;
     config->options      = DLOP_NONE;
     config->surface_caps = DSCAPS_INTERLACED;

     return DFB_OK;
}

static DFBResult
spicTestRegion( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                CoreLayerRegionConfig      *config,
                CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     if (config->options & ~SPIC_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     /* Can't have both at the same time */
     if (config->options & DLOP_ALPHACHANNEL && config->options & DLOP_OPACITY)
          fail |= CLRCF_OPTIONS;

     switch (config->opacity) {
          case 0x00:
          case 0xFF:
               break;
          default:
               if (!(config->options & DLOP_OPACITY))
                    fail |= CLRCF_OPACITY;
     }

     if (config->surface_caps & ~(DSCAPS_INTERLACED | DSCAPS_SEPARATED))
          fail |= CLRCF_SURFACE_CAPS;

     if (config->format != DSPF_ALUT44)
          fail |= CLRCF_FORMAT;

     if (config->width != 720)
          fail |= CLRCF_WIDTH;

     if (config->surface_caps & DSCAPS_INTERLACED) {
          if (config->height != ((dfb_config->matrox_tv_std != DSETV_PAL) ? 480 : 576))
               fail |= CLRCF_HEIGHT;
     } else {
          if (config->height != ((dfb_config->matrox_tv_std != DSETV_PAL) ? 240 : 288))
               fail |= CLRCF_HEIGHT;
     }

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
spicAddRegion( CoreLayer             *layer,
               void                  *driver_data,
               void                  *layer_data,
               void                  *region_data,
               CoreLayerRegionConfig *config )
{
     return DFB_OK;
}

static DFBResult
spicSetRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               void                       *region_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags  updated,
               CoreSurface                *surface,
               CorePalette                *palette )
{
     MatroxDriverData    *mdrv  = (MatroxDriverData*) driver_data;
     MatroxSpicLayerData *mspic = (MatroxSpicLayerData*) layer_data;
     MatroxDeviceData    *mdev  = mdrv->device_data;
     volatile __u8       *mmio  = mdrv->mmio_base;

     /* remember configuration */
     mspic->config = *config;

     if (updated & CLRCF_PALETTE) {
          __u8 y, cb, cr;
          int  i;

          for (i = 0; i < 16; i++) {
               RGB_TO_YCBCR( palette->entries[i].r,
                             palette->entries[i].g,
                             palette->entries[i].b,
                             y, cb, cr );

               mspic->regs.c2SUBPICLUT = (cr << 24) | (cb << 16) | (y << 8) | i;
               mga_out32( mmio, mspic->regs.c2SUBPICLUT, C2SUBPICLUT );
          }
     }

     if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_SURFACE_CAPS |
                    CLRCF_OPTIONS | CLRCF_OPACITY | CLRCF_SURFACE)) {
          spic_calc_buffer( mdrv, mspic, surface, true );
          spic_set_buffer( mdrv, mspic );

          mspic->regs.c2DATACTL = mga_in32( mmio, C2DATACTL );

          if (surface->caps & DSCAPS_INTERLACED || mdev->crtc2_separated)
               mspic->regs.c2DATACTL &= ~C2OFFSETDIVEN;
          else
               mspic->regs.c2DATACTL |= C2OFFSETDIVEN;

          if (config->opacity)
               mspic->regs.c2DATACTL |= C2SUBPICEN;
          else
               mspic->regs.c2DATACTL &= ~C2SUBPICEN;

          if (config->options & DLOP_ALPHACHANNEL)
               mspic->regs.c2DATACTL &= ~C2STATICKEYEN;
          else
               mspic->regs.c2DATACTL |= C2STATICKEYEN;

          mspic->regs.c2DATACTL &= ~C2STATICKEY;
          mspic->regs.c2DATACTL |= ((config->opacity + 1) << 20) & C2STATICKEY;

          mga_out32( mmio, mspic->regs.c2DATACTL, C2DATACTL);
     }

     return DFB_OK;
}

static DFBResult
spicRemoveRegion( CoreLayer *layer,
                  void      *driver_data,
                  void      *layer_data,
                  void      *region_data )
{
     MatroxDriverData    *mdrv  = (MatroxDriverData*) driver_data;
     MatroxSpicLayerData *mspic = (MatroxSpicLayerData*) layer_data;
     volatile __u8       *mmio  = mdrv->mmio_base;

     mspic->regs.c2DATACTL = mga_in32( mmio, C2DATACTL );

     mspic->regs.c2DATACTL &= ~C2SUBPICEN;

     mga_out32( mmio, mspic->regs.c2DATACTL, C2DATACTL );

     return DFB_OK;
}

static DFBResult
spicFlipRegion( CoreLayer           *layer,
                void                *driver_data,
                void                *layer_data,
                void                *region_data,
                CoreSurface         *surface,
                DFBSurfaceFlipFlags  flags )
{
     MatroxDriverData    *mdrv  = (MatroxDriverData*) driver_data;
     MatroxSpicLayerData *mspic = (MatroxSpicLayerData*) layer_data;

     spic_calc_buffer( mdrv, mspic, surface, false );
     spic_set_buffer( mdrv, mspic );

     dfb_surface_flip_buffers( surface, false );

     return DFB_OK;
}

DisplayLayerFuncs matroxSpicFuncs = {
     LayerDataSize:         spicLayerDataSize,
     InitLayer:             spicInitLayer,

     TestRegion:            spicTestRegion,
     AddRegion:             spicAddRegion,
     SetRegion:             spicSetRegion,
     RemoveRegion:          spicRemoveRegion,
     FlipRegion:            spicFlipRegion
};

/* internal */

static void spic_calc_buffer( MatroxDriverData    *mdrv,
                              MatroxSpicLayerData *mspic,
                              CoreSurface         *surface,
                              bool                 front )
{
     SurfaceBuffer *buffer       = front ? surface->front_buffer : surface->back_buffer;
     int            field_offset = buffer->video.pitch;

     mspic->regs.c2SPICSTARTADD1 = buffer->video.offset;
     mspic->regs.c2SPICSTARTADD0 = buffer->video.offset;

     if (surface->caps & DSCAPS_SEPARATED)
          field_offset *= surface->height / 2;

     if (surface->caps & DSCAPS_INTERLACED)
          mspic->regs.c2SPICSTARTADD0 += field_offset;
}

static void spic_set_buffer( MatroxDriverData    *mdrv,
                             MatroxSpicLayerData *mspic )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     mga_out32( mmio, mspic->regs.c2SPICSTARTADD0, C2SPICSTARTADD0 );
     mga_out32( mmio, mspic->regs.c2SPICSTARTADD1, C2SPICSTARTADD1 );
}
