/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <directfb.h>

#include <fusion/arena.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/messages.h>
#include <direct/thread.h>

#include <SDL.h>

#include "sdl.h"
#include "primary.h"

#include <core/core_system.h>

DFB_CORE_SYSTEM( sdl )


DFBSDL  *dfb_sdl      = NULL;
CoreDFB *dfb_sdl_core = NULL;

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_SDL;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "SDL" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     char       *driver;
     CoreScreen *screen;

     D_ASSERT( dfb_sdl == NULL );

     dfb_sdl = (DFBSDL*) SHCALLOC( dfb_core_shmpool(core), 1, sizeof(DFBSDL) );
     if (!dfb_sdl) {
          D_ERROR( "DirectFB/SDL: Couldn't allocate shared memory!\n" );
          return D_OOSHM();
     }

     driver = getenv( "SDL_VIDEODRIVER" );
     if (driver && !strcasecmp( driver, "directfb" )) {
          D_INFO( "DirectFB/SDL: SDL_VIDEODRIVER is 'directfb', unsetting it.\n" );
          unsetenv( "SDL_VIDEODRIVER" );
     }

     /* Initialize SDL */
     if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
          D_ERROR( "DirectFB/SDL: Couldn't initialize SDL: %s\n", SDL_GetError() );

          SHFREE( dfb_core_shmpool(core), dfb_sdl );
          dfb_sdl = NULL;

          return DFB_INIT;
     }

     dfb_sdl_core = core;

     fusion_skirmish_init( &dfb_sdl->lock, "SDL System", dfb_core_world(core) );

     fusion_call_init( &dfb_sdl->call, dfb_sdl_call_handler, NULL, dfb_core_world(core) );

     screen = dfb_screens_register( NULL, NULL, &sdlPrimaryScreenFuncs );

     dfb_layers_register( screen, NULL, &sdlPrimaryLayerFuncs );

     fusion_arena_add_shared_field( dfb_core_arena( core ), "sdl", dfb_sdl );

     *data = dfb_sdl;

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     void       *ret;
     CoreScreen *screen;

     D_ASSERT( dfb_sdl == NULL );

     fusion_arena_get_shared_field( dfb_core_arena( core ), "sdl", &ret );

     dfb_sdl = ret;
     dfb_sdl_core = core;

     screen = dfb_screens_register( NULL, NULL, &sdlPrimaryScreenFuncs );

     dfb_layers_register( screen, NULL, &sdlPrimaryLayerFuncs );

     *data = dfb_sdl;

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     D_ASSERT( dfb_sdl != NULL );

     /* Stop update thread. */
     if (dfb_sdl->update.thread) {
          if (!emergency) {
               dfb_sdl->update.quit = true;

               pthread_cond_signal( &dfb_sdl->update.cond );

               direct_thread_join( dfb_sdl->update.thread );
          }

          direct_thread_destroy( dfb_sdl->update.thread );
     }


     fusion_call_destroy( &dfb_sdl->call );

     fusion_skirmish_prevail( &dfb_sdl->lock );

     SDL_Quit();

     fusion_skirmish_destroy( &dfb_sdl->lock );

     SHFREE( dfb_core_shmpool(dfb_sdl_core), dfb_sdl );
     dfb_sdl = NULL;
     dfb_sdl_core = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     D_ASSERT( dfb_sdl != NULL );

     dfb_sdl = NULL;
     dfb_sdl_core = NULL;

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
     return NULL;
}

static VideoMode *
system_get_current_mode()
{
     return NULL;
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

