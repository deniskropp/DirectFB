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

#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <core/core.h>

D_DEBUG_DOMAIN( DirectFB_CoreGraphicsState, "DirectFB/CoreGraphicsState", "DirectFB CoreGraphicsState" );

/*********************************************************************************************************************/


DFBResult
IGraphicsState_Real__SetDrawingFlags(
                    CoreGraphicsState                         *obj,
                    DFBSurfaceDrawingFlags                     flags
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    dfb_state_set_drawing_flags( &obj->state, flags );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetBlittingFlags(
                    CoreGraphicsState                         *obj,
                    DFBSurfaceBlittingFlags                    flags
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    dfb_state_set_blitting_flags( &obj->state, flags );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetClip(
                    CoreGraphicsState                         *obj,
                    const DFBRegion                           *region
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( region != NULL );

    dfb_state_set_clip( &obj->state, region );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetColor(
                    CoreGraphicsState                         *obj,
                    const DFBColor                            *color
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( color != NULL );

    dfb_state_set_color( &obj->state, color );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetSrcBlend(
                    CoreGraphicsState                         *obj,
                    DFBSurfaceBlendFunction                    function
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    dfb_state_set_src_blend( &obj->state, function );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetDstBlend(
                    CoreGraphicsState                         *obj,
                    DFBSurfaceBlendFunction                    function
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    dfb_state_set_dst_blend( &obj->state, function );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetSrcColorKey(
                    CoreGraphicsState                         *obj,
                    u32                                        key
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    dfb_state_set_src_colorkey( &obj->state, key );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetDstColorKey(
                    CoreGraphicsState                         *obj,
                    u32                                        key
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    dfb_state_set_dst_colorkey( &obj->state, key );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetDestination(
                    CoreGraphicsState                         *obj,
                    CoreSurface                               *surface
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( surface != NULL );

    dfb_state_set_destination( &obj->state, surface );

    obj->state.modified    |= SMF_DESTINATION;

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetSource(
                    CoreGraphicsState                         *obj,
                    CoreSurface                               *surface
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( surface != NULL );

    dfb_state_set_source( &obj->state, surface );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetSourceMask(
                    CoreGraphicsState                         *obj,
                    CoreSurface                               *surface
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( surface != NULL );

    dfb_state_set_source_mask( &obj->state, surface );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetSourceMaskVals(
                    CoreGraphicsState                         *obj,
                    const DFBPoint                            *offset,
                    DFBSurfaceMaskFlags                        flags
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( offset != NULL );

    dfb_state_set_source_mask_vals( &obj->state, offset, flags );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetIndexTranslation(
                    CoreGraphicsState                         *obj,
                    const s32                                 *indices,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    dfb_state_set_index_translation( &obj->state, (const s32*) indices, num );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetColorKey(
                    CoreGraphicsState                         *obj,
                    const DFBColorKey                         *key
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( key != NULL );

    dfb_state_set_colorkey( &obj->state, key );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetRenderOptions(
                    CoreGraphicsState                         *obj,
                    DFBSurfaceRenderOptions                    options
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    dfb_state_set_render_options( &obj->state, options );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetMatrix(
                    CoreGraphicsState                         *obj,
                    const s32                                 *values
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    dfb_state_set_matrix( &obj->state, (const s32*) values );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__SetSource2(
                    CoreGraphicsState                         *obj,
                    CoreSurface                               *surface
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( surface != NULL );

    dfb_state_set_source2( &obj->state, surface );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__DrawRectangles(
                    CoreGraphicsState                         *obj,
                    const DFBRectangle                        *rects,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    for (u32 i=0; i<num; i++)
         dfb_gfxcard_drawrectangle( (DFBRectangle*) &rects[i], &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__DrawLines(
                    CoreGraphicsState                         *obj,
                    const DFBRegion                           *lines,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    dfb_gfxcard_drawlines( (DFBRegion*) lines, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__FillRectangles(
                    CoreGraphicsState                         *obj,
                    const DFBRectangle                        *rects,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    dfb_gfxcard_fillrectangles( rects, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__FillTriangles(
                    CoreGraphicsState                         *obj,
                    const DFBTriangle                         *triangles,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    dfb_gfxcard_filltriangles( triangles, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__FillTrapezoids(
                    CoreGraphicsState                         *obj,
                    const DFBTrapezoid                        *trapezoids,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    dfb_gfxcard_filltrapezoids( trapezoids, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__FillSpans(
                    CoreGraphicsState                         *obj,
                    s32                                        y,
                    const DFBSpan                             *spans,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    dfb_gfxcard_fillspans( y, (DFBSpan*) spans, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__Blit(
                    CoreGraphicsState                         *obj,
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( rects != NULL );
    D_ASSERT( points != NULL );

    if (!obj->state.destination || !obj->state.source)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    // FIXME: remove casts
    if (num > 1)
         dfb_gfxcard_batchblit( (DFBRectangle*) rects, (DFBPoint*) points, num, &obj->state );         
    else
         dfb_gfxcard_blit( (DFBRectangle*) rects, ((DFBPoint*)points)->x, ((DFBPoint*)points)->y, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__Blit2(
                    CoreGraphicsState                         *obj,
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points1,
                    const DFBPoint                            *points2,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( rects != NULL );
    D_ASSERT( points1 != NULL );
    D_ASSERT( points2 != NULL );

    if (!obj->state.destination || !obj->state.source || !obj->state.source2)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    // FIXME: remove casts
    dfb_gfxcard_batchblit2( (DFBRectangle*) rects, (DFBPoint*) points1, (DFBPoint*) points2, num, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__StretchBlit( 
                    CoreGraphicsState                         *obj,
                    const DFBRectangle                        *srects,
                    const DFBRectangle                        *drects,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    D_ASSERT( srects != NULL );
    D_ASSERT( drects != NULL );

    if (!obj->state.destination || !obj->state.source)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    for (u32 i=0; i<num; i++)
         dfb_gfxcard_stretchblit( (DFBRectangle*) &srects[i], (DFBRectangle*) &drects[i], &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__TileBlit(
                    CoreGraphicsState                         *obj,
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points1,
                    const DFBPoint                            *points2,
                    u32                                        num
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    if (!obj->state.destination || !obj->state.source)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    D_ASSERT( rects != NULL );
    D_ASSERT( points1 != NULL );
    D_ASSERT( points2 != NULL );

    // FIXME: remove casts
    for (u32 i=0; i<num; i++)
         dfb_gfxcard_tileblit( (DFBRectangle*) &rects[i], points1[i].x, points1[i].y, points2[i].x, points2[i].y, &obj->state );

    return DFB_OK;
}


DFBResult
IGraphicsState_Real__TextureTriangles(
                    CoreGraphicsState                         *obj,
                    const DFBVertex                           *vertices,
                    u32                                        num,
                    DFBTriangleFormation                       formation
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

    if (!obj->state.destination || !obj->state.source)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    D_ASSERT( vertices != NULL );

    // FIXME: remove casts
    dfb_gfxcard_texture_triangles( (DFBVertex*) vertices, num, formation, &obj->state );

    return DFB_OK;
}

