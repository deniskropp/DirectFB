/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <pthread.h>

#include "directfb.h"

#include "coredefs.h"
#include "coretypes.h"

#include "fbdev.h"
#include "core.h"
#include "layers.h"
#include "gfxcard.h"
#include "reactor.h"
#include "surfaces.h"
#include "surfacemanager.h"
#include "state.h"
#include "windows.h"

#include "misc/mem.h"
#include "misc/util.h"


FBDev *fbdev = NULL;

static DFBResult primaryEnable( DisplayLayer *thiz );
static DFBResult primaryDisable( DisplayLayer *thiz );
static DFBResult primaryTestConfiguration( DisplayLayer               *thiz,
                                           DFBDisplayLayerConfig      *config,
                                           DFBDisplayLayerConfigFlags *failed );
static DFBResult primarySetConfiguration( DisplayLayer          *thiz,
                                          DFBDisplayLayerConfig *config );
static DFBResult primarySetOpacity( DisplayLayer *thiz, __u8 opacity );
static DFBResult primarySetScreenLocation( DisplayLayer *thiz, float x, float y,
                                     float w, float h );
static DFBResult primarySetColorKey( DisplayLayer *thiz, __u32 key );
static DFBResult primaryFlipBuffers( DisplayLayer *thiz );
static void primarylayer_deinit( DisplayLayer *layer );

static DFBResult fbdev_read_modes();
static DFBResult fbdev_set_gamma_ramp( DFBSurfacePixelFormat format );
#ifdef SUPPORT_RGB332
static DFBResult fbdev_set_rgb332_palette();
#endif
static DFBResult fbdev_pan( int buffer );
static DFBResult fbdev_set_mode( DisplayLayer              *layer,
                                 VideoMode                 *mode,
                                 DFBDisplayLayerBufferMode  buffermode );

#if defined(HAVE_INB_OUTB_IOPL)
static inline
void waitretrace (void)
{
    while ((inb (0x3da) & 0x8))
        ;

    while (!(inb (0x3da) & 0x8))
        ;
}
#endif


/** public **/

DFBResult fbdev_open()
{
     if (fbdev) {
          BUG( "fbdev_init() already called!" );
          return DFB_BUG;
     }

     fbdev = (FBDev*) DFBCALLOC( 1, sizeof(FBDev) );

     if (dfb_config->fb_device) {
          fbdev->fd = open( dfb_config->fb_device, O_RDWR );
          if (fbdev->fd < 0) {
               PERRORMSG( "DirectFB/core/fbdev: Error opening `%s'!\n",
                          dfb_config->fb_device);

               DFBFREE( fbdev );
               fbdev = NULL;

               return DFB_INIT;
          }
     }

     fbdev->fd = open( "/dev/fb0", O_RDWR );
     if (fbdev->fd < 0) {
          if (errno == ENOENT) {
               fbdev->fd = open( "/dev/fb/0", O_RDWR );
               if (fbdev->fd < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/core/fbdev: Couldn't open "
                                    "neither `/dev/fb0' nor `/dev/fb/0'!\n" );
                    }
                    else {
                         PERRORMSG( "DirectFB/core/fbdev: "
                                    "Error opening `/dev/fb/0'!\n" );
                    }

                    DFBFREE( fbdev );
                    fbdev = NULL;

                    return DFB_INIT;
               }
          }
          else {
               PERRORMSG( "DirectFB/core/fbdev: Error opening `/dev/fb0'!\n");

               DFBFREE( fbdev );
               fbdev = NULL;

               return DFB_INIT;
          }
     }

     if (ioctl( fbdev->fd, FBIOGET_VSCREENINFO, &fbdev->orig_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not get variable screen information!\n" );
          DFBFREE( fbdev );
          fbdev = NULL;

          return DFB_INIT;
     }

     fbdev->current_var = fbdev->orig_var;
     fbdev->current_var.accel_flags = 0;

     if (ioctl( fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->current_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not disable console acceleration!\n" );
          DFBFREE( fbdev );
          fbdev = NULL;

          return DFB_INIT;
     }

     fbdev_read_modes();

     if (!fbdev->modes) {
          /* try to use current mode*/
          fbdev->modes = (VideoMode*) DFBCALLOC( 1, sizeof(VideoMode) );

          fbdev->modes->xres = fbdev->orig_var.xres;
          fbdev->modes->yres = fbdev->orig_var.yres;
          fbdev->modes->bpp = fbdev->orig_var.bits_per_pixel;
          fbdev->modes->hsync_len = fbdev->orig_var.hsync_len;
          fbdev->modes->vsync_len = fbdev->orig_var.vsync_len;
          fbdev->modes->left_margin = fbdev->orig_var.left_margin;
          fbdev->modes->right_margin = fbdev->orig_var.right_margin;
          fbdev->modes->upper_margin = fbdev->orig_var.upper_margin;
          fbdev->modes->lower_margin = fbdev->orig_var.lower_margin;
          fbdev->modes->pixclock = fbdev->orig_var.pixclock;


          if (fbdev->orig_var.sync & FB_SYNC_HOR_HIGH_ACT)
               fbdev->modes->hsync_high = 1;
          if (fbdev->orig_var.sync & FB_SYNC_VERT_HIGH_ACT)
               fbdev->modes->vsync_high = 1;

          if (fbdev->orig_var.vmode & FB_VMODE_INTERLACED)
               fbdev->modes->laced = 1;
          if (fbdev->orig_var.vmode & FB_VMODE_DOUBLE)
               fbdev->modes->doubled = 1;

          if (fbdev_set_mode(NULL, fbdev->modes, DLBM_FRONTONLY))
          {
               ERRORMSG("DirectFB/core/fbdev: "
                        "No supported modes found in /etc/fb.modes and "
                        "current mode not supported!\n");

               ERRORMSG( "DirectFB/core/fbdev: Current mode's pixelformat: "
                         "rgba %d/%d, %d/%d, %d/%d, %d/%d (%dbit)\n",
                         fbdev->orig_var.red.length,
                         fbdev->orig_var.red.offset,
                         fbdev->orig_var.green.length,
                         fbdev->orig_var.green.offset,
                         fbdev->orig_var.blue.length,
                         fbdev->orig_var.blue.offset,
                         fbdev->orig_var.transp.length,
                         fbdev->orig_var.transp.offset,
                         fbdev->orig_var.bits_per_pixel );

               DFBFREE( fbdev->modes );
               fbdev->modes = NULL;

               return DFB_INIT;
          }
     }

     fbdev->orig_cmap.start  = 0;
     fbdev->orig_cmap.len    = 256;
     fbdev->orig_cmap.red    = (__u16*)DFBMALLOC( 2 * 256 );
     fbdev->orig_cmap.green  = (__u16*)DFBMALLOC( 2 * 256 );
     fbdev->orig_cmap.blue   = (__u16*)DFBMALLOC( 2 * 256 );
     fbdev->orig_cmap.transp = NULL;

     if (ioctl( fbdev->fd, FBIOGETCMAP, &fbdev->orig_cmap ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not retrieve palette for backup!\n" );
          DFBFREE( fbdev->orig_cmap.red );
          DFBFREE( fbdev->orig_cmap.green );
          DFBFREE( fbdev->orig_cmap.blue );
          fbdev->orig_cmap.len = 0;
     }


     return DFB_OK;
}

void fbdev_deinit()
{
     VideoMode *m;

     if (!fbdev) {
          BUG( "fbdev_deinit() called while fbdev == NULL!" );
          return;
     }

     m = fbdev->modes;
     while (m) {
          VideoMode *next = m->next;
          DFBFREE( m );
          m = next;
     }

     if (ioctl( fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->orig_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not restore variable screen information!\n" );
     }

     if (fbdev->orig_cmap.len) {
          if (ioctl( fbdev->fd, FBIOPUTCMAP, &fbdev->orig_cmap ) < 0)
               PERRORMSG( "DirectFB/core/fbdev: "
                          "Could not restore palette!\n" );

          DFBFREE( fbdev->orig_cmap.red );
          DFBFREE( fbdev->orig_cmap.green );
          DFBFREE( fbdev->orig_cmap.blue );
     }

     close( fbdev->fd );

     DFBFREE( fbdev );
     fbdev = NULL;
}

DFBResult fbdev_wait_vsync()
{
#ifdef FBIO_WAITFORVSYNC
     if (!dfb_config->pollvsync_none) {
          gfxcard_sync();
          ioctl( fbdev->fd, FBIO_WAITFORVSYNC );
     }
#endif

     return DFB_OK;
}

DFBResult primarylayer_init()
{
     CoreSurface *surface;
     DFBResult err;

     DisplayLayer *layer = (DisplayLayer*) DFBCALLOC( 1, sizeof(DisplayLayer) );

     layer->id = DLID_PRIMARY;
     layer->caps = DLCAPS_SURFACE;
     sprintf( layer->description, "Primary Layer" );

     layer->deinit = primarylayer_deinit;

     layer->screen.x = 0.0f;
     layer->screen.y = 0.0f;
     layer->screen.w = 1.0f;
     layer->screen.h = 1.0f;

     layer->enabled = 1;
     layer->opacity = 0xFF;

     layer->Enable = primaryEnable;
     layer->Disable = primaryDisable;
     layer->TestConfiguration = primaryTestConfiguration;
     layer->SetConfiguration = primarySetConfiguration;
     layer->SetScreenLocation = primarySetScreenLocation;
     layer->SetOpacity = primarySetOpacity;
     layer->SetColorKey = primarySetColorKey;
     layer->FlipBuffers = primaryFlipBuffers;

     /* allocate the surface */
     surface = (CoreSurface *) DFBCALLOC( 1, sizeof(CoreSurface) );

     pthread_mutex_init( &surface->front_lock, NULL );
     pthread_mutex_init( &surface->back_lock, NULL );

     surface->reactor = reactor_new();

     surface->front_buffer = (SurfaceBuffer *)
          DFBCALLOC( 1, sizeof(SurfaceBuffer) );

     surface->back_buffer = surface->front_buffer;

     layer->surface = surface;

     /* set the mode to initialize the surface */
     err = fbdev_set_mode( layer, NULL, DLBM_FRONTONLY );
     if (err) {
          ERRORMSG( "DirectFB/core/primarylayer: "
                    "Setting default mode failed!\n" );
          DFBFREE( layer );
          return err;
     }

     layer->bg.mode = DLBM_DONTCARE;

     layer->windowstack = windowstack_new( layer );

     layers_add( layer );

     return DFB_OK;
}


/** primary layer internal **/

static DFBResult primaryEnable( DisplayLayer *thiz )
{
     return DFB_OK;
}

static DFBResult primaryDisable( DisplayLayer *thiz )
{
     return DFB_UNSUPPORTED;
}

static DFBResult primaryTestConfiguration( DisplayLayer               *thiz,
                                           DFBDisplayLayerConfig      *config,
                                           DFBDisplayLayerConfigFlags *failed )
{
     VideoMode                  *videomode = NULL;
     DFBDisplayLayerConfigFlags  fail = 0;

     if (config->flags & (DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT)) {
          DFBSurfacePixelFormat pixelformat;
          unsigned int          width, height, bpp;

          if (config->flags & DLCONF_WIDTH)
               width = config->width;
          else
               width = thiz->width;

          if (config->flags & DLCONF_HEIGHT)
               height = config->height;
          else
               height = thiz->height;

          if (config->flags & DLCONF_PIXELFORMAT)
               pixelformat = config->pixelformat;
          else
               pixelformat = thiz->surface->format;

          switch (pixelformat) {
               case DSPF_RGB15:  /* special case where VideoMode->bpp = 15 */
                    bpp = 15;
                    break;

               default:
                    bpp = BYTES_PER_PIXEL(pixelformat) * 8;
          }

          videomode = fbdev->modes;
          while (videomode) {
               if (videomode->xres == width  &&
                   videomode->yres == height  &&
                   videomode->bpp == bpp &&
                   videomode->laced == 0 &&
                   videomode->doubled == 0)
                    break;

               videomode = videomode->next;
          }

          if (!videomode)
               fail |= (config->flags & (DLCONF_WIDTH  |
                                         DLCONF_HEIGHT | DLCONF_PIXELFORMAT));
     }
     if (config->flags & DLCONF_BUFFERMODE) {
          if (fbdev_set_mode( NULL, videomode, config->buffermode ))
               fail |= DLCONF_BUFFERMODE;
     }
     else if (videomode) {
          if (fbdev_set_mode( NULL, videomode, thiz->buffermode ))
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

static DFBResult primarySetConfiguration( DisplayLayer          *thiz,
                                          DFBDisplayLayerConfig *config )
{
     VideoMode                 *videomode;
     DFBDisplayLayerBufferMode  buffermode;
     DFBSurfacePixelFormat      pixelformat;
     unsigned int               width, height, bpp;

     if (!config)
          return fbdev_set_mode( thiz, fbdev->current_mode, thiz->buffermode );

     if (config->flags & DLCONF_OPTIONS  &&  config->options)
          return DFB_UNSUPPORTED;

     if (config->flags & DLCONF_WIDTH)
          width = config->width;
     else
          width = thiz->width;

     if (config->flags & DLCONF_HEIGHT)
          height = config->height;
     else
          height = thiz->height;

     if (config->flags & DLCONF_PIXELFORMAT)
          pixelformat = config->pixelformat;
     else
          pixelformat = thiz->surface->format;

     switch (pixelformat) {
          case DSPF_RGB15:  /* special case where VideoMode->bpp = 15 */
               bpp = 15;
               break;

          default:
               bpp = BYTES_PER_PIXEL(pixelformat) * 8;
     }

     if (config->flags & DLCONF_BUFFERMODE)
          buffermode = config->buffermode;
     else
          buffermode = thiz->buffermode;

     if (fbdev->current_mode->xres == width  &&
         fbdev->current_mode->yres == height &&
         fbdev->current_mode->bpp  == bpp    &&
         thiz->buffermode            == buffermode)
          return DFB_OK;

     videomode = fbdev->modes;
     while (videomode) {
          if (videomode->xres == width  &&
              videomode->yres == height  &&
              videomode->bpp == bpp)
               break;

          videomode = videomode->next;
     }

     if (!videomode)
          return DFB_UNSUPPORTED;

     return fbdev_set_mode( thiz, videomode, buffermode );
}

static DFBResult primarySetOpacity( DisplayLayer *thiz, __u8 opacity )
{
     /* opacity is not supported for normal primary layer */
     if (opacity != 0xFF)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult primarySetScreenLocation( DisplayLayer *thiz, float x, float y,
                                     float w, float h )
{
     /* can only be fullscreen (0, 0, 1, 1) */
     if (x != 0  ||  y != 0  ||  w != 1  ||  h != 1)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult primarySetColorKey( DisplayLayer *thiz, __u32 key )
{
     return DFB_UNSUPPORTED;
}

static DFBResult primaryFlipBuffers( DisplayLayer *thiz )
{
     if (thiz->buffermode == DLBM_FRONTONLY)
          return DFB_UNSUPPORTED;

     if (thiz->buffermode == DLBM_BACKVIDEO) {
          DFBResult ret;

          ret = fbdev_pan( thiz->surface->back_buffer->video.offset ? 1 : 0 );
          if (ret)
               return ret;
     }

     surface_flip_buffers( thiz->surface );

#if defined(HAVE_INB_OUTB_IOPL)
     if (!dfb_config->pollvsync_none && dfb_config->pollvsync_after) {
          iopl(3);
          waitretrace();
     }
#endif

     return DFB_OK;
}

static void primarylayer_deinit( DisplayLayer *layer )
{
     windowstack_destroy( layer->windowstack );

     reactor_free( layer->surface->reactor );

     DFBFREE( layer->surface->front_buffer );

     if (layer->surface->back_buffer != layer->surface->front_buffer)
          DFBFREE( layer->surface->back_buffer );

     DFBFREE( layer->surface );
}


/** fbdev internal **/

static int fbdev_compatible_format( struct fb_var_screeninfo *var,
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

static DFBSurfacePixelFormat fbdev_get_pixelformat( struct fb_var_screeninfo *var )
{
     switch (var->bits_per_pixel) {

#ifdef SUPPORT_RGB332
          case 8:
/*
               This check is omitted, since we want to use RGB332 even if the
               hardware uses a palette (in that case we initzalize a calculated
               one to have correct colors)

               if (fbdev_compatible_format( var, 0, 3, 3, 2, 0, 5, 2, 0 ))
                    return DSPF_RGB332;
*/
               return DSPF_RGB332;
               break;
#endif
          case 15:
               if (fbdev_compatible_format( var, 0, 5, 5, 5, 0, 10, 5, 0 ) |
                   fbdev_compatible_format( var, 1, 5, 5, 5,15, 10, 5, 0 ) )
                    return DSPF_RGB15;

               break;

          case 16:               
               if (fbdev_compatible_format( var, 0, 5, 5, 5, 0, 10, 5, 0 ) |
                   fbdev_compatible_format( var, 1, 5, 5, 5,15, 10, 5, 0 ) )
                    return DSPF_RGB15;

               if (fbdev_compatible_format( var, 0, 5, 6, 5, 0, 11, 5, 0 ))
                    return DSPF_RGB16;

               break;
                              
          case 24:
               if (fbdev_compatible_format( var, 0, 8, 8, 8, 0, 16, 8, 0 ))
                    return DSPF_RGB24;

               break;

          case 32:
               if (fbdev_compatible_format( var, 0, 8, 8, 8, 0, 16, 8, 0 ))
                    return DSPF_RGB32;

               if (fbdev_compatible_format( var, 8, 8, 8, 8, 24, 16, 8, 0 ))
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
static DFBResult fbdev_pan( int buffer )
{
     struct fb_var_screeninfo var;

     var = fbdev->current_var;

     if (var.yres_virtual < var.yres*(buffer+1)) {
          BUG( "panning buffer out of range" );
          return DFB_BUG;
     }

     var.xoffset = 0;
     var.yoffset = var.yres * buffer;

     gfxcard_sync();

     if (ioctl( fbdev->fd, FBIOPAN_DISPLAY, &var ) < 0) {
          int erno = errno;

          PERRORMSG( "DirectFB/core/fbdev: Panning display failed!\n" );

          return errno2dfb( erno );
     }

     fbdev->current_var = var;

     return DFB_OK;
}

/*
 * sets (if layer != NULL) or tests (if layer == NULL) video mode,
 * sets virtual y-resolution according to buffermode
 */
static DFBResult fbdev_set_mode( DisplayLayer *layer,
                                 VideoMode *mode,
                                 DFBDisplayLayerBufferMode buffermode )
{
     struct fb_var_screeninfo var;

     if (!mode)
          mode = fbdev->current_mode ? fbdev->current_mode : fbdev->modes;

     var = fbdev->current_var;

     var.xoffset = 0;
     var.yoffset = 0;
     var.bits_per_pixel = mode->bpp;

     /*
      * since parsing of the argb parameter in fbset is broken, DirectFB
      * sets RGB555 mode, when 15bpp is is given in an /etc/fb.modes entry
      */
     switch (mode->bpp) {
          case 15:
               var.bits_per_pixel = 16;
               var.red.length   = 5;
               var.green.length = 5;
               var.blue.length  = 5;
               var.red.offset   = 10;
               var.green.offset = 5;
               var.blue.offset  = 0;
               break;
          case 16:
               var.red.length   = 5;
               var.green.length = 6;
               var.blue.length  = 5;
               var.red.offset   = 11;
               var.green.offset = 5;
               var.blue.offset  = 0;
               break;
#ifdef SUPPORT_RGB332
          case 8:
#endif
          case 24:
          case 32:
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     var.activate = layer ? FB_ACTIVATE_NOW : FB_ACTIVATE_TEST;

     var.xres = mode->xres;
     var.yres = mode->yres;
     var.xres_virtual = mode->xres;
     var.yres_virtual = mode->yres * ((buffermode == DLBM_BACKVIDEO) ? 2 : 1);

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

     if (ioctl( fbdev->fd, FBIOPUT_VSCREENINFO, &var ) < 0) {
          int erno = errno;

          if (layer)
               PERRORMSG( "DirectFB/core/fbdev: "
                          "Could not set video mode (FBIOPUT_VSCREENINFO)!\n" );

          return errno2dfb( erno );
     }

     /* If layer is NULL the mode was only tested, otherwise apply changes. */
     if (layer) {
          CoreSurface *surface = layer->surface;

          ioctl( fbdev->fd, FBIOGET_VSCREENINFO, &var );


          mode->format = fbdev_get_pixelformat( &var );
          if (mode->format == DSPF_UNKNOWN) {
               /* restore mode */
               ioctl( fbdev->fd, FBIOPUT_VSCREENINFO, &fbdev->current_var );
               return DFB_UNSUPPORTED;
          }

#ifdef SUPPORT_RGB332
          if (mode->format == DSPF_RGB332)
               fbdev_set_rgb332_palette();
          else
#endif
          fbdev_set_gamma_ramp( mode->format );

          /* if mode->bpp contains 16 bit we won't find the mode again! */
          if (mode->format == DSPF_RGB15)
               mode->bpp = 15;

          surface->format = mode->format;

          fbdev->current_var = var;
          fbdev->current_mode = mode;

          layer->width = surface->width = mode->xres;
          layer->height = surface->height = mode->yres;
          layer->buffermode = buffermode;

          surfacemanager_adjust_heap_offset( var.yres_virtual *
                                             var.xres_virtual *
                                             ((var.bits_per_pixel + 7) / 8) );

          surface->front_buffer->policy = CSP_VIDEOONLY;
          surface->front_buffer->video.health = CSH_STORED;
          surface->front_buffer->video.pitch = var.xres_virtual *
                                               BYTES_PER_PIXEL(mode->format);
          surface->front_buffer->video.offset = 0;

          switch (buffermode) {
               case DLBM_FRONTONLY:
                    surface->caps &= ~DSCAPS_FLIPPING;
                    if (surface->back_buffer != surface->front_buffer) {
                         if (surface->back_buffer->system.health)
                              DFBFREE( surface->back_buffer->system.addr );

                         DFBFREE( surface->back_buffer );

                         surface->back_buffer = surface->front_buffer;
                    }
                    break;
               case DLBM_BACKVIDEO:
                    surface->caps |= DSCAPS_FLIPPING;
                    if (surface->back_buffer == surface->front_buffer) {
                         surface->back_buffer = DFBCALLOC( 1, sizeof(SurfaceBuffer) );
                    }
                    else {
                         if (surface->back_buffer->system.health)
                              DFBFREE( surface->back_buffer->system.addr );

                         surface->back_buffer->system.health = CSH_INVALID;
                    }
                    surface->back_buffer->policy = CSP_VIDEOONLY;
                    surface->back_buffer->video.health = CSH_STORED;
                    surface->back_buffer->video.pitch = var.xres_virtual *
                                                  BYTES_PER_PIXEL(mode->format);
                    surface->back_buffer->video.offset =
                                   surface->back_buffer->video.pitch * var.yres;
                    break;
               case DLBM_BACKSYSTEM:
                    surface->caps |= DSCAPS_FLIPPING;
                    if (surface->back_buffer == surface->front_buffer) {
                         surface->back_buffer = DFBCALLOC( 1, sizeof(SurfaceBuffer) );
                    }
                    surface->back_buffer->policy = CSP_SYSTEMONLY;
                    surface->back_buffer->video.health = CSH_INVALID;
                    surface->back_buffer->system.health = CSH_STORED;
                    surface->back_buffer->system.pitch = var.xres *
                                                  BYTES_PER_PIXEL(mode->format);
                    surface->back_buffer->system.addr = DFBREALLOC(
                         surface->back_buffer->system.addr,
                         surface->back_buffer->system.pitch * var.yres );
                    break;
          }

          fbdev_pan(0);

          if (card->AfterSetVar)
               card->AfterSetVar();

          surface_notify_listeners( surface, CSNF_SIZEFORMAT | CSNF_FLIP |
                                             CSNF_VIDEO | CSNF_SYSTEM );
     }

     return DFB_OK;
}

/*
 * parses video modes in /etc/fb.modes and stores them in fbdev->modes
 * (to be replaced by DirectFB's own config system
 */
static DFBResult fbdev_read_modes()
{
     FILE *fp;
     char line[80],label[32],value[16];
     int geometry=0, timings=0;
     int dummy;
     VideoMode temp_mode;
     VideoMode *m = fbdev->modes;

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
                   !fbdev_set_mode(NULL, &temp_mode, DLBM_FRONTONLY))
               {

                    if (!m) {
                         fbdev->modes = DFBMALLOC(sizeof(VideoMode));
                         m = fbdev->modes;
                    }
                    else {
                         m->next = DFBMALLOC(sizeof(VideoMode));
                         m = m->next;
                    }
                    memcpy (m, &temp_mode, sizeof(VideoMode));
                    DEBUGMSG( "DirectFB/core/fbdev: %20s %4dx%4d  %d bit  %s%s\n", label, temp_mode.xres, temp_mode.yres,
                              temp_mode.bpp, temp_mode.laced ? "interlaced " : "", temp_mode.doubled ? "doublescan" : "" );
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

static __u16 fbdev_calc_gamma(int n, int max)
{
    int ret = 65535.0 * ((float)((float)n/(max)));
    if (ret > 65535) ret = 65535;
    if (ret <     0) ret =     0;
    return ret;
}


static DFBResult fbdev_set_gamma_ramp( DFBSurfacePixelFormat format )
{
     int i;

     int red_size   = 0;
     int green_size = 0;
     int blue_size  = 0;

     struct fb_cmap cmap;

     if (!fbdev) {
          BUG( "fbdev_set_gamme_ramp() called while fbdev == NULL!" );

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
               return DSPF_UNKNOWN;
               break;
     }

     cmap.start  = 0;
     /* assume green to have most weight */
     cmap.len    = green_size;
     cmap.red   = (__u16*)alloca( 2 * green_size );
     cmap.green = (__u16*)alloca( 2 * green_size );
     cmap.blue  = (__u16*)alloca( 2 * green_size );
     cmap.transp = NULL;


     for (i = 0; i < red_size; i++)
          cmap.red[i] = fbdev_calc_gamma( i, red_size );

     for (i = 0; i < green_size; i++)
          cmap.green[i] = fbdev_calc_gamma( i, green_size );

     for (i = 0; i < blue_size; i++)
          cmap.blue[i] = fbdev_calc_gamma( i, blue_size );

     if (ioctl( fbdev->fd, FBIOPUTCMAP, &cmap ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not set gamma ramp" );

          return errno2dfb(errno);
     }

     return DFB_OK;
}

#ifdef SUPPORT_RGB332
static DFBResult fbdev_set_rgb332_palette()
{
     int red_val;
     int green_val;
     int blue_val;
     int i = 0;

     struct fb_cmap cmap;

     if (!fbdev) {
          BUG( "fbdev_set_rgb332_palette() called while fbdev == NULL!" );

          return DFB_BUG;
     }

     cmap.start  = 0;
     cmap.len    = 256;
     cmap.red   = (__u16*)alloca( 2 * 256 );
     cmap.green = (__u16*)alloca( 2 * 256 );
     cmap.blue  = (__u16*)alloca( 2 * 256 );
     cmap.transp = NULL;


     for (red_val = 0; red_val  < 8 ; red_val++) {
          for (green_val = 0; green_val  < 8 ; green_val++) {
               for (blue_val = 0; blue_val  < 4 ; blue_val++) {
                    cmap.red[i]   = red_val   << 13;
                    cmap.green[i] = green_val << 13;
                    cmap.blue[i]  = blue_val  << 14;
                    i++;
               }
          }
     }

     if (ioctl( fbdev->fd, FBIOPUTCMAP, &cmap ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not set rgb332 palette" );

          return errno2dfb(errno);
     }

     return DFB_OK;
}
#endif
