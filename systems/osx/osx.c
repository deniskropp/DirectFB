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

#include <Carbon/Carbon.h>

#include "osx.h"
#include "primary.h"

#include <core/core_system.h>

DFB_CORE_SYSTEM( osx )


DFBOSX *dfb_osx = NULL;


static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_OSX;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "OSX" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     char       *driver;
     CoreScreen *screen;

     D_ASSERT( dfb_osx == NULL );

     dfb_osx = (DFBOSX*) SHCALLOC( 1, sizeof(DFBOSX) );
     if (!dfb_osx) {
          D_ERROR( "DirectFB/OSX: Couldn't allocate shared memory!\n" );
          return DFB_NOSYSTEMMEMORY;
     }

     /* Initialize OSX */
     fusion_skirmish_init( &dfb_osx->lock );

     fusion_call_init( &dfb_osx->call, dfb_osx_call_handler, NULL );

     screen = dfb_screens_register( NULL, NULL, &osxPrimaryScreenFuncs );

     dfb_layers_register( screen, NULL, &osxPrimaryLayerFuncs );

     fusion_arena_add_shared_field( dfb_core_arena( core ), "OSX", dfb_osx );

     *data = dfb_osx;

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     void       *ret;
     CoreScreen *screen;

     D_ASSERT( dfb_osx == NULL );

     fusion_arena_get_shared_field( dfb_core_arena( core ), "OSX", &ret );

     dfb_osx = ret;

     screen = dfb_screens_register( NULL, NULL, &osxPrimaryScreenFuncs );

     dfb_layers_register( screen, NULL, &osxPrimaryLayerFuncs );

     *data = dfb_osx;

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     D_ASSERT( dfb_osx != NULL );

     fusion_call_destroy( &dfb_osx->call );

     fusion_skirmish_prevail( &dfb_osx->lock );

     fusion_skirmish_destroy( &dfb_osx->lock );

     SHFREE( dfb_osx );
     dfb_osx = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     D_ASSERT( dfb_osx != NULL );

     dfb_osx = NULL;

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
system_input_filter( InputDevice   *device,
                     DFBInputEvent *event )
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

