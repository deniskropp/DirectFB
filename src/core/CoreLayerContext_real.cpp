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



#include <config.h>

#include "CoreLayerContext.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/wm.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreLayerContext, "DirectFB/CoreLayerContext", "DirectFB CoreLayerContext" );

/*********************************************************************************************************************/

namespace DirectFB {



DFBResult
ILayerContext_Real::GetPrimaryRegion(
                    DFBBoolean                                 create,
                    CoreLayerRegion                          **ret_region
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Requestor::%s()\n", __FUNCTION__ );

    D_ASSERT( ret_region != NULL );

    return dfb_layer_context_get_primary_region( obj, create, ret_region );
}

DFBResult
ILayerContext_Real::TestConfiguration(
                    const DFBDisplayLayerConfig               *config,
                    DFBDisplayLayerConfigFlags                *ret_failed
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Requestor::%s()\n", __FUNCTION__ );

    D_ASSERT( config != NULL );

    return dfb_layer_context_test_configuration( obj, config, ret_failed );
}

DFBResult
ILayerContext_Real::SetConfiguration(
                    const DFBDisplayLayerConfig               *config
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Requestor::%s()\n", __FUNCTION__ );

    D_ASSERT( config != NULL );

    return dfb_layer_context_set_configuration( obj, config );
}

DFBResult
ILayerContext_Real::SetSrcColorKey(
                    const DFBColorKey                         *key
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( key != NULL );

    return dfb_layer_context_set_src_colorkey( obj, key->r, key->g, key->b, key->index );
}


DFBResult
ILayerContext_Real::SetDstColorKey(
                    const DFBColorKey                         *key
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( key != NULL );

    return dfb_layer_context_set_dst_colorkey( obj, key->r, key->g, key->b, key->index );
}


DFBResult
ILayerContext_Real::SetSourceRectangle(
                    const DFBRectangle                        *rectangle
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( rectangle != NULL );

    return dfb_layer_context_set_sourcerectangle( obj, rectangle );
}


DFBResult
ILayerContext_Real::SetScreenLocation(
                    const DFBLocation                         *location
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( location != NULL );

    return dfb_layer_context_set_screenlocation( obj, location );
}


DFBResult
ILayerContext_Real::SetScreenRectangle(
                    const DFBRectangle                        *rectangle
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( rectangle != NULL );

    return dfb_layer_context_set_screenrectangle( obj, rectangle );
}


DFBResult
ILayerContext_Real::SetScreenPosition(
                    const DFBPoint                            *position
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( position != NULL );

    return dfb_layer_context_set_screenposition( obj, position->x, position->y );
}


DFBResult
ILayerContext_Real::SetOpacity(
                    u8                                         opacity
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    return dfb_layer_context_set_opacity( obj, opacity );
}


DFBResult
ILayerContext_Real::SetRotation(
                    s32                                        rotation
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    return dfb_layer_context_set_rotation( obj, rotation );
}


DFBResult
ILayerContext_Real::SetColorAdjustment(
                    const DFBColorAdjustment                  *adjustment
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( adjustment != NULL );

    return dfb_layer_context_set_coloradjustment( obj, adjustment );
}


DFBResult
ILayerContext_Real::SetStereoDepth(
                    DFBBoolean                                 follow_video,
                    s32                                        z
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    return dfb_layer_context_set_stereo_depth( obj, follow_video, z );
}


DFBResult
ILayerContext_Real::SetFieldParity(
                    u32                                        field
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    return dfb_layer_context_set_field_parity( obj, field );
}


DFBResult
ILayerContext_Real::SetClipRegions(
                    const DFBRegion                           *regions,
                    u32                                        num,
                    DFBBoolean                                 positive
)
{
    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( regions != NULL );

    return dfb_layer_context_set_clip_regions( obj, regions, num, positive ? DFB_TRUE : DFB_FALSE );
}

DFBResult
ILayerContext_Real::CreateWindow(
                    const DFBWindowDescription                *description,
                    CoreWindow                               **ret_window
)
{
    DFBResult ret;

    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( description != NULL );
    D_ASSERT( ret_window != NULL );

    if (description->flags & DWDESC_PARENT) {
        CoreWindow *parent;

        ret = dfb_core_get_window( core, description->parent_id, &parent );
        if (ret)
            return ret;

        if (fusion_object_check_owner( &parent->object, Core_GetIdentity(), false )) {
             dfb_window_unref( parent );
             return DFB_ACCESSDENIED;
        }

        dfb_window_unref( parent );
    }

    if (description->flags & DWDESC_TOPLEVEL_ID) {
        CoreWindow *toplevel;

        ret = dfb_core_get_window( core, description->toplevel_id, &toplevel );
        if (ret)
            return ret;

        if (fusion_object_check_owner( &toplevel->object, Core_GetIdentity(), false )) {
             dfb_window_unref( toplevel );
             return DFB_ACCESSDENIED;
        }

        dfb_window_unref( toplevel );
    }

    return dfb_layer_context_create_window( core, obj, description, ret_window );
}

DFBResult
ILayerContext_Real::FindWindow(
                    DFBWindowID                                window_id,
                    CoreWindow                               **ret_window
)
{
    CoreWindow *window;
    FusionID    caller;

    D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

    D_ASSERT( ret_window != NULL );

    window = dfb_layer_context_find_window( obj, window_id );
    if (!window)
         return DFB_IDNOTFOUND;

    caller = Core_GetIdentity();

    if (dfb_config->ownership_check &&
        caller != FUSION_ID_MASTER &&
        window->object.identity != caller &&
        fusion_object_check_owner( &window->object, caller, false ))
    {
         dfb_window_unref( window );
         return DFB_ACCESSDENIED;
    }

    *ret_window = window;

    return DFB_OK;
}

typedef struct {
     unsigned long    resource_id;
     CoreWindow      *window;
} FindWindowByResourceID_Context;

static DFBEnumerationResult
FindWindowByResourceID_WindowCallback( CoreWindow *window,
                                       void       *_ctx )
{
     FindWindowByResourceID_Context *ctx = (FindWindowByResourceID_Context *) _ctx;

     if (window->surface) {
          if (window->surface->resource_id == ctx->resource_id) {
               ctx->window = window;

               return DFENUM_CANCEL;
          }
     }

     return DFENUM_OK;
}

DFBResult
ILayerContext_Real::FindWindowByResourceID(
                    u64                                        resource_id,
                    CoreWindow                               **ret_window
)
{
     DFBResult                       ret;
     CoreLayerContext               *context = obj;
     CoreWindowStack                *stack;
     FindWindowByResourceID_Context  ctx;

     D_DEBUG_AT( DirectFB_CoreLayerContext, "ILayerContext_Real::%s()\n", __FUNCTION__ );

     D_ASSERT( ret_window != NULL );

     stack = context->stack;
     D_ASSERT( stack != NULL );

     ctx.resource_id = resource_id;
     ctx.window      = NULL;

     ret = (DFBResult) dfb_layer_context_lock( context );
     if (ret)
         return ret;

     ret = dfb_wm_enum_windows( stack, FindWindowByResourceID_WindowCallback, &ctx );
     if (ret == DFB_OK) {
          if (ctx.window) {
               ret = (DFBResult) dfb_window_ref( ctx.window );
               if (ret == DFB_OK)
                    *ret_window = ctx.window;
          }
          else
               ret = DFB_IDNOTFOUND;
     }

     dfb_layer_context_unlock( context );

     return ret;
}


}

