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
#include <direct/hash.h>
#include <direct/interface.h>
#include <direct/messages.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include <icomacomponent_dispatcher.h>

#include "icoma_requestor.h"


static DirectResult Probe( void );
static DirectResult Construct( IComaComponent   *thiz,
                               VoodooManager    *manager,
                               VoodooInstanceID  instance,
                               void             *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IComaComponent, Requestor )


/**************************************************************************************************/

/*
 * private data struct of IComaComponent_Requestor
 */
typedef struct {
     int                  ref;      /* reference counter */

     VoodooClient        *client;
     VoodooManager       *manager;

     VoodooInstanceID     instance;

     IComa               *coma;

     DirectHash          *listeners;
} IComaComponent_Requestor_data;

/**************************************************************************************************/

static void
IComaComponent_Requestor_Destruct( IComaComponent *thiz )
{
     IComaComponent_Requestor_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     voodoo_manager_request( data->manager, data->instance,
                             ICOMACOMPONENT_METHOD_ID_Release, VREQ_NONE, NULL,
                             VMBT_NONE );

     direct_hash_destroy( data->listeners );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IComaComponent_Requestor_AddRef( IComaComponent *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComaComponent_Requestor)

     data->ref++;

     return DR_OK;
}

static DirectResult
IComaComponent_Requestor_Release( IComaComponent *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComaComponent_Requestor)

     if (--data->ref == 0)
          IComaComponent_Requestor_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IComaComponent_Requestor_Call( IComaComponent  *thiz,
                               ComaMethodID     method,
                               void            *arg,
                               int             *ret_val )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     unsigned int           length;
     IComa_Requestor_data  *coma_data;

     DIRECT_INTERFACE_GET_DATA(IComaComponent_Requestor)

     DIRECT_INTERFACE_GET_DATA_FROM( data->coma, coma_data, IComa_Requestor );

     if (arg) {
          ComaTLS *coma_tls;

          coma_tls = direct_tls_get( coma_data->tlshm_key );
          if (!coma_tls)
               return DR_BUG;

          if (coma_tls->local == arg) {
               length = coma_tls->length;
          }
          else {
               D_UNIMPLEMENTED();
     
               // add code for memory from IComa::Allocate
     
               return DR_UNIMPLEMENTED;
          }
     }
     else {
          length = 0;
     }

     ret = voodoo_manager_request( data->manager, data->instance,
                                   ICOMACOMPONENT_METHOD_ID_Call, VREQ_RESPOND, &response,
                                   VMBT_UINT, method,
                                   VMBT_UINT, length,
                                   VMBT_ODATA, length, arg,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK) {
          VoodooMessageParser parser;
          int                 val;

          VOODOO_PARSER_BEGIN( parser, response );
          if (arg)
               VOODOO_PARSER_READ_DATA( parser, arg, length );
          VOODOO_PARSER_GET_INT( parser, val );
          VOODOO_PARSER_END( parser );

          if (ret_val)
               *ret_val = val;
     }

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

/**********************************************************************************************************************/

typedef struct {
     ComaListenerFunc     func;
     void                *ctx;

     VoodooInstanceID     instance;
} Listener_Dispatcher;

static DirectResult
Listener_Dispatch( void                 *ctx,
                   void                 *real,
                   VoodooManager        *manager,
                   VoodooRequestMessage *msg )
{
     Listener_Dispatcher *dispatcher = ctx;
     const void          *arg;
     VoodooMessageParser  parser;

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, arg );
     VOODOO_PARSER_END( parser );

     dispatcher->func( dispatcher->ctx, (void*) arg );

     return DR_OK;
}

static DirectResult
IComaComponent_Requestor_Listen( IComaComponent     *thiz,
                                 ComaNotificationID  notification,
                                 ComaListenerFunc    func,
                                 void               *ctx )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     Listener_Dispatcher   *dispatcher;

     DIRECT_INTERFACE_GET_DATA(IComaComponent_Requestor)

     dispatcher = D_CALLOC( 1, sizeof(Listener_Dispatcher) );
     if (!dispatcher)
          return D_OOM();

     dispatcher->func = func;
     dispatcher->ctx  = ctx;

     ret = voodoo_manager_register_local( data->manager, VOODOO_INSTANCE_NONE, dispatcher, NULL,
                                          Listener_Dispatch, &dispatcher->instance );
     if (ret)
          goto error;


     ret = voodoo_manager_request( data->manager, data->instance,
                                   ICOMACOMPONENT_METHOD_ID_Listen, VREQ_RESPOND, &response,
                                   VMBT_UINT, notification,
                                   VMBT_ID, dispatcher->instance,
                                   VMBT_NONE );
     if (ret)
          goto error;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     if (ret)
          goto error;

     direct_hash_insert( data->listeners, notification, dispatcher );

     return DR_OK;


error:
     if (dispatcher->instance)
          voodoo_manager_unregister_local( data->manager, dispatcher->instance );

     D_FREE( dispatcher );

     return ret;
}

/**********************************************************************************************************************/

static DirectResult
IComaComponent_Requestor_Unlisten( IComaComponent     *thiz,
                                   ComaNotificationID  notification )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     Listener_Dispatcher   *dispatcher;

     DIRECT_INTERFACE_GET_DATA(IComaComponent_Requestor)

     dispatcher = direct_hash_lookup( data->listeners, notification );
     if (!dispatcher)
          return DR_IDNOTFOUND;


     ret = voodoo_manager_request( data->manager, data->instance,
                                   ICOMACOMPONENT_METHOD_ID_Unlisten, VREQ_RESPOND, &response,
                                   VMBT_UINT, notification,
                                   VMBT_NONE );
     if (ret)
          return ret;

     voodoo_manager_finish_request( data->manager, response );


     voodoo_manager_unregister_local( data->manager, dispatcher->instance );

     direct_hash_remove( data->listeners, notification );

     D_FREE( dispatcher );

     return ret;
}

/**********************************************************************************************************************/

static DirectResult
Probe( void )
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
           VoodooManager    *manager,
           VoodooInstanceID  instance,
           void             *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IComaComponent_Requestor)

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;
     data->coma     = arg;

     direct_hash_create( 23, &data->listeners );

     thiz->AddRef   = IComaComponent_Requestor_AddRef;
     thiz->Release  = IComaComponent_Requestor_Release;
     thiz->Call     = IComaComponent_Requestor_Call;
     thiz->Listen   = IComaComponent_Requestor_Listen;
     thiz->Unlisten = IComaComponent_Requestor_Unlisten;

     return DR_OK;
}

