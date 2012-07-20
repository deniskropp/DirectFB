/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

#include <directfb.h>

#include <fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/CoreGraphicsState.h>

#include <core/core.h>
#include <core/state.h>

#include <misc/util.h>


D_DEBUG_DOMAIN( Core_GraphicsState, "Core/GraphicsState", "DirectFB Graphics State" );

/**********************************************************************************************************************/

extern void CoreGraphicsState_Destruct( CoreGraphicsState *state );

static void
state_destructor( FusionObject *object, bool zombie, void *ctx )
{
     CoreGraphicsState *state = (CoreGraphicsState*) object;

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     D_DEBUG_AT( Core_GraphicsState, "destroying %p%s\n", state, zombie ? " (ZOMBIE)" : "");

     CoreGraphicsState_Destruct( state );

     dfb_state_set_destination( &state->state, NULL );
     dfb_state_set_source( &state->state, NULL );
     dfb_state_set_source2( &state->state, NULL );
     dfb_state_set_source_mask( &state->state, NULL );

     dfb_state_destroy( &state->state );

     CoreGraphicsState_Deinit_Dispatch( &state->call );

     D_MAGIC_CLEAR( state );

     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_graphics_state_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "GraphicsState Pool",
                                       sizeof(CoreGraphicsState),
                                       sizeof(CoreGraphicsStateNotification),
                                       state_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_graphics_state_create( CoreDFB            *core,
                           CoreGraphicsState **ret_state )
{
     CoreGraphicsState *state;

     D_DEBUG_AT( Core_GraphicsState, "%s()\n", __FUNCTION__ );

     D_ASSERT( ret_state );

     state = dfb_core_create_graphics_state( core );
     if (!state)
          return DFB_FUSION;

     dfb_state_init( &state->state, core );

     CoreGraphicsState_Init_Dispatch( core, state, &state->call );

     D_MAGIC_SET( state, CoreGraphicsState );

     /* activate object */
     fusion_object_activate( &state->object );

     /* return the new state */
     *ret_state = state;

     D_DEBUG_AT( Core_GraphicsState, "  -> %p\n", state );

     return DFB_OK;
}

