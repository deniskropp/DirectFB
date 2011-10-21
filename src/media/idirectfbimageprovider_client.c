/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <stddef.h>
#include <string.h>

#include <directfb.h>

#include <core/CoreDFB.h>

#include <core/core.h>

#include <direct/interface.h>
#include <direct/mem.h>

#include <fusion/conf.h>

#include <display/idirectfbsurface.h>

#include <media/ImageProvider.h>

#include <media/idirectfbimageprovider_client.h>
#include <media/idirectfbdatabuffer.h>


static DirectResult
IDirectFBImageProvider_Client_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_Client )

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBImageProvider_Client_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_Client )

     if (--data->ref == 0) {
          /* Decrease the data buffer reference counter. */
          if (data->buffer)
               data->buffer->Release( data->buffer );

          ImageProvider_Dispose( &data->client );

          DIRECT_DEALLOCATE_INTERFACE( thiz );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_Client_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                     DFBSurfaceDescription  *ret_dsc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_Client )

     return ImageProvider_GetSurfaceDescription( &data->client, ret_dsc );
}

static DFBResult
IDirectFBImageProvider_Client_GetImageDescription( IDirectFBImageProvider *thiz,
                                                   DFBImageDescription    *ret_dsc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_Client )

     return ImageProvider_GetImageDescription( &data->client, ret_dsc );
}

static DFBResult
IDirectFBImageProvider_Client_RenderTo( IDirectFBImageProvider *thiz,
                                        IDirectFBSurface       *destination,
                                        const DFBRectangle     *destination_rect )
{
     IDirectFBSurface_data *destination_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_Client );

     DIRECT_INTERFACE_GET_DATA_FROM( destination, destination_data, IDirectFBSurface );

     return ImageProvider_RenderTo( &data->client, destination_data->surface, destination_rect );
}

static DFBResult
IDirectFBImageProvider_Client_SetRenderCallback( IDirectFBImageProvider *thiz,
                                                 DIRenderCallback        callback,
                                                 void                   *callback_data )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_Client )

     D_UNIMPLEMENTED();

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_Client_WriteBack( IDirectFBImageProvider *thiz,
                                         IDirectFBSurface       *surface,
                                         const DFBRectangle     *src_rect,
                                         const char             *filename )
{
     return DFB_UNIMPLEMENTED;
}

DFBResult
IDirectFBImageProvider_Client_Construct( IDirectFBImageProvider *thiz,
                                         IDirectFBDataBuffer    *buffer,
                                         CoreDFB                *core )
{
     DFBResult                 ret;
     IDirectFBDataBuffer_data *buffer_data;
     u32                       call_id;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_Client)

     ret = buffer->AddRef( buffer );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;

     ret = CoreDFB_CreateImageProvider( core, buffer_data->call.call_id, &call_id );
     if (ret) {
          buffer->Release( buffer );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     fusion_call_init_from( &data->client.call, call_id, dfb_core_world(core) );

     data->ref    = 1;
     data->core   = core;
     data->buffer = buffer;

     thiz->AddRef                = IDirectFBImageProvider_Client_AddRef;
     thiz->Release               = IDirectFBImageProvider_Client_Release;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_Client_GetSurfaceDescription;
     thiz->GetImageDescription   = IDirectFBImageProvider_Client_GetImageDescription;
     thiz->RenderTo              = IDirectFBImageProvider_Client_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_Client_SetRenderCallback;
     thiz->WriteBack             = IDirectFBImageProvider_Client_WriteBack;

     return DFB_OK;
}

