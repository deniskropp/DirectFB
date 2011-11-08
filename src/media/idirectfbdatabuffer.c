/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <errno.h>

#include <direct/interface.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/thread.h>

#include <fusion/conf.h>

#include <directfb.h>

#include <misc/util.h>

#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbfont.h>
#include <media/idirectfbimageprovider.h>
#include <media/idirectfbvideoprovider.h>

#if !DIRECTFB_BUILD_PURE_VOODOO
#include <media/DataBuffer.h>
#endif


void
IDirectFBDataBuffer_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_data *data = (IDirectFBDataBuffer_data*) thiz->priv;

     if (data->filename)
          D_FREE( data->filename );

#if !DIRECTFB_BUILD_PURE_VOODOO
     if (fusion_config->secure_fusion)
          DataBuffer_Deinit_Dispatch( &data->call );
#endif

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBDataBuffer_AddRef( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer)

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBDataBuffer_Release( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer)

     if (--data->ref == 0)
          IDirectFBDataBuffer_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Flush( IDirectFBDataBuffer *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Finish( IDirectFBDataBuffer *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_SeekTo( IDirectFBDataBuffer *thiz,
                            unsigned int         offset )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_GetPosition( IDirectFBDataBuffer *thiz,
                                 unsigned int        *offset )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_GetLength( IDirectFBDataBuffer *thiz,
                               unsigned int        *length )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_WaitForData( IDirectFBDataBuffer *thiz,
                                 unsigned int         length )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                            unsigned int         length,
                                            unsigned int         seconds,
                                            unsigned int         milli_seconds )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_GetData( IDirectFBDataBuffer *thiz,
                             unsigned int         length,
                             void                *data,
                             unsigned int        *read )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_PeekData( IDirectFBDataBuffer *thiz,
                              unsigned int         length,
                              int                  offset,
                              void                *data,
                              unsigned int        *read )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_HasData( IDirectFBDataBuffer *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_PutData( IDirectFBDataBuffer *thiz,
                             const void          *data,
                             unsigned int         length )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_CreateImageProvider( IDirectFBDataBuffer     *thiz,
                                         IDirectFBImageProvider **interface_ptr )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer)

     /* Check arguments */
     if (!interface_ptr)
          return DFB_INVARG;

#if !DIRECTFB_BUILD_PURE_VOODOO
     return IDirectFBImageProvider_CreateFromBuffer( thiz, data->core, data->idirectfb, interface_ptr );
#else
     D_BUG( "%s in pure Voodoo build", __FUNCTION__ );
     return DFB_BUG;
#endif
}

static DFBResult
IDirectFBDataBuffer_CreateVideoProvider( IDirectFBDataBuffer     *thiz,
                                         IDirectFBVideoProvider **interface_ptr )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer)

     /* Check arguments */
     if (!interface_ptr)
          return DFB_INVARG;

#if !DIRECTFB_BUILD_PURE_VOODOO
     return IDirectFBVideoProvider_CreateFromBuffer( thiz, data->core, interface_ptr );
#else
     D_BUG( "%s in pure Voodoo build", __FUNCTION__ );
     return DFB_BUG;
#endif
}

static DFBResult
IDirectFBDataBuffer_CreateFont( IDirectFBDataBuffer       *thiz,
                                const DFBFontDescription  *desc,
                                IDirectFBFont            **interface_ptr )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer)

     /* Check arguments */
     if (!interface_ptr || !desc)
          return DFB_INVARG;

#if !DIRECTFB_BUILD_PURE_VOODOO
     return IDirectFBFont_CreateFromBuffer( thiz, data->core, desc, interface_ptr );
#else
     D_BUG( "%s in pure Voodoo build", __FUNCTION__ );
     return DFB_BUG;
#endif
}

DFBResult
IDirectFBDataBuffer_Construct( IDirectFBDataBuffer *thiz,
                               const char          *filename,
                               CoreDFB             *core,
                               IDirectFB           *idirectfb )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDataBuffer)

     data->ref       = 1;
     data->core      = core;
     data->idirectfb = idirectfb;

     if (filename)
          data->filename = D_STRDUP( filename );

#if !DIRECTFB_BUILD_PURE_VOODOO
     if (fusion_config->secure_fusion)
          DataBuffer_Init_Dispatch( core, thiz, &data->call );
#endif

     thiz->AddRef                 = IDirectFBDataBuffer_AddRef;
     thiz->Release                = IDirectFBDataBuffer_Release;
     thiz->Flush                  = IDirectFBDataBuffer_Flush;
     thiz->Finish                 = IDirectFBDataBuffer_Finish;
     thiz->SeekTo                 = IDirectFBDataBuffer_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_HasData;
     thiz->PutData                = IDirectFBDataBuffer_PutData;
     thiz->CreateImageProvider    = IDirectFBDataBuffer_CreateImageProvider;
     thiz->CreateVideoProvider    = IDirectFBDataBuffer_CreateVideoProvider;
     thiz->CreateFont             = IDirectFBDataBuffer_CreateFont;
     
     return DFB_OK;
}

