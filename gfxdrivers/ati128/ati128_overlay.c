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
#include "ati128.h"

static void ov0_set_regs( ATI128DriverData *adrv, ATI128DeviceData *adev );
static void ov0_calc_regs( ATI128DriverData *adrv, ATI128DeviceData *adev,
                           DisplayLayer *layer );

/* FIXME: no driver globals */
static ATI128DriverData *adrv = NULL;
static ATI128DeviceData *adev = NULL;

#define OV0_SUPPORTED_OPTIONS   (0)


/**********************/

static DFBResult ov0Enable( DisplayLayer *layer )
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

     ov0_calc_regs( adrv, adev, layer );
     ov0_set_regs( adrv, adev );

     return DFB_OK;
}

static DFBResult ov0Disable( DisplayLayer *layer )
{
     /* FIXME: The surface should be destroyed, and the window stack? */
     layer->shared->enabled = 0;

     ati128_out32( adrv->mmio_base, OV0_SCALE_CNTL, 0 );

     return DFB_OK;
}

static DFBResult ov0TestConfiguration( DisplayLayer               *layer,
                                       DFBDisplayLayerConfig      *config,
                                       DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags fail = 0;

     if (config->flags & DLCONF_OPTIONS &&
         config->options & ~OV0_SUPPORTED_OPTIONS)
          fail |= DLCONF_OPTIONS;

     if (config->flags & DLCONF_PIXELFORMAT) {
          switch (config->pixelformat) {
               case DSPF_YUY2:
               case DSPF_UYVY:
               case DSPF_I420:
               case DSPF_YV12:
                    break;

               default:
                    fail |= DLCONF_PIXELFORMAT;
          }
     }

     if (config->flags & DLCONF_WIDTH)
          if (config->width > 2048 || config->width < 1)
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

static DFBResult ov0SetConfiguration( DisplayLayer          *layer,
                                      DFBDisplayLayerConfig *config )
{
     ov0_calc_regs( adrv, adev, layer );
     ov0_set_regs( adrv, adev );

     return DFB_OK;
}

static DFBResult ov0SetOpacity( DisplayLayer *layer,
                                __u8          opacity )
{
     switch (opacity) {
          case 0:
               ati128_out32( adrv->mmio_base, OV0_SCALE_CNTL,
                             adev->ov0.SCALE_CNTL & 0xBFFFFFFF );
               break;
          case 0xFF:
               ati128_out32( adrv->mmio_base, OV0_SCALE_CNTL,
                             adev->ov0.SCALE_CNTL );
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult ov0SetScreenLocation( DisplayLayer *layer,
                                       float         x,
                                       float         y,
                                       float         width,
                                       float         height )
{
     layer->shared->screen.x = x;
     layer->shared->screen.y = y;
     layer->shared->screen.w = width;
     layer->shared->screen.h = height;

     ov0_calc_regs( adrv, adev, layer );
     ov0_set_regs( adrv, adev );

     return DFB_OK;
}

static DFBResult ov0SetSrcColorKey( DisplayLayer *layer,
                                    __u32         key )
{
     return DFB_UNSUPPORTED;
}

static DFBResult ov0SetDstColorKey( DisplayLayer *layer,
                                    __u8          r,
                                    __u8          g,
                                    __u8          b )
{
     return DFB_UNSUPPORTED;
}

static DFBResult ov0FlipBuffers( DisplayLayer *layer )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult ov0SetColorAdjustment( DisplayLayer       *layer,
                                        DFBColorAdjustment *adj )
{
     return DFB_UNIMPLEMENTED;
}

static void ati128_ov0_deinit( DisplayLayer *layer )
{
     ati128_out32( adrv->mmio_base, OV0_SCALE_CNTL, 0 );
}

/* exported symbols */

void
ati128_init_layers( void *drv, void *dev )
{
     DisplayLayer     *layer;
     volatile __u8    *mmio;

     /* FIXME: no driver globals */
     adrv = (ATI128DriverData*) drv;
     adev = (ATI128DeviceData*) dev;

     mmio = adrv->mmio_base;


     layer = (DisplayLayer*)DFBCALLOC( 1, sizeof(DisplayLayer) );

     layer->shared = (DisplayLayerShared*) shcalloc( 1, sizeof(DisplayLayerShared) );

     layer->shared->caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE;

     snprintf( layer->shared->description,
               DFB_DISPLAY_LAYER_INFO_NAME_LENGTH, "ATI128 Overlay" );

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

     layer->Enable             = ov0Enable;
     layer->Disable            = ov0Disable;
     layer->TestConfiguration  = ov0TestConfiguration;
     layer->SetConfiguration   = ov0SetConfiguration;
     layer->SetOpacity         = ov0SetOpacity;
     layer->SetScreenLocation  = ov0SetScreenLocation;
     layer->SetSrcColorKey     = ov0SetSrcColorKey;
     layer->SetDstColorKey     = ov0SetDstColorKey;
     layer->FlipBuffers        = ov0FlipBuffers;
     layer->SetColorAdjustment = ov0SetColorAdjustment;

     layer->deinit             = ati128_ov0_deinit;

     /* gets filled by layers_add: layer->shared->id */
     dfb_layers_add( layer );

     /* reset overlay */
     ati128_out32( mmio, OV0_SCALE_CNTL, 0x80000000 );
     ati128_out32( mmio, OV0_EXCLUSIVE_HORZ, 0 );
     ati128_out32( mmio, OV0_AUTO_FLIP_CNTL, 0 );
     ati128_out32( mmio, OV0_FILTER_CNTL, 0x0000000f );
     ati128_out32( mmio, OV0_COLOR_CNTL, 0x00101000 );
     ati128_out32( mmio, OV0_KEY_CNTL, 0x10 );
     ati128_out32( mmio, OV0_TEST, 0 );
}



/* internal */

static void ov0_set_regs( ATI128DriverData *adrv, ATI128DeviceData *adev )
{
     volatile __u8 *mmio = adrv->mmio_base;

     ati128_out32( mmio, OV0_REG_LOAD_CNTL, 1 );
     while (!(ati128_in32( mmio, OV0_REG_LOAD_CNTL ) & (1 << 3)));

     ati128_out32( mmio, OV0_H_INC,
                   adev->ov0.H_INC );

     ati128_out32( mmio, OV0_STEP_BY,
                   adev->ov0.STEP_BY );

     ati128_out32( mmio, OV0_Y_X_START,
                   adev->ov0.Y_X_START );

     ati128_out32( mmio, OV0_Y_X_END,
                   adev->ov0.Y_X_END );

     ati128_out32( mmio, OV0_V_INC,
                   adev->ov0.V_INC );

     ati128_out32( mmio, OV0_P1_BLANK_LINES_AT_TOP,
                   adev->ov0.P1_BLANK_LINES_AT_TOP );

     ati128_out32( mmio, OV0_P23_BLANK_LINES_AT_TOP,
                   adev->ov0.P23_BLANK_LINES_AT_TOP );

     ati128_out32( mmio, OV0_VID_BUF_PITCH0_VALUE,
                   adev->ov0.VID_BUF_PITCH0_VALUE );

     ati128_out32( mmio, OV0_VID_BUF_PITCH1_VALUE,
                   adev->ov0.VID_BUF_PITCH1_VALUE );

     ati128_out32( mmio, OV0_P1_X_START_END,
                   adev->ov0.P1_X_START_END );

     ati128_out32( mmio, OV0_P2_X_START_END,
                   adev->ov0.P2_X_START_END );

     ati128_out32( mmio, OV0_P3_X_START_END,
                   adev->ov0.P3_X_START_END );

     ati128_out32( mmio, OV0_VID_BUF0_BASE_ADRS,
                   adev->ov0.VID_BUF0_BASE_ADRS );

     ati128_out32( mmio, OV0_VID_BUF1_BASE_ADRS,
                   adev->ov0.VID_BUF1_BASE_ADRS );

     ati128_out32( mmio, OV0_VID_BUF2_BASE_ADRS,
                   adev->ov0.VID_BUF2_BASE_ADRS );

     ati128_out32( mmio, OV0_P1_V_ACCUM_INIT,
                   adev->ov0.P1_V_ACCUM_INIT );

     ati128_out32( mmio, OV0_P23_V_ACCUM_INIT,
                   adev->ov0.P23_V_ACCUM_INIT );

     ati128_out32( mmio, OV0_P1_H_ACCUM_INIT,
                   adev->ov0.P1_H_ACCUM_INIT );

     ati128_out32( mmio, OV0_P23_H_ACCUM_INIT,
                   adev->ov0.P23_H_ACCUM_INIT );

     ati128_out32( mmio, OV0_SCALE_CNTL,
                   adev->ov0.SCALE_CNTL );

     ati128_out32( mmio, OV0_REG_LOAD_CNTL, 0 );
}

static void ov0_calc_regs( ATI128DriverData *adrv, ATI128DeviceData *adev,
                           DisplayLayer *layer )
{
     int h_inc, v_inc, step_by, tmp;
     int p1_h_accum_init, p23_h_accum_init;
     int p1_v_accum_init, p23_v_accum_init;

     DFBRegion      dstBox;
     int            dst_w;
     int            dst_h;
     __u32          offset_u = 0, offset_v = 0;
     CoreSurface   *surface = layer->shared->surface;
     SurfaceBuffer *front = surface->front_buffer;


     /* calculate destination size */
     dst_w = (int)(layer->shared->screen.w * (float)Sfbdev->current_mode->xres + 0.5f);
     dst_h = (int)(layer->shared->screen.h * (float)Sfbdev->current_mode->yres + 0.5f);

     /* calculate destination region */
     dstBox.x1 = (int)(layer->shared->screen.x * (float)Sfbdev->current_mode->xres + 0.5f);
     dstBox.y1 = (int)(layer->shared->screen.y * (float)Sfbdev->current_mode->yres + 0.5f);
     dstBox.x2 = (int)((layer->shared->screen.x + layer->shared->screen.w) * (float)Sfbdev->current_mode->xres + 0.5f);
     dstBox.y2 = (int)((layer->shared->screen.y + layer->shared->screen.h) * (float)Sfbdev->current_mode->yres + 0.5f);

     /* calculate incrementors */
     h_inc   = (surface->width  << 12) / dst_w;
     v_inc   = (surface->height << 20) / dst_h;
     step_by = 1;

     while (h_inc >= (2 << 12)) {
          step_by++;
          h_inc >>= 1;
     }

     /* calculate values for horizontal accumulators */
     tmp = 0x00028000 + (h_inc << 3);
     p1_h_accum_init = ((tmp <<  4) & 0x000f8000) | ((tmp << 12) & 0xf0000000);

     tmp = 0x00028000 + (h_inc << 2);
     p23_h_accum_init = ((tmp <<  4) & 0x000f8000) | ((tmp << 12) & 0x70000000);

     /* calculate values for vertical accumulators */
     tmp = 0x00018000;
     p1_v_accum_init = ((tmp << 4) & 0x03ff8000) | 0x00000001;

     tmp = 0x00018000;
     p23_v_accum_init = ((tmp << 4) & 0x01ff8000) | 0x00000001;

     /* choose pixel format and calculate buffer offsets for planar modes */
     switch (surface->format) {
          case DSPF_UYVY:
               adev->ov0.SCALE_CNTL = R128_SCALER_SOURCE_YVYU422;
               break;

          case DSPF_YUY2:
               adev->ov0.SCALE_CNTL = R128_SCALER_SOURCE_VYUY422;
               break;

          case DSPF_I420:
               adev->ov0.SCALE_CNTL = R128_SCALER_SOURCE_YUV12;

               offset_u = front->video.offset +
                          surface->height * front->video.pitch;
               offset_v = offset_u +
                          (surface->height >> 1) * (front->video.pitch >> 1);
               break;

          case DSPF_YV12:
               adev->ov0.SCALE_CNTL = R128_SCALER_SOURCE_YUV12;

               offset_v = front->video.offset +
                          surface->height * front->video.pitch;
               offset_u = offset_v +
                          (surface->height >> 1) * (front->video.pitch >> 1);
               break;

          default:
               BUG("unexpected pixelformat");
               adev->ov0.SCALE_CNTL = 0;
               return;
     }

     adev->ov0.SCALE_CNTL            |= R128_SCALER_ENABLE |
                                        R128_SCALER_DOUBLE_BUFFER |
                                        R128_SCALER_BURST_PER_PLANE |
                                        R128_SCALER_Y2R_TEMP |
                                        R128_SCALER_PIX_EXPAND;

     adev->ov0.H_INC                  = h_inc | ((h_inc >> 1) << 16);
     adev->ov0.V_INC                  = v_inc;
     adev->ov0.STEP_BY                = step_by | (step_by << 8);
     adev->ov0.Y_X_START              = dstBox.x1 | (dstBox.y1 << 16);
     adev->ov0.Y_X_END                = dstBox.x2 | (dstBox.y2 << 16);
     adev->ov0.P1_BLANK_LINES_AT_TOP  = 0x00000fff | ((surface->height - 1) << 16);
     adev->ov0.P23_BLANK_LINES_AT_TOP = 0x000007ff | ((((surface->height + 1) >> 1) - 1) << 16);
     adev->ov0.VID_BUF_PITCH0_VALUE   = front->video.pitch;
     adev->ov0.VID_BUF_PITCH1_VALUE   = front->video.pitch >> 1;
     adev->ov0.P1_X_START_END         = surface->width - 1;
     adev->ov0.P2_X_START_END         = (surface->width >> 1) - 1;
     adev->ov0.P3_X_START_END         = (surface->width >> 1) - 1;
     adev->ov0.VID_BUF0_BASE_ADRS     = front->video.offset & 0x03fffff0;
     adev->ov0.VID_BUF1_BASE_ADRS     = (offset_u & 0x03fffff0) | 1;
     adev->ov0.VID_BUF2_BASE_ADRS     = (offset_v & 0x03fffff0) | 1;
     adev->ov0.P1_H_ACCUM_INIT        = p1_h_accum_init;
     adev->ov0.P23_H_ACCUM_INIT       = p23_h_accum_init;
     adev->ov0.P1_V_ACCUM_INIT        = p1_v_accum_init;
     adev->ov0.P23_V_ACCUM_INIT       = p23_v_accum_init;
}

