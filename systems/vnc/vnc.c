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

#include <rfb/rfb.h>
#include "vnc.h"
#include "primary.h"

#include <core/core_system.h>


DFB_CORE_SYSTEM( vnc )


DFBVNC *dfb_vnc = NULL;
CoreDFB *dfb_vnc_core = NULL;

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_VNC;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "VNC" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     CoreScreen *core_screen;

     D_ASSERT( dfb_vnc == NULL );

     dfb_vnc = (DFBVNC*) SHCALLOC( dfb_core_shmpool(core), 1, sizeof(DFBVNC) );
     if (!dfb_vnc) {
          D_ERROR( "DirectFB/VNC: Couldn't allocate shared memory!\n" );
          return DFB_NOSYSTEMMEMORY;
     }

     dfb_vnc_core = core;

     fusion_skirmish_init( &dfb_vnc->lock, "VNC System", dfb_core_world(core) );

     fusion_call_init( &dfb_vnc->call, dfb_vnc_call_handler, NULL, dfb_core_world(core) );

     core_screen = dfb_screens_register( NULL, NULL, &vncPrimaryScreenFuncs );

     dfb_layers_register( core_screen, NULL, &vncPrimaryLayerFuncs );

     fusion_arena_add_shared_field( dfb_core_arena( core ), "vnc", dfb_vnc );

     *data = dfb_vnc;

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     void       *ret;
     CoreScreen *core_screen;

     D_ASSERT( dfb_vnc == NULL );

     fusion_arena_get_shared_field( dfb_core_arena( core ), "vnc", &ret );

     dfb_vnc = ret;
     dfb_vnc_core = core;

     core_screen = dfb_screens_register( NULL, NULL, &vncPrimaryScreenFuncs );

     dfb_layers_register( core_screen, NULL, &vncPrimaryLayerFuncs );

     *data = dfb_vnc;

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     D_ASSERT( dfb_vnc != NULL );

     fusion_call_destroy( &dfb_vnc->call );

     fusion_skirmish_prevail( &dfb_vnc->lock );

     fusion_skirmish_destroy( &dfb_vnc->lock );

     SHFREE( dfb_core_shmpool(dfb_vnc_core), dfb_vnc );
     dfb_vnc = NULL;
     dfb_vnc_core = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     D_ASSERT( dfb_vnc != NULL );

     dfb_vnc = NULL;
     dfb_vnc_core = NULL;

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

