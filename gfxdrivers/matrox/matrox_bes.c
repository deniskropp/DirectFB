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
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/windows.h>

#include <core/fbdev/fbdev.h> /* FIXME! */

#include <misc/mem.h>


#include "regs.h"
#include "mmio.h"
#include "matrox.h"

typedef struct {
     bool                  listener_running;
     DFBRectangle          dest;
     DFBDisplayLayerConfig config;

     /* Stored registers */
     struct {
          /* BES */
          __u32 besGLOBCTL;
          __u32 besA1ORG;
          __u32 besA2ORG;
          __u32 besA1CORG;
          __u32 besA2CORG;
          __u32 besA1C3ORG;
          __u32 besA2C3ORG;
          __u32 besCTL;

          __u32 besCTL_field;

          __u32 besHCOORD;
          __u32 besVCOORD;

          __u32 besHSRCST;
          __u32 besHSRCEND;
          __u32 besHSRCLST;

          __u32 besPITCH;

          __u32 besV1WGHT;
          __u32 besV2WGHT;

          __u32 besV1SRCLST;
          __u32 besV2SRCLST;

          __u32 besVISCAL;
          __u32 besHISCAL;

          __u8  xKEYOPMODE;
     } regs;
} MatroxBesLayerData;

static void bes_set_regs( MatroxDriverData *mdrv, MatroxBesLayerData *mbes,
                          bool onsync );
static void bes_calc_regs( MatroxDriverData *mdrv, MatroxBesLayerData *mbes,
                           DisplayLayer *layer, DFBDisplayLayerConfig *config );

#define BES_SUPPORTED_OPTIONS   (DLOP_DEINTERLACING | DLOP_DST_COLORKEY)


/**********************/

static int
besLayerDataSize()
{
     return sizeof(MatroxBesLayerData);
}
     
static DFBResult
besInitLayer( GraphicsDevice             *device,
              DisplayLayer               *layer,
              DisplayLayerInfo           *layer_info,
              DFBDisplayLayerConfig      *default_config,
              DFBColorAdjustment         *default_adj,
              void                       *driver_data,
              void                       *layer_data )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;
     MatroxBesLayerData *mbes = (MatroxBesLayerData*) layer_data;
     volatile __u8      *mmio = mdrv->mmio_base;
     
     /* set capabilities and type */
     layer_info->desc.caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE |
                             DLCAPS_BRIGHTNESS | DLCAPS_CONTRAST |
                             DLCAPS_DEINTERLACING | DLCAPS_DST_COLORKEY;
     layer_info->desc.type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( layer_info->name,
               DFB_DISPLAY_LAYER_INFO_NAME_LENGTH, "Matrox Backend Scaler" );

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
     default_adj->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST;
     default_adj->brightness = 0x8000;
     default_adj->contrast   = 0x8000;
     
     
     /* initialize destination rectangle */
     dfb_primary_layer_rectangle( 0.0f, 0.0f, 1.0f, 1.0f, &mbes->dest );
     
     /* disable backend scaler */
     mga_out32( mmio, 0, BESCTL );
     
     /* set defaults */
     mga_out_dac( mmio, XKEYOPMODE, 0x00 ); /* keying off */

     mga_out_dac( mmio, XCOLMSK0RED,   0xFF ); /* full mask */
     mga_out_dac( mmio, XCOLMSK0GREEN, 0xFF );
     mga_out_dac( mmio, XCOLMSK0BLUE,  0xFF );

     mga_out_dac( mmio, XCOLKEY0RED,   0x00 ); /* default to black */
     mga_out_dac( mmio, XCOLKEY0GREEN, 0x00 );
     mga_out_dac( mmio, XCOLKEY0BLUE,  0x00 );

     mga_out32( mmio, 0x80, BESLUMACTL );
     
     return DFB_OK;
}


static void
besOnOff( MatroxDriverData   *mdrv,
          MatroxBesLayerData *mbes,
          int                 on )
{
     if (on)
          mbes->regs.besCTL |= 1;
     else
          mbes->regs.besCTL &= ~1;

     mga_out32( mdrv->mmio_base,
                mbes->regs.besCTL | mbes->regs.besCTL_field, BESCTL );
}

static DFBResult
besEnable( DisplayLayer *layer,
           void         *driver_data,
           void         *layer_data )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;
     MatroxBesLayerData *mbes = (MatroxBesLayerData*) layer_data;
     
     /* enable backend scaler */
     besOnOff( mdrv, mbes, 1 );

     return DFB_OK;
}

static DFBResult
besDisable( DisplayLayer *layer,
            void         *driver_data,
            void         *layer_data )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;
     MatroxBesLayerData *mbes = (MatroxBesLayerData*) layer_data;

     /* disable backend scaler */
     besOnOff( mdrv, mbes, 0 );

     return DFB_OK;
}

static DFBResult
besTestConfiguration( DisplayLayer               *layer,
                      void                       *driver_data,
                      void                       *layer_data,
                      DFBDisplayLayerConfig      *config,
                      DFBDisplayLayerConfigFlags *failed )
{
     int                         max_width = 1024;
     DFBDisplayLayerConfigFlags  fail      = 0;
     MatroxDriverData           *mdrv      = (MatroxDriverData*) driver_data;

     if (config->options & ~BES_SUPPORTED_OPTIONS)
          fail |= DLCONF_OPTIONS;

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
               if (mdrv->accelerator != FB_ACCEL_MATROX_MGAG200)
                    break;
          default:
               fail |= DLCONF_PIXELFORMAT;
     }

     if (config->width > max_width || config->width < 1)
          fail |= DLCONF_WIDTH;

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
                     void                  *driver_data,
                     void                  *layer_data,
                     DFBDisplayLayerConfig *config )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;
     MatroxBesLayerData *mbes = (MatroxBesLayerData*) layer_data;

     /* remember configuration */
     mbes->config = *config;
     
     bes_calc_regs( mdrv, mbes, layer, config );
     bes_set_regs( mdrv, mbes, true );

     return DFB_OK;
}

static DFBResult
besSetOpacity( DisplayLayer *layer,
               void         *driver_data,
               void         *layer_data,
               __u8          opacity )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;
     MatroxBesLayerData *mbes = (MatroxBesLayerData*) layer_data;
     
     switch (opacity) {
          case 0:
               besOnOff( mdrv, mbes, 0 );
               break;
          case 0xFF:
               besOnOff( mdrv, mbes, 1 );
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
besSetScreenLocation( DisplayLayer *layer,
                      void         *driver_data,
                      void         *layer_data,
                      float         x,
                      float         y,
                      float         width,
                      float         height )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;
     MatroxBesLayerData *mbes = (MatroxBesLayerData*) layer_data;
     
     /* get new destination rectangle */
     dfb_primary_layer_rectangle( x, y, width, height, &mbes->dest );

     bes_calc_regs( mdrv, mbes, layer, &mbes->config );
     bes_set_regs( mdrv, mbes, true );
     
     return DFB_OK;
}

static DFBResult
besSetDstColorKey( DisplayLayer *layer,
                   void         *driver_data,
                   void         *layer_data,
                   __u8          r,
                   __u8          g,
                   __u8          b )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) driver_data;
     volatile __u8    *mmio = mdrv->mmio_base;
     
     switch (dfb_primary_layer_pixelformat()) {
          case DSPF_RGB15:
               r >>= 3;
               g >>= 3;
               b >>= 3;
               break;
          
          case DSPF_RGB16:
               r >>= 3;
               g >>= 2;
               b >>= 3;
               break;
          
          default:
               ;
     }
     
     mga_out_dac( mmio, XCOLKEY0RED,   r );
     mga_out_dac( mmio, XCOLKEY0GREEN, g );
     mga_out_dac( mmio, XCOLKEY0BLUE,  b );

     return DFB_OK;
}

static DFBResult
besFlipBuffers( DisplayLayer        *layer,
                void                *driver_data,
                void                *layer_data,
                DFBSurfaceFlipFlags  flags )
{
     MatroxDriverData   *mdrv    = (MatroxDriverData*) driver_data;
     MatroxBesLayerData *mbes    = (MatroxBesLayerData*) layer_data;
     CoreSurface        *surface = dfb_layer_surface( layer );
     bool                onsync  = (flags & DSFLIP_WAITFORSYNC);
     
     dfb_surface_flip_buffers( surface );
     
     bes_calc_regs( mdrv, mbes, layer, &mbes->config );
     bes_set_regs( mdrv, mbes, onsync );

     if (onsync)
          dfb_fbdev_wait_vsync();
     
     return DFB_OK;
}

static DFBResult
besSetColorAdjustment( DisplayLayer       *layer,
                       void               *driver_data,
                       void               *layer_data,
                       DFBColorAdjustment *adj )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) driver_data;
     volatile __u8    *mmio = mdrv->mmio_base;
     
     mga_out32( mmio, (adj->contrast >> 8) |
                      ((__u8)(((int)adj->brightness >> 8) - 128)) << 16,
                BESLUMACTL );
     
     return DFB_OK;
}

static DFBResult
besSetField( DisplayLayer *layer,
             void         *driver_data,
             void         *layer_data,
             int           field )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;
     MatroxBesLayerData *mbes = (MatroxBesLayerData*) layer_data;
     
     mbes->regs.besCTL_field = field ? 0x2000000 : 0;

     mga_out32( mdrv->mmio_base,
                mbes->regs.besCTL | mbes->regs.besCTL_field, BESCTL );
     
     return DFB_OK;
}


DisplayLayerFuncs matroxBesFuncs = {
     LayerDataSize:      besLayerDataSize,
     InitLayer:          besInitLayer,
     Enable:             besEnable,
     Disable:            besDisable,
     TestConfiguration:  besTestConfiguration,
     SetConfiguration:   besSetConfiguration,
     SetOpacity:         besSetOpacity,
     SetScreenLocation:  besSetScreenLocation,
     SetDstColorKey:     besSetDstColorKey,
     FlipBuffers:        besFlipBuffers,
     SetColorAdjustment: besSetColorAdjustment,
     SetField:           besSetField
};


/* internal */

static void bes_set_regs( MatroxDriverData *mdrv, MatroxBesLayerData *mbes,
                          bool onsync )
{
     int            line = 0;
     volatile __u8 *mmio = mdrv->mmio_base;

     if (!onsync) {
          line = mga_in32( mmio, MGAREG_VCOUNT ) + 48;

          if (line > dfb_fbdev->shared->current_mode->yres)
               line = dfb_fbdev->shared->current_mode->yres;
     }
     
     mga_out32( mmio, mbes->regs.besGLOBCTL | (line << 16), BESGLOBCTL);

     mga_out32( mmio, mbes->regs.besA1ORG, BESA1ORG );
     mga_out32( mmio, mbes->regs.besA2ORG, BESA2ORG );
     mga_out32( mmio, mbes->regs.besA1CORG, BESA1CORG );
     mga_out32( mmio, mbes->regs.besA2CORG, BESA2CORG );

     if (mdrv->accelerator != FB_ACCEL_MATROX_MGAG200) {
          mga_out32( mmio, mbes->regs.besA1C3ORG, BESA1C3ORG );
          mga_out32( mmio, mbes->regs.besA2C3ORG, BESA2C3ORG );
     }

     mga_out32( mmio, mbes->regs.besCTL | mbes->regs.besCTL_field, BESCTL );

     mga_out32( mmio, mbes->regs.besHCOORD, BESHCOORD );
     mga_out32( mmio, mbes->regs.besVCOORD, BESVCOORD );

     mga_out32( mmio, mbes->regs.besHSRCST, BESHSRCST );
     mga_out32( mmio, mbes->regs.besHSRCEND, BESHSRCEND );
     mga_out32( mmio, mbes->regs.besHSRCLST, BESHSRCLST );

     mga_out32( mmio, mbes->regs.besPITCH, BESPITCH );

     mga_out32( mmio, mbes->regs.besV1WGHT, BESV1WGHT );
     mga_out32( mmio, mbes->regs.besV2WGHT, BESV2WGHT );

     mga_out32( mmio, mbes->regs.besV1SRCLST, BESV1SRCLST );
     mga_out32( mmio, mbes->regs.besV2SRCLST, BESV2SRCLST );

     mga_out32( mmio, mbes->regs.besVISCAL, BESVISCAL );
     mga_out32( mmio, mbes->regs.besHISCAL, BESHISCAL );

     mga_out_dac( mmio, XKEYOPMODE, mbes->regs.xKEYOPMODE );
}

static void bes_calc_regs( MatroxDriverData *mdrv, MatroxBesLayerData *mbes,
                           DisplayLayer *layer, DFBDisplayLayerConfig *config )
{
     int tmp, hzoom, intrep;

     DFBRegion      dstBox;
     int            drw_w, drw_h;
     int            field_height;
     CoreSurface   *surface      = dfb_layer_surface( layer );
     SurfaceBuffer *front_buffer = surface->front_buffer;

     /* destination box */
     dstBox.x1 = mbes->dest.x;
     dstBox.y1 = mbes->dest.y;
     dstBox.x2 = mbes->dest.x + mbes->dest.w;
     dstBox.y2 = mbes->dest.y + mbes->dest.h;

     /* destination size */
     drw_w = mbes->dest.w;
     drw_h = mbes->dest.h;
     
     /* should horizontal zoom be used? */
     hzoom = (1000000/Sfbdev->current_var.pixclock >= 135) ? 1 : 0;

     /* initialize */
     mbes->regs.besGLOBCTL = 0;

     /* clear everything but the enable bit that may be set */
     mbes->regs.besCTL &= 1;

     /* pixel format settings */
     switch (surface->format) {
          case DSPF_I420:
          case DSPF_YV12:
               mbes->regs.besGLOBCTL |= BESPROCAMP | BES3PLANE;
               mbes->regs.besCTL     |= BESHFEN | BESVFEN | BESCUPS | BES420PL;
               break;

          case DSPF_UYVY:
               mbes->regs.besGLOBCTL |= BESUYVYFMT;
               /* fall through */

          case DSPF_YUY2:
               mbes->regs.besGLOBCTL |= BESPROCAMP;
               mbes->regs.besCTL     |= BESHFEN | BESVFEN | BESCUPS;
               break;

          case DSPF_RGB15:
               mbes->regs.besGLOBCTL |= BESRGB15;
               break;

          case DSPF_RGB16:
               mbes->regs.besGLOBCTL |= BESRGB16;
               break;

          case DSPF_RGB32:
               mbes->regs.besGLOBCTL |= BESRGB32;

               drw_w = surface->width;
               dstBox.x2 = dstBox.x1 + surface->width;
               break;

          default:
               BUG( "unexpected pixelformat" );
               return;
     }

     mbes->regs.besGLOBCTL |= 3*hzoom | (Sfbdev->current_mode->yres & 0xFFF) << 16;
     mbes->regs.besA1ORG    = front_buffer->video.offset;
     mbes->regs.besA2ORG    = front_buffer->video.offset +
                              front_buffer->video.pitch;

     switch (surface->format) {
          case DSPF_I420:
               mbes->regs.besA1CORG  = mbes->regs.besA1ORG + surface->height *
                                       front_buffer->video.pitch;
               mbes->regs.besA1C3ORG = mbes->regs.besA1CORG + surface->height/2 *
                                       front_buffer->video.pitch/2;
               mbes->regs.besA2CORG  = mbes->regs.besA2ORG + surface->height *
                                       front_buffer->video.pitch;
               mbes->regs.besA2C3ORG = mbes->regs.besA2CORG + surface->height/2 *
                                       front_buffer->video.pitch/2;
               break;

          case DSPF_YV12:
               mbes->regs.besA1C3ORG = mbes->regs.besA1ORG + surface->height *
                                       front_buffer->video.pitch;
               mbes->regs.besA1CORG  = mbes->regs.besA1C3ORG + surface->height/2 *
                                       front_buffer->video.pitch/2;
               mbes->regs.besA2C3ORG = mbes->regs.besA2ORG + surface->height *
                                       front_buffer->video.pitch;
               mbes->regs.besA2CORG  = mbes->regs.besA2C3ORG + surface->height/2 *
                                       front_buffer->video.pitch/2;
               break;

          default:
               ;
     }

     mbes->regs.besHCOORD   = (dstBox.x1 << 16) | (dstBox.x2 - 1);
     mbes->regs.besVCOORD   = (dstBox.y1 << 16) | (dstBox.y2 - 1);

     mbes->regs.besHSRCST   = 0;
     mbes->regs.besHSRCEND  = (surface->width - 1) << 16;
     mbes->regs.besHSRCLST  = (surface->width - 1) << 16;

     mbes->regs.besV1WGHT   = 0;
     mbes->regs.besV2WGHT   = 0x18000;

     mbes->regs.besV1SRCLST = surface->height - 1;
     mbes->regs.besV2SRCLST = surface->height - 2;

     mbes->regs.besPITCH    = front_buffer->video.pitch /
                              DFB_BYTES_PER_PIXEL(surface->format);

     field_height           = surface->height;

     if (config->options & DLOP_DEINTERLACING) {
          field_height        /= 2;
          mbes->regs.besPITCH *= 2;
     }
     else
          mbes->regs.besCTL_field = 0;

     if (config->pixelformat == DSPF_RGB32)
          mbes->regs.besHISCAL = 0x20000;
     else {
          intrep = ((drw_w == surface->width) || (drw_w < 2)) ? 0 : 1;
          tmp = (((surface->width - intrep) << 16) / (drw_w - intrep)) << hzoom;
          if (tmp >= (32 << 16))
               tmp = (32 << 16) - 1;
          mbes->regs.besHISCAL = tmp & 0x001ffffc;
     }
     
     intrep = ((drw_h == field_height) || (drw_h < 2)) ? 0 : 1;
     tmp = ((field_height - intrep) << 16) / (drw_h - intrep);
     if(tmp >= (32 << 16))
          tmp = (32 << 16) - 1;
     mbes->regs.besVISCAL = tmp & 0x001ffffc;

     /* enable color keying? */
     mbes->regs.xKEYOPMODE = (config->options & DLOP_DST_COLORKEY) ? 1 : 0;
}

