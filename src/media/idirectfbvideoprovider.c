/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de>,
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

#include <direct/interface.h>
#include <direct/mem.h>

#include <media/idirectfbvideoprovider.h>
#include <media/idirectfbdatabuffer.h>


static DFBResult
IDirectFBVideoProvider_AddRef( IDirectFBVideoProvider *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_Release( IDirectFBVideoProvider *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_GetCapabilities( IDirectFBVideoProvider       *thiz,
                                        DFBVideoProviderCapabilities *ret_caps )
{
     if (!ret_caps)
          return DFB_INVARG;
          
     *ret_caps = 0;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_GetSurfaceDescription( IDirectFBVideoProvider *thiz,
                                              DFBSurfaceDescription  *ret_dsc )
{
     if (!ret_dsc)
          return DFB_INVARG;
          
     ret_dsc->flags = DSDESC_NONE;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_GetStreamDescription( IDirectFBVideoProvider *thiz,
                                             DFBStreamDescription   *ret_dsc )
{
     if (!ret_dsc)
          return DFB_INVARG;
          
     memset( ret_dsc, 0, sizeof(DFBStreamDescription) );
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_PlayTo( IDirectFBVideoProvider *thiz,
                               IDirectFBSurface       *destination,
                               const DFBRectangle     *destination_rect,
                               DVFrameCallback         callback,
                               void                   *ctx )
{    
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_Stop( IDirectFBVideoProvider *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_GetStatus( IDirectFBVideoProvider *thiz,
                                  DFBVideoProviderStatus *ret_status )
{
     if (!ret_status)
          return DFB_INVARG;
          
     *ret_status = DVSTATE_UNKNOWN;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_SeekTo( IDirectFBVideoProvider *thiz,
                               double                  seconds )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_GetPos( IDirectFBVideoProvider *thiz,
                               double                 *ret_seconds )
{
     if (!ret_seconds)
          return DFB_INVARG;
          
     *ret_seconds = 0.0;
          
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_GetLength( IDirectFBVideoProvider *thiz,
                                  double                 *ret_seconds )
{
     if (!ret_seconds)
          return DFB_INVARG;
          
     *ret_seconds = 0.0;
          
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_GetColorAdjustment( IDirectFBVideoProvider *thiz,
                                           DFBColorAdjustment     *ret_adj )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_SetColorAdjustment( IDirectFBVideoProvider   *thiz,
                                           const DFBColorAdjustment *adj )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_SendEvent( IDirectFBVideoProvider *thiz,
                                  const DFBEvent         *event )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_SetPlaybackFlags( IDirectFBVideoProvider        *thiz,
                                         DFBVideoProviderPlaybackFlags  flags )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_SetSpeed( IDirectFBVideoProvider *thiz,
                                 double                  multiplier )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_GetSpeed( IDirectFBVideoProvider *thiz,
                                 double                 *ret_multiplier )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_SetVolume( IDirectFBVideoProvider *thiz,
                                  float                   level )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_GetVolume( IDirectFBVideoProvider *thiz,
                                  float                  *ret_level )
{
     return DFB_UNIMPLEMENTED;
}

static void
IDirectFBVideoProvider_Construct( IDirectFBVideoProvider *thiz )
{
     thiz->AddRef                = IDirectFBVideoProvider_AddRef;
     thiz->Release               = IDirectFBVideoProvider_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_GetSurfaceDescription;
     thiz->GetStreamDescription  = IDirectFBVideoProvider_GetStreamDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_Stop;
     thiz->GetStatus             = IDirectFBVideoProvider_GetStatus;
     thiz->SeekTo                = IDirectFBVideoProvider_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_GetLength;
     thiz->GetColorAdjustment    = IDirectFBVideoProvider_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBVideoProvider_SetColorAdjustment;
     thiz->SendEvent             = IDirectFBVideoProvider_SendEvent;
     thiz->SetPlaybackFlags      = IDirectFBVideoProvider_SetPlaybackFlags;
     thiz->SetSpeed              = IDirectFBVideoProvider_SetSpeed;
     thiz->GetSpeed              = IDirectFBVideoProvider_GetSpeed;
     thiz->SetVolume             = IDirectFBVideoProvider_SetVolume;
     thiz->GetVolume             = IDirectFBVideoProvider_GetVolume;
}


DFBResult
IDirectFBVideoProvider_CreateFromBuffer( IDirectFBDataBuffer     *buffer,
                                         IDirectFBVideoProvider **interface )
{
     DFBResult                            ret;
     DirectInterfaceFuncs                *funcs = NULL;
     IDirectFBDataBuffer_data            *buffer_data;
     IDirectFBVideoProvider              *videoprovider;
     IDirectFBVideoProvider_ProbeContext  ctx;

     /* Get the private information of the data buffer. */
     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;
     if (!buffer_data)
          return DFB_DEAD;

     /* Provide a fallback for video providers without data buffer support. */
     ctx.filename = buffer_data->filename;
     ctx.buffer   = buffer;
     
     /* Wait until 64 bytes are available. */
     ret = buffer->WaitForData( buffer, sizeof(ctx.header) );
     if (ret)
          return ret;

     /* Clear context header. */
     memset( ctx.header, 0, sizeof(ctx.header) );

     /* Read the first 64 bytes. */
     buffer->PeekData( buffer, sizeof(ctx.header), 0, ctx.header, NULL );

     /* Find a suitable implementation. */
     ret = DirectGetInterface( &funcs, "IDirectFBVideoProvider", NULL, DirectProbeInterface, &ctx );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( videoprovider, IDirectFBVideoProvider );
     
     /* Initialize interface pointers. */
     IDirectFBVideoProvider_Construct( videoprovider );

     /* Construct the interface. */
     ret = funcs->Construct( videoprovider, buffer );
     if (ret)
          return ret;

     *interface = videoprovider;

     return DFB_OK;
}

