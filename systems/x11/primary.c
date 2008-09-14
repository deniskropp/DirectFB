/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

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

#include <GL/glx.h>

#include <string.h>
#include <stdlib.h>

#include "xwindow.h"
#include "x11.h"
#include "primary.h"
#include "glx_surface_pool.h"


D_DEBUG_DOMAIN( X11_Window, "X11/Window", "X11 Window" );
D_DEBUG_DOMAIN( X11_Update, "X11/Update", "X11 Update" );

/**********************************************************************************************************************/

static DFBResult
dfb_x11_create_window( DFBX11 *x11, const CoreLayerRegionConfig *config )
{
     int           ret;
     DFBX11Shared *shared = x11->shared;

     D_ASSERT( config != NULL );

     shared->setmode.config = *config;

     if (fusion_call_execute( &shared->call, FCEF_NONE, X11_CREATE_WINDOW, &shared->setmode, &ret ))
          return DFB_FUSION;

     return ret;
}

static DFBResult
dfb_x11_destroy_window( DFBX11 *x11 )
{
     int           ret;
     DFBX11Shared *shared = x11->shared;

     if (fusion_call_execute( &shared->call, FCEF_NONE, X11_DESTROY_WINDOW, NULL, &ret ))
          return DFB_FUSION;

     return ret;
}

static DFBResult
dfb_x11_update_screen( DFBX11 *x11, const DFBRegion *region, CoreSurfaceBufferLock *lock )
{
     int           ret;
     DFBX11Shared *shared = x11->shared;

     DFB_REGION_ASSERT( region );
     D_ASSERT( lock != NULL );

     shared->update.region = *region;
     shared->update.lock   = lock;

     if (fusion_call_execute( &shared->call, FCEF_NONE, X11_UPDATE_SCREEN, &shared->update, &ret ))
          return DFB_FUSION;

     return ret;
}

static DFBResult
dfb_x11_set_palette( DFBX11 *x11, CorePalette *palette )
{
     int           ret;
     DFBX11Shared *shared = x11->shared;

     D_ASSERT( palette != NULL );

     if (fusion_call_execute( &shared->call, FCEF_NONE, X11_SET_PALETTE, palette, &ret ))
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
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

     *ret_width  = shared->screen_size.w;
     *ret_height = shared->screen_size.h;

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
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

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
          config->width  = shared->screen_size.w;

     if (dfb_config->mode.height)
          config->height = dfb_config->mode.height;
     else
          config->height = shared->screen_size.h;

     if (dfb_config->mode.format != DSPF_UNKNOWN)
          config->pixelformat = dfb_config->mode.format;
     else if (dfb_config->mode.depth > 0)
          config->pixelformat = dfb_pixelformat_for_depth( dfb_config->mode.depth );
     else {
          int depth = DefaultDepthOfScreen( x11->screenptr );

          switch (depth) {
               case 15:
                    config->pixelformat = DSPF_RGB555;
                    break;
               case 16:
                    config->pixelformat = DSPF_RGB16;
                    break;
               case 24:
                    config->pixelformat = DSPF_RGB32;
                    break;
               case 32:
                    config->pixelformat = DSPF_ARGB;
                    break;
               default:
                    printf(" Unsupported X11 screen depth %d \n",depth);
                    return DFB_UNSUPPORTED;
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

     switch (config->format) {
          case DSPF_RGB16:
          case DSPF_NV16:
          case DSPF_RGB444:
          case DSPF_ARGB4444:
          case DSPF_RGB555:
          case DSPF_ARGB1555:
          case DSPF_BGR555:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_AYUV:
               break;

          default:
               fail |= CLRCF_FORMAT;
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
     DFBResult  ret;
     DFBX11    *x11 = driver_data;

     ret = dfb_x11_create_window( x11, config );
     if (ret)
          return ret;

     if (palette)
          dfb_x11_set_palette( x11, palette );

     return DFB_OK;
}

static DFBResult
primaryRemoveRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data )
{
     DFBX11 *x11 = driver_data;

     dfb_x11_destroy_window( x11 );

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
     DFBX11    *x11    = driver_data;
     DFBRegion  region = DFB_REGION_INIT_FROM_DIMENSION( &surface->config.size );

     dfb_surface_flip( surface, false );

     return dfb_x11_update_screen( x11, &region, lock );
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
     DFBX11    *x11    = driver_data;
     DFBRegion  region = DFB_REGION_INIT_FROM_DIMENSION( &surface->config.size );

     if (update && !dfb_region_region_intersect( &region, update ))
          return DFB_OK;

     return dfb_x11_update_screen( x11, &region, lock );
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
update_screen( DFBX11 *x11, const DFBRectangle *clip, CoreSurfaceBufferLock *lock )
{
     void                  *dst;
     void                  *src;
     unsigned int           offset = 0;
     XWindow               *xw;
     XImage                *ximage;
     CoreSurface           *surface;
     CoreSurfaceAllocation *allocation;
     DFBX11Shared          *shared;
     DFBRectangle           rect;

     D_ASSERT( x11 != NULL );
     DFB_RECTANGLE_ASSERT( clip );

     D_DEBUG_AT( X11_Update, "%s( %4d,%4d-%4dx%4d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( clip ) );

     CORE_SURFACE_BUFFER_LOCK_ASSERT( lock );

     shared = x11->shared;
     D_ASSERT( shared != NULL );

     xw = shared->xw;
     D_ASSERT( xw != NULL );

     allocation = lock->allocation;
     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     surface = allocation->surface;
     D_ASSERT( surface != NULL );


     XLockDisplay( x11->display );

     rect.x = rect.y = 0;
     rect.w = xw->width;
     rect.h = xw->height;

     if (!dfb_rectangle_intersect( &rect, clip )) {
          XUnlockDisplay( x11->display );
          return DFB_OK;
     }

     D_DEBUG_AT( X11_Update, "  -> %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( &rect ) );

     /* Check for GLX allocation... */
     if (allocation->pool == shared->glx_pool && lock->handle) {
          glxAllocationData *alloc = lock->handle;

          D_MAGIC_ASSERT( alloc, glxAllocationData );

          /* ...and just call SwapBuffers... */
          //D_DEBUG_AT( X11_Update, "  -> Calling glXSwapBuffers( 0x%lx )...\n", alloc->drawable );
          //glXSwapBuffers( x11->display, alloc->drawable );


          D_DEBUG_AT( X11_Update, "  -> Copying from GLXPixmap...\n" );

          glXWaitGL();

          XCopyArea( x11->display, alloc->pixmap, xw->window, xw->gc,
                     rect.x, rect.y, rect.w, rect.h, rect.x, rect.y );

          glXWaitX();

          XUnlockDisplay( x11->display );

          return DFB_OK;
     }

     /* Check for our special native allocation... */
     if (allocation->pool == shared->x11image_pool && lock->handle) {
          x11Image *image = lock->handle;

          D_MAGIC_ASSERT( image, x11Image );

          /* ...and directly XShmPutImage from that. */
          ximage = image->ximage;
     }
     else {
          /* ...or copy or convert into XShmImage or XImage allocated with the XWindow. */
          ximage = xw->ximage;
          offset = xw->ximage_offset;

          xw->ximage_offset = (offset ? 0 : ximage->height / 2);
          
          dst = xw->virtualscreen + rect.x * xw->bpp + (rect.y + offset) * ximage->bytes_per_line;
          src = lock->addr + DFB_BYTES_PER_LINE( surface->config.format, rect.x ) + rect.y * lock->pitch;

          switch (xw->depth) {
               case 32:
                    dfb_convert_to_argb( surface->config.format, src, lock->pitch,
                                         surface->config.size.h, dst, ximage->bytes_per_line, rect.w, rect.h );
                    break;

               case 24:
                    dfb_convert_to_rgb32( surface->config.format, src, lock->pitch,
                                          surface->config.size.h, dst, ximage->bytes_per_line, rect.w, rect.h );
                    break;

               case 16:
                    dfb_convert_to_rgb16( surface->config.format, src, lock->pitch,
                                          surface->config.size.h, dst, ximage->bytes_per_line, rect.w, rect.h );
                    break;

               case 15:
                    dfb_convert_to_rgb555( surface->config.format, src, lock->pitch,
                                           surface->config.size.h, dst, ximage->bytes_per_line, rect.w, rect.h );
                    break;

               default:
                    D_ONCE( "unsupported depth %d", xw->depth );
          }
     }

     D_ASSERT( ximage != NULL );


     /* Wait for previous data to be processed... */
     XSync( x11->display, False );

     /* ...and immediately queue or send the next! */
     if (x11->use_shm) {
          /* Just queue the command, it's XShm :) */
          XShmPutImage( xw->display, xw->window, xw->gc, ximage,
                        rect.x, rect.y + offset, rect.x, rect.y, rect.w, rect.h, False );

          /* Make sure the queue has really happened! */
          XFlush( x11->display );
     }
     else
          /* Initiate transfer of buffer... */
          XPutImage( xw->display, xw->window, xw->gc, ximage,
                     rect.x, rect.y + offset, rect.x, rect.y, rect.w, rect.h );

     XUnlockDisplay( x11->display );

     return DFB_OK;
}

/******************************************************************************/

DFBResult
dfb_x11_create_window_handler( DFBX11 *x11, CoreLayerRegionConfig *config )
{
     XWindow      *xw;
     DFBX11Shared *shared = x11->shared;

     D_DEBUG_AT( X11_Window, "%s( %p )\n", __FUNCTION__, config );

     D_DEBUG_AT( X11_Window, "  -> %4dx%4d %s\n", config->width, config->height, dfb_pixelformat_name(config->format) );

     XLockDisplay( x11->display );

     xw = shared->xw;
     if (xw != NULL) {
          if (xw->width == config->width && xw->height == config->height) {
               XUnlockDisplay( x11->display );
               return DFB_OK;
          }

          dfb_x11_close_window( x11, xw );
          shared->xw = NULL;
     }

     bool bSucces = dfb_x11_open_window( x11, &xw, 0, 0, config->width, config->height, config->format );

     /* Set video mode */
     if ( !bSucces ) {
          D_ERROR( "ML: DirectFB/X11: Couldn't open %dx%d window: %s\n",
                   config->width, config->height, "X11 error!");

          XUnlockDisplay( x11->display );
          return DFB_FAILURE;
     }
     else
          shared->xw = xw;

     XUnlockDisplay( x11->display );
     return DFB_OK;
}

DFBResult
dfb_x11_destroy_window_handler( DFBX11 *x11 )
{
     DFBX11Shared *shared = x11->shared;

     D_DEBUG_AT( X11_Window, "%s()\n", __FUNCTION__ );

     XLockDisplay( x11->display );

     if (shared->xw) {
          dfb_x11_close_window( x11, shared->xw );
          shared->xw = NULL;
     }

     XSync( x11->display, False );

     XUnlockDisplay( x11->display );

     return DFB_OK;
}

DFBResult
dfb_x11_update_screen_handler( DFBX11 *x11, UpdateScreenData *data )
{
     DFBRectangle rect;

     D_DEBUG_AT( X11_Update, "%s( %p )\n", __FUNCTION__, data );

     rect = DFB_RECTANGLE_INIT_FROM_REGION( &data->region );

     return update_screen( x11, &rect, data->lock );
}

DFBResult
dfb_x11_set_palette_handler( DFBX11 *x11, CorePalette *palette )
{
     return DFB_OK;
}

