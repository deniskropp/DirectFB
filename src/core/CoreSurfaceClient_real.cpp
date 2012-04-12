/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

#include "CoreSurfaceClient.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/surface_client.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreSurfaceClient, "DirectFB/CoreSurfaceClient", "DirectFB CoreSurfaceClient" );

/*********************************************************************************************************************/

namespace DirectFB {


DFBResult
ISurfaceClient_Real::FrameAck(
                        u32                                   flip_count
                        )
{
     int                i;
     CoreSurface       *surface;
     CoreSurfaceClient *client;
     u32                count = 0xffffffff;

     D_DEBUG_AT( DirectFB_CoreSurfaceClient, "ISurfaceClient_Real::%s( %d )\n", __FUNCTION__, flip_count );

     surface = obj->surface;
     CORE_SURFACE_ASSERT( surface );

     dfb_surface_lock( surface );

     // FIXME: handle wrap around


     obj->flip_count = flip_count;

     fusion_vector_foreach (client, i, surface->clients) {
          if (client->flip_count < count)
               count = client->flip_count;
     }

     if (count > surface->flips_acked) {
          surface->flips_acked = count;

          dfb_surface_notify_frame( surface, surface->flips_acked );
     }

     dfb_surface_unlock( surface );

     return DFB_OK;
}


}

