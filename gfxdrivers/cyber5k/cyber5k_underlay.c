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

#include <core/coredefs.h>
#include <core/layers.h>
#include <core/surfaces.h>

#include "cyber5k.h"
#include "cyber5k_alpha.h"
#include "cyber5k_overlay.h"

typedef struct {
     DFBRectangle          dest;
     DFBDisplayLayerConfig config;
     int                   enabled;
} CyberUnderlayLayerData;

static void udl_set_all( CyberDriverData        *cdrv,
                         CyberUnderlayLayerData *cudl,
                         DisplayLayer           *layer );
static void udl_set_location( CyberDriverData        *cdrv,
                              CyberUnderlayLayerData *cudl,
                              DisplayLayer           *layer );

#define CYBER_UNDERLAY_SUPPORTED_OPTIONS     (DLOP_NONE)

/**********************/

static int
udlLayerDataSize()
{
     return sizeof(CyberUnderlayLayerData);
}
     
static DFBResult
udlInitLayer( GraphicsDevice             *device,
              DisplayLayer               *layer,
              DisplayLayerInfo           *layer_info,
              DFBDisplayLayerConfig      *default_config,
              DFBColorAdjustment         *default_adj,
              void                       *driver_data,
              void                       *layer_data )
{
     CyberUnderlayLayerData *cudl = (CyberUnderlayLayerData*) layer_data;
     
     /* set capabilities and type */
     layer_info->desc.caps = DLCAPS_SURFACE | DLCAPS_ALPHACHANNEL |
                             DLCAPS_OPACITY | DLCAPS_SRC_COLORKEY |
                             DLCAPS_SCREEN_LOCATION;
     layer_info->desc.type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE |
                             DLTF_BACKGROUND;

     /* set name */
     snprintf( layer_info->name,
               DFB_DISPLAY_LAYER_INFO_NAME_LENGTH, "CyberPro Underlay" );

     /* fill out the default configuration */
     default_config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                                   DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                                   DLCONF_OPTIONS;
     default_config->width       = 768;
     default_config->height      = 576;
     default_config->pixelformat = DSPF_RGB16;
     default_config->buffermode  = DLBM_FRONTONLY;
     default_config->options     = DLOP_NONE;
     
     /* initialize destination rectangle */
     dfb_primary_layer_rectangle( 0.0f, 0.0f, 1.0f, 1.0f, &cudl->dest );
     
     /* initialize registers */
     cyber_init_overlay();


     /* workaround */
     cyber_change_overlay_fifo();
     cyber_cleanup_overlay();
     cyber_init_overlay();
     
     return DFB_OK;
}


static DFBResult
udlEnable( DisplayLayer *layer,
           void         *driver_data,
           void         *layer_data )
{
     CyberDriverData        *cdrv = (CyberDriverData*) driver_data;
     CyberUnderlayLayerData *cudl = (CyberUnderlayLayerData*) layer_data;

     /* remember */
     cudl->enabled = 1;
     
     /* set up layer */
     udl_set_all( cdrv, cudl, layer );

     return DFB_OK;
}

static DFBResult
udlDisable( DisplayLayer *layer,
            void         *driver_data,
            void         *layer_data )
{
     CyberUnderlayLayerData *cudl = (CyberUnderlayLayerData*) layer_data;
     
     /* remember */
     cudl->enabled = 0;
     
     /* disable and clean up */
     cyber_enable_overlay(0);
     cyber_cleanup_alpha();
     cyber_cleanup_overlay();

     return DFB_OK;
}

static DFBResult
udlTestConfiguration( DisplayLayer               *layer,
                      void                       *driver_data,
                      void                       *layer_data,
                      DFBDisplayLayerConfig      *config,
                      DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags fail = 0;

     /* check for unsupported options */
     if (config->options & ~CYBER_UNDERLAY_SUPPORTED_OPTIONS)
          fail |= DLCONF_OPTIONS;

     /* check pixel format */
     switch (config->pixelformat) {
          case DSPF_RGB332:
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_YUY2:
               break;

          default:
               fail |= DLCONF_PIXELFORMAT;
     }

     /* check width */
     if (config->width > 1024 || config->width < 4)
          fail |= DLCONF_WIDTH;

     /* check height */
     if (config->height > 1024 || config->height < 1)
          fail |= DLCONF_HEIGHT;

     /* write back failing fields */
     if (failed)
          *failed = fail;

     /* return failure if any field failed */
     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
udlSetConfiguration( DisplayLayer          *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     DFBDisplayLayerConfig *config )
{
     CyberDriverData        *cdrv = (CyberDriverData*) driver_data;
     CyberUnderlayLayerData *cudl = (CyberUnderlayLayerData*) layer_data;

     /* remember configuration */
     cudl->config = *config;
     
     /* set up layer */
     if (cudl->enabled)
          udl_set_all( cdrv, cudl, layer );

     return DFB_OK;
}

static DFBResult
udlSetScreenLocation( DisplayLayer *layer,
                      void         *driver_data,
                      void         *layer_data,
                      float         x,
                      float         y,
                      float         width,
                      float         height )
{
     CyberDriverData        *cdrv = (CyberDriverData*) driver_data;
     CyberUnderlayLayerData *cudl = (CyberUnderlayLayerData*) layer_data;

     /* get new destination rectangle */
     dfb_primary_layer_rectangle( x, y, width, height, &cudl->dest );

     /* set up location only */
     if (cudl->enabled)
          udl_set_location( cdrv, cudl, layer );
     
     return DFB_OK;
}

static DFBResult
udlFlipBuffers( DisplayLayer        *layer,
                void                *driver_data,
                void                *layer_data,
                DFBSurfaceFlipFlags  flags)
{
     return DFB_UNIMPLEMENTED;
}


DisplayLayerFuncs cyberUnderlayFuncs = {
     LayerDataSize:      udlLayerDataSize,
     InitLayer:          udlInitLayer,
     Enable:             udlEnable,
     Disable:            udlDisable,
     TestConfiguration:  udlTestConfiguration,
     SetConfiguration:   udlSetConfiguration,
     SetScreenLocation:  udlSetScreenLocation,
     FlipBuffers:        udlFlipBuffers,
};


/* internal */

static void udl_set_all( CyberDriverData        *cdrv,
                         CyberUnderlayLayerData *cudl,
                         DisplayLayer           *layer )
{
     CoreSurface   *surface = dfb_layer_surface( layer );
     SurfaceBuffer *front   = surface->front_buffer;

     /* set the pixel format */
     switch (surface->format) {
          case DSPF_RGB332:
               cyber_set_overlay_format (OVERLAY_RGB8);
               break;

          case DSPF_RGB15:
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
               BUG("unexpected pixelformat");
               break;
     }

     cyber_set_overlay_mode( OVERLAY_WINDOWKEY );
     
     /* set address */
     cyber_set_overlay_srcaddr( front->video.offset, 0, 0,
                                surface->width, front->video.pitch );

     /* set location and scaling */
     udl_set_location( cdrv, cudl, layer );

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
                              DisplayLayer           *layer )
{
     CoreSurface *surface = dfb_layer_surface( layer );

     /* set location */
     cyber_set_overlay_window( cudl->dest.x, cudl->dest.y,
                               cudl->dest.x + cudl->dest.w - 1,
                               cudl->dest.y + cudl->dest.h - 1 );

     /* set scaling */
     cyber_set_overlay_scale( surface->height == 576 ? /* HACK: support interlaced video */
                              OVERLAY_BOBMODE : OVERLAY_WEAVEMODE,
                              surface->width, cudl->dest.w,
                              surface->height, cudl->dest.h );
}

