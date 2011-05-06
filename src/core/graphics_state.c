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
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/graphics_internal.h>
#include <core/graphics_state.h>
#include <core/graphics_state_internal.h>
#include <core/state.h>
#include <core/surface.h>


D_DEBUG_DOMAIN( Core_GraphicsState, "Core/Graphics/State", "DirectFB Core Graphics State" );

/**********************************************************************************************************************/

DFBResult
CoreGraphicsStateClient_Init( CoreGraphicsStateClient *client,
                              CoreDFB                 *core )
{
     DFBResult ret;
     int       val;

     D_ASSERT( client != NULL );
     D_MAGIC_ASSERT( core, CoreDFB );

     client->core = core;

     ret = dfb_graphics_call( core, CORE_GRAPHICS_CREATE_STATE, NULL, 0, FCEF_NONE, &val );
     if (ret) {
          D_DERROR( ret, "%s: dfb_graphics_call( CORE_GRAPHICS_CREATE_STATE ) failed!\n", __FUNCTION__ );
          return ret;
     }

     if (!val) {
          D_DERROR( ret, "%s: dfb_graphics_call( CORE_GRAPHICS_CREATE_STATE ) did not return an ID!\n", __FUNCTION__ );
          return DFB_FAILURE;
     }

     fusion_call_init_from( &client->call, val, dfb_core_world(core) );

     D_MAGIC_SET( client, CoreGraphicsStateClient );

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_SetState( CoreGraphicsStateClient *client,
                                  CardState               *state,
                                  StateModificationFlags   flags )
{
     DFBResult ret;

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_MAGIC_ASSERT( state, CardState );

     if (flags & SMF_DESTINATION) {
          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_SET_DESTINATION,
                                      &state->destination->object.id, sizeof(state->destination->object.id), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_SET_DESTINATION ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     if (flags & SMF_CLIP) {
          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_SET_CLIP,
                                      &state->clip, sizeof(state->clip), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_SET_CLIP ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     if (flags & SMF_COLOR) {
          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_SET_COLOR,
                                      &state->color, sizeof(state->color), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_SET_COLOR ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     return DFB_OK;
}

// TEST
DFBResult
CoreGraphicsStateClient_FillRectangle( CoreGraphicsStateClient *client,
                                       const DFBRectangle      *rect )
{
     DFBResult ret;

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     DFB_RECTANGLE_ASSERT( rect );

     ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                 CORE_GRAPHICS_STATE_FILL_RECTANGLE,
                                 (void*) rect, sizeof(*rect), NULL );
     if (ret) {
          D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_FILL_RECTANGLE ) failed!\n", __FUNCTION__ );
          return ret;
     }

     return DFB_OK;
}

// TEST
DFBResult
CoreGraphicsStateClient_FillRectangles( CoreGraphicsStateClient *client,
                                        const DFBRectangle      *rects,
                                        unsigned int             num )
{
     DFBResult                        ret;
     unsigned int                     size;
     CoreGraphicsStateFillRectangles *fill_rects;

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );

     size = sizeof(CoreGraphicsStateFillRectangles) + num * sizeof(DFBRectangle);

     fill_rects = alloca( size );

     fill_rects->num = num;

     direct_memcpy( fill_rects + 1, rects, num * sizeof(DFBRectangle) );

     ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                 CORE_GRAPHICS_STATE_FILL_RECTANGLES,
                                 fill_rects, size, NULL );
     if (ret) {
          D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_FILL_RECTANGLES ) failed!\n", __FUNCTION__ );
          return ret;
     }

     return DFB_OK;
}

/*********************************************************************************************************************/
/*********************************************************************************************************************/

static DFBResult
CoreGraphicsState_Dispatch_SetDestination( CoreGraphicsState               *state,
                                           CoreGraphicsStateSetDestination *set_destination )
{
     DFBResult    ret;
     CoreSurface *surface;

     D_DEBUG_AT( Core_GraphicsState, "%s( %p )\n", __FUNCTION__, state );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     ret = dfb_core_get_surface( state->core, set_destination->object_id, &surface );
     if (ret)
          return ret;

     ret = dfb_state_set_destination( &state->state, surface );

     dfb_surface_unref( surface );

     return ret;
}

static DFBResult
CoreGraphicsState_Dispatch_SetClip( CoreGraphicsState        *state,
                                    CoreGraphicsStateSetClip *set_clip )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p )\n", __FUNCTION__, state );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_state_set_clip( &state->state, &set_clip->clip );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_SetColor( CoreGraphicsState         *state,
                                     CoreGraphicsStateSetColor *set_color )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p )\n", __FUNCTION__, state );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_state_set_color( &state->state, &set_color->color );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_FillRectangle( CoreGraphicsState              *state,
                                          CoreGraphicsStateFillRectangle *fill_rectangle )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p )\n", __FUNCTION__, state );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_gfxcard_fillrectangles( &fill_rectangle->rect, 1, &state->state );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_FillRectangles( CoreGraphicsState               *state,
                                           CoreGraphicsStateFillRectangles *fill_rectangles )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p )\n", __FUNCTION__, state );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_gfxcard_fillrectangles( fill_rectangles + 1, fill_rectangles->num, &state->state );

     return DFB_OK;
}

/**********************************************************************************************************************/

FusionCallHandlerResult
CoreGraphicsState_Dispatch( int           caller,   /* fusion id of the caller */
                            int           call_arg, /* optional call parameter */
                            void         *call_ptr, /* optional call parameter */
                            void         *ctx,      /* optional handler context */
                            unsigned int  serial,
                            int          *ret_val )
{
     CoreGraphicsState *state = ctx;

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     switch (call_arg) {
          case CORE_GRAPHICS_STATE_SET_DESTINATION:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_DESTINATION\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetDestination( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_SET_CLIP:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_CLIP\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetClip( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_SET_COLOR:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_COLOR\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetColor( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_FILL_RECTANGLE:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_FILL_RECTANGLE\n" );

               *ret_val = CoreGraphicsState_Dispatch_FillRectangle( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_FILL_RECTANGLES:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_FILL_RECTANGLES\n" );

               *ret_val = CoreGraphicsState_Dispatch_FillRectangles( ctx, call_ptr );
               break;

          default:
               D_BUG( "invalid call arg %d", call_arg );
               *ret_val = DFB_INVARG;
     }

     return FCHR_RETURN;
}

