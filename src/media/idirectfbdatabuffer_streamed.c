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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <malloc.h>
#include <errno.h>

#include <sys/time.h>

#include <pthread.h>

#include <core/fusion/reactor.h>
#include <core/fusion/list.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/windows.h>

#include <misc/util.h>
#include <misc/mem.h>

#include <media/idirectfbdatabuffer.h>

/*
 * private data struct of IDirectFBDataBuffer_Streamed
 */
typedef struct {
     IDirectFBDataBuffer_data base;
} IDirectFBDataBuffer_Streamed_data;


static void
IDirectFBDataBuffer_Streamed_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_Destruct( thiz );
}

static DFBResult
IDirectFBDataBuffer_Streamed_Release( IDirectFBDataBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer)

     if (--data->ref == 0)
          IDirectFBDataBuffer_Streamed_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_Flush( IDirectFBDataBuffer *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_SeekTo( IDirectFBDataBuffer *thiz,
                                     unsigned int         offset )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_GetPosition( IDirectFBDataBuffer *thiz,
                                          unsigned int        *offset )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_GetLength( IDirectFBDataBuffer *thiz,
                                        unsigned int        *length )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_WaitForData( IDirectFBDataBuffer *thiz,
                                          unsigned int         length )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                     unsigned int         length,
                                                     unsigned int         seconds,
                                                     unsigned int         milli_seconds )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_GetData( IDirectFBDataBuffer *thiz,
                                      unsigned int         length,
                                      void                *data,
                                      unsigned int        *read )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_PeekData( IDirectFBDataBuffer *thiz,
                                       unsigned int         length,
                                       unsigned int         offset,
                                       void                *data,
                                       unsigned int        *read )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_HasData( IDirectFBDataBuffer *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_PutData( IDirectFBDataBuffer *thiz,
                                      const void          *data,
                                      unsigned int         length )
{
     return DFB_UNIMPLEMENTED;
}

DFBResult
IDirectFBDataBuffer_Streamed_Construct( IDirectFBDataBuffer *thiz )
{
     DFBResult ret;

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDataBuffer_Streamed)

     ret = IDirectFBDataBuffer_Construct( thiz );
     if (ret)
          return ret;

     thiz->Release                = IDirectFBDataBuffer_Streamed_Release;
     thiz->Flush                  = IDirectFBDataBuffer_Streamed_Flush;
     thiz->SeekTo                 = IDirectFBDataBuffer_Streamed_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_Streamed_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_Streamed_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_Streamed_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_Streamed_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_Streamed_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_Streamed_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_Streamed_HasData;
     thiz->PutData                = IDirectFBDataBuffer_Streamed_PutData;

     return DFB_OK;
}

