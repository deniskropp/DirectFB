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

#include "neomagic.h"

typedef struct {
     DFBRectangle          dest;
     DFBDisplayLayerConfig config;

     /* overlay registers */
     struct {
          __u32 OFFSET;
          __u16 PITCH;
          __u16 X1;
          __u16 X2;
          __u16 Y1;
          __u16 Y2;
          __u16 HSCALE;
          __u16 VSCALE;
          __u8  CONTROL;
     } regs;
} NeoOverlayLayerData;

static void ovl_set_regs( NeoDriverData *ndrv, NeoOverlayLayerData *novl );
static void ovl_calc_regs( NeoDriverData *ndrv, NeoOverlayLayerData *novl,
                           DisplayLayer *layer, DFBDisplayLayerConfig *config );

#define NEO_OVERLAY_SUPPORTED_OPTIONS   (DLOP_NONE)

/**********************/

static int
ovlLayerDataSize()
{
     return sizeof(NeoOverlayLayerData);
}
     
static DFBResult
ovlInitLayer( GraphicsDevice             *device,
              DisplayLayer               *layer,
              DisplayLayerInfo           *layer_info,
              DFBDisplayLayerConfig      *default_config,
              DFBColorAdjustment         *default_adj,
              void                       *driver_data,
              void                       *layer_data )
{
     NeoOverlayLayerData *novl = (NeoOverlayLayerData*) layer_data;
     
     /* set capabilities and type */
     layer_info->desc.caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE |
                             DLCAPS_BRIGHTNESS;
     layer_info->desc.type = DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( layer_info->name,
               DFB_DISPLAY_LAYER_INFO_NAME_LENGTH, "NeoMagic Overlay" );

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
     default_adj->flags      = DCAF_BRIGHTNESS;
     default_adj->brightness = 0x8000;
     
     
     /* initialize destination rectangle */
     dfb_primary_layer_rectangle( 0.0f, 0.0f, 1.0f, 1.0f, &novl->dest );
     
     
     /* FIXME: use mmio */
     iopl(3);

     neo_unlock();

     /* reset overlay */
     OUTGR(0xb0, 0x00);

     /* reset brightness */
     OUTGR(0xc4, 0x00);

     /* disable capture */
     OUTGR(0x0a, 0x21);
     OUTSR(0x08, 0xa0);
     OUTGR(0x0a, 0x01);

     neo_lock();
     
     return DFB_OK;
}


static void
ovlOnOff( NeoDriverData       *ndrv,
          NeoOverlayLayerData *novl,
          int                  on )
{
     /* set/clear enable bit */
     if (on)
          novl->regs.CONTROL = 0x01;
     else
          novl->regs.CONTROL = 0x00;

     /* write back to card */
     neo_unlock();
     OUTGR(0xb0, novl->regs.CONTROL);
     neo_lock();
}

static DFBResult
ovlEnable( DisplayLayer *layer,
           void         *driver_data,
           void         *layer_data )
{
     NeoDriverData       *ndrv = (NeoDriverData*) driver_data;
     NeoOverlayLayerData *novl = (NeoOverlayLayerData*) layer_data;
     
     /* enable overlay */
     ovlOnOff( ndrv, novl, 1 );

     return DFB_OK;
}

static DFBResult
ovlDisable( DisplayLayer *layer,
            void         *driver_data,
            void         *layer_data )
{
     NeoDriverData       *ndrv = (NeoDriverData*) driver_data;
     NeoOverlayLayerData *novl = (NeoOverlayLayerData*) layer_data;

     /* disable overlay */
     ovlOnOff( ndrv, novl, 0 );

     return DFB_OK;
}

static DFBResult
ovlTestConfiguration( DisplayLayer               *layer,
                      void                       *driver_data,
                      void                       *layer_data,
                      DFBDisplayLayerConfig      *config,
                      DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags fail = 0;

     /* check for unsupported options */
     if (config->options & ~NEO_OVERLAY_SUPPORTED_OPTIONS)
          fail |= DLCONF_OPTIONS;

     /* check pixel format */
     switch (config->pixelformat) {
          case DSPF_YUY2:
               break;

          default:
               fail |= DLCONF_PIXELFORMAT;
     }

     /* check width */
     if (config->width > 1024 || config->width < 160)
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
ovlSetConfiguration( DisplayLayer          *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     DFBDisplayLayerConfig *config )
{
     NeoDriverData       *ndrv = (NeoDriverData*) driver_data;
     NeoOverlayLayerData *novl = (NeoOverlayLayerData*) layer_data;

     /* remember configuration */
     novl->config = *config;
     
     ovl_calc_regs( ndrv, novl, layer, config );
     ovl_set_regs( ndrv, novl );

     return DFB_OK;
}

static DFBResult
ovlSetOpacity( DisplayLayer *layer,
               void         *driver_data,
               void         *layer_data,
               __u8          opacity )
{
     NeoDriverData       *ndrv = (NeoDriverData*) driver_data;
     NeoOverlayLayerData *novl = (NeoOverlayLayerData*) layer_data;
     
     switch (opacity) {
          case 0:
               ovlOnOff( ndrv, novl, 0 );
               break;
          case 0xFF:
               ovlOnOff( ndrv, novl, 1 );
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
ovlSetScreenLocation( DisplayLayer *layer,
                      void         *driver_data,
                      void         *layer_data,
                      float         x,
                      float         y,
                      float         width,
                      float         height )
{
     NeoDriverData       *ndrv = (NeoDriverData*) driver_data;
     NeoOverlayLayerData *novl = (NeoOverlayLayerData*) layer_data;
     
     /* get new destination rectangle */
     dfb_primary_layer_rectangle( x, y, width, height, &novl->dest );

     ovl_calc_regs( ndrv, novl, layer, &novl->config );
     ovl_set_regs( ndrv, novl );
     
     return DFB_OK;
}

static DFBResult
ovlSetDstColorKey( DisplayLayer *layer,
                   void         *driver_data,
                   void         *layer_data,
                   __u8          r,
                   __u8          g,
                   __u8          b )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
ovlFlipBuffers( DisplayLayer        *layer,
                void                *driver_data,
                void                *layer_data,
                DFBSurfaceFlipFlags  flags )
{
     NeoDriverData       *ndrv = (NeoDriverData*) driver_data;
     NeoOverlayLayerData *novl = (NeoOverlayLayerData*) layer_data;
     CoreSurface         *surface = dfb_layer_surface( layer );
#if 0
     bool                 onsync  = (flags & DSFLIP_WAITFORSYNC);
     
     if (onsync)
          dfb_fbdev_wait_vsync();
#endif
     
     dfb_surface_flip_buffers( surface );
     
     ovl_calc_regs( ndrv, novl, layer, &novl->config );
     ovl_set_regs( ndrv, novl );

     return DFB_OK;
}

static DFBResult
ovlSetColorAdjustment( DisplayLayer       *layer,
                       void               *driver_data,
                       void               *layer_data,
                       DFBColorAdjustment *adj )
{
     neo_unlock();
     OUTGR(0xc4, (signed char)((adj->brightness >> 8) -128));
     neo_lock();

     return DFB_OK;
}


DisplayLayerFuncs neoOverlayFuncs = {
     LayerDataSize:      ovlLayerDataSize,
     InitLayer:          ovlInitLayer,
     Enable:             ovlEnable,
     Disable:            ovlDisable,
     TestConfiguration:  ovlTestConfiguration,
     SetConfiguration:   ovlSetConfiguration,
     SetOpacity:         ovlSetOpacity,
     SetScreenLocation:  ovlSetScreenLocation,
     SetDstColorKey:     ovlSetDstColorKey,
     FlipBuffers:        ovlFlipBuffers,
     SetColorAdjustment: ovlSetColorAdjustment
};


/* internal */

static void ovl_set_regs( NeoDriverData *ndrv, NeoOverlayLayerData *novl )
{
     neo_unlock();

     OUTGR(0xb1, ((novl->regs.X2 >> 4) & 0xf0) | (novl->regs.X1 >> 8));
     OUTGR(0xb2, novl->regs.X1);
     OUTGR(0xb3, novl->regs.X2);
     OUTGR(0xb4, ((novl->regs.Y2 >> 4) & 0xf0) | (novl->regs.Y1 >> 8));
     OUTGR(0xb5, novl->regs.Y1);
     OUTGR(0xb6, novl->regs.Y2);
     OUTGR(0xb7, novl->regs.OFFSET >> 16);
     OUTGR(0xb8, novl->regs.OFFSET >>  8);
     OUTGR(0xb9, novl->regs.OFFSET);
     OUTGR(0xba, novl->regs.PITCH >> 8);
     OUTGR(0xbb, novl->regs.PITCH);
     OUTGR(0xbc, 0x2e);  /* Neo2160: 0x4f */
     OUTGR(0xbd, 0x02);
     OUTGR(0xbe, 0x00);
     OUTGR(0xbf, 0x02);

     OUTGR(0xc0, novl->regs.HSCALE >> 8);
     OUTGR(0xc1, novl->regs.HSCALE);
     OUTGR(0xc2, novl->regs.VSCALE >> 8);
     OUTGR(0xc3, novl->regs.VSCALE);

     neo_lock();
}

static void ovl_calc_regs( NeoDriverData *ndrv, NeoOverlayLayerData *novl,
                           DisplayLayer *layer, DFBDisplayLayerConfig *config )
{
     CoreSurface   *surface      = dfb_layer_surface( layer );
     SurfaceBuffer *front_buffer = surface->front_buffer;

     /* fill register struct */
     novl->regs.X1     = novl->dest.x;
     novl->regs.X2     = novl->dest.x + novl->dest.w - 1;

     novl->regs.Y1     = novl->dest.y;
     novl->regs.Y2     = novl->dest.y + novl->dest.h - 1;

     novl->regs.OFFSET = front_buffer->video.offset;
     novl->regs.PITCH  = front_buffer->video.pitch;

     novl->regs.HSCALE = (surface->width  << 12) / (novl->dest.w + 1);
     novl->regs.VSCALE = (surface->height << 12) / (novl->dest.h + 1);
}

