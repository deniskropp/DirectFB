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

#include <directfb_util.h>

#include <direct/debug.h>

#include <core/CoreSurfaceClient.h>

#include <core/core.h>
#include <core/surface.h>
#include <core/surface_client.h>


D_DEBUG_DOMAIN( Core_SurfClient, "Core/SurfClient", "DirectFB Core Surface Client" );

/**********************************************************************************************************************/

static void
surface_client_destructor( FusionObject *object, bool zombie, void *ctx )
{
     CoreSurfaceClient *client = (CoreSurfaceClient*) object;
     CoreSurface       *surface;
     int                index;

     D_MAGIC_ASSERT( client, CoreSurfaceClient );

     surface = client->surface;
     CORE_SURFACE_ASSERT( surface );

     D_DEBUG_AT( Core_SurfClient, "destroying %p (%dx%d%s)\n", client,
                 surface->config.size.w, surface->config.size.h, zombie ? " ZOMBIE" : "");

     CoreSurfaceClient_Deinit_Dispatch( &client->call );

     dfb_surface_lock( surface );

     index = fusion_vector_index_of( &surface->clients, client );
     D_ASSERT( index >= 0 );

     fusion_vector_remove( &surface->clients, index );

     dfb_surface_unlock( surface );

     dfb_surface_unlink( &client->surface );

     D_MAGIC_CLEAR( client );

     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_surface_client_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Surface Client Pool",
                                       sizeof(CoreSurfaceClient),
                                       0,
                                       surface_client_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_client_create( CoreDFB            *core,
                           CoreSurface        *surface,
                           CoreSurfaceClient **ret_client )
{
     DFBResult          ret;
     CoreSurfaceClient *client;

     CORE_SURFACE_ASSERT( surface );
     D_ASSERT( ret_client != NULL );

     D_DEBUG_AT( Core_SurfClient, "%s( %dx%d %s )\n", __FUNCTION__, surface->config.size.w,
                 surface->config.size.h, dfb_pixelformat_name( surface->config.format ) );

     client = dfb_core_create_surface_client( core );
     if (!client)
          return DFB_FUSION;

     ret = dfb_surface_link( &client->surface, surface );
     if (ret) {
          fusion_object_destroy( &client->object );
          return ret;
     }

     D_MAGIC_SET( client, CoreSurfaceClient );

     *ret_client = client;


     dfb_surface_lock( surface );

     fusion_vector_add( &surface->clients, client );

     CoreSurfaceClient_Init_Dispatch( core, client, &client->call );

     fusion_object_activate( &client->object );

     dfb_surface_unlock( surface );

     return DFB_OK;
}

