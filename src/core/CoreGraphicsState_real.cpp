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

#include <fusion/fusion.h>

#include <core/core.h>

#include <gfx/clip.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreGraphicsState, "DirectFB/CoreGraphicsState", "DirectFB CoreGraphicsState" );

/*********************************************************************************************************************/

namespace DirectFB {


/**********************************************************************************************************************
 * State
 */


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

extern "C" {
extern CoreGraphicsDevice *card;   /* FIXME */
}

static DFBResult
State_Check( CoreGraphicsState   *state,
             DFBAccelerationMask  accel )
{
     D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

     if (!card->funcs.CheckState)
          return DFB_UNSUPPORTED;

     /* If the function needs to be checked... */
     if (!(state->state.checked & accel)) {
          /* Unset unchecked functions. */
          state->state.accel = (DFBAccelerationMask)(state->state.accel & state->state.checked);

          /* Call driver to (re)set the bit if the function is supported. */
          card->funcs.CheckState( card->driver_data, card->device_data, &state->state, accel );

          /* Add the function to 'checked functions'. */
          state->state.checked = (DFBAccelerationMask)(state->state.checked | accel);

          /* Add additional functions the driver might have checked, too. */
          state->state.checked = (DFBAccelerationMask)(state->state.checked | state->state.accel);
     }

     if (state->state.accel & accel)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
State_UnlockBuffers( CoreGraphicsState *state )
{
     if (state->state.flags & (CSF_DESTINATION_LOCKED | CSF_SOURCE_LOCKED | CSF_SOURCE_MASK_LOCKED | CSF_SOURCE2_LOCKED)) {
          D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

          /*
           * Push our own identity for buffer unlocking calls (locality of accessor)
           */
          Core_PushIdentity( 0 );

          if (state->state.flags & CSF_SOURCE2_LOCKED)
               dfb_surface_unlock_buffer( state->state.source2, &state->state.src2 );

          if (state->state.flags & CSF_SOURCE_MASK_LOCKED)
               dfb_surface_unlock_buffer( state->state.source_mask, &state->state.src_mask );

          if (state->state.flags & CSF_SOURCE_LOCKED)
               dfb_surface_unlock_buffer( state->state.source, &state->state.src );

          if (state->state.flags & CSF_DESTINATION_LOCKED)
               dfb_surface_unlock_buffer( state->state.destination, &state->state.dst );

          Core_PopIdentity();

          state->state.flags = (CardStateFlags)(state->state.flags & ~(CSF_DESTINATION_LOCKED | CSF_SOURCE_LOCKED | CSF_SOURCE_MASK_LOCKED | CSF_SOURCE2_LOCKED));
     }

     return DFB_OK;
}

static DFBResult
State_LockBuffers( CoreGraphicsState   *state,
                   DFBAccelerationMask  accel )
{
     DFBResult              ret      = DFB_OK;
     CoreSurfaceAccessFlags access   = CSAF_WRITE;
     CoreSurfaceAccessorID  accessor = CSAID_CPU;

     D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

     /* find locking flags */
     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->state.blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                            DSBLIT_BLEND_COLORALPHA   |
                                            DSBLIT_DST_COLORKEY))
               access = (CoreSurfaceAccessFlags)(access | CSAF_READ);
     }
     else if (state->state.drawingflags & (DSDRAW_BLEND | DSDRAW_DST_COLORKEY))
          access = (CoreSurfaceAccessFlags)(access | CSAF_READ);

     if (state->hw) {
          accessor = CSAID_GPU;
     }

     /*
      * Push our own identity for buffer locking calls (locality of accessor)
      */
     Core_PushIdentity( 0 );

     /* lock destination */
     if (!(state->state.flags & CSF_DESTINATION_LOCKED) || state->state.dst.buffer->surface != state->state.destination) {
          if (state->state.flags & CSF_DESTINATION_LOCKED) // FIXME: also need to check if 'to' has changed! and 'from' for sources!
               dfb_surface_unlock_buffer( state->state.destination, &state->state.dst );

          ret = dfb_surface_lock_buffer( state->state.destination, state->state.to, accessor, access, &state->state.dst );
          if (ret) {
               D_DEBUG_AT( DirectFB_CoreGraphicsState, "  -> Could not lock destination!\n" );
               goto out;
          }

          state->state.flags = (CardStateFlags)(state->state.flags | CSF_DESTINATION_LOCKED);
     }

     /* if blitting... */
     if (DFB_BLITTING_FUNCTION( accel )) {
          /* ...lock source for reading */
          if (!(state->state.flags & CSF_SOURCE_LOCKED) || state->state.src.buffer->surface != state->state.source) {
               if (state->state.flags & CSF_SOURCE_LOCKED)
                    dfb_surface_unlock_buffer( state->state.source, &state->state.src );

               ret = dfb_surface_lock_buffer( state->state.source, state->state.from, accessor, CSAF_READ, &state->state.src );
               if (ret) {
                    D_DEBUG_AT( DirectFB_CoreGraphicsState, "  -> Could not lock source!\n" );
                    goto out;
               }

               state->state.flags = (CardStateFlags)(state->state.flags | CSF_SOURCE_LOCKED);
          }

          /* if using a mask... */
          if (state->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               /* ...lock source mask for reading */
               if (!(state->state.flags & CSF_SOURCE_MASK_LOCKED) || state->state.src_mask.buffer->surface != state->state.source_mask) {
                    if (state->state.flags & CSF_SOURCE_MASK_LOCKED)
                         dfb_surface_unlock_buffer( state->state.source_mask, &state->state.src_mask );

                    ret = dfb_surface_lock_buffer( state->state.source_mask, state->state.from, accessor, CSAF_READ, &state->state.src_mask );
                    if (ret) {
                         D_DEBUG_AT( DirectFB_CoreGraphicsState, "  -> Could not lock source mask!\n" );
                         goto out;
                    }

                    state->state.flags = (CardStateFlags)(state->state.flags | CSF_SOURCE_MASK_LOCKED);
               }
          }

          /* if using source2... */
          if (accel == DFXL_BLIT2) {
               /* ...lock source2 for reading */
               if (!(state->state.flags & CSF_SOURCE2_LOCKED) || state->state.src2.buffer->surface != state->state.source2) {
                    if (state->state.flags & CSF_SOURCE2_LOCKED)
                         dfb_surface_unlock_buffer( state->state.source2, &state->state.src2 );

                    ret = dfb_surface_lock_buffer( state->state.source2, state->state.from, accessor, CSAF_READ, &state->state.src2 );
                    if (ret) {
                         D_DEBUG_AT( DirectFB_CoreGraphicsState, "  -> Could not lock source2!\n" );
                         goto out;
                    }

                    state->state.flags = (CardStateFlags)(state->state.flags | CSF_SOURCE2_LOCKED);
               }
          }
     }

out:
     Core_PopIdentity();

     if (ret)
          State_UnlockBuffers( state );

     return ret;
}

static DFBResult
State_EnterHW( CoreGraphicsState   *state,
               DFBAccelerationMask  accel )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

     if (state->hw)
          return DFB_OK;

     State_UnlockBuffers( state );

     ret = dfb_gfxcard_lock( GDLF_NONE );
     if (ret)
          return ret;

     state->hw = true;
     state->sw = DFXL_NONE;

     return DFB_OK;
}

static DFBResult
State_LeaveHW( CoreGraphicsState   *state )
{
     D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

     if (!state->hw)
          return DFB_OK;

     /* start command processing if not already running */
     if (card->funcs.EmitCommands)
          card->funcs.EmitCommands( card->driver_data, card->device_data );

     /* Store the serial of the operation. */
     if (card->funcs.GetSerial)
          card->funcs.GetSerial( card->driver_data, card->device_data, &state->state.serial );

     State_UnlockBuffers( state );

     dfb_gfxcard_unlock();

     state->hw = false;

     return DFB_OK;
}

static DFBResult
State_Leave( CoreGraphicsState *state )
{
     D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

     if (state->hw)
          State_LeaveHW( state );

     state->sw = DFXL_NONE;

     State_UnlockBuffers( state );

     return DFB_OK;
}

static CoreGraphicsState     *last_state;
static FusionDispatchCleanup *state_cleanup;

static void
State_DispatchCleanup( void *ctx )
{
     if (last_state) {
          State_Leave( last_state );

          last_state = NULL;
     }
}

static DFBResult
State_Enter( CoreGraphicsState   *state,
             DFBAccelerationMask  accel )
{
     DFBResult              ret;
     StateModificationFlags flags = (StateModificationFlags)(SMF_DESTINATION | SMF_RENDER_OPTIONS);

     D_DEBUG_AT( DirectFB_CoreGraphicsState, "%s()\n", __FUNCTION__ );

     if (last_state != state) {
          state->state.modified = SMF_ALL;

          if (last_state)
               State_Leave( last_state );

          last_state = state;
     }

     if (!state_cleanup) {
          ret = (DFBResult) fusion_dispatch_cleanup_add( dfb_core_world(core_dfb), State_DispatchCleanup, NULL, &state_cleanup );
          if (ret)
               return ret;
     }

     state->modified     = (StateModificationFlags)(state->modified     | state->state.modified);
     state->mod_sw       = (StateModificationFlags)(state->mod_sw       | state->state.modified);
     state->state.mod_hw = (StateModificationFlags)(state->state.mod_hw | state->state.modified);

     state->state.modified = SMF_NONE;

     /*
      * Gather state flags that would require a CheckState()
      */
     if (DFB_DRAWING_FUNCTION( accel )) {
          flags = (StateModificationFlags)(flags | SMF_DRAWING_FLAGS);

          if (state->state.drawingflags & DSDRAW_BLEND)
               flags = (StateModificationFlags)(flags | SMF_SRC_BLEND | SMF_DST_BLEND);
     }
     else {
          flags = (StateModificationFlags)(flags | SMF_BLITTING_FLAGS | SMF_SOURCE);

          if (accel == DFXL_BLIT2)
               flags = (StateModificationFlags)(flags | SMF_SOURCE2);

          if (state->state.blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                            DSBLIT_BLEND_COLORALPHA))
               flags = (StateModificationFlags)(flags | SMF_SRC_BLEND | SMF_DST_BLEND);

          if (state->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
               flags = (StateModificationFlags)(flags | SMF_SOURCE_MASK | SMF_SOURCE_MASK_VALS);

          if (state->state.blittingflags & DSBLIT_INDEX_TRANSLATION)
               flags = (StateModificationFlags)(flags | SMF_INDEX_TRANSLATION);
     }

     if (state->modified & flags) {
          state->state.checked = DFXL_NONE;

          ret = State_Check( state, accel );
          if (ret)
               State_LeaveHW( state );
          else
               State_EnterHW( state, accel );
     }

     ret = State_LockBuffers( state, accel );
     if (ret) {
          if (state->hw) {
               State_LeaveHW( state );

               ret = State_LockBuffers( state, accel );
               if (ret)
                    return ret;
          }
          else
               return ret;
     }


     state->modified = SMF_NONE;


     if (state->hw) {
          if (!(state->state.set & accel) || state->state.mod_hw)
               card->funcs.SetState( card->driver_data, card->device_data, &card->funcs, &state->state, accel );
     }
     else {
          if (!(state->sw & accel) || state->mod_sw) {
               if (!gAcquire2( &state->state, accel ))
                    return DFB_UNSUPPORTED;

               state->mod_sw = SMF_NONE;
               state->sw     = accel;
          }
     }

     return DFB_OK;
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

    if (dfb_config->accel1) {
         D_UNIMPLEMENTED();
    }
    else {
         for (u32 i=0; i<num; i++)
              dfb_gfxcard_drawrectangle( (DFBRectangle*) &rects[i], &obj->state );
    }

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::DrawLines(
                    const DFBRegion                           *lines,
                    u32                                        num
)
{
    DFBResult ret;

    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    if (dfb_config->accel1) {
         ret = State_Enter( obj, DFXL_DRAWLINE );
         if (ret)
              return ret;

         if (obj->hw) {
              for (u32 i=0; i<num; i++) {
                   DFBRegion line = lines[i];

                   if (dfb_clip_line( &obj->state.clip, &line ))
                        card->funcs.DrawLine( card->driver_data, card->device_data, &line );
              }
         }
         else {
              for (u32 i=0; i<num; i++) {
                   DFBRegion line = lines[i];

                   if (dfb_clip_line( &obj->state.clip, &line ))
                        gDrawLine( &obj->state, &line );
              }
         }
    }
    else {
         dfb_gfxcard_drawlines( (DFBRegion*) lines, num, &obj->state );
    }

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::FillRectangles(
                    const DFBRectangle                        *rects,
                    u32                                        num
)
{
    DFBResult ret;

    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination)
         return DFB_NOCONTEXT;

    if (dfb_config->accel1) {
         ret = State_Enter( obj, DFXL_FILLRECTANGLE );
         if (ret)
              return ret;

         if (obj->hw) {
              for (u32 i=0; i<num; i++) {
                   DFBRectangle rect = rects[i];

                   if (dfb_clip_rectangle( &obj->state.clip, &rect ))
                        card->funcs.FillRectangle( card->driver_data, card->device_data, &rect );
              }
         }
         else {
              for (u32 i=0; i<num; i++) {
                   DFBRectangle rect = rects[i];

                   if (dfb_clip_rectangle( &obj->state.clip, &rect ))
                        gFillRectangle( &obj->state, &rect );
              }
         }
    }
    else {
         dfb_gfxcard_fillrectangles( rects, num, &obj->state );
    }

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

    if (dfb_config->accel1) {
         D_UNIMPLEMENTED();
    }
    else {
         dfb_gfxcard_filltriangles( triangles, num, &obj->state );
    }

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

    if (dfb_config->accel1) {
         D_UNIMPLEMENTED();
    }
    else {
         dfb_gfxcard_filltrapezoids( trapezoids, num, &obj->state );
    }

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

    if (dfb_config->accel1) {
         D_UNIMPLEMENTED();
    }
    else {
         dfb_gfxcard_fillspans( y, (DFBSpan*) spans, num, &obj->state );
    }

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::Blit(
                    const DFBRectangle                        *rects,
                    const DFBPoint                            *points,
                    u32                                        num
)
{
    DFBResult ret;

    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination || !obj->state.source)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    D_ASSERT( rects != NULL );
    D_ASSERT( points != NULL );

    if (dfb_config->accel1) {
         ret = State_Enter( obj, DFXL_BLIT );
         if (ret)
              return ret;

         if (obj->hw) {
              for (u32 i=0; i<num; i++) {
                   if (dfb_clip_blit_precheck( &obj->state.clip, rects[i].w, rects[i].h, points[i].x, points[i].y )) {
                        DFBRectangle srect = rects[i];
                        DFBRectangle drect = { points[i].x, points[i].y, rects[i].w, rects[i].h };

                        dfb_clip_blit_flipped_rotated( &obj->state.clip, &srect, &drect, obj->state.blittingflags );

                        card->funcs.Blit( card->driver_data, card->device_data, &srect, drect.x, drect.y );
                   }
              }
         }
         else {
              for (u32 i=0; i<num; i++) {
                   if (dfb_clip_blit_precheck( &obj->state.clip, rects[i].w, rects[i].h, points[i].x, points[i].y )) {
                        DFBRectangle srect = rects[i];
                        DFBRectangle drect = { points[i].x, points[i].y, rects[i].w, rects[i].h };

                        dfb_clip_blit_flipped_rotated( &obj->state.clip, &srect, &drect, obj->state.blittingflags );

                        gBlit( &obj->state, &srect, drect.x, drect.y );
                   }
              }
         }
    }
    else {
         // FIXME: remove casts
         dfb_gfxcard_batchblit( (DFBRectangle*) rects, (DFBPoint*) points, num, &obj->state );
    }

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

    if (dfb_config->accel1) {
         D_UNIMPLEMENTED();
    }
    else {
         // FIXME: remove casts
         dfb_gfxcard_batchblit2( (DFBRectangle*) rects, (DFBPoint*) points1, (DFBPoint*) points2, num, &obj->state );
    }

    return DFB_OK;
}


DFBResult
IGraphicsState_Real::StretchBlit(
                    const DFBRectangle                        *srects,
                    const DFBRectangle                        *drects,
                    u32                                        num
)
{
    DFBResult ret;

    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (!obj->state.destination || !obj->state.source)
         return DFB_NOCONTEXT;

    if ((obj->state.blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && !obj->state.source_mask)
         return DFB_NOCONTEXT;

    D_ASSERT( srects != NULL );
    D_ASSERT( drects != NULL );

    if (dfb_config->accel1) {
         ret = State_Enter( obj, DFXL_STRETCHBLIT );
         if (ret)
              return ret;

         if (obj->hw) {
              for (u32 i=0; i<num; i++) {
                   if (dfb_clip_blit_precheck( &obj->state.clip, drects[i].w, drects[i].h, drects[i].x, drects[i].y )) {
                        DFBRectangle srect = srects[i];
                        DFBRectangle drect = drects[i];

                        if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
                            !D_FLAGS_IS_SET( card->caps.clip, DFXL_STRETCHBLIT ))
                        {
                             D_ONCE( "clipping stretchblit outside of driver may cause jaggies" );

                             dfb_clip_stretchblit( &obj->state.clip, &srect, &drect );
                        }

                        card->funcs.StretchBlit( card->driver_data, card->device_data, &srect, &drect );
                   }
              }
         }
         else {
              for (u32 i=0; i<num; i++) {
                   if (dfb_clip_blit_precheck( &obj->state.clip, drects[i].w, drects[i].h, drects[i].x, drects[i].y )) {
                        DFBRectangle srect = srects[i];
                        DFBRectangle drect = drects[i];

                        gStretchBlit( &obj->state, &srect, &drect );
                   }
              }
         }
    }
    else {
         dfb_gfxcard_batchstretchblit( (DFBRectangle*) srects, (DFBRectangle*) drects, num, &obj->state );
    }

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

    if (dfb_config->accel1) {
         D_UNIMPLEMENTED();
    }
    else {
         // FIXME: remove casts
         for (u32 i=0; i<num; i++)
              dfb_gfxcard_tileblit( (DFBRectangle*) &rects[i], points1[i].x, points1[i].y, points2[i].x, points2[i].y, &obj->state );
    }

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

    if (dfb_config->accel1) {
         D_UNIMPLEMENTED();
    }
    else {
         // FIXME: remove casts
         dfb_gfxcard_texture_triangles( (DFBVertex*) vertices, num, formation, &obj->state );
    }

    return DFB_OK;
}


/**********************************************************************************************************************
 * Flush
 */

DFBResult
IGraphicsState_Real::Flush(
)
{
    D_DEBUG_AT( DirectFB_CoreGraphicsState, "IGraphicsState_Real::%s()\n", __FUNCTION__ );

    if (dfb_config->accel1) {
         if (last_state == obj) {
              State_Leave( last_state );

              last_state = NULL;
         }
    }

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


}

