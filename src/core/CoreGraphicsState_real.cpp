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

#include "CoreGraphicsState.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <core/core.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreGraphicsState, "DirectFB/CoreGraphicsState", "DirectFB CoreGraphicsState" );

/*********************************************************************************************************************/

namespace DirectFB {



DFBResult
IGraphicsState_Real::SetDrawingFlags(
                    DFBSurfaceDrawingFlags                     flags
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_drawing_flags( &obj->state, flags );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetBlittingFlags(
                    DFBSurfaceBlittingFlags                    flags
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_blitting_flags( &obj->state, flags );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetClip(
                    const DFBRegion                           *region
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( region != NULL );

    dfb_state_set_clip( &obj->state, region );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetColor(
                    const DFBColor                            *color
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( color != NULL );

    dfb_state_set_color( &obj->state, color );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetSrcBlend(
                    DFBSurfaceBlendFunction                    function
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_src_blend( &obj->state, function );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetDstBlend(
                    DFBSurfaceBlendFunction                    function
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_dst_blend( &obj->state, function );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetSrcColorKey(
                    u32                                        key
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_src_colorkey( &obj->state, key );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetDstColorKey(
                    u32                                        key
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_dst_colorkey( &obj->state, key );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetDestination(
                    CoreSurface                               *surface
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( surface != NULL );

    dfb_state_set_destination( &obj->state, surface );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetSource(
                    CoreSurface                               *surface
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( surface != NULL );

    dfb_state_set_source( &obj->state, surface );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetSourceMask(
                    CoreSurface                               *surface
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( surface != NULL );

    dfb_state_set_source_mask( &obj->state, surface );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetSourceMaskVals(
                    const DFBPoint                            *offset,
                    DFBSurfaceMaskFlags                        flags
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( offset != NULL );

    dfb_state_set_source_mask_vals( &obj->state, offset, flags );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetIndexTranslation(
                    const u32                                 *indices,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_index_translation( &obj->state, (const int*) indices, num );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetColorKey(
                    const DFBColorKey                         *key
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( key != NULL );

    dfb_state_set_colorkey( &obj->state, key );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetRenderOptions(
                    DFBSurfaceRenderOptions                    options
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_render_options( &obj->state, options );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetMatrix(
                    const u32                                 *values
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_matrix( &obj->state, (const s32*) values );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetSource2(
                    CoreSurface                               *surface
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( surface != NULL );

    dfb_state_set_source2( &obj->state, surface );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::DrawRectangles(
                    const DFBRectangle                        *rects,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    for (u32 i=0; i<num; i++)
         dfb_gfxcard_drawrectangle( (DFBRectangle*) &rects[i], &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::FillRectangles(
                    const DFBRectangle                        *rects,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_gfxcard_fillrectangles( rects, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::Blit(
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( rects != NULL );
    D_ASSERT( points != NULL );

    // FIXME: remove casts
    dfb_gfxcard_batchblit( (DFBRectangle*) rects, (DFBPoint*) points, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::StretchBlit(
                    const DFBRectangle                        *srects,
                    const DFBRectangle                        *drects,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( srects != NULL );
    D_ASSERT( drects != NULL );

    //dfb_gfxcard_batchblit( rects, num, &obj->state );

    D_UNIMPLEMENTED();

    return DFB_OK;
}


}

