/*
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
#include <core/surfaces.h>
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
     CoreLayerRegionConfig config;
     CoreSurface*          videoSurface;
     short                 brightness;
     short                 contrast;
     short                 hue;
     short                 saturation;
     int                   field;

     struct {
          __u32 BUFFER;
          __u32 STOP;
          __u32 BASE_0;
          __u32 BASE_1;
          __u32 SIZE_IN_0;
          __u32 SIZE_IN_1;
          __u32 POINT_IN_0;
          __u32 POINT_IN_1;
          __u32 DS_DX_0;
          __u32 DS_DX_1;
          __u32 DT_DY_0;
          __u32 DT_DY_1;
          __u32 POINT_OUT_0;
          __u32 POINT_OUT_1;
          __u32 SIZE_OUT_0;
          __u32 SIZE_OUT_1;
          __u32 FORMAT_0;
          __u32 FORMAT_1;
     } regs;
} NVidiaOverlayLayerData;

static void ov0_set_regs    ( NVidiaDriverData       *nvdrv,
                              NVidiaOverlayLayerData *nvov0 );
static void ov0_calc_regs   ( NVidiaDriverData       *nvdrv,
                              NVidiaOverlayLayerData *nvov0,
                              CoreLayerRegionConfig  *config );
static void ov0_set_colorkey( NVidiaDriverData       *nvdrv,
                              NVidiaOverlayLayerData *nvov0,
                              CoreLayerRegionConfig  *config );
static void ov0_set_csc     ( NVidiaDriverData       *nvdrv,
                              NVidiaOverlayLayerData *nvov0 );

#define OV0_SUPPORTED_OPTIONS \
     ( DLOP_DST_COLORKEY | DLOP_DEINTERLACING )

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
     volatile __u8    *mmio  = nvdrv->mmio_base;
     
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
              CorePalette                *palette )
{
     NVidiaDriverData       *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     
     /* remember configuration */
     nvov0->config = *config;

     /* set configuration */
     if (updated & (CLRCF_WIDTH  | CLRCF_HEIGHT | CLRCF_FORMAT  |
                    CLRCF_SOURCE | CLRCF_DEST   | CLRCF_OPTIONS | CLRCF_OPACITY))
     {
          ov0_calc_regs( nvdrv, nvov0, config );
          ov0_set_regs( nvdrv, nvov0 );
     }

     /* set destination colorkey */
     if (updated & CLRCF_DSTKEY)
          ov0_set_colorkey( nvdrv, nvov0, config );

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
     NVidiaOverlayLayerData *nvov0        = (NVidiaOverlayLayerData*) layer_data;
     DFBSurfaceCapabilities  caps         = DSCAPS_VIDEOONLY;
     CoreSurfacePolicy       front_policy = CSP_VIDEOONLY;
     CoreSurfacePolicy       back_policy  = CSP_VIDEOONLY;
     DFBResult               result;
      
     switch (config->buffermode) {
          case DLBM_FRONTONLY:
               break;
               
          case DLBM_BACKSYSTEM:
               back_policy = CSP_SYSTEMONLY;
               /* fall through */
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
               break;
               
          case DSPF_I420:
          case DSPF_YV12:
               if (config->buffermode == DLBM_BACKSYSTEM)
                    caps &= ~DSCAPS_DOUBLE;
               break;
               
          default:
               D_BUG( "unexpected pixelformat" );
               return DFB_BUG;
     }

     if (DFB_PLANAR_PIXELFORMAT( config->format )) {
          result = dfb_surface_create( layer->core, 
                                       config->width, config->height,
                                       DSPF_YUY2, CSP_VIDEOONLY, caps,
                                       NULL, &nvov0->videoSurface );
          
          if (result == DFB_OK) {
               result = dfb_surface_create( layer->core,
                                            config->width, config->height,
                                            config->format, CSP_SYSTEMONLY,
                                            DSCAPS_SYSTEMONLY, NULL, surface );
          }
     } else {
          result = dfb_surface_create( layer->core,
                                       config->width, config->height,
                                       config->format, CSP_VIDEOONLY, caps, 
                                       NULL, surface );
          
          if (result == DFB_OK) {
               dfb_surface_ref( *surface );
               nvov0->videoSurface = *surface;

               if (front_policy != back_policy)
                    result = dfb_surface_reconfig( *surface,
                                                   front_policy, back_policy );
          }
     }

     if (config->options & DLOP_DEINTERLACING)
          (*surface)->caps |= DSCAPS_INTERLACED;
          
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
     NVidiaOverlayLayerData *nvov0        = (NVidiaOverlayLayerData*) layer_data;
     DFBSurfaceCapabilities  caps         = DSCAPS_VIDEOONLY; 
     CoreSurfacePolicy       front_policy = CSP_VIDEOONLY;
     CoreSurfacePolicy       back_policy  = CSP_VIDEOONLY;
     DFBResult               result;

     switch (config->buffermode) {
          case DLBM_FRONTONLY:
               break;
               
          case DLBM_BACKSYSTEM:
               back_policy = CSP_SYSTEMONLY;
               /* fall through */
          case DLBM_BACKVIDEO:
               caps |=  DSCAPS_DOUBLE;
               break;

          case DLBM_TRIPLE:
               caps |=  DSCAPS_TRIPLE;
               break;

          default:
               D_BUG("unknown buffermode");
               return DFB_BUG;
     }
     
     switch (config->format) {
          case DSPF_YUY2:
          case DSPF_UYVY:
               surface->caps = caps;
               break;
               
          case DSPF_I420:
          case DSPF_YV12:
               front_policy  = CSP_SYSTEMONLY;
               back_policy   = CSP_SYSTEMONLY;
               surface->caps = DSCAPS_SYSTEMONLY;
               if (config->buffermode == DLBM_BACKSYSTEM)
                    caps &= ~DSCAPS_DOUBLE;
               break;

          default:
               D_BUG("unexpected pixelformat");
               return DFB_BUG;
     }

     dfb_surface_unref( nvov0->videoSurface );
     nvov0->videoSurface = NULL;

     result = dfb_surface_reconfig( surface, front_policy, back_policy );
     if (result == DFB_OK)     
          result = dfb_surface_reformat( layer->core, surface, config->width,
                                         config->height, config->format );
     if (result != DFB_OK)
          return result;

     if (DFB_PLANAR_PIXELFORMAT( config->format )) {
          result = dfb_surface_create( layer->core,
                                       config->width, config->height,
                                       DSPF_YUY2, CSP_VIDEOONLY, caps,
                                       NULL, &nvov0->videoSurface );
     } else {
          dfb_surface_ref( surface );
          nvov0->videoSurface = surface;
     }

     if (config->options & DLOP_DEINTERLACING)
          surface->caps |= DSCAPS_INTERLACED;
     
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
     
     if (nvov0->videoSurface)
          dfb_surface_unref( nvov0->videoSurface );
     dfb_surface_unref( surface );
     
     return DFB_OK;
}

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
     NVidiaDriverData       *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     
     dfb_surface_flip_buffers( nvov0->videoSurface, false );

     if (DFB_PLANAR_PIXELFORMAT( surface->format )) {  
          SurfaceBuffer *data_buffer;
          SurfaceBuffer *video_buffer;
          __u32 srcPitch, srcPitch2, dstPitch, s2offset, s3offset, tmp;
          __u32 width  = surface->width;
          __u32 height = surface->height;
          __u8 *dstStart;
          __u8 *buf;
          
          data_buffer  = surface->front_buffer;
          video_buffer = nvov0->videoSurface->front_buffer;

          buf      = data_buffer->system.addr;
          dstStart = dfb_gfxcard_memory_virtual( nvdrv->device, 
                                                 video_buffer->video.offset);
          
          dstPitch  = video_buffer->video.pitch;
          srcPitch  = data_buffer->system.pitch;
          srcPitch2 = srcPitch >> 1;

          s2offset = srcPitch * height;
          s3offset = (srcPitch2 * (height >> 1)) + s2offset;
          if (nvov0->config.format == DSPF_I420) {
               tmp = s2offset;
               s2offset = s3offset;
               s3offset = tmp;
          }
          
          ov0CopyData420( buf, buf + s2offset,
                          buf + s3offset, dstStart, srcPitch, srcPitch2,
                          dstPitch, height, width );
     }

     ov0_calc_regs( nvdrv, nvov0, &nvov0->config );
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
     NVidiaDriverData       *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;

     if (DFB_PLANAR_PIXELFORMAT( surface->format )) {  
          SurfaceBuffer *data_buffer;
          SurfaceBuffer *video_buffer;
          __u32 srcPitch, srcPitch2, dstPitch, s2offset, s3offset, tmp;
          __u32 width  = surface->width;
          __u32 height = surface->height;
          __u8 *dstStart;
          __u8 *buf;
          
          data_buffer  = surface->front_buffer;
          video_buffer = nvov0->videoSurface->front_buffer;

          buf      = data_buffer->system.addr;
          dstStart = dfb_gfxcard_memory_virtual( nvdrv->device,
                                                 video_buffer->video.offset );
          
          dstPitch  = video_buffer->video.pitch;
          srcPitch  = data_buffer->system.pitch;
          srcPitch2 = srcPitch >> 1;

          s2offset = srcPitch * height;
          s3offset = (srcPitch2 * (height >> 1)) + s2offset;
          if(nvov0->config.format == DSPF_I420) {
               tmp = s2offset;
               s2offset = s3offset;
               s3offset = tmp;
          }
          
          ov0CopyData420( buf, buf + s2offset,
                          buf + s3offset, dstStart, srcPitch, srcPitch2,
                          dstPitch, height, width );
     }
     
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
     
     ov0_set_regs( nvdrv, nvov0 );
     
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
     .SetInputField      = ov0SetInputField,
     .AllocateSurface    = ov0AllocateSurface,
     .DeallocateSurface  = ov0DeallocateSurface,
     .ReallocateSurface  = ov0ReallocateSurface
};


/* internal */

static void ov0_set_regs( NVidiaDriverData *nvdrv, NVidiaOverlayLayerData *nvov0 )
{
     volatile __u8 *mmio = nvdrv->mmio_base;
     
     nv_out32( mmio, PVIDEO_BASE_0,      nvov0->regs.BASE_0 );
     nv_out32( mmio, PVIDEO_BASE_1,      nvov0->regs.BASE_1 );
     nv_out32( mmio, PVIDEO_SIZE_IN_0,   nvov0->regs.SIZE_IN_0 );
     nv_out32( mmio, PVIDEO_SIZE_IN_1,   nvov0->regs.SIZE_IN_1 );
     nv_out32( mmio, PVIDEO_POINT_IN_0,  nvov0->regs.POINT_IN_0 );
     nv_out32( mmio, PVIDEO_POINT_IN_1,  nvov0->regs.POINT_IN_1 );
     nv_out32( mmio, PVIDEO_DS_DX_0,     nvov0->regs.DS_DX_0 );
     nv_out32( mmio, PVIDEO_DS_DX_1,     nvov0->regs.DS_DX_1 );
     nv_out32( mmio, PVIDEO_DT_DY_0,     nvov0->regs.DT_DY_0 );
     nv_out32( mmio, PVIDEO_DT_DY_1,     nvov0->regs.DT_DY_1 );
     nv_out32( mmio, PVIDEO_POINT_OUT_0, nvov0->regs.POINT_OUT_0 );
     nv_out32( mmio, PVIDEO_POINT_OUT_1, nvov0->regs.POINT_OUT_1 );
     nv_out32( mmio, PVIDEO_SIZE_OUT_0,  nvov0->regs.SIZE_OUT_0 );
     nv_out32( mmio, PVIDEO_SIZE_OUT_1,  nvov0->regs.SIZE_OUT_1 );
     nv_out32( mmio, PVIDEO_FORMAT_0,    nvov0->regs.FORMAT_0 );
     nv_out32( mmio, PVIDEO_FORMAT_1,    nvov0->regs.FORMAT_1 );

     nv_out32( mmio, PVIDEO_BUFFER, nvov0->regs.BUFFER );
     nv_out32( mmio, PVIDEO_STOP,   nvov0->regs.STOP );
}

static void
ov0_calc_regs( NVidiaDriverData       *nvdrv,
               NVidiaOverlayLayerData *nvov0,
               CoreLayerRegionConfig  *config )
{
     NVidiaDeviceData *nvdev   = nvdrv->device_data;
     DFBRectangle      source  = config->source;
     DFBRectangle      dest    = config->dest;
     SurfaceBuffer    *buffer  = nvov0->videoSurface->front_buffer;
     __u32             offset  = buffer->video.offset;
     __u32             pitch   = buffer->video.pitch;
     int               width   = config->width;
     int               height  = config->height;
     __u32             format;

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

     if (source.w < 1 || source.h < 1 || dest.w < 1 || dest.h < 1) {
          nvov0->regs.BUFFER = 0;
          nvov0->regs.STOP   = PVIDEO_STOP_OVERLAY_ACTIVE |
                               PVIDEO_STOP_METHOD_NORMALLY;
          return;
     }

     if (config->options & DLOP_DEINTERLACING) {
          height   /= 2;
          source.y /= 2;
          source.h /= 2;
          pitch    *= 2;
     }
     
     if (config->format == DSPF_UYVY)
          format = PVIDEO_FORMAT_COLOR_YB8CR8YA8CB8;
     else
          format = PVIDEO_FORMAT_COLOR_CR8YB8CB8YA8;

     if (config->options & DLOP_DST_COLORKEY)
          format |= PVIDEO_FORMAT_DISPLAY_COLOR_KEY_EQUAL;
          
     /* Use Buffer 0 for Odd field */
     nvov0->regs.BASE_0      = (nvdev->fb_offset + offset) & PVIDEO_BASE_MSK;
     nvov0->regs.SIZE_IN_0   = ((height << 16) & PVIDEO_SIZE_IN_HEIGHT_MSK) |
                               ( width         & PVIDEO_SIZE_IN_WIDTH_MSK);
     nvov0->regs.POINT_IN_0  = ((source.y << 16) & PVIDEO_POINT_IN_T_MSK) |
                               ( source.x        & PVIDEO_POINT_IN_S_MSK);
     nvov0->regs.DS_DX_0     = (source.w << 20) / dest.w;
     nvov0->regs.DT_DY_0     = (source.h << 20) / dest.h;
     nvov0->regs.POINT_OUT_0 = ((dest.y << 16) & PVIDEO_POINT_OUT_Y_MSK) |
                               ( dest.x        & PVIDEO_POINT_OUT_X_MSK);
     nvov0->regs.SIZE_OUT_0  = ((dest.h << 16) & PVIDEO_SIZE_OUT_HEIGHT_MSK) |
                               ( dest.w        & PVIDEO_SIZE_OUT_WIDTH_MSK);   
     nvov0->regs.FORMAT_0    = (pitch & PVIDEO_FORMAT_PITCH_MSK) | format; 
     
     /* Use Buffer 1 for Even field */    
     nvov0->regs.BASE_1      = nvov0->regs.BASE_0 + buffer->video.pitch;
     nvov0->regs.SIZE_IN_1   = nvov0->regs.SIZE_IN_0;
     nvov0->regs.POINT_IN_1  = nvov0->regs.POINT_IN_0;
     nvov0->regs.DS_DX_1     = nvov0->regs.DS_DX_0;
     nvov0->regs.DT_DY_1     = nvov0->regs.DT_DY_0;
     nvov0->regs.POINT_OUT_1 = nvov0->regs.POINT_OUT_0;
     nvov0->regs.SIZE_OUT_1  = nvov0->regs.SIZE_OUT_0;
     nvov0->regs.FORMAT_1    = nvov0->regs.FORMAT_0;
 
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
     __u32 key;
     
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
     volatile __u8 *mmio = nvdrv->mmio_base;
     __s32          satSine;
     __s32          satCosine;
     double         angle;

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

