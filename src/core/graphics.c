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

#include <config.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/graphics.h>
#include <core/graphics_internal.h>
#include <core/graphics_state.h>
#include <core/graphics_state_internal.h>
#include <core/state.h>


D_DEBUG_DOMAIN( Core_Graphics, "Core/Graphics", "DirectFB Core Graphics" );

/*********************************************************************************************************************/

DirectResult
dfb_graphics_call( CoreDFB             *core,
                   CoreGraphicsCall     call,
                   void                *arg,
                   size_t               len,
                   FusionCallExecFlags  flags,
                   int                 *ret_val )
{
     return fusion_call_execute2( &core->shared->graphics_call, flags, call, arg, len, ret_val );
}

/*********************************************************************************************************************/

static u32
CoreGraphics_Dispatch_CreateState( DFBGraphicsCore *data )
{
     CoreGraphicsState *state;

     D_DEBUG_AT( Core_Graphics, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBGraphicsCore );

     state = D_CALLOC( 1, sizeof(CoreGraphicsState) );
     if (!state)
          return D_OOM();

     state->core = data->core;

     dfb_state_init( &state->state, data->core );

     fusion_call_init( &state->call, CoreGraphicsState_Dispatch, state, dfb_core_world(data->core) );

     D_MAGIC_SET( state, CoreGraphicsState );

     return state->call.call_id;
}

FusionCallHandlerResult
CoreGraphics_Dispatch( int           caller,   /* fusion id of the caller */
                       int           call_arg, /* optional call parameter */
                       void         *call_ptr, /* optional call parameter */
                       void         *ctx,      /* optional handler context */
                       unsigned int  serial,
                       int          *ret_val )
{
     switch (call_arg) {
          case CORE_GRAPHICS_CREATE_STATE:
               D_DEBUG_AT( Core_Graphics, "=-> CORE_GRAPHICS_CREATE_STATE\n" );

               *ret_val = CoreGraphics_Dispatch_CreateState( ctx );
               break;

          default:
               D_BUG( "invalid call arg %d", call_arg );
               *ret_val = DFB_INVARG;
     }

     return FCHR_RETURN;
}

