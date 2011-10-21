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

#include <direct/list.h>
#include <direct/thread.h>

#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/util.h>

#include <idirectfb.h>

#include <media/DataBuffer.h>

#include <media/idirectfbdatabuffer_client.h>


/*
 * private data struct of IDirectFBDataBuffer_Client
 */
typedef struct {
     IDirectFBDataBuffer_data base;

     DataBuffer               client;
} IDirectFBDataBuffer_Client_data;


static void
IDirectFBDataBuffer_Client_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_Destruct( thiz );
}

static DirectResult
IDirectFBDataBuffer_Client_Release( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer)

     if (--data->ref == 0)
          IDirectFBDataBuffer_Client_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Client_Flush( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     return DataBuffer_Flush( &data->client );
}

static DFBResult
IDirectFBDataBuffer_Client_Finish( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     return DataBuffer_Finish( &data->client );
}

static DFBResult
IDirectFBDataBuffer_Client_SeekTo( IDirectFBDataBuffer *thiz,
                                   unsigned int         offset )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     return DataBuffer_SeekTo( &data->client, offset );
}

static DFBResult
IDirectFBDataBuffer_Client_GetPosition( IDirectFBDataBuffer *thiz,
                                        unsigned int        *ret_offset )
{
     DFBResult ret;
     u64       offset;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     if (!ret_offset)
          return DFB_INVARG;

     ret = DataBuffer_GetPosition( &data->client, &offset );
     if (ret == DFB_OK)
          *ret_offset = offset;

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Client_GetLength( IDirectFBDataBuffer *thiz,
                                      unsigned int        *ret_length )
{
     DFBResult ret;
     u64       length;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     if (!ret_length)
          return DFB_INVARG;

     ret = DataBuffer_GetLength( &data->client, &length );
     if (ret == DFB_OK)
          *ret_length = length;

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Client_WaitForData( IDirectFBDataBuffer *thiz,
                                        unsigned int         length )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     return DataBuffer_WaitForData( &data->client, length );
}

static DFBResult
IDirectFBDataBuffer_Client_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                   unsigned int         length,
                                                   unsigned int         seconds,
                                                   unsigned int         milli_seconds )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     return DataBuffer_WaitForDataWithTimeout( &data->client, length, seconds * 1000 + milli_seconds );
}

static DFBResult
IDirectFBDataBuffer_Client_GetData( IDirectFBDataBuffer *thiz,
                                    unsigned int         length,
                                    void                *data_buffer,
                                    unsigned int        *ret_read )
{
     DFBResult ret;
     u32       read;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     ret = DataBuffer_GetData( &data->client, length, data_buffer, &read );
     if (ret == DFB_OK && ret_read)
          *ret_read = read;

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Client_PeekData( IDirectFBDataBuffer *thiz,
                                     unsigned int         length,
                                     int                  offset,
                                     void                *data_buffer,
                                     unsigned int        *ret_read )
{
     DFBResult ret;
     u32       read;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     ret = DataBuffer_PeekData( &data->client, length, offset, data_buffer, &read );
     if (ret == DFB_OK && ret_read)
          *ret_read = read;

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Client_HasData( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     return DataBuffer_HasData( &data->client );
}

static DFBResult
IDirectFBDataBuffer_Client_PutData( IDirectFBDataBuffer *thiz,
                                    const void          *data_buffer,
                                    unsigned int         length )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Client)

     return DataBuffer_PutData( &data->client, data_buffer, length );
}

DFBResult
IDirectFBDataBuffer_Client_Construct( IDirectFBDataBuffer *thiz,
                                      CoreDFB             *core,
                                      u32                  call_id )
{
     DFBResult ret;
     FusionID  call_owner;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDataBuffer_Client)

     ret = IDirectFBDataBuffer_Construct( thiz, NULL, core, idirectfb_singleton );
     if (ret)
          return ret;

     fusion_call_init_from( &data->client.call, call_id, dfb_core_world(core) );

     ret = fusion_call_get_owner( &data->client.call, &call_owner );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     if (call_owner != Core_GetIdentity()) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_ACCESSDENIED;
     }

     thiz->Release                = IDirectFBDataBuffer_Client_Release;
     thiz->Flush                  = IDirectFBDataBuffer_Client_Flush;
     thiz->Finish                 = IDirectFBDataBuffer_Client_Finish;
     thiz->SeekTo                 = IDirectFBDataBuffer_Client_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_Client_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_Client_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_Client_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_Client_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_Client_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_Client_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_Client_HasData;
     thiz->PutData                = IDirectFBDataBuffer_Client_PutData;

     return DFB_OK;
}

