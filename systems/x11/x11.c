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

#include <core/core_system.h>


DFB_CORE_SYSTEM( x11 )

DFBX11*   dfb_x11      = NULL;
CoreDFB*  dfb_x11_core = NULL;

static VideoMode modes[] = {
     {  320,  200 },
     {  320,  240 },
     {  512,  384 },
     {  640,  480 },
     {  768,  576 },
     { 1024,  600 },
     { 1024,  768 },
     { 1280, 1024 },
     { 1600, 1200 },

     { 0, 0 }
};

/**********************************************************************************************************************/

static FusionCallHandlerResult call_handler( int           caller,
                                             int           call_arg,
                                             void         *call_ptr,
                                             void         *ctx,
                                             unsigned int  serial,
                                             int          *ret_val );

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_X11;   

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "X11" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     int         i, n;
     CoreScreen *screen;

     D_ASSERT( dfb_x11 == NULL );

     dfb_x11 = (DFBX11*) SHCALLOC( dfb_core_shmpool(core), 1, sizeof(DFBX11) );
     if (!dfb_x11) {
          D_ERROR( "DirectFB/X11: Couldn't allocate shared memory!\n" );
          return D_OOSHM();
     }

     dfb_x11_core = core;

     fusion_skirmish_init( &dfb_x11->lock, "X11 System", dfb_core_world(core) );

     fusion_call_init( &dfb_x11->call, call_handler, NULL, dfb_core_world(core) );

     dfb_surface_pool_initialize( core, &x11SurfacePoolFuncs, &dfb_x11->surface_pool );

     screen = dfb_screens_register( NULL, NULL, &x11PrimaryScreenFuncs );

     dfb_layers_register( screen, NULL, &x11PrimaryLayerFuncs );

     fusion_arena_add_shared_field( dfb_core_arena( core ), "x11", dfb_x11 );

     *data = dfb_x11;

     XInitThreads();

     dfb_x11->display = XOpenDisplay(NULL);
     if (!dfb_x11->display) {
          D_ERROR("X11: Error opening X_Display\n");
          return DFB_INIT;
     }

     dfb_x11->screenptr = DefaultScreenOfDisplay(dfb_x11->display);
     dfb_x11->screennum = DefaultScreen(dfb_x11->display);

     for (i=0; i<dfb_x11->screenptr->ndepths; i++) {
          const Depth *depth = &dfb_x11->screenptr->depths[i];

          D_INFO( "X11/Display: Depth %d\n", depth->depth );

          for (n=0; n<depth->nvisuals; n++) {
               Visual *visual = &depth->visuals[n];

               D_INFO( "X11/Display:     Visual (%02lu) 0x%08lx, 0x%08lx, 0x%08lx, %d bits, %d entries\n", visual->visualid,
                       visual->red_mask, visual->green_mask, visual->blue_mask, visual->bits_per_rgb, visual->map_entries );

               switch (depth->depth) {
                    case 24:
                         if (visual->red_mask   == 0xff0000 &&
                             visual->green_mask == 0x00ff00 &&
                             visual->blue_mask  == 0x0000ff) {
                              dfb_x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)] = visual;
                              dfb_x11->visuals[DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]  = visual;
                         }
                         break;
               }
          }
     }

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     void       *ret;
     CoreScreen *screen;

     D_ASSERT( dfb_x11 == NULL );

     fusion_arena_get_shared_field( dfb_core_arena( core ), "x11", &ret );

     dfb_x11 = ret;
     dfb_x11_core = core;

     dfb_surface_pool_join( core, dfb_x11->surface_pool, &x11SurfacePoolFuncs );

     screen = dfb_screens_register( NULL, NULL, &x11PrimaryScreenFuncs );

     dfb_layers_register( screen, NULL, &x11PrimaryLayerFuncs );

     *data = dfb_x11;

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     D_ASSERT( dfb_x11 != NULL );

     dfb_surface_pool_destroy( dfb_x11->surface_pool );

     fusion_call_destroy( &dfb_x11->call );

     fusion_skirmish_prevail( &dfb_x11->lock );
     if (dfb_x11->xw)
         dfb_x11_close_window( dfb_x11->xw );

     if (dfb_x11->display)
         XCloseDisplay( dfb_x11->display );

     fusion_skirmish_destroy( &dfb_x11->lock );

     SHFREE( dfb_core_shmpool(dfb_x11_core), dfb_x11 );
     dfb_x11 = NULL;
     dfb_x11_core = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     D_ASSERT( dfb_x11 != NULL );

     dfb_x11 = NULL;
     dfb_x11_core = NULL;

     return DFB_OK;
}

static DFBResult
system_suspend()
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
system_resume()
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
system_get_accelerator()
{
     return -1;
}

static VideoMode *
system_get_modes()
{
     return modes;
}

static VideoMode *
system_get_current_mode()
{
     return &modes[0];   /* FIXME */
}

static DFBResult
system_thread_init()
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
system_videoram_length()
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
system_auxram_length()
{
     return 0;
}

static void
system_get_busid( int *ret_bus, int *ret_dev, int *ret_func )
{
     return;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     return;
}

static FusionCallHandlerResult
call_handler( int           caller,
              int           call_arg,
              void         *call_ptr,
              void         *ctx,
              unsigned int  serial,
              int          *ret_val )
{
     switch (call_arg) {
          case X11_SET_VIDEO_MODE:
               *ret_val = dfb_x11_set_video_mode_handler( call_ptr );
               break;

          case X11_UPDATE_SCREEN:
               *ret_val = dfb_x11_update_screen_handler( call_ptr );
               break;

          case X11_SET_PALETTE:
               *ret_val = dfb_x11_set_palette_handler( call_ptr );
               break;

          case X11_IMAGE_INIT:
               *ret_val = dfb_x11_image_init_handler( call_ptr );
               break;

          case X11_IMAGE_DESTROY:
               *ret_val = dfb_x11_image_destroy_handler( call_ptr );
               break;

          default:
               D_BUG( "unknown call" );
               *ret_val = DFB_BUG;
               break;
     }

     return FCHR_RETURN;
}

