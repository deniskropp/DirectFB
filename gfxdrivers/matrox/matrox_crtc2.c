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

#include <directfb.h>

#include <fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/gfxcard.h>
#include <core/fbdev/fbdev.h>
#include <core/layers.h>
#include <core/screen.h>
#include <core/surfaces.h>
#include <core/windows.h>

#include <direct/mem.h>

#include <misc/conf.h>
#include <misc/util.h>


#include "regs.h"
#include "mmio.h"
#include "matrox.h"
#include "matrox_maven.h"

typedef struct {
     CoreLayerRegionConfig config;
     DFBColorAdjustment    adj;
     int                   field;

     /* Stored registers */
     struct {
          /* CRTC2 */
          __u32 c2CTL;
          __u32 c2DATACTL;
          __u32 c2MISC;
          __u32 c2OFFSET;

          __u32 c2HPARAM;
          __u32 c2VPARAM;

          __u32 c2STARTADD0;
          __u32 c2STARTADD1;
          __u32 c2PL2STARTADD0;
          __u32 c2PL2STARTADD1;
          __u32 c2PL3STARTADD0;
          __u32 c2PL3STARTADD1;
     } regs;

     MatroxMavenData mav;
} MatroxCrtc2LayerData;

static void crtc2_set_regs           ( MatroxDriverData      *mdrv,
                                       MatroxCrtc2LayerData  *mcrtc2 );

static void crtc2_calc_regs          ( MatroxDriverData      *mdrv,
                                       MatroxCrtc2LayerData  *mcrtc2,
                                       CoreLayerRegionConfig *config,
                                       CoreSurface           *surface );

static void crtc2_calc_buffer        ( MatroxDriverData      *mdrv,
                                       MatroxCrtc2LayerData  *mcrtc2,
                                       CoreSurface           *surface );

static void crtc2_set_buffer         ( MatroxDriverData      *mdrv,
                                       MatroxCrtc2LayerData  *mcrtc2 );

static DFBResult crtc2_disable_output( MatroxDriverData      *mdrv,
                                       MatroxCrtc2LayerData  *mcrtc2 );

static DFBResult crtc2_enable_output ( MatroxDriverData      *mdrv,
                                       MatroxCrtc2LayerData  *mcrtc2 );

#define CRTC2_SUPPORTED_OPTIONS   (DLOP_FIELD_PARITY | DLOP_FLICKER_FILTERING)

/**********************/

static int
crtc2LayerDataSize()
{
     return sizeof(MatroxCrtc2LayerData);
}

static DFBResult
crtc2InitLayer( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                DFBDisplayLayerDescription *description,
                DFBDisplayLayerConfig      *config,
                DFBColorAdjustment         *adjustment )
{
     MatroxDriverData     *mdrv   = (MatroxDriverData*) driver_data;
     MatroxCrtc2LayerData *mcrtc2 = (MatroxCrtc2LayerData*) layer_data;
     MatroxDeviceData     *mdev   = mdrv->device_data;
     MatroxMavenData      *mav    = &mcrtc2->mav;
     bool                  ntsc   = dfb_config->matrox_ntsc;
     DFBResult             res;

     if ((res = maven_init( mav, mdrv )) != DFB_OK)
          return res;


     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE |
                         DLCAPS_FIELD_PARITY | DLCAPS_FLICKER_FILTERING |
                         DLCAPS_BRIGHTNESS | DLCAPS_CONTRAST |
                         DLCAPS_HUE | DLCAPS_SATURATION;
     description->type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Matrox CRTC2 Layer" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;
     config->width       = 720;
     config->height      = ntsc ? 480 : 576;
     config->pixelformat = DSPF_YUY2;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;

     /* fill out default color adjustment,
        only fields set in flags will be accepted from applications */
     adjustment->flags = DCAF_BRIGHTNESS | DCAF_CONTRAST |
                         DCAF_HUE | DCAF_SATURATION;
     if (mdev->g450_matrox) {
          adjustment->brightness = ntsc ? 0xAA00 : 0x9E00;
          adjustment->saturation = ntsc ? 0xAE00 : 0xBB00;
     } else {
          adjustment->brightness = ntsc ? 0xB500 : 0xA800;
          adjustment->saturation = ntsc ? 0x8E00 : 0x9500;
     }
     adjustment->contrast = ntsc ? 0xEA00 : 0xFF00;
     adjustment->hue      = 0x0000;

     /* remember color adjustment */
     mcrtc2->adj = *adjustment;

     return DFB_OK;
}

static DFBResult
crtc2TestRegion( CoreLayer                  *layer,
                 void                       *driver_data,
                 void                       *layer_data,
                 CoreLayerRegionConfig      *config,
                 CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     if (config->options & ~CRTC2_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     switch (config->format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               break;
          default:
               fail |= CLRCF_FORMAT;
     }

     if (config->width != 720)
          fail |= CLRCF_WIDTH;

     if (config->height != (dfb_config->matrox_ntsc ? 480 : 576))
          fail |= CLRCF_HEIGHT;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
crtc2AddRegion( CoreLayer             *layer,
                void                  *driver_data,
                void                  *layer_data,
                void                  *region_data,
                CoreLayerRegionConfig *config )
{
     return DFB_OK;
}

static DFBResult
crtc2SetRegion( CoreLayer                  *layer,
                void                       *driver_data,
                void                       *layer_data,
                void                       *region_data,
                CoreLayerRegionConfig      *config,
                CoreLayerRegionConfigFlags  updated,
                CoreSurface                *surface,
                CorePalette                *palette )
{
     DFBResult             ret;
     MatroxDriverData     *mdrv   = (MatroxDriverData*) driver_data;
     MatroxCrtc2LayerData *mcrtc2 = (MatroxCrtc2LayerData*) layer_data;

     /* remember configuration */
     mcrtc2->config = *config;

     crtc2_calc_regs( mdrv, mcrtc2, config, surface );
     crtc2_calc_buffer( mdrv, mcrtc2, surface );

     ret = crtc2_enable_output( mdrv, mcrtc2 );
     if (ret)
          return ret;

     return DFB_OK;
}

static DFBResult
crtc2RemoveRegion( CoreLayer *layer,
                   void      *driver_data,
                   void      *layer_data,
                   void      *region_data )
{
     MatroxDriverData     *mdrv   = (MatroxDriverData*) driver_data;
     MatroxCrtc2LayerData *mcrtc2 = (MatroxCrtc2LayerData*) layer_data;

     crtc2_disable_output( mdrv, mcrtc2 );

     return DFB_OK;
}

static DFBResult
crtc2FlipRegion( CoreLayer           *layer,
                 void                *driver_data,
                 void                *layer_data,
                 void                *region_data,
                 CoreSurface         *surface,
                 DFBSurfaceFlipFlags  flags )
{
     MatroxDriverData     *mdrv    = (MatroxDriverData*) driver_data;
     MatroxCrtc2LayerData *mcrtc2  = (MatroxCrtc2LayerData*) layer_data;
     volatile __u8        *mmio    = mdrv->mmio_base;

     int                   vdisplay = (dfb_config->matrox_ntsc ? 480/2 : 576/2) + 2;
     int                   line;

     dfb_surface_flip_buffers( surface );
     crtc2_calc_buffer( mdrv, mcrtc2, surface );

     line = mga_in32( mmio, C2VCOUNT ) & 0x00000FFF;
     if (line + 6 > vdisplay && line < vdisplay)
          while ((int)(mga_in32( mmio, C2VCOUNT ) & 0x00000FFF) != vdisplay)
               ;

     if (mcrtc2->config.options & DLOP_FIELD_PARITY) {
          int field = (mga_in32( mmio, C2VCOUNT ) >> 24) & 0x1;

          while (field == mcrtc2->field) {
               dfb_screen_wait_vsync( mdrv->secondary );

               field = (mga_in32( mmio, C2VCOUNT ) >> 24) & 0x1;
          }
     }
     crtc2_set_buffer( mdrv, mcrtc2 );

     if (flags & DSFLIP_WAIT)
          dfb_screen_wait_vsync( mdrv->secondary );

     return DFB_OK;
}

static DFBResult
crtc2SetColorAdjustment( CoreLayer          *layer,
                         void               *driver_data,
                         void               *layer_data,
                         DFBColorAdjustment *adj )
{
     MatroxDriverData     *mdrv   = (MatroxDriverData*) driver_data;
     MatroxCrtc2LayerData *mcrtc2 = (MatroxCrtc2LayerData*) layer_data;
     MatroxMavenData      *mav    = &mcrtc2->mav;
     DFBResult             res;

     if ((res = maven_open( mav, mdrv )) != DFB_OK)
          return res;

     if (adj->flags & DCAF_HUE)
          maven_set_hue( mav, mdrv,
                         adj->hue >> 8 );
     if (adj->flags & DCAF_SATURATION)
          maven_set_saturation( mav, mdrv,
                                adj->saturation >> 8 );
     if (adj->flags & DCAF_BRIGHTNESS ||
         adj->flags & DCAF_CONTRAST)
          maven_set_bwlevel( mav, mdrv,
                             adj->brightness >> 8,
                             adj->contrast >> 8 );

     maven_close( mav, mdrv );

     /* remember color adjustment */
     mcrtc2->adj = *adj;

     return DFB_OK;
}

static DFBResult
crtc2GetCurrentOutputField( CoreLayer *layer,
                            void      *driver_data,
                            void      *layer_data,
                            int       *field )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) driver_data;

     if (!field)
          return DFB_INVARG;

     *field = (mga_in32( mdrv->mmio_base, C2VCOUNT ) >> 24) & 0x1;

     return DFB_OK;
}

#if 0
static DFBResult
crtc2SetFieldParity( CoreLayer *layer,
                     void      *driver_data,
                     void      *layer_data,
                     int        field )
{
     MatroxCrtc2LayerData *mcrtc2 = (MatroxCrtc2LayerData*) layer_data;

     mcrtc2->field = !field;

     return DFB_OK;
}
#endif

DisplayLayerFuncs matroxCrtc2Funcs = {
     LayerDataSize:         crtc2LayerDataSize,
     InitLayer:             crtc2InitLayer,

     TestRegion:            crtc2TestRegion,
     AddRegion:             crtc2AddRegion,
     SetRegion:             crtc2SetRegion,
     RemoveRegion:          crtc2RemoveRegion,
     FlipRegion:            crtc2FlipRegion,

     SetColorAdjustment:    crtc2SetColorAdjustment,
     GetCurrentOutputField: crtc2GetCurrentOutputField
};

/* internal */

static void crtc2_set_regs( MatroxDriverData     *mdrv,
                            MatroxCrtc2LayerData *mcrtc2 )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     mga_out32( mmio, mcrtc2->regs.c2CTL,     C2CTL );
     mga_out32( mmio, mcrtc2->regs.c2DATACTL, C2DATACTL );
     mga_out32( mmio, mcrtc2->regs.c2HPARAM,  C2HPARAM );
     mga_out32( mmio, 0,                      C2HSYNC );
     mga_out32( mmio, mcrtc2->regs.c2VPARAM,  C2VPARAM );
     mga_out32( mmio, 0,                      C2VSYNC );
     mga_out32( mmio, mcrtc2->regs.c2OFFSET,  C2OFFSET );
     mga_out32( mmio, mcrtc2->regs.c2MISC,    C2MISC );
     mga_out32( mmio, 0,                      C2PRELOAD );
}

static void crtc2_calc_regs( MatroxDriverData      *mdrv,
                             MatroxCrtc2LayerData  *mcrtc2,
                             CoreLayerRegionConfig *config,
                             CoreSurface           *surface )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     mcrtc2->regs.c2CTL = 0;

     mcrtc2->regs.c2DATACTL  = mga_in32( mdrv->mmio_base, C2DATACTL );
     mcrtc2->regs.c2DATACTL &= ~0x00000097;

     /* c2pixcksel = 01 (vdoclk) */
     mcrtc2->regs.c2CTL |= (1 << 1);
     if (mdev->g450_matrox)
          mcrtc2->regs.c2CTL |= (1 << 14);

     /*
      * High priority request level.
      * According to G400 specs these values should
      * be fixed when CRTC2 is in YUV mode.
      */
     /* c2hiprilvl = 010 */
     mcrtc2->regs.c2CTL |= (2 << 4);
     /* c2maxhipri = 001 */
     mcrtc2->regs.c2CTL |= (1 << 8);

     /* c2vidrstmod = 01 */
     mcrtc2->regs.c2CTL |= (1 << 28);
     /* c2hploaden = 1 */
     mcrtc2->regs.c2CTL |= (1 << 30);
     /* c2vploaden = 1 */
     mcrtc2->regs.c2CTL |= (1 << 31);

     switch (surface->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               /* c2dithen = 1 */
               mcrtc2->regs.c2DATACTL |= (1 << 0);
               /* c2yfiltend = 1 */
               mcrtc2->regs.c2DATACTL |= (1 << 1);
               /* c2cbcrfilten = 1 */
               mcrtc2->regs.c2DATACTL |= (1 << 2);
               break;
          default:
               break;
     }

     if (dfb_config->matrox_ntsc)
          /* c2ntscen = 1 */
          mcrtc2->regs.c2DATACTL |= (1 << 4);

     /* pixel format settings */
     switch (surface->format) {
          case DSPF_I420:
          case DSPF_YV12:
               /* c2depth = 111 */
               mcrtc2->regs.c2CTL |= (7 << 21);
               break;

          case DSPF_UYVY:
               /* c2uyvyfmt = 1 */
               mcrtc2->regs.c2DATACTL |= (1 << 7);
               /* fall through */

          case DSPF_YUY2:
               /* c2depth = 101 */
               mcrtc2->regs.c2CTL |= (5 << 21);
               break;

          case DSPF_ARGB1555:
               /* c2depth = 001 */
               mcrtc2->regs.c2CTL |= (1 << 21);
               break;

          case DSPF_RGB16:
               /* c2depth = 010 */
               mcrtc2->regs.c2CTL |= (2 << 21);
               break;

          case DSPF_RGB32:
          case DSPF_ARGB:
               /* c2depth = 100 */
               mcrtc2->regs.c2CTL |= (4 << 21);
               break;

          default:
               D_BUG( "unexpected pixelformat" );
               return;
     }

     /* interleaved fields */
     mcrtc2->regs.c2OFFSET = surface->front_buffer->video.pitch * 2;

     {
          int hdisplay, htotal, vdisplay, vtotal;

          if (dfb_config->matrox_ntsc) {
               hdisplay = 720;
               htotal = 858;
               vdisplay = 480 / 2;
               vtotal = 525 / 2;
          } else {
               hdisplay = 720;
               htotal = 864;
               vdisplay = 576 / 2;
               vtotal = 625 / 2;
          }

          mcrtc2->regs.c2HPARAM = ((hdisplay - 8) << 16) | (htotal - 8);
          mcrtc2->regs.c2VPARAM = ((vdisplay - 1) << 16) | (vtotal - 1);

          mcrtc2->regs.c2MISC = 0;
          /* c2vlinecomp */
          mcrtc2->regs.c2MISC |= ((vdisplay + 2) << 16);
     }
}

static void crtc2_calc_buffer( MatroxDriverData     *mdrv,
                               MatroxCrtc2LayerData *mcrtc2,
                               CoreSurface          *surface )
{
     SurfaceBuffer *front_buffer = surface->front_buffer;

     mcrtc2->regs.c2STARTADD1 = front_buffer->video.offset;
     mcrtc2->regs.c2STARTADD0 = front_buffer->video.offset + front_buffer->video.pitch;

     switch (surface->format) {
          case DSPF_I420:
               mcrtc2->regs.c2PL2STARTADD1 = mcrtc2->regs.c2STARTADD1 + surface->height * front_buffer->video.pitch;
               mcrtc2->regs.c2PL2STARTADD0 = mcrtc2->regs.c2PL2STARTADD1 + front_buffer->video.pitch/2;

               mcrtc2->regs.c2PL3STARTADD1 = mcrtc2->regs.c2PL2STARTADD1 + surface->height/2 * front_buffer->video.pitch/2;
               mcrtc2->regs.c2PL3STARTADD0 = mcrtc2->regs.c2PL3STARTADD1 + front_buffer->video.pitch/2;
               break;
          case DSPF_YV12:
               mcrtc2->regs.c2PL3STARTADD1 = mcrtc2->regs.c2STARTADD1 + surface->height * front_buffer->video.pitch;
               mcrtc2->regs.c2PL3STARTADD0 = mcrtc2->regs.c2PL3STARTADD1 + front_buffer->video.pitch/2;

               mcrtc2->regs.c2PL2STARTADD1 = mcrtc2->regs.c2PL3STARTADD1 + surface->height/2 *  front_buffer->video.pitch/2;
               mcrtc2->regs.c2PL2STARTADD0 = mcrtc2->regs.c2PL2STARTADD1 + front_buffer->video.pitch/2;
               break;
          default:
               break;
     }
}

static void crtc2_set_buffer( MatroxDriverData     *mdrv,
                              MatroxCrtc2LayerData *mcrtc2 )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     mga_out32( mmio, mcrtc2->regs.c2STARTADD1, C2STARTADD1 );
     mga_out32( mmio, mcrtc2->regs.c2STARTADD0, C2STARTADD0 );
     mga_out32( mmio, mcrtc2->regs.c2PL2STARTADD1, C2PL2STARTADD1 );
     mga_out32( mmio, mcrtc2->regs.c2PL2STARTADD0, C2PL2STARTADD0 );
     mga_out32( mmio, mcrtc2->regs.c2PL3STARTADD1, C2PL3STARTADD1 );
     mga_out32( mmio, mcrtc2->regs.c2PL3STARTADD0, C2PL3STARTADD0 );
}

static void
crtc2OnOff( MatroxDriverData     *mdrv,
            MatroxCrtc2LayerData *mcrtc2,
            int                   on )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     if (on)
          /* c2en = 1 */
          mcrtc2->regs.c2CTL |= 1;
     else
          /* c2en = 0 */
          mcrtc2->regs.c2CTL &= ~1;
     mga_out32( mmio, mcrtc2->regs.c2CTL, C2CTL );

     if (on)
          /* c2pixclkdis = 0 */
          mcrtc2->regs.c2CTL &= ~8;
     else
          /* c2pixclkdis = 1 */
          mcrtc2->regs.c2CTL |= 8;
     mga_out32( mmio, mcrtc2->regs.c2CTL, C2CTL );

     if (!on) {
          /* c2interlace = 0 */
          mcrtc2->regs.c2CTL &= ~(1 << 25);
          mga_out32( mmio, mcrtc2->regs.c2CTL, C2CTL );
     }
}

static void crtc2_set_mafc( MatroxDriverData     *mdrv,
                            int                   on )
{
     volatile __u8 *mmio = mdrv->mmio_base;
     __u8           val;

     val = mga_in_dac( mmio, XMISCCTRL );
     if (on) {
          /*
           * mfcsel   = 01  (MAFC)
           * vdoutsel = 110 (CRTC2 ITU-R 656)
           */
          val &= ~((3 << 1) | (7 << 5));
          val |=   (1 << 1) | (6 << 5);
     } else {
          /*
           * mfcsel   = 11  (Disable)
           */
          val |= (3 << 1);
     }
     mga_out_dac( mmio, XMISCCTRL, val );
}

static DFBResult
crtc2_disable_output( MatroxDriverData     *mdrv,
                      MatroxCrtc2LayerData *mcrtc2 )
{
     MatroxDeviceData *mdev = mdrv->device_data;
     MatroxMavenData  *mav  = &mcrtc2->mav;
     DFBResult         res;

     if ((res = maven_open( mav, mdrv )) != DFB_OK)
          return res;

     maven_disable( mav, mdrv );
     if (!mdev->g450_matrox)
          crtc2_set_mafc( mdrv, 0 );
     crtc2OnOff( mdrv, mcrtc2, 0 );

     maven_close( mav, mdrv );

     if (mdev->g450_matrox) {
          volatile __u8 *mmio = mdrv->mmio_base;
          __u8           val;

          /*
           * dac2pdn = 0
           * cfifopdn = 0
           */
          val = mga_in_dac( mmio, XPWRCTRL );
          val &= ~((1 << 4) | (1 << 0));
          mga_out_dac( mmio, XPWRCTRL, val );

          /* dac2outsel = 00 (disable) */
          val = mga_in_dac( mmio, XDISPCTRL );
          val &= ~(3 << 2);
          mga_out_dac( mmio, XDISPCTRL, val );
     }

     return DFB_OK;
}

static DFBResult
crtc2_enable_output( MatroxDriverData      *mdrv,
                     MatroxCrtc2LayerData  *mcrtc2 )
{
     MatroxDeviceData *mdev = mdrv->device_data;
     MatroxMavenData  *mav  = &mcrtc2->mav;
     volatile __u8    *mmio = mdrv->mmio_base;
     DFBResult         res;

     if ((res = maven_open( mav, mdrv )) != DFB_OK)
          return res;

     if (mdev->g450_matrox) {
          volatile __u8 *mmio = mdrv->mmio_base;
          __u8           val;

          val = mga_in_dac( mmio, XGENIOCTRL );
          val |= 0x40;
          mga_out_dac( mmio, XGENIOCTRL, val );
          val = mga_in_dac( mmio, XGENIODATA );
          val &= ~0x40;
          mga_out_dac( mmio, XGENIODATA, val );

          /*
           * dac2pdn = 1
           * cfifopdn = 1
           */
          val = mga_in_dac( mmio, XPWRCTRL );
          val |= (1 << 4) | (1 << 0);
          mga_out_dac( mmio, XPWRCTRL, val );

          /* dac2outsel = 11 (TVE) */
          val = mga_in_dac( mmio, XDISPCTRL );
          val |= (3 << 2);
          mga_out_dac( mmio, XDISPCTRL, val );

          if (dfb_config->matrox_cable == 1) {
               /*
                * dac2hsoff = 0
                * dac2vsoff = 0
                * dac2hspol = 0 (+)
                * dac2vspol = 0 (+)
                */
               val = mga_in_dac( mmio, XSYNCCTRL );
               val &= 0x0F;
               mga_out_dac( mmio, XSYNCCTRL, val );
          }
     }

     maven_disable( mav, mdrv );
     if (!mdev->g450_matrox)
          crtc2_set_mafc( mdrv, 0 );
     crtc2OnOff( mdrv, mcrtc2, 0 );

     crtc2_set_regs( mdrv, mcrtc2 );
     crtc2_set_buffer( mdrv, mcrtc2 );

     if (!mdev->g450_matrox)
          crtc2_set_mafc( mdrv, 1 );
     crtc2OnOff( mdrv, mcrtc2, 1 );

     maven_set_regs( mav, mdrv, &mcrtc2->config, &mcrtc2->adj );

     /* c2interlace = 1 */
     mcrtc2->regs.c2CTL |= (1 << 25);
     if (mdev->g450_matrox)
          mcrtc2->regs.c2CTL |= (1 << 12);
     while ((mga_in32( mmio, C2VCOUNT ) & 0x00000FFF) != 1)
          ;
     while ((mga_in32( mmio, C2VCOUNT ) & 0x00000FFF) != 0)
          ;
     mga_out32( mmio, mcrtc2->regs.c2CTL, C2CTL );

     maven_enable( mav, mdrv );

     if (!mdev->g450_matrox) {
          while ((mga_in32( mmio, C2VCOUNT ) & 0x00000FFF) != 1)
               ;
          while ((mga_in32( mmio, C2VCOUNT ) & 0x00000FFF) != 0)
               ;
          maven_sync( mav, mdrv );
     }

     maven_close( mav, mdrv );

     return DFB_OK;
}
