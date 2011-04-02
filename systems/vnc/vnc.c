/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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
#include <core/surface.h>
#include <core/system.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <direct/messages.h>

#include <rfb/rfb.h>
#include "vnc.h"
#include "primary.h"

#include <core/core_system.h>


DFB_CORE_SYSTEM( vnc )


static DFBVNC *dfb_vnc;

static FusionCallHandlerResult
VNC_Dispatch( int           caller,
              int           call_arg,
              void         *call_ptr,
              void         *ctx,
              unsigned int  serial,
              int          *ret_val );

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_VNC;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "VNC" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     D_ASSERT( dfb_vnc == NULL );

     dfb_vnc = (DFBVNC*) D_CALLOC( 1, sizeof(DFBVNC) );
     if (!dfb_vnc)
          return D_OOM();

     dfb_vnc->core = core;

     dfb_vnc->shared = (DFBVNCShared*) SHCALLOC( dfb_core_shmpool(core), 1, sizeof(DFBVNCShared) );
     if (!dfb_vnc->shared) {
          D_FREE( dfb_vnc );
          return D_OOSHM();
     }

     dfb_vnc->shared->screen_size.w = dfb_config->mode.width  ? dfb_config->mode.width  : 1280;
     dfb_vnc->shared->screen_size.h = dfb_config->mode.height ? dfb_config->mode.height :  720;

     fusion_call_init( &dfb_vnc->shared->call, VNC_Dispatch, dfb_vnc, dfb_core_world(core) );

     dfb_vnc->screen = dfb_screens_register( NULL, dfb_vnc, vncPrimaryScreenFuncs );

     dfb_vnc->layer = dfb_layers_register( dfb_vnc->screen, dfb_vnc, vncPrimaryLayerFuncs );

     fusion_arena_add_shared_field( dfb_core_arena( core ), "vnc", dfb_vnc->shared );

     *data = dfb_vnc;

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     void *ret;

     D_ASSERT( dfb_vnc == NULL );

     fusion_arena_get_shared_field( dfb_core_arena( core ), "vnc", &ret );

     dfb_vnc = (DFBVNC*) D_CALLOC( 1, sizeof(DFBVNC) );
     if (!dfb_vnc)
          return D_OOM();

     dfb_vnc->core = core;

     dfb_vnc->shared = ret;

     dfb_vnc->screen = dfb_screens_register( NULL, dfb_vnc, vncPrimaryScreenFuncs );

     dfb_vnc->layer = dfb_layers_register( dfb_vnc->screen, dfb_vnc, vncPrimaryLayerFuncs );

     *data = dfb_vnc;

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     D_ASSERT( dfb_vnc != NULL );

     fusion_call_destroy( &dfb_vnc->shared->call );

     SHFREE( dfb_core_shmpool(dfb_vnc->core), dfb_vnc->shared );

     D_FREE( dfb_vnc );
     dfb_vnc = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     D_ASSERT( dfb_vnc != NULL );

     D_FREE( dfb_vnc );
     dfb_vnc = NULL;

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


static int
system_get_accelerator( void )
{
     return -1;
}

static VideoMode *
system_get_modes( void )
{
     return NULL;
}

static VideoMode *
system_get_current_mode( void )
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
     return;
}

static int
system_surface_data_size( void )
{
     /* Return zero because shared surface data is unneeded. */
     return 0;
}

static void
system_surface_data_init( CoreSurface *surface, void *data )
{
     /* Ignore since unneeded. */
     return;
}

static void
system_surface_data_destroy( CoreSurface *surface, void *data )
{
     /* Ignore since unneeded. */
     return;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     return;
}

/**********************************************************************************************************************/

static int
VNC_Dispatch_MarkRectAsModified( DFBVNC                   *vnc,
                                 DFBVNCMarkRectAsModified *mark )
{
     rfbMarkRectAsModified( vnc->rfb_screen, mark->region.x1, mark->region.y1, mark->region.x2 + 1, mark->region.y2 + 1 );

     return 0;
}

static FusionCallHandlerResult
VNC_Dispatch( int           caller,
              int           call_arg,
              void         *call_ptr,
              void         *ctx,
              unsigned int  serial,
              int          *ret_val )
{
     DFBVNC *vnc = ctx;

     switch (call_arg) {
          case VNC_MARK_RECT_AS_MODIFIED:
               *ret_val = VNC_Dispatch_MarkRectAsModified( vnc, call_ptr );
               break;

          default:
               D_BUG( "unknown call" );
               *ret_val = DFB_BUG;
               break;
     }

     return FCHR_RETURN;
}

