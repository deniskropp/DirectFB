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
#include <strings.h>
#if defined(HAVE_SYSIO)
# include <sys/io.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/kd.h>

#include <linux/fb.h>

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
#include <core/sig.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>
#include <core/state.h>
#include <core/windows.h>

#include <gfx/convert.h>

#include <misc/mem.h>
#include <misc/util.h>

#include "vt.h"

#include <core/core_system.h>

DFB_CORE_SYSTEM( fbdev )


typedef struct {
     int   request;
     void *arg;
} FBDevIoctl;

static ReactionResult fbdev_ioctl_listener( const void *msg_data, void *ctx );

static Reaction fbdev_ioctl_reaction;

static int fbdev_ioctl( int request, void *arg, int arg_size );

#define FBDEV_IOCTL(request,arg)   fbdev_ioctl( request, arg, sizeof(*(arg)) )

/* FIXME: should not be exported */
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
primaryWaitVSync         ( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data );

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
     WaitVSync:          primaryWaitVSync,
          
     AllocateSurface:    primaryAllocateSurface,
     ReallocateSurface:  primaryReallocateSurface,
     /* default DeallocateSurface copes with our chunkless video buffers */
};

static const int zero = 0;

static DFBResult dfb_fbdev_read_modes();
static DFBResult dfb_fbdev_set_gamma_ramp( DFBSurfacePixelFormat format );
#ifdef SUPPORT_RGB332
static DFBResult dfb_fbdev_set_rgb332_palette();
#endif
static DFBResult dfb_fbdev_pan( int offset );
static DFBResult dfb_fbdev_set_mode( DisplayLayer          *layer,
                                     VideoMode             *mode,
                                     DFBDisplayLayerConfig *config );

static inline
void waitretrace (void)
{
#if defined(HAVE_INB_OUTB_IOPL)
     static int iopl_done = 0;

     if (!iopl_done) {
          if (iopl(3))
               return;

          iopl_done = 1;
     }

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

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_FBDEV;
     
     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "FBDev" );
}

static DFBResult
system_initialize()
{
     DFBResult ret;

     if (dfb_fbdev) {
          BUG( "dfb_fbdev_init() already called!" );
          return DFB_BUG;
     }

     dfb_fbdev = (FBDev*) DFBCALLOC( 1, sizeof(FBDev) );

     dfb_fbdev->shared = (FBDevShared*) shcalloc( 1, sizeof(FBDevShared) );
     
#ifndef FUSION_FAKE
     arena_add_shared_field( dfb_core->arena, "fbdev", dfb_fbdev->shared );
#endif
     
     ret = dfb_fbdev_open();
     if (ret) {
          shfree( dfb_fbdev->shared );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return ret;
     }

     
     ret = dfb_vt_initialize();
     if (ret) {
          shfree( dfb_fbdev->shared );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return ret;
     }


     /* Retrieve fixed informations like video ram size */
     if (ioctl( dfb_fbdev->fd, FBIOGET_FSCREENINFO, &dfb_fbdev->shared->fix ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not get fixed screen information!\n" );
          shfree( dfb_fbdev->shared );
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }
     
     /* Map the framebuffer */
     dfb_fbdev->framebuffer_base = mmap( NULL, dfb_fbdev->shared->fix.smem_len,
                                         PROT_READ | PROT_WRITE, MAP_SHARED,
                                         dfb_fbdev->fd, 0 );
     if ((int)(dfb_fbdev->framebuffer_base) == -1) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not mmap the framebuffer!\n");
          shfree( dfb_fbdev->shared );
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }
     
     if (ioctl( dfb_fbdev->fd, FBIOGET_VSCREENINFO, &dfb_fbdev->shared->orig_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not get variable screen information!\n" );
          shfree( dfb_fbdev->shared );
          munmap( dfb_fbdev->framebuffer_base, dfb_fbdev->shared->fix.smem_len );
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }

     dfb_fbdev->shared->current_var = dfb_fbdev->shared->orig_var;
     dfb_fbdev->shared->current_var.accel_flags = 0;

     if (ioctl( dfb_fbdev->fd, FBIOPUT_VSCREENINFO, &dfb_fbdev->shared->current_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not disable console acceleration!\n" );
          shfree( dfb_fbdev->shared );
          munmap( dfb_fbdev->framebuffer_base, dfb_fbdev->shared->fix.smem_len );
          close( dfb_fbdev->fd );
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }

     dfb_fbdev->shared->orig_cmap.start  = 0;
     dfb_fbdev->shared->orig_cmap.len    = 256;
     dfb_fbdev->shared->orig_cmap.red    = (__u16*)shmalloc( 2 * 256 );
     dfb_fbdev->shared->orig_cmap.green  = (__u16*)shmalloc( 2 * 256 );
     dfb_fbdev->shared->orig_cmap.blue   = (__u16*)shmalloc( 2 * 256 );
     dfb_fbdev->shared->orig_cmap.transp = (__u16*)shmalloc( 2 * 256 );

     if (ioctl( dfb_fbdev->fd, FBIOGETCMAP, &dfb_fbdev->shared->orig_cmap ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not retrieve palette for backup!\n" );
          shfree( dfb_fbdev->shared->orig_cmap.red );
          shfree( dfb_fbdev->shared->orig_cmap.green );
          shfree( dfb_fbdev->shared->orig_cmap.blue );
          shfree( dfb_fbdev->shared->orig_cmap.transp );
          dfb_fbdev->shared->orig_cmap.len = 0;
     }

     dfb_fbdev->shared->temp_cmap.len    = 256;
     dfb_fbdev->shared->temp_cmap.red    = shcalloc( 256, 2 );
     dfb_fbdev->shared->temp_cmap.green  = shcalloc( 256, 2 );
     dfb_fbdev->shared->temp_cmap.blue   = shcalloc( 256, 2 );
     dfb_fbdev->shared->temp_cmap.transp = shcalloc( 256, 2 );

     dfb_fbdev->shared->current_cmap.len    = 256;
     dfb_fbdev->shared->current_cmap.red    = shcalloc( 256, 2 );
     dfb_fbdev->shared->current_cmap.green  = shcalloc( 256, 2 );
     dfb_fbdev->shared->current_cmap.blue   = shcalloc( 256, 2 );
     dfb_fbdev->shared->current_cmap.transp = shcalloc( 256, 2 );

     skirmish_init( &dfb_fbdev->shared->rpc_lock );
     
     dfb_fbdev->shared->rpc_reactor = reactor_new( sizeof(FBDevIoctl) );

     reactor_attach( dfb_fbdev->shared->rpc_reactor,
                     fbdev_ioctl_listener, NULL, &fbdev_ioctl_reaction );

     /* Register primary layer functions */
     dfb_layers_register( NULL, NULL, &primaryLayerFuncs );

     return DFB_OK;
}

static DFBResult
system_join()
{
#ifndef FUSION_FAKE
     DFBResult ret;

     if (dfb_fbdev) {
          BUG( "dfb_fbdev_join() called and display != NULL" );
          return DFB_BUG;
     }

     ret = dfb_vt_join();
     if (ret)
          return ret;
     
     dfb_fbdev = (FBDev*)DFBCALLOC( 1, sizeof(FBDev) );

     arena_get_shared_field( dfb_core->arena, "fbdev", (void**) &dfb_fbdev->shared );

     /* Open framebuffer device */
     ret = dfb_fbdev_open();
     if (ret) {
          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;
          return ret;
     }

     /* Map the framebuffer */
     dfb_fbdev->framebuffer_base = mmap( NULL, dfb_fbdev->shared->fix.smem_len,
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
     
     /* Register primary layer functions */
     dfb_layers_register( NULL, NULL, &primaryLayerFuncs );
     
#endif
     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     DFBResult  ret;
     VideoMode *m;
     
     if (!dfb_fbdev)
          return DFB_OK;

     m = dfb_fbdev->shared->modes;
     while (m) {
          VideoMode *next = m->next;
          shfree( m );
          m = next;
     }

     if (ioctl( dfb_fbdev->fd, FBIOPUT_VSCREENINFO, &dfb_fbdev->shared->orig_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not restore variable screen information!\n" );
     }

     if (dfb_fbdev->shared->orig_cmap.len) {
          if (ioctl( dfb_fbdev->fd, FBIOPUTCMAP, &dfb_fbdev->shared->orig_cmap ) < 0)
               PERRORMSG( "DirectFB/core/fbdev: "
                          "Could not restore palette!\n" );

          shfree( dfb_fbdev->shared->orig_cmap.red );
          shfree( dfb_fbdev->shared->orig_cmap.green );
          shfree( dfb_fbdev->shared->orig_cmap.blue );
          shfree( dfb_fbdev->shared->orig_cmap.transp );
     }

     shfree( dfb_fbdev->shared->temp_cmap.red );
     shfree( dfb_fbdev->shared->temp_cmap.green );
     shfree( dfb_fbdev->shared->temp_cmap.blue );
     shfree( dfb_fbdev->shared->temp_cmap.transp );

     shfree( dfb_fbdev->shared->current_cmap.red );
     shfree( dfb_fbdev->shared->current_cmap.green );
     shfree( dfb_fbdev->shared->current_cmap.blue );
     shfree( dfb_fbdev->shared->current_cmap.transp );

     reactor_detach( dfb_fbdev->shared->rpc_reactor, &fbdev_ioctl_reaction );

     reactor_free( dfb_fbdev->shared->rpc_reactor );

     skirmish_destroy( &dfb_fbdev->shared->rpc_lock );
     
     munmap( dfb_fbdev->framebuffer_base, dfb_fbdev->shared->fix.smem_len );
     
     ret = dfb_vt_shutdown( emergency );
     if (ret)
          return ret;
     
     close( dfb_fbdev->fd );

     shfree( dfb_fbdev->shared );
     DFBFREE( dfb_fbdev );
     dfb_fbdev = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
#ifndef FUSION_FAKE
     DFBResult ret;

     if (dfb_fbdev) {
          munmap( dfb_fbdev->framebuffer_base,
                  dfb_fbdev->shared->fix.smem_len );
     
          close( dfb_fbdev->fd );

          DFBFREE( dfb_fbdev );
          dfb_fbdev = NULL;
     }

     ret = dfb_vt_leave( emergency );
     if (ret)
          return ret;
     
#endif
     return DFB_OK;
}

static DFBResult
system_suspend()
{
     return DFB_OK;
}

static DFBResult
system_resume()
{
     return DFB_OK;
}

/******************************************************************************/

static volatile void *
system_map_mmio( unsigned int    offset,
                 int             length )
{
     void *addr;

     if (length < 0)
          length = dfb_fbdev->shared->fix.mmio_len;

     addr = mmap( NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                  dfb_fbdev->fd, dfb_fbdev->shared->fix.smem_len + offset );
     if ((int)(addr) == -1) {
          PERRORMSG( "DirectFB/core/gfxcard: Could not mmap MMIO region "
                     "(offset %d, length %d)!\n", offset, length );
          return NULL;
     }

     return (volatile void*) addr;
}

static void
system_unmap_mmio( volatile void  *addr,
                   int             length )
{
     if (length < 0)
          length = dfb_fbdev->shared->fix.mmio_len;

     if (munmap( (void*) addr, length ) < 0)
          PERRORMSG( "DirectFB/core/gfxcard: Could not unmap MMIO region "
                     "at %p (length %d)!\n", addr, length );
}

static int
system_get_accelerator()
{
#ifdef FB_ACCEL_MATROX_MGAG400
     if (!strcmp( dfb_fbdev->shared->fix.id, "MATROX DH" ))
          return FB_ACCEL_MATROX_MGAG400;
#endif
     return dfb_fbdev->shared->fix.accel;
}

static VideoMode *
system_get_modes()
{
     return dfb_fbdev->shared->modes;
}

static VideoMode *
system_get_current_mode()
{
     return dfb_fbdev->shared->current_mode;
}

static DFBResult
system_thread_init()
{
     if (dfb_config->block_all_signals)
          dfb_sig_block_all();
     
     return dfb_vt_detach( false );
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     return dfb_fbdev->shared->fix.smem_start + offset;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     return (void*)((__u8*)(dfb_fbdev->framebuffer_base) + offset);
}

static unsigned int
system_videoram_length()
{
     return dfb_fbdev->shared->fix.smem_len;
}

/******************************************************************************/

static DFBResult init_modes()
{
     dfb_fbdev_read_modes();

     if (!dfb_fbdev->shared->modes) {
          /* try to use current mode*/
          dfb_fbdev->shared->modes = (VideoMode*) shcalloc( 1, sizeof(VideoMode) );

          dfb_fbdev->shared->modes->xres = dfb_fbdev->shared->orig_var.xres;
          dfb_fbdev->shared->modes->yres = dfb_fbdev->shared->orig_var.yres;
          dfb_fbdev->shared->modes->bpp  = dfb_fbdev->shared->orig_var.bits_per_pixel;
          dfb_fbdev->shared->modes->hsync_len = dfb_fbdev->shared->orig_var.hsync_len;
          dfb_fbdev->shared->modes->vsync_len = dfb_fbdev->shared->orig_var.vsync_len;
          dfb_fbdev->shared->modes->left_margin = dfb_fbdev->shared->orig_var.left_margin;
          dfb_fbdev->shared->modes->right_margin = dfb_fbdev->shared->orig_var.right_margin;
          dfb_fbdev->shared->modes->upper_margin = dfb_fbdev->shared->orig_var.upper_margin;
          dfb_fbdev->shared->modes->lower_margin = dfb_fbdev->shared->orig_var.lower_margin;
          dfb_fbdev->shared->modes->pixclock = dfb_fbdev->shared->orig_var.pixclock;


          if (dfb_fbdev->shared->orig_var.sync & FB_SYNC_HOR_HIGH_ACT)
               dfb_fbdev->shared->modes->hsync_high = 1;
          if (dfb_fbdev->shared->orig_var.sync & FB_SYNC_VERT_HIGH_ACT)
               dfb_fbdev->shared->modes->vsync_high = 1;

          if (dfb_fbdev->shared->orig_var.vmode & FB_VMODE_INTERLACED)
               dfb_fbdev->shared->modes->laced = 1;
          if (dfb_fbdev->shared->orig_var.vmode & FB_VMODE_DOUBLE)
               dfb_fbdev->shared->modes->doubled = 1;

          if (dfb_fbdev_set_mode(NULL, dfb_fbdev->shared->modes, NULL)) {
               ERRORMSG("DirectFB/core/fbdev: "
                        "No supported modes found in /etc/fb.modes and "
                        "current mode not supported!\n");

               ERRORMSG( "DirectFB/core/fbdev: Current mode's pixelformat: "
                         "rgba %d/%d, %d/%d, %d/%d, %d/%d (%dbit)\n",
                         dfb_fbdev->shared->orig_var.red.length,
                         dfb_fbdev->shared->orig_var.red.offset,
                         dfb_fbdev->shared->orig_var.green.length,
                         dfb_fbdev->shared->orig_var.green.offset,
                         dfb_fbdev->shared->orig_var.blue.length,
                         dfb_fbdev->shared->orig_var.blue.offset,
                         dfb_fbdev->shared->orig_var.transp.length,
                         dfb_fbdev->shared->orig_var.transp.offset,
                         dfb_fbdev->shared->orig_var.bits_per_pixel );

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
     layer_info->desc.caps = DLCAPS_SURFACE | DLCAPS_CONTRAST | DLCAPS_SATURATION | DLCAPS_BRIGHTNESS;
     layer_info->desc.type = DLTF_GRAPHICS;

     /* set name */
     snprintf( layer_info->desc.name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "FBDev Primary Layer" );

     /* fill out default color adjustment */
     default_adj->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST | DCAF_SATURATION;
     default_adj->brightness = 0x8000;
     default_adj->contrast   = 0x8000;
     default_adj->saturation = 0x8000;

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
     else if (dfb_config->mode.depth > 0) {
          default_config->pixelformat = dfb_pixelformat_for_depth( dfb_config->mode.depth );
          if (default_config->pixelformat == DSPF_UNKNOWN)
               ERRORMSG("DirectFB/fbdev: Unknown depth (%d) specified!\n",
                        dfb_config->mode.depth);
     }
     
     if (default_config->pixelformat == DSPF_UNKNOWN) {
          default_config->pixelformat = DSPF_RGB16;
          
          if (dfb_fbdev_set_mode( NULL, NULL, default_config ))
               default_config->pixelformat = dfb_pixelformat_for_depth( dfb_fbdev->shared->orig_var.bits_per_pixel );
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

          videomode = dfb_fbdev->shared->modes;
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

     videomode = dfb_fbdev->shared->modes;
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
          dfb_layer_wait_vsync( layer );

     ret = dfb_fbdev_pan( surface->back_buffer->video.offset );
     if (ret)
          return ret;

     if ((flags & DSFLIP_WAITFORSYNC) && dfb_config->pollvsync_after)
          dfb_layer_wait_vsync( layer );
          
     dfb_surface_flip_buffers( surface );

     return DFB_OK;
}

static DFBResult
primarySetColorAdjustment( DisplayLayer               *layer,
                           void                       *driver_data,
                           void                       *layer_data,
                           DFBColorAdjustment         *adj )
{
     struct fb_cmap *cmap       = &dfb_fbdev->shared->current_cmap;
     struct fb_cmap *temp       = &dfb_fbdev->shared->temp_cmap;  
     int             contrast   = adj->contrast >> 8;
     int             brightness = (adj->brightness >> 8) - 128;
     int             saturation = adj->saturation >> 8;
     int             r, g, b, i;

     if (dfb_fbdev->shared->fix.visual != FB_VISUAL_DIRECTCOLOR)
          return DFB_UNIMPLEMENTED;

     /* Use gamma ramp to set color attributes */
     for (i = 0; i < cmap->len; i++) {
          r = cmap->red[i];
	  g = cmap->green[i];
	  b = cmap->blue[i];
	  r >>= 8;
	  g >>= 8;
	  b >>= 8;

         /* 
	  * Brightness Adjustment: Increase/Decrease each color channels
	  * by a constant amount as specified by value of brightness.
	  */
	  if (adj->flags & DCAF_BRIGHTNESS) {
	      r += brightness;
	      g += brightness; 
	      b += brightness;
	  
	      r = (r < 0) ? 0 : r;      
	      g = (g < 0) ? 0 : g;      
	      b = (b < 0) ? 0 : b;      

	      r = (r > 255) ? 255 : r;  
	      g = (g > 255) ? 255 : g;  
	      b = (b > 255) ? 255 : b; 
	  }

	  /*
	   * Contrast Adjustment:  We increase/decrease the "separation"
	   * between colors in proportion to the value specified by the
	   * contrast control. Decreasing the contrast has a side effect
	   * of decreasing the brightness.
	   */
	  
	  if (adj->flags & DCAF_CONTRAST) {
	      /* Increase contrast */
	      if (contrast > 128) {
		  int c = contrast - 128;

		  r = ((r + c/2)/c) * c;
		  g = ((g + c/2)/c) * c;
		  b = ((b + c/2)/c) * c;
	      }
	      /* Decrease contrast */
	      else if (contrast < 127) {
		  float c = (float)contrast/128.0;

		  r = (int)((float)r * c);
		  g = (int)((float)g * c);
		  b = (int)((float)b * c);
	      }
	      r = (r < 0) ? 0 : r;      
	      g = (g < 0) ? 0 : g;      
	      b = (b < 0) ? 0 : b;      

	      r = (r > 255) ? 255 : r;  
	      g = (g > 255) ? 255 : g;  
	      b = (b > 255) ? 255 : b; 
	  }

	  /*
	   * Saturation Adjustment:  This is is a better implementation.
	   * Saturation is implemented by "mixing" a proportion of medium
	   * gray to the color value.  On the other side, "removing"
	   * a proportion of medium gray oversaturates the color.  
	   */
	  if (adj->flags & DCAF_SATURATION) {
	      if (saturation > 128) {
	          float gray = ((float)saturation - 128.0)/128.0; 
		  float color = 1.0 - gray;
	      
		  r = (int)(((float)r - 128.0 * gray)/color);
		  g = (int)(((float)g - 128.0 * gray)/color);
		  b = (int)(((float)b - 128.0 * gray)/color);
	      }
	      else if (saturation < 128) {
		  float color = (float)saturation/128.0; 
		  float gray = 1.0 - color;

		  r = (int)(((float) r * color) + (128.0 * gray));
		  g = (int)(((float) g * color) + (128.0 * gray));
		  b = (int)(((float) b * color) + (128.0 * gray));
	      }
	  
	      r = (r < 0) ? 0 : r;      
	      g = (g < 0) ? 0 : g;      
	      b = (b < 0) ? 0 : b;      

	      r = (r > 255) ? 255 : r;  
	      g = (g > 255) ? 255 : g;  
	      b = (b > 255) ? 255 : b; 
	  }
	  r |= r << 8;
	  g |= g << 8;
	  b |= b << 8;

          temp->red[i]   =  (unsigned short)r;
          temp->green[i] =  (unsigned short)g;
          temp->blue[i]  =  (unsigned short)b;
      }

     temp->len = cmap->len;
     temp->start = cmap->start;
     if (FBDEV_IOCTL( FBIOPUTCMAP, temp ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not set the palette!\n" );

          return errno2dfb(errno);
     }

     return DFB_OK;
}

static DFBResult
primarySetPalette ( DisplayLayer               *layer,
                    void                       *driver_data,
                    void                       *layer_data,
                    CorePalette                *palette )
{
     int            i;
     struct fb_cmap *cmap = &dfb_fbdev->shared->current_cmap;

     DFB_ASSERT( layer != NULL );
     DFB_ASSERT( palette != NULL );

     for (i = 0; i < cmap->len; i++) {
          cmap->red[i]     = palette->entries[i].r;
          cmap->green[i]   = palette->entries[i].g;
          cmap->blue[i]    = palette->entries[i].b;
          cmap->transp[i]  = 0xff - palette->entries[i].a;

          cmap->red[i]    |= cmap->red[i] << 8;
          cmap->green[i]  |= cmap->green[i] << 8;
          cmap->blue[i]   |= cmap->blue[i] << 8;
          cmap->transp[i] |= cmap->transp[i] << 8;
     }

     if (FBDEV_IOCTL( FBIOPUTCMAP, cmap ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not set the palette!\n" );

          return errno2dfb(errno);
     }
     
     return DFB_OK;
}

static DFBResult
primaryWaitVSync( DisplayLayer *layer,
                  void         *driver_data,
                  void         *layer_data )
{
     if (dfb_config->pollvsync_none)
          return DFB_OK;

#ifdef FBIO_WAITFORVSYNC
     dfb_gfxcard_sync();
     if (ioctl( dfb_fbdev->fd, FBIO_WAITFORVSYNC, &zero ))
#endif
          waitretrace();

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
     if (config->buffermode == DLBM_TRIPLE)
          caps |= DSCAPS_TRIPLE;
     else if (config->buffermode != DLBM_FRONTONLY)
          caps |= DSCAPS_FLIPPING;

     /* allocate surface object */
     surface = (CoreSurface*) fusion_object_create( dfb_gfxcard_surface_pool() );
     if (!surface)
          return DFB_FAILURE;

     /* reallocation just needs an allocated buffer structure */
     surface->idle_buffer = surface->back_buffer =
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
          DFBResult    ret;
          CorePalette *palette;
           
          ret = dfb_palette_create( 1 << DFB_BITS_PER_PIXEL( config->pixelformat ),
                                    &palette );
          if (ret)
               return ret;

          if (config->pixelformat == DSPF_LUT8)
               dfb_palette_generate_rgb332_map( palette );
          
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
                    return DSPF_ARGB1555;

               break;

          case 16:
               if (dfb_fbdev_compatible_format( var, 0, 5, 5, 5, 0, 10, 5, 0 ) |
                   dfb_fbdev_compatible_format( var, 1, 5, 5, 5,15, 10, 5, 0 ) )
                    return DSPF_ARGB1555;

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
static DFBResult dfb_fbdev_pan( int offset )
{
     struct fb_var_screeninfo var;

     var = dfb_fbdev->shared->current_var;

     if (var.yres_virtual < offset + var.yres) {
          BUG( "panning buffer out of range" );
          return DFB_BUG;
     }

     var.xoffset = 0;
     var.yoffset = offset;

     dfb_gfxcard_sync();

     if (FBDEV_IOCTL( FBIOPAN_DISPLAY, &var ) < 0) {
          int erno = errno;

          PERRORMSG( "DirectFB/core/fbdev: Panning display failed!\n" );

          return errno2dfb( erno );
     }

     dfb_fbdev->shared->current_var = var;

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
          mode = dfb_fbdev->shared->current_mode ? dfb_fbdev->shared->current_mode : dfb_fbdev->shared->modes;

     vyres = mode->yres;
     
     var = dfb_fbdev->shared->current_var;

     var.xoffset = 0;
     var.yoffset = 0;

     if (config) {
          if (config->buffermode == DLBM_TRIPLE)
               vyres *= 3;
          else if (config->buffermode == DLBM_BACKVIDEO)
               vyres *= 2;

          var.bits_per_pixel = DFB_BYTES_PER_PIXEL(config->pixelformat) * 8;

          var.transp.length = var.transp.offset = 0;
          
          switch (config->pixelformat) {
               case DSPF_ARGB1555:
                    var.transp.length = 1;
                    var.red.length    = 5;
                    var.green.length  = 5;
                    var.blue.length   = 5;
                    var.transp.offset = 15;
                    var.red.offset    = 10;
                    var.green.offset  = 5;
                    var.blue.offset   = 0;
                    break;
     
               case DSPF_RGB16:
                    var.red.length    = 5;
                    var.green.length  = 6;
                    var.blue.length   = 5;
                    var.red.offset    = 11;
                    var.green.offset  = 5;
                    var.blue.offset   = 0;
                    break;
     
               case DSPF_ARGB:
                    var.transp.length = 8;
                    var.red.length    = 8;
                    var.green.length  = 8;
                    var.blue.length   = 8;
                    var.transp.offset = 24;
                    var.red.offset    = 16;
                    var.green.offset  = 8;
                    var.blue.offset   = 0;
                    break;

               case DSPF_LUT8:
               case DSPF_RGB24:
               case DSPF_RGB32:
               case DSPF_RGB332:
                    break;
     
               default:
                    return DFB_UNSUPPORTED;
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

     dfb_gfxcard_lock();
     
     if (FBDEV_IOCTL( FBIOPUT_VSCREENINFO, &var ) < 0) {
          int erno = errno;

          if (layer)
               PERRORMSG( "DirectFB/core/fbdev: "
                          "Could not set video mode (FBIOPUT_VSCREENINFO)!\n" );

          dfb_gfxcard_unlock( true );
          
          return errno2dfb( erno );
     }
     else {
         /* 
          * the video mode was set successfully, check if there is enough
          * video ram (for buggy framebuffer drivers)
          */
          
          if  (dfb_fbdev->shared->fix.smem_len < (var.yres_virtual * 
                                                  var.xres_virtual * 
                                                  var.bits_per_pixel >> 3))
          {
               if (layer)
                    PERRORMSG( "DirectFB/core/fbdev: "
                      "Could not set video mode (not enough video ram)!\n" );

               dfb_gfxcard_unlock( true );
               
               return DFB_INVARG;
          }
     }

     /* If layer is NULL the mode was only tested, otherwise apply changes. */
     if (layer) {
          struct fb_fix_screeninfo  fix;
          DFBSurfacePixelFormat     format;
          CoreSurface              *surface = dfb_layer_surface( layer );

          FBDEV_IOCTL( FBIOGET_VSCREENINFO, &var );


          format = dfb_fbdev_get_pixelformat( &var );
          if (format == DSPF_UNKNOWN || var.yres_virtual < vyres) {
               /* restore mode */
               FBDEV_IOCTL( FBIOPUT_VSCREENINFO, &dfb_fbdev->shared->current_var );
               
               dfb_gfxcard_unlock( true );
               
               return DFB_UNSUPPORTED;
          }

          if (!config) {
               dfb_gfxcard_unlock( true );
               
               return DFB_OK;
          }
          
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

          dfb_fbdev->shared->current_var = var;
          dfb_fbdev->shared->current_mode = mode;
          
          surface->width  = mode->xres;
          surface->height = mode->yres;
          surface->format = format;
          
          /* To get the new pitch */
          FBDEV_IOCTL( FBIOGET_FSCREENINFO, &fix );
          
	  /* ++Tony: Other information (such as visual formats) will also change */
          dfb_fbdev->shared->fix = fix;

          dfb_gfxcard_adjust_heap_offset( var.yres_virtual * fix.line_length );

          surface->front_buffer->surface = surface;
          surface->front_buffer->policy = CSP_VIDEOONLY;
          surface->front_buffer->video.health = CSH_STORED;
          surface->front_buffer->video.pitch = fix.line_length;
          surface->front_buffer->video.offset = 0;

          switch (config->buffermode) {
               case DLBM_FRONTONLY:
                    surface->caps &= ~(DSCAPS_FLIPPING | DSCAPS_TRIPLE);

                    if (surface->back_buffer != surface->front_buffer) {
                         if (surface->back_buffer->system.addr)
                              shfree( surface->back_buffer->system.addr );

                         shfree( surface->back_buffer );

                         surface->back_buffer = surface->front_buffer;
                    }

                    if (surface->idle_buffer != surface->front_buffer) {
                         if (surface->idle_buffer->system.addr)
                              shfree( surface->idle_buffer->system.addr );

                         shfree( surface->idle_buffer );

                         surface->idle_buffer = surface->front_buffer;
                    }
                    break;
               case DLBM_BACKVIDEO:
                    surface->caps |= DSCAPS_FLIPPING;
                    surface->caps &= ~DSCAPS_TRIPLE;

                    if (surface->back_buffer == surface->front_buffer) {
                         surface->back_buffer = shcalloc( 1, sizeof(SurfaceBuffer) );
                    }
                    else {
                         if (surface->back_buffer->system.addr) {
                              shfree( surface->back_buffer->system.addr );
                              surface->back_buffer->system.addr = NULL;
                         }

                         surface->back_buffer->system.health = CSH_INVALID;
                    }
                    surface->back_buffer->surface = surface;
                    surface->back_buffer->policy = CSP_VIDEOONLY;
                    surface->back_buffer->video.health = CSH_STORED;
                    surface->back_buffer->video.pitch = fix.line_length;
                    surface->back_buffer->video.offset =
                                   surface->back_buffer->video.pitch * var.yres;

                    if (surface->idle_buffer != surface->front_buffer) {
                         if (surface->idle_buffer->system.addr)
                              shfree( surface->idle_buffer->system.addr );

                         shfree( surface->idle_buffer );

                         surface->idle_buffer = surface->front_buffer;
                    }
                    break;
               case DLBM_TRIPLE:
                    surface->caps |= DSCAPS_FLIPPING | DSCAPS_TRIPLE;

                    if (surface->back_buffer == surface->front_buffer) {
                         surface->back_buffer = shcalloc( 1, sizeof(SurfaceBuffer) );
                    }
                    else {
                         if (surface->back_buffer->system.addr) {
                              shfree( surface->back_buffer->system.addr );
                              surface->back_buffer->system.addr = NULL;
                         }

                         surface->back_buffer->system.health = CSH_INVALID;
                    }
                    surface->back_buffer->surface = surface;
                    surface->back_buffer->policy = CSP_VIDEOONLY;
                    surface->back_buffer->video.health = CSH_STORED;
                    surface->back_buffer->video.pitch = fix.line_length;
                    surface->back_buffer->video.offset =
                                   surface->back_buffer->video.pitch * var.yres;

                    if (surface->idle_buffer == surface->front_buffer) {
                         surface->idle_buffer = shcalloc( 1, sizeof(SurfaceBuffer) );
                    }
                    else {
                         if (surface->idle_buffer->system.addr) {
                              shfree( surface->idle_buffer->system.addr );
                              surface->idle_buffer->system.addr = NULL;
                         }

                         surface->idle_buffer->system.health = CSH_INVALID;
                    }
                    surface->idle_buffer->surface = surface;
                    surface->idle_buffer->policy = CSP_VIDEOONLY;
                    surface->idle_buffer->video.health = CSH_STORED;
                    surface->idle_buffer->video.pitch = fix.line_length;
                    surface->idle_buffer->video.offset =
                                   surface->idle_buffer->video.pitch * var.yres * 2;
                    break;
               case DLBM_BACKSYSTEM:
                    surface->caps |= DSCAPS_FLIPPING;
                    surface->caps &= ~DSCAPS_TRIPLE;

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

                    if (surface->idle_buffer != surface->front_buffer) {
                         if (surface->idle_buffer->system.addr)
                              shfree( surface->idle_buffer->system.addr );

                         shfree( surface->idle_buffer );

                         surface->idle_buffer = surface->front_buffer;
                    }
                    break;
          }

          dfb_fbdev_pan(0);

          dfb_gfxcard_after_set_var();

          dfb_surface_notify_listeners( surface, CSNF_SIZEFORMAT | CSNF_FLIP |
                                                 CSNF_VIDEO | CSNF_SYSTEM );
     }

     dfb_gfxcard_unlock( true );
     
     return DFB_OK;
}

/*
 * parses video modes in /etc/fb.modes and stores them in dfb_fbdev->shared->modes
 * (to be replaced by DirectFB's own config system
 */
static DFBResult dfb_fbdev_read_modes()
{
     FILE *fp;
     char line[80],label[32],value[16];
     int geometry=0, timings=0;
     int dummy;
     VideoMode temp_mode;
     VideoMode *m = dfb_fbdev->shared->modes;

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
                         dfb_fbdev->shared->modes = shcalloc(1, sizeof(VideoMode));
                         m = dfb_fbdev->shared->modes;
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
     int red_max    = 0;
     int green_max  = 0;
     int blue_max   = 0;

     struct fb_cmap *cmap;

     if (!dfb_fbdev) {
          BUG( "dfb_fbdev_set_gamma_ramp() called while dfb_fbdev == NULL!" );

          return DFB_BUG;
     }

     switch (format) {
          case DSPF_ARGB1555:
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

     /*
      * ++Tony: The gamma ramp must be set differently if in DirectColor,
      *         ie, to mimic TrueColor, index == color[index].
      */
     if (dfb_fbdev->shared->fix.visual == FB_VISUAL_DIRECTCOLOR) {
          red_max   = 65536 / (256/red_size);
          green_max = 65536 / (256/green_size);
	  blue_max  = 65536 / (256/blue_size);
     }
     else {
         red_max   = red_size;
         green_max = green_size;
         blue_max  = blue_size;
     }

     /* assume green to have most weight */
     dfb_fbdev->shared->temp_cmap.len = green_size;

     cmap = &dfb_fbdev->shared->current_cmap;

     for (i = 0; i < red_size; i++)
          cmap->red[i] = dfb_fbdev_calc_gamma( i, red_max );

     for (i = 0; i < green_size; i++)
          cmap->green[i] = dfb_fbdev_calc_gamma( i, green_max );

     for (i = 0; i < blue_size; i++)
          cmap->blue[i] = dfb_fbdev_calc_gamma( i, blue_max );

     /* ++Tony: Some drivers use the upper byte, some use the lower */
     if (dfb_fbdev->shared->fix.visual == FB_VISUAL_DIRECTCOLOR) {
         for (i = 0; i < red_size; i++)
	     cmap->red[i] |= cmap->red[i] << 8;

         for (i = 0; i < green_size; i++)
             cmap->green[i] |= cmap->green[i] << 8;

         for (i = 0; i < blue_size; i++)
             cmap->blue[i] |= cmap->blue[i] << 8;
     }

     if (FBDEV_IOCTL( FBIOPUTCMAP, cmap ) < 0) {
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
     cmap.red    = (__u16*)shmalloc( 2 * 256 );
     cmap.green  = (__u16*)shmalloc( 2 * 256 );
     cmap.blue   = (__u16*)shmalloc( 2 * 256 );
     cmap.transp = (__u16*)shmalloc( 2 * 256 );


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

     if (FBDEV_IOCTL( FBIOPUTCMAP, &cmap ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not set rgb332 palette" );

          shfree( cmap.red );
          shfree( cmap.green );
          shfree( cmap.blue );
          shfree( cmap.transp );

          return errno2dfb(errno);
     }

     shfree( cmap.red );
     shfree( cmap.green );
     shfree( cmap.blue );
     shfree( cmap.transp );

     return DFB_OK;
}
#endif

static ReactionResult
fbdev_ioctl_listener( const void *msg_data, void *ctx )
{
     const FBDevIoctl *message = (const FBDevIoctl*) msg_data;

     if (!dfb_config->kd_graphics && message->request == FBIOPUT_VSCREENINFO)
          ioctl( dfb_vt->fd, KDSETMODE, KD_GRAPHICS );
     
     if (ioctl( dfb_fbdev->fd, message->request, message->arg ))
          dfb_fbdev->shared->rpc_ret = errno;
     else
          dfb_fbdev->shared->rpc_ret = 0;

     if (!dfb_config->kd_graphics && message->request == FBIOPUT_VSCREENINFO)
          ioctl( dfb_vt->fd, KDSETMODE, KD_TEXT );
     
     return RS_OK;
}

static int
fbdev_ioctl( int request, void *arg, int arg_size )
{
     int         timeout = 100000;
     void       *tmp_shm = NULL;
     int         erno;
     FBDevIoctl  message;

     DFB_ASSERT( dfb_fbdev != NULL );
     DFB_ASSERT( dfb_fbdev->shared != NULL );

     if (dfb_core_is_master())
          return ioctl( dfb_fbdev->fd, request, arg );
     

     message.request = request;
     
     if (arg) {
          if (fusion_is_shared( arg ))
               message.arg = arg;
          else {
               message.arg = tmp_shm = shmalloc( arg_size );
               dfb_memcpy( message.arg, arg, arg_size );
          }
     }
     else
          message.arg = NULL;

     
     skirmish_prevail( &dfb_fbdev->shared->rpc_lock );

     dfb_fbdev->shared->rpc_ret = -1;
     
     reactor_dispatch( dfb_fbdev->shared->rpc_reactor, &message, false, NULL );

     while (dfb_fbdev->shared->rpc_ret == -1) {
          if (! --timeout) {
               ERRORMSG( "DirectFB/core/fbdev: Timeout "
                         "while waiting for completion of rpc ioctl!\n" );

               dfb_fbdev->shared->rpc_ret = ETIMEDOUT;

               break;
          }
          
          sched_yield();
     }

     erno = dfb_fbdev->shared->rpc_ret;
     
     skirmish_dismiss( &dfb_fbdev->shared->rpc_lock );

     
     if (tmp_shm) {
          dfb_memcpy( arg, tmp_shm, arg_size );
          shfree( tmp_shm );
     }

     errno = erno;

     return errno ? -1 : 0;
}

