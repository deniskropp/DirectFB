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

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/gfxcard.h>
#include <core/fbdev.h>
#include <core/layers.h>
#include <core/surfaces.c>
#include <core/windows.c>

#include "regs.h"
#include "mmio.h"
#include "matrox.h"

static struct {
     __u32 besGLOBCTL;
     __u32 besA1ORG;
     __u32 besA2ORG;
     __u32 besCTL;

     __u32 besCTL_field;

     __u32 besHCOORD;
     __u32 besVCOORD;

     __u32 besHSRCST;
     __u32 besHSRCEND;
     __u32 besHSRCLST;

     __u32 besLUMACTL;
     __u32 besPITCH;

     __u32 besV1WGHT;
     __u32 besV2WGHT;

     __u32 besV1SRCLST;
     __u32 besV2SRCLST;

     __u32 besVISCAL;
     __u32 besHISCAL;
} bes_regs;

static void bes_set_regs();
static void bes_calc_regs( DisplayLayer *layer );


#define BES_SUPPORTED_OPTIONS   (DLOP_INTERLACED_VIDEO)


static ReactionResult besSurfaceListener (const void *msg_data, void *ctx)
{
     CoreSurfaceNotification *notification = (CoreSurfaceNotification*)msg_data;
//     DisplayLayer            *layer        = (DisplayLayer*) ctx;

     if (notification->flags & (CSNF_SET_EVEN | CSNF_SET_ODD)) {
          bes_regs.besCTL_field =
               (notification->flags & CSNF_SET_ODD) ? 0x2000000 : 0;

          mga_out32( mmio_base, bes_regs.besCTL | bes_regs.besCTL_field, BESCTL );
     }

     if (notification->flags & CSNF_DESTROY)
          return RS_REMOVE;

     return RS_OK;
}


/**********************/

DFBResult besEnable( DisplayLayer *layer )
{
     if (!layer->surface) {
          DFBResult              ret;
          DFBSurfaceCapabilities caps = DSCAPS_VIDEOONLY;

          if (layer->options & DLOP_INTERLACED_VIDEO)
               caps |= DSCAPS_INTERLACED;

          /* FIXME HARDCODER! */
          ret = surface_create( layer->width, layer->height, DSPF_RGB16,
                                CSP_VIDEOONLY, caps, &layer->surface );
          if (ret)
               return ret;

          reactor_attach( layer->surface->reactor, besSurfaceListener, layer );
     }

     if (!layer->windowstack)
          layer->windowstack = windowstack_new( layer );

     layer->enabled = 1;

     bes_calc_regs( layer );
     bes_set_regs();

     return DFB_OK;
}

DFBResult besDisable( DisplayLayer *layer )
{
     /* FIXME: The surface should be destroyed, and the window stack? */
     layer->enabled = 0;

     mga_out32( mmio_base, 0, BESCTL );

     return DFB_OK;
}

DFBResult besTestConfiguration( DisplayLayer               *layer,
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
               case DSPF_RGB32:
                    max_width = 512;
               case DSPF_RGB15:
               case DSPF_RGB16:
               case DSPF_YUY2:
               case DSPF_UYVY: /* FIXME: not supported on G200 */
                    break;
               default:
                    fail |= DLCONF_PIXELFORMAT;
          }
     else
          if (layer->surface->format == DSPF_RGB32)
               max_width = 512;

     if (config->flags & DLCONF_WIDTH) {
          if (config->width > max_width || config->width < 1)
               fail |= DLCONF_WIDTH;
     }
     else {
          if (layer->width > max_width)
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

DFBResult besSetConfiguration( DisplayLayer          *layer,
                               DFBDisplayLayerConfig *config )
{
     DFBResult                 ret;
     int                       width, height;
     DFBSurfacePixelFormat     format;
     DFBDisplayLayerBufferMode buffermode;
     DFBDisplayLayerOptions    options;

     ret = besTestConfiguration( layer, config, NULL );
     if (ret)
          return ret;

     if (config->flags & DLCONF_WIDTH)
          width = config->width;
     else
          width = layer->width;

     if (config->flags & DLCONF_HEIGHT)
          height = config->height;
     else
          height = layer->height;

     if (config->flags & DLCONF_PIXELFORMAT)
          format = config->pixelformat;
     else
          format = layer->surface->format;

     if (config->flags & DLCONF_BUFFERMODE)
          buffermode = config->buffermode;
     else
          buffermode = layer->buffermode;

     if (config->flags & DLCONF_OPTIONS)
          options = config->options;
     else
          options = layer->options;

     if (layer->buffermode != buffermode) {
          ONCE("Changing the buffermode of the overlay is unimplemented!");
          return DFB_UNIMPLEMENTED;
     }

     if (layer->width != width ||
         layer->height != height ||
         layer->surface->format != format ||
         layer->options != options)
     {
          /* FIXME: write surface management functions
                    for easier configuration changes */

          ret = surface_reformat( layer->surface, width, height, format );
          if (ret)
               return ret;

          if (options & DLOP_INTERLACED_VIDEO)
               layer->surface->caps |= DSCAPS_INTERLACED;
          else
               layer->surface->caps &= ~DSCAPS_INTERLACED;

          layer->options = options;
          layer->width   = width;
          layer->height  = height;

          layer->windowstack->cursor_region.x1 = 0;
          layer->windowstack->cursor_region.y1 = 0;
          layer->windowstack->cursor_region.x2 = layer->width - 1;
          layer->windowstack->cursor_region.y2 = layer->height - 1;

          bes_calc_regs( layer );
          bes_set_regs();
     }

     return DFB_OK;
}

DFBResult besSetOpacity( DisplayLayer *layer,
                         __u8          opacity )
{
     switch (opacity) {
          case 0:
               mga_out32( mmio_base, 0, BESCTL );
               break;
          case 0xFF:
               mga_out32( mmio_base, 1, BESCTL );
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

DFBResult besSetScreenLocation( DisplayLayer *layer,
                                float         x,
                                float         y,
                                float         width,
                                float         height )
{
     layer->screen.x = x;
     layer->screen.y = y;
     layer->screen.w = width;
     layer->screen.h = height;

     bes_calc_regs( layer );
     bes_set_regs();

     return DFB_OK;
}

DFBResult besSetColorKey( DisplayLayer *layer,
                          __u32         key )
{
     return DFB_UNSUPPORTED;
}

DFBResult besFlipBuffers( DisplayLayer *layer )
{
     return DFB_UNIMPLEMENTED;
}

DFBResult besSetColorAdjustment( DisplayLayer       *layer,
                                 DFBColorAdjustment *adj )
{
     if (adj->flags & ~(DCAF_BRIGHTNESS | DCAF_CONTRAST))
          return DFB_UNSUPPORTED;

     if (adj->flags & DCAF_BRIGHTNESS)
          layer->adjustment.brightness = adj->brightness;

     if (adj->flags & DCAF_CONTRAST)
          layer->adjustment.contrast = adj->contrast;

     bes_calc_regs( layer );
     bes_set_regs();

     return DFB_OK;
}

void matrox_bes_deinit( DisplayLayer *layer )
{
     mga_out32( mmio_base, 0, BESCTL );
}

/* exported symbols */

void driver_init_layers()
{
     DisplayLayer *layer;

     if (old_matrox)
          return;

     layer = (DisplayLayer*)DFBCALLOC( 1, sizeof(DisplayLayer) );

     layer->caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE | DLCAPS_BRIGHTNESS |
                   DLCAPS_CONTRAST | DLCAPS_INTERLACED_VIDEO;

     sprintf( layer->description, "Matrox Backend Scaler" );

     layer->enabled = 0;

     layer->width = 640;
     layer->height = 480;
     layer->buffermode = DLBM_FRONTONLY;
     layer->options = 0;

     layer->screen.x = 0.0f;
     layer->screen.y = 0.0f;
     layer->screen.w = 1.0f;
     layer->screen.h = 1.0f;

     layer->opacity = 0xFF;

     layer->bg.mode  = DLBM_DONTCARE;

     layer->adjustment.flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST;
     layer->adjustment.brightness = 0x8000;
     layer->adjustment.contrast   = 0x8000;

     layer->Enable = besEnable;
     layer->Disable = besDisable;
     layer->TestConfiguration = besTestConfiguration;
     layer->SetConfiguration = besSetConfiguration;
     layer->SetOpacity = besSetOpacity;
     layer->SetScreenLocation = besSetScreenLocation;
     layer->SetColorKey = besSetColorKey;
     layer->FlipBuffers = besFlipBuffers;
     layer->SetColorAdjustment = besSetColorAdjustment;

     layer->deinit = matrox_bes_deinit;

     outMGAdac(0x51, 0x00); /* keying off */
     outMGAdac(0x52, 0xFF); /* full mask */
     outMGAdac(0x53, 0xFF);
     outMGAdac(0x54, 0xFF);

     mga_out32( mmio_base, 0x80, BESLUMACTL );

     /* gets filled by layers_add: layer->id */
     layers_add( layer );
}



/* internal */

static void bes_set_regs()
{
     mga_out32( mmio_base, bes_regs.besGLOBCTL, BESGLOBCTL);

     mga_out32( mmio_base, bes_regs.besA1ORG, BESA1ORG );
     mga_out32( mmio_base, bes_regs.besA2ORG, BESA2ORG );

     mga_out32( mmio_base, bes_regs.besCTL | bes_regs.besCTL_field, BESCTL );

     mga_out32( mmio_base, bes_regs.besHCOORD, BESHCOORD );
     mga_out32( mmio_base, bes_regs.besVCOORD, BESVCOORD );

     mga_out32( mmio_base, bes_regs.besHSRCST, BESHSRCST );
     mga_out32( mmio_base, bes_regs.besHSRCEND, BESHSRCEND );
     mga_out32( mmio_base, bes_regs.besHSRCLST, BESHSRCLST );

     mga_out32( mmio_base, bes_regs.besLUMACTL, BESLUMACTL );
     mga_out32( mmio_base, bes_regs.besPITCH, BESPITCH );

     mga_out32( mmio_base, bes_regs.besV1WGHT, BESV1WGHT );
     mga_out32( mmio_base, bes_regs.besV2WGHT, BESV2WGHT );

     mga_out32( mmio_base, bes_regs.besV1SRCLST, BESV1SRCLST );
     mga_out32( mmio_base, bes_regs.besV2SRCLST, BESV2SRCLST );

     mga_out32( mmio_base, bes_regs.besVISCAL, BESVISCAL );
     mga_out32( mmio_base, bes_regs.besHISCAL, BESHISCAL );
}

static void bes_calc_regs( DisplayLayer *layer )
{
     int tmp, hzoom, intrep;

     DFBRegion    dstBox;
     int          drw_w;
     int          drw_h;
     int          field_height;
     CoreSurface *surface = layer->surface;


     drw_w = (int)(layer->screen.w * (float)fbdev->current_mode->xres + 0.5f);
     drw_h = (int)(layer->screen.h * (float)fbdev->current_mode->yres + 0.5f);

     dstBox.x1 = (int)(layer->screen.x * (float)fbdev->current_mode->xres + 0.5f);
     dstBox.y1 = (int)(layer->screen.y * (float)fbdev->current_mode->yres + 0.5f);
     dstBox.x2 = (int)((layer->screen.x + layer->screen.w) * (float)fbdev->current_mode->xres + 0.5f);
     dstBox.y2 = (int)((layer->screen.y + layer->screen.h) * (float)fbdev->current_mode->yres + 0.5f);

     hzoom = (1000000/fbdev->current_var.pixclock >= 135) ? 1 : 0;

     bes_regs.besCTL = BESCTL_BESEN;

     switch (surface->format) {
          case DSPF_YUY2:
               bes_regs.besGLOBCTL = BESPROCAMP;
               bes_regs.besCTL    |= BESCTL_BESHFEN |
                                     BESCTL_BESVFEN | BESCTL_BESCUPS;
               break;
          case DSPF_UYVY:
               bes_regs.besGLOBCTL = BESPROCAMP | 0x40;
               bes_regs.besCTL    |= BESCTL_BESHFEN |
                                     BESCTL_BESVFEN | BESCTL_BESCUPS;
               break;
          case DSPF_RGB15:
               bes_regs.besGLOBCTL = BESRGB15;
               break;
          case DSPF_RGB16:
               bes_regs.besGLOBCTL = BESRGB16;
               break;
          case DSPF_RGB32:
               drw_w = layer->width;
               dstBox.x2 = dstBox.x1 + layer->width;
               bes_regs.besGLOBCTL = BESRGB32;
               break;
          default:
               BUG( "unexpected pixelformat" );
               return;
     }

     bes_regs.besGLOBCTL |= 3*hzoom | (fbdev->current_mode->yres & 0xFFF) << 16;
     bes_regs.besA1ORG    = surface->front_buffer->video.offset;
     bes_regs.besA2ORG    = surface->front_buffer->video.offset +
                            surface->front_buffer->video.pitch;

     bes_regs.besHCOORD   = (dstBox.x1 << 16) | (dstBox.x2 - 1);
     bes_regs.besVCOORD   = (dstBox.y1 << 16) | (dstBox.y2 - 1);

     bes_regs.besHSRCST   = 0;
     bes_regs.besHSRCEND  = (layer->width - 1) << 16;
     bes_regs.besHSRCLST  = (layer->width - 1) << 16;

     bes_regs.besLUMACTL  = (layer->adjustment.contrast >> 8) |
                            ((__u8)(((int)layer->adjustment.brightness >> 8)
                                     - 128)) << 16;

     bes_regs.besV1WGHT   = 0;
     bes_regs.besV2WGHT   = 0x18000;

     bes_regs.besV1SRCLST = layer->height - 1;
     bes_regs.besV2SRCLST = layer->height - 2;

     bes_regs.besPITCH    = surface->front_buffer->video.pitch;

     field_height         = layer->height;

     if (layer->options & DLOP_INTERLACED_VIDEO)
          field_height >>= 1;
     else {
          bes_regs.besPITCH >>= 1;
          bes_regs.besCTL_field = 0;
     }


     if (layer->surface->format == DSPF_RGB32)
          bes_regs.besHISCAL = 0x20000;
     else {
          intrep = ((drw_w == layer->width) || (drw_w < 2)) ? 0 : 1;
          tmp = (((layer->width - intrep) << 16) / (drw_w - intrep)) << hzoom;
          if (tmp >= (32 << 16))
               tmp = (32 << 16) - 1;
          bes_regs.besHISCAL = tmp & 0x001ffffc;
     }

     intrep = ((drw_h == field_height) || (drw_h < 2)) ? 0 : 1;
     tmp = ((field_height - intrep) << 16) / (drw_h - intrep);
     if(tmp >= (32 << 16))
          tmp = (32 << 16) - 1;
     bes_regs.besVISCAL = tmp & 0x001ffffc;
}

