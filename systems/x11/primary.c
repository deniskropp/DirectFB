/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include <string.h>
#include <stdlib.h>

#ifdef USE_GLX
#include <GL/glx.h>
#include "glx_surface_pool.h"
#endif

#include "xwindow.h"
#include "x11.h"
#include "primary.h"

D_DEBUG_DOMAIN( X11_Layer,  "X11/Layer",  "X11 Layer" );
D_DEBUG_DOMAIN( X11_Update, "X11/Update", "X11 Update" );

/**********************************************************************************************************************/

static DFBResult
dfb_x11_create_window( DFBX11 *x11, X11LayerData *lds, const CoreLayerRegionConfig *config )
{
     int           ret;
     DFBX11Shared *shared = x11->shared;

     D_ASSERT( config != NULL );

     shared->setmode.config = *config;
     shared->setmode.xw     = &(lds->xw);

     if (fusion_call_execute( &shared->call, FCEF_NONE, X11_CREATE_WINDOW, &shared->setmode, &ret ))
          return DFB_FUSION;

     return ret;
}

static DFBResult
dfb_x11_destroy_window( DFBX11 *x11, X11LayerData *lds )
{
     int           ret;
     DFBX11Shared *shared = x11->shared;
     
     shared->destroy.xw = &(lds->xw);

     if (fusion_call_execute( &shared->call, FCEF_NONE, X11_DESTROY_WINDOW, &shared->destroy, &ret ))
          return DFB_FUSION;

     return ret;
}

static DFBResult
dfb_x11_update_screen( DFBX11 *x11, X11LayerData *lds, const DFBRegion *left_region, const DFBRegion *right_region,
                       CoreSurfaceBufferLock *left_lock, CoreSurfaceBufferLock *right_lock )
{
     int           ret;
     DFBX11Shared *shared = x11->shared;

     DFB_REGION_ASSERT( left_region );
     D_ASSERT( left_lock != NULL );

     /* FIXME: Just a hot fix! */
     if (shared->update.left_lock.buffer)
          return DFB_OK;

     shared->update.xw           = lds->xw;
     shared->update.left_region  = *left_region;
     shared->update.left_lock    = *left_lock;

     shared->update.stereo       = (lds->config.options & DLOP_STEREO);

     if (shared->update.stereo) {
          DFB_REGION_ASSERT( right_region );
          D_ASSERT( right_lock != NULL );

          shared->update.right_region = *right_region;
          shared->update.right_lock   = *right_lock;
     }

     if (fusion_call_execute( &shared->call, FCEF_NONE, X11_UPDATE_SCREEN, &shared->update, &ret ))
          return DFB_FUSION;

     return ret;
}

static DFBResult
dfb_x11_set_palette( DFBX11 *x11, X11LayerData *lds, CorePalette *palette )
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
     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     /* Set the screen capabilities. */
     description->caps     = DSCCAPS_ENCODERS | DSCCAPS_OUTPUTS;
     description->encoders = 1;
     description->outputs  = 1;

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

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     *ret_width  = shared->screen_size.w;
     *ret_height = shared->screen_size.h;

     return DFB_OK;
}

static DFBResult
primaryInitEncoder( CoreScreen                  *screen,
                    void                        *driver_data,
                    void                        *screen_data,
                    int                          encoder,
                    DFBScreenEncoderDescription *description,
                    DFBScreenEncoderConfig      *config )
{
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

     (void) shared;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     direct_snputs( description->name, "X11 Encoder", DFB_SCREEN_ENCODER_DESC_NAME_LENGTH );

     description->caps            = DSECAPS_TV_STANDARDS | DSECAPS_SCANMODE   | DSECAPS_FREQUENCY |
                                    DSECAPS_CONNECTORS   | DSECAPS_RESOLUTION | DSECAPS_FRAMING;
     description->type            = DSET_DIGITAL;
     description->tv_standards    = DSETV_DIGITAL;
     description->all_connectors  = DSOC_COMPONENT | DSOC_HDMI;
     description->all_resolutions = DSOR_640_480  | DSOR_720_480   | DSOR_720_576   | DSOR_800_600 |
                                    DSOR_1024_768 | DSOR_1152_864  | DSOR_1280_720  | DSOR_1280_768 |
                                    DSOR_1280_960 | DSOR_1280_1024 | DSOR_1400_1050 | DSOR_1600_1200 |
                                    DSOR_1920_1080 | DSOR_960_540 | DSOR_1440_540;

     config->flags          = DSECONF_TV_STANDARD | DSECONF_SCANMODE   | DSECONF_FREQUENCY |
                              DSECONF_CONNECTORS  | DSECONF_RESOLUTION | DSECONF_FRAMING;
     config->tv_standard    = DSETV_DIGITAL;
     config->out_connectors = DSOC_COMPONENT | DSOC_HDMI;
     config->scanmode       = DSESM_PROGRESSIVE;
     config->frequency      = DSEF_60HZ;
     config->framing        = DSEPF_MONO;
     config->resolution     = DSOR_1280_720;

     return DFB_OK;
}

static DFBResult
primaryTestEncoderConfig( CoreScreen                   *screen,
                          void                         *driver_data,
                          void                         *screen_data,
                          int                           encoder,
                          const DFBScreenEncoderConfig *config,
                          DFBScreenEncoderConfigFlags  *failed )
{
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

     (void) shared;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
primarySetEncoderConfig( CoreScreen                   *screen,
                         void                         *driver_data,
                         void                         *screen_data,
                         int                           encoder,
                         const DFBScreenEncoderConfig *config )
{
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

     int hor[] = { 640,720,720,800,1024,1152,1280,1280,1280,1280,1400,1600,1920 };
     int ver[] = { 480,480,576,600, 768, 864, 720, 768, 960,1024,1050,1200,1080 };

     int res;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     (void)encoder; /* all outputs are active */

     res = D_BITn32(config->resolution);
     if ( (res == -1) || (res >= D_ARRAY_SIZE(hor)) )
          return DFB_INVARG;

     shared->screen_size.w = hor[res];
     shared->screen_size.h = ver[res];

     return DFB_OK;
}

static DFBResult
primaryInitOutput( CoreScreen                   *screen,
                   void                         *driver_data,
                   void                         *screen_data,
                   int                           output,
                   DFBScreenOutputDescription   *description,
                   DFBScreenOutputConfig        *config )
{
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

     (void) shared;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     direct_snputs( description->name, "X11 Output", DFB_SCREEN_OUTPUT_DESC_NAME_LENGTH );

     description->caps = DSOCAPS_RESOLUTION;

     config->flags      |= DSOCONF_RESOLUTION;
     config->resolution  = DSOR_UNKNOWN;

     return DFB_OK;
}

static DFBResult
primaryTestOutputConfig( CoreScreen                  *screen,
                         void                        *driver_data,
                         void                        *screen_data,
                         int                          output,
                         const DFBScreenOutputConfig *config,
                         DFBScreenOutputConfigFlags  *failed )
{
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

     (void) shared;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
primarySetOutputConfig( CoreScreen                  *screen,
                        void                        *driver_data,
                        void                        *screen_data,
                        int                          output,
                        const DFBScreenOutputConfig *config )
{
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

     int hor[] = { 640,720,720,800,1024,1152,1280,1280,1280,1280,1400,1600,1920 };
     int ver[] = { 480,480,576,600, 768, 864, 720, 768, 960,1024,1050,1200,1080 };

     int res;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     (void)output; /* all outputs are active */

     /* we support screen resizing only */
     if (config->flags != DSOCONF_RESOLUTION)
          return DFB_INVARG;

     res = D_BITn32(config->resolution);
     if ( (res == -1) || (res >= D_ARRAY_SIZE(hor)) )
          return DFB_INVARG;

     shared->screen_size.w = hor[res];
     shared->screen_size.h = ver[res];

     return DFB_OK;
}

static ScreenFuncs primaryScreenFuncs = {
     .InitScreen        = primaryInitScreen,
     .GetScreenSize     = primaryGetScreenSize,
     .InitEncoder       = primaryInitEncoder,
     .TestEncoderConfig = primaryTestEncoderConfig,
     .SetEncoderConfig  = primarySetEncoderConfig,
     .InitOutput        = primaryInitOutput,
     .TestOutputConfig  = primaryTestOutputConfig,
     .SetOutputConfig   = primarySetOutputConfig
};

ScreenFuncs *x11PrimaryScreenFuncs = &primaryScreenFuncs;

/******************************************************************************/

static int
primaryLayerDataSize( void )
{
     return sizeof(X11LayerData);
}

static int
primaryRegionDataSize( void )
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
     char         *name;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     {
          static int     layer_counter = 0;
          X11LayerData  *lds           = layer_data;

          char *names[] = { "Primary", "Secondary", "Tertiary" };
          name = "Other";
          if( layer_counter < 3 )
               name = names[layer_counter];

          lds->layer_id = layer_counter;
          layer_counter++;
     }

     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE | DLCAPS_LR_MONO | DLCAPS_STEREO;
     description->type = DLTF_GRAPHICS;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "X11 %s Layer", name );

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

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

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
          case DSPF_LUT8:
          case DSPF_RGB16:
          case DSPF_NV16:
          case DSPF_RGB444:
          case DSPF_ARGB4444:
          case DSPF_RGBA4444:
          case DSPF_RGB555:
          case DSPF_ARGB1555:
          case DSPF_RGBA5551:
          case DSPF_BGR555:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_ABGR:
          case DSPF_AYUV:
          case DSPF_AVYU:
          case DSPF_VYU:
          case DSPF_UYVY:
          case DSPF_ARGB8565:
          case DSPF_RGBAF88871:
          case DSPF_YUV444P:
          case DSPF_YV16:
               break;

          default:
               fail |= CLRCF_FORMAT;
               break;
     }

     if (config->options & ~(DLOP_ALPHACHANNEL | DLOP_LR_MONO | DLOP_STEREO))
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
                  CoreSurfaceBufferLock      *left_lock,
                  CoreSurfaceBufferLock      *right_lock )
{
     DFBResult  ret;

     DFBX11       *x11 = driver_data;
     X11LayerData *lds = layer_data;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     if (x11->shared->x_error)
          return DFB_FAILURE;

     lds->config = *config;

     ret = dfb_x11_create_window( x11, lds, config );
     if (ret)
          return ret;

     x11->shared->stereo       = !!(lds->config.options & DLOP_STEREO);
     x11->shared->stereo_width = lds->config.width / 2;

     if (palette)
          dfb_x11_set_palette( x11, lds, palette );

     return DFB_OK;
}

static DFBResult
primarySetStereoDepth( CoreLayer              *layer,
                       void                   *driver_data,
                       void                   *layer_data,
                       bool                    follow_video,
                       int                     z )
{
     return DFB_OK;
}

static DFBResult
primaryRemoveRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data )
{
     DFBX11       *x11 = driver_data;
     X11LayerData *lds = layer_data;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     if (x11->shared->x_error)
          return DFB_FAILURE;

     dfb_x11_destroy_window( x11, lds );

     return DFB_OK;
}

static DFBResult
primaryFlipRegion( CoreLayer             *layer,
                   void                  *driver_data,
                   void                  *layer_data,
                   void                  *region_data,
                   CoreSurface           *surface,
                   DFBSurfaceFlipFlags    flags,
                   CoreSurfaceBufferLock *left_lock,
                   CoreSurfaceBufferLock *right_lock )
{
     DFBX11       *x11 = driver_data;
     X11LayerData *lds = layer_data;

     DFBRegion  region = DFB_REGION_INIT_FROM_DIMENSION( &surface->config.size );

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     if (x11->shared->x_error)
          return DFB_FAILURE;

     dfb_surface_flip( surface, false );

     dfb_surface_notify_display( surface, left_lock->buffer );

     if (lds->config.options & DLOP_STEREO)
          dfb_surface_notify_display( surface, right_lock->buffer );

     return dfb_x11_update_screen( x11, lds, &region, &region, left_lock, right_lock );
}

static DFBResult
primaryUpdateRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data,
                     CoreSurface           *surface,
                     const DFBRegion       *left_update,
                     CoreSurfaceBufferLock *left_lock,
                     const DFBRegion       *right_update,
                     CoreSurfaceBufferLock *right_lock )
{
     DFBX11       *x11 = driver_data;
     X11LayerData *lds = layer_data;

     DFBRegion  left_region  = DFB_REGION_INIT_FROM_DIMENSION( &surface->config.size );
     DFBRegion  right_region = DFB_REGION_INIT_FROM_DIMENSION( &surface->config.size );

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     if (x11->shared->x_error)
          return DFB_FAILURE;

     if (left_update && !dfb_region_region_intersect( &left_region, left_update ))
          return DFB_OK;

     if (right_update && !dfb_region_region_intersect( &right_region, right_update ))
          return DFB_OK;

     return dfb_x11_update_screen( x11, lds, &left_region, &right_region, left_lock, right_lock );
}

static DisplayLayerFuncs primaryLayerFuncs = {
     .LayerDataSize  = primaryLayerDataSize,
     .RegionDataSize = primaryRegionDataSize,
     .InitLayer      = primaryInitLayer,

     .TestRegion     = primaryTestRegion,
     .AddRegion      = primaryAddRegion,
     .SetRegion      = primarySetRegion,
     .SetStereoDepth = primarySetStereoDepth,
     .RemoveRegion   = primaryRemoveRegion,
     .FlipRegion     = primaryFlipRegion,
     .UpdateRegion   = primaryUpdateRegion,
};

DisplayLayerFuncs *x11PrimaryLayerFuncs = &primaryLayerFuncs;

/******************************************************************************/

static DFBResult
update_screen( DFBX11 *x11, const DFBRectangle *clip, CoreSurfaceBufferLock *lock, XWindow *xw )
{
     void                  *dst;
     void                  *src;
     unsigned int           offset = 0;
     XImage                *ximage;
     CoreSurface           *surface;
     CoreSurfaceAllocation *allocation;
     DFBX11Shared          *shared;
     DFBRectangle           rect;
     bool                   direct = false;

     D_ASSERT( x11 != NULL );
     DFB_RECTANGLE_ASSERT( clip );

     D_DEBUG_AT( X11_Update, "%s( %4d,%4d-%4dx%4d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( clip ) );

     CORE_SURFACE_BUFFER_LOCK_ASSERT( lock );

     shared = x11->shared;
     D_ASSERT( shared != NULL );

     XLockDisplay( x11->display );

     if (!xw) {
          XUnlockDisplay( x11->display );
          return DFB_OK;
     }

     allocation = lock->allocation;
     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     surface = allocation->surface;
     D_ASSERT( surface != NULL );


     rect.x = rect.y = 0;
     rect.w = xw->width;
     rect.h = xw->height;

     if (!dfb_rectangle_intersect( &rect, clip )) {
          XUnlockDisplay( x11->display );
          return DFB_OK;
     }

     D_DEBUG_AT( X11_Update, "  -> %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( &rect ) );

#ifdef USE_GLX
     /* Check for GLX allocation... */
     if (allocation->pool == shared->glx_pool && lock->handle) {
          LocalPixmap *pixmap = lock->handle;

          D_MAGIC_ASSERT( pixmap, LocalPixmap );

          /* ...and just call SwapBuffers... */
          //D_DEBUG_AT( X11_Update, "  -> Calling glXSwapBuffers( 0x%lx )...\n", alloc->drawable );
          //glXSwapBuffers( x11->display, alloc->drawable );


          D_DEBUG_AT( X11_Update, "  -> Copying from GLXPixmap...\n" );

          glXWaitGL();

          XCopyArea( x11->display, pixmap->pixmap, xw->window, xw->gc,
                     rect.x, rect.y, rect.w, rect.h, rect.x, rect.y );

          glXWaitX();

          XUnlockDisplay( x11->display );

          return DFB_OK;
     }
#endif

     /* Check for our special native allocation... */
     if (allocation->pool == shared->x11image_pool && lock->handle) {
          x11Image *image = lock->handle;

          D_MAGIC_ASSERT( image, x11Image );

          /* ...and directly XShmPutImage from that. */
          ximage = image->ximage;

          direct = true;
     }
     else {
          /* ...or copy or convert into XShmImage or XImage allocated with the XWindow. */
          ximage = xw->ximage;
          offset = xw->ximage_offset;

          xw->ximage_offset = (offset ? 0 : ximage->height / 2);

          /* make sure the 16-bit input formats are properly 2-pixel-clipped */
          switch (surface->config.format) {
               case DSPF_I420:
               case DSPF_YV12:
               case DSPF_NV12:
               case DSPF_NV21:
                    if (rect.y & 1) {
                         rect.y--;
                         rect.h++;
                    }
                    /* fall through */
               case DSPF_YUY2:
               case DSPF_UYVY:
               case DSPF_NV16:
                    if (rect.x & 1) {
                         rect.x--;
                         rect.w++;
                    }
               default: /* no action */
                    break;
          }

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
                    if (surface->config.format == DSPF_LUT8) {
                         int width = rect.w; int height = rect.h;
                         const u8    *src8    = src;
                         u16         *dst16   = dst;
                         CorePalette *palette = surface->palette;
                         int          x;
                         while (height--) {

                              for (x=0; x<width; x++) {
                                   DFBColor color = palette->entries[src8[x]];
                                   dst16[x] = PIXEL_RGB16( color.r, color.g, color.b );
                              }

                              src8  += lock->pitch;
                              dst16 += ximage->bytes_per_line / 2;
                         }
                    }
                    else {
                    dfb_convert_to_rgb16( surface->config.format, src, lock->pitch,
                                          surface->config.size.h, dst, ximage->bytes_per_line, rect.w, rect.h );
                    }
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

     /* Wait for display if single buffered and not converted... */
     if (direct && !(surface->config.caps & DSCAPS_FLIPPING))
          XSync( x11->display, False );

     XUnlockDisplay( x11->display );

     return DFB_OK;
}

static void
update_scaled32( XWindow *xw, const DFBRectangle *clip, CoreSurfaceBufferLock *lock, int xoffset )
{
     u32 *dst;
     u64 *src;
     int  x, y;

     D_ASSERT( xw != NULL );
     DFB_RECTANGLE_ASSERT( clip );

     D_DEBUG_AT( X11_Update, "%s( %4d,%4d-%4dx%4d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( clip ) );

     CORE_SURFACE_BUFFER_LOCK_ASSERT( lock );

     dst = (u32*)(xw->virtualscreen + ((clip->x / 2) + xoffset) * xw->bpp + (clip->y + xw->ximage_offset) * xw->ximage->bytes_per_line);
     src = lock->addr + 4 * clip->x + clip->y * lock->pitch;

     for (y=0; y<clip->h; y++) {
          for (x=0; x<clip->w/2; x++) {
               u64 S2 = src[x];

               S2 &= ~0x0101010101010101ULL;
               S2 >>= 1;

               dst[x] = ((u32) S2) + ((u32) (S2 >> 32));
          }

          dst = (u32*)((u8*) dst + xw->ximage->bytes_per_line);
          src = (u64*)((u8*) src + lock->pitch);
     }
}

static DFBResult
update_stereo( DFBX11 *x11, const DFBRectangle *left_clip, const DFBRectangle *right_clip,
               CoreSurfaceBufferLock *left_lock, CoreSurfaceBufferLock *right_lock, XWindow *xw )
{
     CoreSurface  *surface;
     DFBRectangle  rect;
     DFBRectangle  left;
     DFBRectangle  right;

     D_ASSERT( x11 != NULL );
     DFB_RECTANGLE_ASSERT( left_clip );
     DFB_RECTANGLE_ASSERT( right_clip );

     D_DEBUG_AT( X11_Update, "%s( %4d,%4d-%4dx%4d | %4d,%4d-%4dx%4d )\n",
                 __FUNCTION__, DFB_RECTANGLE_VALS( left_clip ), DFB_RECTANGLE_VALS( right_clip ) );

     CORE_SURFACE_BUFFER_LOCK_ASSERT( left_lock );
     CORE_SURFACE_BUFFER_LOCK_ASSERT( right_lock );

     XLockDisplay( x11->display );

     if (!xw) {
          XUnlockDisplay( x11->display );
          return DFB_OK;
     }

     D_ASSERT( left_lock->allocation->surface == right_lock->allocation->surface );

     surface = left_lock->allocation->surface;
     D_ASSERT( surface != NULL );

     switch (surface->config.format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     if (!left_lock->addr || !right_lock->addr)
          return DFB_UNSUPPORTED;

     xw->ximage_offset = (xw->ximage_offset ? 0 : xw->height);

     left  = *left_clip;
     right = *right_clip;

     if (left.x & 1) {
          left.x--;
          left.w++;
     }

     if (left.w & 1)
          left.w++;

     if (right.x & 1) {
          right.x--;
          right.w++;
     }

     if (right.w & 1)
          right.w++;

     update_scaled32( xw, &left, left_lock, 0 );
     update_scaled32( xw, &right, right_lock, xw->width / 2 );

     // FIXME
     rect.x = 0;
     rect.y = 0;
     rect.w = xw->width;
     rect.h = xw->height;

     /* Wait for previous data to be processed... */
     XSync( x11->display, False );

     /* ...and immediately queue or send the next! */
     if (x11->use_shm) {
          /* Just queue the command, it's XShm :) */
          XShmPutImage( xw->display, xw->window, xw->gc, xw->ximage,
                        rect.x, rect.y + xw->ximage_offset, rect.x, rect.y, rect.w, rect.h, False );

          /* Make sure the queue has really happened! */
          XFlush( x11->display );
     }
     else
          /* Initiate transfer of buffer... */
          XPutImage( xw->display, xw->window, xw->gc, xw->ximage,
                     rect.x, rect.y + xw->ximage_offset, rect.x, rect.y, rect.w, rect.h );

     XUnlockDisplay( x11->display );

     return DFB_OK;
}

/******************************************************************************/

DFBResult
dfb_x11_create_window_handler( DFBX11 *x11, SetModeData *setmode )
{
     XWindow                *xw;
     DFBX11Shared           *shared = x11->shared;
     CoreLayerRegionConfig  *config;

     config = &setmode->config;
     xw     = *(setmode->xw);

     D_DEBUG_AT( X11_Layer, "%s( %p )\n", __FUNCTION__, config );

     D_DEBUG_AT( X11_Layer, "  -> %4dx%4d %s\n", config->width, config->height, dfb_pixelformat_name(config->format) );

     XLockDisplay( x11->display );

     if (xw != NULL) {
          if (xw->width == config->width && xw->height == config->height) {
               XUnlockDisplay( x11->display );
               return DFB_OK;
          }

          *(setmode->xw) = NULL;
          dfb_x11_close_window( x11, xw );
          shared->window_count--;
     }

     bool bSucces = dfb_x11_open_window( x11, &xw, dfb_config->x11_position.x, dfb_config->x11_position.y,
                                         config->width, config->height, config->format );

     /* Set video mode */
     if ( !bSucces ) {
          D_ERROR( "DirectFB/X11: Couldn't open %dx%d window!\n", config->width, config->height );

          XUnlockDisplay( x11->display );
          return DFB_FAILURE;
     }
     else {
          *(setmode->xw) = xw;
          shared->window_count++;
     }

     XUnlockDisplay( x11->display );
     return DFB_OK;
}

DFBResult
dfb_x11_destroy_window_handler( DFBX11 *x11, DestroyData *destroy )
{
     DFBX11Shared *shared = x11->shared;
     XWindow      *xw;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     XLockDisplay( x11->display );

     xw = *(destroy->xw);

     if (xw) {
          *(destroy->xw) = NULL;

          dfb_x11_close_window( x11, xw );
          shared->window_count--;
     }

     XSync( x11->display, False );

     XUnlockDisplay( x11->display );

     return DFB_OK;
}

DFBResult
dfb_x11_update_screen_handler( DFBX11 *x11, UpdateScreenData *data )
{
     D_DEBUG_AT( X11_Update, "%s( %p )\n", __FUNCTION__, data );

     if (data->stereo) {
          DFBRectangle left_rect;
          DFBRectangle right_rect;

          left_rect  = DFB_RECTANGLE_INIT_FROM_REGION( &data->left_region );
          right_rect = DFB_RECTANGLE_INIT_FROM_REGION( &data->right_region );

          if (data->left_lock.buffer && data->right_lock.buffer)
               update_stereo( x11, &left_rect, &right_rect, &data->left_lock, &data->right_lock, data->xw );
     }
     else {
          DFBRectangle rect;

          rect = DFB_RECTANGLE_INIT_FROM_REGION( &data->left_region );

          if (data->left_lock.buffer)
               update_screen( x11, &rect, &data->left_lock, data->xw );
     }

     data->left_lock.buffer  = NULL;
     data->right_lock.buffer = NULL;

     return DFB_OK;
}

DFBResult
dfb_x11_set_palette_handler( DFBX11 *x11, CorePalette *palette )
{
     return DFB_OK;
}

