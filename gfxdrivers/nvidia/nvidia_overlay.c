/*
   Written by Oliver Schwartz <Oliver.Schwartz@gmx.de> and
              Claudio Ciccani <klan82@cheapnet.it>.

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
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>
#include <core/system.h>
#include <core/layer_control.h>

#include <gfx/convert.h>

#include <direct/memcpy.h>
#include <direct/messages.h>

#include "nvidia.h"
#include "nvidia_mmio.h"


typedef struct {
     CoreLayerRegionConfig config;
     CoreSurface*          videoSurface;
     short                 brightness;
     short                 contrast;
     short                 saturation;
     short                 hue;
     __u32                 colorkey;
     int                   buffer;

     struct {
          __u32 NV_PVIDEO_BUFFER;      // 0x700
          __u32 NV_PVIDEO_STOP;        // 0x704
          __u32 NV_PVIDEO_BASE;        // 0x900
          __u32 NV_PVIDEO_SIZE_IN;     // 0x928
          __u32 NV_PVIDEO_POINT_IN;    // 0x930
          __u32 NV_PVIDEO_DS_DX;       // 0x938
          __u32 NV_PVIDEO_DT_DY;       // 0x940
          __u32 NV_PVIDEO_POINT_OUT;   // 0x948
          __u32 NV_PVIDEO_SIZE_OUT;    // 0x950
          __u32 NV_PVIDEO_FORMAT;      // 0x958
     } regs;
} NVidiaOverlayLayerData;

static void ov0_set_regs ( NVidiaDriverData       *nvdrv,
                           NVidiaOverlayLayerData *nvov0 );
static void ov0_calc_regs( NVidiaDriverData       *nvdrv,
                           NVidiaOverlayLayerData *nvov0,
                           CoreLayer              *layer,
                           CoreLayerRegionConfig  *config );
static void ov0_set_csc  ( NVidiaDriverData       *nvdrv,
                           NVidiaOverlayLayerData *nvov0 );

#define OV0_SUPPORTED_OPTIONS   (DLOP_DST_COLORKEY)

/**********************/



static int
ov0LayerDataSize()
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
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     NVidiaDriverData       *nvdrv = (NVidiaDriverData*) driver_data;
     __u32                   vram  = dfb_gfxcard_memory_length();
     
     /* set capabilities and type */
     description->caps =  DLCAPS_SURFACE      | DLCAPS_SCREEN_LOCATION |
                          DLCAPS_BRIGHTNESS   | DLCAPS_CONTRAST        |
                          DLCAPS_SATURATION   | DLCAPS_HUE             |
                          DLCAPS_DST_COLORKEY;
     description->type = DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "NVidia Overlay" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE |
                           DLCONF_OPTIONS;
     config->width       = 640;
     config->height      = 480;
     config->pixelformat = DSPF_YUY2;
     config->buffermode  = DLBM_BACKSYSTEM;
     config->options     = DLOP_NONE;

     /* fill out default color adjustment,
        only fields set in flags will be accepted from applications */
     adjustment->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST |
                              DCAF_SATURATION | DCAF_HUE;
     adjustment->brightness = 0x8000;
     adjustment->contrast   = 0x8000;
     adjustment->saturation = 0x8000;
     adjustment->hue        = 0x8000;

     /* set video buffers start and limit */
     nvdrv->PVIDEO[0x920/4] = 0;
     nvdrv->PVIDEO[0x924/4] = 0;
     if (nvdrv->chip != 0x02a0) {
          nvdrv->PVIDEO[0x908/4] = vram - 1;
          nvdrv->PVIDEO[0x90C/4] = vram - 1;
     }
 
     /* reset overlay */
     nvov0->brightness = 0;
     nvov0->contrast   = 4096;
     nvov0->saturation = 4096;
     nvov0->hue        = 0;
     nvov0->colorkey   = 0;
     ov0_set_csc( nvdrv, nvov0 );

     return DFB_OK;
}

static void
ov0OnOff( NVidiaDriverData       *nvdrv,
          NVidiaOverlayLayerData *nvov0,
          int                    on )
{
     if (on)
          nvov0->regs.NV_PVIDEO_STOP = 0;
     else
          nvov0->regs.NV_PVIDEO_STOP = 1;

     nvdrv->PVIDEO[0x704/4] = nvov0->regs.NV_PVIDEO_STOP;
}


static DFBResult
ov0Remove( CoreLayer *layer,
           void      *driver_data,
           void      *layer_data,
           void      *region_data )
{
     NVidiaDriverData    *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;

     /* disable overlay */
     ov0OnOff(nvdrv, nvov0, 0);

     return DFB_OK;
}


static DFBResult
ov0TestRegion(CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     /* check for unsupported options */
     if (config->options & ~OV0_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     /* check buffermode */
     if (config->buffermode == DLBM_WINDOWS)
          fail |= CLRCF_BUFFERMODE;

     /* check pixel format */
     switch (config->format) {
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               break;

          default:
               fail |= CLRCF_FORMAT;
               break;
     }

     /* check width */
     if (config->width > 2048 || config->width < 1)
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
ov0SetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags updated,
              CoreSurface                *surface,
              CorePalette                *palette )
{
     NVidiaDriverData    *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;

     /* remember configuration */
     nvov0->config  = *config;

     ov0_calc_regs( nvdrv, nvov0, layer, config );
     ov0_set_regs( nvdrv, nvov0 );

     /* set destination colorkey */
     if (config->options & DLOP_DST_COLORKEY) {
          nvov0->colorkey = dfb_color_to_pixel( dfb_primary_layer_pixelformat(),
                                                config->dst_key.r, config->dst_key.g,
                                                config->dst_key.b );
          ov0_set_csc( nvdrv, nvov0 );
     }

     switch (config->opacity) {
          case 0:
               ov0OnOff( nvdrv, nvov0, 0 );
          break;
          default:
               ov0OnOff( nvdrv, nvov0, 1 );
          break;
     }

     /* enable overlay */
     ov0OnOff(nvdrv, nvov0, 1);

     return DFB_OK;
}


static DFBResult
ov0AllocateSurface( CoreLayer              *layer,
                    void                   *driver_data,
                    void                   *layer_data,
                    void                   *region_data,
                    CoreLayerRegionConfig  *config,
                    CoreSurface           **surface )
{
     DFBResult               result;
     NVidiaOverlayLayerData *nvov0     = (NVidiaOverlayLayerData*) layer_data;
     DFBSurfacePixelFormat   format;
     DFBSurfaceCapabilities  caps      = DSCAPS_VIDEOONLY;
     __u32                   dst_width;
     __u32                   src_width;

     switch (config->buffermode) {
          case DLBM_FRONTONLY:
          case DLBM_BACKSYSTEM:
               break;

          case DLBM_BACKVIDEO:
               caps |= DSCAPS_DOUBLE;
               break;

          case DLBM_TRIPLE:
               caps |= DSCAPS_TRIPLE;
               break;

          default:
               D_BUG("unknown buffermode");
               return DFB_BUG;
     }

     switch (config->format) {
          case DSPF_YUY2:
          case DSPF_UYVY:
               format    = config->format;
               src_width = config->width;
               break;

          case DSPF_I420:
          case DSPF_YV12:
               format    = DSPF_YUY2;
               src_width = (config->width + 7) & ~7;
               break;

          default:
               D_BUG("unexpected pixelformat");
               return DFB_BUG;
     }

     dst_width = (config->width + 31) & ~31;

     result = dfb_surface_create(NULL, dst_width, config->height,
                                 format, CSP_VIDEOONLY, caps,
                                 NULL, &nvov0->videoSurface );
     if (result != DFB_OK)
          return result;

     result = dfb_surface_create( NULL, src_width, config->height,
                                  config->format, CSP_SYSTEMONLY,
                                  DSCAPS_SYSTEMONLY, NULL, surface );
     return result;
}

static DFBResult
ov0ReallocateSurface( CoreLayer             *layer,
                      void                  *driver_data,
                      void                  *layer_data,
                      void                  *region_data,
                      CoreLayerRegionConfig *config,
                      CoreSurface           *surface )
{
     DFBResult               result;
     NVidiaOverlayLayerData *nvov0        = (NVidiaOverlayLayerData*) layer_data;
     CoreSurface            *videoSurface = nvov0->videoSurface;
     DFBSurfacePixelFormat   format;
     __u32                   dst_width;
     __u32                   src_width;

     switch (config->buffermode) {
          case DLBM_FRONTONLY:
          case DLBM_BACKSYSTEM:
               videoSurface->caps &= ~DSCAPS_FLIPPING;
               break;

          case DLBM_BACKVIDEO:
               videoSurface->caps &= ~DSCAPS_FLIPPING;
               videoSurface->caps |=  DSCAPS_DOUBLE;
               break;

          case DLBM_TRIPLE:
               videoSurface->caps &= ~DSCAPS_FLIPPING;
               videoSurface->caps |=  DSCAPS_TRIPLE;
               break;

          default:
               D_BUG("unknown buffermode");
               return DFB_BUG;
     }

     switch (config->format) {
          case DSPF_YUY2:
          case DSPF_UYVY:
               format    = config->format;
               src_width = config->width;
               break;

          case DSPF_I420:
          case DSPF_YV12:
               format    = DSPF_YUY2;
               src_width = (config->width + 7) & ~7;
               break;

          default:
               D_BUG("unexpected pixelformat");
               return DFB_BUG;
     }

     dst_width = (config->width + 31) & ~31;

     D_DEBUG("DirectFB/NVidia/Overlay: Reallocate %d kBytes for video surface\n",
                                    dst_width * config->height *
                                    DFB_BYTES_PER_PIXEL(format) / 1024);

     result = dfb_surface_reconfig( videoSurface,
                                    CSP_VIDEOONLY, CSP_VIDEOONLY );
     if (result != DFB_OK)
          return result;

     result = dfb_surface_reformat( NULL, videoSurface, dst_width,
                                    config->height, format );
     if (result != DFB_OK)
          return result;

     result = dfb_surface_reformat( NULL, surface, src_width,
                                    config->height, config->format );

     return result;
}

static DFBResult
ov0DeallocateSurface( CoreLayer   *layer,
                      void        *driver_data,
                      void        *layer_data,
                      void        *region_data,
                      CoreSurface *surface )
{
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     dfb_surface_unref( nvov0->videoSurface );
     dfb_surface_unref( surface );
     return DFB_OK;
}

/*
 * CopyData
 */
static void
ov0CopyData422
(
  __u8 *src,
  __u8 *dst,
  int   srcPitch,
  int   dstPitch,
  int   h,
  int   w
)
{
    w <<= 1;
    while(h--)
    {
        direct_memcpy(dst, src, w);
        src += srcPitch;
        dst += dstPitch;
    }
}

/*
 * CopyMungedData
 */
static void
ov0CopyData420
(
     __u8 *src1,
     __u8 *src2,
     __u8 *src3,
     __u8 *dst1,
     int   srcPitch,
     int   srcPitch2,
     int   dstPitch,
     int   h,
     int   w
)
{
     __u32 *dst;
     __u8 *s1, *s2, *s3;
     int i, j;

     w >>= 1;

     for(j = 0; j < h; j++) {
          dst = (__u32 *)dst1;
          s1 = src1;  s2 = src2;  s3 = src3;
          i = w;
          while(i > 4) {
#ifdef WORDS_BIGENDIAN
               dst[0] = (s1[0] << 24) | (s1[1] << 8) | (s3[0] << 16) | s2[0];
               dst[1] = (s1[2] << 24) | (s1[3] << 8) | (s3[1] << 16) | s2[1];
               dst[2] = (s1[4] << 24) | (s1[5] << 8) | (s3[2] << 16) | s2[2];
               dst[3] = (s1[6] << 24) | (s1[7] << 8) | (s3[3] << 16) | s2[3];
#else
               dst[0] = s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24);
               dst[1] = s1[2] | (s1[3] << 16) | (s3[1] << 8) | (s2[1] << 24);
               dst[2] = s1[4] | (s1[5] << 16) | (s3[2] << 8) | (s2[2] << 24);
               dst[3] = s1[6] | (s1[7] << 16) | (s3[3] << 8) | (s2[3] << 24);
#endif
               dst += 4; s2 += 4; s3 += 4; s1 += 8;
               i -= 4;
          }

          while(i--) {
#ifdef WORDS_BIGENDIAN
               dst[0] = (s1[0] << 24) | (s1[1] << 8) | (s3[0] << 16) | s2[0];
#else
               dst[0] = s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24);
#endif
               dst++; s2++; s3++;
               s1 += 2;
          }

          dst1 += dstPitch;
          src1 += srcPitch;
          if(j & 1) {
               src2 += srcPitch2;
               src3 += srcPitch2;
          }
     }
}


static DFBResult
ov0FlipRegion ( CoreLayer           *layer,
                void                *driver_data,
                void                *layer_data,
                void                *region_data,
                CoreSurface         *surface,
                DFBSurfaceFlipFlags  flags )


{
     NVidiaDriverData    *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     __u32 srcPitch, srcPitch2, dstPitch, s2offset, s3offset, tmp;
     __u32 width = nvov0->config.width;
     __u32 height = nvov0->config.height;
     __u8 *dstStart;
     __u8 *buf;
     SurfaceBuffer *data_buffer;
     SurfaceBuffer *video_buffer;

     dfb_surface_flip_buffers( nvov0->videoSurface, false );

     data_buffer = surface->front_buffer;
     video_buffer = nvov0->videoSurface->front_buffer;

     dstPitch = video_buffer->video.pitch;
     srcPitch = data_buffer->system.pitch;

     buf = data_buffer->system.addr;
     dstStart = (__u8*)dfb_system_video_memory_virtual(video_buffer->video.offset);

     switch(nvov0->config.format) {
          case DSPF_YV12:
          case DSPF_I420:
               s2offset = srcPitch * height;
               srcPitch2 = srcPitch >> 1;
               s3offset = (srcPitch2 * (height >> 1)) + s2offset;
               if(nvov0->config.format == DSPF_I420) {
                    tmp = s2offset;
                    s2offset = s3offset;
                    s3offset = tmp;
               }
               ov0CopyData420(buf, buf + s2offset,
                             buf + s3offset, dstStart, srcPitch, srcPitch2,
                             dstPitch, height, width);
               break;
          case DSPF_UYVY:
          case DSPF_YUY2:
          default:
               ov0CopyData422(buf, dstStart, srcPitch, dstPitch, height, width);
               break;
     }

     nvov0->buffer ^= 1;
     ov0_calc_regs( nvdrv, nvov0, layer, &nvov0->config );
     ov0_set_regs( nvdrv, nvov0 );

     if (flags & DSFLIP_WAIT)
          dfb_layer_wait_vsync( layer );
     return DFB_OK;
}


static DFBResult
ov0UpdateRegion ( CoreLayer           *layer,
                  void                *driver_data,
                  void                *layer_data,
                  void                *region_data,
                  CoreSurface         *surface,
                  const DFBRegion     *update )

{
     NVidiaDriverData    *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     __u32 srcPitch, srcPitch2, dstPitch, s2offset, s3offset, tmp;
     __u32 width = nvov0->config.width;
     __u32 height = nvov0->config.height;
     __u8 *dstStart;
     __u8 *buf;
     SurfaceBuffer *data_buffer;
     SurfaceBuffer *video_buffer;

     data_buffer = surface->front_buffer;
     video_buffer = nvov0->videoSurface->front_buffer;

     dstPitch = video_buffer->video.pitch;
     srcPitch = data_buffer->system.pitch;

     buf = data_buffer->system.addr;
     dstStart = (__u8*)dfb_system_video_memory_virtual(video_buffer->video.offset);

     switch(nvov0->config.format) {
          case DSPF_YV12:
          case DSPF_I420:
               s2offset = srcPitch * height;
               srcPitch2 = srcPitch >> 1;
               s3offset = (srcPitch2 * (height >> 1)) + s2offset;
               if(nvov0->config.format == DSPF_I420) {
                    tmp = s2offset;
                    s2offset = s3offset;
                    s3offset = tmp;
               }
               ov0CopyData420(buf, buf + s2offset,
                             buf + s3offset, dstStart, srcPitch, srcPitch2,
                             dstPitch, height, width);
               break;
          case DSPF_UYVY:
          case DSPF_YUY2:
          default:
               ov0CopyData422(buf, dstStart, srcPitch, dstPitch, height, width);
               break;
     }

     nvov0->buffer ^= 1;
     ov0_calc_regs( nvdrv, nvov0, layer, &nvov0->config );
     ov0_set_regs( nvdrv, nvov0 );

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


DisplayLayerFuncs nvidiaOverlayFuncs = {
     .LayerDataSize      = ov0LayerDataSize,
     .InitLayer          = ov0InitLayer,
     .SetRegion          = ov0SetRegion,
     .RemoveRegion       = ov0Remove,
     .TestRegion         = ov0TestRegion,
     .FlipRegion         = ov0FlipRegion,
     .UpdateRegion       = ov0UpdateRegion,
     .SetColorAdjustment = ov0SetColorAdjustment,
     .AllocateSurface    = ov0AllocateSurface,
     .DeallocateSurface  = ov0DeallocateSurface,
     .ReallocateSurface  = ov0ReallocateSurface
};


/* internal */

static void ov0_set_regs( NVidiaDriverData *nvdrv, NVidiaOverlayLayerData *nvov0 )
{
     nvdrv->PVIDEO[(0x900/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_BASE;
     nvdrv->PVIDEO[(0x928/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_SIZE_IN;
     nvdrv->PVIDEO[(0x930/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_POINT_IN;
     nvdrv->PVIDEO[(0x938/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_DS_DX;
     nvdrv->PVIDEO[(0x940/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_DT_DY;
     nvdrv->PVIDEO[(0x948/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_POINT_OUT;
     nvdrv->PVIDEO[(0x950/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_SIZE_OUT;
     nvdrv->PVIDEO[(0x958/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_FORMAT;

     nvdrv->PVIDEO[0x704/4] = nvov0->regs.NV_PVIDEO_STOP;
     nvdrv->PVIDEO[0x700/4] = nvov0->regs.NV_PVIDEO_BUFFER;
}

static void
ov0_calc_regs( NVidiaDriverData       *nvdrv,
               NVidiaOverlayLayerData *nvov0,
               CoreLayer              *layer,
               CoreLayerRegionConfig  *config )
{
     CoreSurface   *surface = nvov0->videoSurface;
     SurfaceBuffer *buffer  = surface->front_buffer;
     __u32          pitch   = buffer->video.pitch;

     if (nvdrv->chip == 0x2a0) /* GeForce3 XBox */
          nvov0->regs.NV_PVIDEO_BASE = (nvdrv->fb_base + buffer->video.offset)
                                        & nvdrv->fb_mask;
     else
          nvov0->regs.NV_PVIDEO_BASE = buffer->video.offset & nvdrv->fb_mask;

     nvov0->regs.NV_PVIDEO_SIZE_IN   = (config->height << 16)   | (config->width & 0xffff);
     nvov0->regs.NV_PVIDEO_POINT_IN  = (config->source.y << 20) | ((config->source.x << 4) & 0xffff);
     nvov0->regs.NV_PVIDEO_DS_DX     = (config->source.w << 20) / config->dest.w;
     nvov0->regs.NV_PVIDEO_DT_DY     = (config->source.h << 20) / config->dest.h;
     nvov0->regs.NV_PVIDEO_POINT_OUT = (config->dest.y << 16)   | (config->dest.x & 0xffff);
     nvov0->regs.NV_PVIDEO_SIZE_OUT  = (config->dest.h << 16)   | (config->dest.w & 0xffff);

     if(config->format != DSPF_UYVY)
          pitch |= 1 << 16;

     if (config->options & DLOP_DST_COLORKEY)
          pitch |= 1 << 20;   /* use color key */

     nvov0->regs.NV_PVIDEO_FORMAT = pitch;
     nvov0->regs.NV_PVIDEO_BUFFER = 1 << (nvov0->buffer << 2);
}

static void
ov0_set_csc( NVidiaDriverData       *nvdrv,
             NVidiaOverlayLayerData *nvov0 )
{
     __s32  satSine, satCosine;
     double angle;

     angle = (double) nvov0->hue * M_PI / 180.0;
     satSine = nvov0->saturation * sin(angle);
     if (satSine < -1024)
          satSine = -1024;
     satCosine = nvov0->saturation * cos(angle);
     if (satCosine < -1024)
          satCosine = -1024;

     nvdrv->PVIDEO[0x910/4] = (nvov0->brightness << 16) | nvov0->contrast;
     nvdrv->PVIDEO[0x914/4] = (nvov0->brightness << 16) | nvov0->contrast;
     nvdrv->PVIDEO[0x918/4] = (satSine << 16) | (satCosine & 0xffff);
     nvdrv->PVIDEO[0x91c/4] = (satSine << 16) | (satCosine & 0xffff);
     nvdrv->PVIDEO[0xb00/4] = nvov0->colorkey;
}

