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
#include <core/state.h>
#include <core/surface.h>

#include <core/CoreGraphics_internal.h>
#include <core/CoreGraphicsState.h>
#include <core/CoreGraphicsState_internal.h>


D_DEBUG_DOMAIN( Core_GraphicsState, "Core/Graphics/State", "DirectFB Core Graphics State" );

/**********************************************************************************************************************/

DFBResult
CoreGraphicsStateClient_Init( CoreGraphicsStateClient *client,
                              CardState               *state )
{
     DFBResult ret;
     int       val;

     D_ASSERT( client != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT( state->core, CoreDFB );

     client->core  = state->core;
     client->state = state;

     ret = dfb_graphics_call( state->core, CORE_GRAPHICS_CREATE_STATE, NULL, 0, FCEF_NONE, &val );
     if (ret) {
          D_DERROR( ret, "%s: dfb_graphics_call( CORE_GRAPHICS_CREATE_STATE ) failed!\n", __FUNCTION__ );
          return ret;
     }

     if (!val) {
          D_DERROR( ret, "%s: dfb_graphics_call( CORE_GRAPHICS_CREATE_STATE ) did not return an ID!\n", __FUNCTION__ );
          return DFB_FAILURE;
     }

     fusion_call_init_from( &client->call, val, dfb_core_world(state->core) );

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

     if (flags & SMF_SOURCE) {
          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_SET_SOURCE,
                                      &state->source->object.id, sizeof(state->source->object.id), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_SET_SOURCE ) failed!\n", __FUNCTION__ );
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

     if (flags & SMF_DRAWING_FLAGS) {
          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_SET_DRAWINGFLAGS,
                                      &state->drawingflags, sizeof(state->drawingflags), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_SET_DRAWINGFLAGS ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     if (flags & SMF_BLITTING_FLAGS) {
          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_SET_BLITTINGFLAGS,
                                      &state->blittingflags, sizeof(state->blittingflags), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_SET_BLITTINGFLAGS ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     if (flags & SMF_SRC_BLEND) {
          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_SET_SRC_BLEND,
                                      &state->src_blend, sizeof(state->src_blend), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_SET_SRC_BLEND ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     if (flags & SMF_DST_BLEND) {
          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_SET_DST_BLEND,
                                      &state->dst_blend, sizeof(state->dst_blend), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_SET_DST_BLEND ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     if (flags & SMF_SRC_COLORKEY) {
          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_SET_SRC_COLORKEY,
                                      &state->src_colorkey, sizeof(state->src_colorkey), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_SET_SRC_COLORKEY ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     if (flags & SMF_DST_COLORKEY) {
          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_SET_DST_COLORKEY,
                                      &state->dst_colorkey, sizeof(state->dst_colorkey), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_SET_DST_COLORKEY ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_Update( CoreGraphicsStateClient *client,
                                DFBAccelerationMask      accel,
                                CardState               *state )
{
     DFBResult              ret;
     StateModificationFlags flags = SMF_DESTINATION | SMF_CLIP;

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_MAGIC_ASSERT( state, CardState );

     if (DFB_DRAWING_FUNCTION( accel )) {
          flags |= SMF_DRAWING_FLAGS | SMF_COLOR;

          if (state->drawingflags & DSDRAW_BLEND)
               flags |= SMF_SRC_BLEND | SMF_DST_BLEND;

          if (state->drawingflags & DSDRAW_DST_COLORKEY)
               flags |= SMF_DST_COLORKEY;

          // FIXME: incomplete
     }
     else {
          flags |= SMF_BLITTING_FLAGS | SMF_SOURCE;

          if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                      DSBLIT_COLORIZE |
                                      DSBLIT_SRC_PREMULTCOLOR))
               flags |= SMF_COLOR;

          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                      DSBLIT_BLEND_COLORALPHA))
               flags |= SMF_SRC_BLEND | SMF_DST_BLEND;

          if (state->blittingflags & DSBLIT_SRC_COLORKEY)
               flags |= SMF_SRC_COLORKEY;

          if (state->blittingflags & DSBLIT_DST_COLORKEY)
               flags |= SMF_DST_COLORKEY;

          // FIXME: incomplete
     }

     ret = CoreGraphicsStateClient_SetState( client, state, state->modified & flags );
     if (ret)
          return ret;

     state->modified &= ~flags;

     return DFB_OK;
}

// TEST
DFBResult
CoreGraphicsStateClient_FillRectangle( CoreGraphicsStateClient *client,
                                       const DFBRectangle      *rect )
{
     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     DFB_RECTANGLE_ASSERT( rect );

     if (dfb_core_is_master( client->core )) {
          dfb_gfxcard_fillrectangles( rect, 1, client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_FILLRECTANGLE, client->state );

          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_FILL_RECTANGLE,
                                      (void*) rect, sizeof(*rect), NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_FILL_RECTANGLE ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     return DFB_OK;
}

// TEST
DFBResult
CoreGraphicsStateClient_FillRectangles( CoreGraphicsStateClient *client,
                                        const DFBRectangle      *rects,
                                        unsigned int             num )
{
     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );

     if (dfb_core_is_master( client->core )) {
          dfb_gfxcard_fillrectangles( rects, num, client->state );
     }
     else {
          DFBResult                        ret;
          unsigned int                     size;
          CoreGraphicsStateFillRectangles *fill_rects;

          size = sizeof(CoreGraphicsStateFillRectangles) + num * sizeof(DFBRectangle);

          fill_rects = alloca( size );

          fill_rects->num = num;

          direct_memcpy( fill_rects + 1, rects, num * sizeof(DFBRectangle) );

          CoreGraphicsStateClient_Update( client, DFXL_FILLRECTANGLE, client->state );

          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_FILL_RECTANGLES,
                                      fill_rects, size, NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_FILL_RECTANGLES ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     return DFB_OK;
}

// TEST
DFBResult
CoreGraphicsStateClient_Blit( CoreGraphicsStateClient *client,
                              const DFBRectangle      *rects,
                              const DFBPoint          *points,
                              unsigned int             num )
{
     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );
     D_ASSERT( points != NULL );

     if (dfb_core_is_master( client->core )) {
          dfb_gfxcard_batchblit( rects, points, num, client->state );
     }
     else {
          DFBResult              ret;
          unsigned int           size;
          CoreGraphicsStateBlit *blit;

          size = sizeof(CoreGraphicsStateBlit) + num * sizeof(DFBRectangle) + num * sizeof(DFBPoint);

          blit = alloca( size );

          blit->num = num;

          direct_memcpy( blit + 1, rects, num * sizeof(DFBRectangle) );
          direct_memcpy( (char*)(blit + 1) + num * sizeof(DFBRectangle), points, num * sizeof(DFBPoint) );

          CoreGraphicsStateClient_Update( client, DFXL_BLIT, client->state );

          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_BLIT,
                                      blit, size, NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_BLIT ) failed!\n", __FUNCTION__ );
               return ret;
          }
     }

     return DFB_OK;
}

// TEST
DFBResult
CoreGraphicsStateClient_StretchBlit( CoreGraphicsStateClient *client,
                                     const DFBRectangle      *srects,
                                     const DFBRectangle      *drects,
                                     unsigned int             num )
{
     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( srects != NULL );
     D_ASSERT( drects != NULL );

     if (num == 0)
          return DFB_OK;

     if (num != 1)
          D_UNIMPLEMENTED();

     if (dfb_core_is_master( client->core )) {
          dfb_gfxcard_stretchblit( srects, drects, client->state );
     }
     else {
          DFBResult                     ret;
          unsigned int                  size;
          CoreGraphicsStateStretchBlit *stretch_blit;

          size = sizeof(CoreGraphicsStateStretchBlit) + num * 2 * sizeof(DFBRectangle);

          stretch_blit = alloca( size );

          stretch_blit->num = num;

          direct_memcpy( stretch_blit + 1, srects, num * sizeof(DFBRectangle) );
          direct_memcpy( (char*)(stretch_blit + 1) + num * sizeof(DFBRectangle), drects, num * sizeof(DFBRectangle) );

          CoreGraphicsStateClient_Update( client, DFXL_STRETCHBLIT, client->state );

          ret = fusion_call_execute2( &client->call, FCEF_ONEWAY,
                                      CORE_GRAPHICS_STATE_STRETCH_BLIT,
                                      stretch_blit, size, NULL );
          if (ret) {
               D_DERROR( ret, "%s: fusion_call_execute2( CORE_GRAPHICS_STATE_STRETCH_BLIT ) failed!\n", __FUNCTION__ );
               return ret;
          }
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
CoreGraphicsState_Dispatch_SetSource( CoreGraphicsState          *state,
                                      CoreGraphicsStateSetSource *set_source )
{
     DFBResult    ret;
     CoreSurface *surface;

     D_DEBUG_AT( Core_GraphicsState, "%s( %p )\n", __FUNCTION__, state );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     ret = dfb_core_get_surface( state->core, set_source->object_id, &surface );
     if (ret)
          return ret;

     ret = dfb_state_set_source( &state->state, surface );

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
     D_DEBUG_AT( Core_GraphicsState, "%s( %p, %02x %02x %02x %02x )\n", __FUNCTION__,
                 state, set_color->color.a, set_color->color.r, set_color->color.g, set_color->color.b );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_state_set_color( &state->state, &set_color->color );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_SetDrawingFlags( CoreGraphicsState                *state,
                                            CoreGraphicsStateSetDrawingFlags *set_drawingflags )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p, 0x%08x )\n", __FUNCTION__, state, set_drawingflags->flags );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_state_set_drawing_flags( &state->state, set_drawingflags->flags );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_SetBlittingFlags( CoreGraphicsState                 *state,
                                             CoreGraphicsStateSetBlittingFlags *set_blittingflags )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p, 0x%08x )\n", __FUNCTION__, state, set_blittingflags->flags );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_state_set_blitting_flags( &state->state, set_blittingflags->flags );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_SetSrcBlend( CoreGraphicsState            *state,
                                        CoreGraphicsStateSetSrcBlend *set_src_blend )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p, %d )\n", __FUNCTION__, state, set_src_blend->function );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_state_set_src_blend( &state->state, set_src_blend->function );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_SetDstBlend( CoreGraphicsState            *state,
                                        CoreGraphicsStateSetDstBlend *set_dst_blend )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p, %d )\n", __FUNCTION__, state, set_dst_blend->function );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_state_set_dst_blend( &state->state, set_dst_blend->function );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_SetSrcColorkey( CoreGraphicsState               *state,
                                           CoreGraphicsStateSetSrcColorkey *set_src_colorkey )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p, %06x )\n", __FUNCTION__, state, set_src_colorkey->key );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_state_set_src_colorkey( &state->state, set_src_colorkey->key );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_SetDstColorkey( CoreGraphicsState               *state,
                                           CoreGraphicsStateSetDstColorkey *set_dst_colorkey )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p, %06x )\n", __FUNCTION__, state, set_dst_colorkey->key );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_state_set_dst_colorkey( &state->state, set_dst_colorkey->key );

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

     dfb_gfxcard_fillrectangles( (DFBRectangle*)( fill_rectangles + 1 ),
                                 fill_rectangles->num, &state->state );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_Blit( CoreGraphicsState     *state,
                                 CoreGraphicsStateBlit *blit )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p )\n", __FUNCTION__, state );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_gfxcard_batchblit( (DFBRectangle*)( blit + 1 ),
                            (DFBPoint*)( (char*)(blit + 1) + blit->num * sizeof(DFBRectangle) ),
                            blit->num, &state->state );

     return DFB_OK;
}

static DFBResult
CoreGraphicsState_Dispatch_StretchBlit( CoreGraphicsState            *state,
                                        CoreGraphicsStateStretchBlit *stretch_blit )
{
     D_DEBUG_AT( Core_GraphicsState, "%s( %p )\n", __FUNCTION__, state );

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     dfb_gfxcard_stretchblit( (DFBRectangle*)( stretch_blit + 1 ),
                              (DFBRectangle*)( (char*)(stretch_blit + 1) + stretch_blit->num * sizeof(DFBRectangle) ),
                              &state->state );

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

          case CORE_GRAPHICS_STATE_SET_SOURCE:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_SOURCE\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetSource( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_SET_CLIP:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_CLIP\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetClip( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_SET_COLOR:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_COLOR\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetColor( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_SET_DRAWINGFLAGS:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_DRAWINGFLAGS\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetDrawingFlags( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_SET_BLITTINGFLAGS:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_BLITTINGFLAGS\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetBlittingFlags( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_SET_SRC_BLEND:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_SRC_BLEND\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetSrcBlend( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_SET_DST_BLEND:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_DST_BLEND\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetDstBlend( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_SET_SRC_COLORKEY:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_SRC_COLORKEY\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetSrcColorkey( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_SET_DST_COLORKEY:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_SET_DST_COLORKEY\n" );

               *ret_val = CoreGraphicsState_Dispatch_SetDstColorkey( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_FILL_RECTANGLE:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_FILL_RECTANGLE\n" );

               *ret_val = CoreGraphicsState_Dispatch_FillRectangle( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_FILL_RECTANGLES:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_FILL_RECTANGLES\n" );

               *ret_val = CoreGraphicsState_Dispatch_FillRectangles( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_BLIT:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_BLIT\n" );

               *ret_val = CoreGraphicsState_Dispatch_Blit( ctx, call_ptr );
               break;

          case CORE_GRAPHICS_STATE_STRETCH_BLIT:
               D_DEBUG_AT( Core_GraphicsState, "=-> CORE_GRAPHICS_STATE_STRETCH_BLIT\n" );

               *ret_val = CoreGraphicsState_Dispatch_StretchBlit( ctx, call_ptr );
               break;

          default:
               D_BUG( "invalid call arg %d", call_arg );
               *ret_val = DFB_INVARG;
     }

     return FCHR_RETURN;
}

