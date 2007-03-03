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

static void
IDirectFBEventBuffer_Dispatcher_Destruct( IDirectFBEventBuffer *thiz )
{
     IDirectFBEventBuffer_Dispatcher_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     data->real->Release( data->real );

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

     return data->real->Reset( data->real );
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_WaitForEvent( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return data->real->WaitForEvent( data->real );
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_WaitForEventWithTimeout( IDirectFBEventBuffer *thiz,
                                                         unsigned int          seconds,
                                                         unsigned int          milli_seconds )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return data->real->WaitForEventWithTimeout( data->real, seconds, milli_seconds );
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_WakeUp( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return data->real->WakeUp( data->real );
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_GetEvent( IDirectFBEventBuffer *thiz,
                                          DFBEvent             *event )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return data->real->GetEvent( data->real, event );
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_PeekEvent( IDirectFBEventBuffer *thiz,
                                           DFBEvent             *event )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return data->real->PeekEvent( data->real, event );
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_HasEvent( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return data->real->HasEvent( data->real );
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_PostEvent( IDirectFBEventBuffer *thiz,
                                           const DFBEvent       *event )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return data->real->PostEvent( data->real, event );
}

static DFBResult
IDirectFBEventBuffer_Dispatcher_CreateFileDescriptor( IDirectFBEventBuffer *thiz,
                                                      int                  *fd )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Dispatcher)

     return data->real->CreateFileDescriptor( data->real, fd );
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
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

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
     DFBResult        ret;
     VoodooInstanceID instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBEventBuffer_Dispatcher)

     ret = voodoo_manager_register_local( manager, false, thiz, real, Dispatch, &instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     *ret_instance = instance;

     data->real    = real;
     data->self    = instance;
     data->super   = super;
     data->manager = manager;

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

