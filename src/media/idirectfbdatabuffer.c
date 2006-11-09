/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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
#include <unistd.h>

#include <string.h>
#include <errno.h>

#include <sys/time.h>

#include <pthread.h>

#include <fusion/reactor.h>
#include <direct/list.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/windows.h>

#include <misc/util.h>
#include <direct/interface.h>
#include <direct/mem.h>

#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbimageprovider.h>
#include <media/idirectfbvideoprovider.h>


void
IDirectFBDataBuffer_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_data *data = (IDirectFBDataBuffer_data*) thiz->priv;

     if (data->filename)
          D_FREE( data->filename );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBDataBuffer_AddRef( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer)

     data->ref++;

     return DFB_OK;
}

static DFBResult
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
                                         IDirectFBImageProvider **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer)

     /* Check arguments */
     if (!interface)
          return DFB_INVARG;

     return IDirectFBImageProvider_CreateFromBuffer( thiz, data->core, interface );
}

static DFBResult
IDirectFBDataBuffer_CreateVideoProvider( IDirectFBDataBuffer     *thiz,
                                         IDirectFBVideoProvider **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer)

     /* Check arguments */
     if (!interface)
          return DFB_INVARG;

     return IDirectFBVideoProvider_CreateFromBuffer( thiz, interface );
}

DFBResult
IDirectFBDataBuffer_Construct( IDirectFBDataBuffer *thiz,
                               const char          *filename,
                               CoreDFB             *core )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDataBuffer)

     data->ref  = 1;
     data->core = core;

     if (filename)
          data->filename = D_STRDUP( filename );

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
     
     return DFB_OK;
}

