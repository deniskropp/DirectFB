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

#include "CoreWindowStack.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/wm.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreWindowStack, "DirectFB/CoreWindowStack", "DirectFB CoreWindowStack" );

/*********************************************************************************************************************/

namespace DirectFB {



DFBResult
IWindowStack_Real::RepaintAll(
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_repaint_all( obj );
}


DFBResult
IWindowStack_Real::GetInsets(
     CoreWindow                               *window,
     DFBInsets                                *ret_insets
)
{
    DFBResult ret;

    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    ret = (DFBResult) dfb_layer_context_lock( obj->context );
    if (ret)
         return ret;

    ret = dfb_wm_get_insets( obj, window, ret_insets );

    dfb_layer_context_unlock( obj->context );

    return ret;
}


DFBResult
IWindowStack_Real::CursorEnable(
                    bool                                       enable
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_cursor_enable( core, obj, enable );
}


DFBResult
IWindowStack_Real::CursorSetShape(
                    CoreSurface                               *shape,
                    const DFBPoint                            *hotspot
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_cursor_set_shape( obj, shape, hotspot->x, hotspot->y );
}


DFBResult
IWindowStack_Real::CursorSetOpacity(
                    u8                                         opacity
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_cursor_set_opacity( obj, opacity );
}


DFBResult
IWindowStack_Real::CursorSetAcceleration(
                    u32                                        numerator,
                    u32                                        denominator,
                    u32                                        threshold
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_cursor_set_acceleration( obj, numerator, denominator, threshold );
}


DFBResult
IWindowStack_Real::CursorWarp(
                    const DFBPoint                            *position
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_cursor_warp( obj, position->x, position->y );
}


DFBResult
IWindowStack_Real::CursorGetPosition(
                    DFBPoint                                  *ret_position
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_get_cursor_position( obj, &ret_position->x, &ret_position->y );
}


DFBResult
IWindowStack_Real::BackgroundSetMode(
                    DFBDisplayLayerBackgroundMode              mode
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_set_background_mode( obj, mode );
}


DFBResult
IWindowStack_Real::BackgroundSetImage(
                    CoreSurface                               *image
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_set_background_image( obj, image );
}


DFBResult
IWindowStack_Real::BackgroundSetColor(
                    const DFBColor                            *color
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_set_background_color( obj, color );
}


DFBResult
IWindowStack_Real::BackgroundSetColorIndex(
                    s32                                        index
)
{
    D_DEBUG_AT( DirectFB_CoreWindowStack, "IWindowStack_Real::%s()\n", __FUNCTION__ );

    return dfb_windowstack_set_background_color_index( obj, index );
}


}

