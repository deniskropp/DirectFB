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

     /* overlay registers */
     struct {
          __u32 overlay_Y_X_START;
          __u32 overlay_Y_X_END;
          __u32 overlay_GRAPHICS_KEY_CLR;
          __u32 overlay_GRAPHICS_KEY_MSK;
          __u32 overlay_VIDEO_KEY_CLR;
          __u32 overlay_VIDEO_KEY_MSK;
          __u32 overlay_KEY_CNTL;
          __u32 overlay_SCALE_INC;
          __u32 overlay_SCALE_CNTL;
          __u32 scaler_HEIGHT_WIDTH;
          __u32 scaler_BUF0_OFFSET;
          __u32 scaler_BUF_PITCH;
          __u32 scaler_BUF0_OFFSET_U;
          __u32 scaler_BUF0_OFFSET_V;
          __u32 video_FORMAT;
     } regs;
} Mach64OverlayLayerData;

static void ov_set_regs( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov );
static void ov_calc_regs( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov,
                           CoreLayerRegionConfig *config, CoreSurface *surface );
static void ov_set_buffer( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov );
static void ov_calc_buffer( Mach64DriverData *mdrv, Mach64OverlayLayerData *mov,
                            CoreLayerRegionConfig *config, CoreSurface *surface );

#define OV_SUPPORTED_OPTIONS   (DLOP_SRC_COLORKEY | DLOP_DST_COLORKEY)

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
     volatile __u8    *mmio = mdrv->mmio_base;

     /* set capabilities and type */
     description->caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE |
                         DLCAPS_BRIGHTNESS | DLCAPS_SATURATION |
                         DLCAPS_SRC_COLORKEY | DLCAPS_DST_COLORKEY;
     description->type = DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Mach64 Overlay" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;
     config->width       = 640;
     config->height      = 480;
     config->pixelformat = DSPF_YUY2;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;

     /* fill out default color adjustment,
        only fields set in flags will be accepted from applications */
     if (mdev->chip >= CHIP_3D_RAGE_PRO) {
          adjustment->flags      = DCAF_BRIGHTNESS | DCAF_SATURATION;
          adjustment->brightness = 0x8000;
          adjustment->saturation = 0x8000;
     } else
          adjustment->flags      = DCAF_NONE;

     /* reset overlay */
     if (mdev->chip >= CHIP_3D_RAGE_PRO) {
          mach64_waitfifo( mdrv, mdev, 6 );
          mach64_out32( mmio, SCALER_H_COEFF0, 0x00002000 );
          mach64_out32( mmio, SCALER_H_COEFF1, 0x0D06200D );
          mach64_out32( mmio, SCALER_H_COEFF2, 0x0D0A1C0D );
          mach64_out32( mmio, SCALER_H_COEFF3, 0x0C0E1A0C );
          mach64_out32( mmio, SCALER_H_COEFF4, 0x0C14140C );
          mach64_out32( mmio, SCALER_COLOUR_CNTL, 0x00101000 );
     }

     mach64_waitfifo( mdrv, mdev, 1 );
     mach64_out32( mmio, OVERLAY_SCALE_CNTL, 0 );

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
     int max_width = (mdev->chip >= CHIP_3D_RAGE_PRO) ? 768 : 720;

     /* check for unsupported options */
     if (config->options & ~OV_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     /* check pixel format */
     switch (config->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               break;

          default:
               fail |= CLRCF_FORMAT;
     }

     /* check width */
     if (config->width > max_width || config->width < 1)
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

     ov_calc_buffer( mdrv, mov, config, surface );
     ov_calc_regs( mdrv, mov, config, surface );
     ov_set_buffer( mdrv, mov );
     ov_set_regs( mdrv, mov );

     return DFB_OK;
}

static DFBResult
ovRemoveRegion( CoreLayer *layer,
                void      *driver_data,
                void      *layer_data,
                void      *region_data )
{
     Mach64DriverData       *mdrv = (Mach64DriverData*) driver_data;
     Mach64DeviceData       *mdev = mdrv->device_data;

     mach64_waitfifo( mdrv, mdev, 1 );

     mach64_out32( mdrv->mmio_base, OVERLAY_SCALE_CNTL, 0 );

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
     volatile __u8    *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 1 );

     mach64_out32( mmio, SCALER_COLOUR_CNTL,
                   (((adj->brightness >> 9) - 64) & 0x0000007F) |
                   ((adj->saturation >> 3)  & 0x00001F00) |
                   ((adj->saturation << 5)  & 0x001F0000) );

     return DFB_OK;
}

DisplayLayerFuncs mach64OverlayFuncs = {
     LayerDataSize:      ovLayerDataSize,
     InitLayer:          ovInitLayer,

     TestRegion:         ovTestRegion,
     SetRegion:          ovSetRegion,
     RemoveRegion:       ovRemoveRegion,
     FlipRegion:         ovFlipRegion,
     SetColorAdjustment: ovSetColorAdjustment
};

/* internal */

static void ov_set_buffer( Mach64DriverData       *mdrv,
                           Mach64OverlayLayerData *mov )
{
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile __u8 *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 3 );

     mach64_out32( mmio, SCALER_BUF0_OFFSET,       mov->regs.scaler_BUF0_OFFSET );
     mach64_out32( mmio, SCALER_BUF0_OFFSET_U,     mov->regs.scaler_BUF0_OFFSET_U );
     mach64_out32( mmio, SCALER_BUF0_OFFSET_V,     mov->regs.scaler_BUF0_OFFSET_V );
}

static void ov_set_regs( Mach64DriverData       *mdrv,
                         Mach64OverlayLayerData *mov )
{
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile __u8 *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 12 );

     mach64_out32( mmio, SCALER_HEIGHT_WIDTH,      mov->regs.scaler_HEIGHT_WIDTH );
     mach64_out32( mmio, SCALER_BUF_PITCH,         mov->regs.scaler_BUF_PITCH );
     mach64_out32( mmio, VIDEO_FORMAT,             mov->regs.video_FORMAT );
     mach64_out32( mmio, OVERLAY_Y_X_START,        mov->regs.overlay_Y_X_START );
     mach64_out32( mmio, OVERLAY_Y_X_END,          mov->regs.overlay_Y_X_END );
     mach64_out32( mmio, OVERLAY_GRAPHICS_KEY_CLR, mov->regs.overlay_GRAPHICS_KEY_CLR );
     mach64_out32( mmio, OVERLAY_GRAPHICS_KEY_MSK, mov->regs.overlay_GRAPHICS_KEY_MSK );
     mach64_out32( mmio, OVERLAY_VIDEO_KEY_CLR,    mov->regs.overlay_VIDEO_KEY_CLR );
     mach64_out32( mmio, OVERLAY_VIDEO_KEY_MSK,    mov->regs.overlay_VIDEO_KEY_MSK );
     mach64_out32( mmio, OVERLAY_KEY_CNTL,         mov->regs.overlay_KEY_CNTL);
     mach64_out32( mmio, OVERLAY_SCALE_INC,        mov->regs.overlay_SCALE_INC );
     mach64_out32( mmio, OVERLAY_SCALE_CNTL,       mov->regs.overlay_SCALE_CNTL );
}

static __u32 ovColorKey[] = {
     VIDEO_KEY_FN_TRUE      | GRAPHICS_KEY_FN_TRUE  | OVERLAY_CMP_MIX_OR, /* 0 */
     VIDEO_KEY_FN_NOT_EQUAL | GRAPHICS_KEY_FN_FALSE | OVERLAY_CMP_MIX_OR, /* DLOP_SRC_COLORKEY */
     VIDEO_KEY_FN_FALSE     | GRAPHICS_KEY_FN_EQUAL | OVERLAY_CMP_MIX_OR, /* DLOP_DST_COLORKEY */
     VIDEO_KEY_FN_NOT_EQUAL | GRAPHICS_KEY_FN_EQUAL | OVERLAY_CMP_MIX_AND /* DLOP_SRC_COLORKEY |
                                                                             DLOP_DST_COLORKEY */
};

static void ov_calc_regs( Mach64DriverData       *mdrv,
                          Mach64OverlayLayerData *mov,
                          CoreLayerRegionConfig  *config,
                          CoreSurface            *surface )
{
     Mach64DeviceData *mdev = mdrv->device_data;
     volatile __u8 *mmio = mdrv->mmio_base;
     DFBSurfacePixelFormat primary_format = dfb_primary_layer_pixelformat();
     SurfaceBuffer *front_buffer = surface->front_buffer;
     VideoMode *mode = dfb_system_current_mode();
     __u32 lcd_gen_ctrl, vert_stretching;
     DFBRegion dst;
     int h_inc, v_inc;

     /* FIXME: I don't think this should be NULL ever. */
     if (!mode)
          return;

     dst.x1 = config->dest.x;
     dst.y1 = config->dest.y;
     dst.x2 = config->dest.x + config->dest.w - 1;
     dst.y2 = config->dest.y + config->dest.h - 1;

     dfb_region_intersect( &dst, 0, 0, mode->xres - 1, mode->yres - 1 );

     h_inc = (config->source.w << 12) / config->dest.w;

     lcd_gen_ctrl = mach64_in_lcd( mdev, mmio, LCD_GEN_CTRL );
     vert_stretching = mach64_in_lcd( mdev, mmio, VERT_STRETCHING );

     if ((lcd_gen_ctrl & LCD_ON) && (vert_stretching & VERT_STRETCH_EN))
          v_inc = (config->source.h << 2) * (vert_stretching & VERT_STRETCH_RATIO0) / config->dest.h;
     else
          v_inc = (config->source.h << 12) / config->dest.h;

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
               mov->regs.video_FORMAT = SCALER_IN_YUV12;
               break;

          case DSPF_YV12:
               mov->regs.video_FORMAT = SCALER_IN_YUV12;
               break;

          default:
               D_BUG("unexpected pixelformat");
               return;
     }

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
               D_BUG("unexpected pixelformat");
               return;
     }
     mov->regs.overlay_GRAPHICS_KEY_MSK = (1 << DFB_COLOR_BITS_PER_PIXEL( primary_format )) - 1;

     mov->regs.overlay_KEY_CNTL = ovColorKey[(config->options >> 3) & 3];

     mov->regs.scaler_HEIGHT_WIDTH = (config->source.w << 16) | config->source.h;
     mov->regs.scaler_BUF_PITCH    = front_buffer->video.pitch / DFB_BYTES_PER_PIXEL( surface->format );

     mov->regs.overlay_Y_X_START = (dst.x1 << 16) | dst.y1 | OVERLAY_LOCK_START;
     mov->regs.overlay_Y_X_END   = (dst.x2 << 16) | dst.y2;

     mov->regs.overlay_SCALE_INC  = (h_inc << 16) | v_inc;
     mov->regs.overlay_SCALE_CNTL = SCALE_PIX_EXPAND | SCALE_EN;

     if (config->opacity)
          mov->regs.overlay_SCALE_CNTL |= OVERLAY_EN;
}


static void ov_calc_buffer( Mach64DriverData       *mdrv,
                            Mach64OverlayLayerData *mov,
                            CoreLayerRegionConfig  *config,
                            CoreSurface            *surface )
{
     __u32 offset, offset_u = 0, offset_v = 0;
     SurfaceBuffer *front_buffer = surface->front_buffer;
     int cropleft, croptop;

     /* Source cropping */
     cropleft = config->source.x;
     croptop  = config->source.y;

     /* Add destination cropping */
     if (config->dest.x < 0)
          cropleft += -config->dest.x * config->source.w / config->dest.w;
     if (config->dest.y < 0)
          croptop  += -config->dest.y * config->source.h / config->dest.h;

     switch (surface->format) {
          case DSPF_I420:
               cropleft &= ~15;
               croptop &= ~1;

               offset_u = front_buffer->video.offset + surface->height * front_buffer->video.pitch;
               offset_v = offset_u + (surface->height/2) * (front_buffer->video.pitch/2);
               offset_u += (croptop/2) * (front_buffer->video.pitch/2) + (cropleft/2);
               offset_v += (croptop/2) * (front_buffer->video.pitch/2) + (cropleft/2);
               break;

          case DSPF_YV12:
               cropleft &= ~15;
               croptop &= ~1;

               offset_v = front_buffer->video.offset + surface->height * front_buffer->video.pitch;
               offset_u = offset_v + (surface->height/2) * (front_buffer->video.pitch/2);
               offset_u += (croptop/2) * (front_buffer->video.pitch/2) + (cropleft/2);
               offset_v += (croptop/2) * (front_buffer->video.pitch/2) + (cropleft/2);
               break;

          default:
               break;
     }

     offset = front_buffer->video.offset;
     offset += croptop * front_buffer->video.pitch + cropleft * DFB_BYTES_PER_PIXEL( surface->format );

     mov->regs.scaler_BUF0_OFFSET     = offset;
     mov->regs.scaler_BUF0_OFFSET_U   = offset_u;
     mov->regs.scaler_BUF0_OFFSET_V   = offset_v;
}
