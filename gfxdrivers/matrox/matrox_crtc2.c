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
#include <errno.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <directfb.h>

#include <core/fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/gfxcard.h>
#include <core/fbdev/fbdev.h>
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/windows.h>

#include <misc/mem.h>
#include <misc/util.h>


#include "regs.h"
#include "mmio.h"
#include "matrox.h"
#include "matrox_maven.h"

typedef struct {
     DFBDisplayLayerConfig config;

     /* Stored registers */
     struct {
          /* CRTC2 */
          __u32 c2CTL;
          __u32 c2DATACTL;
          __u32 c2MISC;
          __u32 c2OFFSET;

          __u32 c2HPARAM;
          __u32 c2HSYNC;
          __u32 c2VPARAM;
          __u32 c2VSYNC;
          __u32 c2PRELOAD;

          __u32 c2STARTADD0;
          __u32 c2STARTADD1;
          __u32 c2PL2STARTADD0;
          __u32 c2PL2STARTADD1;
          __u32 c2PL3STARTADD0;
          __u32 c2PL3STARTADD1;

          __u32 c2SPICSTARTADD0;
          __u32 c2SPICSTARTADD1;
          __u32 c2SUBPICLUT;

          __u32 c2VCOUNT;
     } regs;

     struct maven_data md;
     struct mavenregs mr;
} MatroxCrtc2LayerData;

static void crtc2_wait_vsync( MatroxDriverData *mdrv );
static void crtc2_set_mafc( MatroxDriverData *mdrv, int on );
static void crtc2_set_regs( MatroxDriverData *mdrv, MatroxCrtc2LayerData *mcrtc2 );
static void crtc2_calc_regs( MatroxDriverData *mdrv, MatroxCrtc2LayerData *mcrtc2,
                             DisplayLayer *layer );
static void crtc2_set_buffer( MatroxDriverData *mdrv, MatroxCrtc2LayerData *mcrtc2,
                              DisplayLayer *layer );

#define CRTC2_SUPPORTED_OPTIONS   (DLOP_DEINTERLACING | DLOP_FLICKER_FILTERING)

/**********************/

static int
crtc2LayerDataSize()
{
     return sizeof(MatroxCrtc2LayerData);
}
     
static DFBResult
crtc2InitLayer( GraphicsDevice             *device,
                DisplayLayer               *layer,
                DisplayLayerInfo           *layer_info,
                DFBDisplayLayerConfig      *default_config,
                DFBColorAdjustment         *default_adj,
                void                       *driver_data,
                void                       *layer_data )
{
     /* set capabilities and type */
     layer_info->desc.caps = DLCAPS_SURFACE |
                             DLCAPS_DEINTERLACING | DLCAPS_FLICKER_FILTERING |
                             DLCAPS_BRIGHTNESS | DLCAPS_CONTRAST |
                             DLCAPS_HUE | DLCAPS_SATURATION;
     layer_info->desc.type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( layer_info->name,
               DFB_DISPLAY_LAYER_INFO_NAME_LENGTH, "Matrox CRTC2" );

     /* fill out the default configuration */
     default_config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                                   DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                                   DLCONF_OPTIONS;
     default_config->width       = 720;
     default_config->height      = dfb_config->matrox_ntsc ? 486 : 576;
     default_config->pixelformat = DSPF_YUY2;
     default_config->buffermode  = DLBM_FRONTONLY;
     default_config->options     = DLOP_NONE;

     /* fill out default color adjustment,
        only fields set in flags will be accepted from applications */
     default_adj->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST |
                               DCAF_HUE | DCAF_SATURATION;
     default_adj->brightness = 0xFFFF;
     default_adj->contrast   = 0xFFFF;
     default_adj->hue        = 0x0000;
     default_adj->saturation = dfb_config->matrox_ntsc ? 0x8E00 : 0x9500;

     return DFB_OK;
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


static DFBResult
crtc2Enable( DisplayLayer *layer,
             void         *driver_data,
             void         *layer_data )
{
     return DFB_OK;
}

static DFBResult
crtc2Disable( DisplayLayer *layer,
              void         *driver_data,
              void         *layer_data )
{
     MatroxDriverData     *mdrv   = (MatroxDriverData*) driver_data;
     MatroxCrtc2LayerData *mcrtc2 = (MatroxCrtc2LayerData*) layer_data;

     /* disable crtc2 */
     crtc2OnOff( mdrv, mcrtc2, 0 );

     crtc2_set_mafc( mdrv, 0 );

     return DFB_OK;
}

static DFBResult
crtc2TestConfiguration( DisplayLayer               *layer,
                        void                       *driver_data,
                        void                       *layer_data,
                        DFBDisplayLayerConfig      *config,
                        DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags  fail = 0;

     if (config->options & ~CRTC2_SUPPORTED_OPTIONS)
          fail |= DLCONF_OPTIONS;

     switch (config->pixelformat) {
          case DSPF_ARGB:
          case DSPF_RGB32:
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               break;
          default:
               fail |= DLCONF_PIXELFORMAT;
     }

     if (config->width != 720)
          fail |= DLCONF_WIDTH;

     if (config->height != (dfb_config->matrox_ntsc ? 486 : 576))
          fail |= DLCONF_HEIGHT;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
crtc2SetConfiguration( DisplayLayer          *layer,
                       void                  *driver_data,
                       void                  *layer_data,
                       DFBDisplayLayerConfig *config )
{
     MatroxDriverData     *mdrv   = (MatroxDriverData*) driver_data;
     MatroxCrtc2LayerData *mcrtc2 = (MatroxCrtc2LayerData*) layer_data;
     volatile __u8        *mmio   = mdrv->mmio_base;

     /* remember configuration */
     mcrtc2->config = *config;

     crtc2_set_mafc( mdrv, 1 );

     if (maven_open( &mcrtc2->md ) < 0)
          return errno2dfb( errno );

     maven_set_mode( &mcrtc2->md, dfb_config->matrox_ntsc ? MODE_NTSC : MODE_PAL );
     maven_compute( &mcrtc2->md, &mcrtc2->mr );
     crtc2_calc_regs( mdrv, mcrtc2, layer );

     crtc2OnOff( mdrv, mcrtc2, 0 );
     crtc2_set_regs( mdrv, mcrtc2 );
     crtc2_set_buffer( mdrv, mcrtc2, layer );
     crtc2OnOff( mdrv, mcrtc2, 1 );
     maven_program( &mcrtc2->md, &mcrtc2->mr );
     if (config->options & DLOP_FLICKER_FILTERING)
          maven_set_deflicker( &mcrtc2->md, 2 );

     while ((mga_in32( mmio, C2VCOUNT ) & 0x00000FFF) != 1)
          ;
     while ((mga_in32( mmio, C2VCOUNT ) & 0x00000FFF) != 0)
          ;
     /* c2interlace = 1 */
     mcrtc2->regs.c2CTL |= (1 << 25);
     mga_out32( mmio, mcrtc2->regs.c2CTL, C2CTL );
     mga_out32( mmio, mcrtc2->regs.c2PRELOAD, C2PRELOAD );

     maven_close( &mcrtc2->md );

     return DFB_OK;
}

static DFBResult
crtc2FlipBuffers( DisplayLayer        *layer,
                  void                *driver_data,
                  void                *layer_data,
                  DFBSurfaceFlipFlags  flags )
{
     MatroxDriverData     *mdrv    = (MatroxDriverData*) driver_data;
     MatroxCrtc2LayerData *mcrtc2  = (MatroxCrtc2LayerData*) layer_data;
     CoreSurface          *surface = dfb_layer_surface( layer );

     dfb_surface_flip_buffers( surface );

     crtc2_set_buffer( mdrv, mcrtc2, layer );

     if (flags & DSFLIP_WAITFORSYNC)
          crtc2_wait_vsync( mdrv );

     return DFB_OK;
}

static DFBResult
crtc2SetColorAdjustment( DisplayLayer       *layer,
                         void               *driver_data,
                         void               *layer_data,
                         DFBColorAdjustment *adj )
{
     MatroxCrtc2LayerData *mcrtc2  = (MatroxCrtc2LayerData*) layer_data;

     if (maven_open( &mcrtc2->md ) < 0)
          return errno2dfb( errno );

     if (adj->flags & DCAF_HUE)
          maven_set_hue( &mcrtc2->md,
                         adj->hue >> 8 );
     if (adj->flags & DCAF_SATURATION)
          maven_set_saturation( &mcrtc2->md,
                                adj->saturation >> 8 );
     if (adj->flags & DCAF_BRIGHTNESS ||
         adj->flags & DCAF_CONTRAST)
          maven_set_bwlevel( &mcrtc2->md,
                             adj->brightness >> 8,
                             adj->contrast >> 8 );

     maven_close( &mcrtc2->md );

     return DFB_OK;
}

static DFBResult
crtc2SetOpacity( DisplayLayer *layer,
                 void         *driver_data,
                 void         *layer_data,
                 __u8          opacity )
{
     MatroxDriverData     *mdrv   = (MatroxDriverData*) driver_data;
     MatroxCrtc2LayerData *mcrtc2 = (MatroxCrtc2LayerData*) layer_data;

     switch (opacity) {
          case 0:
               crtc2OnOff( mdrv, mcrtc2, 0 );
               crtc2_set_mafc( mdrv, 0 );
               break;
          case 0xFF:
               crtc2_set_mafc( mdrv, 1 );
               crtc2OnOff( mdrv, mcrtc2, 1 );
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

DisplayLayerFuncs matroxCrtc2Funcs = {
     LayerDataSize:      crtc2LayerDataSize,
     InitLayer:          crtc2InitLayer,
     Enable:             crtc2Enable,
     Disable:            crtc2Disable,
     TestConfiguration:  crtc2TestConfiguration,
     SetConfiguration:   crtc2SetConfiguration,
     FlipBuffers:        crtc2FlipBuffers,
     SetColorAdjustment: crtc2SetColorAdjustment,
     SetOpacity:         crtc2SetOpacity
};

/* internal */

static void crtc2_wait_vsync( MatroxDriverData *mdrv )
{
#ifdef FBIO_WAITFORVSYNC
     dfb_gfxcard_sync();
     ioctl( dfb_fbdev->fd, FBIO_WAITFORVSYNC, 1 );
#else
     int vdisplay = (dfb_config->matrox_ntsc ? 486/2 : 576/2) + 2;
     while ((mga_in32( mdrv->mmio_base, C2VCOUNT ) & 0x00000FFF) != vdisplay)
          ;
#endif
}

static void crtc2_set_mafc( MatroxDriverData     *mdrv,
                            int                   on )
{
     volatile __u8 *mmio = mdrv->mmio_base;
     uint8 val;

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

static void crtc2_set_regs( MatroxDriverData     *mdrv,
                            MatroxCrtc2LayerData *mcrtc2 )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     mga_out32( mmio, mcrtc2->regs.c2CTL,     C2CTL );
     mga_out32( mmio, mcrtc2->regs.c2DATACTL, C2DATACTL );
     mga_out32( mmio, mcrtc2->regs.c2HPARAM,  C2HPARAM );
     mga_out32( mmio, mcrtc2->regs.c2VPARAM,  C2VPARAM );
     mga_out32( mmio, mcrtc2->regs.c2HSYNC,   C2HSYNC );
     mga_out32( mmio, mcrtc2->regs.c2VSYNC,   C2VSYNC );
     mga_out32( mmio, mcrtc2->regs.c2MISC,    C2MISC );
}

static void crtc2_calc_regs( MatroxDriverData     *mdrv,
                             MatroxCrtc2LayerData *mcrtc2,
                             DisplayLayer         *layer )
{
     CoreSurface   *surface = dfb_layer_surface( layer );

     mcrtc2->regs.c2CTL = 0;
     mcrtc2->regs.c2DATACTL = 0;

     /* c2pixcksel = 01 (vdoclk) */
     mcrtc2->regs.c2CTL |= (1 << 1);

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
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_UYVY:
          case DSPF_YUY2:
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

          case DSPF_RGB15:
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
               BUG( "unexpected pixelformat" );
               return;
     }

     {
          int hdisplay, htotal, vdisplay, vtotal;

          if (dfb_config->matrox_ntsc) {
               hdisplay = 720;
               htotal = 858;
               vdisplay = 486 / 2;
               vtotal = 525 / 2;
          } else {
               hdisplay = 720;
               htotal = 864;
               vdisplay = 576 / 2;
               vtotal = 625 / 2;
          }

          mcrtc2->regs.c2HPARAM = ((hdisplay - 8) << 16) | (htotal - 8);
          mcrtc2->regs.c2VPARAM = ((vdisplay - 1) << 16) | (vtotal - 1);

          /* Ignored in YUV mode */
          mcrtc2->regs.c2HSYNC = 0;
          mcrtc2->regs.c2VSYNC = 0;

          mcrtc2->regs.c2PRELOAD = 0;

          mcrtc2->regs.c2MISC = 0;
          /* c2vlinecomp */
          mcrtc2->regs.c2MISC |= ((vdisplay + 2) << 16);
     }
}

static void crtc2_set_buffer( MatroxDriverData     *mdrv,
                              MatroxCrtc2LayerData *mcrtc2,
                              DisplayLayer         *layer )
{
     CoreSurface   *surface      = dfb_layer_surface( layer );
     SurfaceBuffer *front_buffer = surface->front_buffer;
     volatile __u8 *mmio         = mdrv->mmio_base;

     /* interleaved fields */
     mcrtc2->regs.c2OFFSET = front_buffer->video.pitch * 2;

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

     mga_out32( mmio, mcrtc2->regs.c2OFFSET, C2OFFSET );
     mga_out32( mmio, mcrtc2->regs.c2STARTADD1, C2STARTADD1 );
     mga_out32( mmio, mcrtc2->regs.c2STARTADD0, C2STARTADD0 );
     mga_out32( mmio, mcrtc2->regs.c2PL2STARTADD1, C2PL2STARTADD1 );
     mga_out32( mmio, mcrtc2->regs.c2PL2STARTADD0, C2PL2STARTADD0 );
     mga_out32( mmio, mcrtc2->regs.c2PL3STARTADD1, C2PL3STARTADD1 );
     mga_out32( mmio, mcrtc2->regs.c2PL3STARTADD0, C2PL3STARTADD0 );
}
