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

#include <core/CoreDFB.h>
#include <core/CoreGraphicsState.h>
#include <core/CoreGraphicsStateClient.h>


D_DEBUG_DOMAIN( Core_GraphicsState, "Core/Graphics/State", "DirectFB Core Graphics State" );

/**********************************************************************************************************************/

DFBResult
CoreGraphicsStateClient_Init( CoreGraphicsStateClient *client,
                              CardState               *state )
{
     DFBResult ret;

     D_ASSERT( client != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT( state->core, CoreDFB );

     client->core  = state->core;
     client->state = state;

     ret = CoreDFB_CreateState( state->core, &client->gfx_state );
     if (ret)
          return ret;

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
          ret = CoreGraphicsState_SetDestination( client->gfx_state, state->destination );
          if (ret)
               return ret;
     }

     if (flags & SMF_SOURCE) {
          ret = CoreGraphicsState_SetSource( client->gfx_state, state->source );
          if (ret)
               return ret;
     }

     if (flags & SMF_CLIP) {
          ret = CoreGraphicsState_SetClip( client->gfx_state, &state->clip );
          if (ret)
               return ret;
     }

     if (flags & SMF_COLOR) {
          ret = CoreGraphicsState_SetColor( client->gfx_state, &state->color );
          if (ret)
               return ret;
     }

     if (flags & SMF_DRAWING_FLAGS) {
          ret = CoreGraphicsState_SetDrawingFlags( client->gfx_state, state->drawingflags );
          if (ret)
               return ret;
     }

     if (flags & SMF_BLITTING_FLAGS) {
          ret = CoreGraphicsState_SetBlittingFlags( client->gfx_state, state->blittingflags );
          if (ret)
               return ret;
     }

     if (flags & SMF_SRC_BLEND) {
          ret = CoreGraphicsState_SetSrcBlend( client->gfx_state, state->src_blend );
          if (ret)
               return ret;
     }

     if (flags & SMF_DST_BLEND) {
          ret = CoreGraphicsState_SetDstBlend( client->gfx_state, state->dst_blend );
          if (ret)
               return ret;
     }

     if (flags & SMF_SRC_COLORKEY) {
          ret = CoreGraphicsState_SetSrcColorKey( client->gfx_state, state->src_colorkey );
          if (ret)
               return ret;
     }

     if (flags & SMF_DST_COLORKEY) {
          ret = CoreGraphicsState_SetDstColorKey( client->gfx_state, state->dst_colorkey );
          if (ret)
               return ret;
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

DFBResult
CoreGraphicsStateClient_DrawRectangles( CoreGraphicsStateClient *client,
                                        const DFBRectangle      *rects,
                                        unsigned int             num )
{
     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );

     if (dfb_core_is_master( client->core )) {
          unsigned int i;

          for (i=0; i<num; i++)
               dfb_gfxcard_drawrectangle( &rects[i], client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_DRAWRECTANGLE, client->state );

          ret = CoreGraphicsState_DrawRectangles( client->gfx_state, rects, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

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
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_FILLRECTANGLE, client->state );

          ret = CoreGraphicsState_FillRectangles( client->gfx_state, rects, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

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
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_BLIT, client->state );

          ret = CoreGraphicsState_Blit( client->gfx_state, rects, points, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

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
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_STRETCHBLIT, client->state );

          ret = CoreGraphicsState_Blit( client->gfx_state, srects, drects, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

