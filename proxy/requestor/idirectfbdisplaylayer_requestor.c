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

#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include <idirectfbdisplaylayer_dispatcher.h>

#include "idirectfbdisplaylayer_requestor.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBDisplayLayer *thiz,
                            VoodooManager         *manager,
                            VoodooInstanceID       instance,
                            void                  *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBDisplayLayer, Requestor )


/**************************************************************************************************/

static void
IDirectFBDisplayLayer_Requestor_Destruct( IDirectFBDisplayLayer *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBDisplayLayer_Requestor_AddRef( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_Release( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (--data->ref == 0)
          IDirectFBDisplayLayer_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetID( IDirectFBDisplayLayer *thiz,
                                       DFBDisplayLayerID     *id )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetDescription( IDirectFBDisplayLayer      *thiz,
                                                DFBDisplayLayerDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetSurface( IDirectFBDisplayLayer  *thiz,
                                            IDirectFBSurface      **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetScreen( IDirectFBDisplayLayer  *thiz,
                                           IDirectFBScreen       **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetCooperativeLevel( IDirectFBDisplayLayer           *thiz,
                                                     DFBDisplayLayerCooperativeLevel  level )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetOpacity( IDirectFBDisplayLayer *thiz,
                                            __u8                   opacity )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetCurrentOutputField( IDirectFBDisplayLayer *thiz, int *field )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetFieldParity( IDirectFBDisplayLayer *thiz, int field )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetScreenLocation( IDirectFBDisplayLayer *thiz,
                                                   float                  x,
                                                   float                  y,
                                                   float                  width,
                                                   float                  height )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetSrcColorKey( IDirectFBDisplayLayer *thiz,
                                                __u8                   r,
                                                __u8                   g,
                                                __u8                   b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetDstColorKey( IDirectFBDisplayLayer *thiz,
                                                __u8                   r,
                                                __u8                   g,
                                                __u8                   b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetLevel( IDirectFBDisplayLayer *thiz,
                                          int                   *level )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetLevel( IDirectFBDisplayLayer *thiz,
                                          int                    level )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetConfiguration( IDirectFBDisplayLayer *thiz,
                                                  DFBDisplayLayerConfig *ret_config )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_GetConfiguration, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK) {
          VoodooMessageParser parser;

          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_READ_DATA( parser, ret_config, sizeof(DFBDisplayLayerConfig) );
          VOODOO_PARSER_END( parser );
     }

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_TestConfiguration( IDirectFBDisplayLayer       *thiz,
                                                   const DFBDisplayLayerConfig *config,
                                                   DFBDisplayLayerConfigFlags  *failed )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetConfiguration( IDirectFBDisplayLayer       *thiz,
                                                  const DFBDisplayLayerConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetBackgroundMode( IDirectFBDisplayLayer         *thiz,
                                                   DFBDisplayLayerBackgroundMode  background_mode )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetBackgroundImage( IDirectFBDisplayLayer *thiz,
                                                    IDirectFBSurface      *surface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetBackgroundColor( IDirectFBDisplayLayer *thiz,
                                                    __u8 r, __u8 g, __u8 b, __u8 a )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_CreateWindow( IDirectFBDisplayLayer       *thiz,
                                              const DFBWindowDescription  *desc,
                                              IDirectFBWindow            **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_CreateWindow, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(DFBWindowDescription), desc,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBWindow",
                                            response->instance, NULL, &interface );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetWindow( IDirectFBDisplayLayer  *thiz,
                                           DFBWindowID             id,
                                           IDirectFBWindow       **window )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_EnableCursor( IDirectFBDisplayLayer *thiz, int enable )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetCursorPosition( IDirectFBDisplayLayer *thiz,
                                                   int *x, int *y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_WarpCursor( IDirectFBDisplayLayer *thiz, int x, int y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetCursorAcceleration( IDirectFBDisplayLayer *thiz,
                                                       int                    numerator,
                                                       int                    denominator,
                                                       int                    threshold )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetCursorShape( IDirectFBDisplayLayer *thiz,
                                                IDirectFBSurface      *shape,
                                                int                    hot_x,
                                                int                    hot_y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetCursorOpacity( IDirectFBDisplayLayer *thiz,
                                                  __u8                   opacity )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetColorAdjustment( IDirectFBDisplayLayer *thiz,
                                                    DFBColorAdjustment    *adj )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetColorAdjustment( IDirectFBDisplayLayer    *thiz,
                                                    const DFBColorAdjustment *adj )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_WaitForSync( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

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
Construct( IDirectFBDisplayLayer *thiz,
           VoodooManager         *manager,
           VoodooInstanceID       instance,
           void                  *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDisplayLayer_Requestor)

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;

     thiz->AddRef                = IDirectFBDisplayLayer_Requestor_AddRef;
     thiz->Release               = IDirectFBDisplayLayer_Requestor_Release;
     thiz->GetID                 = IDirectFBDisplayLayer_Requestor_GetID;
     thiz->GetDescription        = IDirectFBDisplayLayer_Requestor_GetDescription;
     thiz->GetSurface            = IDirectFBDisplayLayer_Requestor_GetSurface;
     thiz->GetScreen             = IDirectFBDisplayLayer_Requestor_GetScreen;
     thiz->SetCooperativeLevel   = IDirectFBDisplayLayer_Requestor_SetCooperativeLevel;
     thiz->SetOpacity            = IDirectFBDisplayLayer_Requestor_SetOpacity;
     thiz->GetCurrentOutputField = IDirectFBDisplayLayer_Requestor_GetCurrentOutputField;
     thiz->SetScreenLocation     = IDirectFBDisplayLayer_Requestor_SetScreenLocation;
     thiz->SetSrcColorKey        = IDirectFBDisplayLayer_Requestor_SetSrcColorKey;
     thiz->SetDstColorKey        = IDirectFBDisplayLayer_Requestor_SetDstColorKey;
     thiz->GetLevel              = IDirectFBDisplayLayer_Requestor_GetLevel;
     thiz->SetLevel              = IDirectFBDisplayLayer_Requestor_SetLevel;
     thiz->GetConfiguration      = IDirectFBDisplayLayer_Requestor_GetConfiguration;
     thiz->TestConfiguration     = IDirectFBDisplayLayer_Requestor_TestConfiguration;
     thiz->SetConfiguration      = IDirectFBDisplayLayer_Requestor_SetConfiguration;
     thiz->SetBackgroundMode     = IDirectFBDisplayLayer_Requestor_SetBackgroundMode;
     thiz->SetBackgroundColor    = IDirectFBDisplayLayer_Requestor_SetBackgroundColor;
     thiz->SetBackgroundImage    = IDirectFBDisplayLayer_Requestor_SetBackgroundImage;
     thiz->GetColorAdjustment    = IDirectFBDisplayLayer_Requestor_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBDisplayLayer_Requestor_SetColorAdjustment;
     thiz->CreateWindow          = IDirectFBDisplayLayer_Requestor_CreateWindow;
     thiz->GetWindow             = IDirectFBDisplayLayer_Requestor_GetWindow;
     thiz->WarpCursor            = IDirectFBDisplayLayer_Requestor_WarpCursor;
     thiz->SetCursorAcceleration = IDirectFBDisplayLayer_Requestor_SetCursorAcceleration;
     thiz->EnableCursor          = IDirectFBDisplayLayer_Requestor_EnableCursor;
     thiz->GetCursorPosition     = IDirectFBDisplayLayer_Requestor_GetCursorPosition;
     thiz->SetCursorShape        = IDirectFBDisplayLayer_Requestor_SetCursorShape;
     thiz->SetCursorOpacity      = IDirectFBDisplayLayer_Requestor_SetCursorOpacity;
     thiz->SetFieldParity        = IDirectFBDisplayLayer_Requestor_SetFieldParity;
     thiz->WaitForSync           = IDirectFBDisplayLayer_Requestor_WaitForSync;

     return DFB_OK;
}

