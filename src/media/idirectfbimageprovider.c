/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <core/core.h>

#include <direct/interface.h>
#include <direct/mem.h>

#include <media/idirectfbimageprovider.h>
#include <media/idirectfbdatabuffer.h>


static DFBResult
IDirectFBImageProvider_AddRef( IDirectFBImageProvider *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_Release( IDirectFBImageProvider *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                              DFBSurfaceDescription  *ret_dsc )
{
     if (!ret_dsc)
          return DFB_INVARG;
          
     ret_dsc->flags = DSDESC_NONE;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_GetImageDescription( IDirectFBImageProvider *thiz,
                                            DFBImageDescription    *ret_dsc )
{
     if (!ret_dsc)
          return DFB_INVARG;
          
     ret_dsc->caps = DICAPS_NONE;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_RenderTo( IDirectFBImageProvider *thiz,
                                 IDirectFBSurface       *destination,
                                 const DFBRectangle     *destination_rect )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_SetRenderCallback( IDirectFBImageProvider *thiz,
                                          DIRenderCallback        callback,
                                          void                   *callback_data )
{
     return DFB_UNIMPLEMENTED;
}

static void
IDirectFBImageProvider_Construct( IDirectFBImageProvider *thiz )
{
     thiz->AddRef                = IDirectFBImageProvider_AddRef;
     thiz->Release               = IDirectFBImageProvider_Release;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_GetSurfaceDescription;
     thiz->GetImageDescription   = IDirectFBImageProvider_GetImageDescription;
     thiz->RenderTo              = IDirectFBImageProvider_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_SetRenderCallback;
}
     

DFBResult
IDirectFBImageProvider_CreateFromBuffer( IDirectFBDataBuffer     *buffer,
                                         CoreDFB                 *core,
                                         IDirectFBImageProvider **interface )
{
     DFBResult                            ret;
     DirectInterfaceFuncs                *funcs = NULL;
     IDirectFBDataBuffer_data            *buffer_data;
     IDirectFBImageProvider              *imageprovider;
     IDirectFBImageProvider_ProbeContext  ctx;

     /* Get the private information of the data buffer. */
     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;
     if (!buffer_data)
          return DFB_DEAD;

     /* Clear for safety, especially header data. */
     memset( &ctx, 0, sizeof(ctx) );

     /* Provide a fallback for image providers without data buffer support. */
     ctx.filename = buffer_data->filename;

     /* Wait until 32 bytes are available. */
     ret = buffer->WaitForData( buffer, 32 );
     if (ret)
          return ret;

     /* Read the first 32 bytes. */
     buffer->PeekData( buffer, 32, 0, ctx.header, NULL );

     /* Find a suitable implementation. */
     ret = DirectGetInterface( &funcs, "IDirectFBImageProvider", NULL, DirectProbeInterface, &ctx );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( imageprovider, IDirectFBImageProvider );
     
     /* Initialize interface pointers. */
     IDirectFBImageProvider_Construct( imageprovider );

     /* Construct the interface. */
     ret = funcs->Construct( imageprovider, buffer, core );
     if (ret)
          return ret;

     *interface = imageprovider;

     return DFB_OK;
}

