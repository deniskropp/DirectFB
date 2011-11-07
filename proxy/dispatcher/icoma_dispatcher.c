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
#include <coma/policy.h>

#include <coma/icoma.h>

#include "icoma_dispatcher.h"
#include "icomacomponent_dispatcher.h"

static DirectResult Probe( void );
static DirectResult Construct( IComa            *thiz,
                               IComa            *real,
                               VoodooManager    *manager,
                               VoodooInstanceID  super,
                               void             *arg,      /* Optional arguments to constructor */
                               VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IComa, Dispatcher )


/**************************************************************************************************/

/*
 * private data struct of IComa_Dispatcher
 */
typedef struct {
     int                    ref;          /* reference counter */

     IComa                 *real;

     VoodooInstanceID       super;
     VoodooInstanceID       self;

     VoodooManager         *manager;

     char                  *name;
} IComa_Dispatcher_data;

/**************************************************************************************************/

static void
IComa_Dispatcher_Destruct( IComa *thiz )
{
     IComa_Dispatcher_data *data;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     data = thiz->priv;

     D_FREE( data->name );

     voodoo_manager_unregister_local( data->manager, data->self );

     data->real->Release( data->real );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IComa_Dispatcher_AddRef( IComa *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComa_Dispatcher)

     data->ref++;

     return DR_OK;
}

static DirectResult
IComa_Dispatcher_Release( IComa *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IComa_Dispatcher)

     if (--data->ref == 0)
          IComa_Dispatcher_Destruct( thiz );

     return DR_OK;
}

/**************************************************************************************************/

static DirectResult
Dispatch_GetComponent( IComa *thiz, IComa *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                    ret;
     const char                     *name;
     unsigned int                    timeout;
     VoodooMessageParser             parser;
     IComaComponent                 *component;
     VoodooInstanceID                instance;
     IComaComponent_Dispatcher_args  args;

     DIRECT_INTERFACE_GET_DATA(IComa_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_STRING( parser, name );
     VOODOO_PARSER_GET_UINT( parser, timeout );
     VOODOO_PARSER_END( parser );

     if (!coma_policy_check_component( data->name, name ))
          return DR_ACCESSDENIED;

     ret = real->GetComponent( real, name, timeout, &component );
     if (ret)
          return ret;

     args.coma           = real;
     args.manager_name   = data->name;
     args.component_name = name;

     ret = voodoo_construct_dispatcher( manager, "IComaComponent",
                                        component, data->self, &args, &instance, NULL );
     if (ret) {
          component->Release( component );
          return ret;
     }

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DR_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IComa/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case ICOMA_METHOD_ID_GetComponent:
               return Dispatch_GetComponent( dispatcher, real, manager, msg );
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
Construct( IComa            *thiz,
           IComa            *real,
           VoodooManager    *manager,
           VoodooInstanceID  super,
           void             *arg,      /* Optional arguments to constructor */
           VoodooInstanceID *ret_instance )
{
     DirectResult      ret;
     const char       *name = arg;
     VoodooInstanceID  instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IComa_Dispatcher)

     ret = voodoo_manager_register_local( manager, super, thiz, real, Dispatch, &instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref     = 1;
     data->real    = real;
     data->super   = super;
     data->self    = instance;
     data->manager = manager;
     data->name    = D_STRDUP( name );

     thiz->AddRef  = IComa_Dispatcher_AddRef;
     thiz->Release = IComa_Dispatcher_Release;

     *ret_instance = instance;

     return DR_OK;
}

