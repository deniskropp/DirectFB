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

#include <directfb.h>

#include <core/coretypes.h>
#include <core/layers.h>
#include <core/surfaces.h>

#include "cyber5k.h"
#include "cyber5k_alpha.h"
#include "cyber5k_overlay.h"

typedef struct {
     CoreLayerRegionConfig config;
} CyberUnderlayLayerData;

static void udl_set_all     ( CyberDriverData        *cdrv,
                              CyberUnderlayLayerData *cudl,
                              CoreLayerRegionConfig  *config,
                              CoreSurface            *surface );
static void udl_set_location( CyberDriverData        *cdrv,
                              CyberUnderlayLayerData *cudl,
                              CoreLayerRegionConfig  *config,
                              CoreSurface            *surface );

#define CYBER_UNDERLAY_SUPPORTED_OPTIONS     (DLOP_NONE)

/**********************/

static int
udlLayerDataSize()
{
     return sizeof(CyberUnderlayLayerData);
}

static DFBResult
udlInitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *default_config,
              DFBColorAdjustment         *default_adj )
{
     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE | DLCAPS_ALPHACHANNEL |
                         DLCAPS_OPACITY | DLCAPS_SRC_COLORKEY |
                         DLCAPS_SCREEN_LOCATION;
     description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE |
                         DLTF_BACKGROUND;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "CyberPro Underlay" );

     /* fill out the default configuration */
     default_config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                                   DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                                   DLCONF_OPTIONS;
     default_config->width       = 768;
     default_config->height      = 576;
     default_config->pixelformat = DSPF_RGB16;
     default_config->buffermode  = DLBM_FRONTONLY;
     default_config->options     = DLOP_NONE;

     /* initialize registers */
     cyber_init_overlay();

     /* workaround */
     cyber_change_overlay_fifo();
     cyber_cleanup_overlay();
     cyber_init_overlay();

     return DFB_OK;
}

static DFBResult
udlTestRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     /* check for unsupported options */
     if (config->options & ~CYBER_UNDERLAY_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     /* check pixel format */
     switch (config->format) {
          case DSPF_RGB332:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_YUY2:
               break;

          default:
               fail |= CLRCF_FORMAT;
     }

     /* check width */
     if (config->width > 1024 || config->width < 4)
          fail |= CLRCF_WIDTH;

     /* check height */
     if (config->height > 1024 || config->height < 1)
          fail |= CLRCF_HEIGHT;

     /* write back failing fields */
     if (failed)
          *failed = fail;

     /* return failure if any field failed */
     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
udlSetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags  updated,
              CoreSurface                *surface,
              CorePalette                *palette )
{
     CyberDriverData        *cdrv = (CyberDriverData*) driver_data;
     CyberUnderlayLayerData *cudl = (CyberUnderlayLayerData*) layer_data;

     /* remember configuration */
     cudl->config = *config;

     /* set up layer */
     udl_set_all( cdrv, cudl, config, surface );

     return DFB_OK;
}

static DFBResult
udlRemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
     /* disable and clean up */
     cyber_enable_overlay(0);
     cyber_cleanup_alpha();
     cyber_cleanup_overlay();

     return DFB_OK;
}

static DFBResult
udlFlipRegion( CoreLayer           *layer,
               void                *driver_data,
               void                *layer_data,
               void                *region_data,
               CoreSurface         *surface,
               DFBSurfaceFlipFlags  flags )
{
     CyberDriverData        *cdrv = (CyberDriverData*) driver_data;
     CyberUnderlayLayerData *cudl = (CyberUnderlayLayerData*) layer_data;

     dfb_surface_flip_buffers( surface, false );

     udl_set_all( cdrv, cudl, &cudl->config, surface );

     return DFB_OK;
}


DisplayLayerFuncs cyberUnderlayFuncs = {
     LayerDataSize:      udlLayerDataSize,
     InitLayer:          udlInitLayer,

     TestRegion:         udlTestRegion,
     SetRegion:          udlSetRegion,
     RemoveRegion:       udlRemoveRegion,
     FlipRegion:         udlFlipRegion
};


/* internal */

static void udl_set_all( CyberDriverData        *cdrv,
                         CyberUnderlayLayerData *cudl,
                         CoreLayerRegionConfig  *config,
                         CoreSurface            *surface )
{
     SurfaceBuffer *front = surface->front_buffer;

     /* set the pixel format */
     switch (surface->format) {
          case DSPF_RGB332:
               cyber_set_overlay_format (OVERLAY_RGB8);
               break;

          case DSPF_ARGB1555:
               cyber_set_overlay_format (OVERLAY_RGB555);
               break;

          case DSPF_RGB16:
               cyber_set_overlay_format (OVERLAY_RGB565);
               break;

          case DSPF_RGB24:
               cyber_set_overlay_format (OVERLAY_RGB888);
               break;

          case DSPF_ARGB:
          case DSPF_RGB32:
               cyber_set_overlay_format (OVERLAY_RGB8888);
               break;

          case DSPF_YUY2:
               cyber_set_overlay_format (OVERLAY_YUV422);
               break;

          default:
               D_BUG("unexpected pixelformat");
               break;
     }

     cyber_set_overlay_mode( OVERLAY_WINDOWKEY );

     /* set address */
     cyber_set_overlay_srcaddr( front->video.offset, 0, 0,
                                surface->width, front->video.pitch );

     /* set location and scaling */
     udl_set_location( cdrv, cudl, config, surface );

     /* tune fifo */
     cyber_change_overlay_fifo();

     /* set up alpha blending */
     cyber_enable_alpha( 1 );
     cyber_enable_fullscreen_alpha( 1 );
     cyber_select_blend_src1( SRC1_GRAPHICS );
     cyber_select_blend_src2( SRC2_OVERLAY1 );

     /* FIXME: find out why the opacity can't be set outside of this function */
     cyber_set_alpha_reg( 0xcc, 0xcc, 0xcc );
		
     /* turn it on */
     cyber_enable_overlay(1);
}

static void udl_set_location( CyberDriverData        *cdrv,
                              CyberUnderlayLayerData *cudl,
                              CoreLayerRegionConfig  *config,
                              CoreSurface            *surface )
{
     /* set location */
     cyber_set_overlay_window( config->dest.x, config->dest.y,
                               config->dest.x + config->dest.w - 1,
                               config->dest.y + config->dest.h - 1 );

     /* set scaling */
     cyber_set_overlay_scale( surface->height == 576 ? /* HACK: support interlaced video */
                              OVERLAY_BOBMODE : OVERLAY_WEAVEMODE,
                              surface->width, config->dest.w,
                              surface->height, config->dest.h );
}

