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

#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/manager.h>

#include "idirectfbeventbuffer_dispatcher.h"

static DFBResult Probe();
static DFBResult Construct( IDirectFBEventBuffer *thiz,
                            IDirectFBEventBuffer *real,
                            VoodooManager        *manager,
                            VoodooInstanceID      super,
                            void                 *arg,
                            VoodooInstanceID     *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBEventBuffer, Dispatcher )


/**************************************************************************************************/

/*
 * private data struct of IDirectFBEventBuffer_Dispatcher
 */
typedef struct {
     int                   ref;      /* reference counter */

     IDirectFBEventBuffer *real;
} IDirectFBEventBuffer_Dispatcher_data;

/**************************************************************************************************/

static void
IDirectFBEventBuffer_Dispatcher_Destruct( IDirectFBEventBuffer *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBEventBuffer_Dispatcher_AddRef( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_Release( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     if (--data->ref == 0)
          IDirectFBEventBuffer_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_Reset( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_WaitForEvent( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_WaitForEventWithTimeout( IDirectFBEventBuffer *thiz,
                                                         unsigned int          seconds,
                                                         unsigned int          milli_seconds )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_WakeUp( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_GetEvent( IDirectFBEventBuffer *thiz,
                                          DFBEvent             *event )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_PeekEvent( IDirectFBEventBuffer *thiz,
                                           DFBEvent             *event )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_HasEvent( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_PostEvent( IDirectFBEventBuffer *thiz,
                                           const DFBEvent       *event )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_CreateFileDescriptor( IDirectFBEventBuffer *thiz,
                                                      int                  *fd )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_PostEvent( IDirectFBEventBuffer *thiz, IDirectFBEventBuffer *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     const DFBEvent      *event;
     VoodooMessageParser  parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, event );
     VOODOO_PARSER_END( parser );

     real->PostEvent( real, event );

     return DFB_OK;
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBEventBuffer/Dispatcher: "
              "Handling request for instance %lu with method %lu...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBEVENTBUFFER_METHOD_ID_PostEvent:
               return Dispatch_PostEvent( dispatcher, real, manager, msg );
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
Construct( IDirectFBEventBuffer *thiz,
           IDirectFBEventBuffer *real,
           VoodooManager        *manager,
           VoodooInstanceID      super,
           void                 *arg,      /* Optional arguments to constructor */
           VoodooInstanceID     *ret_instance )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBEventBuffer_Dispatcher)

     ret = voodoo_manager_register( manager, false, thiz, real, Dispatch, ret_instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref  = 1;
     data->real = real;

     thiz->AddRef                  = IDirectFBEventBuffer_Dispatcher_AddRef;
     thiz->Release                 = IDirectFBEventBuffer_Dispatcher_Release;
     thiz->Reset                   = IDirectFBEventBuffer_Dispatcher_Reset;
     thiz->WaitForEvent            = IDirectFBEventBuffer_Dispatcher_WaitForEvent;
     thiz->WaitForEventWithTimeout = IDirectFBEventBuffer_Dispatcher_WaitForEventWithTimeout;
     thiz->GetEvent                = IDirectFBEventBuffer_Dispatcher_GetEvent;
     thiz->PeekEvent               = IDirectFBEventBuffer_Dispatcher_PeekEvent;
     thiz->HasEvent                = IDirectFBEventBuffer_Dispatcher_HasEvent;
     thiz->PostEvent               = IDirectFBEventBuffer_Dispatcher_PostEvent;
     thiz->WakeUp                  = IDirectFBEventBuffer_Dispatcher_WakeUp;
     thiz->CreateFileDescriptor    = IDirectFBEventBuffer_Dispatcher_CreateFileDescriptor;

     return DFB_OK;
}

