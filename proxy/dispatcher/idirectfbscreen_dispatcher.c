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

#include <core/layers.h>
#include <core/screen.h>

#include <direct/debug.h>
#include <direct/interface.h>

#include <voodoo/manager.h>
#include <voodoo/message.h>

#include "idirectfbscreen_dispatcher.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBScreen  *thiz,
                            IDirectFBScreen  *real,
                            VoodooManager    *manager,
                            VoodooInstanceID  super,
                            void             *arg,
                            VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBScreen, Dispatcher )

/**************************************************************************************************/

/*
 * private data struct of IDirectFBScreen_Dispatcher
 */
typedef struct {
     int                    ref;      /* reference counter */

     IDirectFBScreen       *real;
} IDirectFBScreen_Dispatcher_data;

/**************************************************************************************************/

static void
IDirectFBScreen_Dispatcher_Destruct( IDirectFBScreen *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBScreen_Dispatcher_AddRef( IDirectFBScreen *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_Dispatcher_Release( IDirectFBScreen *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (--data->ref == 0)
          IDirectFBScreen_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_Dispatcher_GetID( IDirectFBScreen *thiz,
                                  DFBScreenID     *id )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!id)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_GetDescription( IDirectFBScreen      *thiz,
                                           DFBScreenDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!desc)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_EnumDisplayLayers( IDirectFBScreen         *thiz,
                                              DFBDisplayLayerCallback  callbackfunc,
                                              void                    *callbackdata )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!callbackfunc)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_SetPowerMode( IDirectFBScreen    *thiz,
                                         DFBScreenPowerMode  mode )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_WaitForSync( IDirectFBScreen *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_GetMixerDescriptions( IDirectFBScreen           *thiz,
                                                 DFBScreenMixerDescription *descriptions )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!descriptions)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_GetMixerConfiguration( IDirectFBScreen      *thiz,
                                                  int                   mixer,
                                                  DFBScreenMixerConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!config)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_TestMixerConfiguration( IDirectFBScreen            *thiz,
                                                   int                         mixer,
                                                   const DFBScreenMixerConfig *config,
                                                   DFBScreenMixerConfigFlags  *failed )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!config || (config->flags & ~DSMCONF_ALL))
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_SetMixerConfiguration( IDirectFBScreen            *thiz,
                                                  int                         mixer,
                                                  const DFBScreenMixerConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!config || (config->flags & ~DSMCONF_ALL))
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_GetEncoderDescriptions( IDirectFBScreen             *thiz,
                                                   DFBScreenEncoderDescription *descriptions )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!descriptions)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_GetEncoderConfiguration( IDirectFBScreen        *thiz,
                                                    int                     encoder,
                                                    DFBScreenEncoderConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!config)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_TestEncoderConfiguration( IDirectFBScreen              *thiz,
                                                     int                           encoder,
                                                     const DFBScreenEncoderConfig *config,
                                                     DFBScreenEncoderConfigFlags  *failed )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!config || (config->flags & ~DSECONF_ALL))
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_SetEncoderConfiguration( IDirectFBScreen              *thiz,
                                                    int                           encoder,
                                                    const DFBScreenEncoderConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!config || (config->flags & ~DSECONF_ALL))
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_GetOutputDescriptions( IDirectFBScreen            *thiz,
                                                  DFBScreenOutputDescription *descriptions )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!descriptions)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_GetOutputConfiguration( IDirectFBScreen       *thiz,
                                                   int                    output,
                                                   DFBScreenOutputConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!config)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_TestOutputConfiguration( IDirectFBScreen             *thiz,
                                                    int                          output,
                                                    const DFBScreenOutputConfig *config,
                                                    DFBScreenOutputConfigFlags  *failed )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!config || (config->flags & ~DSOCONF_ALL))
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBScreen_Dispatcher_SetOutputConfiguration( IDirectFBScreen             *thiz,
                                                   int                          output,
                                                   const DFBScreenOutputConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBScreen_Dispatcher)

     if (!config || (config->flags & ~DSOCONF_ALL))
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBScreen/Dispatcher: "
              "Handling request for instance %lu with method %lu...\n", msg->instance, msg->method );

/*     switch (msg->method) {
          case IDIRECTFB_METHOD_ID_EnumScreens:
               return Dispatch_EnumScreens( dispatcher, real, manager, msg );
     }*/

     return DFB_NOSUCHMETHOD;
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
           IDirectFBScreen  *real,
           VoodooManager    *manager,
           VoodooInstanceID  super,
           void             *arg,      /* Optional arguments to constructor */
           VoodooInstanceID *ret_instance )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBScreen_Dispatcher)

     ret = voodoo_manager_register_local( manager, false, thiz, real, Dispatch, ret_instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref  = 1;
     data->real = real;

     thiz->AddRef                   = IDirectFBScreen_Dispatcher_AddRef;
     thiz->Release                  = IDirectFBScreen_Dispatcher_Release;
     thiz->GetID                    = IDirectFBScreen_Dispatcher_GetID;
     thiz->GetDescription           = IDirectFBScreen_Dispatcher_GetDescription;
     thiz->EnumDisplayLayers        = IDirectFBScreen_Dispatcher_EnumDisplayLayers;
     thiz->SetPowerMode             = IDirectFBScreen_Dispatcher_SetPowerMode;
     thiz->WaitForSync              = IDirectFBScreen_Dispatcher_WaitForSync;
     thiz->GetMixerDescriptions     = IDirectFBScreen_Dispatcher_GetMixerDescriptions;
     thiz->GetMixerConfiguration    = IDirectFBScreen_Dispatcher_GetMixerConfiguration;
     thiz->TestMixerConfiguration   = IDirectFBScreen_Dispatcher_TestMixerConfiguration;
     thiz->SetMixerConfiguration    = IDirectFBScreen_Dispatcher_SetMixerConfiguration;
     thiz->GetEncoderDescriptions   = IDirectFBScreen_Dispatcher_GetEncoderDescriptions;
     thiz->GetEncoderConfiguration  = IDirectFBScreen_Dispatcher_GetEncoderConfiguration;
     thiz->TestEncoderConfiguration = IDirectFBScreen_Dispatcher_TestEncoderConfiguration;
     thiz->SetEncoderConfiguration  = IDirectFBScreen_Dispatcher_SetEncoderConfiguration;
     thiz->GetOutputDescriptions    = IDirectFBScreen_Dispatcher_GetOutputDescriptions;
     thiz->GetOutputConfiguration   = IDirectFBScreen_Dispatcher_GetOutputConfiguration;
     thiz->TestOutputConfiguration  = IDirectFBScreen_Dispatcher_TestOutputConfiguration;
     thiz->SetOutputConfiguration   = IDirectFBScreen_Dispatcher_SetOutputConfiguration;

     return DFB_OK;
}

