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
 * private data struct of IDirectFBDataBuffer_Memory
 */
typedef struct {
     IDirectFBDataBuffer_data base;
} IDirectFBDataBuffer_Memory_data;


static void
IDirectFBDataBuffer_Memory_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_Destruct( thiz );
}

static DFBResult
IDirectFBDataBuffer_Memory_Release( IDirectFBDataBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBDataBuffer)

     if (--data->ref == 0)
          IDirectFBDataBuffer_Memory_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Memory_Flush( IDirectFBDataBuffer *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_SeekTo( IDirectFBDataBuffer *thiz,
                                   unsigned int         offset )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_GetPosition( IDirectFBDataBuffer *thiz,
                                        unsigned int        *offset )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_GetLength( IDirectFBDataBuffer *thiz,
                                      unsigned int        *length )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_WaitForData( IDirectFBDataBuffer *thiz,
                                        unsigned int         length )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                   unsigned int         length,
                                                   unsigned int         seconds,
                                                   unsigned int         milli_seconds )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_GetData( IDirectFBDataBuffer *thiz,
                                    unsigned int         length,
                                    void                *data,
                                    unsigned int        *read )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_PeekData( IDirectFBDataBuffer *thiz,
                                     unsigned int         length,
                                     unsigned int         offset,
                                     void                *data,
                                     unsigned int        *read )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_HasData( IDirectFBDataBuffer *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_PutData( IDirectFBDataBuffer *thiz,
                                    const void          *data,
                                    unsigned int         length )
{
     return DFB_UNIMPLEMENTED;
}

DFBResult
IDirectFBDataBuffer_Memory_Construct( IDirectFBDataBuffer *thiz,
                                      void                *data_buffer,
                                      unsigned int         length )
{
     DFBResult ret;

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDataBuffer_Memory)

     ret = IDirectFBDataBuffer_Construct( thiz );
     if (ret)
          return ret;

     thiz->Release                = IDirectFBDataBuffer_Memory_Release;
     thiz->Flush                  = IDirectFBDataBuffer_Memory_Flush;
     thiz->SeekTo                 = IDirectFBDataBuffer_Memory_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_Memory_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_Memory_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_Memory_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_Memory_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_Memory_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_Memory_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_Memory_HasData;
     thiz->PutData                = IDirectFBDataBuffer_Memory_PutData;

     return DFB_OK;
}

