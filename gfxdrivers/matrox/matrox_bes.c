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

#include <linux/fb.h>

#include <directfb.h>

#include <direct/messages.h>

#include <fusion/shmalloc.h>

#include <core/accelerators.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/gfxcard.h>
#include <core/layer_control.h>
#include <core/layers.h>
#include <core/screen.h>
#include <core/surfaces.h>
#include <core/system.h>
#include <core/windows.h>

#include <direct/mem.h>

#include <misc/util.h>

#include "regs.h"
#include "mmio.h"
#include "matrox.h"

typedef struct {
     CoreLayerRegionConfig config;

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
                           CoreLayerRegionConfig *config, CoreSurface *surface );

#define BES_SUPPORTED_OPTIONS   (DLOP_DEINTERLACING | DLOP_DST_COLORKEY)


/**********************/

static int
besLayerDataSize()
{
     return sizeof(MatroxBesLayerData);
}

static DFBResult
besInitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;
     volatile __u8      *mmio = mdrv->mmio_base;

     /* set capabilities and type */
     description->caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE |
                         DLCAPS_DEINTERLACING | DLCAPS_DST_COLORKEY;
     description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Matrox Backend Scaler" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;
     config->width       = 640;
     config->height      = 480;
     config->pixelformat = DSPF_YUY2;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;

     adjustment->flags   = DCAF_NONE;

     if (mdrv->accelerator != FB_ACCEL_MATROX_MGAG200) {
          description->caps      |= DLCAPS_BRIGHTNESS | DLCAPS_CONTRAST;

          /* fill out default color adjustment,
             only fields set in flags will be accepted from applications */
          adjustment->flags      |= DCAF_BRIGHTNESS | DCAF_CONTRAST;
          adjustment->brightness  = 0x8000;
          adjustment->contrast    = 0x8000;

          mga_out32( mmio, 0x80, BESLUMACTL );
     }

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

     return DFB_OK;
}

static DFBResult
besTestRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags *failed )
{
     MatroxDriverData           *mdrv       = (MatroxDriverData*) driver_data;
     MatroxDeviceData           *mdev       = mdrv->device_data;
     int                         max_width  = mdev->g450_matrox ? 2048 : 1024;
     int                         max_height = 1024;
     CoreLayerRegionConfigFlags  fail       = 0;

     if (config->options & ~BES_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     if (config->options & DLOP_DEINTERLACING) {
          /* make sure BESPITCH < 4096 */
          max_width = mdev->g450_matrox ? 2048 - 128 : 1024;
          max_height = 2048;
     }

     switch (config->format) {
          case DSPF_YUY2:
          case DSPF_NV12:
          case DSPF_NV21:
               break;

          case DSPF_ARGB:
          case DSPF_RGB32:
               if (!mdev->g450_matrox)
                    max_width = 512;
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               /* these formats are not supported by G200 */
               if (mdrv->accelerator != FB_ACCEL_MATROX_MGAG200)
                    break;
          default:
               fail |= CLRCF_FORMAT;
     }

     switch (config->format) {
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_NV12:
          case DSPF_NV21:
               if (config->height & 1)
                    fail |= CLRCF_HEIGHT;
          case DSPF_YUY2:
          case DSPF_UYVY:
               if (config->width & 1)
                    fail |= CLRCF_WIDTH;
          default:
               break;
     }

     if (config->width > max_width || config->width < 1)
          fail |= CLRCF_WIDTH;

     if (config->height > max_height || config->height < 1)
          fail |= CLRCF_HEIGHT;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
besSetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags  updated,
              CoreSurface                *surface,
              CorePalette                *palette )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;
     MatroxBesLayerData *mbes = (MatroxBesLayerData*) layer_data;
     volatile __u8      *mmio = mdrv->mmio_base;

     /* remember configuration */
     mbes->config = *config;

     /* set main configuration */
     if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT |
                    CLRCF_OPTIONS | CLRCF_DEST | CLRCF_OPACITY | CLRCF_SOURCE))
     {
          bes_calc_regs( mdrv, mbes, config, surface );
          bes_set_regs( mdrv, mbes, true );
     }

     /* set color key */
     if (updated & CLRCF_DSTKEY) {
          DFBColor key = config->dst_key;

          switch (dfb_primary_layer_pixelformat()) {
               case DSPF_ARGB1555:
                    key.r >>= 3;
                    key.g >>= 3;
                    key.b >>= 3;
                    break;

               case DSPF_RGB16:
                    key.r >>= 3;
                    key.g >>= 2;
                    key.b >>= 3;
                    break;

               default:
                    ;
          }

          mga_out_dac( mmio, XCOLKEY0RED,   key.r );
          mga_out_dac( mmio, XCOLKEY0GREEN, key.g );
          mga_out_dac( mmio, XCOLKEY0BLUE,  key.b );
     }

     return DFB_OK;
}

static DFBResult
besRemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;

     /* disable backend scaler */
     mga_out32( mdrv->mmio_base, 0, BESCTL );

     return DFB_OK;
}

static DFBResult
besFlipRegion( CoreLayer           *layer,
               void                *driver_data,
               void                *layer_data,
               void                *region_data,
               CoreSurface         *surface,
               DFBSurfaceFlipFlags  flags )
{
     MatroxDriverData   *mdrv = (MatroxDriverData*) driver_data;
     MatroxBesLayerData *mbes = (MatroxBesLayerData*) layer_data;

     bes_calc_regs( mdrv, mbes, &mbes->config, surface );
     bes_set_regs( mdrv, mbes, flags & DSFLIP_ONSYNC );

     dfb_surface_flip_buffers( surface, false );

     if (flags & DSFLIP_WAIT)
          dfb_screen_wait_vsync( mdrv->primary );

     return DFB_OK;
}

static DFBResult
besSetColorAdjustment( CoreLayer          *layer,
                       void               *driver_data,
                       void               *layer_data,
                       DFBColorAdjustment *adj )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) driver_data;
     volatile __u8    *mmio = mdrv->mmio_base;

     if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG200)
          return DFB_UNSUPPORTED;

     mga_out32( mmio, (adj->contrast >> 8) |
                      ((__u8)(((int)adj->brightness >> 8) - 128)) << 16,
                BESLUMACTL );

     return DFB_OK;
}

static DFBResult
besSetInputField( CoreLayer *layer,
                  void      *driver_data,
                  void      *layer_data,
                  void      *region_data,
                  int        field )
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

     TestRegion:         besTestRegion,
     SetRegion:          besSetRegion,
     RemoveRegion:       besRemoveRegion,
     FlipRegion:         besFlipRegion,

     SetColorAdjustment: besSetColorAdjustment,
     SetInputField:      besSetInputField
};


/* internal */

static void bes_set_regs( MatroxDriverData *mdrv, MatroxBesLayerData *mbes,
                          bool onsync )
{
     int            line = 0;
     volatile __u8 *mmio = mdrv->mmio_base;
     VideoMode     *mode = dfb_system_current_mode();

     if (!mode) {
          mode = dfb_system_modes();
          if (!mode)
               return;
     }

     line = onsync ? mode->yres : (mga_in32( mmio, MGAREG_VCOUNT ) + 48);
     if (line > mode->yres)
          line = mode->yres;

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

static void bes_calc_regs( MatroxDriverData      *mdrv,
                           MatroxBesLayerData    *mbes,
                           CoreLayerRegionConfig *config,
                           CoreSurface           *surface )
{
     MatroxDeviceData *mdev = mdrv->device_data;
     int cropleft, cropright, croptop, cropbot, croptop_uv;
     int pitch, tmp, hzoom, intrep, field_height;
     DFBRectangle   source, dest;
     DFBRegion      dst;
     bool           visible;
     SurfaceBuffer *buffer = surface->back_buffer;
     VideoMode     *mode   = dfb_system_current_mode();

     if (!mode) {
          mode = dfb_system_modes();
          if (!mode) {
               D_BUG( "No current and no default mode" );
               return;
          }
     }

     source = config->source;
     dest   = config->dest;

     if (!mdev->g450_matrox && (surface->format == DSPF_RGB32 || surface->format == DSPF_ARGB))
          dest.w = source.w;

     pitch = buffer->video.pitch;

     field_height = surface->height;

     if (config->options & DLOP_DEINTERLACING) {
          field_height /= 2;
          source.y /= 2;
          source.h /= 2;
          pitch    *= 2;
     } else
          mbes->regs.besCTL_field = 0;

     /* destination region */
     dst.x1 = dest.x;
     dst.y1 = dest.y;
     dst.x2 = dest.x + dest.w - 1;
     dst.y2 = dest.y + dest.h - 1;

     visible = dfb_region_intersect( &dst, 0, 0, mode->xres - 1, mode->yres - 1 );

     /* calculate destination cropping */
     cropleft  = -dest.x;
     croptop   = -dest.y;
     cropright = dest.x + dest.w - mode->xres;
     cropbot   = dest.y + dest.h - mode->yres;

     cropleft   = cropleft > 0 ? cropleft : 0;
     croptop    = croptop > 0 ? croptop : 0;
     cropright  = cropright > 0 ? cropright : 0;
     cropbot    = cropbot > 0 ? cropbot : 0;
     croptop_uv = croptop;

     /* scale crop values to source dimensions */
     if (cropleft)
          cropleft = ((__u64) (source.w << 16) * cropleft / dest.w) & ~0x3;
     if (croptop)
          croptop = ((__u64) (source.h << 16) * croptop / dest.h) & ~0x3;
     if (cropright)
          cropright = ((__u64) (source.w << 16) * cropright / dest.w) & ~0x3;
     if (cropbot)
          cropbot = ((__u64) (source.h << 16) * cropbot / dest.h) & ~0x3;
     if (croptop_uv)
          croptop_uv = ((__u64) ((source.h/2) << 16) * croptop_uv / dest.h) & ~0x3;

     /* should horizontal zoom be used? */
     if (mdev->g450_matrox)
          hzoom = (1000000/mode->pixclock >= 234) ? 1 : 0;
     else
          hzoom = (1000000/mode->pixclock >= 135) ? 1 : 0;

     /* initialize */
     mbes->regs.besGLOBCTL = 0;

     /* enable/disable depending on opacity */
     mbes->regs.besCTL = (config->opacity && visible) ? BESEN : 0;

     /* pixel format settings */
     switch (surface->format) {
          case DSPF_YV12:
               mbes->regs.besGLOBCTL |= BESCORDER;
               /* fall through */

          case DSPF_I420:
               mbes->regs.besGLOBCTL |= BESPROCAMP | BES3PLANE;
               mbes->regs.besCTL     |= BESHFEN | BESVFEN | BESCUPS | BES420PL;
               break;

          case DSPF_NV21:
               mbes->regs.besGLOBCTL |= BESCORDER;
               /* fall through */

          case DSPF_NV12:
               mbes->regs.besGLOBCTL |= BESPROCAMP;
               mbes->regs.besCTL     |= BESHFEN | BESVFEN | BESCUPS | BES420PL;
               break;

          case DSPF_UYVY:
               mbes->regs.besGLOBCTL |= BESUYVYFMT;
               /* fall through */

          case DSPF_YUY2:
               mbes->regs.besGLOBCTL |= BESPROCAMP;
               mbes->regs.besCTL     |= BESHFEN | BESVFEN | BESCUPS;
               break;

          case DSPF_ARGB1555:
               mbes->regs.besGLOBCTL |= BESRGB15;
               break;

          case DSPF_RGB16:
               mbes->regs.besGLOBCTL |= BESRGB16;
               break;

          case DSPF_ARGB:
          case DSPF_RGB32:
               mbes->regs.besGLOBCTL |= BESRGB32;
               break;

          default:
               D_BUG( "unexpected pixelformat" );
               return;
     }

     if (surface->width > 1024)
          mbes->regs.besCTL &= ~BESVFEN;

     mbes->regs.besGLOBCTL |= 3*hzoom | (mode->yres & 0xFFF) << 16;

     mbes->regs.besPITCH = pitch / DFB_BYTES_PER_PIXEL(surface->format);

     /* buffer offsets */
     mbes->regs.besA1ORG = buffer->video.offset +
                           pitch * (source.y + (croptop >> 16));
     mbes->regs.besA2ORG = mbes->regs.besA1ORG +
                           buffer->video.pitch;

     switch (surface->format) {
          case DSPF_NV12:
          case DSPF_NV21:
               mbes->regs.besA1CORG  = buffer->video.offset +
                                       surface->height * buffer->video.pitch +
                                       pitch * (source.y/2 + (croptop_uv >> 16));
               mbes->regs.besA2CORG  = mbes->regs.besA1CORG +
                                       buffer->video.pitch;
               break;

          case DSPF_I420:
          case DSPF_YV12:
               mbes->regs.besA1CORG  = buffer->video.offset +
                                       surface->height * buffer->video.pitch +
                                       pitch/2 * (source.y/2 + (croptop_uv >> 16));
               mbes->regs.besA2CORG  = mbes->regs.besA1CORG +
                                       buffer->video.pitch/2;

               mbes->regs.besA1C3ORG = mbes->regs.besA1CORG +
                                       surface->height/2 * buffer->video.pitch/2;
               mbes->regs.besA2C3ORG = mbes->regs.besA1C3ORG +
                                       buffer->video.pitch/2;
               break;

          default:
               ;
     }

     mbes->regs.besHCOORD   = (dst.x1 << 16) | dst.x2;
     mbes->regs.besVCOORD   = (dst.y1 << 16) | dst.y2;

     mbes->regs.besHSRCST   = (source.x << 16) + cropleft;
     mbes->regs.besHSRCEND  = ((source.x + source.w - 1) << 16) - cropright;
     mbes->regs.besHSRCLST  = (surface->width - 1) << 16;

     /* vertical starting weights */
     tmp = croptop & 0xfffc;
     mbes->regs.besV1WGHT = tmp;
     if (tmp >= 0x8000) {
          tmp = tmp - 0x8000;
          /* fields start on the same line */
          if ((source.y + (croptop >> 16)) & 1)
               mbes->regs.besCTL |= BESV1SRCSTP | BESV2SRCSTP;
     } else {
          tmp = 0x10000 | (0x8000 - tmp);
          /* fields start on alternate lines */
          if ((source.y + (croptop >> 16)) & 1)
               mbes->regs.besCTL |= BESV1SRCSTP;
          else
               mbes->regs.besCTL |= BESV2SRCSTP;
     }
     mbes->regs.besV2WGHT = tmp;

     mbes->regs.besV1SRCLST = mbes->regs.besV2SRCLST =
          field_height - 1 - source.y - (croptop >> 16);

     /* horizontal scaling */
     if (!mdev->g450_matrox && (surface->format == DSPF_RGB32 || surface->format == DSPF_ARGB)) {
          mbes->regs.besHISCAL   = 0x20000 << hzoom;
          mbes->regs.besHSRCST  *= 2;
          mbes->regs.besHSRCEND *= 2;
          mbes->regs.besHSRCLST *= 2;
          mbes->regs.besPITCH   *= 2;
     } else {
          intrep = ((mbes->regs.besCTL & BESHFEN) || (source.w > dest.w)) ? 1 : 0;
          if ((dest.w == source.w) || (dest.w < 2))
               intrep = 0;
          tmp = (((source.w - intrep) << 16) / (dest.w - intrep)) << hzoom;
          if (tmp >= (32 << 16))
               tmp = (32 << 16) - 1;
          mbes->regs.besHISCAL = tmp & 0x001ffffc;
     }

     /* vertical scaling */
     intrep = ((mbes->regs.besCTL & BESVFEN) || (source.h > dest.h)) ? 1 : 0;
     if ((dest.h == source.h) || (dest.h < 2))
          intrep = 0;
     tmp = ((source.h - intrep) << 16) / (dest.h - intrep);
     if(tmp >= (32 << 16))
          tmp = (32 << 16) - 1;
     mbes->regs.besVISCAL = tmp & 0x001ffffc;

     /* enable color keying? */
     mbes->regs.xKEYOPMODE = (config->options & DLOP_DST_COLORKEY) ? 1 : 0;
}
