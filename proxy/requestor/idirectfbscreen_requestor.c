/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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
#include <unistd.h>

#include <string.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>

#include <voodoo/manager.h>
#include <voodoo/message.h>

#include <idirectfbscreen_dispatcher.h>


static DFBResult Probe();
static DFBResult Construct( IDirectFBScreen  *thiz,
                            VoodooManager    *manager,
                            VoodooInstanceID  instance,
                            void             *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBScreen, Requestor )

/**************************************************************************************************/

/*
 * private data struct of IDirectFBScreen_Requestor
 */
typedef struct {
     int                    ref;      /* reference counter */

     VoodooManager         *manager;
     VoodooInstanceID       instance;
} IDirectFBScreen_Requestor_data;

/**************************************************************************************************/

static void
IDirectFBScreen_Requestor_Destruct( IDirectFBScreen *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBScreen_Requestor_AddRef( IDirectFBScreen *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_Requestor_Release( IDirectFBScreen *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (--data->ref == 0)
          IDirectFBScreen_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_Requestor_GetID( IDirectFBScreen *thiz,
                                 DFBScreenID     *ret_id )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     DFBScreenID            id;

     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!ret_id)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBSCREEN_METHOD_ID_GetID, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_ID( parser, id );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_id = id;

     return ret;
}

static DFBResult
IDirectFBScreen_Requestor_GetDescription( IDirectFBScreen      *thiz,
                                          DFBScreenDescription *ret_desc )
{
     DirectResult                ret;
     VoodooResponseMessage      *response;
     VoodooMessageParser         parser;
     const DFBScreenDescription *desc; 

     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!ret_desc)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBSCREEN_METHOD_ID_GetDescription,
                                   VREQ_RESPOND, &response, VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_DATA( parser, desc );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_desc = *desc;

     return ret;
}

static DFBResult
IDirectFBScreen_Requestor_GetSize( IDirectFBScreen *thiz,
                                   int             *width,
                                   int             *height )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     const DFBDimension    *size; 

     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!width && !height)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBSCREEN_METHOD_ID_GetSize,
                                   VREQ_RESPOND, &response, VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_DATA( parser, size );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     if (width)
          *width = size->w;
          
     if (height)
          *height = size->h;

     return ret;
}

static DFBResult
IDirectFBScreen_Requestor_EnumDisplayLayers( IDirectFBScreen         *thiz,
                                   DFBDisplayLayerCallback  callbackfunc,
                                   void                    *callbackdata )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!callbackfunc)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_SetPowerMode( IDirectFBScreen    *thiz,
                                        DFBScreenPowerMode  mode )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBSCREEN_METHOD_ID_SetPowerMode, VREQ_NONE, NULL,
                                    VMBT_INT, mode,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBScreen_Requestor_WaitForSync( IDirectFBScreen *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBSCREEN_METHOD_ID_WaitForSync, VREQ_NONE, NULL,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBScreen_Requestor_GetMixerDescriptions( IDirectFBScreen           *thiz,
                                      DFBScreenMixerDescription *descriptions )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!descriptions)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_GetMixerConfiguration( IDirectFBScreen      *thiz,
                                       int                   mixer,
                                       DFBScreenMixerConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!config)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_TestMixerConfiguration( IDirectFBScreen            *thiz,
                                        int                         mixer,
                                        const DFBScreenMixerConfig *config,
                                        DFBScreenMixerConfigFlags  *failed )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!config || (config->flags & ~DSMCONF_ALL))
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_SetMixerConfiguration( IDirectFBScreen            *thiz,
                                       int                         mixer,
                                       const DFBScreenMixerConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!config || (config->flags & ~DSMCONF_ALL))
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_GetEncoderDescriptions( IDirectFBScreen             *thiz,
                                        DFBScreenEncoderDescription *descriptions )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!descriptions)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_GetEncoderConfiguration( IDirectFBScreen        *thiz,
                                         int                     encoder,
                                         DFBScreenEncoderConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!config)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_TestEncoderConfiguration( IDirectFBScreen              *thiz,
                                          int                           encoder,
                                          const DFBScreenEncoderConfig *config,
                                          DFBScreenEncoderConfigFlags  *failed )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!config || (config->flags & ~DSECONF_ALL))
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_SetEncoderConfiguration( IDirectFBScreen              *thiz,
                                         int                           encoder,
                                         const DFBScreenEncoderConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!config || (config->flags & ~DSECONF_ALL))
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_GetOutputDescriptions( IDirectFBScreen            *thiz,
                                       DFBScreenOutputDescription *descriptions )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!descriptions)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_GetOutputConfiguration( IDirectFBScreen       *thiz,
                                        int                    output,
                                        DFBScreenOutputConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!config)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_TestOutputConfiguration( IDirectFBScreen             *thiz,
                                         int                          output,
                                         const DFBScreenOutputConfig *config,
                                         DFBScreenOutputConfigFlags  *failed )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!config || (config->flags & ~DSOCONF_ALL))
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Requestor_SetOutputConfiguration( IDirectFBScreen             *thiz,
                                        int                          output,
                                        const DFBScreenOutputConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Requestor)

     if (!config || (config->flags & ~DSOCONF_ALL))
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DFBResult
Probe()
{
     /* This implementation has to be loaded explicitly. */
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBScreen  *thiz,
           VoodooManager    *manager,
           VoodooInstanceID  instance,
           void             *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBScreen_Requestor)

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;

     thiz->AddRef                   = IDirectFBScreen_Requestor_AddRef;
     thiz->Release                  = IDirectFBScreen_Requestor_Release;
     thiz->GetID                    = IDirectFBScreen_Requestor_GetID;
     thiz->GetDescription           = IDirectFBScreen_Requestor_GetDescription;
     thiz->GetSize                  = IDirectFBScreen_Requestor_GetSize;
     thiz->EnumDisplayLayers        = IDirectFBScreen_Requestor_EnumDisplayLayers;
     thiz->SetPowerMode             = IDirectFBScreen_Requestor_SetPowerMode;
     thiz->WaitForSync              = IDirectFBScreen_Requestor_WaitForSync;
     thiz->GetMixerDescriptions     = IDirectFBScreen_Requestor_GetMixerDescriptions;
     thiz->GetMixerConfiguration    = IDirectFBScreen_Requestor_GetMixerConfiguration;
     thiz->TestMixerConfiguration   = IDirectFBScreen_Requestor_TestMixerConfiguration;
     thiz->SetMixerConfiguration    = IDirectFBScreen_Requestor_SetMixerConfiguration;
     thiz->GetEncoderDescriptions   = IDirectFBScreen_Requestor_GetEncoderDescriptions;
     thiz->GetEncoderConfiguration  = IDirectFBScreen_Requestor_GetEncoderConfiguration;
     thiz->TestEncoderConfiguration = IDirectFBScreen_Requestor_TestEncoderConfiguration;
     thiz->SetEncoderConfiguration  = IDirectFBScreen_Requestor_SetEncoderConfiguration;
     thiz->GetOutputDescriptions    = IDirectFBScreen_Requestor_GetOutputDescriptions;
     thiz->GetOutputConfiguration   = IDirectFBScreen_Requestor_GetOutputConfiguration;
     thiz->TestOutputConfiguration  = IDirectFBScreen_Requestor_TestOutputConfiguration;
     thiz->SetOutputConfiguration   = IDirectFBScreen_Requestor_SetOutputConfiguration;

     return DFB_OK;
}

