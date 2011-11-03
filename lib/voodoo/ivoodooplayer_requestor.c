/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>

#include <voodoo/ivoodooplayer.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include "ivoodooplayer_dispatcher.h"


static DirectResult Probe( void );
static DirectResult Construct( IVoodooPlayer *thiz, const char *host, int session );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IVoodooPlayer, Requestor )


/**************************************************************************************************/

/*
 * private data struct of IVoodooPlayer_Requestor
 */
typedef struct {
     int                  ref;      /* reference counter */

     VoodooClient        *client;
     VoodooManager       *manager;

     VoodooInstanceID     instance;
} IVoodooPlayer_Requestor_data;

/**************************************************************************************************/

static void
IVoodooPlayer_Requestor_Destruct( IVoodooPlayer *thiz )
{
     IVoodooPlayer_Requestor_data *data = thiz->priv;

	D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     voodoo_client_destroy( data->client );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IVoodooPlayer_Requestor_AddRef( IVoodooPlayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Requestor)

     data->ref++;

     return DR_OK;
}

static DirectResult
IVoodooPlayer_Requestor_Release( IVoodooPlayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Requestor)

     if (--data->ref == 0)
          IVoodooPlayer_Requestor_Destruct( thiz );

     return DR_OK;
}

static DirectResult
IVoodooPlayer_Requestor_GetApps( IVoodooPlayer        *thiz,
                                 unsigned int          max_num,
                                 unsigned int         *ret_num,
                                 VoodooAppDescription *ret_applications )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     if (!max_num || !ret_num || !ret_applications)
          return DR_INVARG;

     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IVOODOOPLAYER_METHOD_ID_GetApps, VREQ_RESPOND, &response,
                                   VMBT_UINT, max_num,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK) {
          VoodooMessageParser parser;
          unsigned int        num;

          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_GET_UINT( parser, num );

          if (num > max_num)
               num = max_num;

          *ret_num = num;

          if (num > 0)
               VOODOO_PARSER_READ_DATA( parser, ret_applications, num * sizeof(VoodooAppDescription) );

          VOODOO_PARSER_END( parser );
     }

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DirectResult
IVoodooPlayer_Requestor_LaunchApp( IVoodooPlayer *thiz,
                                   const u8       app_uuid[16],
                                   const u8       player_uuid[16],
                                   u8             ret_instance_uuid[16] )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     if (!app_uuid || !player_uuid || !ret_instance_uuid)
          return DR_INVARG;

     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IVOODOOPLAYER_METHOD_ID_LaunchApp, VREQ_RESPOND, &response,
                                   VMBT_DATA, 16, app_uuid,
                                   VMBT_DATA, 16, player_uuid,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK) {
          VoodooMessageParser parser;

          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_READ_DATA( parser, ret_instance_uuid, 16 );
          VOODOO_PARSER_END( parser );
     }

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DirectResult
IVoodooPlayer_Requestor_StopInstance( IVoodooPlayer *thiz,
                                      const u8       instance_uuid[16] )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     if (!instance_uuid)
          return DR_INVARG;

     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IVOODOOPLAYER_METHOD_ID_StopInstance, VREQ_RESPOND, &response,
                                   VMBT_DATA, 16, instance_uuid,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DirectResult
IVoodooPlayer_Requestor_WaitInstance( IVoodooPlayer *thiz,
                                      const u8       instance_uuid[16] )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     if (!instance_uuid)
          return DR_INVARG;

     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IVOODOOPLAYER_METHOD_ID_WaitInstance, VREQ_RESPOND, &response,
                                   VMBT_DATA, 16, instance_uuid,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DirectResult
IVoodooPlayer_Requestor_GetInstances( IVoodooPlayer                *thiz,
                                      unsigned int                  max_num,
                                      unsigned int                 *ret_num,
                                      VoodooAppInstanceDescription *ret_instances )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     if (!max_num || !ret_num || !ret_instances)
          return DR_INVARG;

     DIRECT_INTERFACE_GET_DATA(IVoodooPlayer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IVOODOOPLAYER_METHOD_ID_GetInstances, VREQ_RESPOND, &response,
                                   VMBT_UINT, max_num,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK) {
          VoodooMessageParser parser;
          unsigned int        num;

          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_GET_UINT( parser, num );

          if (num > max_num)
               num = max_num;

          *ret_num = num;

          if (num > 0)
               VOODOO_PARSER_READ_DATA( parser, ret_instances, num * sizeof(VoodooAppInstanceDescription) );

          VOODOO_PARSER_END( parser );
     }

     voodoo_manager_finish_request( data->manager, response );

     return ret;
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
Construct( IVoodooPlayer *thiz, const char *host, int session )
{
     DirectResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IVoodooPlayer_Requestor)

     data->ref = 1;

     ret = voodoo_client_create( host, session, &data->client );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->manager = voodoo_client_manager( data->client );

     ret = voodoo_manager_super( data->manager, "IVoodooPlayer", &data->instance );
     if (ret) {
          voodoo_client_destroy( data->client );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     thiz->AddRef       = IVoodooPlayer_Requestor_AddRef;
     thiz->Release      = IVoodooPlayer_Requestor_Release;
     thiz->GetApps      = IVoodooPlayer_Requestor_GetApps;
     thiz->LaunchApp    = IVoodooPlayer_Requestor_LaunchApp;
     thiz->StopInstance = IVoodooPlayer_Requestor_StopInstance;
     thiz->WaitInstance = IVoodooPlayer_Requestor_WaitInstance;
     thiz->GetInstances = IVoodooPlayer_Requestor_GetInstances;

     return DR_OK;
}

