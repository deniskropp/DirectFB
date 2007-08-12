/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
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

#include <fusion/types.h>

#include <stdio.h>

#include <directfb.h>
#include <directfb_util.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/memcpy.h>
#include <direct/messages.h>


#include <string.h>
#include <stdlib.h>

#include "xwindow.h"
#include "x11.h"
#include "primary.h"


/**********************************************************************************************************************/

static DFBResult
dfb_x11_set_video_mode( const CoreLayerRegionConfig *config )
{
     int ret;

     D_ASSERT( config != NULL );

     dfb_x11->setmode.config = *config;

     if (fusion_call_execute( &dfb_x11->call, FCEF_NONE, X11_SET_VIDEO_MODE, &dfb_x11->setmode, &ret ))
          return DFB_FUSION;

     return ret;
}

static DFBResult
dfb_x11_update_screen( const DFBRegion *region, CoreSurfaceBufferLock *lock )
{
     int ret;

     DFB_REGION_ASSERT( region );
     D_ASSERT( lock != NULL );

     dfb_x11->update.region = *region;
     dfb_x11->update.lock   = lock;

     if (fusion_call_execute( &dfb_x11->call, FCEF_NONE, X11_UPDATE_SCREEN, &dfb_x11->update, &ret ))
          return DFB_FUSION;

     return ret;
}

static DFBResult
dfb_x11_set_palette( CorePalette *palette )
{
     int ret;

     D_ASSERT( palette != NULL );

     if (fusion_call_execute( &dfb_x11->call, FCEF_NONE, X11_SET_PALETTE, palette, &ret ))
          return DFB_FUSION;

     return ret;
}

/**********************************************************************************************************************/

static DFBResult
primaryInitScreen( CoreScreen           *screen,
                   CoreGraphicsDevice   *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     /* Set the screen capabilities. */
     description->caps = DSCCAPS_NONE;

     /* Set the screen name. */
     snprintf( description->name,
               DFB_SCREEN_DESC_NAME_LENGTH, "X11 Primary Screen" );

     return DFB_OK;
}

static DFBResult
primaryGetScreenSize( CoreScreen *screen,
                      void       *driver_data,
                      void       *screen_data,
                      int        *ret_width,
                      int        *ret_height )
{
     D_ASSERT( dfb_x11 != NULL );

     if (dfb_x11->primary) {
          *ret_width  = dfb_x11->primary->config.size.w;
          *ret_height = dfb_x11->primary->config.size.h;
     }
     else {
          if (dfb_config->mode.width)
               *ret_width  = dfb_config->mode.width;
          else
               *ret_width  = 640;

          if (dfb_config->mode.height)
               *ret_height = dfb_config->mode.height;
          else
               *ret_height = 480;
     }

     return DFB_OK;
}

ScreenFuncs x11PrimaryScreenFuncs = {
     InitScreen:    primaryInitScreen,
     GetScreenSize: primaryGetScreenSize
};

/******************************************************************************/

static int
primaryLayerDataSize()
{
     return 0;
}

static int
primaryRegionDataSize()
{
     return 0;
}

static DFBResult
primaryInitLayer( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  DFBDisplayLayerDescription *description,
                  DFBDisplayLayerConfig      *config,
                  DFBColorAdjustment         *adjustment )
{
     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE;
     description->type = DLTF_GRAPHICS;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "X11 Primary Layer" );

     /* fill out the default configuration */
     config->flags       = DLCONF_WIDTH       | DLCONF_HEIGHT |
                           DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->buffermode  = DLBM_FRONTONLY;

     if (dfb_config->mode.width)
          config->width  = dfb_config->mode.width;
     else
          config->width  = 640;

     if (dfb_config->mode.height)
          config->height = dfb_config->mode.height;
     else
          config->height = 480;

     if (dfb_config->mode.format != DSPF_UNKNOWN)
          config->pixelformat = dfb_config->mode.format;
     else if (dfb_config->mode.depth > 0)
          config->pixelformat = dfb_pixelformat_for_depth( dfb_config->mode.depth );
     else {
          Display *display =XOpenDisplay(NULL);
          int depth=DefaultDepth(display,DefaultScreen(display));
          XCloseDisplay(display);
          switch (depth) {
               case 16:
                    config->pixelformat = DSPF_RGB16;
                    break;
               case 24:
                    /*config->pixelformat = DSPF_RGB24;
                    break;
                    */
               case 32:
                    config->pixelformat = DSPF_RGB32;
                    break;
               default:
                    printf(" Unsupported X11 screen depth %d \n",depth);
                    exit(-1);
                    break;
          }
     }

     return DFB_OK;
}

static DFBResult
primaryTestRegion( CoreLayer                  *layer,
                   void                       *driver_data,
                   void                       *layer_data,
                   CoreLayerRegionConfig      *config,
                   CoreLayerRegionConfigFlags *failed )
{
     CoreLayerRegionConfigFlags fail = 0;

     switch (config->buffermode) {
          case DLBM_FRONTONLY:
          case DLBM_BACKSYSTEM:
          case DLBM_BACKVIDEO:
          case DLBM_TRIPLE:
               break;

          default:
               fail |= CLRCF_BUFFERMODE;
               break;
     }

     if (config->options)
          fail |= CLRCF_OPTIONS;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
primaryAddRegion( CoreLayer             *layer,
                  void                  *driver_data,
                  void                  *layer_data,
                  void                  *region_data,
                  CoreLayerRegionConfig *config )
{
     return DFB_OK;
}

static DFBResult
primarySetRegion( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  void                       *region_data,
                  CoreLayerRegionConfig      *config,
                  CoreLayerRegionConfigFlags  updated,
                  CoreSurface                *surface,
                  CorePalette                *palette,
                  CoreSurfaceBufferLock      *lock )
{
     DFBResult ret;

     ret = dfb_x11_set_video_mode( config );
     if (ret)
          return ret;

     if (surface)
          dfb_x11->primary = surface;

     if (palette)
          dfb_x11_set_palette( palette );

     return DFB_OK;
}

static DFBResult
primaryRemoveRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data )
{
     dfb_x11->primary = NULL;

     return DFB_OK;
}

static DFBResult
primaryFlipRegion( CoreLayer             *layer,
                   void                  *driver_data,
                   void                  *layer_data,
                   void                  *region_data,
                   CoreSurface           *surface,
                   DFBSurfaceFlipFlags    flags,
                   CoreSurfaceBufferLock *lock )
{
     DFBRegion region = DFB_REGION_INIT_FROM_DIMENSION( &surface->config.size );

     dfb_surface_flip( surface, false );

     return dfb_x11_update_screen( &region, lock );
}

static DFBResult
primaryUpdateRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data,
                     CoreSurface           *surface,
                     const DFBRegion       *update,
                     CoreSurfaceBufferLock *lock )
{
     DFBRegion region = DFB_REGION_INIT_FROM_DIMENSION( &surface->config.size );

     return dfb_x11_update_screen( update ? : &region, lock );
}

DisplayLayerFuncs x11PrimaryLayerFuncs = {
     LayerDataSize:     primaryLayerDataSize,
     RegionDataSize:    primaryRegionDataSize,
     InitLayer:         primaryInitLayer,

     TestRegion:        primaryTestRegion,
     AddRegion:         primaryAddRegion,
     SetRegion:         primarySetRegion,
     RemoveRegion:      primaryRemoveRegion,
     FlipRegion:        primaryFlipRegion,
     UpdateRegion:      primaryUpdateRegion,
};

/******************************************************************************/

static DFBResult
update_screen( CoreSurface *surface, int x, int y, int w, int h, CoreSurfaceBufferLock *lock )
{
#if 0
     int          i;
     void        *dst;
     void        *src;
     int          pitch;
     DFBResult    ret;
     XWindow     *xw = dfb_x11->xw;

     D_ASSERT( surface != NULL );
     D_ASSERT( lock != NULL );

     src = lock->addr;
     pitch = lock->pitch;

     xw->ximage_offset = xw->ximage_offset ? 0 : (xw->ximage->height / 2);

     dst = xw->virtualscreen + xw->ximage_offset * xw->ximage->bytes_per_line;

     src += DFB_BYTES_PER_LINE( surface->config.format, x ) + y * pitch;
     dst += x * xw->bpp + y * xw->ximage->bytes_per_line;

     switch (xw->depth) {
          case 16:
               dfb_convert_to_rgb16( surface->config.format, src, pitch,
                                     surface->config.size.h, dst, xw->ximage->bytes_per_line, w, h );
               break;

          case 24:
               if (xw->bpp == 4)
                    dfb_convert_to_rgb32( surface->config.format, src, pitch,
                                          surface->config.size.h, dst, xw->ximage->bytes_per_line, w, h );
               break;

          default:
               D_ONCE( "unsupported depth %d", xw->depth );
     }

     XSync(dfb_x11->display, False);

     XShmPutImage(dfb_x11->display, xw->window, xw->gc, xw->ximage,
                  x, xw->ximage_offset + y, x, y, w, h, False);
#else
     XWindow *xw     = dfb_x11->xw;
     XImage  *ximage = lock->handle;

     XSync( dfb_x11->display, False );

     XShmPutImage( dfb_x11->display, xw->window, xw->gc, ximage,
                   x, y, x, y, w, h, False );

     XFlush( dfb_x11->display );
#endif
     return DFB_OK;
}

/******************************************************************************/

DFBResult
dfb_x11_set_video_mode_handler( CoreLayerRegionConfig *config )
{
     XWindow *xw = dfb_x11->xw;

     if (xw != NULL) {
          if (xw->width == config->width && xw->height == config->height)
               return DFB_OK;

          dfb_x11_close_window( xw );
          dfb_x11->xw = NULL;
     }

     bool bSucces = dfb_x11_open_window(&xw, 0, 0, config->width, config->height);

     /* Set video mode */
     if ( !bSucces ) {
          D_ERROR( "ML: DirectFB/X11: Couldn't open %dx%d window: %s\n",
                   config->width, config->height, "X11 error!");

          fusion_skirmish_dismiss( &dfb_x11->lock );
          return DFB_FAILURE;
     }
     else
          dfb_x11->xw = xw;

     return DFB_OK;
}

DFBResult
dfb_x11_update_screen_handler( UpdateScreenData *data )
{
     return update_screen( dfb_x11->primary,
                           data->region.x1,  data->region.y1,
                           data->region.x2 - data->region.x1 + 1,
                           data->region.y2 - data->region.y1 + 1, data->lock );
}

DFBResult
dfb_x11_set_palette_handler( CorePalette *palette )
{
     return DFB_OK;
}

