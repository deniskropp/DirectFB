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

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#if defined(HAVE_SYSIO)
# include <sys/io.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <pthread.h>

#include <core/fusion/shmalloc.h>
#include <core/fusion/reactor.h>
#include <core/fusion/arena.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/fbdev/fbdev.h>
#include <core/layers.h>
#include <core/gfxcard.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>
#include <core/state.h>
#include <core/windows.h>

#include <gfx/convert.h>

#include <misc/mem.h>
#include <misc/util.h>


FBDev *dfb_fbdev = NULL;


static int
primaryLayerDataSize     ();
     
static DFBResult
primaryInitLayer         ( GraphicsDevice             *device,
                           DisplayLayer               *layer,
                           DisplayLayerInfo           *layer_info,
                           DFBDisplayLayerConfig      *default_config,
                           DFBColorAdjustment         *default_adj,
                           void                       *driver_data,
                           void                       *layer_data );

static DFBResult
primaryEnable            ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data );

static DFBResult
primaryDisable           ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data );

static DFBResult
primaryTestConfiguration ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           DFBDisplayLayerConfigFlags *failed );

static DFBResult
primarySetConfiguration  ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config );

static DFBResult
primarySetOpacity        ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        opacity );
     
static DFBResult
primarySetScreenLocation ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           float                       x,
                           float                       y,
                           float                       width,
                           float                       height );
     
static DFBResult
primarySetSrcColorKey    ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        r,
                           __u8                        g,
                           __u8                        b );
     
static DFBResult
primarySetDstColorKey    ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        r,
                           __u8                        g,
                           __u8                        b );
     
static DFBResult
primaryFlipBuffers       ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBSurfaceFlipFlags         flags );
     
static DFBResult
primarySetColorAdjustment( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBColorAdjustment         *adj );

static DFBResult
primarySetPalette        ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           CorePalette                *palette );

          
static DFBResult
primaryAllocateSurface   ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           CoreSurface               **surface );

static DFBResult
primaryReallocateSurface ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           CoreSurface                *surface );

static DisplayLayerFuncs primaryLayerFuncs = {
     LayerDataSize:      primaryLayerDataSize,
     InitLayer:          primaryInitLayer,
     Enable:             primaryEnable,
     Disable:            primaryDisable,
     TestConfiguration:  primaryTestConfiguration,
     SetConfiguration:   primarySetConfiguration,
     SetOpacity:         primarySetOpacity,
     SetScreenLocation:  primarySetScreenLocation,
     SetSrcColorKey:     primarySetSrcColorKey,
     SetDstColorKey:     primarySetDstColorKey,
     FlipBuffers:        primaryFlipBuffers,
     SetColorAdjustment: primarySetColorAdjustment,
     SetPalette:         primarySetPalette,
          
     AllocateSurface:    primaryAllocateSurface,
     ReallocateSurface:  primaryReallocateSurface,
     /* default DeallocateSurface copes with our chunkless video buffers */
};

static DFBResult dfb_fbdev_read_modes();
static DFBResult dfb_fbdev_set_gamma_ramp( DFBSurfacePixelFormat format );
#ifdef SUPPORT_RGB332
static DFBResult dfb_fbdev_set_rgb332_palette();
#endif
static DFBResult dfb_fbdev_pan( int buffer );
static DFBResult dfb_fbdev_set_mode( DisplayLayer          *layer,
                                     VideoMode             *mode,
                                     DFBDisplayLayerConfig *config );

static inline
void waitretrace (void)
{
#if defined(HAVE_INB_OUTB_IOPL)
     iopl(3);

     if (!(inb (0x3cc) & 1)) {
       while ((inb (0x3ba) & 0x8))
	 ;
       
       while (!(inb (0x3ba) & 0x8))
	 ;
     }
     else {
       while ((inb (0x3da) & 0x8))
	 ;
       
       while (!(inb (0x3da) & 0x8))
	 ;
     }
#endif
}


static DFBResult dfb_fbdev_open()
{
     if (dfb_config->fb_device) {
          dfb_fbdev->fd = open( dfb_config->fb_device, O_RDWR );
          if (dfb_fbdev->fd < 0) {
               PERRORMSG( "DirectFB/core/fbdev: Error opening `%s'!\n",
                          dfb_config->fb_device);

               return errno2dfb( errno );
          }
     }
     else if (getenv( "FRAMEBUFFER" ) && *getenv( "FRAMEBUFFER" ) != '\0') {
          dfb_fbdev->fd = open( getenv ("FRAMEBUFFER"), O_RDWR );
          if (dfb_fbdev->fd < 0) {
               PERRORMSG( "DirectFB/core/fbdev: Error opening `%s'!\n",
                          getenv ("FRAMEBUFFER"));

               return errno2dfb( errno );
          }
     }
     else {
          dfb_fbdev->fd = open( "/dev/fb0", O_RDWR );
          if (dfb_fbdev->fd < 0) {
               if (errno == ENOENT) {
                    dfb_fbdev->fd = open( "/dev/fb/0", O_RDWR );
                    if (dfb_fbdev->fd < 0) {
                         if (errno == ENOENT) {
                              PERRORMSG( "DirectFB/core/fbdev: Couldn't open "
                                         "neither `/dev/fb0' nor `/dev/fb/0'!\n" );
                         }
                         else {
                              PERRORMSG( "DirectFB/core/fbdev: "
                                         "Error opening `/dev/fb/0'!\n" );
                         }

                         return errno2dfb( errno );
                    }
               }
               else {
                    PERRORMSG( "DirectFB/core/fbdev: Error opening `/dev/fb0'!\n");

                    return errno2dfb( errno );
               }
          }
     }

     return DFB_OK;
}

/** public **/

DFBResult dfb_fbdev_initialize()
{
     DFBResult ret;

     if (dfb_fbdev) {
          BUG( "dfb_fbdev_init() already called!" );
          return DFB_BUG;
     }

     dfb_fbdev = (FBDev*) DFBCALLOC( 1, sizeof(FBDev) );

     Sfbdev = (FBDevShared*) shcalloc( 1, sizeof(FBDevShared) );


     ret = dfb_fbdev_open();
     if (ret) {
          shfree( Sfbdev );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return ret;
     }

     /* Retrieve fixed informations like video ram size */
     if (ioctl( dfb_fbdev->fd, FBIOGET_FSCREENINFO, &Sfbdev->fix ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not get fixed screen information!\n" );
          shfree( Sfbdev );
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }
     
     /* Map the framebuffer */
     dfb_fbdev->framebuffer_base = mmap( NULL, Sfbdev->fix.smem_len,
                                         PROT_READ | PROT_WRITE, MAP_SHARED,
                                         dfb_fbdev->fd, 0 );
     if ((int)(dfb_fbdev->framebuffer_base) == -1) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not mmap the framebuffer!\n");
          shfree( Sfbdev );
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }
     
     if (ioctl( dfb_fbdev->fd, FBIOGET_VSCREENINFO, &Sfbdev->orig_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not get variable screen information!\n" );
          shfree( Sfbdev );
          munmap( dfb_fbdev->framebuffer_base, Sfbdev->fix.smem_len );
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }

     Sfbdev->current_var = Sfbdev->orig_var;
     Sfbdev->current_var.accel_flags = 0;

     if (ioctl( dfb_fbdev->fd, FBIOPUT_VSCREENINFO, &Sfbdev->current_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not disable console acceleration!\n" );
          shfree( Sfbdev );
          munmap( dfb_fbdev->framebuffer_base, Sfbdev->fix.smem_len );
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }

     Sfbdev->orig_cmap.start  = 0;
     Sfbdev->orig_cmap.len    = 256;
     Sfbdev->orig_cmap.red    = (__u16*)shmalloc( 2 * 256 );
     Sfbdev->orig_cmap.green  = (__u16*)shmalloc( 2 * 256 );
     Sfbdev->orig_cmap.blue   = (__u16*)shmalloc( 2 * 256 );
     Sfbdev->orig_cmap.transp = (__u16*)shmalloc( 2 * 256 );

     if (ioctl( dfb_fbdev->fd, FBIOGETCMAP, &Sfbdev->orig_cmap ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not retrieve palette for backup!\n" );
          shfree( Sfbdev->orig_cmap.red );
          shfree( Sfbdev->orig_cmap.green );
          shfree( Sfbdev->orig_cmap.blue );
          shfree( Sfbdev->orig_cmap.transp );
          Sfbdev->orig_cmap.len = 0;
     }

     /* Register primary layer functions */
     dfb_layers_register( NULL, NULL, &primaryLayerFuncs );

#ifndef FUSION_FAKE
     arena_add_shared_field( dfb_core->arena, Sfbdev, "Sfbdev" );
#endif

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult dfb_fbdev_join()
{
     DFBResult ret;
     struct fb_var_screeninfo var;

     if (dfb_fbdev) {
          BUG( "dfb_fbdev_join() called and display != NULL" );
          return DFB_BUG;
     }

     dfb_fbdev = (FBDev*)DFBCALLOC( 1, sizeof(FBDev) );

     arena_get_shared_field( dfb_core->arena, (void**) &Sfbdev, "Sfbdev" );

     /* Open framebuffer device */
     ret = dfb_fbdev_open();
     if (ret) {
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;
          return ret;
     }

     /* Map the framebuffer */
     dfb_fbdev->framebuffer_base = mmap( NULL, Sfbdev->fix.smem_len,
                                         PROT_READ | PROT_WRITE, MAP_SHARED,
                                         dfb_fbdev->fd, 0 );
     if ((int)(dfb_fbdev->framebuffer_base) == -1) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not mmap the framebuffer!\n");
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }
     
     /*
      * Disable console acceleration.
      */
     if (ioctl( dfb_fbdev->fd, FBIOGET_VSCREENINFO, &var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not get variable screen information!\n" );
          munmap( dfb_fbdev->framebuffer_base, Sfbdev->fix.smem_len );
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }

     var.accel_flags = 0;

     if (ioctl( dfb_fbdev->fd, FBIOPUT_VSCREENINFO, &var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not disable console acceleration!\n" );
          munmap( dfb_fbdev->framebuffer_base, Sfbdev->fix.smem_len );
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }

     /* Register primary layer functions */
     dfb_layers_register( NULL, NULL, &primaryLayerFuncs );
     
     return DFB_OK;
}
#endif

DFBResult dfb_fbdev_shutdown( bool emergency )
{
     VideoMode *m;
     
     if (!dfb_fbdev)
          return DFB_OK;

     m = Sfbdev->modes;
     while (m) {
          VideoMode *next = m->next;
          shfree( m );
          m = next;
     }

     if (ioctl( dfb_fbdev->fd, FBIOPUT_VSCREENINFO, &Sfbdev->orig_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not restore variable screen information!\n" );
     }

     if (Sfbdev->orig_cmap.len) {
          if (ioctl( dfb_fbdev->fd, FBIOPUTCMAP, &Sfbdev->orig_cmap ) < 0)
               PERRORMSG( "DirectFB/core/fbdev: "
                          "Could not restore palette!\n" );

          shfree( Sfbdev->orig_cmap.red );
          shfree( Sfbdev->orig_cmap.green );
          shfree( Sfbdev->orig_cmap.blue );
          shfree( Sfbdev->orig_cmap.transp );
     }

     munmap( dfb_fbdev->framebuffer_base, Sfbdev->fix.smem_len );
     
     close( dfb_fbdev->fd );

     DFBFREE( dfb_fbdev );
     dfb_fbdev = NULL;

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult dfb_fbdev_leave( bool emergency )
{
     munmap( dfb_fbdev->framebuffer_base, Sfbdev->fix.smem_len );
     
     close( dfb_fbdev->fd );

     DFBFREE( dfb_fbdev );
     dfb_fbdev = NULL;

     return DFB_OK;
}
#endif




VideoMode *dfb_fbdev_modes()
{
     return Sfbdev->modes;
}

DFBResult dfb_fbdev_wait_vsync()
{
     if (dfb_config->pollvsync_none)
          return DFB_OK;

#ifdef FBIO_WAITFORVSYNC
     dfb_gfxcard_sync();
     if (ioctl( dfb_fbdev->fd, FBIO_WAITFORVSYNC ))
#endif
          waitretrace();

     return DFB_OK;
}

static DFBResult init_modes()
{
     dfb_fbdev_read_modes();

     if (!Sfbdev->modes) {
          /* try to use current mode*/
          Sfbdev->modes = (VideoMode*) shcalloc( 1, sizeof(VideoMode) );

          Sfbdev->modes->xres = Sfbdev->orig_var.xres;
          Sfbdev->modes->yres = Sfbdev->orig_var.yres;
          Sfbdev->modes->bpp  = Sfbdev->orig_var.bits_per_pixel;
          Sfbdev->modes->hsync_len = Sfbdev->orig_var.hsync_len;
          Sfbdev->modes->vsync_len = Sfbdev->orig_var.vsync_len;
          Sfbdev->modes->left_margin = Sfbdev->orig_var.left_margin;
          Sfbdev->modes->right_margin = Sfbdev->orig_var.right_margin;
          Sfbdev->modes->upper_margin = Sfbdev->orig_var.upper_margin;
          Sfbdev->modes->lower_margin = Sfbdev->orig_var.lower_margin;
          Sfbdev->modes->pixclock = Sfbdev->orig_var.pixclock;


          if (Sfbdev->orig_var.sync & FB_SYNC_HOR_HIGH_ACT)
               Sfbdev->modes->hsync_high = 1;
          if (Sfbdev->orig_var.sync & FB_SYNC_VERT_HIGH_ACT)
               Sfbdev->modes->vsync_high = 1;

          if (Sfbdev->orig_var.vmode & FB_VMODE_INTERLACED)
               Sfbdev->modes->laced = 1;
          if (Sfbdev->orig_var.vmode & FB_VMODE_DOUBLE)
               Sfbdev->modes->doubled = 1;

          if (dfb_fbdev_set_mode(NULL, Sfbdev->modes, NULL)) {
               ERRORMSG("DirectFB/core/fbdev: "
                        "No supported modes found in /etc/fb.modes and "
                        "current mode not supported!\n");

               ERRORMSG( "DirectFB/core/fbdev: Current mode's pixelformat: "
                         "rgba %d/%d, %d/%d, %d/%d, %d/%d (%dbit)\n",
                         Sfbdev->orig_var.red.length,
                         Sfbdev->orig_var.red.offset,
                         Sfbdev->orig_var.green.length,
                         Sfbdev->orig_var.green.offset,
                         Sfbdev->orig_var.blue.length,
                         Sfbdev->orig_var.blue.offset,
                         Sfbdev->orig_var.transp.length,
                         Sfbdev->orig_var.transp.offset,
                         Sfbdev->orig_var.bits_per_pixel );

               return DFB_INIT;
          }
     }

     return DFB_OK;
}

/** primary layer functions **/

static int
primaryLayerDataSize     ()
{
     return 0;
}
     
static DFBResult
primaryInitLayer         ( GraphicsDevice             *device,
                           DisplayLayer               *layer,
                           DisplayLayerInfo           *layer_info,
                           DFBDisplayLayerConfig      *default_config,
                           DFBColorAdjustment         *default_adj,
                           void                       *driver_data,
                           void                       *layer_data )
{
     DFBResult  ret;
     VideoMode *default_mode;

     /* initialize mode table */
     ret = init_modes();
     if (ret)
          return ret;

     default_mode = dfb_fbdev->shared->modes;

     /* set capabilities and type */
     layer_info->desc.caps = DLCAPS_SURFACE;
     layer_info->desc.type = DLTF_GRAPHICS;

     /* set name */
     snprintf( layer_info->name,
               DFB_DISPLAY_LAYER_INFO_NAME_LENGTH, "Primary Layer" );

     /* fill out the default configuration */
     default_config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                                   DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     default_config->buffermode  = DLBM_FRONTONLY;

     if (dfb_config->mode.width)
          default_config->width  = dfb_config->mode.width;
     else
          default_config->width  = default_mode->xres;

     if (dfb_config->mode.height)
          default_config->height = dfb_config->mode.height;
     else
          default_config->height = default_mode->yres;
     
     if (dfb_config->mode.format != DSPF_UNKNOWN)
          default_config->pixelformat = dfb_config->mode.format;
     else if (dfb_config->mode.depth > 0)
          default_config->pixelformat = dfb_pixelformat_for_depth( dfb_config->mode.depth );
     else {
          default_config->pixelformat = DSPF_RGB16;
          
          if (dfb_fbdev_set_mode( NULL, NULL, default_config ))
               default_config->pixelformat = dfb_pixelformat_for_depth( Sfbdev->orig_var.bits_per_pixel );
     }

     return DFB_OK;
}

static DFBResult
primaryEnable            ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data )
{
     /* always enabled */
     return DFB_OK;
}

static DFBResult
primaryDisable           ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data )
{
     /* cannot be disabled */
     return DFB_UNSUPPORTED;
}

static DFBResult
primaryTestConfiguration ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           DFBDisplayLayerConfigFlags *failed )
{
     VideoMode                  *videomode = NULL;
     DFBDisplayLayerConfigFlags  fail = 0;

     if (config->flags & (DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT)) {
#ifndef SUPPORT_RGB332
          if (config->pixelformat == DSPF_RGB332)
               fail |= DLCONF_PIXELFORMAT;
#endif

          videomode = Sfbdev->modes;
          while (videomode) {
               if (videomode->xres == config->width  &&
                   videomode->yres == config->height)
                    break;

               videomode = videomode->next;
          }

          if (!videomode)
               fail |= (config->flags & (DLCONF_WIDTH | DLCONF_HEIGHT));
     }
     if (config->flags & DLCONF_BUFFERMODE) {
          if (dfb_fbdev_set_mode( NULL, videomode, config ))
               fail |= DLCONF_BUFFERMODE;
     }
     else if (videomode) {
          if (dfb_fbdev_set_mode( NULL, videomode, config ))
               fail |= (config->flags & (DLCONF_WIDTH  |
                                         DLCONF_HEIGHT | DLCONF_PIXELFORMAT));
     }

     if (config->flags & DLCONF_OPTIONS  &&  config->options)
          fail |= DLCONF_OPTIONS;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
primarySetConfiguration  ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config )
{
     VideoMode *videomode;

     videomode = Sfbdev->modes;
     while (videomode) {
          if (videomode->xres == config->width  &&
              videomode->yres == config->height)
               break;

          videomode = videomode->next;
     }

     if (!videomode)
          return DFB_UNSUPPORTED;

     return dfb_fbdev_set_mode( layer, videomode, config );
}

static DFBResult
primarySetOpacity        ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        opacity )
{
     /* opacity is not supported for normal primary layer */
     if (opacity != 0xFF)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}
     
static DFBResult
primarySetScreenLocation ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           float                       x,
                           float                       y,
                           float                       width,
                           float                       height )
{
     /* can only be fullscreen (0, 0, 1, 1) */
     if (x != 0  ||  y != 0  ||  width != 1  ||  height != 1)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}
     
static DFBResult
primarySetSrcColorKey    ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        r,
                           __u8                        g,
                           __u8                        b )
{
     return DFB_UNSUPPORTED;
}
     
static DFBResult
primarySetDstColorKey    ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           __u8                        r,
                           __u8                        g,
                           __u8                        b )
{
     return DFB_UNSUPPORTED;
}
     
static DFBResult
primaryFlipBuffers       ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBSurfaceFlipFlags         flags )
{
     DFBResult    ret;
     CoreSurface *surface = dfb_layer_surface( layer );

     if ((flags & DSFLIP_WAITFORSYNC) && !dfb_config->pollvsync_after)
          dfb_fbdev_wait_vsync();
     
     ret = dfb_fbdev_pan( surface->back_buffer->video.offset ? 1 : 0 );
     if (ret)
          return ret;

     if ((flags & DSFLIP_WAITFORSYNC) && dfb_config->pollvsync_after)
          dfb_fbdev_wait_vsync();
          
     dfb_surface_flip_buffers( surface );

     return DFB_OK;
}
     
static DFBResult
primarySetColorAdjustment( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBColorAdjustment         *adj )
{
     /* maybe we could use the gamma ramp here */
     return DFB_UNSUPPORTED;
}

static DFBResult
primarySetPalette ( DisplayLayer               *layer,
                    void                       *driver_data,
                    void                       *layer_data,
                    CorePalette                *palette )
{
     int            i;
     struct fb_cmap cmap;

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( palette != NULL );

     cmap.start  = 0;
     cmap.len    = palette->num_entries;
     cmap.red    = (__u16*)alloca( 2 * cmap.len );
     cmap.green  = (__u16*)alloca( 2 * cmap.len );
     cmap.blue   = (__u16*)alloca( 2 * cmap.len );
     cmap.transp = (__u16*)alloca( 2 * cmap.len );

     for (i = 0; i < cmap.len; i++) {
          cmap.red[i]     = palette->entries[i].r;
          cmap.green[i]   = palette->entries[i].g;
          cmap.blue[i]    = palette->entries[i].b;
          cmap.transp[i]  = palette->entries[i].a;

          cmap.red[i]    |= cmap.red[i] << 8;
          cmap.green[i]  |= cmap.green[i] << 8;
          cmap.blue[i]   |= cmap.blue[i] << 8;
          cmap.transp[i] |= cmap.transp[i] << 8;
     }

     if (ioctl( dfb_fbdev->fd, FBIOPUTCMAP, &cmap ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not set the palette!\n" );

          return errno2dfb(errno);
     }
     
     return DFB_OK;
}

static DFBResult
primaryAllocateSurface   ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           CoreSurface               **ret_surface )
{
     DFBResult               ret;
     CoreSurface            *surface;
     DFBSurfaceCapabilities  caps = DSCAPS_VIDEOONLY;

     /* determine further capabilities */
     if (config->buffermode != DLBM_FRONTONLY)
          caps |= DSCAPS_FLIPPING;

     /* allocate surface object */
     surface = (CoreSurface*) fusion_object_create( dfb_gfxcard_surface_pool() );
     if (!surface)
          return DFB_FAILURE;

     /* reallocation just needs an allocated buffer structure */
     surface->back_buffer  =
     surface->front_buffer = shcalloc( 1, sizeof(SurfaceBuffer) );

     if (!surface->front_buffer) {
          shfree( surface );
          return DFB_NOSYSTEMMEMORY;
     }
     
     /* initialize surface structure */
     ret = dfb_surface_init( surface, config->width, config->height,
                             config->pixelformat, caps, NULL );
     if (ret) {
          shfree( surface );
          return ret;
     }

     /* return surface */
     *ret_surface = surface;

     return DFB_OK;
}

static DFBResult
primaryReallocateSurface ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBDisplayLayerConfig      *config,
                           CoreSurface                *surface )
{
     /* reallocation is done during SetConfiguration,
        because the pitch can only be determined AFTER setting the mode */
     if (DFB_PIXELFORMAT_IS_INDEXED(config->pixelformat) && !surface->palette) {
          CorePalette *palette = dfb_palette_create( 256 );
          if (!palette)
               return DFB_FAILURE;

          dfb_surface_set_palette( surface, palette );

          dfb_palette_unref( palette );
     }

     return DFB_OK;
}


/** fbdev internal **/

static int dfb_fbdev_compatible_format( struct fb_var_screeninfo *var,
                                        int al, int rl, int gl, int bl,
                                        int ao, int ro, int go, int bo )
{
     int ah, rh, gh, bh;
     int vah, vrh, vgh, vbh;

     ah = al + ao - 1;
     rh = rl + ro - 1;
     gh = gl + go - 1;
     bh = bl + bo - 1;

     vah = var->transp.length + var->transp.offset - 1;
     vrh = var->red.length + var->red.offset - 1;
     vgh = var->green.length + var->green.offset - 1;
     vbh = var->blue.length + var->blue.offset - 1;

     if (ah == vah && al >= var->transp.length &&
         rh == vrh && rl >= var->red.length &&
         gh == vgh && gl >= var->green.length &&
         bh == vbh && bl >= var->blue.length)
          return 1;

     return 0;
}

static DFBSurfacePixelFormat dfb_fbdev_get_pixelformat( struct fb_var_screeninfo *var )
{
     switch (var->bits_per_pixel) {

          case 8:
#ifdef SUPPORT_RGB332
/*
               This check is omitted, since we want to use RGB332 even if the
               hardware uses a palette (in that case we initialize a calculated
               one to have correct colors)

               if (fbdev_compatible_format( var, 0, 3, 3, 2, 0, 5, 2, 0 ))*/
                    return DSPF_RGB332;

               break;
#endif
               return DSPF_LUT8;
          case 15:
               if (dfb_fbdev_compatible_format( var, 0, 5, 5, 5, 0, 10, 5, 0 ) |
                   dfb_fbdev_compatible_format( var, 1, 5, 5, 5,15, 10, 5, 0 ) )
                    return DSPF_RGB15;

               break;

          case 16:
               if (dfb_fbdev_compatible_format( var, 0, 5, 5, 5, 0, 10, 5, 0 ) |
                   dfb_fbdev_compatible_format( var, 1, 5, 5, 5,15, 10, 5, 0 ) )
                    return DSPF_RGB15;

               if (dfb_fbdev_compatible_format( var, 0, 5, 6, 5, 0, 11, 5, 0 ))
                    return DSPF_RGB16;

               break;

          case 24:
               if (dfb_fbdev_compatible_format( var, 0, 8, 8, 8, 0, 16, 8, 0 ))
                    return DSPF_RGB24;

               break;

          case 32:
               if (dfb_fbdev_compatible_format( var, 0, 8, 8, 8, 0, 16, 8, 0 ))
                    return DSPF_RGB32;

               if (dfb_fbdev_compatible_format( var, 8, 8, 8, 8, 24, 16, 8, 0 ))
                    return DSPF_ARGB;

               break;
     }

     ERRORMSG( "DirectFB/core/fbdev: Unsupported pixelformat: "
               "rgba %d/%d, %d/%d, %d/%d, %d/%d (%dbit)\n",
                var->red.length,    var->red.offset,
                var->green.length,  var->green.offset,
                var->blue.length,   var->blue.offset,
                var->transp.length, var->transp.offset,
                var->bits_per_pixel );

     return DSPF_UNKNOWN;
}

/*
 * pans display (flips buffer) using fbdev ioctl
 */
static DFBResult dfb_fbdev_pan( int buffer )
{
     struct fb_var_screeninfo var;

     var = Sfbdev->current_var;

     if (var.yres_virtual < var.yres*(buffer+1)) {
          BUG( "panning buffer out of range" );
          return DFB_BUG;
     }

     var.xoffset = 0;
     var.yoffset = var.yres * buffer;

     dfb_gfxcard_sync();

     if (ioctl( dfb_fbdev->fd, FBIOPAN_DISPLAY, &var ) < 0) {
          int erno = errno;

          PERRORMSG( "DirectFB/core/fbdev: Panning display failed!\n" );

          return errno2dfb( erno );
     }

     Sfbdev->current_var = var;

     return DFB_OK;
}

/*
 * sets (if layer != NULL) or tests (if layer == NULL) video mode,
 * sets virtual y-resolution according to buffermode
 */
static DFBResult dfb_fbdev_set_mode( DisplayLayer          *layer,
                                     VideoMode             *mode,
                                     DFBDisplayLayerConfig *config )
{
     int                      vyres;
     struct fb_var_screeninfo var;

     DEBUGMSG("DirectFB/core/fbdev: dfb_fbdev_set_mode (layer: %p, "
              "mode: %p, buffermode: %d)\n", layer, mode,
              config ? config->buffermode : DLBM_FRONTONLY);

     if (!mode)
          mode = Sfbdev->current_mode ? Sfbdev->current_mode : Sfbdev->modes;

     vyres = mode->yres;
     
     var = Sfbdev->current_var;

     var.xoffset = 0;
     var.yoffset = 0;

     if (config) {
          if (config->buffermode == DLBM_BACKVIDEO)
               vyres <<= 1;

          var.bits_per_pixel = DFB_BYTES_PER_PIXEL(config->pixelformat) * 8;

          switch (config->pixelformat) {
               case DSPF_RGB15:
                    var.red.length   = 5;
                    var.green.length = 5;
                    var.blue.length  = 5;
                    var.red.offset   = 10;
                    var.green.offset = 5;
                    var.blue.offset  = 0;
                    break;
     
               case DSPF_RGB16:
                    var.red.length   = 5;
                    var.green.length = 6;
                    var.blue.length  = 5;
                    var.red.offset   = 11;
                    var.green.offset = 5;
                    var.blue.offset  = 0;
                    break;
     
               default:
                    ;
          }
     }
     else
          var.bits_per_pixel = mode->bpp;

     var.activate = layer ? FB_ACTIVATE_NOW : FB_ACTIVATE_TEST;

     var.xres = mode->xres;
     var.yres = mode->yres;
     var.xres_virtual = mode->xres;
     var.yres_virtual = vyres;

     var.pixclock = mode->pixclock;
     var.left_margin = mode->left_margin;
     var.right_margin = mode->right_margin;
     var.upper_margin = mode->upper_margin;
     var.lower_margin = mode->lower_margin;
     var.hsync_len = mode->hsync_len;
     var.vsync_len = mode->vsync_len;

     var.sync = 0;
     if (mode->hsync_high)
          var.sync |= FB_SYNC_HOR_HIGH_ACT;
     if (mode->vsync_high)
          var.sync |= FB_SYNC_VERT_HIGH_ACT;
     if (mode->csync_high)
          var.sync |= FB_SYNC_COMP_HIGH_ACT;
     if (mode->sync_on_green)
          var.sync |= FB_SYNC_ON_GREEN;
     if (mode->external_sync)
          var.sync |= FB_SYNC_EXT;

     var.vmode = 0;
     if (mode->laced)
          var.vmode |= FB_VMODE_INTERLACED;
     if (mode->doubled)
          var.vmode |= FB_VMODE_DOUBLE;

     if (ioctl( dfb_fbdev->fd, FBIOPUT_VSCREENINFO, &var ) < 0) {
          int erno = errno;

          if (layer)
               PERRORMSG( "DirectFB/core/fbdev: "
                          "Could not set video mode (FBIOPUT_VSCREENINFO)!\n" );

          return errno2dfb( erno );
     }

     /* If layer is NULL the mode was only tested, otherwise apply changes. */
     if (layer) {
          struct fb_fix_screeninfo  fix;
          DFBSurfacePixelFormat     format;
          CoreSurface              *surface = dfb_layer_surface( layer );

          ioctl( dfb_fbdev->fd, FBIOGET_VSCREENINFO, &var );


          format = dfb_fbdev_get_pixelformat( &var );
          if (format == DSPF_UNKNOWN || var.yres_virtual < vyres) {
               /* restore mode */
               ioctl( dfb_fbdev->fd, FBIOPUT_VSCREENINFO, &Sfbdev->current_var );
               return DFB_UNSUPPORTED;
          }

          if (!config)
               return DFB_OK;
          
          /* force format for 8bit */
          if (format != config->pixelformat && DFB_BYTES_PER_PIXEL(format) == 1) {
               format = config->pixelformat;
          }

#ifdef SUPPORT_RGB332
          if (config->pixelformat == DSPF_RGB332)
               dfb_fbdev_set_rgb332_palette();
          else
#endif
          dfb_fbdev_set_gamma_ramp( config->pixelformat );

          Sfbdev->current_var = var;
          Sfbdev->current_mode = mode;
          
          surface->width  = mode->xres;
          surface->height = mode->yres;
          surface->format = format;
          
          /* To get the new pitch */
          ioctl( dfb_fbdev->fd, FBIOGET_FSCREENINFO, &fix );
          
          dfb_gfxcard_adjust_heap_offset( var.yres_virtual * fix.line_length );

          surface->front_buffer->surface = surface;
          surface->front_buffer->policy = CSP_VIDEOONLY;
          surface->front_buffer->video.health = CSH_STORED;
          surface->front_buffer->video.pitch = fix.line_length;
          surface->front_buffer->video.offset = 0;

          switch (config->buffermode) {
               case DLBM_FRONTONLY:
                    surface->caps &= ~DSCAPS_FLIPPING;
                    if (surface->back_buffer != surface->front_buffer) {
                         if (surface->back_buffer->system.health)
                              shfree( surface->back_buffer->system.addr );

                         shfree( surface->back_buffer );

                         surface->back_buffer = surface->front_buffer;
                    }
                    break;
               case DLBM_BACKVIDEO:
                    surface->caps |= DSCAPS_FLIPPING;
                    if (surface->back_buffer == surface->front_buffer) {
                         surface->back_buffer = shcalloc( 1, sizeof(SurfaceBuffer) );
                    }
                    else {
                         if (surface->back_buffer->system.health)
                              shfree( surface->back_buffer->system.addr );

                         surface->back_buffer->system.health = CSH_INVALID;
                    }
                    surface->back_buffer->surface = surface;
                    surface->back_buffer->policy = CSP_VIDEOONLY;
                    surface->back_buffer->video.health = CSH_STORED;
                    surface->back_buffer->video.pitch = fix.line_length;
                    surface->back_buffer->video.offset =
                                   surface->back_buffer->video.pitch * var.yres;
                    break;
               case DLBM_BACKSYSTEM:
                    surface->caps |= DSCAPS_FLIPPING;
                    if (surface->back_buffer == surface->front_buffer) {
                         surface->back_buffer = shcalloc( 1, sizeof(SurfaceBuffer) );
                    }
                    surface->back_buffer->surface = surface;
                    surface->back_buffer->policy = CSP_SYSTEMONLY;
                    surface->back_buffer->video.health = CSH_INVALID;
                    surface->back_buffer->system.health = CSH_STORED;
                    surface->back_buffer->system.pitch =
                         (DFB_BYTES_PER_LINE(format, var.xres) + 3) & ~3;

                    if (surface->back_buffer->system.addr)
                         shfree( surface->back_buffer->system.addr );

                    surface->back_buffer->system.addr =
                         shmalloc( surface->back_buffer->system.pitch * var.yres );
                    break;
          }

          dfb_fbdev_pan(0);

          dfb_gfxcard_after_set_var();

          dfb_surface_notify_listeners( surface, CSNF_SIZEFORMAT | CSNF_FLIP |
                                                 CSNF_VIDEO | CSNF_SYSTEM );
     }

     return DFB_OK;
}

/*
 * parses video modes in /etc/fb.modes and stores them in Sfbdev->modes
 * (to be replaced by DirectFB's own config system
 */
static DFBResult dfb_fbdev_read_modes()
{
     FILE *fp;
     char line[80],label[32],value[16];
     int geometry=0, timings=0;
     int dummy;
     VideoMode temp_mode;
     VideoMode *m = Sfbdev->modes;

     if (!(fp = fopen("/etc/fb.modes","r")))
          return errno2dfb( errno );

     while (fgets(line,79,fp)) {
          if (sscanf(line, "mode \"%31[^\"]\"",label) == 1) {
               memset( &temp_mode, 0, sizeof(VideoMode) );
               geometry = 0;
               timings = 0;
               while (fgets(line,79,fp) && !(strstr(line,"endmode"))) {
                    if (5 == sscanf(line," geometry %d %d %d %d %d", &temp_mode.xres, &temp_mode.yres, &dummy, &dummy, &temp_mode.bpp)) {
                         geometry = 1;
                    }
                    else if (7 == sscanf(line," timings %d %d %d %d %d %d %d", &temp_mode.pixclock, &temp_mode.left_margin,  &temp_mode.right_margin,
                                    &temp_mode.upper_margin, &temp_mode.lower_margin, &temp_mode.hsync_len,    &temp_mode.vsync_len)) {
                         timings = 1;
                    }
                    else if (1 == sscanf(line, " hsync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.hsync_high = 1;
                    }
                    else if (1 == sscanf(line, " vsync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.vsync_high = 1;
                    }
                    else if (1 == sscanf(line, " csync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.csync_high = 1;
                    }
                    else if (1 == sscanf(line, " laced %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.laced = 1;
                    }
                    else if (1 == sscanf(line, " double %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.doubled = 1;
                    }
                    else if (1 == sscanf(line, " gsync %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.sync_on_green = 1;
                    }
                    else if (1 == sscanf(line, " extsync %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.external_sync = 1;
                    }
               }
               if (geometry &&
                   timings &&
                   !dfb_fbdev_set_mode(NULL, &temp_mode, DLBM_FRONTONLY))
               {
                    if (!m) {
                         Sfbdev->modes = shcalloc(1, sizeof(VideoMode));
                         m = Sfbdev->modes;
                    }
                    else {
                         m->next = shcalloc(1, sizeof(VideoMode));
                         m = m->next;
                    }
                    memcpy (m, &temp_mode, sizeof(VideoMode));
                    DEBUGMSG( "DirectFB/core/fbdev: %20s %4dx%4d  %s%s\n", label, temp_mode.xres, temp_mode.yres,
                              temp_mode.laced ? "interlaced " : "", temp_mode.doubled ? "doublescan" : "" );
               }
          }
     }

     fclose (fp);

     return DFB_OK;
}

/*
 * some fbdev drivers use the palette as gamma ramp in >8bpp modes, to have
 * correct colors, the gamme ramp has to be initialized.
 */

static __u16 dfb_fbdev_calc_gamma(int n, int max)
{
    int ret = 65535.0 * ((float)((float)n/(max)));
    if (ret > 65535) ret = 65535;
    if (ret <     0) ret =     0;
    return ret;
}


static DFBResult dfb_fbdev_set_gamma_ramp( DFBSurfacePixelFormat format )
{
     int i;

     int red_size   = 0;
     int green_size = 0;
     int blue_size  = 0;

     struct fb_cmap cmap;

     if (!dfb_fbdev) {
          BUG( "dfb_fbdev_set_gamme_ramp() called while dfb_fbdev == NULL!" );

          return DFB_BUG;
     }

     switch (format) {
          case DSPF_RGB15:
               red_size   = 32;
               green_size = 32;
               blue_size  = 32;
               break;
          case DSPF_RGB16:
               red_size   = 32;
               green_size = 64;
               blue_size  = 32;
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
               red_size   = 256;
               green_size = 256;
               blue_size  = 256;
               break;
          default:
               return DFB_OK;
     }

     cmap.start  = 0;
     /* assume green to have most weight */
     cmap.len    = green_size;
     cmap.red   = (__u16*)alloca( 2 * green_size );
     cmap.green = (__u16*)alloca( 2 * green_size );
     cmap.blue  = (__u16*)alloca( 2 * green_size );
     cmap.transp = NULL;


     for (i = 0; i < red_size; i++)
          cmap.red[i] = dfb_fbdev_calc_gamma( i, red_size );

     for (i = 0; i < green_size; i++)
          cmap.green[i] = dfb_fbdev_calc_gamma( i, green_size );

     for (i = 0; i < blue_size; i++)
          cmap.blue[i] = dfb_fbdev_calc_gamma( i, blue_size );

     if (ioctl( dfb_fbdev->fd, FBIOPUTCMAP, &cmap ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not set gamma ramp" );

          return errno2dfb(errno);
     }

     return DFB_OK;
}

#ifdef SUPPORT_RGB332
static DFBResult dfb_fbdev_set_rgb332_palette()
{
     int red_val;
     int green_val;
     int blue_val;
     int i = 0;

     struct fb_cmap cmap;

     if (!dfb_fbdev) {
          BUG( "dfb_fbdev_set_rgb332_palette() called while dfb_fbdev == NULL!" );

          return DFB_BUG;
     }

     cmap.start  = 0;
     cmap.len    = 256;
     cmap.red    = (__u16*)alloca( 2 * 256 );
     cmap.green  = (__u16*)alloca( 2 * 256 );
     cmap.blue   = (__u16*)alloca( 2 * 256 );
     cmap.transp = (__u16*)alloca( 2 * 256 );


     for (red_val = 0; red_val  < 8 ; red_val++) {
          for (green_val = 0; green_val  < 8 ; green_val++) {
               for (blue_val = 0; blue_val  < 4 ; blue_val++) {
                    cmap.red[i]    = dfb_fbdev_calc_gamma( red_val, 7 );
                    cmap.green[i]  = dfb_fbdev_calc_gamma( green_val, 7 );
                    cmap.blue[i]   = dfb_fbdev_calc_gamma( blue_val, 3 );
                    cmap.transp[i] = (i ? 0x2000 : 0xffff);
                    i++;
               }
          }
     }

     if (ioctl( dfb_fbdev->fd, FBIOPUTCMAP, &cmap ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not set rgb332 palette" );

          return errno2dfb(errno);
     }

     return DFB_OK;
}
#endif
