/*
   (c) Copyright 2000  convergence integrated media GmbH.
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


#include "regs.h"
#include "mmio.h"
#include "matrox.h"

typedef struct {
     MatroxDriverData *mdrv;
     MatroxDeviceData *mdev;
     DisplayLayer     *layer;
} BESListenerContext;

static void bes_set_regs( MatroxDriverData *mdrv, MatroxDeviceData *mdev );
static void bes_calc_regs( MatroxDriverData *mdrv, MatroxDeviceData *mdev,
                           DisplayLayer *layer );

/* FIXME: no driver globals */
static MatroxDriverData *mdrv = NULL;
static MatroxDeviceData *mdev = NULL;

#define BES_SUPPORTED_OPTIONS   (DLOP_INTERLACED_VIDEO | DLOP_DST_COLORKEY)


static ReactionResult besSurfaceListener (const void *msg_data, void *ctx)
{
     BESListenerContext      *context      = (BESListenerContext*) ctx;
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*)msg_data;

     if (notification->flags & (CSNF_SET_EVEN | CSNF_SET_ODD)) {
          context->mdev->regs.besCTL_field =
               (notification->flags & CSNF_SET_ODD) ? 0x2000000 : 0;

          mga_out32( context->mdrv->mmio_base,
                     context->mdev->regs.besCTL |
                     context->mdev->regs.besCTL_field, BESCTL );
     }

     if (notification->flags & CSNF_DESTROY) {
          DFBFREE( context );
          return RS_REMOVE;
     }

     return RS_OK;
}


/**********************/

static DFBResult
besEnable( DisplayLayer *layer )
{
     if (!layer->shared->surface) {
          DFBResult               ret;
          DFBSurfaceCapabilities  caps = DSCAPS_VIDEOONLY;
          BESListenerContext     *context;

          if (layer->shared->options & DLOP_INTERLACED_VIDEO)
               caps |= DSCAPS_INTERLACED;

          /* FIXME HARDCODER! */
          ret = dfb_surface_create( layer->shared->width, layer->shared->height, DSPF_RGB16,
                                    CSP_VIDEOONLY, caps, &layer->shared->surface );
          if (ret)
               return ret;

          context = DFBCALLOC( 1, sizeof(BESListenerContext) );

          context->mdrv  = mdrv;
          context->mdev  = mdev;
          context->layer = layer;

          reactor_attach( layer->shared->surface->reactor,
                          besSurfaceListener, context );
     }

     if (!layer->shared->windowstack)
          layer->shared->windowstack = dfb_windowstack_new( layer );

     layer->shared->enabled = 1;

     bes_calc_regs( mdrv, mdev, layer );
     bes_set_regs( mdrv, mdev );

     return DFB_OK;
}

static DFBResult
besDisable( DisplayLayer *layer )
{
     /* FIXME: The surface should be destroyed, and the window stack? */
     layer->shared->enabled = 0;

     mga_out32( mdrv->mmio_base, 0, BESCTL );

     return DFB_OK;
}

static DFBResult
besTestConfiguration( DisplayLayer               *layer,
                      DFBDisplayLayerConfig      *config,
                      DFBDisplayLayerConfigFlags *failed )
{
     int max_width = 1024;
     DFBDisplayLayerConfigFlags fail = 0;

     if (config->flags & DLCONF_OPTIONS &&
         config->options & ~BES_SUPPORTED_OPTIONS)
          fail |= DLCONF_OPTIONS;

     if (config->flags & DLCONF_PIXELFORMAT)
          switch (config->pixelformat) {
               case DSPF_YUY2:
                    break;

               case DSPF_RGB32:
                    max_width = 512;
               case DSPF_RGB15:
               case DSPF_RGB16:
               case DSPF_UYVY:
               case DSPF_I420:
               case DSPF_YV12:
                    /* these formats are not supported by G200 */
                    if (mdev->accelerator != FB_ACCEL_MATROX_MGAG200)
                         break;
               default:
                    fail |= DLCONF_PIXELFORMAT;
          }
     else
          if (layer->shared->surface->format == DSPF_RGB32)
               max_width = 512;

     if (config->flags & DLCONF_WIDTH) {
          if (config->width > max_width || config->width < 1)
               fail |= DLCONF_WIDTH;
     }
     else {
          if (layer->shared->width > max_width)
               fail |= DLCONF_WIDTH;
     }

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
besSetConfiguration( DisplayLayer          *layer,
                     DFBDisplayLayerConfig *config )
{
     bes_calc_regs( mdrv, mdev, layer );
     bes_set_regs( mdrv, mdev );

     return DFB_OK;
}

static DFBResult
besSetOpacity( DisplayLayer *layer,
               __u8          opacity )
{
     switch (opacity) {
          case 0:
               mga_out32( mdrv->mmio_base, 0, BESCTL );
               break;
          case 0xFF:
               mga_out32( mdrv->mmio_base, 1, BESCTL );
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
besSetScreenLocation( DisplayLayer *layer,
                      float         x,
                      float         y,
                      float         width,
                      float         height )
{
     layer->shared->screen.x = x;
     layer->shared->screen.y = y;
     layer->shared->screen.w = width;
     layer->shared->screen.h = height;

     bes_calc_regs( mdrv, mdev, layer );
     bes_set_regs( mdrv, mdev );

     return DFB_OK;
}

static DFBResult
besSetSrcColorKey( DisplayLayer *layer,
                   __u32         key )
{
     return DFB_UNSUPPORTED;
}

static DFBResult
besSetDstColorKey( DisplayLayer *layer,
                   __u8          r,
                   __u8          g,
                   __u8          b )
{
     volatile __u8 *mmio = mdrv->mmio_base;
     
     mga_out_dac( mmio, XCOLKEY0RED,   r );
     mga_out_dac( mmio, XCOLKEY0GREEN, g );
     mga_out_dac( mmio, XCOLKEY0BLUE,  b );

     return DFB_OK;
}

static DFBResult
besFlipBuffers( DisplayLayer *layer )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
besSetColorAdjustment( DisplayLayer       *layer,
                       DFBColorAdjustment *adj )
{
     if (adj->flags & ~(DCAF_BRIGHTNESS | DCAF_CONTRAST))
          return DFB_UNSUPPORTED;

     if (adj->flags & DCAF_BRIGHTNESS)
          layer->shared->adjustment.brightness = adj->brightness;

     if (adj->flags & DCAF_CONTRAST)
          layer->shared->adjustment.contrast = adj->contrast;

     bes_calc_regs( mdrv, mdev, layer );
     bes_set_regs( mdrv, mdev );

     return DFB_OK;
}

static void
matrox_bes_deinit( DisplayLayer *layer )
{
     mga_out32( mdrv->mmio_base, 0, BESCTL );
}

/* exported symbols */

void matrox_init_bes( void *drv, void *dev )
{
     DisplayLayer     *layer;
     volatile __u8    *mmio;

     /* FIXME: no driver globals */
     mdrv = (MatroxDriverData*) drv;
     mdev = (MatroxDeviceData*) dev;

     mmio = mdrv->mmio_base;

     if (mdev->old_matrox)
          return;

     layer = (DisplayLayer*)DFBCALLOC( 1, sizeof(DisplayLayer) );

     layer->shared = (DisplayLayerShared*) shcalloc( 1, sizeof(DisplayLayerShared) );

     layer->shared->caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE | DLCAPS_BRIGHTNESS |
                   DLCAPS_CONTRAST | DLCAPS_INTERLACED_VIDEO | DLCAPS_DST_COLORKEY;

     snprintf( layer->shared->description,
               DFB_DISPLAY_LAYER_INFO_NAME_LENGTH, "Matrox Backend Scaler" );

     layer->shared->enabled = 0;

     layer->shared->width = 640;
     layer->shared->height = 480;
     layer->shared->buffermode = DLBM_FRONTONLY;
     layer->shared->options = 0;

     layer->shared->screen.x = 0.0f;
     layer->shared->screen.y = 0.0f;
     layer->shared->screen.w = 1.0f;
     layer->shared->screen.h = 1.0f;

     layer->shared->opacity = 0xFF;

     layer->shared->bg.mode  = DLBM_DONTCARE;

     layer->shared->adjustment.flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST;
     layer->shared->adjustment.brightness = 0x8000;
     layer->shared->adjustment.contrast   = 0x8000;

     layer->Enable = besEnable;
     layer->Disable = besDisable;
     layer->TestConfiguration = besTestConfiguration;
     layer->SetConfiguration = besSetConfiguration;
     layer->SetOpacity = besSetOpacity;
     layer->SetScreenLocation = besSetScreenLocation;
     layer->SetSrcColorKey = besSetSrcColorKey;
     layer->SetDstColorKey = besSetDstColorKey;
     layer->FlipBuffers = besFlipBuffers;
     layer->SetColorAdjustment = besSetColorAdjustment;

     layer->deinit = matrox_bes_deinit;

     mga_out_dac( mmio, XKEYOPMODE, 0x00 ); /* keying off */

     mga_out_dac( mmio, XCOLMSK0RED,   0xFF ); /* full mask */
     mga_out_dac( mmio, XCOLMSK0GREEN, 0xFF );
     mga_out_dac( mmio, XCOLMSK0BLUE,  0xFF );

     mga_out_dac( mmio, XCOLKEY0RED,   0x00 ); /* default to black */
     mga_out_dac( mmio, XCOLKEY0GREEN, 0x00 );
     mga_out_dac( mmio, XCOLKEY0BLUE,  0x00 );

     mga_out32( mmio, 0x80, BESLUMACTL );

     /* gets filled by layers_add: layer->shared->id */
     dfb_layers_add( layer );
}



/* internal */

static void bes_set_regs( MatroxDriverData *mdrv, MatroxDeviceData *mdev )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     mga_out32( mmio, mdev->regs.besGLOBCTL, BESGLOBCTL);

     mga_out32( mmio, mdev->regs.besA1ORG, BESA1ORG );
     mga_out32( mmio, mdev->regs.besA2ORG, BESA2ORG );
     mga_out32( mmio, mdev->regs.besA1CORG, BESA1CORG );
     mga_out32( mmio, mdev->regs.besA2CORG, BESA2CORG );

     if (mdev->accelerator != FB_ACCEL_MATROX_MGAG200) {
          mga_out32( mmio, mdev->regs.besA1C3ORG, BESA1C3ORG );
          mga_out32( mmio, mdev->regs.besA2C3ORG, BESA2C3ORG );
     }

     mga_out32( mmio, mdev->regs.besCTL | mdev->regs.besCTL_field, BESCTL );

     mga_out32( mmio, mdev->regs.besHCOORD, BESHCOORD );
     mga_out32( mmio, mdev->regs.besVCOORD, BESVCOORD );

     mga_out32( mmio, mdev->regs.besHSRCST, BESHSRCST );
     mga_out32( mmio, mdev->regs.besHSRCEND, BESHSRCEND );
     mga_out32( mmio, mdev->regs.besHSRCLST, BESHSRCLST );

     mga_out32( mmio, mdev->regs.besLUMACTL, BESLUMACTL );
     mga_out32( mmio, mdev->regs.besPITCH, BESPITCH );

     mga_out32( mmio, mdev->regs.besV1WGHT, BESV1WGHT );
     mga_out32( mmio, mdev->regs.besV2WGHT, BESV2WGHT );

     mga_out32( mmio, mdev->regs.besV1SRCLST, BESV1SRCLST );
     mga_out32( mmio, mdev->regs.besV2SRCLST, BESV2SRCLST );

     mga_out32( mmio, mdev->regs.besVISCAL, BESVISCAL );
     mga_out32( mmio, mdev->regs.besHISCAL, BESHISCAL );

     mga_out_dac( mmio, XKEYOPMODE, mdev->regs.xKEYOPMODE );
}

static void bes_calc_regs( MatroxDriverData *mdrv, MatroxDeviceData *mdev,
                           DisplayLayer *layer )
{
     int tmp, hzoom, intrep;

     DFBRegion           dstBox;
     int                 drw_w;
     int                 drw_h;
     int                 field_height;
     DisplayLayerShared *shared  = layer->shared;
     CoreSurface        *surface = shared->surface;


     drw_w = (int)(shared->screen.w * (float)Sfbdev->current_mode->xres + 0.5f);
     drw_h = (int)(shared->screen.h * (float)Sfbdev->current_mode->yres + 0.5f);

     dstBox.x1 = (int)(shared->screen.x * (float)Sfbdev->current_mode->xres + 0.5f);
     dstBox.y1 = (int)(shared->screen.y * (float)Sfbdev->current_mode->yres + 0.5f);
     dstBox.x2 = (int)((shared->screen.x + shared->screen.w) * (float)Sfbdev->current_mode->xres + 0.5f);
     dstBox.y2 = (int)((shared->screen.y + shared->screen.h) * (float)Sfbdev->current_mode->yres + 0.5f);

     hzoom = (1000000/Sfbdev->current_var.pixclock >= 135) ? 1 : 0;

     mdev->regs.besGLOBCTL = 0;
     mdev->regs.besCTL     = BESEN;

     switch (surface->format) {
          case DSPF_I420:
          case DSPF_YV12:
               mdev->regs.besGLOBCTL |= BESPROCAMP | BES3PLANE;
               mdev->regs.besCTL     |= BESHFEN | BESVFEN | BESCUPS | BES420PL;
               break;

          case DSPF_UYVY:
               mdev->regs.besGLOBCTL |= BESUYVYFMT;
               /* fall through */

          case DSPF_YUY2:
               mdev->regs.besGLOBCTL |= BESPROCAMP;
               mdev->regs.besCTL     |= BESHFEN | BESVFEN | BESCUPS;
               break;

          case DSPF_RGB15:
               mdev->regs.besGLOBCTL |= BESRGB15;
               break;

          case DSPF_RGB16:
               mdev->regs.besGLOBCTL |= BESRGB16;
               break;

          case DSPF_RGB32:
               mdev->regs.besGLOBCTL |= BESRGB32;

               drw_w = shared->width;
               dstBox.x2 = dstBox.x1 + shared->width;
               break;

          default:
               BUG( "unexpected pixelformat" );
               return;
     }

     mdev->regs.besGLOBCTL |= 3*hzoom | (Sfbdev->current_mode->yres & 0xFFF) << 16;
     mdev->regs.besA1ORG    = surface->front_buffer->video.offset;
     mdev->regs.besA2ORG    = surface->front_buffer->video.offset +
                              surface->front_buffer->video.pitch;

     switch (surface->format) {
          case DSPF_I420:
               mdev->regs.besA1CORG  = mdev->regs.besA1ORG + surface->height *
                                       surface->front_buffer->video.pitch;
               mdev->regs.besA1C3ORG = mdev->regs.besA1CORG + surface->height/2 *
                                       surface->front_buffer->video.pitch/2;
               mdev->regs.besA2CORG  = mdev->regs.besA2ORG + surface->height *
                                       surface->front_buffer->video.pitch;
               mdev->regs.besA2C3ORG = mdev->regs.besA2CORG + surface->height/2 *
                                       surface->front_buffer->video.pitch/2;
               break;

          case DSPF_YV12:
               mdev->regs.besA1C3ORG = mdev->regs.besA1ORG + surface->height *
                                       surface->front_buffer->video.pitch;
               mdev->regs.besA1CORG  = mdev->regs.besA1C3ORG + surface->height/2 *
                                       surface->front_buffer->video.pitch/2;
               mdev->regs.besA2C3ORG = mdev->regs.besA2ORG + surface->height *
                                       surface->front_buffer->video.pitch;
               mdev->regs.besA2CORG  = mdev->regs.besA2C3ORG + surface->height/2 *
                                       surface->front_buffer->video.pitch/2;
               break;

          default:
               ;
     }

     mdev->regs.besHCOORD   = (dstBox.x1 << 16) | (dstBox.x2 - 1);
     mdev->regs.besVCOORD   = (dstBox.y1 << 16) | (dstBox.y2 - 1);

     mdev->regs.besHSRCST   = 0;
     mdev->regs.besHSRCEND  = (shared->width - 1) << 16;
     mdev->regs.besHSRCLST  = (shared->width - 1) << 16;

     mdev->regs.besLUMACTL  = (shared->adjustment.contrast >> 8) |
                              ((__u8)(((int)shared->adjustment.brightness >> 8)
                                       - 128)) << 16;
     
     mdev->regs.besV1WGHT   = 0;
     mdev->regs.besV2WGHT   = 0x18000;

     mdev->regs.besV1SRCLST = shared->height - 1;
     mdev->regs.besV2SRCLST = shared->height - 2;

     mdev->regs.besPITCH    = surface->front_buffer->video.pitch /
                              DFB_BYTES_PER_PIXEL(surface->format);

     field_height           = shared->height;

     if (shared->options & DLOP_INTERLACED_VIDEO) {
          field_height        /= 2;
          mdev->regs.besPITCH *= 2;
     }
     else
          mdev->regs.besCTL_field = 0;

     if (shared->surface->format == DSPF_RGB32)
          mdev->regs.besHISCAL = 0x20000;
     else {
          intrep = ((drw_w == shared->width) || (drw_w < 2)) ? 0 : 1;
          tmp = (((shared->width - intrep) << 16) / (drw_w - intrep)) << hzoom;
          if (tmp >= (32 << 16))
               tmp = (32 << 16) - 1;
          mdev->regs.besHISCAL = tmp & 0x001ffffc;
     }
     
     intrep = ((drw_h == field_height) || (drw_h < 2)) ? 0 : 1;
     tmp = ((field_height - intrep) << 16) / (drw_h - intrep);
     if(tmp >= (32 << 16))
          tmp = (32 << 16) - 1;
     mdev->regs.besVISCAL = tmp & 0x001ffffc;

     /* enable color keying? */
     mdev->regs.xKEYOPMODE = (shared->options & DLOP_DST_COLORKEY) ? 1 : 0;
}

