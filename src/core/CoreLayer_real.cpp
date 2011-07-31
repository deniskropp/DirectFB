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

#include "CoreLayer.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/core.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreLayer, "DirectFB/CoreLayer", "DirectFB CoreLayer" );

/*********************************************************************************************************************/

namespace DirectFB {



DFBResult
ILayer_Real::CreateContext(
                    CoreLayerContext                         **ret_context
)
{
    D_DEBUG_AT( DirectFB_CoreLayer, "ILayer_Requestor::%s()\n", __FUNCTION__ );

    D_ASSERT( ret_context != NULL );

    return dfb_layer_create_context( obj, ret_context );
}

DFBResult
ILayer_Real::ActivateContext(
                    CoreLayerContext                          *context
)
{
    D_DEBUG_AT( DirectFB_CoreLayer, "ILayer_Requestor::%s()\n", __FUNCTION__ );

    D_ASSERT( context != NULL );

    return dfb_layer_activate_context( obj, context );
}

DFBResult
ILayer_Real::GetPrimaryContext(
                    bool                                       activate,
                    CoreLayerContext                         **ret_context
)
{
     D_DEBUG_AT( DirectFB_CoreLayer, "ILayer_Requestor::%s()\n", __FUNCTION__ );

     D_ASSERT( ret_context != NULL );

     return dfb_layer_get_primary_context( obj, activate, ret_context );
}


}
