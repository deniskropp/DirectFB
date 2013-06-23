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

#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <misc/conf.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include <idirectfbdisplaylayer_dispatcher.h>

#include "idirectfbdisplaylayer_requestor.h"
#include "idirectfbsurface_requestor.h"


static DFBResult Probe( void );
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
     IDirectFBDisplayLayer_Requestor_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     voodoo_manager_request( data->manager, data->instance,
                             IDIRECTFBDISPLAYLAYER_METHOD_ID_Release, VREQ_NONE, NULL,
                             VMBT_NONE );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IDirectFBDisplayLayer_Requestor_AddRef( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBDisplayLayer_Requestor_Release( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (--data->ref == 0)
          IDirectFBDisplayLayer_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetID( IDirectFBDisplayLayer *thiz,
                                       DFBDisplayLayerID     *ret_id )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     DFBDisplayLayerID      id;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (!ret_id)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_GetID, VREQ_RESPOND, &response,
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
IDirectFBDisplayLayer_Requestor_GetDescription( IDirectFBDisplayLayer      *thiz,
                                                DFBDisplayLayerDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetSurface( IDirectFBDisplayLayer  *thiz,
                                            IDirectFBSurface      **interface_ptr )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetScreen( IDirectFBDisplayLayer  *thiz,
                                           IDirectFBScreen       **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface_ptr = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_GetScreen, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBScreen",
                                            response->instance, data->idirectfb, &interface_ptr );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface_ptr;

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetCooperativeLevel( IDirectFBDisplayLayer           *thiz,
                                                     DFBDisplayLayerCooperativeLevel  level )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_SetCooperativeLevel, VREQ_RESPOND, &response,
                                   VMBT_UINT, level,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetOpacity( IDirectFBDisplayLayer *thiz,
                                            u8                     opacity )
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
                                                u8                     r,
                                                u8                     g,
                                                u8                     b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetDstColorKey( IDirectFBDisplayLayer *thiz,
                                                u8                     r,
                                                u8                     g,
                                                u8                     b )
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
     if (ret == DR_OK) {
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
                                                   DFBDisplayLayerConfigFlags  *ret_failed )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (!config)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_TestConfiguration, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(DFBDisplayLayerConfig), config,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     if (ret_failed) {
          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_GET_UINT( parser, *ret_failed );
          VOODOO_PARSER_END( parser );
     }

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetConfiguration( IDirectFBDisplayLayer       *thiz,
                                                  const DFBDisplayLayerConfig *config )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (!config)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_SetConfiguration, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(DFBDisplayLayerConfig), config,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetBackgroundMode( IDirectFBDisplayLayer         *thiz,
                                                   DFBDisplayLayerBackgroundMode  background_mode )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBDISPLAYLAYER_METHOD_ID_SetBackgroundMode, VREQ_NONE, NULL,
                                    VMBT_INT, background_mode,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetBackgroundImage( IDirectFBDisplayLayer *thiz,
                                                    IDirectFBSurface      *surface )
{
     IDirectFBSurface_Requestor_data *surface_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (!surface)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM( surface, surface_data, IDirectFBSurface_Requestor );

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBDISPLAYLAYER_METHOD_ID_SetBackgroundImage, VREQ_NONE, NULL,
                                    VMBT_ID, surface_data->instance,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBDisplayLayer_Requestor_SetBackgroundColor( IDirectFBDisplayLayer *thiz,
                                                    u8 r, u8 g, u8 b, u8 a )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_CreateWindow( IDirectFBDisplayLayer       *thiz,
                                              const DFBWindowDescription  *real_desc,
                                              IDirectFBWindow            **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     DFBWindowDescription   desc;
     void                  *interface_ptr = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     direct_memcpy( &desc, real_desc, sizeof(DFBWindowDescription) );

     if (!(desc.flags & DWDESC_RESOURCE_ID)) {
          desc.flags       |= DWDESC_RESOURCE_ID;
          desc.resource_id  = dfb_config->resource_id;
     }

     D_INFO( "IDirectFBDisplayLayer_Requestor_CreateWindow: Using resource ID %lu\n", desc.resource_id );

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_CreateWindow, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(DFBWindowDescription), &desc,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBWindow",
                                            response->instance, data->idirectfb, &interface_ptr );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface_ptr;

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetWindow( IDirectFBDisplayLayer  *thiz,
                                           DFBWindowID             id,
                                           IDirectFBWindow       **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface_ptr = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_GetWindow, VREQ_RESPOND, &response,
                                   VMBT_ID, id,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBWindow",
                                            response->instance, data->idirectfb, &interface_ptr );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface_ptr;

     return ret;
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
                                                  u8                     opacity )
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

static DFBResult
IDirectFBDisplayLayer_Requestor_GetWindowByResourceID( IDirectFBDisplayLayer  *thiz,
                                                       unsigned long           resource_id,
                                                       IDirectFBWindow       **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface_ptr = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_GetWindowByResourceID, VREQ_RESPOND, &response,
                                   VMBT_ID, resource_id,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBWindow",
                                            response->instance, data->idirectfb, &interface_ptr );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface_ptr;

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_Requestor_GetRotation( IDirectFBDisplayLayer *thiz,
                                             int                   *ret_rotation )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Requestor)

     if (!ret_rotation)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBDISPLAYLAYER_METHOD_ID_GetRotation, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DR_OK) {
          VoodooMessageParser parser;

          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_GET_INT( parser, *ret_rotation );
          VOODOO_PARSER_END( parser );
     }

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

static DFBResult
Construct( IDirectFBDisplayLayer *thiz,
           VoodooManager         *manager,
           VoodooInstanceID       instance,
           void                  *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDisplayLayer_Requestor)

     data->ref       = 1;
     data->manager   = manager;
     data->instance  = instance;
     data->idirectfb = arg;

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
     thiz->GetWindowByResourceID = IDirectFBDisplayLayer_Requestor_GetWindowByResourceID;
     thiz->GetRotation           = IDirectFBDisplayLayer_Requestor_GetRotation;

     return DFB_OK;
}

