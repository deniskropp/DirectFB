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

#include <core/CoreDFB.h>
#include <core/CoreGraphicsState.h>

#include <media/ImageProvider_includes.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <core/core.h>

#include <media/idirectfbdatabuffer_client.h>

D_DEBUG_DOMAIN( DirectFB_CoreDFB, "DirectFB/Core", "DirectFB Core" );

/*********************************************************************************************************************/


DFBResult
ICore_Real__Register( CoreDFB                  *obj,
                      u32                       slave_call
)
{
    D_DEBUG_AT( DirectFB_CoreDFB, "%s()\n", __FUNCTION__ );

    D_MAGIC_ASSERT( obj, CoreDFB );

    return Core_Resource_AddIdentity( Core_GetIdentity(), slave_call );
}

DFBResult
ICore_Real__CreateSurface( CoreDFB                  *obj,
                           const CoreSurfaceConfig  *config,
                           CoreSurfaceTypeFlags      type,
                           u64                       resource_id,
                           CorePalette              *palette,
                           CoreSurface             **ret_surface )
{
     DFBResult    ret;
     CoreSurface *surface;

     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p )\n", __FUNCTION__, core_dfb );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( config != NULL );
     D_ASSERT( ret_surface != NULL );

     ret = Core_Resource_CheckSurface( config, type, resource_id, palette );
     if (ret)
          return ret;

     ret = dfb_surface_create( obj, config, type, resource_id, palette, &surface );
     if (ret)
          return ret;

     Core_Resource_AddSurface( surface );

     *ret_surface = surface;

     return DFB_OK;
}

DFBResult
ICore_Real__CreatePalette( CoreDFB      *obj,
                           u32           size,
                           CorePalette **ret_palette )
{
     D_DEBUG_AT( DirectFB_CoreDFB, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( ret_palette != NULL );

     return dfb_palette_create( obj, size, ret_palette );
}

DFBResult
ICore_Real__CreateState( CoreDFB                              *obj,
                    CoreGraphicsState                        **ret_state
)
{
    D_DEBUG_AT( DirectFB_CoreDFB, "%s()\n", __FUNCTION__ );

    D_MAGIC_ASSERT( obj, CoreDFB );
    D_ASSERT( ret_state != NULL );

    return dfb_graphics_state_create( core_dfb, ret_state );
}

DFBResult
ICore_Real__WaitIdle( CoreDFB                                 *obj

)
{
    D_DEBUG_AT( DirectFB_CoreDFB, "%s()\n", __FUNCTION__ );

    D_MAGIC_ASSERT( obj, CoreDFB );

    return dfb_gfxcard_sync();
}

DFBResult
ICore_Real__CreateImageProvider( CoreDFB                                 *obj,
                    u32                                        buffer_call,
                    u32                                       *ret_call
)
{
     DFBResult               ret;
     IDirectFBDataBuffer    *buffer;
     IDirectFBImageProvider *provider;
     ImageProviderDispatch  *dispatch;

     D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( ret_call != NULL );

     DIRECT_ALLOCATE_INTERFACE( buffer, IDirectFBDataBuffer );
     if (!buffer)
          return (DFBResult) D_OOM();

     /* Construct data buffer client */
     ret = IDirectFBDataBuffer_Client_Construct( buffer, core_dfb, buffer_call );
     if (ret)
          return ret;

     /* Create image provider */
     ret = buffer->CreateImageProvider( buffer, &provider );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     /* Create dispatch object */
     ret = ImageProviderDispatch_Create( buffer, provider, &dispatch );
     if (ret) {
          provider->Release( provider );
          buffer->Release( buffer );
          return ret;
     }

     *ret_call = dispatch->call.call_id;

     return DFB_OK;
}


