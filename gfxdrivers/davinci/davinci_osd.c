/*
   TI Davinci driver - Graphics Layer

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org>

   Code is derived from VMWare driver.

   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <asm/types.h>

#include <stdio.h>
#include <sys/ioctl.h>

#include <directfb.h>
#include <directfb_util.h>

#include <core/layers.h>
#include <core/surface.h>
#include <core/surface_buffer.h>

#include <gfx/convert.h>

#include <direct/memcpy.h>
#include <direct/messages.h>

#include "davincifb.h"

#include "davinci_gfxdriver.h"
#include "davinci_osd.h"


D_DEBUG_DOMAIN( Davinci_OSD, "Davinci/OSD", "TI Davinci OSD" );

/**********************************************************************************************************************/

static int
osdLayerDataSize()
{
     return sizeof(DavinciOSDLayerData);
}

static DFBResult
osdInitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
     int                  ret;
     DavinciDriverData   *ddrv = driver_data;
     DavinciOSDLayerData *dosd = layer_data;

     D_DEBUG_AT( Davinci_OSD, "%s()\n", __FUNCTION__ );

     ret = ioctl( ddrv->fb[OSD0].fd, FBIOGET_VSCREENINFO, &dosd->var0 );
     if (ret) {
          D_PERROR( "Davinci/OSD: FBIOGET_VSCREENINFO (fb%d) failed!\n", OSD0 );
          return DFB_INIT;
     }

     ret = ioctl( ddrv->fb[OSD1].fd, FBIOGET_VSCREENINFO, &dosd->var1 );
     if (ret) {
          D_PERROR( "Davinci/OSD: FBIOGET_VSCREENINFO (fb%d) failed!\n", OSD1 );
          return DFB_INIT;
     }

     ret = ioctl( ddrv->fb[OSD0].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", OSD0, 0 );

     ret = ioctl( ddrv->fb[OSD1].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", OSD1, 0 );

     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE | DLCAPS_ALPHACHANNEL | DLCAPS_OPACITY | DLCAPS_SCREEN_POSITION |
                         DLCAPS_SRC_COLORKEY;
     description->type = DLTF_GRAPHICS;

     /* set name */
     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "TI Davinci OSD" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;
     config->width       = 640;
     config->height      = 480;
     config->pixelformat = DSPF_ARGB;
     config->buffermode  = DLBM_FRONTONLY;
     config->options     = DLOP_ALPHACHANNEL;

     return DFB_OK;
}

static DFBResult
osdTestRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     D_DEBUG_AT( Davinci_OSD, "%s()\n", __FUNCTION__ );

     if (config->options & ~DAVINCI_OSD_SUPPORTED_OPTIONS)
          fail |= CLRCF_OPTIONS;

     switch (config->format) {
          case DSPF_RGB444:
          case DSPF_RGB555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB4444:
          case DSPF_ARGB1555:
          case DSPF_ARGB:
               break;

          default:
               fail |= CLRCF_FORMAT;
     }

     if (config->width  < 8 || config->width  > 720)
          fail |= CLRCF_WIDTH;

     if (config->height < 8 || config->height > 576)
          fail |= CLRCF_HEIGHT;


     if (config->dest.x < 0 || config->dest.y < 0)
          fail |= CLRCF_DEST;

     if (config->dest.x + config->dest.w > 720)
          fail |= CLRCF_DEST;

     if (config->dest.y + config->dest.h > 576)
          fail |= CLRCF_DEST;


     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
osdSetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags  updated,
              CoreSurface                *surface,
              CorePalette                *palette,
              CoreSurfaceBufferLock      *lock )
{
     int                  ret;
     DavinciDriverData   *ddrv = driver_data;
     DavinciDeviceData   *ddev = ddrv->ddev;
     DavinciOSDLayerData *dosd = layer_data;

     D_DEBUG_AT( Davinci_OSD, "%s()\n", __FUNCTION__ );

     D_ASSERT( ddrv != NULL );
     D_ASSERT( ddev != NULL );
     D_ASSERT( dosd != NULL );

     ret = ioctl( ddrv->fb[OSD0].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", OSD0, 0 );

     ret = ioctl( ddrv->fb[OSD1].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", OSD1, 0 );

     ioctl( ddrv->fb[OSD0].fd, FBIO_WAITFORVSYNC );

     /* Update blend parameters? */
     if (updated & (CLRCF_OPTIONS | CLRCF_OPACITY | CLRCF_SRCKEY | CLRCF_FORMAT)) {
          vpbe_blink_option_t        blink = {0};
          vpbe_bitmap_blend_params_t blend = {0};

          D_DEBUG_AT( Davinci_OSD, "  -> %s\n", dfb_pixelformat_name( config->format ) );

          if (config->options & DLOP_SRC_COLORKEY) {
               blend.enable_colorkeying = 1;
               blend.colorkey           = dfb_color_to_pixel( DSPF_RGB16,
                                                              config->src_key.r,
                                                              config->src_key.g,
                                                              config->src_key.b );

               D_DEBUG_AT( Davinci_OSD, "  -> color key 0x%02x (%02x %02x %02x)\n",
                           blend.colorkey, config->src_key.r, config->src_key.g, config->src_key.b );
          }
          else if (config->options & DLOP_OPACITY) {
               blend.bf = config->opacity >> 5;

               D_DEBUG_AT( Davinci_OSD, "  -> opacity %d/7\n", blend.bf );
          }
          else
               blend.bf = 7;

          ret = ioctl( ddrv->fb[OSD0].fd, FBIO_SET_BITMAP_BLEND_FACTOR, &blend );
          if (ret)
               D_PERROR( "Davinci/OSD: FBIO_SET_BITMAP_BLEND_FACTOR (fb%d)!\n", OSD0 );

          if (config->options & DLOP_ALPHACHANNEL)
               dosd->alpha = DFB_PIXELFORMAT_HAS_ALPHA( config->format );
          else
               dosd->alpha = 0;

          D_DEBUG_AT( Davinci_OSD, "  -> %salpha channel\n", dosd->alpha ? "" : "no " );

          if (dosd->alpha) {
               if (ioctl( ddrv->fb[OSD0].fd, FBIO_ENABLE_DISABLE_ATTRIBUTE_WIN, dosd->alpha ))
                    D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_ATTRIBUTE_WIN (fb%d - %d)!\n", OSD0, dosd->alpha );
          }

          if (ioctl( ddrv->fb[OSD1].fd, FBIO_SET_BLINK_INTERVAL, &blink ))
               D_PERROR( "Davinci/OSD: FBIO_SET_BLINK_INTERVAL (fb%d - disable)!\n", OSD1 );
     }

     /* Update size? */
     if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_BUFFERMODE)) {
          vpbe_window_position_t win_pos;

          D_DEBUG_AT( Davinci_OSD, "  -> %dx%d\n", config->width, config->height );

/*********************************** Start workaround ***********************************/
          win_pos.xpos = 0;
          win_pos.ypos = 0;

          ret = ioctl( ddrv->fb[OSD0].fd, FBIO_SETPOS, &win_pos );
          if (ret)
               D_PERROR( "Davinci/OSD: FBIO_SETPOS (fb%d - %d,%d) failed!\n", OSD0, win_pos.xpos, win_pos.ypos );

          ret = ioctl( ddrv->fb[OSD1].fd, FBIO_SETPOS, &win_pos );
          if (ret)
               D_PERROR( "Davinci/OSD: FBIO_SETPOS (fb%d - %d,%d) failed!\n", OSD1, win_pos.xpos, win_pos.ypos );

          updated |= CLRCF_DEST;

          dosd->var0.yoffset = dosd->var1.yoffset = 0;
/*********************************** End workaround ***********************************/

          /* Set width and height. */
          dosd->var0.xres = config->width;
          dosd->var0.yres = config->height;
          dosd->var1.xres = config->width;
          dosd->var1.yres = config->height;

          dosd->var0.yres_virtual = ddrv->fb[OSD0].size / lock->pitch;

          ret = ioctl( ddrv->fb[OSD0].fd, FBIOPUT_VSCREENINFO, &dosd->var0 );
          if (ret)
               D_PERROR( "Davinci/OSD: FBIOPUT_VSCREENINFO (fb%d) failed!\n", OSD0 );

          ret = ioctl( ddrv->fb[OSD1].fd, FBIOPUT_VSCREENINFO, &dosd->var1 );
          if (ret)
               D_PERROR( "Davinci/OSD: FBIOPUT_VSCREENINFO (fb%d) failed!\n", OSD1 );
     }

     /* Update position? */
     if (updated & CLRCF_DEST) {
          vpbe_window_position_t win_pos;

          D_DEBUG_AT( Davinci_OSD, "  -> %d, %d\n", config->dest.x, config->dest.y );

          /* Set horizontal and vertical offset. */
          win_pos.xpos = config->dest.x;
          win_pos.ypos = config->dest.y;

          ret = ioctl( ddrv->fb[OSD0].fd, FBIO_SETPOS, &win_pos );
          if (ret)
               D_PERROR( "Davinci/OSD: FBIO_SETPOS (fb%d - %d,%d) failed!\n", OSD0, config->dest.x, config->dest.y );

          ret = ioctl( ddrv->fb[OSD1].fd, FBIO_SETPOS, &win_pos );
          if (ret)
               D_PERROR( "Davinci/OSD: FBIO_SETPOS (fb%d - %d,%d) failed!\n", OSD1, config->dest.x, config->dest.y );
     }

     davincifb_pan_display( &ddrv->fb[OSD0], &dosd->var0,
                            (config->format == DSPF_RGB16) ? lock : NULL, DSFLIP_NONE );

     ret = ioctl( ddrv->fb[OSD0].fd, FBIOGET_FSCREENINFO, &ddev->fix[OSD0] );
     if (ret)
          D_PERROR( "Davinci/OSD: FBIOGET_FSCREENINFO (fb%d) failed!\n", OSD0 );

     ret = ioctl( ddrv->fb[OSD1].fd, FBIOGET_FSCREENINFO, &ddev->fix[OSD1] );
     if (ret)
          D_PERROR( "Davinci/OSD: FBIOGET_FSCREENINFO (fb%d) failed!\n", OSD1 );

     dosd->enable = true;

     if (ioctl( ddrv->fb[OSD0].fd, FBIO_ENABLE_DISABLE_ATTRIBUTE_WIN, 0 ))
          D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_ATTRIBUTE_WIN (fb%d - %d)!\n", OSD0, 0 );

     return DFB_OK;
}

static DFBResult
osdRemoveRegion( CoreLayer *layer,
                 void      *driver_data,
                 void      *layer_data,
                 void      *region_data )
{
     int                  ret;
     DavinciDriverData   *ddrv = driver_data;
     DavinciOSDLayerData *dosd = layer_data;

     D_DEBUG_AT( Davinci_OSD, "%s()\n", __FUNCTION__ );

     D_ASSERT( ddrv != NULL );

     ret = ioctl( ddrv->fb[OSD0].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", OSD0, 0 );

     ret = ioctl( ddrv->fb[OSD1].fd, FBIO_ENABLE_DISABLE_WIN, 0 );
     if (ret)
          D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", OSD1, 0 );

     dosd->enable = false;

     return DFB_OK;
}

static void
update_buffers( DavinciDriverData     *ddrv,
                DavinciDeviceData     *ddev,
                CoreSurface           *surface,
                CoreSurfaceBufferLock *lock,
                const DFBRegion       *update )
{
     DFBRectangle       rect;
     CoreSurfaceBuffer *buffer;

     D_ASSERT( ddrv != NULL );
     D_ASSERT( ddev != NULL );
     D_ASSERT( surface != NULL );
     D_ASSERT( lock != NULL );
     DFB_REGION_ASSERT_IF( update );

     buffer = lock->buffer;
     D_ASSERT( buffer != NULL );

     if (update) {
          rect = DFB_RECTANGLE_INIT_FROM_REGION( update );

          if (rect.x & 1) {
               rect.x &= ~1;
               rect.w++;
          }

          if (rect.w & 1)
               rect.w++;
     }
     else {
          rect.x = 0;
          rect.y = 0;
          rect.w = surface->config.size.w;
          rect.h = surface->config.size.h;
     }

     if (ddrv->c64x_present) {
          unsigned long rgb   = ddev->fix[OSD0].smem_start + rect.x * 2 + rect.y * ddev->fix[OSD0].line_length;
          unsigned long alpha = ddev->fix[OSD1].smem_start + rect.x / 2 + rect.y * ddev->fix[OSD1].line_length;
          unsigned long src   = lock->phys                 + rect.x * 4 + rect.y * lock->pitch;

          D_ASSUME( ddev->fix[OSD0].line_length == ddev->fix[OSD1].line_length );

          davinci_c64x_dither_argb( &ddrv->c64x, rgb, alpha, ddev->fix[OSD0].line_length, src, lock->pitch, rect.w, rect.h );
          davinci_c64x_write_back_all( &ddrv->c64x );
     }
     else {
          u32  *src32 = lock->addr + rect.y * lock->pitch + DFB_BYTES_PER_LINE( buffer->format, rect.x );
          int   sp4   = lock->pitch / 4;
          u32  *dst32 = ddrv->fb[OSD0].mem + rect.y * ddev->fix[OSD0].line_length + rect.x * 2;
          int   dp4   = ddev->fix[OSD0].line_length / 4;
          u8   *dst8  = ddrv->fb[OSD1].mem + rect.y * ddev->fix[OSD1].line_length + rect.x / 2;
          int   dp    = ddev->fix[OSD1].line_length;
          int   w2    = rect.w / 2;
          u32   z     = 0;

          switch (buffer->format) {
               case DSPF_ARGB4444:
                    while (rect.h--) {
                         int x;

                         for (x=0; x<w2; x++) {
                              dst32[x] = ((src32[x] & 0x0f000f00) << 4) | ((src32[x] & 0x08000800)     ) |
                                         ((src32[x] & 0x00f000f0) << 3) | ((src32[x] & 0x00c000c0) >> 1) |
                                         ((src32[x] & 0x000f000f) << 1) | ((src32[x] & 0x00080008) >> 3);

                              dst8[x] = ((src32[x] & 0xe0000000) >> 29) | ((src32[x] & 0x0000e000) >> 9);
                         }

                         src32 += sp4;
                         dst32 += dp4;
                         dst8  += dp;
                    }
                    break;

               case DSPF_ARGB1555:
                    while (rect.h--) {
                         int x;

                         for (x=0; x<w2; x++) {
                              dst32[x] = ((src32[x] & 0x7c007c00) << 1) |
                                         ((src32[x] & 0x03e003e0) << 1) |
                                          (src32[x] & 0x003f003f);

                              dst8[x] = ((src32[x] & 0x80000000) ? 0x70 : 0x00) |
                                        ((src32[x] & 0x00008000) ? 0x07 : 0x00);
                         }

                         src32 += sp4;
                         dst32 += dp4;
                         dst8  += dp;
                    }
                    break;

               case DSPF_ARGB:
                    while (rect.h--) {
                         int x;

                         for (x=0; x<w2; x++) {
                              register u32 s0 = src32[(x<<1)+0];
                              register u32 s1 = src32[(x<<1)+1];

                              dst32[x] = ((s0 & 0x00f80000) >>  8) |
                                         ((s0 & 0x0000fc00) >>  5) |
                                         ((s0 & 0x000000f8) >>  3) |
                                         ((s1 & 0x00f80000) <<  8) |
                                         ((s1 & 0x0000fc00) << 11) |
                                         ((s1 & 0x000000f8) << 13) ;

#ifndef DAVINCI_NO_DITHER
                              if ((s0 & s1) >> 24 == 0xff)
                                   dst8[x] = 0x77;
                              else {
                                   register int pt, da;

                                   z ^= ((z << 13) | (z >> 19));
                                   z += 0x87654321;
                                   pt = s0 - ((s0 & 0xf8000000) >> 3);
                                   da = (((pt >> 29) & 0x07)  + ( ((z&0x1f) - ((pt >> 24) & 0x1f))>>31 )) << 4;

                                   z ^= ((z << 13) | (z >> 19));
                                   z += 0x87654321;
                                   pt = s1 - ((s1 & 0xf8000000) >> 3);
                                   da |= (((pt >> 29) & 0x07) + ( ((z&0x1f) - ((pt >> 24) & 0x1f))>>31 ));


                                   dst8[x] = da;
                              }
#else
                              dst8[x] = ((s0 & 0xe0000000) >> 25) |
                                        ((s1 & 0xe0000000) >> 29) ;
#endif
                         }

                         src32 += sp4;
                         dst32 += dp4;
                         dst8  += dp;
                    }
                    break;

               default:
                    D_ONCE( "unsupported format" );
          }
     }
}

static void
update_rgb( DavinciDriverData     *ddrv,
            DavinciDeviceData     *ddev,
            CoreSurface           *surface,
            CoreSurfaceBufferLock *lock,
            const DFBRegion       *update )
{
     DFBRectangle       rect;
     CoreSurfaceBuffer *buffer;

     D_ASSERT( ddrv != NULL );
     D_ASSERT( ddev != NULL );
     D_ASSERT( surface != NULL );
     D_ASSERT( lock != NULL );
     DFB_REGION_ASSERT_IF( update );

     buffer = lock->buffer;
     D_ASSERT( buffer != NULL );

     if (update)
          rect = DFB_RECTANGLE_INIT_FROM_REGION( update );
     else {
          rect.x = 0;
          rect.y = 0;
          rect.w = surface->config.size.w;
          rect.h = surface->config.size.h;
     }

     dfb_convert_to_rgb16( buffer->format,
                           lock->addr + rect.y * lock->pitch + DFB_BYTES_PER_LINE( buffer->format, rect.x ),
                           lock->pitch,
                           surface->config.size.h,
                           ddrv->fb[OSD0].mem + rect.y * ddev->fix[OSD0].line_length + rect.x * 2,
                           ddev->fix[OSD0].line_length,
                           rect.w, rect.h );
}

static void
enable_osd( DavinciDriverData   *ddrv,
            DavinciOSDLayerData *dosd )
{
     if (!dosd->enable)
          return;

     ioctl( ddrv->fb[OSD0].fd, FBIO_WAITFORVSYNC );

     if (ioctl( ddrv->fb[OSD0].fd, FBIO_ENABLE_DISABLE_ATTRIBUTE_WIN, dosd->alpha ))
          D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_ATTRIBUTE_WIN (fb%d - %d)!\n", OSD0, dosd->alpha );

     if (ioctl( ddrv->fb[OSD0].fd, FBIO_ENABLE_DISABLE_WIN, 1 ))
          D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", OSD0, 1 );

     if (ioctl( ddrv->fb[OSD1].fd, FBIO_ENABLE_DISABLE_WIN, dosd->alpha ))
          D_PERROR( "Davinci/OSD: FBIO_ENABLE_DISABLE_WIN (fb%d - %d)!\n", OSD1, dosd->alpha );

     dosd->enable = false;
}

static DFBResult
osdFlipRegion( CoreLayer             *layer,
               void                  *driver_data,
               void                  *layer_data,
               void                  *region_data,
               CoreSurface           *surface,
               DFBSurfaceFlipFlags    flags,
               CoreSurfaceBufferLock *lock )
{
     CoreSurfaceBuffer   *buffer;
     DavinciDriverData   *ddrv = driver_data;
     DavinciDeviceData   *ddev = ddrv->ddev;
     DavinciOSDLayerData *dosd = layer_data;

     D_DEBUG_AT( Davinci_OSD, "%s()\n", __FUNCTION__ );

     D_ASSERT( surface != NULL );
     D_ASSERT( lock != NULL );
     D_ASSERT( ddrv != NULL );
     D_ASSERT( ddev != NULL );

     buffer = lock->buffer;
     D_ASSERT( buffer != NULL );

     if (buffer->format != DSPF_RGB16) {
          if (DFB_PIXELFORMAT_HAS_ALPHA( buffer->format ))
               update_buffers( ddrv, ddev, surface, lock, NULL );
          else
               update_rgb( ddrv, ddev, surface, lock, NULL );
     }
     else
          davincifb_pan_display( &ddrv->fb[OSD0], &dosd->var0, lock, flags );

     dfb_surface_flip( surface, false );

     enable_osd( ddrv, dosd );

     return DFB_OK;
}

static DFBResult
osdUpdateRegion( CoreLayer             *layer,
                 void                  *driver_data,
                 void                  *layer_data,
                 void                  *region_data,
                 CoreSurface           *surface,
                 const DFBRegion       *update,
                 CoreSurfaceBufferLock *lock )
{
     CoreSurfaceBuffer   *buffer;
     DavinciDriverData   *ddrv = driver_data;
     DavinciDeviceData   *ddev = ddrv->ddev;
     DavinciOSDLayerData *dosd = layer_data;

     D_DEBUG_AT( Davinci_OSD, "%s()\n", __FUNCTION__ );

     D_ASSERT( surface != NULL );
     D_ASSERT( lock != NULL );
     D_ASSERT( ddrv != NULL );
     D_ASSERT( ddev != NULL );

     buffer = lock->buffer;
     D_ASSERT( buffer != NULL );

     if (buffer->format != DSPF_RGB16) {
          if (DFB_PIXELFORMAT_HAS_ALPHA( buffer->format ))
               update_buffers( ddrv, ddev, surface, lock, update );
          else
               update_rgb( ddrv, ddev, surface, lock, update );
     }

     enable_osd( ddrv, dosd );

     return DFB_OK;
}

const DisplayLayerFuncs davinciOSDLayerFuncs = {
     LayerDataSize:      osdLayerDataSize,
     InitLayer:          osdInitLayer,

     TestRegion:         osdTestRegion,
     SetRegion:          osdSetRegion,
     RemoveRegion:       osdRemoveRegion,
     FlipRegion:         osdFlipRegion,
     UpdateRegion:       osdUpdateRegion
};

