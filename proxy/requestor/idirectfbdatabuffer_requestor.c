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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/interface.h>

#include <voodoo/manager.h>
#include <voodoo/message.h>

#include <media/idirectfbdatabuffer.h>

#include <idirectfbdatabuffer_dispatcher.h>


static DFBResult Probe();
static DFBResult Construct( IDirectFBDataBuffer *thiz,
                            VoodooManager       *manager,
                            VoodooInstanceID     instance,
                            void                *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBDataBuffer, Requestor )

/**************************************************************************************************/

/*
 * private data struct of IDirectFBDataBuffer_Requestor
 */
typedef struct {
     IDirectFBDataBuffer_data  base;

     VoodooManager            *manager;
     VoodooInstanceID          instance;
} IDirectFBDataBuffer_Requestor_data;

/**************************************************************************************************/

static void
IDirectFBDataBuffer_Requestor_Destruct( IDirectFBDataBuffer *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     IDirectFBDataBuffer_Destruct( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBDataBuffer_Requestor_AddRef( IDirectFBDataBuffer *thiz )
{
     DFBResult              ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_AddRef, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     if (ret == DFB_OK)
          data->base.ref++;

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Requestor_Release( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     if (--data->base.ref == 0)
          IDirectFBDataBuffer_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Requestor_Flush( IDirectFBDataBuffer *thiz )
{
     DFBResult              ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_Flush, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Requestor_SeekTo( IDirectFBDataBuffer *thiz,
                                      unsigned int         offset )
{
     DFBResult              ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_SeekTo, VREQ_RESPOND, &response,
                                   VMBT_UINT, offset,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Requestor_GetPosition( IDirectFBDataBuffer *thiz,
                                           unsigned int        *ret_offset )
{
     DFBResult              ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     unsigned int           offset;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     if (!ret_offset)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_GetPosition, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_UINT( parser, offset );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_offset = offset;

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Requestor_GetLength( IDirectFBDataBuffer *thiz,
                                         unsigned int        *ret_length )
{
     DFBResult              ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     unsigned int           length;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     if (!ret_length)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_GetLength, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_UINT( parser, length );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_length = length;

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Requestor_WaitForData( IDirectFBDataBuffer *thiz,
                                           unsigned int         length )
{
     DFBResult              ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_WaitForData, VREQ_RESPOND, &response,
                                   VMBT_UINT, length,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Requestor_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                      unsigned int         length,
                                                      unsigned int         seconds,
                                                      unsigned int         milli_seconds )
{
     DFBResult              ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_WaitForDataWithTimeout, VREQ_RESPOND, &response,
                                   VMBT_UINT, length,
                                   VMBT_UINT, seconds,
                                   VMBT_UINT, milli_seconds,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Requestor_GetData( IDirectFBDataBuffer *thiz,
                                       unsigned int         length,
                                       void                *dest,
                                       unsigned int        *ret_read )
{
     DFBResult              ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     unsigned int           read;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     if (!length || !dest)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_GetData, VREQ_RESPOND, &response,
                                   VMBT_UINT, length,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_UINT( parser, read );
     VOODOO_PARSER_READ_DATA( parser, dest, length );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     if (ret_read)
          *ret_read = read;

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Requestor_PeekData( IDirectFBDataBuffer *thiz,
                                        unsigned int         length,
                                        int                  offset,
                                        void                *dest,
                                        unsigned int        *ret_read )
{
     DFBResult              ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     unsigned int           read;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     if (!length || !dest)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_PeekData, VREQ_RESPOND, &response,
                                   VMBT_UINT, length,
                                   VMBT_INT, offset,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_UINT( parser, read );
     VOODOO_PARSER_READ_DATA( parser, dest, length );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     if (ret_read)
          *ret_read = read;

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Requestor_HasData( IDirectFBDataBuffer *thiz )
{
     DFBResult              ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_HasData, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Requestor_PutData( IDirectFBDataBuffer *thiz,
                                       const void          *source,
                                       unsigned int         length )
{
     DFBResult              ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Requestor)

     if (!source || !length)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDATABUFFER_METHOD_ID_PutData, VREQ_RESPOND, &response,
                                   VMBT_UINT, length,
                                   VMBT_DATA, length, source,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

/**************************************************************************************************/

static DFBResult
Probe()
{
     /* This implementation has to be loaded explicitly. */
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBDataBuffer *thiz,
           VoodooManager       *manager,
           VoodooInstanceID     instance,
           void                *arg )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDataBuffer_Requestor)

     ret = IDirectFBDataBuffer_Construct( thiz, NULL );
     if (ret)
          return ret;

     data->manager  = manager;
     data->instance = instance;

     thiz->AddRef                 = IDirectFBDataBuffer_Requestor_AddRef;
     thiz->Release                = IDirectFBDataBuffer_Requestor_Release;
     thiz->Flush                  = IDirectFBDataBuffer_Requestor_Flush;
     thiz->SeekTo                 = IDirectFBDataBuffer_Requestor_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_Requestor_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_Requestor_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_Requestor_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_Requestor_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_Requestor_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_Requestor_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_Requestor_HasData;
     thiz->PutData                = IDirectFBDataBuffer_Requestor_PutData;

     return DFB_OK;
}

