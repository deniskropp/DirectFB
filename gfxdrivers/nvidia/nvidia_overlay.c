/*
   Written by Oliver Schwartz <Oliver.Schwartz@gmx.de>

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

#include <core/coredefs.h>
#include <core/layers.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>
#include <core/system.h>
#include <misc/memcpy.h>

#include "nvidia.h"
#include "math.h"

typedef struct {
     DFBRectangle          dest;
     DFBDisplayLayerConfig config;
     CoreSurface*          videoSurface;
     short                 brightness;
     short                 contrast;
     short                 saturation;
     short                 hue;
     int                   buffer;
     __u32                 fbstart;

     struct {
          __u32 NV_PVIDEO_BUFFER;      // 0x8700
          __u32 NV_PVIDEO_STOP;        // 0x8704
          __u32 NV_PVIDEO_BASE;        // 0x8900
          __u32 NV_PVIDEO_LIMIT;       // 0x8908
          __u32 NV_PVIDEO_LUMINANCE;   // 0x8910
          __u32 NV_PVIDEO_CHROMINANCE; // 0x8918
          __u32 NV_PVIDEO_OFFSET;      // 0x8920
          __u32 NV_PVIDEO_SIZE_IN;     // 0x8928
          __u32 NV_PVIDEO_POINT_IN;    // 0x8930
          __u32 NV_PVIDEO_DS_DX;       // 0x8938
          __u32 NV_PVIDEO_DT_DY;       // 0x8940
          __u32 NV_PVIDEO_POINT_OUT;   // 0x8948
          __u32 NV_PVIDEO_SIZE_OUT;    // 0x8950
          __u32 NV_PVIDEO_FORMAT;      // 0x8958
          __u32 NV_PVIDEO_COLOR_KEY;   // 0x8b00
     } regs;
} NVidiaOverlayLayerData;

static void ov0_set_regs ( NVidiaDriverData       *nvdrv,
                           NVidiaOverlayLayerData *nvov0 );
static void ov0_calc_regs( NVidiaDriverData       *nvdrv,
                           NVidiaOverlayLayerData *nvov0,
                           CoreLayer              *layer,
                           DFBDisplayLayerConfig  *config );

#define OV0_SUPPORTED_OPTIONS   (DLOP_NONE)

/**********************/



static int
ov0LayerDataSize()
{
     return sizeof(NVidiaOverlayLayerData);
}

static DFBResult
ov0InitLayer( GraphicsDevice             *device,
              CoreLayer                  *layer,
              DisplayLayerInfo           *layer_info,
              DFBDisplayLayerConfig      *default_config,
              DFBColorAdjustment         *default_adj,
              void                       *driver_data,
              void                       *layer_data )
{
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     __s32                  satSine, satCosine;
     double                 angle;

     /* set capabilities and type */
     layer_info->desc.caps = DLCAPS_SCREEN_LOCATION | DLCAPS_SURFACE;
     layer_info->desc.type = DLTF_VIDEO | DLTF_STILL_PICTURE;

     /* set name */
     snprintf( layer_info->desc.name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "NVidia Overlay" );

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
     default_adj->flags = DCAF_NONE;


     /* initialize destination rectangle */
     dfb_primary_layer_rectangle( 0.0f, 0.0f, 1.0f, 1.0f, &nvov0->dest );

     /* reset overlay */
     nvov0->brightness           = 0;
     nvov0->contrast             = 4096;
     nvov0->saturation           = 4096;
     nvov0->hue                  = 0;
     angle = (double)nvov0->hue * 3.1415927 / 180.0;
     satSine = nvov0->saturation * sin(angle);
     if (satSine < -1024)
          satSine = -1024;
     satCosine = nvov0->saturation * cos(angle);
     if (satCosine < -1024)
          satCosine = -1024;

     nvov0->regs.NV_PVIDEO_LUMINANCE = (nvov0->brightness << 16) | nvov0->contrast;
     nvov0->regs.NV_PVIDEO_CHROMINANCE = (satSine << 16) | (satCosine & 0xffff);
     nvov0->regs.NV_PVIDEO_COLOR_KEY = 0;

     nvov0->buffer = 0;
     nvov0->fbstart = dfb_gfxcard_memory_physical(device, 0);
     return DFB_OK;
}

static void
ov0OnOff( NVidiaDriverData       *nvdrv,
          NVidiaOverlayLayerData *nvov0,
          int                    on )
{
     if (on)
     {
          nvov0->regs.NV_PVIDEO_STOP = 0;
     }
     else
     {
          nvov0->regs.NV_PVIDEO_STOP = 1;
     }
     nvdrv->PMC[0x00008704/4] = nvov0->regs.NV_PVIDEO_STOP;
}


static DFBResult
ov0Enable( CoreLayer *layer,
           void      *driver_data,
           void      *layer_data )
{
     NVidiaDriverData    *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;

     /* enable overlay */
     ov0OnOff(nvdrv, nvov0, 1);

     return DFB_OK;
}

static DFBResult
ov0Disable( CoreLayer *layer,
            void      *driver_data,
            void      *layer_data )
{
     NVidiaDriverData    *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;

     /* disable overlay */
     ov0OnOff(nvdrv, nvov0, 0);

     return DFB_OK;
}

static DFBResult
ov0TestConfiguration( CoreLayer                  *layer,
                      void                       *driver_data,
                      void                       *layer_data,
                      DFBDisplayLayerConfig      *config,
                      DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags fail = 0;

     /* check for unsupported options */
     if (config->options & ~OV0_SUPPORTED_OPTIONS)
          fail |= DLCONF_OPTIONS;

     /* check pixel format */
     switch (config->pixelformat) {
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_I420:
          case DSPF_YV12:
               break;

          default:
               fail |= DLCONF_PIXELFORMAT;
     }

     /* check width */
     if (config->width > 2048 || config->width < 1)
          fail |= DLCONF_WIDTH;

     /* check height */
     if (config->height > 1024 || config->height < 1)
          fail |= DLCONF_HEIGHT;

     /* write back failing fields */
     if (failed)
          *failed = fail;

     /* return failure if any field failed */
     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
ov0SetConfiguration( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     DFBDisplayLayerConfig *config )
{
     NVidiaDriverData    *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;

     /* remember configuration */
     nvov0->config = *config;

     ov0_calc_regs( nvdrv, nvov0, layer, config );
     ov0_set_regs( nvdrv, nvov0 );

     return DFB_OK;
}

static DFBResult
ov0SetOpacity( CoreLayer *layer,
               void      *driver_data,
               void      *layer_data,
               __u8       opacity )
{
     NVidiaDriverData    *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;

     switch (opacity) {
          case 0:
               ov0OnOff( nvdrv, nvov0, 0 );
               break;
          case 0xFF:
               ov0OnOff( nvdrv, nvov0, 1 );
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
ov0SetScreenLocation( CoreLayer *layer,
                      void      *driver_data,
                      void      *layer_data,
                      float      x,
                      float      y,
                      float      width,
                      float      height )
{
     NVidiaDriverData    *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;

     /* get new destination rectangle */
     dfb_primary_layer_rectangle( x, y, width, height, &nvov0->dest );

     ov0_calc_regs( nvdrv, nvov0, layer, &nvov0->config );
     ov0_set_regs( nvdrv, nvov0 );

     return DFB_OK;
}

static DFBResult
ov0SetDstColorKey( CoreLayer *layer,
                   void      *driver_data,
                   void      *layer_data,
                   __u8       r,
                   __u8       g,
                   __u8       b )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
ov0AllocateSurface( CoreLayer              *layer,
                    void                   *driver_data,
                    void                   *layer_data,
                    DFBDisplayLayerConfig  *config,
                    CoreSurface           **surface )
{
     DFBResult result;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;

     DFBSurfaceCapabilities  caps   = 0;

     /* choose buffermode */
     if (config->flags & DLCONF_BUFFERMODE) {
          switch (config->buffermode) {
               case DLBM_FRONTONLY:
                    break;

               case DLBM_BACKVIDEO:
                    caps |= DSCAPS_FLIPPING;
                    break;

               case DLBM_BACKSYSTEM:
                    ONCE("DLBM_BACKSYSTEM in default config is unimplemented");
                    break;

               default:
                    BUG("unknown buffermode");
                    break;
          }
     }

     result = dfb_surface_create(NULL, config->width, config->height,
                                 config->pixelformat, CSP_VIDEOONLY,
                                 caps | DSCAPS_VIDEOONLY, NULL,
                                 &nvov0->videoSurface);
     if (result == DFB_OK)
     {
          result = dfb_surface_create( NULL, config->width, config->height,
                                       config->pixelformat, CSP_SYSTEMONLY,
                                       caps | DSCAPS_SYSTEMONLY, NULL, surface );
     }
     return result;
}

static DFBResult
ov0ReallocateSurface( CoreLayer             *layer,
                      void                  *driver_data,
                      void                  *layer_data,
                      DFBDisplayLayerConfig *config,
                      CoreSurface           *surface )
{
     DFBResult result;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     __u32 dstPitch;

     switch (config->buffermode) {
          case DLBM_BACKVIDEO:
               DEBUGMSG("Reallocate: BACKVIDEO\n");
               surface->caps |= DSCAPS_FLIPPING;
               nvov0->videoSurface->caps |= DSCAPS_FLIPPING;
               break;
          case DLBM_BACKSYSTEM:
               DEBUGMSG("Reallocate: BACKSYTEM\n");
               surface->caps |= DSCAPS_FLIPPING;
               nvov0->videoSurface->caps |= DSCAPS_FLIPPING;
               break;
          case DLBM_FRONTONLY:
               DEBUGMSG("Reallocate: FRONTONLY\n");
               surface->caps &= ~DSCAPS_FLIPPING;
               nvov0->videoSurface->caps &= ~DSCAPS_FLIPPING;
               break;

          default:
               BUG("unknown buffermode");
               return DFB_BUG;
     }

     result = dfb_surface_reconfig( surface,
                                    CSP_SYSTEMONLY, CSP_SYSTEMONLY );
     if (result)
          return result;

     result = dfb_surface_reconfig( nvov0->videoSurface,
                                    CSP_VIDEOONLY, CSP_VIDEOONLY );
     if (result)
          return result;


     dstPitch = ((config->width << 1) + 63) & ~63;
     DEBUGMSG("Reallocate: %d kBytes\n", dstPitch * config->height *
                                         DFB_BYTES_PER_PIXEL(config->pixelformat) /
                                         1024);
     result = dfb_surface_reformat( NULL, nvov0->videoSurface, dstPitch,
                                    config->height, config->pixelformat );

     if (result == DFB_OK)
     {
          result = dfb_surface_reformat( NULL, surface, config->width,
                                         config->height, config->pixelformat );
     }
     return result;
}

static DFBResult
ov0DeallocateSurface( CoreLayer   *layer,
                      void        *driver_data,
                      void        *layer_data,
                      CoreSurface *surface )
{
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     dfb_surface_unref( nvov0->videoSurface );
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
  int            srcPitch,
  int            dstPitch,
  int            h,
  int            w
)
{
    w <<= 1;
    while(h--)
    {
        dfb_memcpy(dst, src, w);
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
     int            srcPitch,
     int            srcPitch2,
     int            dstPitch,
     int            h,
     int            w
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
ov0FlipBuffers( CoreLayer           *layer,
                void                *driver_data,
                void                *layer_data,
                DFBSurfaceFlipFlags  flags )
{
     NVidiaDriverData    *nvdrv = (NVidiaDriverData*) driver_data;
     NVidiaOverlayLayerData *nvov0 = (NVidiaOverlayLayerData*) layer_data;
     bool onsync  = (flags & DSFLIP_WAITFORSYNC);
     __u32 srcPitch, srcPitch2, dstPitch, s2offset, s3offset, tmp;
     __u32 width = nvov0->config.width;
     __u32 height = nvov0->config.height;
     __u8 *dstStart;
     __u8 *buf;
     SurfaceBuffer *data_buffer;
     SurfaceBuffer *video_buffer;

     CoreSurface   *surface      = dfb_layer_surface( layer );

     dfb_surface_flip_buffers( surface );
     dfb_surface_flip_buffers( nvov0->videoSurface );

     data_buffer = surface->front_buffer;
     video_buffer = nvov0->videoSurface->front_buffer;

     dstPitch = ((width << 1) + 63) & ~63;
     buf = data_buffer->system.addr;
     dstStart = (__u8*)dfb_system_video_memory_virtual(video_buffer->video.offset);
     switch(nvov0->config.pixelformat) {
          case DSPF_YV12:
          case DSPF_I420:
               srcPitch = (width + 3) & ~3;	/* of luma */
               s2offset = srcPitch * height;
               srcPitch2 = ((width >> 1) + 3) & ~3;
               s3offset = (srcPitch2 * (height >> 1)) + s2offset;
               if(nvov0->config.pixelformat == DSPF_I420) {
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
               srcPitch = (width << 1);
               ov0CopyData422(buf, dstStart, srcPitch, dstPitch, height, width);
               break;
     }
     nvov0->buffer ^= 1;
     ov0_calc_regs( nvdrv, nvov0, layer, &nvov0->config );
     ov0_set_regs( nvdrv, nvov0 );

     if (onsync)
          dfb_layer_wait_vsync( layer );
     return DFB_OK;
}

static DFBResult
ov0SetColorAdjustment( CoreLayer          *layer,
                       void               *driver_data,
                       void               *layer_data,
                       DFBColorAdjustment *adj )
{
     return DFB_UNIMPLEMENTED;
}


DisplayLayerFuncs nvidiaOverlayFuncs = {
     LayerDataSize:      ov0LayerDataSize,
     InitLayer:          ov0InitLayer,
     Enable:             ov0Enable,
     Disable:            ov0Disable,
     TestConfiguration:  ov0TestConfiguration,
     SetConfiguration:   ov0SetConfiguration,
     SetOpacity:         ov0SetOpacity,
     SetScreenLocation:  ov0SetScreenLocation,
     SetDstColorKey:     ov0SetDstColorKey,
     FlipBuffers:        ov0FlipBuffers,
     SetColorAdjustment: ov0SetColorAdjustment,
     AllocateSurface:    ov0AllocateSurface,
     DeallocateSurface:  ov0DeallocateSurface,
     ReallocateSurface:  ov0ReallocateSurface
};


/* internal */

static void ov0_set_regs( NVidiaDriverData *nvdrv, NVidiaOverlayLayerData *nvov0 )
{
     nvdrv->PMC[0x8700/4] = nvov0->regs.NV_PVIDEO_BUFFER;
     nvdrv->PMC[0x8704/4] = nvov0->regs.NV_PVIDEO_STOP;
     nvdrv->PMC[(0x8900/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_BASE;
     nvdrv->PMC[(0x8908/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_LIMIT;
     nvdrv->PMC[(0x8910/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_LUMINANCE;
     nvdrv->PMC[(0x8918/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_CHROMINANCE;
     nvdrv->PMC[(0x8920/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_OFFSET;
     nvdrv->PMC[(0x8928/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_SIZE_IN;
     nvdrv->PMC[(0x8930/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_POINT_IN;
     nvdrv->PMC[(0x8938/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_DS_DX;
     nvdrv->PMC[(0x8940/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_DT_DY;
     nvdrv->PMC[(0x8948/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_POINT_OUT;
     nvdrv->PMC[(0x8950/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_SIZE_OUT;
     nvdrv->PMC[(0x8958/4) + nvov0->buffer] = nvov0->regs.NV_PVIDEO_FORMAT;
     nvdrv->PMC[0x8b00/4] = nvov0->regs.NV_PVIDEO_COLOR_KEY;
}

static void
ov0_calc_regs( NVidiaDriverData       *nvdrv,
               NVidiaOverlayLayerData *nvov0,
               CoreLayer              *layer,
               DFBDisplayLayerConfig  *config )
{
     __u32         pitch = ((config->width << 1) + 63) & ~63;
     CoreSurface   *surface      = nvov0->videoSurface;
     SurfaceBuffer *front_buffer = surface->front_buffer;

     // nvov0->regs.NV_PVIDEO_BASE = front_buffer->video.offset & 0x07fffff0;
     // XBOX-specific: add nvov0->fbstart
     nvov0->regs.NV_PVIDEO_BASE = (nvov0->fbstart + front_buffer->video.offset) & 0x03fffff0;
     nvov0->regs.NV_PVIDEO_LIMIT = 0x07ffffff;
     nvov0->regs.NV_PVIDEO_SIZE_IN = (config->height << 16) | pitch;
     nvov0->regs.NV_PVIDEO_POINT_IN = 0;
     nvov0->regs.NV_PVIDEO_DS_DX = (config->width << 20) / nvov0->dest.w;
     nvov0->regs.NV_PVIDEO_DT_DY = (config->height << 20) / nvov0->dest.h;
     nvov0->regs.NV_PVIDEO_POINT_OUT = (nvov0->dest.y << 16) | nvov0->dest.x;
     nvov0->regs.NV_PVIDEO_SIZE_OUT = ((nvov0->dest.h << 16) | nvov0->dest.w);

     // pitch |= 1 << 20;   /* use color key */

     if(config->pixelformat != DSPF_UYVY)
          pitch |= 1 << 16;

     nvov0->regs.NV_PVIDEO_FORMAT = pitch;
     nvov0->regs.NV_PVIDEO_BUFFER = 1 << (nvov0->buffer << 2);
}


