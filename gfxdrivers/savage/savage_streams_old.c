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

#if 0	/* TO BE REWRITTEN */


#include <asm/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/io.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/surfaces.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/windows.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>
#include <misc/mem.h>

#include "savage.h"
#include "savage_streams_old.h"
#include "mmio.h"
#include "savage_bci.h"

  
 
static __u32 PSIDF[] = {
     SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_KRGB16,
     SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB16,
     SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB24,
     SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_RGB32,
     SAVAGE_PRIMARY_STREAM_CONTROL_PSIDF_ARGB,
     0,
     0,
     0,
     0
};

/* HACK */
static SavageDriverData *sdrv = NULL;
static SavageDeviceData *sdev = NULL;

static inline
void waitretrace (void)
{
     iopl(3);
    while ((inb (0x3da) & 0x8))
        ;

    while (!(inb (0x3da) & 0x8))
        ;
}

static DFBResult savage_streams_old_create_secondary( DisplayLayer *layer )
{
     DFBResult ret;

     ret = dfb_surface_create( layer->shared->width, layer->shared->height,
                               DSPF_RGB16, CSP_VIDEOONLY,
                               DSCAPS_VIDEOONLY, &layer->shared->surface );
     if (ret)
          return ret;
     
     layer->shared->windowstack = dfb_windowstack_new( layer );

     return DFB_OK;
}

/* layer functions */
static DFBResult savageSecondaryEnable( DisplayLayer *layer )
{
     DFBResult ret;

     if (!layer->shared->surface) {
          ret = savage_streams_old_create_secondary( layer );
          if (ret)
               return ret;
     }

     layer->shared->enabled = 1;

     savage_streams_old_restore( sdrv, sdev );
     
     return DFB_OK;
}

static DFBResult savageSecondaryDisable( DisplayLayer *layer )
{
     volatile __u8 *mmio = sdrv->mmio_base;

     /* turn off stream operation */ 
     waitretrace();

     vga_out8( mmio, 0x3d4, 0x67 );
     vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) & ~0x0c );
     
     layer->shared->enabled = 0;
     
     return DFB_OK;
}

static DFBResult savageSecondaryTestConfiguration(
                                            DisplayLayer               *layer,
                                            DFBDisplayLayerConfig      *config,
                                            DFBDisplayLayerConfigFlags *failed )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult savageSecondarySetConfiguration( DisplayLayer          *layer,
                                                  DFBDisplayLayerConfig *config)
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult savageSecondarySetOpacity( DisplayLayer *layer,
                                            __u8          opacity )
{
     return DFB_UNSUPPORTED;
}

static DFBResult savageSecondarySetScreenLocation( DisplayLayer *layer,
                                                   float         x,
                                                   float         y,
                                                   float         width,
                                                   float         height )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult savageSecondarySetSrcColorKey( DisplayLayer *layer,
                                                __u32         key )
{
     return DFB_UNSUPPORTED;
}

static DFBResult savageSecondarySetDstColorKey( DisplayLayer *layer,
                                                __u8          r,
                                                __u8          g,
                                                __u8          b )
{
     return DFB_UNSUPPORTED;
}

static DFBResult savageSecondaryFlipBuffers( DisplayLayer *layer )
{
     return DFB_UNSUPPORTED;
}

static void savage_streams_old_deinit()
{
}

DFBResult savage_streams_old_init( SavageDriverData *_sdrv,
                                   SavageDeviceData *_sdev )
{
     DisplayLayer *layer = (DisplayLayer*)calloc( 1, sizeof(DisplayLayer) );

     layer->shared = (DisplayLayerShared*) shcalloc( 1, sizeof(DisplayLayerShared) );
     
     sdrv = _sdrv;
     sdev = _sdev;

     layer->shared->caps = DLCAPS_ALPHACHANNEL | DLCAPS_OPACITY;
     sprintf( layer->shared->description, "Savage Secondary Stream" );

     layer->deinit = savage_streams_old_deinit;

     layer->shared->width = dfb_layers->shared->width;
     layer->shared->height = dfb_layers->shared->height;

     layer->shared->screen.x = 0.0f;
     layer->shared->screen.y = 0.0f;
     layer->shared->screen.w = 1.0f;
     layer->shared->screen.h = 1.0f;

     layer->shared->enabled = 0;
     layer->shared->opacity = 0xFF;

     layer->Enable = savageSecondaryEnable;          
     layer->Disable = savageSecondaryDisable;
     layer->TestConfiguration = savageSecondaryTestConfiguration;
     layer->SetConfiguration = savageSecondarySetConfiguration;
     layer->SetOpacity = savageSecondarySetOpacity;
     layer->SetScreenLocation = savageSecondarySetScreenLocation;
     layer->SetSrcColorKey = savageSecondarySetSrcColorKey;
     layer->SetDstColorKey = savageSecondarySetDstColorKey;
     layer->FlipBuffers = savageSecondaryFlipBuffers;

     layer->shared->bg.mode  = DLBM_DONTCARE;

     dfb_layers_add( layer );

     sdev->secondary = layer;
     
     return DFB_OK;
}

void savage_streams_old_restore( SavageDriverData *sdrv,
                                 SavageDeviceData *sdev )
{
     volatile __u8 *mmio = sdrv->mmio_base;

     /* unlock cr20 - cr3f */
     vga_out8( mmio, 0x3d4, 0x38 );
     vga_out8( mmio, 0x3d5, 0x48 );

     /* unlock cr40 - crff and allow cr36, cr37, cr68 and cr6f to be written */
     vga_out8( mmio, 0x3d4, 0x39 );
     vga_out8( mmio, 0x3d5, 0xa5 );

     /* unlock sr09 - srff */
     vga_out8( mmio, 0x3c4, 0x08 );
     vga_out8( mmio, 0x3c5, 0x06 );
     
     /* turn off stream operation */ 
     waitretrace();

     vga_out8( mmio, 0x3d4, 0x67 );
     vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) & ~0x0c );
     
     if (sdev->secondary && sdev->secondary->shared->enabled) {
          CoreSurface   *surface = sdev->secondary->shared->surface;
          SurfaceBuffer *buffer  = surface->front_buffer;
     
          /* turn on */
          waitretrace();
          
          vga_out8( mmio, 0x3d4, 0x23 );
          vga_out8( mmio, 0x3d5, 0x00 );
          
          vga_out8( mmio, 0x3d4, 0x26 );
          vga_out8( mmio, 0x3d5, 0x00 );
          
          vga_out8( mmio, 0x3d4, 0x67 );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) | 0x0c );
          
          
          /* tweak */
#if 0
          vga_out8( mmio, 0x3d4, 0x85 );
          printf( "cr85: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, 0x00 );
     
          vga_out8( mmio, 0x3d4, 0x88 );
          printf( "cr88: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) & ~0x01 );
     
          vga_out8( mmio, 0x3d4, 0x71 );
          printf( "cr71: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
     
          vga_out8( mmio, 0x3d4, 0x73 );
          printf( "cr73: 0x%02x\n", vga_in8( mmio, 0x3d5 ) );
          vga_out8( mmio, 0x3d5, 0xf0 );
     
     
          vga_out8( mmio, 0x3d4, 0x69 );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) | 0x80 );
     
          vga_out8( mmio, 0x3d4, 0x51 );
          vga_out8( mmio, 0x3d5, vga_in8( mmio, 0x3d5 ) | 0x80 );
#endif
     

          /* configure */
          savage_out32( mmio, SAVAGE_PRIMARY_STREAM_WINDOW_START, 0x00010001 );
          savage_out32( mmio, SAVAGE_PRIMARY_STREAM_WINDOW_SIZE, ((dfb_layers->shared->width - 1) << 16) | dfb_layers->shared->height );
          savage_out32( mmio, SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_ADRESS0, 0 );
          savage_out32( mmio, SAVAGE_PRIMARY_STREAM_FRAME_BUFFER_ADRESS1, 0 );
          savage_out32( mmio, SAVAGE_PRIMARY_STREAM_STRIDE, dfb_layers->shared->surface->front_buffer->video.pitch );
          savage_out32( mmio, SAVAGE_PRIMARY_STREAM_CONTROL, PSIDF[DFB_PIXELFORMAT_INDEX(dfb_layers->shared->surface->format)] );
          savage_out32( mmio, SAVAGE_PRIMARY_STREAM_FRAMEBUFFER_SIZE, (dfb_layers->shared->surface->front_buffer->video.pitch * dfb_layers->shared->surface->height)/8-1 );
          
          
          savage_out32( mmio, SAVAGE_CHROMA_KEY_CONTROL, 0 );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_CONTROL, 0x05000000 | surface->width );
          savage_out32( mmio, SAVAGE_CHROMA_KEY_UPPER_BOUND, 0 );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_HORIZONTAL_SCALING, 0x8000 );
          savage_out32( mmio, SAVAGE_COLOR_ADJUSTMENT, 0 );
          savage_out32( mmio, SAVAGE_BLEND_CONTROL, 0x02000000 | (4 << 10) );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_MULTIPLE_BUFFER_SUPPORT, 0 );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS0, buffer->video.offset );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS1, 0 );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_FRAME_BUFFER_ADDRESS2, 0 );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_FRAMEBUFFER_SIZE, ((buffer->video.pitch * surface->height)/8-1)  | 0x00400000 );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_STRIDE, buffer->video.pitch );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_SCALING, 0x8000 );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_SOURCE_LINE_COUNT, surface->height );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_VERTICAL_INITIAL_VALUE, 0 );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_WINDOW_START, 0x00010001 );
          savage_out32( mmio, SAVAGE_SECONDARY_STREAM_WINDOW_SIZE, ((sdev->secondary->shared->width - 1) << 16) | sdev->secondary->shared->height );
          
          savage_out32( mmio, SAVAGE_GENLOCK_CONTROL, 0 );

          
          /* bitch */
          {
               int pitch = buffer->video.pitch;
               unsigned char cr92;
               
               pitch = (pitch + 7) / 8;
               vga_out8( mmio, 0x3d4, 0x92);
               cr92 = vga_in8( mmio, 0x3d5);
               vga_out8( mmio, 0x3d5, (cr92 & 0x40) | (pitch >> 8) | 0x80);
               vga_out8( mmio, 0x3d4, 0x93);
               vga_out8( mmio, 0x3d5, pitch);
          }
     }
}

#endif

