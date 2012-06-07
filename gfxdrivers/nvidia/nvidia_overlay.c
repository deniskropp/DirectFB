/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Oliver Schwartz <Oliver.Schwartz@gmx.de> and
              Claudio Ciccani <klan@users.sf.net>.

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
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <core/coredefs.h>
#include <core/surface.h>
#include <core/gfxcard.h>

#include <core/layers.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/layers_internal.h>

#include <gfx/convert.h>

#include <direct/memcpy.h>
#include <direct/messages.h>

#include "nvidia.h"
#include "nvidia_regs.h"
#include "nvidia_accel.h"


typedef struct {
     CoreLayerRegionConfig  config;
     CoreSurface           *videoSurface;
     CoreSurfaceBufferLock *lock;
     
     short                  brightness;
     short                  contrast;
     short                  hue;
     short                  saturation;
     int                    field;

     struct {
          u32 BUFFER;
          u32 STOP;
          u32 UVBASE_0;
          u32 UVBASE_1;
          u32 UVOFFSET_0;
          u32 UVOFFSET_1;
          u32 BASE_0;
          u32 BASE_1;
          u32 OFFSET_0;
          u32 OFFSET_1;
          u32 SIZE_IN_0;
          u32 SIZE_IN_1;
          u32 POINT_IN_0;
          u32 POINT_IN_1;
          u32 DS_DX_0;
          u32 DS_DX_1;
          u32 DT_DY_0;
          u32 DT_DY_1;
          u32 POINT_OUT_0;
          u32 POINT_OUT_1;
          u32 SIZE_OUT_0;
          u32 SIZE_OUT_1;
          u32 FORMAT_0;
          u32 FORMAT_1;
     } regs;
} NVidiaOverlayLayerData;

static void ov0_set_regs    ( NVidiaDriverData           *nvdrv,
                              NVidiaOverlayLayerData     *nvov0,
                              CoreLayerRegionConfigFlags  flags );
static void ov0_calc_regs   ( NVidiaDriverData           *nvdrv,
                              NVidiaOverlayLayerData     *nvov0,
                              CoreLayerRegionConfig      *config,
                              CoreLayerRegionConfigFlags  flags );
static void ov0_set_colorkey( NVidiaDriverData       *nvdrv,
                              NVidiaOverlayLayerData *nvov0,
                              CoreLayerRegionConfig  *config );
static void ov0_set_csc     ( NVidiaDriverData       *nvdrv,
                              NVidiaOverlayLayerData *nvov0 );

#define OV0_SUPPORTED_OPTIONS \
     ( DLOP_DST_COLORKEY | DLOP_DEINTERLACING )

/**********************/



static int
ov0LayerDataSize( void )
{
     return sizeof(NVidiaOverlayLayerData);
}

static DFBResult
ov0InitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
     NVidiaDriverData       *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     
     /* set capabilities and type */
     description->caps =  DLCAPS_SURFACE      | DLCAPS_SCREEN_LOCATION |
                          DLCAPS_BRIGHTNESS   | DLCAPS_CONTRAST        |
                          DLCAPS_SATURATION   | DLCAPS_HUE             |
                          DLCAPS_DST_COLORKEY | DLCAPS_DEINTERLACING;
     description->type = DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "NVidia Overlay" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT     |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;
     config->width       = 640;
     config->height      = 480;
     config->pixelformat = DSPF_YUY2;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_NONE;

     /* fill out default color adjustment,
        only fields set in flags will be accepted from applications */
     adjustment->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST |
                              DCAF_SATURATION | DCAF_HUE;
     adjustment->brightness = 0x8000;
     adjustment->contrast   = 0x8000;
     adjustment->saturation = 0x8000;
     adjustment->hue        = 0x8000;
 
     /* reset overlay */
     nvov0->brightness = 0;
     nvov0->contrast   = 4096;
     nvov0->hue        = 0;
     nvov0->saturation = 4096;
     ov0_set_csc( nvdrv, nvov0 );

     return DFB_OK;
}

static DFBResult
ov0Remove( CoreLayer *layer,
           void      *driver_data,
           void      *layer_data,
           void      *region_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     volatile u8      *mmio  = nvdrv->mmio_base;
     
     /* disable overlay */
     nv_out32( mmio, PVIDEO_STOP, PVIDEO_STOP_OVERLAY_ACTIVE | 
                                  PVIDEO_STOP_METHOD_IMMEDIATELY );
     nv_out32( mmio, PVIDEO_BUFFER, 0 );

     return DFB_OK;
}

static DFBResult
ov0TestRegion(CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags *failed )
{
     NVidiaDriverData           *nvdrv = driver_data;
     NVidiaDeviceData           *nvdev = nvdrv->device_data;
     CoreLayerRegionConfigFlags  fail  = CLRCF_NONE;


     /* check for unsupported options */
     if (config->options & ~OV0_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     /* check buffermode */
     switch (config->buffermode) {
          case DLBM_FRONTONLY:
          case DLBM_BACKSYSTEM:
          case DLBM_BACKVIDEO:
          case DLBM_TRIPLE:
               break;

          default:
               fail |= CLRCF_BUFFERMODE;
               break;
     }

     /* check pixel format */
     switch (config->format) { 
          case DSPF_YUY2:
          case DSPF_UYVY:
               break;

          case DSPF_NV12:
          /*case DSPF_NV21:*/
               if (nvdev->arch < NV_ARCH_30)
                    fail |= CLRCF_FORMAT;
               break;

          default:
               fail |= CLRCF_FORMAT;
               break;
     }

     /* check width */
     if (config->width > 2046 || config->width < 1)
          fail |= CLRCF_WIDTH;

     /* check height */
     if (config->height > 2046 || config->height < 1)
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
ov0SetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags  updated,
              CoreSurface                *surface,
              CorePalette                *palette,
              CoreSurfaceBufferLock      *left_lock,
              CoreSurfaceBufferLock      *right_lock )
{
     NVidiaDriverData       *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     
     /* remember configuration */
     nvov0->config = *config;
     
     nvov0->videoSurface = surface;
     nvov0->lock = left_lock;

     /* set configuration */
     if (updated & (CLRCF_WIDTH  | CLRCF_HEIGHT | CLRCF_FORMAT  |
                    CLRCF_SOURCE | CLRCF_DEST   | CLRCF_OPTIONS | CLRCF_OPACITY))
     {
          ov0_calc_regs( nvdrv, nvov0, config, updated );
          ov0_set_regs( nvdrv, nvov0, updated );
     }

     /* set destination colorkey */
     if (updated & CLRCF_DSTKEY)
          ov0_set_colorkey( nvdrv, nvov0, config );

     return DFB_OK;
}

static DFBResult
ov0FlipRegion ( CoreLayer             *layer,
                void                  *driver_data,
                void                  *layer_data,
                void                  *region_data,
                CoreSurface           *surface,
                DFBSurfaceFlipFlags    flags,
                CoreSurfaceBufferLock *left_lock,
                CoreSurfaceBufferLock *right_lock )
{
     NVidiaDriverData       *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
    
     nvov0->videoSurface = surface;
     nvov0->lock = left_lock;

     dfb_surface_flip( nvov0->videoSurface, false );

     ov0_calc_regs( nvdrv, nvov0, &nvov0->config, CLRCF_SURFACE );
     ov0_set_regs( nvdrv, nvov0, CLRCF_SURFACE );

     if (flags & DSFLIP_WAIT)
          dfb_layer_wait_vsync( layer );
     
     return DFB_OK;
}

static DFBResult
ov0SetColorAdjustment( CoreLayer          *layer,
                       void               *driver_data,
                       void               *layer_data,
                       DFBColorAdjustment *adj )
{
     NVidiaDriverData       *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;

     if (adj->flags & DCAF_BRIGHTNESS) {
          nvov0->brightness = (adj->brightness >> 8) - 128;
          D_DEBUG( "DirectFB/NVidia/Overlay: brightness=%i\n", nvov0->brightness );
     }

     if (adj->flags & DCAF_CONTRAST) {
          nvov0->contrast   = 8191 - (adj->contrast >> 3); /* contrast inverted ?! */
          D_DEBUG( "DirectFB/NVidia/Overlay: contrast=%i\n", nvov0->contrast );
     }

     if (adj->flags & DCAF_SATURATION) {
          nvov0->saturation = adj->saturation >> 3;
          D_DEBUG( "DirectFB/NVidia/Overlay: saturation=%i\n", nvov0->saturation );
     }

     if (adj->flags & DCAF_HUE) {
          nvov0->hue        = (adj->hue / 182 - 180) % 360;
          D_DEBUG( "DirectFB/NVidia/Overlay: hue=%i\n", nvov0->hue );
     }

     ov0_set_csc( nvdrv, nvov0 );

     return DFB_OK;
}

static DFBResult
ov0SetInputField( CoreLayer *layer,
                  void      *driver_data,
                  void      *layer_data,
                  void      *region_data,
                  int        field )
{
     NVidiaDriverData       *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
    
     nvov0->field = field;
     nvov0->regs.BUFFER = 1 << (field << 2);
     
     nv_out32( nvdrv->mmio_base, PVIDEO_BUFFER, nvov0->regs.BUFFER );
     
     return DFB_OK;
}

DisplayLayerFuncs nvidiaOverlayFuncs = {
     .LayerDataSize      = ov0LayerDataSize,
     .InitLayer          = ov0InitLayer,
     .SetRegion          = ov0SetRegion,
     .RemoveRegion       = ov0Remove,
     .TestRegion         = ov0TestRegion,
     .FlipRegion         = ov0FlipRegion,
     .SetColorAdjustment = ov0SetColorAdjustment,
     .SetInputField      = ov0SetInputField,
};


/* internal */

static void ov0_set_regs( NVidiaDriverData           *nvdrv, 
                          NVidiaOverlayLayerData     *nvov0,
                          CoreLayerRegionConfigFlags  flags )
{
     volatile u8 *mmio = nvdrv->mmio_base;
     
     if (flags & CLRCF_SURFACE) {
          if (DFB_PLANAR_PIXELFORMAT(nvov0->config.format)) { 
               nv_out32( mmio, PVIDEO_UVBASE_0, nvov0->regs.UVBASE_0 );
               nv_out32( mmio, PVIDEO_UVBASE_1, nvov0->regs.UVBASE_1 );
               nv_out32( mmio, PVIDEO_UVOFFSET_0, nvov0->regs.UVOFFSET_0 );
               nv_out32( mmio, PVIDEO_UVOFFSET_1, nvov0->regs.UVOFFSET_1 );
          }
          nv_out32( mmio, PVIDEO_BASE_0, nvov0->regs.BASE_0 );
          nv_out32( mmio, PVIDEO_BASE_1, nvov0->regs.BASE_1 );
          nv_out32( mmio, PVIDEO_OFFSET_0, nvov0->regs.OFFSET_0 );
          nv_out32( mmio, PVIDEO_OFFSET_1, nvov0->regs.OFFSET_1 );
     }
     if (flags & (CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_OPTIONS)) {
          nv_out32( mmio, PVIDEO_SIZE_IN_0, nvov0->regs.SIZE_IN_0 );
          nv_out32( mmio, PVIDEO_SIZE_IN_1, nvov0->regs.SIZE_IN_1 );
     }
     if (flags & (CLRCF_SOURCE | CLRCF_DEST | CLRCF_OPTIONS)) {
          nv_out32( mmio, PVIDEO_POINT_IN_0, nvov0->regs.POINT_IN_0 );
          nv_out32( mmio, PVIDEO_POINT_IN_1, nvov0->regs.POINT_IN_1 );
          nv_out32( mmio, PVIDEO_DS_DX_0,    nvov0->regs.DS_DX_0 );
          nv_out32( mmio, PVIDEO_DS_DX_1,    nvov0->regs.DS_DX_1 );
          nv_out32( mmio, PVIDEO_DT_DY_0,    nvov0->regs.DT_DY_0 );
          nv_out32( mmio, PVIDEO_DT_DY_1,    nvov0->regs.DT_DY_1 );
     }
     if (flags & CLRCF_DEST) {
          nv_out32( mmio, PVIDEO_POINT_OUT_0, nvov0->regs.POINT_OUT_0 );
          nv_out32( mmio, PVIDEO_POINT_OUT_1, nvov0->regs.POINT_OUT_1 );
          nv_out32( mmio, PVIDEO_SIZE_OUT_0,  nvov0->regs.SIZE_OUT_0 );
          nv_out32( mmio, PVIDEO_SIZE_OUT_1,  nvov0->regs.SIZE_OUT_1 );
     }
     if (flags & (CLRCF_FORMAT | CLRCF_SURFACE | CLRCF_OPTIONS)) {
          nv_out32( mmio, PVIDEO_FORMAT_0, nvov0->regs.FORMAT_0 );
          nv_out32( mmio, PVIDEO_FORMAT_1, nvov0->regs.FORMAT_1 );
     }
     nv_out32( mmio, PVIDEO_BUFFER, nvov0->regs.BUFFER );
     nv_out32( mmio, PVIDEO_STOP,   nvov0->regs.STOP );
}

static void
ov0_calc_regs( NVidiaDriverData           *nvdrv,
               NVidiaOverlayLayerData     *nvov0,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags  flags )
{
     NVidiaDeviceData *nvdev = nvdrv->device_data;
 
     if (flags & (CLRCF_WIDTH  | CLRCF_HEIGHT | 
                  CLRCF_SOURCE | CLRCF_DEST   | CLRCF_OPTIONS)) {
          int          width  = config->width;
          int          height = config->height;
          DFBRectangle source = config->source;
          DFBRectangle dest   = config->dest;
     
          source.x <<= 4;
          source.y <<= 4;
     
          if (dest.x < 0) {
               source.x -= (dest.x * source.w << 4) / dest.w;
               source.w += dest.x * source.w / dest.w;
               dest.w   += dest.x;
               dest.x    = 0;
          }

          if (dest.y < 0) {
               source.y -= (dest.y * source.h << 4) / dest.h;
               source.h += dest.y * source.h / dest.h;
               dest.h   += dest.y;
               dest.y    = 0;
          }

          if (config->options & DLOP_DEINTERLACING) {
               height   /= 2;
               source.y /= 2;
               source.h /= 2;
          }

          if (source.w < 1 || source.h < 1 || dest.w < 1 || dest.h < 1) {
               nvov0->regs.STOP = PVIDEO_STOP_OVERLAY_ACTIVE |
                                  PVIDEO_STOP_METHOD_NORMALLY;
               return;
          }
          
          nvov0->regs.SIZE_IN_0   = 
          nvov0->regs.SIZE_IN_1   = ((height << 16) & PVIDEO_SIZE_IN_HEIGHT_MSK) |
                                    ( width         & PVIDEO_SIZE_IN_WIDTH_MSK);
          nvov0->regs.POINT_IN_0  =
          nvov0->regs.POINT_IN_1  = ((source.y << 16) & PVIDEO_POINT_IN_T_MSK) |
                                    ( source.x        & PVIDEO_POINT_IN_S_MSK);
          nvov0->regs.DS_DX_0     =
          nvov0->regs.DS_DX_1     = (source.w << 20) / dest.w;
          nvov0->regs.DT_DY_0     =
          nvov0->regs.DT_DY_1     = (source.h << 20) / dest.h;
          nvov0->regs.POINT_OUT_0 =
          nvov0->regs.POINT_OUT_1 = ((dest.y << 16) & PVIDEO_POINT_OUT_Y_MSK) |
                                    ( dest.x        & PVIDEO_POINT_OUT_X_MSK);
          nvov0->regs.SIZE_OUT_0  =
          nvov0->regs.SIZE_OUT_1  = ((dest.h << 16) & PVIDEO_SIZE_OUT_HEIGHT_MSK) |
                                    ( dest.w        & PVIDEO_SIZE_OUT_WIDTH_MSK);
     }
     
     if (flags & (CLRCF_SURFACE | CLRCF_FORMAT | CLRCF_OPTIONS)) {
          CoreSurfaceBufferLock *lock = nvov0->lock;
          u32                    format;
          
          if (config->options & DLOP_DEINTERLACING)
               format = (lock->pitch*2) & PVIDEO_FORMAT_PITCH_MSK;
          else
               format =  lock->pitch    & PVIDEO_FORMAT_PITCH_MSK;
               
          if (DFB_PLANAR_PIXELFORMAT(config->format))
               format |= PVIDEO_FORMAT_PLANAR_NV;
    
          if (config->format == DSPF_UYVY)
               format |= PVIDEO_FORMAT_COLOR_YB8CR8YA8CB8;
          else
               format |= PVIDEO_FORMAT_COLOR_CR8YB8CB8YA8;

          if (config->options & DLOP_DST_COLORKEY)
               format |= PVIDEO_FORMAT_DISPLAY_COLOR_KEY_EQUAL;
               
          /* Use Buffer 0 for Odd field */
          nvov0->regs.OFFSET_0   = (nvdev->fb_offset + lock->offset) & PVIDEO_OFFSET_MSK;
          /* Use Buffer 1 for Even field */
          nvov0->regs.OFFSET_1   = nvov0->regs.OFFSET_0 + lock->pitch;
          if (DFB_PLANAR_PIXELFORMAT(config->format)) {
               CoreSurface *surface = nvov0->videoSurface;
               nvov0->regs.UVOFFSET_0 = (nvov0->regs.OFFSET_0 + 
                                         lock->pitch * surface->config.size.h) & PVIDEO_UVOFFSET_MSK;
               nvov0->regs.UVOFFSET_1 = nvov0->regs.UVOFFSET_0 + lock->pitch;
          }
          nvov0->regs.FORMAT_0 = 
          nvov0->regs.FORMAT_1 = format;
     }
 
     nvov0->regs.BUFFER = 1 << (nvov0->field << 2);
     nvov0->regs.STOP   = (config->opacity)
                          ? PVIDEO_STOP_OVERLAY_INACTIVE
                          : PVIDEO_STOP_OVERLAY_ACTIVE;
     nvov0->regs.STOP  |= PVIDEO_STOP_METHOD_NORMALLY;
}

static void
ov0_set_colorkey( NVidiaDriverData       *nvdrv,
                  NVidiaOverlayLayerData *nvov0,
                  CoreLayerRegionConfig  *config )
{
     u32 key;
     
     key = dfb_color_to_pixel( dfb_primary_layer_pixelformat(),
                               config->dst_key.r,
                               config->dst_key.g,
                               config->dst_key.b );
   
     nv_out32( nvdrv->mmio_base, PVIDEO_COLOR_KEY, key );
}

static void
ov0_set_csc( NVidiaDriverData       *nvdrv,
             NVidiaOverlayLayerData *nvov0 )
{
     volatile u8 *mmio = nvdrv->mmio_base;
     s32          satSine;
     s32          satCosine;
     double       angle;

     angle = (double) nvov0->hue * M_PI / 180.0;
     satSine = nvov0->saturation * sin(angle);
     if (satSine < -1024)
          satSine = -1024;
     satCosine = nvov0->saturation * cos(angle);
     if (satCosine < -1024)
          satCosine = -1024;

     nv_out32( mmio, PVIDEO_LUMINANCE_0, (nvov0->brightness << 16) |
                                         (nvov0->contrast & 0xffff) );
     nv_out32( mmio, PVIDEO_LUMINANCE_1, (nvov0->brightness << 16) |
                                         (nvov0->contrast & 0xffff) );
     nv_out32( mmio, PVIDEO_CHROMINANCE_0, (satSine << 16) |
                                           (satCosine & 0xffff) );
     nv_out32( mmio, PVIDEO_CHROMINANCE_1, (satSine << 16) |
                                           (satCosine & 0xffff) );
}

