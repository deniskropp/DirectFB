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
#include <sys/io.h>

#include <core/coredefs.h>
#include <core/layers.h>
#include <core/surface.h>

#include "neomagic.h"

typedef struct {
     CoreLayerRegionConfig config;

     /* overlay registers */
     struct {
          u32 OFFSET;
          u16 PITCH;
          u16 X1;
          u16 X2;
          u16 Y1;
          u16 Y2;
          u16 HSCALE;
          u16 VSCALE;
          u8  CONTROL;
     } regs;
} NeoOverlayLayerData;

static void ovl_set_regs ( NeoDriverData         *ndrv,
                           NeoOverlayLayerData   *novl );
static void ovl_calc_regs( NeoDriverData         *ndrv,
                           NeoOverlayLayerData   *novl,
                           CoreLayerRegionConfig *config,
                           CoreSurface           *surface,
                           CoreSurfaceBufferLock *lock );

#define NEO_OVERLAY_SUPPORTED_OPTIONS   (DLOP_NONE)

/**********************/

static int
ovlLayerDataSize()
{
     return sizeof(NeoOverlayLayerData);
}

static DFBResult
ovlInitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *default_config,
              DFBColorAdjustment         *default_adj )
{
     /* set capabilities and type */
     description->caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE |
                         DLCAPS_BRIGHTNESS;
     description->type = DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "NeoMagic Overlay" );

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

     /* FIXME: use mmio */
     if (iopl(3) < 0) {
          D_PERROR( "NeoMagic/Overlay: Could not change I/O permission level!\n" );
          return DFB_UNSUPPORTED;
     }

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

     /* FIXME: use mmio */
     if (iopl(3) < 0) {
          D_PERROR( "NeoMagic/Overlay: Could not change I/O permission level!\n" );
          return;
     }

     /* write back to card */
     neo_unlock();
     OUTGR(0xb0, novl->regs.CONTROL);
     neo_lock();
}

static DFBResult
ovlTestRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     /* check for unsupported options */
     if (config->options & ~NEO_OVERLAY_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     /* check pixel format */
     switch (config->format) {
          case DSPF_YUY2:
               break;

          default:
               fail |= CLRCF_FORMAT;
     }

     /* check width */
     if (config->width > 1024 || config->width < 160)
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
ovlSetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags  updated,
              CoreSurface                *surface,
              CorePalette                *palette,
              CoreSurfaceBufferLock      *lock )
{
     NeoDriverData       *ndrv = (NeoDriverData*) driver_data;
     NeoOverlayLayerData *novl = (NeoOverlayLayerData*) layer_data;

     /* remember configuration */
     novl->config = *config;

     ovl_calc_regs( ndrv, novl, config, surface, lock );
     ovl_set_regs( ndrv, novl );

     /* enable overlay */
     ovlOnOff( ndrv, novl, config->opacity );

     return DFB_OK;
}

static DFBResult
ovlRemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
     NeoDriverData       *ndrv = (NeoDriverData*) driver_data;
     NeoOverlayLayerData *novl = (NeoOverlayLayerData*) layer_data;

     /* disable overlay */
     ovlOnOff( ndrv, novl, 0 );

     return DFB_OK;
}

static DFBResult
ovlFlipRegion(  CoreLayer             *layer,
                void                  *driver_data,
                void                  *layer_data,
                void                  *region_data,
                CoreSurface           *surface,
                DFBSurfaceFlipFlags    flags,
                CoreSurfaceBufferLock *lock )
{
     NeoDriverData       *ndrv = (NeoDriverData*) driver_data;
     NeoOverlayLayerData *novl = (NeoOverlayLayerData*) layer_data;
#if 0
     bool                 onsync  = (flags & DSFLIP_WAITFORSYNC);

     if (onsync)
          dfb_fbdev_wait_vsync();
#endif

     dfb_surface_flip( surface, false );

     ovl_calc_regs( ndrv, novl, &novl->config, surface, lock );
     ovl_set_regs( ndrv, novl );

     return DFB_OK;
}

static DFBResult
ovlSetColorAdjustment( CoreLayer          *layer,
                       void               *driver_data,
                       void               *layer_data,
                       DFBColorAdjustment *adj )
{
     /* FIXME: use mmio */
     if (iopl(3) < 0) {
          D_PERROR( "NeoMagic/Overlay: Could not change I/O permission level!\n" );
          return DFB_UNSUPPORTED;
     }

     neo_unlock();
     OUTGR(0xc4, (signed char)((adj->brightness >> 8) -128));
     neo_lock();

     return DFB_OK;
}


DisplayLayerFuncs neoOverlayFuncs = {
     LayerDataSize:      ovlLayerDataSize,
     InitLayer:          ovlInitLayer,
     SetRegion:          ovlSetRegion,
     RemoveRegion:       ovlRemoveRegion,
     TestRegion:         ovlTestRegion,
     FlipRegion:         ovlFlipRegion,
     SetColorAdjustment: ovlSetColorAdjustment
};


/* internal */

static void ovl_set_regs( NeoDriverData *ndrv, NeoOverlayLayerData *novl )
{
     /* FIXME: use mmio */
     if (iopl(3) < 0) {
          D_PERROR( "NeoMagic/Overlay: Could not change I/O permission level!\n" );
          return;
     }

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

static void ovl_calc_regs( NeoDriverData         *ndrv,
                           NeoOverlayLayerData   *novl,
                           CoreLayerRegionConfig *config,
                           CoreSurface           *surface,
                           CoreSurfaceBufferLock *lock )
{
     /* fill register struct */
     novl->regs.X1     = config->dest.x;
     novl->regs.X2     = config->dest.x + config->dest.w - 1;

     novl->regs.Y1     = config->dest.y;
     novl->regs.Y2     = config->dest.y + config->dest.h - 1;

     novl->regs.OFFSET = lock->offset;
     novl->regs.PITCH  = lock->pitch;

     novl->regs.HSCALE = (surface->config.size.w  << 12) / (config->dest.w + 1);
     novl->regs.VSCALE = (surface->config.size.h << 12) / (config->dest.h + 1);
}

