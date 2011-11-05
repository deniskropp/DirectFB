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

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include <idivine_dispatcher.h>


static DFBResult Probe();
static DFBResult Construct( IDiVine *thiz, const char *host, int session );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDiVine, Requestor )


/**************************************************************************************************/

/*
 * private data struct of IDiVine_Requestor
 */
typedef struct {
     int                  ref;      /* reference counter */

     VoodooClient        *client;
     VoodooManager       *manager;

     VoodooInstanceID     instance;
} IDiVine_Requestor_data;

/**************************************************************************************************/

static void
IDiVine_Requestor_Destruct( IDiVine *thiz )
{
     IDiVine_Requestor_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     voodoo_manager_request( data->manager, data->instance,
                             IDIVINE_METHOD_ID_Release, VREQ_NONE, NULL,
                             VMBT_NONE );

     voodoo_client_destroy( data->client );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IDiVine_Requestor_AddRef( IDiVine *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDiVine_Requestor)

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDiVine_Requestor_Release( IDiVine *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDiVine_Requestor)

     if (--data->ref == 0)
          IDiVine_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDiVine_Requestor_SendEvent( IDiVine             *thiz,
                             const DFBInputEvent *event )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDiVine_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIVINE_METHOD_ID_SendEvent, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(DFBInputEvent), event,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDiVine_Requestor_SendSymbol( IDiVine                 *thiz,
                              DFBInputDeviceKeySymbol  symbol )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDiVine_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIVINE_METHOD_ID_SendSymbol, VREQ_RESPOND, &response,
                                   VMBT_UINT, symbol,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
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
Construct( IDiVine *thiz, const char *host, int session )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDiVine_Requestor)

     data->ref = 1;

     ret = voodoo_client_create( host, session, &data->client );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->manager = voodoo_client_manager( data->client );

     ret = voodoo_manager_super( data->manager, "IDiVine", &data->instance );
     if (ret) {
          voodoo_client_destroy( data->client );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     thiz->AddRef        = IDiVine_Requestor_AddRef;
     thiz->Release       = IDiVine_Requestor_Release;
     thiz->SendEvent     = IDiVine_Requestor_SendEvent;
     thiz->SendSymbol    = IDiVine_Requestor_SendSymbol;

     return DFB_OK;
}

