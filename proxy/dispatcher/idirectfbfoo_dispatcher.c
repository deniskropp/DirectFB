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

#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include "idirectfbfoo_dispatcher.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBFoo     *thiz,
                            IDirectFBFoo     *real,
                            VoodooManager    *manager,
                            VoodooInstanceID  super,
                            void             *arg,
                            VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBFoo, Dispatcher )


/**************************************************************************************************/

static void
IDirectFBFoo_Dispatcher_Destruct( IDirectFBFoo *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBFoo_Dispatcher_AddRef( IDirectFBFoo *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFoo_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBFoo_Dispatcher_Release( IDirectFBFoo *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFoo_Dispatcher)

     if (--data->ref == 0)
          IDirectFBFoo_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBFoo_Dispatcher_Bar( IDirectFBFoo *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFoo_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_Bar( IDirectFBFoo *thiz, IDirectFBFoo *real,
              VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBFoo_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBFoo/Dispatcher: "
              "Handling request for instance %lu with method %lu...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBFOO_METHOD_ID_Bar:
               return Dispatch_Bar( dispatcher, real, manager, msg );
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
Construct( IDirectFBFoo     *thiz,     /* Dispatcher interface */
           IDirectFBFoo     *real,     /* Real interface implementation */
           VoodooManager    *manager,  /* Manager of the Voodoo framework */
           VoodooInstanceID  super,    /* Instance ID of the super interface */
           void             *arg,      /* Optional arguments to constructor */
           VoodooInstanceID *ret_instance )
{
     DFBResult        ret;
     VoodooInstanceID instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBFoo_Dispatcher)

     D_ASSERT( real != NULL );
     D_ASSERT( manager != NULL );
     D_ASSERT( super != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_instance != NULL );

     /* Register the dispatcher, getting a new instance ID that refers to it. */
     ret = voodoo_manager_register( manager, false, thiz, real, Dispatch, &instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Return the new instance. */
     *ret_instance = instance;

     /* Initialize interface data. */
     data->ref   = 1;
     data->real  = real;
     data->self  = instance;
     data->super = super;

     /* Initialize interface methods. */
     thiz->AddRef  = IDirectFBFoo_Dispatcher_AddRef;
     thiz->Release = IDirectFBFoo_Dispatcher_Release;
     thiz->Bar     = IDirectFBFoo_Dispatcher_Bar;

     return DFB_OK;
}

