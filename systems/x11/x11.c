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

#include <config.h>

#include <fusion/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <directfb.h>

#include <fusion/arena.h>
#include <fusion/shmalloc.h>
#include <fusion/lock.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/messages.h>


#include "primary.h"
#include "xwindow.h"
#include "x11.h"
#include "x11_surface_pool.h"
#include "x11_surface_pool_bridge.h"

#ifdef USE_GLX
#include "glx_surface_pool.h"
#endif

#include "vpsmem_surface_pool.h"

#include <core/core_system.h>

D_DEBUG_DOMAIN( X11_Core, "X11/Core", "Main X11 system functions" );

DFB_CORE_SYSTEM( x11 )


static VideoMode modes[] = {
     { .xres =  320, .yres =  200 },
     { .xres =  320, .yres =  240 },
     { .xres =  512, .yres =  384 },
     { .xres =  640, .yres =  480 },
     { .xres =  768, .yres =  576 },

     { .xres = 1024, .yres =  576 },     // 16:9
     { .xres = 1024, .yres =  600 },     // Where does that mode come from? :-)
     { .xres = 1024, .yres =  768 },     // 4:3

     { .xres = 1280, .yres =  720 },     // 16:9
     { .xres = 1280, .yres =  960 },     // 4:3
     { .xres = 1280, .yres = 1024 },     // 5:4

     { .xres = 1440, .yres =  810 },     // 16:9
     { .xres = 1440, .yres = 1080 },     // 4:3

     { .xres = 1600, .yres =  900 },     // 16:9, obviously :)
     { .xres = 1600, .yres = 1200 },     // 4:3

     { .xres = 1920, .yres = 1080 },     // 16:9
     { .xres = 1920, .yres = 1200 },     // 16:10

     { .xres = 0, .yres = 0 }
};

/**********************************************************************************************************************/

static FusionCallHandlerResult call_handler( int           caller,
                                             int           call_arg,
                                             void         *call_ptr,
                                             void         *ctx,
                                             unsigned int  serial,
                                             int          *ret_val );

/**********************************************************************************************************************/

static DFBResult
InitLocal( DFBX11 *x11, DFBX11Shared *shared, CoreDFB *core )
{
     int i, n;

     XInitThreads();

     x11->shared = shared;
     x11->core   = core;

     x11->display = XOpenDisplay(getenv("DISPLAY"));
     if (!x11->display) {
          D_ERROR("X11: Error in XOpenDisplay for '%s'\n", getenv("DISPLAY"));
          return DFB_INIT;
     }

     x11->screenptr = DefaultScreenOfDisplay(x11->display);
     x11->screennum = DefaultScreen(x11->display);

     for (i=0; i<x11->screenptr->ndepths; i++) {
          const Depth *depth = &x11->screenptr->depths[i];

          for (n=0; n<depth->nvisuals; n++) {
               Visual *visual = &depth->visuals[n];

               D_DEBUG_AT( X11_Core, "[Visual %d] ID 0x%02lx, depth %d, RGB 0x%06lx/0x%06lx/0x%06lx, %d bpRGB, %d entr.\n",
                        n, visual->visualid, depth->depth,
                        visual->red_mask, visual->green_mask, visual->blue_mask,
                        visual->bits_per_rgb, visual->map_entries );

               switch (depth->depth) {
                    case 32:
                         if (visual->red_mask   == 0xff0000 &&
                             visual->green_mask == 0x00ff00 &&
                             visual->blue_mask  == 0x0000ff &&
                             !x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_ARGB)])
                              x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_ARGB)] = visual;
                         break;

                    case 24:
                         if (visual->red_mask   == 0xff0000 &&
                             visual->green_mask == 0x00ff00 &&
                             visual->blue_mask  == 0x0000ff &&
                             !x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)])
                              x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)] = visual;
                         break;

                    case 16:
                         if (visual->red_mask   == 0xf800 &&
                             visual->green_mask == 0x07e0 &&
                             visual->blue_mask  == 0x001f &&
                             !x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)])
                              x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)] = visual;
                         break;

                    case 15:
                         if (visual->red_mask   == 0x7c00 &&
                             visual->green_mask == 0x03e0 &&
                             visual->blue_mask  == 0x001f &&
                             !x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB555)])
                              x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB555)] = visual;
                         break;
               }
          }
     }

     if (XShmQueryExtension( x11->display ))
          XShmQueryVersion( x11->display, &x11->xshm_major, &x11->xshm_minor, &x11->use_shm );


     x11->screen = dfb_screens_register( NULL, x11, &x11PrimaryScreenFuncs );

     dfb_layers_register( x11->screen, x11, &x11PrimaryLayerFuncs );

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_X11;   
     info->caps = CSCAPS_ACCELERATION;

     D_DEBUG_AT( X11_Core, "%s()\n", __FUNCTION__ );

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "X11" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     DFBResult     ret;
     DFBX11       *x11;
     DFBX11Shared *shared;

     D_DEBUG_AT( X11_Core, "%s()\n", __FUNCTION__ );

     x11 = D_CALLOC( 1, sizeof(DFBX11) );
     if (!x11)
          return D_OOM();

     shared = SHCALLOC( dfb_core_shmpool( core ), 1, sizeof(DFBX11Shared) );
     if (!shared) {
          D_FREE( x11 );
          return D_OOSHM();
     }


     /*
      * Local init (master and slave)
      */
     ret = InitLocal( x11, shared, core );
     if (ret) {
          SHFREE( dfb_core_shmpool( core ), shared );
          D_FREE( x11 );
          return ret;
     }


     /*
      * Shared init (master only)
      */
     shared->data_shmpool = dfb_core_shmpool_data( core );

     shared->screen_size.w = x11->screenptr->width;
     shared->screen_size.h = x11->screenptr->height;

     fusion_skirmish_init( &shared->lock, "X11 System", dfb_core_world(core) );

     fusion_call_init( &shared->call, call_handler, x11, dfb_core_world(core) );


     /*
      * Must be set before initializing the pools!
      */
     *data = x11;


     /*
      * Master init
      */
     dfb_surface_pool_initialize( core, &x11SurfacePoolFuncs, &shared->x11image_pool );

#ifdef USE_GLX
     dfb_surface_pool_initialize( core, &glxSurfacePoolFuncs, &shared->glx_pool );
#endif

     if (dfb_config->video_length) {
          shared->vpsmem_length = dfb_config->video_length;

          dfb_surface_pool_initialize( core, &vpsmemSurfacePoolFuncs, &shared->vpsmem_pool );
     }

#ifdef USE_GLX
     dfb_surface_pool_bridge_initialize( core, &x11SurfacePoolBridgeFuncs, x11, &shared->x11_pool_bridge );
#endif

     fusion_arena_add_shared_field( dfb_core_arena( core ), "x11", shared );

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     DFBResult     ret;
     void         *ptr;
     DFBX11       *x11;
     DFBX11Shared *shared;

     D_DEBUG_AT( X11_Core, "%s()\n", __FUNCTION__ );

     x11 = D_CALLOC( 1, sizeof(DFBX11) );
     if (!x11)
          return D_OOM();

     fusion_arena_get_shared_field( dfb_core_arena( core ), "x11", &ptr );
     shared = ptr;


     /*
      * Local init (master and slave)
      */
     ret = InitLocal( x11, shared, core );
     if (ret) {
          D_FREE( x11 );
          return ret;
     }


     /*
      * Must be set before joining the pools!
      */
     *data = x11;


     /*
      * Slave init
      */
     if (shared->x11image_pool)
          dfb_surface_pool_join( core, shared->x11image_pool, &x11SurfacePoolFuncs );

#ifdef USE_GLX
     if (shared->glx_pool)
          dfb_surface_pool_join( core, shared->glx_pool, &glxSurfacePoolFuncs );
#endif

     if (shared->vpsmem_pool)
          dfb_surface_pool_join( core, shared->vpsmem_pool, &vpsmemSurfacePoolFuncs );

#ifdef USE_GLX
     if (shared->x11_pool_bridge)
          dfb_surface_pool_bridge_join( core, shared->x11_pool_bridge, &x11SurfacePoolBridgeFuncs, x11 );
#endif

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     DFBX11       *x11    = dfb_system_data();
     DFBX11Shared *shared = x11->shared;

     D_DEBUG_AT( X11_Core, "%s()\n", __FUNCTION__ );

     /*
      * Master deinit
      */
     if (shared->x11_pool_bridge)
          dfb_surface_pool_bridge_destroy( shared->x11_pool_bridge );

     if (shared->vpsmem_pool)
          dfb_surface_pool_destroy( shared->vpsmem_pool );

     if (shared->glx_pool)
          dfb_surface_pool_destroy( shared->glx_pool );

     if (shared->x11image_pool)
          dfb_surface_pool_destroy( shared->x11image_pool );


     /*
      * Shared deinit (master only)
      */
     fusion_call_destroy( &shared->call );

     fusion_skirmish_prevail( &shared->lock );

     if (shared->xw)
         dfb_x11_close_window( x11, shared->xw );

     fusion_skirmish_destroy( &shared->lock );


     SHFREE( dfb_core_shmpool( x11->core ), shared );


     /*
      * Local deinit (master and slave)
      */
     if (x11->display)
         XCloseDisplay( x11->display );

     D_FREE( x11 );

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     DFBX11       *x11    = dfb_system_data();
     DFBX11Shared *shared = x11->shared;

     D_DEBUG_AT( X11_Core, "%s()\n", __FUNCTION__ );

     /*
      * Slave deinit
      */
     if (shared->x11_pool_bridge)
          dfb_surface_pool_bridge_leave( shared->x11_pool_bridge );

     if (shared->vpsmem_pool)
          dfb_surface_pool_leave( shared->vpsmem_pool );

     if (shared->glx_pool)
          dfb_surface_pool_leave( shared->glx_pool );

     if (shared->x11image_pool)
          dfb_surface_pool_leave( shared->x11image_pool );


     /*
      * Local deinit (master and slave)
      */
     if (x11->display)
         XCloseDisplay( x11->display );

     D_FREE( x11 );

     return DFB_OK;
}

static DFBResult
system_suspend( void )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
system_resume( void )
{
     return DFB_UNIMPLEMENTED;
}

static volatile void *
system_map_mmio( unsigned int    offset,
                 int             length )
{
     return NULL;
}

static void
system_unmap_mmio( volatile void  *addr,
                   int             length )
{
}

static int
system_get_accelerator( void )
{
     return dfb_config->accelerator;
}

static VideoMode *
system_get_modes( void )
{
     return modes;
}

static VideoMode *
system_get_current_mode( void )
{
     return &modes[0];   /* FIXME */
}

static DFBResult
system_thread_init( void )
{
     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     return false;
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_videoram_length( void )
{
     return 0;
}

static unsigned long
system_aux_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_aux_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_auxram_length( void )
{
     return 0;
}

static void
system_get_busid( int *ret_bus, int *ret_dev, int *ret_func )
{
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
}

static FusionCallHandlerResult
call_handler( int           caller,
              int           call_arg,
              void         *call_ptr,
              void         *ctx,
              unsigned int  serial,
              int          *ret_val )
{
     DFBX11 *x11 = ctx;

     switch (call_arg) {
          case X11_CREATE_WINDOW:
               *ret_val = dfb_x11_create_window_handler( x11, call_ptr );
               break;

          case X11_DESTROY_WINDOW:
               *ret_val = dfb_x11_destroy_window_handler( x11 );
               break;

          case X11_UPDATE_SCREEN:
               *ret_val = dfb_x11_update_screen_handler( x11, call_ptr );
               break;

          case X11_SET_PALETTE:
               *ret_val = dfb_x11_set_palette_handler( x11, call_ptr );
               break;

          case X11_IMAGE_INIT:
               *ret_val = dfb_x11_image_init_handler( x11, call_ptr );
               break;

          case X11_IMAGE_DESTROY:
               *ret_val = dfb_x11_image_destroy_handler( x11, call_ptr );
               break;

          default:
               D_BUG( "unknown call" );
               *ret_val = DFB_BUG;
               break;
     }

     return FCHR_RETURN;
}

