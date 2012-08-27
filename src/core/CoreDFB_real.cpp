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

extern "C" {
#include <directfb.h>

#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/conf.h>
#include <fusion/fusion.h>

#include <core/clipboard.h>
#include <core/core.h>

#include <media/idirectfbdatabuffer_client.h>

#include <idirectfb.h>
}

D_DEBUG_DOMAIN( DirectFB_CoreDFB, "DirectFB/Core", "DirectFB Core" );

/*********************************************************************************************************************/

namespace DirectFB {


DFBResult
ICore_Real::Initialize(
)
{
    D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s()\n", __FUNCTION__ );

    D_MAGIC_ASSERT( obj, CoreDFB );

    if (Core_GetIdentity() != FUSION_ID_MASTER)
         return DFB_ACCESSDENIED;

    return dfb_core_initialize( core );
}

DFBResult
ICore_Real::Register(
     u32                       slave_call
)
{
    D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s()\n", __FUNCTION__ );

    D_MAGIC_ASSERT( obj, CoreDFB );

    return Core_Resource_AddIdentity( Core_GetIdentity(), slave_call );
}

DFBResult
ICore_Real::CreateSurface( const CoreSurfaceConfig  *config,
                           CoreSurfaceTypeFlags      type,
                           u64                       resource_id,
                           CorePalette              *palette,
                           CoreSurface             **ret_surface )
{
     DFBResult    ret;
     CoreSurface *surface;

     D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s( %p )\n", __FUNCTION__, core );

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
ICore_Real::CreatePalette( u32           size,
                           CorePalette **ret_palette )
{
     D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s( %p )\n", __FUNCTION__, core );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( ret_palette != NULL );

     return dfb_palette_create( obj, size, ret_palette );
}

DFBResult
ICore_Real::CreateState(
                    CoreGraphicsState                        **ret_state
)
{
    D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s()\n", __FUNCTION__ );

    D_MAGIC_ASSERT( obj, CoreDFB );
    D_ASSERT( ret_state != NULL );

    return dfb_graphics_state_create( core, ret_state );
}

DFBResult
ICore_Real::WaitIdle(

)
{
    D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s()\n", __FUNCTION__ );

    D_MAGIC_ASSERT( obj, CoreDFB );

    return dfb_gfxcard_sync();
}

DFBResult
ICore_Real::CreateImageProvider(
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
     ret = IDirectFBDataBuffer_Client_Construct( buffer, core, buffer_call );
     if (ret)
          return ret;

     /* Create image provider */
     ret = buffer->CreateImageProvider( buffer, &provider );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     /* Create dispatch object */
     ret = ImageProviderDispatch_Create( idirectfb_singleton, buffer, provider, &dispatch );
     if (ret) {
          provider->Release( provider );
          buffer->Release( buffer );
          return ret;
     }

     *ret_call = dispatch->call.call_id;

     return DFB_OK;
}

DFBResult
ICore_Real::AllowSurface( CoreSurface *surface,
                          const char  *executable,
                          u32          executable_length )
{
     D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s( %p, %p, '%s' )\n", __FUNCTION__, core, surface, executable );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( surface != NULL );
     D_ASSERT( executable != NULL );

     return (DFBResult) fusion_object_add_access( &surface->object, executable );
}

DFBResult
ICore_Real::GetSurface( u32           surface_id,
                        CoreSurface **ret_surface )
{
     DFBResult    ret;
     CoreSurface *surface;
     char         path[1000];
     size_t       path_length;

     D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s( %u )\n", __FUNCTION__, surface_id );

     D_MAGIC_ASSERT( obj, CoreDFB );

     if (fusion_config->secure_fusion) {
          ret = (DFBResult) fusion_get_fusionee_path( dfb_core_world(core), Core_GetIdentity(), path, sizeof(path), &path_length );
          if (ret)
               return ret;

          D_DEBUG_AT( DirectFB_CoreDFB, "  -> '%s'\n", path );
     }

     ret = dfb_core_get_surface( core, surface_id, &surface );
     if (ret)
          return ret;

     if (fusion_config->secure_fusion) {
          ret = (DFBResult) fusion_object_has_access( &surface->object, path );
          if (ret) {
               D_DEBUG_AT( DirectFB_CoreDFB, "  -> NO ACCESS!\n" );
               dfb_surface_unref( surface );
               return ret;
          }

          // FIXME: access to this object needs to be given for other places like using as a blitting source
          surface->object.owner = Core_GetIdentity();
     }

     *ret_surface = surface;

     return DFB_OK;
}


DFBResult
ICore_Real::ClipboardSet(
                    const char                                *mime_type,
                    u32                                        mime_type_size,
                    const char                                *data,
                    u32                                        data_size,
                    u64                                        timestamp_us
)
{
    struct timeval tv;

    D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s()\n", __FUNCTION__ );

    tv.tv_sec  = timestamp_us / 1000000;
    tv.tv_usec = timestamp_us % 1000000;

    return dfb_clipboard_set( (DFBClipboardCore*) DFB_CORE( core, CLIPBOARD ), mime_type, data, data_size, &tv );
}


DFBResult
ICore_Real::ClipboardGet(
                    char                                      *ret_mime_type,
                    u32                                       *ret_mime_type_size,
                    char                                      *ret_data,
                    u32                                       *ret_data_size
)
{
    DFBResult     ret;
    char         *mime_type;
    void         *data;
    unsigned int  data_size;

    D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s()\n", __FUNCTION__ );

    ret = dfb_clipboard_get( (DFBClipboardCore*) DFB_CORE( core, CLIPBOARD ), &mime_type, &data, &data_size );
    if (ret)
         return ret;

    direct_memcpy( ret_mime_type, mime_type, strlen(mime_type) + 1 );
    *ret_mime_type_size = strlen(mime_type) + 1;

    direct_memcpy( ret_data, data, data_size );
    *ret_data_size = data_size;

    free( data );
    free( mime_type );

    return DFB_OK;
}


DFBResult
ICore_Real::ClipboardGetTimestamp(
                    u64                                       *ret_timestamp_us
)
{
    DFBResult      ret;
    struct timeval tv;

    D_DEBUG_AT( DirectFB_CoreDFB, "ICore_Real::%s()\n", __FUNCTION__ );

    ret = dfb_clipboard_get_timestamp( (DFBClipboardCore*) DFB_CORE( core, CLIPBOARD ), &tv );
    if (ret)
         return ret;

    *ret_timestamp_us = tv.tv_sec * 1000000 + tv.tv_usec;

    return DFB_OK;
}


}

