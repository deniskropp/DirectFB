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
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <directfb.h>

#include <core/fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/gfxcard.h>
#include <core/fbdev/fbdev.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/windows.h>

#include <misc/mem.h>

#include <gfx/convert.h>


#include "regs.h"
#include "mmio.h"
#include "matrox.h"
#include "matrox_maven.h"

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

static void spic_set_buffer( MatroxDriverData    *mdrv,
                             MatroxSpicLayerData *mspic,
                             CoreSurface         *surface );

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
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;

     config->width       = 720;
     config->height      = dfb_config->matrox_ntsc ? 480 : 576;
     config->pixelformat = DSPF_ALUT44;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;

     return DFB_OK;
}

static void
spicOnOff( MatroxDriverData    *mdrv,
           MatroxSpicLayerData *mspic,
           int                  on )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     mspic->regs.c2DATACTL = mga_in32( mmio, C2DATACTL );
     if (on)
          /* c2subpicen = 1 */
          mspic->regs.c2DATACTL |= (1 << 3);
     else
          /* c2subpicen = 0 */
          mspic->regs.c2DATACTL &= ~(1 << 3);
     mga_out32( mmio, mspic->regs.c2DATACTL, C2DATACTL );
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
     if (config->options & DLOP_ALPHACHANNEL &&
         config->options & DLOP_OPACITY)
          fail |= CLRCF_OPTIONS;

     switch (config->format) {
          case DSPF_ALUT44:
               break;
          default:
               fail |= CLRCF_FORMAT;
     }

     if (config->width != 720)
          fail |= CLRCF_WIDTH;

     if (config->height != (dfb_config->matrox_ntsc ? 480 : 576))
          fail |= CLRCF_HEIGHT;

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
     volatile __u8       *mmio  = mdrv->mmio_base;
     __u8                 r, g, b, y, cb, cr;
     int                  i;

     if (!palette || palette->num_entries != 16)
          return DFB_UNSUPPORTED;

     /* remember configuration */
     mspic->config = *config;

     spic_set_buffer( mdrv, mspic, surface );

     for (i = 0; i < 16; i++) {
          r  = palette->entries[i].r;
          g  = palette->entries[i].g;
          b  = palette->entries[i].b;
          y  =  Y_FROM_RGB( r, g, b );
          cb = CB_FROM_RGB( r, g, b );
          cr = CR_FROM_RGB( r, g, b );

          mspic->regs.c2SUBPICLUT = (cr << 24) | (cb << 16) | (y << 8) | i;
          mga_out32( mmio, mspic->regs.c2SUBPICLUT, C2SUBPICLUT );
     }

     /* enable spic */
     spicOnOff( mdrv, mspic, 1 );

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

     /* disable spic */
     spicOnOff( mdrv, mspic, 0 );

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

     dfb_surface_flip_buffers( surface );

     spic_set_buffer( mdrv, mspic, surface );

     return DFB_OK;
}

#if 0
static DFBResult
spicSetOpacity( CoreLayer *layer,
                void      *driver_data,
                void      *layer_data,
                __u8       opacity )
{
     MatroxDriverData    *mdrv  = (MatroxDriverData*) driver_data;
     MatroxSpicLayerData *mspic = (MatroxSpicLayerData*) layer_data;
     volatile __u8       *mmio  = mdrv->mmio_base;

     mspic->regs.c2DATACTL = mga_in32( mmio, C2DATACTL );
     switch (opacity) {
          case 0:
               /* c2subpicen = 0 */
               mspic->regs.c2DATACTL &= ~(1 << 3);
               break;
          case 0xFF:
               /* c2subpicen = 1 */
               mspic->regs.c2DATACTL |= (1 << 3);
               break;
          default:
               if (!(mspic->config.options & DLOP_OPACITY))
                    return DFB_UNSUPPORTED;

               /* c2subpicen = 1 */
               mspic->regs.c2DATACTL |= (1 << 3);
               break;
     }

     if (mspic->config.options & DLOP_ALPHACHANNEL)
          /* c2statickeyen = 0 */
          mspic->regs.c2DATACTL &= ~(1 << 5);
     else
          /* c2statickeyen = 1 */
          mspic->regs.c2DATACTL |= (1 << 5);

     /* c2statickey */
     mspic->regs.c2DATACTL &= ~0x1F000000;
     mspic->regs.c2DATACTL |= ((opacity + 1) << 20) & 0x1F000000;

     mga_out32( mmio, mspic->regs.c2DATACTL, C2DATACTL);

     return DFB_OK;
}
#endif

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

static void spic_set_buffer( MatroxDriverData    *mdrv,
                             MatroxSpicLayerData *mspic,
                             CoreSurface         *surface )
{
     SurfaceBuffer *front_buffer = surface->front_buffer;
     volatile __u8 *mmio         = mdrv->mmio_base;

     mspic->regs.c2SPICSTARTADD1 = front_buffer->video.offset;
     mspic->regs.c2SPICSTARTADD0 = front_buffer->video.offset + front_buffer->video.pitch;

     mga_out32( mmio, mspic->regs.c2SPICSTARTADD1, C2SPICSTARTADD1 );
     mga_out32( mmio, mspic->regs.c2SPICSTARTADD0, C2SPICSTARTADD0 );
}
