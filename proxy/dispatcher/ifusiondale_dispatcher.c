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

#include <coma/policy.h>

#include <ifusiondale.h>

#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include "ifusiondale_dispatcher.h"

static DirectResult Probe( void );
static DirectResult Construct( IFusionDale      *thiz,
                               VoodooManager    *manager,
                               VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionDale, Dispatcher )


/**************************************************************************************************/

/*
 * private data struct of IFusionDale_Dispatcher
 */
typedef struct {
     int                    ref;          /* reference counter */

     IFusionDale           *real;

     VoodooInstanceID       self;         /* The instance of this dispatcher itself. */

     VoodooManager         *manager;
} IFusionDale_Dispatcher_data;

/**************************************************************************************************/

static void
IFusionDale_Dispatcher_Destruct( IFusionDale *thiz )
{
     IFusionDale_Dispatcher_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     data->real->Release( data->real );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IFusionDale_Dispatcher_AddRef( IFusionDale *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionDale_Dispatcher)

     data->ref++;

     return DR_OK;
}

static DirectResult
IFusionDale_Dispatcher_Release( IFusionDale *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionDale_Dispatcher)

     if (--data->ref == 0)
          IFusionDale_Dispatcher_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IFusionDale_Dispatcher_EnterComa( IFusionDale  *thiz,
                                  const char   *name,
                                  IComa       **ret_coma )
{
     DIRECT_INTERFACE_GET_DATA(IFusionDale_Dispatcher)

     return DR_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_Release( IFusionDale *thiz, IFusionDale *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IFusionDale_Dispatcher)

     return voodoo_manager_unregister_local( manager, data->self );
}

static DirectResult
Dispatch_EnterComa( IFusionDale *thiz, IFusionDale *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     const char          *name;
     VoodooMessageParser  parser;
     IComa               *coma;
     VoodooInstanceID     instance;

     DIRECT_INTERFACE_GET_DATA(IFusionDale_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_STRING( parser, name );
     VOODOO_PARSER_END( parser );

     if (!coma_policy_check_manager( name ))
          return DR_ACCESSDENIED;

     ret = real->EnterComa( real, name, &coma );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IComa",
                                        coma, data->self, (char*)name, &instance, NULL );
     if (ret) {
          coma->Release( coma );
          return ret;
     }

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DR_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IFusionDale/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IFUSIONDALE_METHOD_ID_Release:
               return Dispatch_Release( dispatcher, real, manager, msg );

          case IFUSIONDALE_METHOD_ID_EnterComa:
               return Dispatch_EnterComa( dispatcher, real, manager, msg );
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
Construct( IFusionDale *thiz, VoodooManager *manager, VoodooInstanceID *ret_instance )
{
     DirectResult      ret;
     IFusionDale      *real;
     VoodooInstanceID  instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionDale_Dispatcher)

     FusionDaleInit( NULL, NULL );

     ret = FusionDaleCreate( &real );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ret = voodoo_manager_register_local( manager, VOODOO_INSTANCE_NONE, thiz, real, Dispatch, &instance );
     if (ret) {
          real->Release( real );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     *ret_instance = instance;

     data->ref     = 1;
     data->real    = real;
     data->self    = instance;
     data->manager = manager;

     thiz->AddRef        = IFusionDale_Dispatcher_AddRef;
     thiz->Release       = IFusionDale_Dispatcher_Release;
     thiz->EnterComa     = IFusionDale_Dispatcher_EnterComa;

     return DR_OK;
}

