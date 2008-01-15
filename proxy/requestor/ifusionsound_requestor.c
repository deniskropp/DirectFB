/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fusionsound.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include <media/ifusionsoundmusicprovider.h>

#include <ifusionsound_dispatcher.h>

static DFBResult Probe();
static DFBResult Construct( IFusionSound *thiz, const char *host, int session );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSound, Requestor )


/**************************************************************************************************/

/*
 * private data struct of IFusionSound_Requestor
 */
typedef struct {
     int                  ref;      /* reference counter */

     VoodooClient        *client;
     VoodooManager       *manager;

     VoodooInstanceID     instance;
} IFusionSound_Requestor_data;

/**************************************************************************************************/

static void
IFusionSound_Requestor_Destruct( IFusionSound *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IFusionSound_Requestor_AddRef( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSound_Requestor_Release( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)

     if (--data->ref == 0)
          IFusionSound_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSound_Requestor_GetDeviceDescription( IFusionSound        *thiz,
                                             FSDeviceDescription *desc )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUND_METHOD_ID_GetDeviceDescription,
                                   VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK) {
          VoodooMessageParser parser;

          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_READ_DATA( parser, desc, sizeof(FSDeviceDescription) );
          VOODOO_PARSER_END( parser );
     }

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSound_Requestor_CreateBuffer( IFusionSound               *thiz,
                                     const FSBufferDescription  *desc,
                                     IFusionSoundBuffer        **ret_interface )
{
     DirectResult               ret;
     VoodooResponseMessage     *response;
     VoodooMessageParser        parser;
     void                      *interface = NULL;
     const FSBufferDescription *dsc;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)
     
     if (!desc || !ret_interface)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUND_METHOD_ID_CreateBuffer, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(FSBufferDescription), desc,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK) {
          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_GET_DATA( parser, dsc );
          VOODOO_PARSER_END( parser );
          
          ret = voodoo_construct_requestor( data->manager, "IFusionSoundBuffer",
                                            response->instance, (void*)dsc, &interface );
     }

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;

     return ret;
}

static DFBResult
IFusionSound_Requestor_CreateStream( IFusionSound               *thiz,
                                     const FSStreamDescription  *desc,
                                     IFusionSoundStream        **ret_interface )
{
     DirectResult               ret;
     VoodooResponseMessage     *response;
     VoodooMessageParser        parser;
     void                      *interface = NULL;
     const FSStreamDescription *dsc;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)
     
     if (!ret_interface)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUND_METHOD_ID_CreateStream, VREQ_RESPOND, &response,
                                   VMBT_ODATA, sizeof(FSStreamDescription), desc,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK) {
          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_GET_DATA( parser, dsc );
          VOODOO_PARSER_END( parser );
          
          ret = voodoo_construct_requestor( data->manager, "IFusionSoundStream",
                                            response->instance, (void*)dsc, &interface );
     }

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;

     return ret;
}

static DFBResult
IFusionSound_Requestor_CreateMusicProvider( IFusionSound               *thiz,
                                            const char                 *filename,
                                            IFusionSoundMusicProvider **ret_interface )
{
#if 0
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)
     
     if (!filename || !ret_interface)
          return DFB_INVARG;
         
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUND_METHOD_ID_CreateMusicProvider,
                                   VREQ_RESPOND, &response,
                                   VMBT_STRING, filename,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK)
          ret = voodoo_construct_requestor( data->manager, "IFusionSoundMusicProvider",
                                            response->instance, NULL, &interface );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;
     
     return ret;
#else
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)

     /* Check arguments */
     if (!filename || !ret_interface)
          return DFB_INVARG;

     return IFusionSoundMusicProvider_Create( filename, ret_interface );
#endif
}

static DFBResult
IFusionSound_Requestor_GetMasterVolume( IFusionSound *thiz, 
                                        float        *level )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)
     
     if (!level)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUND_METHOD_ID_GetMasterVolume, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_READ_DATA( parser, level, sizeof(float) );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSound_Requestor_SetMasterVolume( IFusionSound *thiz, 
                                        float         level )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)
     
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUND_METHOD_ID_SetMasterVolume, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(float), &level,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
} 

static DFBResult
IFusionSound_Requestor_GetLocalVolume( IFusionSound *thiz, 
                                       float        *level )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)
     
     if (!level)
          return DFB_INVARG;
          
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUND_METHOD_ID_GetLocalVolume, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_READ_DATA( parser, level, sizeof(float) );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSound_Requestor_SetLocalVolume( IFusionSound *thiz, 
                                       float         level )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     
     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)
     
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUND_METHOD_ID_SetLocalVolume, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(float), &level,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSound_Requestor_Suspend( IFusionSound *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUND_METHOD_ID_Suspend, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IFusionSound_Requestor_Resume( IFusionSound *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IFusionSound_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IFUSIONSOUND_METHOD_ID_Resume, VREQ_RESPOND, &response,
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
Construct( IFusionSound *thiz, const char *host, int session )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSound_Requestor)

     data->ref = 1;

     ret = voodoo_client_create( host, session, &data->client );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->manager = voodoo_client_manager( data->client );

     ret = voodoo_manager_super( data->manager, "IFusionSound", &data->instance );
     if (ret) {
          voodoo_client_destroy( data->client );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }
     
     thiz->AddRef               = IFusionSound_Requestor_AddRef;
     thiz->Release              = IFusionSound_Requestor_Release;
     thiz->GetDeviceDescription = IFusionSound_Requestor_GetDeviceDescription;
     thiz->CreateBuffer         = IFusionSound_Requestor_CreateBuffer;
     thiz->CreateStream         = IFusionSound_Requestor_CreateStream;
     thiz->CreateMusicProvider  = IFusionSound_Requestor_CreateMusicProvider;
     thiz->GetMasterVolume      = IFusionSound_Requestor_GetMasterVolume;
     thiz->SetMasterVolume      = IFusionSound_Requestor_SetMasterVolume;
     thiz->GetLocalVolume       = IFusionSound_Requestor_GetLocalVolume;
     thiz->SetLocalVolume       = IFusionSound_Requestor_SetLocalVolume;
     thiz->Suspend              = IFusionSound_Requestor_Suspend;
     thiz->Resume               = IFusionSound_Requestor_Resume;

     return DFB_OK;
}

     
