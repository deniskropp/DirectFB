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

#include <divine.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>

#include <idivine.h>

#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include "idivine_dispatcher.h"

static DFBResult Probe();
static DFBResult Construct( IDiVine          *thiz,
                            VoodooManager    *manager,
                            VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDiVine, Dispatcher )


/**************************************************************************************************/

/*
 * private data struct of IDiVine_Dispatcher
 */
typedef struct {
     int                    ref;          /* reference counter */

     IDiVine               *real;

     VoodooInstanceID       self;         /* The instance of this dispatcher itself. */
} IDiVine_Dispatcher_data;

/**************************************************************************************************/

static void
IDiVine_Dispatcher_Destruct( IDiVine *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IDiVine_Dispatcher_AddRef( IDiVine *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDiVine_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDiVine_Dispatcher_Release( IDiVine *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDiVine_Dispatcher)

     if (--data->ref == 0)
          IDiVine_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDiVine_Dispatcher_SendEvent( IDiVine             *thiz,
                              const DFBInputEvent *event )
{
     DIRECT_INTERFACE_GET_DATA(IDiVine_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_SendEvent( IDiVine *thiz, IDiVine *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     const DFBInputEvent *event;
     VoodooMessageParser  parser;

     DIRECT_INTERFACE_GET_DATA(IDiVine_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, event );
     VOODOO_PARSER_END( parser );

     ret = real->SendEvent( real, event );

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SendSymbol( IDiVine *thiz, IDiVine *real,
                     VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult            ret;
     DFBInputDeviceKeySymbol symbol;
     VoodooMessageParser     parser;

     DIRECT_INTERFACE_GET_DATA(IDiVine_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, symbol );
     VOODOO_PARSER_END( parser );

     ret = real->SendSymbol( real, symbol );

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDiVine/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIVINE_METHOD_ID_SendEvent:
               return Dispatch_SendEvent( dispatcher, real, manager, msg );

          case IDIVINE_METHOD_ID_SendSymbol:
               return Dispatch_SendSymbol( dispatcher, real, manager, msg );
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

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
static DFBResult
Construct( IDiVine *thiz, VoodooManager *manager, VoodooInstanceID *ret_instance )
{
     DFBResult         ret;
     IDiVine          *real;
     VoodooInstanceID  instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDiVine_Dispatcher)

     ret = DiVineCreate( &real );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ret = voodoo_manager_register_local( manager, true, thiz, real, Dispatch, &instance );
     if (ret) {
          real->Release( real );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     *ret_instance = instance;

     data->ref  = 1;
     data->real = real;
     data->self = instance;

     thiz->AddRef        = IDiVine_Dispatcher_AddRef;
     thiz->Release       = IDiVine_Dispatcher_Release;
     thiz->SendEvent     = IDiVine_Dispatcher_SendEvent;

     return DFB_OK;
}

