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

#include <config.h>

#include <stdio.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/util.h>

#include "regs.h"
#include "mmio.h"
#include "mach64.h"

typedef struct {
     CoreLayerRegionConfig config;
     bool                  visible;

     /* overlay registers */
     struct {
          u32 overlay_Y_X_START;
          u32 overlay_Y_X_END;
          u32 overlay_GRAPHICS_KEY_CLR;
          u32 overlay_GRAPHICS_KEY_MSK;
          u32 overlay_VIDEO_KEY_CLR;
          u32 overlay_VIDEO_KEY_MSK;
          u32 overlay_KEY_CNTL;
          u32 overlay_SCALE_INC;
          u32 overlay_SCALE_CNTL;
          u32 scaler_HEIGHT_WIDTH;
          u32 scaler_BUF_PITCH;
          u32 scaler_BUF0_OFFSET;
          u32 scaler_BUF1_OFFSET;
          u32 scaler_BUF0_OFFSET_U;
          u32 scaler_BUF0_OFFSET_V;
          u32 scaler_BUF1_OFFSET_U;
          u32 scaler_BUF1_OFFSET_V;
          u32 video_FORMAT;
          u32 capture_CONFIG;
     } regs;
} Mach64OverlayLayerData;

static void ov_reset( Mach64DriverData *mdrv );
static void ov_set_regs( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov );
static void ov_calc_regs( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov,
                          CoreLayerRegionConfig *config, CoreSurface *surface );
static void ov_set_buffer( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov );
static void ov_calc_buffer( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov,
                            CoreLayerRegionConfig *config, CoreSurface *surface );
static void ov_set_colorkey( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov );
static void ov_calc_colorkey( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov,
                              CoreLayerRegionConfig *config );
static void ov_set_opacity( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov );
static void ov_calc_opacity( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov,
                             CoreLayerRegionConfig *config );
static void ov_set_field( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov );

#define OV_SUPPORTED_OPTIONS   (DLOP_SRC_COLORKEY | DLOP_DST_COLORKEY | DLOP_DEINTERLACING)

/**********************/

static int
ovLayerDataSize()
{
     return sizeof(Mach64OverlayLayerData);
}

static DFBResult
ovInitLayer( CoreLayer                  *layer,
             void                       *driver_data,
             void                       *layer_data,
             DFBDisplayLayerDescription *description,
             DFBDisplayLayerConfig      *config,
             DFBColorAdjustment         *adjustment )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) driver_data;
     Mach64DeviceData *mdev = mdrv->device_data;

     /* set capabilities and type */
     description->caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE |
                         DLCAPS_DST_COLORKEY | DLCAPS_DEINTERLACING;

     if (mdev->chip >= CHIP_264VT3)
          description->caps |= DLCAPS_SRC_COLORKEY;

     description->type = DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Mach64 Overlay" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;
     config->width       = (mdev->chip >= CHIP_264VT3) ? 640 : 320;
     config->height      = (mdev->chip >= CHIP_264VT3) ? 480 : 240;
     config->pixelformat = DSPF_YUY2;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;

     adjustment->flags   = DCAF_NONE;

     if (mdev->chip >= CHIP_3D_RAGE_PRO) {
          description->caps      |= DLCAPS_BRIGHTNESS | DLCAPS_SATURATION;

          /* fill out default color adjustment,
             only fields set in flags will be accepted from applications */
          adjustment->flags      |= DCAF_BRIGHTNESS | DCAF_SATURATION;
          adjustment->brightness  = 0x8000;
          adjustment->saturation  = 0x8000;
     }

     return DFB_OK;
}

static DFBResult
ovTestRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags *failed )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) driver_data;
     Mach64DeviceData *mdev = mdrv->device_data;
     CoreLayerRegionConfigFlags fail = 0;
     int max_width, max_height = 1024;

     switch (mdev->chip) {
          case CHIP_264VT: /* 264VT2 verified */
          case CHIP_3D_RAGE: /* not verified */
               max_width = 384;
               break;
          case CHIP_264VT3: /* not verified */
          case CHIP_3D_RAGE_II: /* not verified */
          case CHIP_3D_RAGE_IIPLUS:
          case CHIP_264VT4: /* not verified */
          case CHIP_3D_RAGE_IIC:
          case CHIP_3D_RAGE_XLXC:
          case CHIP_3D_RAGE_MOBILITY:
               max_width = 720;
               break;
          case CHIP_3D_RAGE_PRO: /* not verified */
          case CHIP_3D_RAGE_LT_PRO:
               max_width = 768;
               break;
          default:
               D_BUG( "unknown chip" );
               return DFB_UNSUPPORTED;
     }

     if (config->options & DLOP_DEINTERLACING)
          max_height = 2048;

     /* check for unsupported options */
     if (config->options & ~OV_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     /*
      * Video keying doesn't work the same way on 264VT2 as it does
      * on later chips. If enabled the overlay goes completely black
      * so clearly it does something but not what we want.
      */
     if (mdev->chip < CHIP_264VT3 && config->options & DLOP_SRC_COLORKEY)
          fail |= CLRCF_OPTIONS;

     /* check pixel format */
     switch (config->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_YUY2:
          case DSPF_UYVY:
               break;
          case DSPF_I420:
          case DSPF_YV12:
               if (mdev->chip >= CHIP_3D_RAGE_PRO)
                    break;
          default:
               fail |= CLRCF_FORMAT;
     }

     switch (config->format) {
          case DSPF_I420:
          case DSPF_YV12:
               if (config->height & 1)
                    fail |= CLRCF_HEIGHT;
          case DSPF_YUY2:
          case DSPF_UYVY:
               if (config->width & 1)
                    fail |= CLRCF_WIDTH;
          default:
               break;
     }

     /* check width */
     if (config->width > max_width || config->width < 1)
          fail |= CLRCF_WIDTH;

     /* check height */
     if (config->height > max_height || config->height < 1)
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
ovSetRegion( CoreLayer                  *layer,
             void                       *driver_data,
             void                       *layer_data,
             void                       *region_data,
             CoreLayerRegionConfig      *config,
             CoreLayerRegionConfigFlags  updated,
             CoreSurface                *surface,
             CorePalette                *palette )
{
     Mach64DriverData       *mdrv = (Mach64DriverData*) driver_data;
     Mach64OverlayLayerData *mov  = (Mach64OverlayLayerData*) layer_data;

     /* remember configuration */
     mov->config = *config;

     if (updated == CLRCF_ALL)
          ov_reset( mdrv );

     if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_SOURCE | CLRCF_DEST | CLRCF_OPTIONS)) {
          ov_calc_buffer( mdrv, mov, config, surface );
          ov_calc_regs( mdrv, mov, config, surface );
          ov_set_buffer( mdrv, mov );
          ov_set_regs( mdrv, mov );
     }

     if (updated & (CLRCF_OPTIONS | CLRCF_SRCKEY | CLRCF_DSTKEY)) {
          ov_calc_colorkey( mdrv, mov, config );
          ov_set_colorkey( mdrv, mov );
     }

     if (updated & CLRCF_OPTIONS)
          ov_set_field( mdrv, mov );

     if (updated & (CLRCF_DEST | CLRCF_OPACITY)) {
          ov_calc_opacity( mdrv, mov, config );
          ov_set_opacity( mdrv, mov );
     }

     return DFB_OK;
}

static DFBResult
ovRemoveRegion( CoreLayer *layer,
                void      *driver_data,
                void      *layer_data,
                void      *region_data )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) driver_data;
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile u8      *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 2 );

     /*
      * On 264VT2 the keyer sometimes remains active
      * even after the overlay has been disabled.
      */
     mach64_out32( mmio, OVERLAY_KEY_CNTL,
                   VIDEO_KEY_FN_FALSE | GRAPHICS_KEY_FN_FALSE | OVERLAY_CMP_MIX_OR );

     mach64_out32( mmio, OVERLAY_SCALE_CNTL, 0 );

     return DFB_OK;
}

static DFBResult
ovFlipRegion( CoreLayer           *layer,
              void                *driver_data,
              void                *layer_data,
              void                *region_data,
              CoreSurface         *surface,
              DFBSurfaceFlipFlags  flags )
{
     Mach64DriverData       *mdrv = (Mach64DriverData*) driver_data;
     Mach64OverlayLayerData *mov  = (Mach64OverlayLayerData*) layer_data;

     dfb_surface_flip_buffers( surface, false );

     ov_calc_buffer( mdrv, mov, &mov->config, surface );
     ov_set_buffer( mdrv, mov );

     return DFB_OK;
}

static DFBResult
ovSetColorAdjustment( CoreLayer          *layer,
                      void               *driver_data,
                      void               *layer_data,
                      DFBColorAdjustment *adj )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) driver_data;
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile u8      *mmio = mdrv->mmio_base;

     if (mdev->chip < CHIP_3D_RAGE_PRO)
          return DFB_UNSUPPORTED;

     mach64_waitfifo( mdrv, mdev, 1 );

     mach64_out32( mmio, SCALER_COLOUR_CNTL,
                   (((adj->brightness >> 9) - 64) & 0x0000007F) |
                   ((adj->saturation >> 3)  & 0x00001F00) |
                   ((adj->saturation << 5)  & 0x001F0000) );

     return DFB_OK;
}

static DFBResult
ovSetInputField( CoreLayer          *layer,
                 void               *driver_data,
                 void               *layer_data,
                 void               *region_data,
                 int                 field )
{
     Mach64DriverData       *mdrv = (Mach64DriverData*) driver_data;
     Mach64OverlayLayerData *mov  = (Mach64OverlayLayerData*) layer_data;

     mov->regs.capture_CONFIG = OVL_BUF_MODE_SINGLE | (field ? OVL_BUF_NEXT_BUF1 : OVL_BUF_NEXT_BUF0);

     ov_set_field( mdrv, mov );

     return DFB_OK;
}

DisplayLayerFuncs mach64OverlayFuncs = {
     LayerDataSize:      ovLayerDataSize,
     InitLayer:          ovInitLayer,

     TestRegion:         ovTestRegion,
     SetRegion:          ovSetRegion,
     RemoveRegion:       ovRemoveRegion,
     FlipRegion:         ovFlipRegion,
     SetColorAdjustment: ovSetColorAdjustment,
     SetInputField:      ovSetInputField,
};

/* internal */

static void ov_reset( Mach64DriverData *mdrv )
{
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile u8      *mmio = mdrv->mmio_base;

     if (mdev->chip >= CHIP_3D_RAGE_PRO) {
          mach64_waitfifo( mdrv, mdev, 6 );

          mach64_out32( mmio, SCALER_H_COEFF0, 0x00002000 );
          mach64_out32( mmio, SCALER_H_COEFF1, 0x0D06200D );
          mach64_out32( mmio, SCALER_H_COEFF2, 0x0D0A1C0D );
          mach64_out32( mmio, SCALER_H_COEFF3, 0x0C0E1A0C );
          mach64_out32( mmio, SCALER_H_COEFF4, 0x0C14140C );
          mach64_out32( mmio, SCALER_COLOUR_CNTL, 0x00101000 );
     }

     if (mdev->chip >= CHIP_264VT3) {
          mach64_waitfifo( mdrv, mdev, 2 );

          mach64_out32( mmio, OVERLAY_EXCLUSIVE_HORZ, 0 );
          mach64_out32( mmio, OVERLAY_EXCLUSIVE_VERT, 0 );
     }

     mach64_waitfifo( mdrv, mdev, 2 );

     mach64_out32( mmio, OVERLAY_SCALE_CNTL, 0 );
     mach64_out32( mmio, SCALER_TEST, 0 );
}

static void ov_set_regs( Mach64DriverData       *mdrv,
                         Mach64OverlayLayerData *mov )
{
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile u8      *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, (mdev->chip >= CHIP_264VT3) ? 6 : 7 );

     mach64_out32( mmio, VIDEO_FORMAT,          mov->regs.video_FORMAT );
     mach64_out32( mmio, OVERLAY_Y_X_START,     mov->regs.overlay_Y_X_START );
     mach64_out32( mmio, OVERLAY_Y_X_END,       mov->regs.overlay_Y_X_END );
     mach64_out32( mmio, OVERLAY_SCALE_INC,     mov->regs.overlay_SCALE_INC );
     mach64_out32( mmio, SCALER_HEIGHT_WIDTH,   mov->regs.scaler_HEIGHT_WIDTH );

     if (mdev->chip >= CHIP_264VT3) {
          mach64_out32( mmio, SCALER_BUF_PITCH, mov->regs.scaler_BUF_PITCH );
     } else {
          mach64_out32( mmio, BUF0_PITCH,       mov->regs.scaler_BUF_PITCH );
          mach64_out32( mmio, BUF1_PITCH,       mov->regs.scaler_BUF_PITCH );
     }
}

static void ov_set_buffer( Mach64DriverData       *mdrv,
                           Mach64OverlayLayerData *mov )
{
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile u8      *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, (mdev->chip >= CHIP_3D_RAGE_PRO) ? 6 : 2 );

     if (mdev->chip >= CHIP_264VT3) {
          mach64_out32( mmio, SCALER_BUF0_OFFSET,   mov->regs.scaler_BUF0_OFFSET );
          mach64_out32( mmio, SCALER_BUF1_OFFSET,   mov->regs.scaler_BUF1_OFFSET );
     } else {
          mach64_out32( mmio, BUF0_OFFSET,          mov->regs.scaler_BUF0_OFFSET );
          mach64_out32( mmio, BUF1_OFFSET,          mov->regs.scaler_BUF1_OFFSET );
     }

     if (mdev->chip >= CHIP_3D_RAGE_PRO) {
          mach64_out32( mmio, SCALER_BUF0_OFFSET_U, mov->regs.scaler_BUF0_OFFSET_U );
          mach64_out32( mmio, SCALER_BUF0_OFFSET_V, mov->regs.scaler_BUF0_OFFSET_V );
          mach64_out32( mmio, SCALER_BUF1_OFFSET_U, mov->regs.scaler_BUF1_OFFSET_U );
          mach64_out32( mmio, SCALER_BUF1_OFFSET_V, mov->regs.scaler_BUF1_OFFSET_V );
     }
}

static void ov_set_colorkey( Mach64DriverData       *mdrv,
                             Mach64OverlayLayerData *mov )
{
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile u8      *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 5 );

     mach64_out32( mmio, OVERLAY_GRAPHICS_KEY_CLR, mov->regs.overlay_GRAPHICS_KEY_CLR );
     mach64_out32( mmio, OVERLAY_GRAPHICS_KEY_MSK, mov->regs.overlay_GRAPHICS_KEY_MSK );
     mach64_out32( mmio, OVERLAY_VIDEO_KEY_CLR,    mov->regs.overlay_VIDEO_KEY_CLR );
     mach64_out32( mmio, OVERLAY_VIDEO_KEY_MSK,    mov->regs.overlay_VIDEO_KEY_MSK );
     mach64_out32( mmio, OVERLAY_KEY_CNTL,         mov->regs.overlay_KEY_CNTL );
}

static void ov_set_opacity( Mach64DriverData       *mdrv,
                            Mach64OverlayLayerData *mov )
{
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile u8      *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 1 );

     mach64_out32( mmio, OVERLAY_SCALE_CNTL, mov->regs.overlay_SCALE_CNTL );
}

static void ov_set_field( Mach64DriverData       *mdrv,
                          Mach64OverlayLayerData *mov )
{
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile u8      *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 1 );

     mach64_out32( mmio, CAPTURE_CONFIG, mov->regs.capture_CONFIG );
}

static void ov_calc_regs( Mach64DriverData       *mdrv,
                          Mach64OverlayLayerData *mov,
                          CoreLayerRegionConfig  *config,
                          CoreSurface            *surface )
{
     Mach64DeviceData *mdev   = mdrv->device_data;
     volatile u8      *mmio   = mdrv->mmio_base;
     SurfaceBuffer    *buffer = surface->front_buffer;
     VideoMode        *mode   = dfb_system_current_mode();
     int               yres   = mode->yres;
     int               pitch  = buffer->video.pitch / DFB_BYTES_PER_PIXEL( surface->format );
     DFBRectangle      source = config->source;
     DFBRectangle      dest   = config->dest;

     DFBRegion dst;
     int h_inc, v_inc;
     u32 lcd_gen_ctrl, vert_stretching;
     u8 ecp_div;

     if (mode->doubled) {
          dest.y *= 2;
          dest.h *= 2;
          yres   *= 2;
     }

     if (config->options & DLOP_DEINTERLACING) {
          source.y /= 2;
          source.h /= 2;
          pitch    *= 2;
     } else
          mov->regs.capture_CONFIG = OVL_BUF_MODE_SINGLE | OVL_BUF_NEXT_BUF0;

     dst.x1 = dest.x;
     dst.y1 = dest.y;
     dst.x2 = dest.x + dest.w - 1;
     dst.y2 = dest.y + dest.h - 1;

     mov->visible = dfb_region_intersect( &dst, 0, 0, mode->xres - 1, yres - 1 );

     if (mode->laced) {
          dest.y /= 2;
          dest.h /= 2;
     }

     ecp_div = (mach64_in_pll( mmio, PLL_VCLK_CNTL ) & ECP_DIV) >> 4;
     h_inc = (source.w << (12 + ecp_div)) / dest.w;

     lcd_gen_ctrl = mach64_in_lcd( mdev, mmio, LCD_GEN_CTRL );
     vert_stretching = mach64_in_lcd( mdev, mmio, VERT_STRETCHING );
     if ((lcd_gen_ctrl & LCD_ON) && (vert_stretching & VERT_STRETCH_EN))
          v_inc = (source.h << 2) * (vert_stretching & VERT_STRETCH_RATIO0) / dest.h;
     else
          v_inc = (source.h << 12) / dest.h;

     switch (surface->format) {
          case DSPF_ARGB1555:
               mov->regs.video_FORMAT = SCALER_IN_RGB15;
               break;
          case DSPF_RGB16:
               mov->regs.video_FORMAT = SCALER_IN_RGB16;
               break;
          case DSPF_RGB32:
               mov->regs.video_FORMAT = SCALER_IN_RGB32;
               break;
          case DSPF_UYVY:
               mov->regs.video_FORMAT = SCALER_IN_YVYU422;
               break;
          case DSPF_YUY2:
               mov->regs.video_FORMAT = SCALER_IN_VYUY422;
               break;
          case DSPF_I420:
          case DSPF_YV12:
               mov->regs.video_FORMAT = SCALER_IN_YUV12;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
     }

     mov->regs.scaler_HEIGHT_WIDTH = (source.w << 16) | source.h;
     mov->regs.scaler_BUF_PITCH    = pitch;

     mov->regs.overlay_Y_X_START   = (dst.x1 << 16) | dst.y1 | OVERLAY_LOCK_START;
     mov->regs.overlay_Y_X_END     = (dst.x2 << 16) | dst.y2;
     mov->regs.overlay_SCALE_INC   = (h_inc << 16) | v_inc;
}

static void ov_calc_buffer( Mach64DriverData       *mdrv,
                            Mach64OverlayLayerData *mov,
                            CoreLayerRegionConfig  *config,
                            CoreSurface            *surface )
{
     SurfaceBuffer *buffer = surface->front_buffer;
     int            pitch  = buffer->video.pitch;
     DFBRectangle   source = config->source;

     u32 offset, offset_u, offset_v;
     int cropleft, croptop;

     if (config->options & DLOP_DEINTERLACING) {
          source.y /= 2;
          source.h /= 2;
          pitch    *= 2;
     }

     /* Source cropping */
     cropleft = source.x;
     croptop  = source.y;

     /* Add destination cropping */
     if (config->dest.x < 0)
          cropleft += -config->dest.x * source.w / config->dest.w;
     if (config->dest.y < 0)
          croptop  += -config->dest.y * source.h / config->dest.h;

     switch (surface->format) {
          case DSPF_I420:
               cropleft &= ~15;
               croptop  &= ~1;

               offset_u  = buffer->video.offset + surface->height * buffer->video.pitch;
               offset_v  = offset_u + surface->height/2 * buffer->video.pitch/2;
               offset_u += croptop/2 * pitch/2 + cropleft/2;
               offset_v += croptop/2 * pitch/2 + cropleft/2;
               break;

          case DSPF_YV12:
               cropleft &= ~15;
               croptop  &= ~1;

               offset_v  = buffer->video.offset + surface->height * buffer->video.pitch;
               offset_u  = offset_v + surface->height/2 * buffer->video.pitch/2;
               offset_v += croptop/2 * pitch/2 + cropleft/2;
               offset_u += croptop/2 * pitch/2 + cropleft/2;
               break;

          default:
               offset_u = 0;
               offset_v = 0;
               break;
     }

     offset  = buffer->video.offset;
     offset += croptop * pitch + cropleft * DFB_BYTES_PER_PIXEL( surface->format );

     mov->regs.scaler_BUF0_OFFSET   = offset;
     mov->regs.scaler_BUF0_OFFSET_U = offset_u;
     mov->regs.scaler_BUF0_OFFSET_V = offset_v;

     mov->regs.scaler_BUF1_OFFSET   = offset   + buffer->video.pitch;
     mov->regs.scaler_BUF1_OFFSET_U = offset_u + buffer->video.pitch/2;
     mov->regs.scaler_BUF1_OFFSET_V = offset_v + buffer->video.pitch/2;
}

static u32 ovColorKey[] = {
     VIDEO_KEY_FN_TRUE      | GRAPHICS_KEY_FN_TRUE  | OVERLAY_CMP_MIX_OR, /* 0 */
     VIDEO_KEY_FN_NOT_EQUAL | GRAPHICS_KEY_FN_FALSE | OVERLAY_CMP_MIX_OR, /* DLOP_SRC_COLORKEY */
     VIDEO_KEY_FN_FALSE     | GRAPHICS_KEY_FN_EQUAL | OVERLAY_CMP_MIX_OR, /* DLOP_DST_COLORKEY */
     VIDEO_KEY_FN_NOT_EQUAL | GRAPHICS_KEY_FN_EQUAL | OVERLAY_CMP_MIX_AND /* DLOP_SRC_COLORKEY |
                                                                             DLOP_DST_COLORKEY */
};

static void ov_calc_colorkey( Mach64DriverData       *mdrv,
                              Mach64OverlayLayerData *mov,
                              CoreLayerRegionConfig  *config )
{
     DFBSurfacePixelFormat primary_format = dfb_primary_layer_pixelformat();

     /* Video key is always RGB24 */
     mov->regs.overlay_VIDEO_KEY_CLR = PIXEL_RGB32( config->src_key.r,
                                                    config->src_key.g,
                                                    config->src_key.b );

     /* The same mask is used for all three components */
     mov->regs.overlay_VIDEO_KEY_MSK = 0xFF;

     switch (primary_format) {
          case DSPF_RGB332:
               mov->regs.overlay_GRAPHICS_KEY_CLR = PIXEL_RGB332( config->dst_key.r,
                                                                  config->dst_key.g,
                                                                  config->dst_key.b );
               break;
          case DSPF_ARGB1555:
               mov->regs.overlay_GRAPHICS_KEY_CLR = PIXEL_ARGB1555( config->dst_key.a,
                                                                    config->dst_key.r,
                                                                    config->dst_key.g,
                                                                    config->dst_key.b );
               break;
          case DSPF_RGB16:
               mov->regs.overlay_GRAPHICS_KEY_CLR = PIXEL_RGB16( config->dst_key.r,
                                                                 config->dst_key.g,
                                                                 config->dst_key.b );
               break;
          case DSPF_RGB32:
               mov->regs.overlay_GRAPHICS_KEY_CLR = PIXEL_RGB32( config->dst_key.r,
                                                                 config->dst_key.g,
                                                                 config->dst_key.b );
               break;
          case DSPF_ARGB:
               mov->regs.overlay_GRAPHICS_KEY_CLR = PIXEL_ARGB( config->dst_key.a,
                                                                config->dst_key.r,
                                                                config->dst_key.g,
                                                                config->dst_key.b );
               break;
          default:
               D_BUG( "unexpected pixelformat" );
     }

     mov->regs.overlay_GRAPHICS_KEY_MSK = (1 << DFB_COLOR_BITS_PER_PIXEL( primary_format )) - 1;

     mov->regs.overlay_KEY_CNTL = ovColorKey[(config->options >> 3) & 3];
}

static void ov_calc_opacity( Mach64DriverData       *mdrv,
                             Mach64OverlayLayerData *mov,
                             CoreLayerRegionConfig  *config )
{
     mov->regs.overlay_SCALE_CNTL = SCALE_PIX_EXPAND | SCALE_Y2R_TEMP;

     if (config->opacity && mov->visible)
          mov->regs.overlay_SCALE_CNTL |= OVERLAY_EN | SCALE_EN;
}
