/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
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
#include <core/fbdev.h>
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/windows.h>

#include <misc/mem.h>

#include "neomagic.h"

static void
neo_overlay_set_regs ( NeoDriverData *ndrv,
                       NeoDeviceData *ndev );

static void
neo_overlay_calc_regs( NeoDriverData *ndrv,
                       NeoDeviceData *ndev,
                       DisplayLayer  *layer );

/* FIXME: no driver globals */
static NeoDriverData *ndrv = NULL;
static NeoDeviceData *ndev = NULL;

#define NEO_OVERLAY_SUPPORTED_OPTIONS   (0)


/**********************/

static DFBResult
neoOverlayEnable( DisplayLayer *layer )
{
     if (!layer->shared->surface) {
          DFBResult ret;

          /* FIXME HARDCODER! */
          ret = dfb_surface_create( layer->shared->width, layer->shared->height,
                                    DSPF_YUY2, CSP_VIDEOONLY, DSCAPS_VIDEOONLY,
                                    &layer->shared->surface );
          if (ret)
               return ret;
     }

     if (!layer->shared->windowstack)
          layer->shared->windowstack = dfb_windowstack_new( layer );

     layer->shared->enabled = 1;

     neo_overlay_calc_regs( ndrv, ndev, layer );
     neo_overlay_set_regs( ndrv, ndev );

     return DFB_OK;
}

static DFBResult
neoOverlayDisable( DisplayLayer *layer )
{
     /* FIXME: The surface should be destroyed, and the window stack? */
     layer->shared->enabled = 0;

     neo_unlock();
     OUTGR(0xb0, 0x00);
     neo_lock();

     return DFB_OK;
}

static DFBResult
neoOverlayTestConfiguration( DisplayLayer               *layer,
                             DFBDisplayLayerConfig      *config,
                             DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags fail = 0;

     if (config->flags & DLCONF_OPTIONS &&
         config->options & ~NEO_OVERLAY_SUPPORTED_OPTIONS)
          fail |= DLCONF_OPTIONS;

     if (config->flags & DLCONF_PIXELFORMAT) {
          switch (config->pixelformat) {
               case DSPF_YUY2:
                    break;

               default:
                    fail |= DLCONF_PIXELFORMAT;
          }
     }

     if (config->flags & DLCONF_WIDTH)
          if (config->width > 1024 || config->width < 160)
               fail |= DLCONF_WIDTH;

     if (config->flags & DLCONF_HEIGHT)
          if (config->height > 1024 || config->height < 1)
               fail |= DLCONF_HEIGHT;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
neoOverlaySetConfiguration( DisplayLayer          *layer,
                            DFBDisplayLayerConfig *config )
{
     neo_overlay_calc_regs( ndrv, ndev, layer );
     neo_overlay_set_regs( ndrv, ndev );

     return DFB_OK;
}

static DFBResult
neoOverlaySetOpacity( DisplayLayer *layer,
                      __u8          opacity )
{
     switch (opacity) {
          case 0:
               neo_unlock();
               OUTGR(0xb0, 0x00);
               neo_lock();
               break;
          case 0xFF:
               neo_unlock();
               OUTGR(0xb0, ndev->OVERLAY.CONTROL);
               neo_lock();
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
neoOverlaySetScreenLocation( DisplayLayer *layer,
                             float         x,
                             float         y,
                             float         width,
                             float         height )
{
     /* TODO: add workaround */
     if (x + width > 1.0f  ||  y + height > 1.0f)
          return DFB_UNIMPLEMENTED;

     layer->shared->screen.x = x;
     layer->shared->screen.y = y;
     layer->shared->screen.w = width;
     layer->shared->screen.h = height;

     neo_overlay_calc_regs( ndrv, ndev, layer );
     neo_overlay_set_regs( ndrv, ndev );

     return DFB_OK;
}

static DFBResult
neoOverlaySetSrcColorKey( DisplayLayer *layer,
                          __u32         key )
{
     return DFB_UNSUPPORTED;
}

static DFBResult
neoOverlaySetDstColorKey( DisplayLayer *layer,
                          __u32         key )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
neoOverlayFlipBuffers( DisplayLayer *layer )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
neoOverlaySetColorAdjustment( DisplayLayer       *layer,
                              DFBColorAdjustment *adj )
{
     return DFB_UNIMPLEMENTED;
}

static void
neo_overlay_deinit( DisplayLayer *layer )
{
     neo_unlock();
     OUTGR(0xb0, 0x00);
     neo_lock();
}

/* exported symbols */

DFBResult
neo_init_overlay( void *driver_data,
                  void *device_data )
{
     DisplayLayer *layer;

     /* FIXME: no driver globals */
     ndrv = (NeoDriverData*) driver_data;
     ndev = (NeoDeviceData*) device_data;

     /* create a layer */
     layer = (DisplayLayer*)DFBCALLOC( 1, sizeof(DisplayLayer) );

     layer->shared = (DisplayLayerShared*) shcalloc( 1, sizeof(DisplayLayerShared) );

     layer->shared->caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE;

     snprintf( layer->shared->description,
               DFB_DISPLAY_LAYER_INFO_NAME_LENGTH, "NeoMagic Overlay" );

     layer->shared->enabled    = 0;

     layer->shared->width      = 640;
     layer->shared->height     = 480;
     layer->shared->buffermode = DLBM_FRONTONLY;
     layer->shared->options    = 0;

     layer->shared->screen.x   = 0.0f;
     layer->shared->screen.y   = 0.0f;
     layer->shared->screen.w   = 1.0f;
     layer->shared->screen.h   = 1.0f;

     layer->shared->opacity    = 0xFF;

     layer->shared->bg.mode    = DLBM_DONTCARE;

     layer->Enable             = neoOverlayEnable;
     layer->Disable            = neoOverlayDisable;
     layer->TestConfiguration  = neoOverlayTestConfiguration;
     layer->SetConfiguration   = neoOverlaySetConfiguration;
     layer->SetOpacity         = neoOverlaySetOpacity;
     layer->SetScreenLocation  = neoOverlaySetScreenLocation;
     layer->SetSrcColorKey     = neoOverlaySetSrcColorKey;
     layer->SetDstColorKey     = neoOverlaySetDstColorKey;
     layer->FlipBuffers        = neoOverlayFlipBuffers;
     layer->SetColorAdjustment = neoOverlaySetColorAdjustment;

     layer->deinit             = neo_overlay_deinit;

     /* gets filled by layers_add: layer->shared->id */
     dfb_layers_add( layer );

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



/* internal */

static void
neo_overlay_set_regs( NeoDriverData *ndrv,
                      NeoDeviceData *ndev )
{
     iopl (3);

     neo_unlock();

     OUTGR(0xb1, ((ndev->OVERLAY.X2 >> 4) & 0xf0) | (ndev->OVERLAY.X1 >> 8));
     OUTGR(0xb2, ndev->OVERLAY.X1);
     OUTGR(0xb3, ndev->OVERLAY.X2);
     OUTGR(0xb4, ((ndev->OVERLAY.Y2 >> 4) & 0xf0) | (ndev->OVERLAY.Y1 >> 8));
     OUTGR(0xb5, ndev->OVERLAY.Y1);
     OUTGR(0xb6, ndev->OVERLAY.Y2);
     OUTGR(0xb7, ndev->OVERLAY.OFFSET >> 16);
     OUTGR(0xb8, ndev->OVERLAY.OFFSET >>  8);
     OUTGR(0xb9, ndev->OVERLAY.OFFSET);
     OUTGR(0xba, ndev->OVERLAY.PITCH >> 8);
     OUTGR(0xbb, ndev->OVERLAY.PITCH);
     OUTGR(0xbc, 0x2e);  /* Neo2160: 0x4f */
     OUTGR(0xbd, 0x02);
     OUTGR(0xbe, 0x00);
     OUTGR(0xbf, 0x02);

     OUTGR(0xc0, ndev->OVERLAY.HSCALE >> 8);
     OUTGR(0xc1, ndev->OVERLAY.HSCALE);
     OUTGR(0xc2, ndev->OVERLAY.VSCALE >> 8);
     OUTGR(0xc3, ndev->OVERLAY.VSCALE);

     OUTGR(0xb0, ndev->OVERLAY.CONTROL);

     neo_lock();
}

static void
neo_overlay_calc_regs( NeoDriverData *ndrv,
                       NeoDeviceData *ndev,
                       DisplayLayer  *layer )
{
     DFBRectangle        dst;
     DisplayLayerShared *shared  = layer->shared;
     CoreSurface        *surface = shared->surface;
     SurfaceBuffer      *front   = surface->front_buffer;

     /* calculate destination rectangle */
     dst.x = (int)(shared->screen.x * (float)Sfbdev->current_mode->xres + 0.5f);
     dst.y = (int)(shared->screen.y * (float)Sfbdev->current_mode->yres + 0.5f);
     dst.w = (int)(shared->screen.w * (float)Sfbdev->current_mode->xres + 0.5f);
     dst.h = (int)(shared->screen.h * (float)Sfbdev->current_mode->yres + 0.5f);

     /* fill register struct */
     ndev->OVERLAY.CONTROL = 0x01;

     ndev->OVERLAY.X1      = dst.x;
     ndev->OVERLAY.X2      = dst.x + dst.w - 1;

     ndev->OVERLAY.Y1      = dst.y;
     ndev->OVERLAY.Y2      = dst.y + dst.h - 1;

     ndev->OVERLAY.OFFSET  = front->video.offset;
     ndev->OVERLAY.PITCH   = front->video.pitch;

     ndev->OVERLAY.HSCALE  = (surface->width  << 12) / (dst.w + 1);
     ndev->OVERLAY.VSCALE  = (surface->height << 12) / (dst.h + 1);
}

