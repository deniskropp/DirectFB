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

#include "x11.h"
#include "primary.h"

D_DEBUG_DOMAIN( X11_Layer,  "X11/Layer",  "X11 Layer" );
D_DEBUG_DOMAIN( X11_Update, "X11/Update", "X11 Update" );

/**********************************************************************************************************************/

static int error_code = 0;

static int
error_handler( Display *display, XErrorEvent *event )
{
     char buf[512];

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     XGetErrorText( display, event->error_code, buf, sizeof(buf) );

     D_ERROR( "X11/Window: Error! %s\n", buf );

     error_code = event->error_code;

     return 0;
}

static DFBResult
primaryInitScreen( CoreScreen           *screen,
                   CoreGraphicsDevice   *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;
     void         *old_error_handler = 0;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     /* Set the screen capabilities. */
     description->caps    = DSCCAPS_OUTPUTS;
     description->outputs = 1;

     /* Set the screen name. */
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH, "X11/VDPAU Primary Screen" );


     shared->depth = DefaultDepthOfScreen( x11->screenptr );



     XSetWindowAttributes attr = { .background_pixmap = 0 };

     attr.event_mask =
            ButtonPressMask
          | ButtonReleaseMask
          | PointerMotionMask
          | KeyPressMask
          | KeyReleaseMask
          | ExposureMask
          | StructureNotifyMask;

     XLockDisplay( x11->display );

     old_error_handler = XSetErrorHandler( error_handler );

     error_code = 0;

     shared->window = XCreateWindow( x11->display,
                                     RootWindowOfScreen(x11->screenptr),
                                     0, 0, shared->screen_size.w, shared->screen_size.h, 0, shared->depth, InputOutput,
                                     DefaultVisualOfScreen(x11->screenptr), CWEventMask, &attr );
     XSync( x11->display, False );

     if (!shared->window || error_code) {
          D_ERROR( "DirectFB/X11/VDPAU: XCreateWindow() failed!\n" );
          XUnlockDisplay( x11->display );
          return DFB_FAILURE;
     }

     XSelectInput( x11->display, shared->window,
                   ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                   KeyPressMask | KeyReleaseMask | StructureNotifyMask );


     XSizeHints Hints;

     /*
      * Here we inform the function of what we are going to change for the
      * window (there's also PPosition but it's obsolete)
      */
     Hints.flags = PSize | PMinSize | PMaxSize;

     /*
      * Now we set the structure to the values we need for width & height.
      * For esthetic reasons we set Width=MinWidth=MaxWidth.
      * The same goes for Height. You can try whith differents values, or
      * let's use Hints.flags=Psize; and resize your window..
      */
     Hints.min_width  = Hints.max_width  = Hints.base_width  = shared->screen_size.w;
     Hints.min_height = Hints.max_height = Hints.base_height = shared->screen_size.h;

     /* Now we can set the size hints for the specified window */
     XSetWMNormalHints( x11->display, shared->window, &Hints );

     /* We change the title of the window (default:Untitled) */
     XStoreName( x11->display, shared->window, "DirectFB/VDPAU" );


     /* maps the window and raises it to the top of the stack */
     XMapRaised( x11->display, shared->window );


     XSetErrorHandler( old_error_handler );


     VdpStatus status;

     status = x11->vdp.PresentationQueueTargetCreateX11( x11->vdp.device, shared->window, &shared->vdp_target );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: PresentationQueueTargetCreateX11() failed (status %d, '%s')!\n",
                   status, x11->vdp.GetErrorString( status ) );
          XUnlockDisplay( x11->display );
          return DFB_FAILURE;
     }

     status = x11->vdp.PresentationQueueCreate( x11->vdp.device, shared->vdp_target, &shared->vdp_queue );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: PresentationQueueCreate() failed (status %d, '%s')!\n",
                   status, x11->vdp.GetErrorString( status ) );
          XUnlockDisplay( x11->display );
          return DFB_FAILURE;
     }

     XUnlockDisplay( x11->display );

     return DFB_OK;
}

static DFBResult
primaryShutdownScreen( CoreScreen *screen,
                       void       *driver_data,
                       void       *screen_data )
{
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

     VdpStatus status;

     status = x11->vdp.PresentationQueueDestroy( shared->vdp_queue );
     if (status)
          D_ERROR( "DirectFB/X11/VDPAU: PresentationQueueDestroy() failed (status %d, '%s')!\n",
                   status, x11->vdp.GetErrorString( status ) );

     status = x11->vdp.PresentationQueueTargetDestroy( shared->vdp_target );
     if (status)
          D_ERROR( "DirectFB/X11/VDPAU: PresentationQueueTargetDestroy() failed (status %d, '%s')!\n",
                   status, x11->vdp.GetErrorString( status ) );

     XDestroyWindow( x11->display, shared->window );

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

     description->caps = DSOCAPS_RESOLUTION;

     config->flags |= DSOCONF_RESOLUTION;
     config->resolution = DSOR_UNKNOWN;

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

     // FIXME: recreate window/target etc.

     return DFB_OK;
}

static ScreenFuncs primaryScreenFuncs = {
     .InitScreen       = primaryInitScreen,
     .ShutdownScreen   = primaryShutdownScreen,
     .GetScreenSize    = primaryGetScreenSize,
     .InitOutput       = primaryInitOutput,
     .TestOutputConfig = primaryTestOutputConfig,
     .SetOutputConfig  = primarySetOutputConfig
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
     description->caps = DLCAPS_SURFACE;
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
          case DSPF_ARGB:
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
                  CoreSurfaceBufferLock      *left_lock,
                  CoreSurfaceBufferLock      *right_lock )
{
     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
primaryRemoveRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data )
{
     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
DisplaySurface( DFBX11               *x11,
                VdpPresentationQueue  queue,
                VdpOutputSurface      surface )
{
//     VdpStatus status;
     VdpTime   current = 0;
/*
     status = vdp->PresentationQueueGetTime( queue, &current );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: PresentationQueueGetTime() failed (status %d, '%s')!\n",
                   status, vdp->GetErrorString( status ) );
          return DFB_FAILURE;
     }
*/

     DirectResult                       ret;
     DFBX11CallPresentationQueueDisplay display;

     display.presentation_queue         = queue;
     display.surface                    = surface;
     display.clip_width                 = 0;
     display.clip_height                = 0;
     display.earliest_presentation_time = current;

     ret = fusion_call_execute2( &x11->shared->call, FCEF_ONEWAY, X11_VDPAU_PRESENTATION_QUEUE_DISPLAY, &display, sizeof(display), NULL );
     if (ret) {
          D_DERROR( ret, "DirectFB/X11/VDPAU: fusion_call_execute2() failed!\n" );
          return ret;
     }

/*
     status = vdp->PresentationQueueDisplay( queue, surface, 0, 0, current );
     if (status) {
          D_ERROR( "DirectFB/X11/VDPAU: PresentationQueueDisplay() failed (status %d, '%s')!\n",
                   status, vdp->GetErrorString( status ) );
          return DFB_FAILURE;
     }
*/
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
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     dfb_surface_flip( surface, false );

     return DisplaySurface( x11, shared->vdp_queue, (VdpOutputSurface) (unsigned long) left_lock->handle );
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
     DFBX11       *x11    = driver_data;
     DFBX11Shared *shared = x11->shared;

     DFBRegion     region = DFB_REGION_INIT_FROM_DIMENSION( &surface->config.size );

     D_DEBUG_AT( X11_Layer, "%s()\n", __FUNCTION__ );

     if (left_update && !dfb_region_region_intersect( &region, left_update ))
          return DFB_OK;

     return DisplaySurface( x11, shared->vdp_queue, (VdpOutputSurface) (unsigned long) left_lock->handle );
}

static DisplayLayerFuncs primaryLayerFuncs = {
     .LayerDataSize  = primaryLayerDataSize,
     .RegionDataSize = primaryRegionDataSize,
     .InitLayer      = primaryInitLayer,

     .TestRegion     = primaryTestRegion,
     .AddRegion      = primaryAddRegion,
     .SetRegion      = primarySetRegion,
     .RemoveRegion   = primaryRemoveRegion,
     .FlipRegion     = primaryFlipRegion,
     .UpdateRegion   = primaryUpdateRegion,
};

DisplayLayerFuncs *x11PrimaryLayerFuncs = &primaryLayerFuncs;

