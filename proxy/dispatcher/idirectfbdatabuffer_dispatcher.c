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

#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include <media/idirectfbdatabuffer.h>

#include "idirectfb_dispatcher.h"
#include "idirectfbdatabuffer_dispatcher.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBDataBuffer *thiz,
                            IDirectFBDataBuffer *real,
                            VoodooManager       *manager,
                            VoodooInstanceID     super,
                            VoodooInstanceID    *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBDataBuffer, Dispatcher )


/**************************************************************************************************/

/*
 * private data struct of IDirectFBDataBuffer_Dispatcher
 */
typedef struct {
     IDirectFBDataBuffer_data  base;

     IDirectFBDataBuffer      *real;

     VoodooInstanceID          self;         /* The instance of this dispatcher itself. */
     VoodooInstanceID          super;        /* The instance of the super interface. */

     VoodooManager            *manager;
} IDirectFBDataBuffer_Dispatcher_data;

/**************************************************************************************************/

static void
IDirectFBDataBuffer_Dispatcher_Destruct( IDirectFBDataBuffer *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     IDirectFBDataBuffer_Destruct( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBDataBuffer_Dispatcher_AddRef( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     data->base.ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_Release( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     if (--data->base.ref == 0)
          IDirectFBDataBuffer_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_Flush( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     return data->real->Flush( data->real );
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_SeekTo( IDirectFBDataBuffer *thiz,
                                       unsigned int         offset )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     return data->real->SeekTo( data->real, offset );
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_GetPosition( IDirectFBDataBuffer *thiz,
                                            unsigned int        *offset )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     return data->real->GetPosition( data->real, offset );
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_GetLength( IDirectFBDataBuffer *thiz,
                                          unsigned int        *length )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     return data->real->GetLength( data->real, length );
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_WaitForData( IDirectFBDataBuffer *thiz,
                                            unsigned int         length )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     return data->real->WaitForData( data->real, length );
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                       unsigned int         length,
                                                       unsigned int         seconds,
                                                       unsigned int         milli_seconds )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     return data->real->WaitForDataWithTimeout( data->real, length, seconds, milli_seconds );
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_GetData( IDirectFBDataBuffer *thiz,
                                        unsigned int         length,
                                        void                *dest,
                                        unsigned int        *read )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     return data->real->GetData( data->real, length, dest, read );
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_PeekData( IDirectFBDataBuffer *thiz,
                                         unsigned int         length,
                                         int                  offset,
                                         void                *dest,
                                         unsigned int        *read )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     return data->real->PeekData( data->real, length, offset, dest, read );
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_HasData( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     return data->real->HasData( data->real );
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_PutData( IDirectFBDataBuffer *thiz,
                                        const void          *source,
                                        unsigned int         length )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     return data->real->PutData( data->real, source, length );
}

static DFBResult
IDirectFBDataBuffer_Dispatcher_CreateImageProvider( IDirectFBDataBuffer     *thiz,
                                                    IDirectFBImageProvider **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     if (!ret_interface)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->super,
                                   IDIRECTFB_METHOD_ID_CreateImageProvider,
                                   VREQ_RESPOND | VREQ_ASYNC, &response,
                                   VMBT_ID, data->self,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBImageProvider",
                                            response->instance, &interface );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;

     return ret;
}

/**************************************************************************************************/

static DirectResult
Dispatch_AddRef( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                 VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     ret = thiz->AddRef( thiz );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Release( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     thiz->Release( thiz );

     return DFB_OK;
}

static DirectResult
Dispatch_Flush( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     ret = real->Flush( real );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SeekTo( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                 VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     unsigned int        offset;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, offset );
     VOODOO_PARSER_END( parser );

     ret = real->SeekTo( real, offset );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetPosition( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     unsigned int offset;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     ret = real->GetPosition( real, &offset );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_UINT, offset,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetLength( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     unsigned int length;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     ret = real->GetLength( real, &length );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_UINT, length,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_WaitForData( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     unsigned int        length;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, length );
     VOODOO_PARSER_END( parser );

     ret = real->WaitForData( real, length );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                                 VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     unsigned int        length;
     unsigned int        seconds;
     unsigned int        milli_seconds;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, length );
     VOODOO_PARSER_GET_UINT( parser, seconds );
     VOODOO_PARSER_GET_UINT( parser, milli_seconds );
     VOODOO_PARSER_END( parser );

     ret = real->WaitForDataWithTimeout( real, length, seconds, milli_seconds );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetData( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     unsigned int         length;
     unsigned int         read;
     void                *tmp;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, length );
     VOODOO_PARSER_END( parser );

     if (length > 16384)
          length = 16384;

     tmp = alloca( length );

     ret = real->GetData( real, length, tmp, &read );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_UINT, read,
                                    VMBT_DATA, read, tmp,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_PeekData( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                   VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     unsigned int         length;
     int                  offset;
     unsigned int         read;
     void                *tmp;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, length );
     VOODOO_PARSER_GET_INT( parser, offset );
     VOODOO_PARSER_END( parser );

     if (length > 16384)
          length = 16384;

     tmp = alloca( length );

     ret = real->PeekData( real, length, offset, tmp, &read );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_UINT, read,
                                    VMBT_DATA, read, tmp,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_HasData( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     ret = real->HasData( real );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_PutData( IDirectFBDataBuffer *thiz, IDirectFBDataBuffer *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     unsigned int         length;
     const void          *source;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDataBuffer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, length );
     VOODOO_PARSER_GET_DATA( parser, source );
     VOODOO_PARSER_END( parser );

     ret = real->PutData( real, source, length );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBDataBuffer/Dispatcher: "
              "Handling request for instance %lu with method %lu...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBDATABUFFER_METHOD_ID_AddRef:
               return Dispatch_AddRef( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_Release:
               return Dispatch_Release( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_Flush:
               return Dispatch_Flush( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_SeekTo:
               return Dispatch_SeekTo( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_GetPosition:
               return Dispatch_GetPosition( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_GetLength:
               return Dispatch_GetLength( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_WaitForData:
               return Dispatch_WaitForData( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_WaitForDataWithTimeout:
               return Dispatch_WaitForDataWithTimeout( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_GetData:
               return Dispatch_GetData( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_PeekData:
               return Dispatch_PeekData( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_HasData:
               return Dispatch_HasData( dispatcher, real, manager, msg );

          case IDIRECTFBDATABUFFER_METHOD_ID_PutData:
               return Dispatch_PutData( dispatcher, real, manager, msg );
     }

     return DFB_NOSUCHMETHOD;
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
           IDirectFBDataBuffer *real,
           VoodooManager       *manager,
           VoodooInstanceID     super,
           VoodooInstanceID    *ret_instance )
{
     DFBResult        ret;
     VoodooInstanceID instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDataBuffer_Dispatcher)

     ret = IDirectFBDataBuffer_Construct( thiz, NULL );
     if (ret)
          return ret;

     ret = voodoo_manager_register( manager, false, thiz, real, Dispatch, &instance );
     if (ret) {
          IDirectFBDataBuffer_Destruct( thiz );
          return ret;
     }

     *ret_instance = instance;

     data->real    = real;
     data->self    = instance;
     data->super   = super;
     data->manager = manager;

     thiz->AddRef                 = IDirectFBDataBuffer_Dispatcher_AddRef;
     thiz->Release                = IDirectFBDataBuffer_Dispatcher_Release;
     thiz->Flush                  = IDirectFBDataBuffer_Dispatcher_Flush;
     thiz->SeekTo                 = IDirectFBDataBuffer_Dispatcher_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_Dispatcher_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_Dispatcher_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_Dispatcher_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_Dispatcher_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_Dispatcher_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_Dispatcher_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_Dispatcher_HasData;
     thiz->PutData                = IDirectFBDataBuffer_Dispatcher_PutData;
     thiz->CreateImageProvider    = IDirectFBDataBuffer_Dispatcher_CreateImageProvider;

     return DFB_OK;
}

