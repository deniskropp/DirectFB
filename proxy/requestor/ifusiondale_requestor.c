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

#include <ifusiondale.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include <ifusiondale_dispatcher.h>


static DirectResult Probe( void );
static DirectResult Construct( IFusionDale *thiz, const char *host, int session );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionDale, Requestor )


/**************************************************************************************************/

/*
 * private data struct of IFusionDale_Requestor
 */
typedef struct {
     int                  ref;      /* reference counter */

     VoodooClient        *client;
     VoodooManager       *manager;

     VoodooInstanceID     instance;
} IFusionDale_Requestor_data;

/**************************************************************************************************/

static void
IFusionDale_Requestor_Destruct( IFusionDale *thiz )
{
     IFusionDale_Requestor_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     voodoo_manager_request( data->manager, data->instance,
                             IFUSIONDALE_METHOD_ID_Release, VREQ_NONE, NULL,
                             VMBT_NONE );

     voodoo_client_destroy( data->client );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IFusionDale_Requestor_AddRef( IFusionDale *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionDale_Requestor)

     data->ref++;

     return DR_OK;
}

static DirectResult
IFusionDale_Requestor_Release( IFusionDale *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionDale_Requestor)

     if (--data->ref == 0)
          IFusionDale_Requestor_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IFusionDale_Requestor_EnterComa( IFusionDale  *thiz,
                                 const char   *name,
                                 IComa       **ret_coma )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface_ptr = NULL;

     DIRECT_INTERFACE_GET_DATA(IFusionDale_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONDALE_METHOD_ID_EnterComa, VREQ_RESPOND, &response,
                                   VMBT_STRING, name,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK)
          ret = voodoo_construct_requestor( data->manager, "IComa",
                                            response->instance, NULL, &interface_ptr );

     voodoo_manager_finish_request( data->manager, response );

     *ret_coma = interface_ptr;

     return ret;
}

/**************************************************************************************************/

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
Construct( IFusionDale *thiz, const char *host, int session )
{
     DirectResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionDale_Requestor)

     data->ref = 1;

     ret = voodoo_client_create( host, session, &data->client );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->manager = voodoo_client_manager( data->client );

     ret = voodoo_manager_super( data->manager, "IFusionDale", &data->instance );
     if (ret) {
          voodoo_client_destroy( data->client );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     thiz->AddRef        = IFusionDale_Requestor_AddRef;
     thiz->Release       = IFusionDale_Requestor_Release;
     thiz->EnterComa     = IFusionDale_Requestor_EnterComa;

     return DR_OK;
}

