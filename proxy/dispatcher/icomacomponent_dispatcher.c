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

#include <fusiondale.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>

#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include <coma/coma.h>
#include <coma/component.h>
#include <coma/policy.h>

#include <coma/icoma.h>
#include <coma/icomacomponent.h>

#include "icomacomponent_dispatcher.h"

static DirectResult Probe( void );
static DirectResult Construct( IComaComponent   *thiz,
                               IComaComponent   *real,
                               VoodooManager    *manager,
                               VoodooInstanceID  super,
                               void             *arg,      /* Optional arguments to constructor */
                               VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IComaComponent, Dispatcher )


/**************************************************************************************************/

/*
 * private data struct of IComaComponent_Dispatcher
 */
typedef struct {
     int                    ref;          /* reference counter */

     IComaComponent        *real;

     VoodooInstanceID       super;
     VoodooInstanceID       self;

     VoodooManager         *manager;

     IComa                 *coma;

     DirectHash            *listeners;

     char                  *coma_name;
     char                  *component_name;
} IComaComponent_Dispatcher_data;

/**************************************************************************************************/

static void
IComaComponent_Dispatcher_Destruct( IComaComponent *thiz )
{
     IComaComponent_Dispatcher_data *data;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     data = thiz->priv;

     D_FREE( data->component_name );
     D_FREE( data->coma_name );

     direct_hash_destroy( data->listeners );

     voodoo_manager_unregister_local( data->manager, data->self );

     data->real->Release( data->real );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IComaComponent_Dispatcher_AddRef( IComaComponent *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComaComponent_Dispatcher)

     data->ref++;

     return DR_OK;
}

static DirectResult
IComaComponent_Dispatcher_Release( IComaComponent *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComaComponent_Dispatcher)

     if (--data->ref == 0)
          IComaComponent_Dispatcher_Destruct( thiz );

     return DR_OK;
}

/**************************************************************************************************/

static DirectResult
Dispatch_Call( IComaComponent *thiz, IComaComponent *real,
               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     ComaMethodID         method;
     unsigned int         length;
     void                *arg;
     int                  ret_val;
     VoodooMessageParser  parser;
     IComa               *coma;

     DIRECT_INTERFACE_GET_DATA(IComaComponent_Dispatcher)

     coma = data->coma;

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, method );
     VOODOO_PARSER_GET_UINT( parser, length );

     if (!coma_policy_check_method( data->coma_name, data->component_name, method ))
          return DR_ACCESSDENIED;

     if (length) {
          ret = coma->GetLocal( coma, length, &arg );
          if (ret) {
               VOODOO_PARSER_END( parser );
               return ret;
          }

          VOODOO_PARSER_READ_DATA( parser, arg, length );
     }
     else
          arg = NULL;

     VOODOO_PARSER_END( parser );

     ret = real->Call( real, method, arg, &ret_val );
     if (ret)
          return ret;

     if (arg)
          return voodoo_manager_respond( manager, true, msg->header.serial,
                                         DR_OK, VOODOO_INSTANCE_NONE,
                                         VMBT_DATA, length, arg,
                                         VMBT_INT, ret_val,
                                         VMBT_NONE );

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DR_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, ret_val,
                                    VMBT_NONE );
}

/**********************************************************************************************************************/

typedef struct {
     VoodooManager    *manager;
     VoodooInstanceID  instance;

     IComa            *coma;
} Listener_Requestor;

static void
Listener_Request( void *ctx,
                  void *arg )
{
     DirectResult        ret;
     Listener_Requestor *requestor = ctx;
     IComa_data         *coma_data;
     int                 size;

     coma_data = (IComa_data *) requestor->coma->priv;

     ret = coma_allocation_size( coma_data->coma, arg, &size );
     if (ret) {
          D_DERROR( ret, "IComaComponent_Dispatcher/Listener_Request: Could not lookup allocation size!\n" );
          return;
     }

     ret = voodoo_manager_request( requestor->manager, requestor->instance, 0, VREQ_NONE, NULL,
                                   VMBT_DATA, size, arg,
                                   VMBT_NONE );
     if (ret)
          D_DERROR( ret, "IComaComponent_Dispatcher/Listener_Request: Request failed!\n" );
}

static DirectResult
Dispatch_Listen( IComaComponent *thiz, IComaComponent *real,
                 VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     ComaNotificationID   notification;
     VoodooInstanceID     instance;
     VoodooMessageParser  parser;
     Listener_Requestor  *requestor;

     DIRECT_INTERFACE_GET_DATA(IComaComponent_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, notification );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     if (!coma_policy_check_notification( data->coma_name, data->component_name, notification ))
          return DR_ACCESSDENIED;

     requestor = D_CALLOC( 1, sizeof(Listener_Requestor) );
     if (!requestor)
          return D_OOM();

     requestor->manager  = data->manager;
     requestor->instance = instance;
     requestor->coma     = data->coma;

     ret = real->Listen( real, notification, Listener_Request, requestor );
     if (ret) {
          D_FREE( requestor );
          return ret;
     }

     direct_hash_insert( data->listeners, notification, requestor );

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DR_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_Unlisten( IComaComponent *thiz, IComaComponent *real,
                   VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     ComaNotificationID   notification;
     VoodooMessageParser  parser;
     Listener_Requestor  *requestor;

     DIRECT_INTERFACE_GET_DATA(IComaComponent_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, notification );
     VOODOO_PARSER_END( parser );

     requestor = direct_hash_lookup( data->listeners, notification );
     if (!requestor)
          return DR_IDNOTFOUND;

     ret = real->Unlisten( real, notification );
     if (ret) {
          D_DERROR( ret, "IComaComponent_Dispatcher: Unlisten failed!\n" );
          return ret;
     }

     direct_hash_remove( data->listeners, notification );

     D_FREE( requestor );

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DR_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

/**********************************************************************************************************************/

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IComaComponent/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case ICOMACOMPONENT_METHOD_ID_Call:
               return Dispatch_Call( dispatcher, real, manager, msg );

          case ICOMACOMPONENT_METHOD_ID_Listen:
               return Dispatch_Listen( dispatcher, real, manager, msg );

          case ICOMACOMPONENT_METHOD_ID_Unlisten:
               return Dispatch_Unlisten( dispatcher, real, manager, msg );
     }

     return DR_NOSUCHMETHOD;
}

/**************************************************************************************************/

static DirectResult
Probe()
{
     /* This implementation has to be loaded explicitly. */
     return DR_UNSUPPORTED;
}

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
static DirectResult
Construct( IComaComponent   *thiz,
           IComaComponent   *real,
           VoodooManager    *manager,
           VoodooInstanceID  super,
           void             *arg,      /* Optional arguments to constructor */
           VoodooInstanceID *ret_instance )
{
     DirectResult     ret;
     VoodooInstanceID instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IComaComponent_Dispatcher)

     ret = voodoo_manager_register_local( manager, false, thiz, real, Dispatch, &instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     IComaComponent_data *real_data = real->priv;

     data->ref     = 1;
     data->real    = real;
     data->super   = super;
     data->self    = instance;
     data->manager = manager;
     data->coma    = arg;

     data->coma_name      = D_STRDUP( real_data->coma->name );
     data->component_name = D_STRDUP( real_data->component->name );

     ret = direct_hash_create( 17, &data->listeners );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     thiz->AddRef  = IComaComponent_Dispatcher_AddRef;
     thiz->Release = IComaComponent_Dispatcher_Release;

     *ret_instance = instance;

     return DR_OK;
}

