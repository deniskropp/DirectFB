/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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



//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include "CoreGraphicsState.h"
#include "Renderer.h"
#include "Task.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <fusion/fusion.h>

#include <core/core.h>

#include <gfx/clip.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreGraphicsState,          "DirectFB/CoreGraphicsState",          "DirectFB CoreGraphicsState" );
D_DEBUG_DOMAIN( DirectFB_CoreGraphicsState_Throttle, "DirectFB/CoreGraphicsState/Throttle", "DirectFB CoreGraphicsState Throttle" );

/*********************************************************************************************************************/

namespace DirectFB {


/**********************************************************************************************************************
 * State
 */

extern "C" {
void
CoreGraphicsState_Destruct( CoreGraphicsState *state )
{
     if (state->renderer)
          delete state->renderer;
}
}

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
IGraphicsState_Real::SetColorAndIndex(
                    const DFBColor                            *color,
                    u32                                        index)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( color != NULL );

    dfb_state_set_color( &obj->state, color );
    dfb_state_set_color_index( &obj->state, index );

    obj->state.colors[0]        = *color;
    obj->state.color_indices[0] = index;

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

    obj->state.modified = (StateModificationFlags)(obj->state.modified | SMF_DESTINATION);

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
                    const s32                                 *indices,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_index_translation( &obj->state, (const s32*) indices, num );

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
                    const s32                                 *values
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
IGraphicsState_Real::SetFrom(
                    CoreSurfaceBufferRole                      role,
                    DFBSurfaceStereoEye                        eye
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s( %d, %d )\n", __FUNCTION__, role, eye );

    dfb_state_set_from( &obj->state, role, eye );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetTo(
                    CoreSurfaceBufferRole                      role,
                    DFBSurfaceStereoEye                        eye
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s( %d, %d )\n", __FUNCTION__, role, eye );

    dfb_state_set_to( &obj->state, role, eye );

    return DFB_OK;
}


/**********************************************************************************************************************
 * Rendering
 */

class ThrottleGraphicsState : public Renderer::Throttle
{
private:
    CoreGraphicsState *state;

public:
    ThrottleGraphicsState( Renderer          &renderer,
                           CoreGraphicsState *state )
        :
        Throttle( renderer ),
        state( state )
    {
        D_DEBUG_AT( DirectFB_CoreGraphicsState_Throttle, "ThrottleGraphicsState::%s( %p )\n", __FUNCTION__, this );
    }

protected:
    virtual void SetThrottle( int percent )
    {
        D_DEBUG_AT( DirectFB_CoreGraphicsState_Throttle, "ThrottleGraphicsState::%s( %p, %d )\n", __FUNCTION__, this, percent );

        fusion_call_set_quota( &state->call, state->object.identity, percent ? 0 : (dfb_config->graphics_state_call_limit ?: 0xffffffff) );
    }
};

static void
CoreGraphicsState_SetupRenderer( CoreGraphicsState *state )
{
    if (!state->renderer) {
         state->renderer = new Renderer( &state->state, state );

         state->renderer->SetThrottle( new ThrottleGraphicsState(*state->renderer, state) );
    }
}


DFBResult
IGraphicsState_Real::DrawRectangles(
                    const DFBRectangle                        *rects,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->DrawRectangles( rects, num );

         return DFB_OK;
    }

    for (u32 i=0; i<num; i++)
         dfb_gfxcard_drawrectangle( (DFBRectangle*) &rects[i], &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::DrawLines(
                    const DFBRegion                           *lines,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->DrawLines( lines, num );

         return DFB_OK;
    }

    dfb_gfxcard_drawlines( (DFBRegion*) lines, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::FillRectangles(
                    const DFBRectangle                        *rects,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->FillRectangles( rects, num );

         return DFB_OK;
    }

    dfb_gfxcard_fillrectangles( rects, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::FillTriangles(
                    const DFBTriangle                         *triangles,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->FillTriangles( triangles, num );

         return DFB_OK;
    }

    dfb_gfxcard_filltriangles( triangles, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::FillTrapezoids(
                    const DFBTrapezoid                        *trapezoids,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->FillTrapezoids( trapezoids, num );

         return DFB_OK;
    }

    dfb_gfxcard_filltrapezoids( trapezoids, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::FillSpans(
                    s32                                        y,
                    const DFBSpan                             *spans,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->FillSpans( y, spans, num );

         return DFB_OK;
    }

    dfb_gfxcard_fillspans( y, (DFBSpan*) spans, num, &obj->state );

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

    if (!obj->state.destination || !obj->state.source)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    D_ASSERT( rects != NULL );
    D_ASSERT( points != NULL );

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->Blit( rects, points, num );

         return DFB_OK;
    }

    // FIXME: remove casts
    dfb_gfxcard_batchblit( (DFBRectangle*) rects, (DFBPoint*) points, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::Blit2(
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points1,
                    const DFBPoint                            *points2,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination || !obj->state.source || !obj->state.source2)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    D_ASSERT( rects != NULL );
    D_ASSERT( points1 != NULL );
    D_ASSERT( points2 != NULL );

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->Blit2( rects, points1, points2, num );

         return DFB_OK;
    }

    // FIXME: remove casts
    dfb_gfxcard_batchblit2( (DFBRectangle*) rects, (DFBPoint*) points1, (DFBPoint*) points2, num, &obj->state );

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

    if (!obj->state.destination || !obj->state.source)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    D_ASSERT( srects != NULL );
    D_ASSERT( drects != NULL );

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->StretchBlit( srects, drects, num );

         return DFB_OK;
    }

    dfb_gfxcard_batchstretchblit( (DFBRectangle*) srects, (DFBRectangle*) drects, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::TileBlit(
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points1,
                    const DFBPoint                            *points2,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination || !obj->state.source)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    D_ASSERT( rects != NULL );
    D_ASSERT( points1 != NULL );
    D_ASSERT( points2 != NULL );

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->TileBlit( rects, points1, points2, num );

         return DFB_OK;
    }

    // FIXME: remove casts
    for (u32 i=0; i<num; i++)
         dfb_gfxcard_tileblit( (DFBRectangle*) &rects[i], points1[i].x, points1[i].y, points2[i].x, points2[i].y, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::TextureTriangles(
                    const DFBVertex                           *vertices,
                    u32                                        num,
                    DFBTriangleFormation                       formation
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination || !obj->state.source)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    D_ASSERT( vertices != NULL );

    if (dfb_config->task_manager) {
         CoreGraphicsState_SetupRenderer( obj );

         obj->renderer->TextureTriangles( vertices, num, formation );

         return DFB_OK;
    }

    // FIXME: remove casts
    dfb_gfxcard_texture_triangles( (DFBVertex*) vertices, num, formation, &obj->state );

    return DFB_OK;
}


/**********************************************************************************************************************
 * Flush
 */

DFBResult
IGraphicsState_Real::Flush(
                    u32                                         cookie,
                    u32                                         flags
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s( cookie %u, flags 0x%08x )\n", __FUNCTION__, cookie, flags );

    if (dfb_config->task_manager) {
        if (obj->renderer)
            obj->renderer->Flush( cookie, (CoreGraphicsStateClientFlushFlags) flags );
    }
    else if (cookie) {
        dfb_gfxcard_sync();

        dfb_graphics_state_dispatch_done( obj, cookie );
    }
    else
        dfb_gfxcard_flush();

    return DFB_OK;
}

DFBResult
IGraphicsState_Real::ReleaseSource(
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    dfb_state_set_source( &obj->state, NULL );
    dfb_state_set_source_mask( &obj->state, NULL );
    dfb_state_set_source2( &obj->state, NULL );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::SetSrcConvolution(
                    const DFBConvolutionFilter               *filter
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( filter != NULL );

    dfb_state_set_src_convolution( &obj->state, filter );

    return DFB_OK;
}

DFBResult
IGraphicsState_Real::GetAccelerationMask(
                    DFBAccelerationMask *ret_accel
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( ret_accel != NULL );

    return dfb_state_get_acceleration_mask( &obj->state, ret_accel );
}


}

