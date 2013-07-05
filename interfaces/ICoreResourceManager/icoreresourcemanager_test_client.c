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

#include <directfb.h>
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/interface.h>

#include <fusion/fusion.h>

#include <core/core.h>
#include <core/surface.h>

#include "icoreresourcemanager_test_client.h"


D_DEBUG_DOMAIN( ICoreResourceClient_test, "ICoreResourceClient/test", "ICoreResourceClient Interface test Implementation" );

/**********************************************************************************************************************/

static void
ICoreResourceClient_test_Destruct( ICoreResourceClient *thiz )
{
     ICoreResourceClient_test_data *data;

     D_DEBUG_AT( ICoreResourceClient_test, "%s( %p )\n", __FUNCTION__, thiz );

     D_ASSERT( thiz != NULL );

     data = thiz->priv;

     D_INFO( "ICoreResourceClient/test: Removing ID %lu\n", data->identity );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
ICoreResourceClient_test_AddRef( ICoreResourceClient *thiz )
{
     DIRECT_INTERFACE_GET_DATA(ICoreResourceClient_test)

     D_DEBUG_AT( ICoreResourceClient_test, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
ICoreResourceClient_test_Release( ICoreResourceClient *thiz )
{
     DIRECT_INTERFACE_GET_DATA(ICoreResourceClient_test)

     D_DEBUG_AT( ICoreResourceClient_test, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          ICoreResourceClient_test_Destruct( thiz );

     return DFB_OK;
}

/**********************************************************************************************************************/

static inline unsigned int
surface_mem( const CoreSurfaceConfig *config )
{
     unsigned int mem;

     mem = DFB_PLANE_MULTIPLY( config->format, config->size.h ) *
           DFB_BYTES_PER_LINE( config->format, config->size.w );

     if (config->caps & DSCAPS_TRIPLE)
          mem *= 3;
     else if (config->caps & DSCAPS_DOUBLE)
          mem *= 2;

     return mem;
}

static DFBResult
ICoreResourceClient_test_CheckSurface( ICoreResourceClient     *thiz,
                                       const CoreSurfaceConfig *config,
                                       CoreSurfaceTypeFlags     type,
                                       u64                      resource_id,
                                       CorePalette             *palette )
{
     DIRECT_INTERFACE_GET_DATA(ICoreResourceClient_test)

     D_DEBUG_AT( ICoreResourceClient_test, "%s( %p [%lu] )\n", __FUNCTION__, thiz, data->identity );

     D_INFO( "ICoreResourceClient/test: Check surface %dx%d %s %uk (ID %lu, at %uk)\n", config->size.w, config->size.h,
             dfb_pixelformat_name( config->format ), surface_mem( config ) / 1024, data->identity, data->surface_mem / 1024 );

     if (data->surface_mem > 1024 * 1024) {
      //    return DFB_LIMITEXCEEDED;
     }

     return DFB_OK;
}

static DFBResult
ICoreResourceClient_test_CheckSurfaceUpdate( ICoreResourceClient     *thiz,
                                             CoreSurface             *surface,
                                             const CoreSurfaceConfig *config )
{
     unsigned int mem;
     (void)mem;

     DIRECT_INTERFACE_GET_DATA(ICoreResourceClient_test)

     D_DEBUG_AT( ICoreResourceClient_test, "%s( %p [%lu] )\n", __FUNCTION__, thiz, data->identity );

     mem = surface_mem( &surface->config );

     D_DEBUG_AT( ICoreResourceClient_test, "  -> %u bytes\n", mem );

     return DFB_OK;
}

static DFBResult
ICoreResourceClient_test_AddSurface( ICoreResourceClient *thiz,
                                     CoreSurface         *surface )
{
     unsigned int mem;

     DIRECT_INTERFACE_GET_DATA(ICoreResourceClient_test)

     D_DEBUG_AT( ICoreResourceClient_test, "%s( %p [%lu] )\n", __FUNCTION__, thiz, data->identity );

     mem = surface_mem( &surface->config );

     D_DEBUG_AT( ICoreResourceClient_test, "  -> %u bytes\n", mem );

     data->surface_mem += mem;

     return DFB_OK;
}

static DFBResult
ICoreResourceClient_test_RemoveSurface( ICoreResourceClient *thiz,
                                        CoreSurface         *surface )
{
     unsigned int mem;

     DIRECT_INTERFACE_GET_DATA(ICoreResourceClient_test)

     D_DEBUG_AT( ICoreResourceClient_test, "%s( %p [%lu] )\n", __FUNCTION__, thiz, data->identity );

     mem = surface_mem( &surface->config );

     D_DEBUG_AT( ICoreResourceClient_test, "  -> %u bytes\n", mem );

     data->surface_mem -= mem;

     return DFB_OK;
}

static DFBResult
ICoreResourceClient_test_UpdateSurface( ICoreResourceClient     *thiz,
                                        CoreSurface             *surface,
                                        const CoreSurfaceConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(ICoreResourceClient_test)

     D_DEBUG_AT( ICoreResourceClient_test, "%s( %p [%lu] )\n", __FUNCTION__, thiz, data->identity );

     data->surface_mem -= surface_mem( &surface->config );
     data->surface_mem += surface_mem( config );

     return DFB_OK;
}

/**********************************************************************************************************************/

DirectResult
ICoreResourceClient_test_Construct( ICoreResourceClient  *thiz,
                                    ICoreResourceManager *manager,
                                    FusionID              identity )
{
     char   buf[512] = { 0 };
     size_t len;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, ICoreResourceClient_test)

     D_DEBUG_AT( ICoreResourceClient_test, "%s( %p )\n", __FUNCTION__, thiz );

     fusion_get_fusionee_path( core_dfb->world, identity, buf, 512, &len );

     D_INFO( "ICoreResourceClient/test: Adding ID %lu - '%s'\n", identity, buf );

     /* Initialize interface data. */
     data->ref      = 1;
     data->manager  = manager;
     data->identity = identity;


     /* Initialize function pointer table. */
     thiz->AddRef             = ICoreResourceClient_test_AddRef;
     thiz->Release            = ICoreResourceClient_test_Release;

     thiz->CheckSurface       = ICoreResourceClient_test_CheckSurface;
     thiz->CheckSurfaceUpdate = ICoreResourceClient_test_CheckSurfaceUpdate;
     thiz->AddSurface         = ICoreResourceClient_test_AddSurface;
     thiz->RemoveSurface      = ICoreResourceClient_test_RemoveSurface;
     thiz->UpdateSurface      = ICoreResourceClient_test_UpdateSurface;


     return DFB_OK;
}

