/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de> and
	      Alex Song <alexsong@comports.com>.

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

#include <asm/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/io.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <math.h>

#include <directfb.h>

#include <core/fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/windows.h>
#include <core/fbdev/fbdev.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>
#include <misc/mem.h>

#include "savage.h"
#include "savage_streams_old.h"
#include "mmio.h"
#include "savage_bci.h"

/* #define SAVAGE_DEBUG */
#ifdef SAVAGE_DEBUG
#define SVGDBG(x...) fprintf(stderr, "savage_streams_old:" x)
#else
#define SVGDBG(x...)
#endif
  
typedef struct {
     DFBRectangle          dest;
     DFBDisplayLayerConfig config;
     int video_pitch;

     struct {
          /* secondary stream registers */
          __u32 SSTREAM_CTRL;
          __u32 SSTREAM_H_SCALE;
          __u32 BLEND_CTRL;
          __u32 SSTREAM_MULTIBUF;
          __u32 SSTREAM_FB_ADDR0;
          __u32 SSTREAM_FB_ADDR1;
          __u32 SSTREAM_STRIDE;
          __u32 SSTREAM_V_SCALE;
          __u32 SSTREAM_V_INIT_VALUE;
          __u32 SSTREAM_SRC_LINE_COUNT;
          __u32 SSTREAM_WIN_START;
          __u32 SSTREAM_WIN_SIZE;
          __u32 SSTREAM_FB_CB_ADDR;
          __u32 SSTREAM_FB_CR_ADDR;
          __u32 SSTREAM_CBCR_STRIDE;
          __u32 SSTREAM_FB_SIZE;
          __u32 SSTREAM_FB_ADDR2;
     } regs;
} SavageSecondaryLayerData;

typedef struct {
     DFBDisplayLayerConfig config;
     int dx;
     int dy;
     int init;

     struct {
          /* primary stream registers */
          __u32 PSTREAM_CTRL;
          __u32 PSTREAM_FB_ADDR0;
          __u32 PSTREAM_FB_ADDR1;
          __u32 PSTREAM_STRIDE;
          __u32 PSTREAM_WIN_START;
          __u32 PSTREAM_WIN_SIZE;
          __u32 PSTREAM_FB_SIZE;
     } regs;
} SavagePrimaryLayerData;

DisplayLayerFuncs pfuncs;
void *pdriver_data;

/* function prototypes */
static void
secondary_set_regs(SavageDriverData *sdrv, SavageSecondaryLayerData *slay);
static void
secondary_calc_regs(SavageDriverData *sdrv, SavageSecondaryLayerData *slay,
                    DisplayLayer *layer, DFBDisplayLayerConfig *config);
static void
primary_set_regs(SavageDriverData *sdrv, SavagePrimaryLayerData *play);
static void
primary_calc_regs(SavageDriverData *sdrv, SavagePrimaryLayerData *play,
                  DisplayLayer *layer, DFBDisplayLayerConfig *config);

static inline
void waitretrace (void)
{
     iopl(3);
    while ((inb (0x3da) & 0x8))
        ;

    while (!(inb (0x3da) & 0x8))
        ;
}

static void
streamOnOff(SavageDriverData * sdrv, int on)
{
     volatile __u8 *mmio = sdrv->mmio_base;

     waitretrace();
     
     if (on) {
       vga_out8( mmio, 0x3d4, 0x23 );
       vga_out8( mmio, 0x3d5, 0x00 );

       vga_out8( mmio, 0x3d4, 0x26 );
       vga_out8( mmio, 0x3d5, 0x00 );

       /* turn on stream operation */ 
       vga_out8( mmio, 0x3d4, 0x67 );
       vga_out8( mmio, 0x3d5, 0x0c );
     } else {
       /* turn off stream operation */ 
       vga_out8( mmio, 0x3d4, 0x67 );
       vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) & ~0x0c );
     }
}

/* secondary layer functions */
static int
savageSecondaryLayerDataSize()
{
     SVGDBG("savageSecondaryLayerDataSize\n");
     return sizeof(SavageSecondaryLayerData);
     }

static DFBResult
savageSecondaryInitLayer( GraphicsDevice             *device,
                          DisplayLayer               *layer,
                          DisplayLayerInfo           *layer_info,
                          DFBDisplayLayerConfig      *default_config,
                          DFBColorAdjustment         *default_adj,
                          void                       *driver_data,
                          void                       *layer_data )
{
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;
    
     SVGDBG("savageSecondaryInitLayer\n");
    
     /* set capabilities and type */
     layer_info->desc.caps = DLCAPS_SURFACE | DLCAPS_SCREEN_LOCATION |
                             DLCAPS_BRIGHTNESS | DLCAPS_CONTRAST |
                             DLCAPS_OPACITY | DLCAPS_HUE | DLCAPS_SATURATION |
                             DLCAPS_ALPHACHANNEL | DLCAPS_SRC_COLORKEY |
                             DLCAPS_DST_COLORKEY;
     layer_info->desc.type = DLTF_GRAPHICS | DLTF_VIDEO | DLTF_STILL_PICTURE;
     
     /* set name */
     snprintf(layer_info->name, DFB_DISPLAY_LAYER_INFO_NAME_LENGTH,
              "Savage Secondary Stream");
     
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
     default_adj->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST |
                               DCAF_HUE | DCAF_SATURATION;
     default_adj->brightness = 0x8000;
     default_adj->contrast   = 0x8000;
     default_adj->hue        = 0x8000;
     default_adj->saturation = 0x8000;

     /* initialize destination rectangle */
     dfb_primary_layer_rectangle(0.0f, 0.0f, 1.0f, 1.0f, &slay->dest);
     
     return DFB_OK;
}

static DFBResult savageSecondaryEnable( DisplayLayer *layer,
					void         *driver_data,
					void         *layer_data )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     volatile __u8 *mmio = sdrv->mmio_base;

     SVGDBG("savageSecondaryEnable\n");

     /* put secondary stream on top of primary stream */
     savage_out32(mmio, SAVAGE_BLEND_CONTROL,
                  SAVAGE_BLEND_CONTROL_COMP_SSTREAM);

     return DFB_OK;
}

static DFBResult savageSecondaryDisable( DisplayLayer *layer,
					 void         *driver_data,
					 void         *layer_data )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     volatile __u8 *mmio = sdrv->mmio_base;
     
     SVGDBG("savageSecondaryDisable\n");

     /* put primary stream on top of secondary stream */
     savage_out32(mmio, SAVAGE_BLEND_CONTROL,
		  SAVAGE_BLEND_CONTROL_COMP_PSTREAM);
     
     return DFB_OK;
}

static DFBResult
savageSecondaryTestConfiguration( DisplayLayer               *layer,
				  void                       *driver_data,
				  void                       *layer_data,
                                            DFBDisplayLayerConfig      *config,
                                            DFBDisplayLayerConfigFlags *failed )
{
     DFBDisplayLayerConfigFlags fail = 0;

     SVGDBG("savageSecondaryTestConfig\n");

     /* check for unsupported options */
     /* savage only supports one option at a time */
     switch (config->options) {
     case DLOP_NONE:
     case DLOP_ALPHACHANNEL:
     case DLOP_SRC_COLORKEY:
     case DLOP_DST_COLORKEY:
     case DLOP_OPACITY:
          break;
     default:
          fail |= DLCONF_OPTIONS;
          break;
     }

     /* check pixel format */
     switch (config->pixelformat) {
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
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
     if (config->height > 2048 || config->height < 1)
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
savageSecondarySetConfiguration( DisplayLayer          *layer,
				 void                  *driver_data,
				 void                  *layer_data,
                                                  DFBDisplayLayerConfig *config)
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;

     SVGDBG("savageSecondarySetConfiguration w:%i h:%i bpp:%i\n",
            config->width, config->height,
            DFB_BYTES_PER_PIXEL(config->pixelformat) * 8);

     /* remember configuration */
     slay->config = *config;

     secondary_calc_regs(sdrv, slay, layer, config);
     secondary_set_regs(sdrv, slay);

     return DFB_OK;
}

static DFBResult
savageSecondarySetOpacity( DisplayLayer *layer,
			   void         *driver_data,
			   void         *layer_data,
                                            __u8          opacity )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;
     volatile __u8 *mmio = sdrv->mmio_base;
     
     SVGDBG("savageSecondarySetOpacity\n");
     switch (opacity) {
          case 0:
               /* put primary stream on top of secondary stream */
               savage_out32(mmio, SAVAGE_BLEND_CONTROL,
                            SAVAGE_BLEND_CONTROL_COMP_PSTREAM);
               break;
          case 0xFF:
               /* put secondary stream on top of primary stream */
               savage_out32(mmio, SAVAGE_BLEND_CONTROL,
                            SAVAGE_BLEND_CONTROL_COMP_SSTREAM);
               break;
          default:
               if (slay->config.options == DLOP_OPACITY) {
	            /* reverse opacity */
		    opacity = 7 - (opacity >> 5);

		    /* for some reason opacity can not be zero */
		    if (opacity == 0)
		         opacity = 1;

                    /* dissolve primary and secondary stream */
                    savage_out32(mmio, SAVAGE_BLEND_CONTROL,
                                 SAVAGE_BLEND_CONTROL_COMP_DISSOLVE |
                                 KP_KS(opacity,0));
               } else {
     return DFB_UNSUPPORTED;
}
               break;
     }
     return DFB_OK;
}

static DFBResult
savageSecondarySetScreenLocation( DisplayLayer *layer,
				  void         *driver_data,
				  void         *layer_data,
                                                   float         x,
                                                   float         y,
                                                   float         width,
                                                   float         height )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;
     volatile __u8    *mmio = sdrv->mmio_base;

     SVGDBG("savageSecondarySetScreenLocation x:%f y:%f w:%f h:%f\n",
	    x, y, width, height);

     /* get new destination rectangle */
     dfb_primary_layer_rectangle(x, y, width, height, &slay->dest);

     if ((slay->regs.SSTREAM_FB_SIZE & 0x00400000) == 0) {
          /* secondary is yuv format */
          if (slay->dest.w < (slay->config.width / 2) ||
              slay->dest.h < (slay->config.height / 32)) {
               return DFB_UNSUPPORTED;
          }
     } else {
          /* secondary is rgb format */
          if (slay->dest.w < slay->config.width ||
              slay->dest.h < slay->config.height) {
               return DFB_UNSUPPORTED;
          }
}

     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_HORIZONTAL_SCALING,
                  ((32768 * slay->config.width) / slay->dest.w) & 0x0000FFFF);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_SCALING,
                  ((32768 * slay->config.height) / slay->dest.h) & 0x000FFFFF);
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_WINDOW_START,
                  OS_XY(slay->dest.x, slay->dest.y));
     savage_out32(mmio, SAVAGE_SECONDARY_STREAM_WINDOW_SIZE,
                  OS_WH(slay->dest.w, slay->dest.h));
     return DFB_OK;
}

static DFBResult
savageSecondarySetSrcColorKey( DisplayLayer *layer,
			       void         *driver_data,
			       void         *layer_data,
                               __u8          r,
			       __u8          g,
			       __u8          b )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;
     volatile __u8    *mmio = sdrv->mmio_base;

     SVGDBG("savageSecondarySetSrcColorKey\n");

     if (slay->config.options == DLOP_SRC_COLORKEY) {
          __u32 reg;

          switch (slay->config.pixelformat) {
          case DSPF_RGB15:
          case DSPF_RGB16:
               reg = 0x14000000;
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
               reg = 0x17000000;
               break;
          default:
     return DFB_UNSUPPORTED;
}

          savage_out32(mmio, SAVAGE_CHROMA_KEY_CONTROL,
                       reg | (r << 16) | (g << 8) | (b));
          savage_out32(mmio, SAVAGE_CHROMA_KEY_UPPER_BOUND,
                       0x00000000 | (r << 16) | (g << 8) | (b));

          return DFB_OK;
     } else {
          return DFB_UNSUPPORTED;
     }
}

static DFBResult
savageSecondarySetDstColorKey( DisplayLayer *layer,
			       void         *driver_data,
			       void         *layer_data,
                                                __u8          r,
                                                __u8          g,
                                                __u8          b )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;
     volatile __u8    *mmio = sdrv->mmio_base;

     SVGDBG("savageSecondarySetDstColorKey\n");

     if (slay->config.options == DLOP_DST_COLORKEY) {
          __u32 reg;

          switch (dfb_primary_layer_pixelformat()) {
          case DSPF_RGB16:
               reg = 0x14000000;
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
               reg = 0x17000000;
               break;
          default:
     return DFB_UNSUPPORTED;
}

          savage_out32(mmio, SAVAGE_CHROMA_KEY_CONTROL,
                       reg | (r << 16) | (g << 8) | (b));
          savage_out32(mmio, SAVAGE_CHROMA_KEY_UPPER_BOUND,
                       0x00000000 | (r << 16) | (g << 8) | (b));

          return DFB_OK;
     } else {
          return DFB_UNSUPPORTED;
     }
}

static DFBResult
savageSecondaryFlipBuffers(  DisplayLayer        *layer,
			     void                *driver_data,
			     void                *layer_data,
                             DFBSurfaceFlipFlags flags )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;
     CoreSurface        *surface = dfb_layer_surface( layer );

     SVGDBG("savageSecondaryFlipBuffers\n");

     dfb_surface_flip_buffers( surface );
     
     secondary_calc_regs(sdrv, slay, layer, &slay->config);
     secondary_set_regs(sdrv, slay);

     if (flags & DSFLIP_WAITFORSYNC)
          dfb_fbdev_wait_vsync();

     return DFB_OK;
}


static DFBResult
savageSecondarySetColorAdjustment( DisplayLayer       *layer,
				   void               *driver_data,
				   void               *layer_data,
				   DFBColorAdjustment *adj )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavageSecondaryLayerData *slay = (SavageSecondaryLayerData*) layer_data;
     volatile __u8    *mmio = sdrv->mmio_base;
     
     SVGDBG("savageSecondaryColorAdjustment b:%i c:%i h:%i s:%i\n",
	    adj->brightness, adj->contrast, adj->hue, adj->saturation);

     if ((slay->regs.SSTREAM_FB_SIZE & 0x00400000) == 0) {
          /* secondary is yuv format */
          __u32 reg;
          long sat = adj->saturation * 16 / 65536;
          double hue = (adj->hue - 0x8000) * 3.141592654 / 32768.0;
          unsigned char hs1 = ((char)(sat * cos(hue))) & 0x1f;
          unsigned char hs2 = ((char)(sat * sin(hue))) & 0x1f;

          reg = 0x80008000 | (adj->brightness >> 8) |
               ((adj->contrast & 0xf800) >> 3) | (hs1 << 16) | (hs2 << 24);

          savage_out32(mmio, SAVAGE_COLOR_ADJUSTMENT, reg);

          return DFB_OK;
     } else {
          /* secondary is rgb format */
          return DFB_UNSUPPORTED;
     }
}

DisplayLayerFuncs savageSecondaryFuncs = {
     LayerDataSize:      savageSecondaryLayerDataSize,
     InitLayer:          savageSecondaryInitLayer,
     Enable:             savageSecondaryEnable,
     Disable:            savageSecondaryDisable,
     TestConfiguration:  savageSecondaryTestConfiguration,
     SetConfiguration:   savageSecondarySetConfiguration,
     SetOpacity:         savageSecondarySetOpacity,
     SetScreenLocation:  savageSecondarySetScreenLocation,
     SetSrcColorKey:     savageSecondarySetSrcColorKey,
     SetDstColorKey:     savageSecondarySetDstColorKey,
     /*GetLevel:*/
     /*SetLevel:*/
     FlipBuffers:        savageSecondaryFlipBuffers,
     SetColorAdjustment: savageSecondarySetColorAdjustment,
     /*SetPalette:*/
     /*AllocateSurface:*/
     /*ReallocateSurface:*/
     /*DeallocateSurface:*/
};

/* secondary internal */
static void
secondary_set_regs(SavageDriverData *sdrv, SavageSecondaryLayerData *slay)
{
    volatile __u8 *mmio = sdrv->mmio_base;

    SVGDBG("secondary_set_regs\n");
     
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_CONTROL,
		 slay->regs.SSTREAM_CTRL);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_HORIZONTAL_SCALING,
		 slay->regs.SSTREAM_H_SCALE);
    savage_out32(mmio, SAVAGE_BLEND_CONTROL,
                 slay->regs.BLEND_CTRL);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_MULTIPLE_BUFFER_SUPPORT,
		 slay->regs.SSTREAM_MULTIBUF);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS0,
		 slay->regs.SSTREAM_FB_ADDR0);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS1,
		 slay->regs.SSTREAM_FB_ADDR1);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS2,
		 slay->regs.SSTREAM_FB_ADDR2);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_SIZE,
		 slay->regs.SSTREAM_FB_SIZE);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_STRIDE,
		 slay->regs.SSTREAM_STRIDE);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_SCALING,
		 slay->regs.SSTREAM_V_SCALE);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_SOURCE_LINE_COUNT,
		 slay->regs.SSTREAM_SRC_LINE_COUNT);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_INITIAL_VALUE,
		 slay->regs.SSTREAM_V_INIT_VALUE);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_WINDOW_START,
		 slay->regs.SSTREAM_WIN_START);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_WINDOW_SIZE,
		 slay->regs.SSTREAM_WIN_SIZE);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FB_CB_ADDRESS,
                 slay->regs.SSTREAM_FB_CB_ADDR);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FB_CR_ADDRESS,
                 slay->regs.SSTREAM_FB_CR_ADDR);
    savage_out32(mmio, SAVAGE_SECONDARY_STREAM_CBCR_STRIDE,
                 slay->regs.SSTREAM_CBCR_STRIDE);

    /* Set FIFO L2 on second stream. */
    {
        int pitch = slay->video_pitch;
	unsigned char cr92;

	SVGDBG("FIFO L2 pitch:%i\n", pitch);
	pitch = (pitch + 7) / 8;
	vga_out8(mmio, 0x3d4, 0x92);
	cr92 = vga_in8( mmio, 0x3d5);
	vga_out8(mmio, 0x3d5, (cr92 & 0x40) | (pitch >> 8) | 0x80);
	vga_out8(mmio, 0x3d4, 0x93);
	vga_out8(mmio, 0x3d5, pitch);
    }
}

static void
secondary_calc_regs(SavageDriverData *sdrv, SavageSecondaryLayerData *slay,
                    DisplayLayer *layer, DFBDisplayLayerConfig *config)
{
     CoreSurface * surface = dfb_layer_surface(layer);
     SurfaceBuffer * front_buffer = surface->front_buffer;
     /* source size */
     const int src_w = surface->width;
     const int src_h = surface->height;
     /* destination size */
     const int drw_w = slay->dest.w;
     const int drw_h = slay->dest.h;
     
     SVGDBG("secondary_calc_regs x:%i y:%i w:%i h:%i\n",
            slay->dest.x, slay->dest.y, slay->dest.w, slay->dest.h);
     SVGDBG("w:%i h:%i pitch:%i video.offset:%x\n", surface->width,
            surface->height, front_buffer->video.pitch,
            front_buffer->video.offset);
     
     slay->video_pitch = 1;
     slay->regs.SSTREAM_FB_SIZE = (((front_buffer->video.pitch *
                                     surface->height) / 8) - 1) & 0x003fffff;

     switch (surface->format) {
     case DSPF_RGB15:
          SVGDBG("secondary set to DSPF_RGB15\n");
          slay->regs.SSTREAM_FB_SIZE |= 0x00400000;
          slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_KRGB16;
          break;
     case DSPF_RGB16:
          SVGDBG("secondary set to DSPF_RGB16\n");
          slay->regs.SSTREAM_FB_SIZE |= 0x00400000;
          slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB16;
          break;
     case DSPF_RGB24:
          SVGDBG("secondary set to DSPF_RGB24\n");
          slay->regs.SSTREAM_FB_SIZE |= 0x00400000;
          slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB24;
          break;
     case DSPF_RGB32:
          SVGDBG("secondary set to DSPF_RGB32\n");
          slay->regs.SSTREAM_FB_SIZE |= 0x00400000;
          slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_RGB32;
          break;
     case DSPF_YUY2:
          SVGDBG("secondary set to DSPF_YUY2\n");
          slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YCbCr422;
          break;
     case DSPF_UYVY:
          SVGDBG("secondary set to DSPF_UYVY\n");
          slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_CbYCrY422;
          break;
     case DSPF_I420:
          SVGDBG("secondary set to DSPF_I420\n");
	  slay->video_pitch = 2;
          slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YCbCr420;
          slay->regs.SSTREAM_FB_CB_ADDR = front_buffer->video.offset +
               (surface->height * front_buffer->video.pitch);
          slay->regs.SSTREAM_FB_CR_ADDR = slay->regs.SSTREAM_FB_CB_ADDR +
               ((surface->height * front_buffer->video.pitch)/4);
          slay->regs.SSTREAM_CBCR_STRIDE = ((front_buffer->video.pitch/2)
                                            & 0x00001fff);
          break;
     case DSPF_YV12:
          SVGDBG("secondary set to DSPF_YV12\n");
	  slay->video_pitch = 2;
          slay->regs.SSTREAM_CTRL = SAVAGE_SECONDARY_STREAM_CONTROL_SSIDF_YCbCr420;
          slay->regs.SSTREAM_FB_CR_ADDR = front_buffer->video.offset +
               surface->height * front_buffer->video.pitch;
          slay->regs.SSTREAM_FB_CB_ADDR = slay->regs.SSTREAM_FB_CR_ADDR +
               (surface->height * front_buffer->video.pitch)/4;
          slay->regs.SSTREAM_CBCR_STRIDE = ((front_buffer->video.pitch/2)
                                            & 0x00001fff);
          break;
     default:
          BUG("unexpected secondary pixelformat");
          return;
     }

     slay->regs.SSTREAM_CTRL |= src_w;
     
     switch (config->options) {
     case DLOP_ALPHACHANNEL:
          SVGDBG("secondary option DLOP_ALPHACHANNEL\n");
          slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_ALPHA;
          break;
     case DLOP_SRC_COLORKEY:
          SVGDBG("secondary option DLOP_SRC_COLORKEY\n");
          slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_SCOLORKEY;
          break;
     case DLOP_DST_COLORKEY:
          SVGDBG("secondary option DLOP_DST_COLORKEY\n");
          slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_PCOLORKEY;
          break;
     case DLOP_OPACITY:
          SVGDBG("secondary option DLOP_OPACITY\n");
          /* fall through */
     default:
          SVGDBG("secondary option default\n");
          slay->regs.BLEND_CTRL = SAVAGE_BLEND_CONTROL_COMP_SSTREAM;
          break;
     }

     slay->regs.SSTREAM_H_SCALE = ((32768 * src_w) / drw_w) & 0x0000FFFF;
     slay->regs.SSTREAM_V_SCALE = ((32768 * src_h) / drw_h) & 0x000FFFFF;
     slay->regs.SSTREAM_V_INIT_VALUE = 0;
     slay->regs.SSTREAM_SRC_LINE_COUNT = src_h & 0x7ff;
     slay->regs.SSTREAM_MULTIBUF = 0;
     slay->regs.SSTREAM_FB_ADDR0 = front_buffer->video.offset & 0x01ffffff;
     slay->regs.SSTREAM_FB_ADDR1 = 0;
     slay->regs.SSTREAM_FB_ADDR2 = 0;
     slay->regs.SSTREAM_STRIDE = front_buffer->video.pitch & 0x00001fff;
     slay->regs.SSTREAM_WIN_START = OS_XY(slay->dest.x, slay->dest.y);
     slay->regs.SSTREAM_WIN_SIZE = OS_WH(drw_w, drw_h);
     
     /* remember pitch */
     slay->video_pitch *= front_buffer->video.pitch;
}

/* primary layer functions */
static int
savagePrimaryLayerDataSize()
{
     SVGDBG("savagePrimaryLayerDataSize\n");
     return sizeof(SavagePrimaryLayerData);
}

static DFBResult
savagePrimaryInitLayer( GraphicsDevice             *device,
                        DisplayLayer               *layer,
                        DisplayLayerInfo           *layer_info,
                        DFBDisplayLayerConfig      *default_config,
                        DFBColorAdjustment         *default_adj,
                        void                       *driver_data,
                        void                       *layer_data )
{
     SavagePrimaryLayerData *play = (SavagePrimaryLayerData*) layer_data;
     DFBResult ret;
    
     SVGDBG("savagePrimaryInitLayer w:%i h:%i bpp:%i\n",
            dfb_config->mode.width, dfb_config->mode.height,
            dfb_config->mode.depth);
    
     /* call the original initialization function first */
     ret = pfuncs.InitLayer (device, layer, layer_info,
                             default_config, default_adj,
                             pdriver_data, layer_data);
     if (ret)
          return ret;

     /* set name */
     snprintf(layer_info->name, DFB_DISPLAY_LAYER_INFO_NAME_LENGTH,
              "Savage Primary Stream");

     /* add support for options */
     default_config->flags |= DLCONF_OPTIONS;
     default_config->options = DLOP_NONE;

     /* add capabilities */
     layer_info->desc.caps |= DLCAPS_SCREEN_LOCATION;
     
     play->dx = 0;
     play->dy = 0;
     play->init = 0;

     return DFB_OK;
}

static DFBResult
savagePrimarySetConfiguration( DisplayLayer          *layer,
                               void                  *driver_data,
                               void                  *layer_data,
                               DFBDisplayLayerConfig *config )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavagePrimaryLayerData *play = (SavagePrimaryLayerData*) layer_data;
     DFBResult ret;

     SVGDBG("savagePrimarySetConfiguration w:%i h:%i bpp:%i\n",
            config->width, config->height,
            DFB_BYTES_PER_PIXEL(config->pixelformat) * 8);

     ret = pfuncs.SetConfiguration(layer, driver_data, layer_data, config);
     if (ret != DFB_OK)
          return ret;

     /* remember configuration */
     play->config = *config;

     primary_calc_regs(sdrv, play, layer, config);
     primary_set_regs(sdrv, play);
     
     return DFB_OK;
}

static DFBResult
savagePrimarySetScreenLocation( DisplayLayer *layer,
                                void         *driver_data,
                                void         *layer_data,
                                float         x,
                                float         y,
                                float         width,
                                float         height )
{
     SavageDriverData *sdrv = (SavageDriverData*) driver_data;
     SavagePrimaryLayerData *play = (SavagePrimaryLayerData*) layer_data;

     SVGDBG("savagePrimarySetScreenLocation x:%f y:%f w:%f h:%f\n",
	    x, y, width, height);
     
     if (width != 1  ||  height != 1)
          return DFB_UNSUPPORTED;

     play->dx = (int)(x * (float)play->config.width + 0.5f);
     play->dy = (int)(y * (float)play->config.height + 0.5f);
     
     primary_calc_regs(sdrv, play, layer, &play->config);
     primary_set_regs(sdrv, play);
     
     return DFB_OK;
}

DisplayLayerFuncs savagePrimaryFuncs = {
     LayerDataSize:      savagePrimaryLayerDataSize,
     InitLayer:          savagePrimaryInitLayer,
     /*Enable:             savagePrimaryEnable,*/
     /*Disable:            savagePrimaryDisable,*/
     /*TestConfiguration:  savagePrimaryTestConfiguration,*/
     SetConfiguration:   savagePrimarySetConfiguration,
     /*SetOpacity:         savagePrimarySetOpacity,*/
     SetScreenLocation:  savagePrimarySetScreenLocation,
     /*SetSrcColorKey:     savagePrimarySetSrcColorKey,*/
     /*SetDstColorKey:     savagePrimarySetDstColorKey,*/
     /*GetLevel:*/
     /*SetLevel:*/
     /*FlipBuffers:        savagePrimaryFlipBuffers,*/
     /*SetColorAdjustment: savagePrimarySetColorAdjustment,*/
     /*SetPalette:*/
     /*AllocateSurface:    savagePrimaryAllocateSurface,*/
     /*ReallocateSurface:*/
     /*DeallocateSurface:*/
};
          
/* primary internal */
static void
primary_set_regs(SavageDriverData *sdrv, SavagePrimaryLayerData *play)
{
     volatile __u8 *mmio = sdrv->mmio_base;
          
     SVGDBG("primary_set_regs\n");

     /* turn streams on */
     streamOnOff(sdrv, 1);

     /* setup primary stream */
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_WINDOW_START,
                  play->regs.PSTREAM_WIN_START);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_WINDOW_SIZE,
                  play->regs.PSTREAM_WIN_SIZE);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_ADDRESS0,
                  play->regs.PSTREAM_FB_ADDR0);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_ADDRESS1,
                  play->regs.PSTREAM_FB_ADDR1);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_STRIDE,
                  play->regs.PSTREAM_STRIDE);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_CONTROL,
                  play->regs.PSTREAM_CTRL);
     savage_out32(mmio, SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_SIZE,
                  play->regs.PSTREAM_FB_SIZE);

     if (play->init == 0) {
          /* tweak */
          /* fifo fetch delay register */
          vga_out8( mmio, 0x3d4, 0x85 );
          SVGDBG( "cr85: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, 0x00 );
     
          /* force high priority for display channel memory */
          vga_out8( mmio, 0x3d4, 0x88 );
          SVGDBG( "cr88: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) & ~0x01 );
     
          /* primary stream timeout register */
          vga_out8( mmio, 0x3d4, 0x71 );
          SVGDBG( "cr71: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
     
          /* secondary stream timeout register */
          vga_out8( mmio, 0x3d4, 0x73 );
          SVGDBG( "cr73: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
     
          /* set primary stream to use memory mapped io */
          vga_out8( mmio, 0x3d4, 0x69 );
          SVGDBG( "cr69: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) | 0x80 );
     
          /* enable certain registers to be loaded on vsync */
          vga_out8( mmio, 0x3d4, 0x51 );
          SVGDBG( "cr51: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) | 0x80 );

          /* setup secondary stream */
          savage_out32(mmio, SAVAGE_CHROMA_KEY_CONTROL, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_CONTROL, 0);
          savage_out32(mmio, SAVAGE_CHROMA_KEY_UPPER_BOUND, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_HORIZONTAL_SCALING, 0);
          savage_out32(mmio, SAVAGE_COLOR_ADJUSTMENT, 0);
          savage_out32(mmio, SAVAGE_BLEND_CONTROL, 1 << 24);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_MULTIPLE_BUFFER_SUPPORT, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS0, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS1, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS2, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_SIZE, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_STRIDE, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_SCALING, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_SOURCE_LINE_COUNT, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_INITIAL_VALUE, 0);
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_WINDOW_START,
                  OS_XY(0xfffe, 0xfffe));
          savage_out32(mmio, SAVAGE_SECONDARY_STREAM_WINDOW_SIZE,
		  OS_WH(10,2));

          play->init = 1;
     }
}

static void
primary_calc_regs(SavageDriverData *sdrv, SavagePrimaryLayerData *play,
                  DisplayLayer *layer, DFBDisplayLayerConfig *config)
{
     CoreSurface * surface = dfb_layer_surface(layer);
     SurfaceBuffer * front_buffer = surface->front_buffer;
     
     SVGDBG("primary_calc_regs w:%i h:%i pitch:%i video.offset:%x\n",
            surface->width, surface->height, front_buffer->video.pitch,
	    front_buffer->video.offset);
     
     switch (surface->format) {
     case DSPF_RGB15:
          SVGDBG("primary set to DSPF_RGB15\n");
          play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_KRGB16;
          break;
     case DSPF_RGB16:
          SVGDBG("primary set to DSPF_RGB16\n");
          play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB16;
          break;
     case DSPF_RGB24:
          SVGDBG("primary set to DSPF_RGB24 (unaccelerated)\n");
          play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB24;
          break;
     case DSPF_RGB32:
          SVGDBG("primary set to DSPF_RGB32\n");
          play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB32;
          break;
     case DSPF_ARGB:
          SVGDBG("primary set to DSPF_ARGB\n");
          play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_ARGB;
          break;
#ifdef SUPPORT_RGB332
     case DSPF_RGB332:
          SVGDBG("primary set to DSPF_RGB332\n");
          play->regs.PSTREAM_CTRL = SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_CLUT;
          break;
#endif
     default:
          BUG("unexpected primary pixelformat");
          return;
     }

     play->regs.PSTREAM_FB_ADDR0 = front_buffer->video.offset & 0x01ffffff;
     play->regs.PSTREAM_FB_ADDR1 = 0;
     play->regs.PSTREAM_STRIDE = front_buffer->video.pitch & 0x00001fff;
     play->regs.PSTREAM_WIN_START = OS_XY(play->dx, play->dy);
     play->regs.PSTREAM_WIN_SIZE = OS_WH(surface->width, surface->height);
     play->regs.PSTREAM_FB_SIZE = (((front_buffer->video.pitch *
                                     surface->height) / 8) - 1) & 0x003fffff;
}
/* end of code */
