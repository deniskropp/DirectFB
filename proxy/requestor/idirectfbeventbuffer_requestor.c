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
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/manager.h>

#include <idirectfbeventbuffer_dispatcher.h>

#include "idirectfbeventbuffer_requestor.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBEventBuffer *thiz,
                            VoodooManager        *manager,
                            VoodooInstanceID      instance,
                            void                 *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBEventBuffer, Requestor )


/**************************************************************************************************/

static void *feed_thread( DirectThread *thread, void *arg );

/**************************************************************************************************/

static void
IDirectFBEventBuffer_Requestor_Destruct( IDirectFBEventBuffer *thiz )
{
     IDirectFBEventBuffer_Requestor_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     data->stop = true;

     data->src->WakeUp( data->src );

     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBEventBuffer_Requestor_AddRef( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBEventBuffer_Requestor_Release( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     if (--data->ref == 0)
          IDirectFBEventBuffer_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBEventBuffer_Requestor_Reset( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Requestor_WaitForEvent( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Requestor_WaitForEventWithTimeout( IDirectFBEventBuffer *thiz,
                                                        unsigned int          seconds,
                                                        unsigned int          milli_seconds )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Requestor_WakeUp( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Requestor_GetEvent( IDirectFBEventBuffer *thiz,
                                         DFBEvent             *event )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Requestor_PeekEvent( IDirectFBEventBuffer *thiz,
                                          DFBEvent             *event )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Requestor_HasEvent( IDirectFBEventBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBEventBuffer_Requestor_PostEvent( IDirectFBEventBuffer *thiz,
                                          const DFBEvent       *event )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     if (!event)
          return DFB_INVARG;

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBEVENTBUFFER_METHOD_ID_PostEvent, VREQ_NONE, NULL,
                                    VMBT_DATA, sizeof(DFBEvent), event,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBEventBuffer_Requestor_CreateFileDescriptor( IDirectFBEventBuffer *thiz,
                                                     int                  *fd )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBEventBuffer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
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
           VoodooManager        *manager,
           VoodooInstanceID      instance,
           void                 *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBEventBuffer_Requestor)

     voodoo_manager_register_remote( manager, false, thiz, instance );

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;

     data->src      = arg;
     data->dst      = thiz;

     thiz->AddRef                  = IDirectFBEventBuffer_Requestor_AddRef;
     thiz->Release                 = IDirectFBEventBuffer_Requestor_Release;
     thiz->Reset                   = IDirectFBEventBuffer_Requestor_Reset;
     thiz->WaitForEvent            = IDirectFBEventBuffer_Requestor_WaitForEvent;
     thiz->WaitForEventWithTimeout = IDirectFBEventBuffer_Requestor_WaitForEventWithTimeout;
     thiz->GetEvent                = IDirectFBEventBuffer_Requestor_GetEvent;
     thiz->PeekEvent               = IDirectFBEventBuffer_Requestor_PeekEvent;
     thiz->HasEvent                = IDirectFBEventBuffer_Requestor_HasEvent;
     thiz->PostEvent               = IDirectFBEventBuffer_Requestor_PostEvent;
     thiz->WakeUp                  = IDirectFBEventBuffer_Requestor_WakeUp;
     thiz->CreateFileDescriptor    = IDirectFBEventBuffer_Requestor_CreateFileDescriptor;

     data->thread = direct_thread_create( DTT_INPUT, feed_thread, data, "Event Feed" );

     return DFB_OK;
}

/**************************************************************************************************/

static void *
feed_thread( DirectThread *thread, void *arg )
{
     DFBResult                            ret;
     IDirectFBEventBuffer_Requestor_data *data = arg;
     IDirectFBEventBuffer                *src  = data->src;
     IDirectFBEventBuffer                *dst  = data->dst;

     while (!data->stop) {
          DFBEvent event;

          ret = src->WaitForEvent( src );
          if (ret) {
               if (ret == DFB_INTERRUPTED)
                    continue;

               DirectFBError( "IDirectFBEventBuffer::WaitForEvent", ret );
               return NULL;
          }

          if (data->stop)
               return NULL;

          while (src->GetEvent( src, &event ) == DFB_OK) {
               ret = dst->PostEvent( dst, &event );
               if (ret) {
                    DirectFBError( "IDirectFBEventBuffer::PostEvent", ret );
                    return NULL;
               }

               if (data->stop)
                    return NULL;
          }
     }

     return NULL;
}

