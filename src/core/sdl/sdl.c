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

#include <stdio.h>

#include <directfb.h>

#include <core/fusion/arena.h>
#include <core/fusion/shmalloc.h>
                                   
#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <SDL.h>

#include "sdl.h"
#include "primary.h"

#include <core/core_system.h>

DFB_CORE_SYSTEM( sdl )


DFBSDL *dfb_sdl = NULL;


static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_SDL;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "SDL" );
}

static DFBResult
system_initialize()
{
     DFB_ASSERT( dfb_sdl == NULL );

     dfb_sdl = (DFBSDL*) shcalloc( 1, sizeof(DFBSDL) );
     if (!dfb_sdl) {
          ERRORMSG( "DirectFB/SDL: Couldn't allocate shared memory!\n" );
          return DFB_NOSYSTEMMEMORY;
     }
     
     /* Initialize SDL */
     if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
          ERRORMSG( "DirectFB/SDL: Couldn't initialize SDL: %s\n",
                    SDL_GetError() );
          
          shfree( dfb_sdl );
          dfb_sdl = NULL;
          
          return DFB_INIT;
     }
     
     fusion_skirmish_init( &dfb_sdl->lock );
     
     fusion_call_init( &dfb_sdl->call, dfb_sdl_call_handler, NULL );
     
     dfb_layers_register( NULL, NULL, &sdlPrimaryLayerFuncs );

#ifndef FUSION_FAKE
     fusion_arena_add_shared_field( dfb_core->arena, "sdl", dfb_sdl );
#endif
     
     return DFB_OK;
}

static DFBResult
system_join()
{
#ifndef FUSION_FAKE
     void *ret;

     DFB_ASSERT( dfb_sdl == NULL );

     fusion_arena_get_shared_field( dfb_core->arena, "sdl", &ret );

     dfb_sdl = ret;
     
     dfb_layers_register( NULL, NULL, &sdlPrimaryLayerFuncs );

#endif     
     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     DFB_ASSERT( dfb_sdl != NULL );
     
     fusion_call_destroy( &dfb_sdl->call );
     
     fusion_skirmish_prevail( &dfb_sdl->lock );
     
     SDL_Quit();

     fusion_skirmish_destroy( &dfb_sdl->lock );
     
     shfree( dfb_sdl );
     dfb_sdl = NULL;
     
     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
#ifndef FUSION_FAKE
     DFB_ASSERT( dfb_sdl != NULL );

     dfb_sdl = NULL;

#endif     
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

