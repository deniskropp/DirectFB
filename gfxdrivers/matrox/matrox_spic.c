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
     DFBDisplayLayerConfig config;

     /* Stored registers */
     struct {
          /* CRTC2 sub picture */
          __u32 c2DATACTL;

          __u32 c2SPICSTARTADD0;
          __u32 c2SPICSTARTADD1;
          __u32 c2SUBPICLUT;
     } regs;
} MatroxSpicLayerData;

static void spic_set_buffer( MatroxDriverData *mdrv, MatroxSpicLayerData *mspic,
                             DisplayLayer *layer );

#define SPIC_SUPPORTED_OPTIONS   (DLOP_ALPHACHANNEL | DLOP_OPACITY)

/**********************/

static int
spicLayerDataSize()
{
     return sizeof(MatroxSpicLayerData);
}
     
static DFBResult
spicInitLayer( GraphicsDevice        *device,
               DisplayLayer          *layer,
               DisplayLayerInfo      *layer_info,
               DFBDisplayLayerConfig *default_config,
               DFBColorAdjustment    *default_adj,
               void                  *driver_data,
               void                  *layer_data )
{
     /* set capabilities and type */
     layer_info->desc.caps = DLCAPS_SURFACE | DLCAPS_ALPHACHANNEL | DLCAPS_OPACITY;
     layer_info->desc.type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( layer_info->desc.name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Matrox CRTC2 Sub-Picture" );

     /* fill out the default configuration */
     default_config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                                   DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                                   DLCONF_OPTIONS;

     default_config->width       = 720;
     default_config->height      = dfb_config->matrox_ntsc ? 480 : 576;
     default_config->pixelformat = DSPF_ALUT44;
     default_config->buffermode  = DLBM_FRONTONLY;
     default_config->options     = DLOP_NONE;

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
spicEnable( DisplayLayer *layer,
            void         *driver_data,
            void         *layer_data )
{
     MatroxDriverData    *mdrv  = (MatroxDriverData*) driver_data;
     MatroxSpicLayerData *mspic = (MatroxSpicLayerData*) layer_data;

     /* enable spic */
     spicOnOff( mdrv, mspic, 1 );

     return DFB_OK;
}

static DFBResult
spicDisable( DisplayLayer *layer,
             void         *driver_data,
             void         *layer_data )
{
     MatroxDriverData    *mdrv  = (MatroxDriverData*) driver_data;
     MatroxSpicLayerData *mspic = (MatroxSpicLayerData*) layer_data;

     /* disable spic */
     spicOnOff( mdrv, mspic, 0 );

     return DFB_OK;
}

static DFBResult
spicTestConfiguration( DisplayLayer               *layer,
                       void                       *driver_data,
                       void                       *layer_data,
                       DFBDisplayLayerConfig      *config,
                       DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags fail = 0;

     if (config->options & ~SPIC_SUPPORTED_OPTIONS)
          fail |= DLCONF_OPTIONS;

     /* Can't have both at the same time */
     if (config->options & DLOP_ALPHACHANNEL &&
         config->options & DLOP_OPACITY)
          fail |= DLCONF_OPTIONS;

     switch (config->pixelformat) {
          case DSPF_ALUT44:
               break;
          default:
               fail |= DLCONF_PIXELFORMAT;
     }

     if (config->width != 720)
          fail |= DLCONF_WIDTH;

     if (config->height != (dfb_config->matrox_ntsc ? 480 : 576))
          fail |= DLCONF_HEIGHT;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
spicSetConfiguration( DisplayLayer          *layer,
                      void                  *driver_data,
                      void                  *layer_data,
                      DFBDisplayLayerConfig *config )
{
     MatroxDriverData    *mdrv  = (MatroxDriverData*) driver_data;
     MatroxSpicLayerData *mspic = (MatroxSpicLayerData*) layer_data;

     /* remember configuration */
     mspic->config = *config;

     spic_set_buffer( mdrv, mspic, layer );

     return DFB_OK;
}

static DFBResult
spicSetPalette( DisplayLayer *layer,
                void         *driver_data,
                void         *layer_data,
                CorePalette  *palette )
{
     MatroxDriverData    *mdrv  = (MatroxDriverData*) driver_data;
     MatroxSpicLayerData *mspic = (MatroxSpicLayerData*) layer_data;
     volatile __u8 *mmio        = mdrv->mmio_base;
     __u8                 r, g, b, y, cb, cr;
     int                  i;

     if (palette->num_entries != 16)
          return DFB_UNSUPPORTED;

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

     return DFB_OK;
}

static DFBResult
spicSetOpacity( DisplayLayer *layer,
                void         *driver_data,
                void         *layer_data,
                __u8          opacity )
{
     MatroxDriverData    *mdrv  = (MatroxDriverData*) driver_data;
     MatroxSpicLayerData *mspic = (MatroxSpicLayerData*) layer_data;
     volatile __u8 *mmio        = mdrv->mmio_base;

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

static DFBResult
spicFlipBuffers( DisplayLayer        *layer,
                 void                *driver_data,
                 void                *layer_data,
                 DFBSurfaceFlipFlags  flags )
{
     MatroxDriverData     *mdrv    = (MatroxDriverData*) driver_data;
     MatroxSpicLayerData  *mspic   = (MatroxSpicLayerData*) layer_data;
     CoreSurface          *surface = dfb_layer_surface( layer );

     dfb_surface_flip_buffers( surface );

     spic_set_buffer( mdrv, mspic, layer );

     return DFB_OK;
}

DisplayLayerFuncs matroxSpicFuncs = {
     LayerDataSize:      spicLayerDataSize,
     InitLayer:          spicInitLayer,
     Enable:             spicEnable,
     Disable:            spicDisable,
     TestConfiguration:  spicTestConfiguration,
     SetConfiguration:   spicSetConfiguration,
     FlipBuffers:        spicFlipBuffers,
     SetPalette:         spicSetPalette,
     SetOpacity:         spicSetOpacity
};

/* internal */

static void spic_set_buffer( MatroxDriverData    *mdrv,
                             MatroxSpicLayerData *mspic,
                             DisplayLayer        *layer )
{
     CoreSurface   *surface      = dfb_layer_surface( layer );
     SurfaceBuffer *front_buffer = surface->front_buffer;
     volatile __u8 *mmio         = mdrv->mmio_base;

     mspic->regs.c2SPICSTARTADD1 = front_buffer->video.offset;
     mspic->regs.c2SPICSTARTADD0 = front_buffer->video.offset + front_buffer->video.pitch;

     mga_out32( mmio, mspic->regs.c2SPICSTARTADD1, C2SPICSTARTADD1 );
     mga_out32( mmio, mspic->regs.c2SPICSTARTADD0, C2SPICSTARTADD0 );
}
