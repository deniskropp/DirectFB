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

#include "ImageProvider.h"

extern "C" {
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/conf.h>

#include <core/core.h>

#include <display/idirectfbsurface.h>
}

D_DEBUG_DOMAIN( DirectFB_ImageProvider, "DirectFB/ImageProvider", "DirectFB ImageProvider" );

/*********************************************************************************************************************/

static void
ImageProviderDispatch_cleanup( void *ctx,
                               void *ctx2 )
{
     ImageProviderDispatch_Destroy( (ImageProviderDispatch*) ctx );
}

DFBResult
ImageProviderDispatch_Create( IDirectFBDataBuffer     *buffer,
                              IDirectFBImageProvider  *provider,
                              ImageProviderDispatch  **ret_dispatch )
{
     ImageProviderDispatch *dispatch;

     dispatch = (ImageProviderDispatch*) D_CALLOC( 1, sizeof(ImageProviderDispatch) );
     if (!dispatch)
          return (DFBResult) D_OOM();

     dispatch->buffer   = buffer;
     dispatch->provider = provider;

     ImageProvider_Init_Dispatch( core_dfb, dispatch, &dispatch->call );

     fusion_call_add_permissions( &dispatch->call, Core_GetIdentity(), FUSION_CALL_PERMIT_EXECUTE );

     Core_Resource_AddCleanup( Core_GetIdentity(), ImageProviderDispatch_cleanup, dispatch, NULL, &dispatch->cleanup );

     D_MAGIC_SET( dispatch, ImageProviderDispatch );

     *ret_dispatch = dispatch;

     return DFB_OK;
}

static void
ImageProviderDispatch_destruct( void *ctx,
                                void *ctx2 )
{
     ImageProviderDispatch *dispatch = (ImageProviderDispatch*) ctx;

     D_MAGIC_ASSERT( dispatch, ImageProviderDispatch );

     ImageProvider_Deinit_Dispatch( &dispatch->call );

     D_MAGIC_CLEAR( dispatch );

     D_FREE( dispatch );
}

void
ImageProviderDispatch_Destroy( ImageProviderDispatch *dispatch )
{
     D_MAGIC_ASSERT( dispatch, ImageProviderDispatch );

     dispatch->provider->Release( dispatch->provider );
     dispatch->buffer->Release( dispatch->buffer );

     Core_AsyncCall( ImageProviderDispatch_destruct, dispatch, NULL );
}

/*********************************************************************************************************************/

namespace DirectFB {


DFBResult
IImageProvider_Real::Dispose(
)
{
     D_MAGIC_ASSERT( obj, ImageProviderDispatch );

     Core_Resource_DisposeCleanup( obj->cleanup );

     ImageProviderDispatch_Destroy( obj );

     return DFB_OK;
}


DFBResult
IImageProvider_Real::GetSurfaceDescription(
                    DFBSurfaceDescription                     *ret_description
)
{
     D_MAGIC_ASSERT( obj, ImageProviderDispatch );

     return obj->provider->GetSurfaceDescription( obj->provider, ret_description );
}


DFBResult
IImageProvider_Real::GetImageDescription(
                    DFBImageDescription                       *ret_description
)
{
     D_MAGIC_ASSERT( obj, ImageProviderDispatch );

     return obj->provider->GetImageDescription( obj->provider, ret_description );
}


DFBResult
IImageProvider_Real::RenderTo(
                    CoreSurface                               *destination,
                    const DFBRectangle                        *rect
)
{
     DFBResult         ret;
     IDirectFBSurface *surface;

     D_MAGIC_ASSERT( obj, ImageProviderDispatch );

     DIRECT_ALLOCATE_INTERFACE( surface, IDirectFBSurface );
     if (!surface)
          return (DFBResult) D_OOM();

     ret = IDirectFBSurface_Construct( surface, NULL, NULL, NULL, NULL, destination, DSCAPS_NONE, core );
     if (ret)
          return ret;

     ret = obj->provider->RenderTo( obj->provider, surface, rect );

     surface->Release( surface );

     return ret;
}


}
