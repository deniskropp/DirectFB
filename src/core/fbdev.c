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

#include "misc/util.h"


FBDev   *display = NULL;

/* internal functions */
static DFBResult read_modes();


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


/*
 * sets (layer != NULL) or tests (layer == NULL) video mode,
 * sets virtual y-resolution according to buffermode
 */
DFBResult fbdev_set_mode( DisplayLayer *layer,
                          VideoMode *mode,
                          DFBDisplayLayerBufferMode buffermode );

/*
 * deinitializes DirectFB display stuff and restores fbdev settings
 */
void fbdev_deinit()
{
     VideoMode *m;

     if (!display) {
          BUG( "fbdev_deinit() called while display == NULL!" );
          return;
     }

     m = display->modes;
     while (m) {
          VideoMode *next = m->next;
          free( m );
          m = next;
     }

     if (ioctl( display->fd, FBIOPUT_VSCREENINFO, &display->orig_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not restore variable screen information!\n" );
     }

     close( display->fd );

     free( display );
     display = NULL;
}

DFBResult fbdev_open()
{
     if (display) {
          BUG( "fbdev_init() already called!" );
          return DFB_BUG;
     }

     display = (FBDev*) calloc( 1, sizeof(FBDev) );

     display->fd = open( "/dev/fb0", O_RDWR );
     if (display->fd < 0) {
          if (errno == ENOENT) {
               display->fd = open( "/dev/fb/0", O_RDWR );
               if (display->fd < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/core/fbdev: Couldn't open "
                                    "neither `/dev/fb0' nor `/dev/fb/0'!\n" );
                    }
                    else {
                         PERRORMSG( "DirectFB/core/fbdev: "
                                    "Error opening `/dev/fb/0'!\n" );
                    }

                    free( display );
                    display = NULL;

                    return DFB_INIT;
               }
          }
          else {
               PERRORMSG( "DirectFB/core/fbdev: Error opening `/dev/fb0'!\n");

               free( display );
               display = NULL;

               return DFB_INIT;
          }
     }

     if (ioctl( display->fd, FBIOGET_VSCREENINFO, &display->orig_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not get variable screen information!\n" );
          free( display );
          display = NULL;

          return DFB_INIT;
     }

     display->current_var = display->orig_var;
     display->current_var.accel_flags = 0;

     if (ioctl( display->fd, FBIOPUT_VSCREENINFO, &display->current_var ) < 0) {
          PERRORMSG( "DirectFB/core/fbdev: "
                     "Could not disable font acceleration!\n" );
          free( display );
          display = NULL;

          return DFB_INIT;
     }

     read_modes();

     if (!display->modes) {
          /* try to use current mode*/
          display->modes = (VideoMode*) calloc( 1, sizeof(VideoMode) );
     
          display->modes->xres = display->orig_var.xres;
          display->modes->yres = display->orig_var.yres;
          display->modes->bpp = display->orig_var.bits_per_pixel;
          display->modes->hsync_len = display->orig_var.hsync_len;
          display->modes->vsync_len = display->orig_var.vsync_len;
          display->modes->left_margin = display->orig_var.left_margin;
          display->modes->right_margin = display->orig_var.right_margin;
          display->modes->upper_margin = display->orig_var.upper_margin;
          display->modes->lower_margin = display->orig_var.lower_margin;
          display->modes->pixclock = display->orig_var.pixclock;
     
          if (display->orig_var.sync & FB_SYNC_HOR_HIGH_ACT)
               display->modes->hsync_high = 1;
          if (display->orig_var.sync & FB_SYNC_VERT_HIGH_ACT)
               display->modes->vsync_high = 1;
     
          if (display->orig_var.vmode & FB_VMODE_INTERLACED)
               display->modes->laced = 1;
          if (display->orig_var.vmode & FB_VMODE_DOUBLE)
               display->modes->doubled = 1;
     
          if (fbdev_set_mode(NULL, display->modes, DLBM_FRONTONLY))
          {
               ERRORMSG("DirectFB/core/fbdev: "
                        "No valid modes found in /etc/fb.modes, "
                        "current mode not suitable!\n");

               free( display->modes );
               display->modes = NULL;

               return DFB_INIT;
          }
     }

     core_cleanup_push( fbdev_deinit );

     return DFB_OK;
}

DFBSurfacePixelFormat fbdev_get_pixelformat( struct fb_var_screeninfo *var )
{
     switch (var->bits_per_pixel) {
          case 15:
               if (var->red.length    == 5 && var->red.offset    == 10 &&
                   var->green.length  == 5 && var->green.offset  ==  5 &&
                   var->blue.length   == 5 && var->blue.offset   ==  0)
                    return DSPF_RGB15;

               break;

          case 16:
               if (var->red.length    == 5 && var->red.offset    == 10 &&
                   var->green.length  == 5 && var->green.offset  ==  5 &&
                   var->blue.length   == 5 && var->blue.offset   ==  0)
                    return DSPF_RGB15;
               
               if (var->red.length    == 5 && var->red.offset    == 11 &&
                   var->green.length  == 6 && var->green.offset  ==  5 &&
                   var->blue.length   == 5 && var->blue.offset   ==  0)
                    return DSPF_RGB16;

               break;
          
          case 24:
               if (var->red.length    == 8 && var->red.offset    == 16 &&
                   var->green.length  == 8 && var->green.offset  ==  8 &&
                   var->blue.length   == 8 && var->blue.offset   ==  0)
                    return DSPF_RGB24;

               break;

          case 32:
               if (var->transp.length == 0 &&
                   var->red.length    == 8 && var->red.offset    == 16 &&
                   var->green.length  == 8 && var->green.offset  ==  8 &&
                   var->blue.length   == 8 && var->blue.offset   ==  0)
                    return DSPF_RGB32;
               
               if (var->transp.length == 8 && var->transp.offset == 24 &&
                   var->red.length    == 8 && var->red.offset    == 16 &&
                   var->green.length  == 8 && var->green.offset  ==  8 &&
                   var->blue.length   == 8 && var->blue.offset   ==  0)
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
DFBResult fbdev_pan( int buffer )
{
     struct fb_var_screeninfo var;

     var = display->current_var;

     if (var.yres_virtual < var.yres*(buffer+1)) {
          BUG( "panning buffer out of range" );
          return DFB_BUG;
     }

     var.xoffset = 0;
     var.yoffset = var.yres * buffer;

     gfxcard_sync();

     if (ioctl( display->fd, FBIOPAN_DISPLAY, &var ) < 0) {
          int erno = errno;

          PERRORMSG( "DirectFB/core/fbdev: Panning display failed!\n" );

          return errno2dfb( erno );
     }

     display->current_var = var;

     return DFB_OK;
}

DFBResult fbdev_set_mode( DisplayLayer *layer,
                          VideoMode *mode,
                          DFBDisplayLayerBufferMode buffermode )
{
     struct fb_var_screeninfo var;

     if (!mode)
          mode = display->current_mode ? display->current_mode : display->modes;

     var = display->current_var;
          
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

     var.vmode = 0;
     if (mode->laced)
          var.vmode |= FB_VMODE_INTERLACED;
     if (mode->doubled)
          var.vmode |= FB_VMODE_DOUBLE;

     if (ioctl( display->fd, FBIOPUT_VSCREENINFO, &var ) < 0) {
          int erno = errno;

          if (layer)
               PERRORMSG( "DirectFB/core/fbdev: "
                          "Could not set video mode (FBIOPUT_VSCREENINFO)!\n" );

          return errno2dfb( erno );
     }

     if (layer) {
          CoreSurface *surface = layer->surface;

          ioctl( display->fd, FBIOGET_VSCREENINFO, &var );

          surface->format = mode->format = fbdev_get_pixelformat( &var );
          if (mode->format == DSPF_UNKNOWN)
               return DFB_UNSUPPORTED;

          display->current_var = var;
          display->current_mode = mode;

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
                              free( surface->back_buffer->system.addr );

                         free( surface->back_buffer );

                         surface->back_buffer = surface->front_buffer;
                    }
                    break;
               case DLBM_BACKVIDEO:
                    surface->caps |= DSCAPS_FLIPPING;
                    if (surface->back_buffer == surface->front_buffer) {
                         surface->back_buffer = malloc( sizeof(SurfaceBuffer) );
                         memset( surface->back_buffer, 0,
                                 sizeof(SurfaceBuffer) );
                    }
                    else {
                         if (surface->back_buffer->system.health)
                              free( surface->back_buffer->system.addr );

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
                         surface->back_buffer = malloc( sizeof(SurfaceBuffer) );
                         memset( surface->back_buffer, 0,
                                 sizeof(SurfaceBuffer) );
                    }
                    surface->back_buffer->policy = CSP_SYSTEMONLY;
                    surface->back_buffer->video.health = CSH_INVALID;
                    surface->back_buffer->system.health = CSH_STORED;
                    surface->back_buffer->system.pitch = var.xres *
                                                  BYTES_PER_PIXEL(mode->format);
                    surface->back_buffer->system.addr = realloc (
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

DFBResult fbdev_wait_vsync()
{
#ifdef FBIO_WAITFORVSYNC
     if (!dfb_config->pollvsync_none)
          ioctl( display->fd, FBIO_WAITFORVSYNC );
#endif

     return DFB_OK;
}

/************ file internal helper functions *************/

/*
 * parses video modes in /etc/fb.modes and stores them in display->modes
 * (to be replaced by DirectFB's own config system
 */
static DFBResult read_modes()
{
     FILE *fp;
     char line[80],label[32],value[16];
     int geometry=0, timings=0;
     int dummy;
     VideoMode temp_mode;
     VideoMode *m = display->modes;

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
                    if (7 == sscanf(line," timings %d %d %d %d %d %d %d", &temp_mode.pixclock, &temp_mode.left_margin,  &temp_mode.right_margin,
                                    &temp_mode.upper_margin, &temp_mode.lower_margin, &temp_mode.hsync_len,    &temp_mode.vsync_len)) {
                         timings = 1;
                    }
                    if (1 == sscanf(line, " hsync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.hsync_high = 1;
                    }
                    if (1 == sscanf(line, " vsync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.vsync_high = 1;
                    }
                    if (1 == sscanf(line, " laced %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.laced = 1;
                    }
                    if (1 == sscanf(line, " double %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.doubled = 1;
                    }
               }
               if (geometry &&
                   timings &&
                   !fbdev_set_mode(NULL, &temp_mode, DLBM_FRONTONLY))
               {

                    if (!m) {
                         display->modes = malloc (sizeof(VideoMode));
                         m = display->modes;
                    }
                    else {
                         m->next = malloc (sizeof(VideoMode));
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




/** DisplayLayer function hooks **/

static DFBResult primaryEnable( DisplayLayer *thiz )
{
     return DFB_OK;
}

static DFBResult primaryDisable( DisplayLayer *thiz )
{
     /* disable the primary layer? huh!? */
     return DFB_UNSUPPORTED;
}

static DFBResult primaryTestConfiguration( DisplayLayer               *thiz,
                                           DFBDisplayLayerConfig      *config,
                                           DFBDisplayLayerConfigFlags *failed )
{
     VideoMode                  *videomode = NULL;
     DFBDisplayLayerConfigFlags  fail = 0;
     
     if (config->flags & (DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT)) {
          unsigned int  width, height, bpp;

          if (config->flags & DLCONF_WIDTH)
               width = config->width;
          else
               width = thiz->width;
          
          if (config->flags & DLCONF_HEIGHT)
               height = config->height;
          else
               height = thiz->height;
          
          if (config->flags & DLCONF_PIXELFORMAT)
               bpp = BYTES_PER_PIXEL(config->pixelformat) * 8;
          else
               bpp = BYTES_PER_PIXEL(thiz->surface->format) * 8;
          
          videomode = display->modes;
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
     VideoMode                 *videomode = display->modes;
     DFBDisplayLayerBufferMode  buffermode;
     unsigned int               width, height, bpp;
     
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
          bpp = BYTES_PER_PIXEL(config->pixelformat) * 8;
     else
          bpp = BYTES_PER_PIXEL(thiz->surface->format) * 8;

     if (config->flags & DLCONF_BUFFERMODE)
          buffermode = config->buffermode;
     else
          buffermode = thiz->buffermode;
     
     if (display->current_mode->xres == width  &&
         display->current_mode->yres == height &&
         display->current_mode->bpp  == bpp    &&
         thiz->buffermode            == buffermode)
          return DFB_OK;

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
          return DFB_UNSUPPORTED;

     return fbdev_set_mode( thiz, videomode, buffermode );
}

DFBResult primarySetOpacity( DisplayLayer *thiz, __u8 opacity )
{
     /* opacity is not supported for normal primary layer */
     if (opacity != 0xFF)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

DFBResult primarySetScreenLocation( DisplayLayer *thiz, float x, float y,
                                     float w, float h )
{
     /* can only be fullscreen (0, 0, 1, 1) */
     if (x != 0  ||  y != 0  ||  w != 1  ||  h != 1)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

DFBResult primarySetColorKey( DisplayLayer *thiz, __u32 key )
{
     return DFB_UNSUPPORTED;
}

DFBResult primaryFlipBuffers( DisplayLayer *thiz )
{
     if (thiz->buffermode == DLBM_FRONTONLY)
          return DFB_UNSUPPORTED;

     /* FIXME: has to be done "during" pan */
     surface_flip_buffers( thiz->surface );
     
     if (thiz->buffermode == DLBM_BACKVIDEO) {
          DFBResult ret;
           
          ret = fbdev_pan( thiz->surface->front_buffer->video.offset ? 1 : 0 );
          if (ret) {
               surface_flip_buffers( thiz->surface ); /* FIXME */
               return ret;
          }
     }

#if defined(HAVE_INB_OUTB_IOPL)
     if (!dfb_config->pollvsync_none && dfb_config->pollvsync_after) {
          iopl(3);
          waitretrace();
     }
#endif

     return DFB_OK;
}


/**
     internal functions
 **/

static void primarylayer_deinit( DisplayLayer *layer )
{
     windowstack_destroy( layer->windowstack );
     reactor_free( layer->surface->reactor );
     free( layer->surface->front_buffer );
     free( layer->surface );
}

DFBResult primarylayer_init()
{
     CoreSurface *surface;
     DFBResult err;

     DisplayLayer *layer = (DisplayLayer*) calloc( 1, sizeof(DisplayLayer) );

     layer->id = DLID_PRIMARY;
     layer->caps = 0;
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
     surface = (CoreSurface *) calloc ( 1, sizeof(CoreSurface) );

     pthread_mutex_init( &surface->front_lock, NULL );
     pthread_mutex_init( &surface->back_lock, NULL );

     surface->reactor = reactor_new();

     surface->front_buffer = (SurfaceBuffer *) 
          calloc( 1, sizeof(SurfaceBuffer) );

     surface->back_buffer = surface->front_buffer;

     layer->surface = surface;

     /* set the mode to initialize the surface */
     err = fbdev_set_mode( layer, NULL, DLBM_FRONTONLY );
     if (err) {
          ERRORMSG( "DirectFB/core/primarylayer: "
                    "Setting default mode failed!\n" );
          free( layer );
          return err;
     }

     layer->bg.mode = DLBM_DONTCARE;

     layer->windowstack = windowstack_new( layer );

     layers_add( layer );

     return DFB_OK;
}

