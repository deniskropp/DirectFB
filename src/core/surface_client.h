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

#ifndef __CORE__SURFACE_CLIENT_H__
#define __CORE__SURFACE_CLIENT_H__

#include <directfb.h>

#include <direct/debug.h>

#include <fusion/object.h>
#include <fusion/vector.h>

#include <core/coretypes.h>



/*
 * Client of a Surface
 */
struct __DFB_CoreSurfaceClient {
     FusionObject             object;

     int                      magic;

     CoreSurface             *surface;

     FusionCall               call;

     u32                      flip_count;    /* last acknowledged frame */
};

#define CORE_SURFACE_CLIENT_ASSERT(client)                                                     \
     do {                                                                                      \
          D_MAGIC_ASSERT( client, CoreSurfaceClient );                                         \
     } while (0)


DFBResult dfb_surface_client_create ( CoreDFB            *core,
                                      CoreSurface        *surface,
                                      CoreSurfaceClient **ret_client );


FUSION_OBJECT_METHODS( CoreSurfaceClient, dfb_surface_client )

FusionObjectPool *dfb_surface_client_pool_create( const FusionWorld *world );

#endif

