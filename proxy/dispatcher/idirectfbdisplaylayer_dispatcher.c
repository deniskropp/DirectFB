/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include "idirectfbdisplaylayer_dispatcher.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBDisplayLayer *thiz,
                            IDirectFBDisplayLayer *real,
                            VoodooManager         *manager,
                            VoodooInstanceID       super,
                            void                  *arg,
                            VoodooInstanceID      *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBDisplayLayer, Dispatcher )


/**************************************************************************************************/

static void
IDirectFBDisplayLayer_Dispatcher_Destruct( IDirectFBDisplayLayer *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBDisplayLayer_Dispatcher_AddRef( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_Release( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     if (--data->ref == 0)
          IDirectFBDisplayLayer_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetID( IDirectFBDisplayLayer *thiz,
                                        DFBDisplayLayerID     *id )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetDescription( IDirectFBDisplayLayer      *thiz,
                                                 DFBDisplayLayerDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetSurface( IDirectFBDisplayLayer  *thiz,
                                             IDirectFBSurface      **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetScreen( IDirectFBDisplayLayer  *thiz,
                                            IDirectFBScreen       **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetCooperativeLevel( IDirectFBDisplayLayer           *thiz,
                                                      DFBDisplayLayerCooperativeLevel  level )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetOpacity( IDirectFBDisplayLayer *thiz,
                                            __u8                   opacity )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetCurrentOutputField( IDirectFBDisplayLayer *thiz, int *field )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetFieldParity( IDirectFBDisplayLayer *thiz, int field )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetScreenLocation( IDirectFBDisplayLayer *thiz,
                                                    float                  x,
                                                    float                  y,
                                                    float                  width,
                                                    float                  height )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetSrcColorKey( IDirectFBDisplayLayer *thiz,
                                                 __u8                   r,
                                                 __u8                   g,
                                                 __u8                   b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetDstColorKey( IDirectFBDisplayLayer *thiz,
                                                 __u8                   r,
                                                 __u8                   g,
                                                 __u8                   b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetLevel( IDirectFBDisplayLayer *thiz,
                                           int                   *level )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetLevel( IDirectFBDisplayLayer *thiz,
                                           int                    level )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetConfiguration( IDirectFBDisplayLayer *thiz,
                                                   DFBDisplayLayerConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_TestConfiguration( IDirectFBDisplayLayer      *thiz,
                                                    DFBDisplayLayerConfig      *config,
                                                    DFBDisplayLayerConfigFlags *failed )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetConfiguration( IDirectFBDisplayLayer *thiz,
                                                   DFBDisplayLayerConfig *config )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetBackgroundMode( IDirectFBDisplayLayer         *thiz,
                                                    DFBDisplayLayerBackgroundMode  background_mode )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetBackgroundImage( IDirectFBDisplayLayer *thiz,
                                                     IDirectFBSurface      *surface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetBackgroundColor( IDirectFBDisplayLayer *thiz,
                                                     __u8 r, __u8 g, __u8 b, __u8 a )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_CreateWindow( IDirectFBDisplayLayer       *thiz,
                                               const DFBWindowDescription  *desc,
                                               IDirectFBWindow            **window )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetWindow( IDirectFBDisplayLayer  *thiz,
                                            DFBWindowID             id,
                                            IDirectFBWindow       **window )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_EnableCursor( IDirectFBDisplayLayer *thiz, int enable )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetCursorPosition( IDirectFBDisplayLayer *thiz,
                                                    int *x, int *y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_WarpCursor( IDirectFBDisplayLayer *thiz, int x, int y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetCursorAcceleration( IDirectFBDisplayLayer *thiz,
                                                        int                    numerator,
                                                        int                    denominator,
                                                        int                    threshold )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetCursorShape( IDirectFBDisplayLayer *thiz,
                                                 IDirectFBSurface      *shape,
                                                 int                    hot_x,
                                                 int                    hot_y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetCursorOpacity( IDirectFBDisplayLayer *thiz,
                                                   __u8                   opacity )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetColorAdjustment( IDirectFBDisplayLayer *thiz,
                                                     DFBColorAdjustment    *adj )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetColorAdjustment( IDirectFBDisplayLayer *thiz,
                                                     DFBColorAdjustment    *adj )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_WaitForSync( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_GetConfiguration( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                           VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult          ret;
     DFBDisplayLayerConfig config;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     ret = real->GetConfiguration( real, &config );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(DFBDisplayLayerConfig), &config,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_CreateWindow( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                ret;
     const DFBWindowDescription *desc;
     IDirectFBWindow            *window;
     VoodooInstanceID            instance;
     VoodooMessageParser         parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, desc );
     VOODOO_PARSER_END( parser );

     ret = real->CreateWindow( real, desc, &window );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBWindow",
                                        window, data->self, NULL, &instance, NULL );
     if (ret) {
          window->Release( window );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBDisplayLayer/Dispatcher: "
              "Handling request for instance %lu with method %lu...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBDISPLAYLAYER_METHOD_ID_GetConfiguration:
               return Dispatch_GetConfiguration( dispatcher, real, manager, msg );
          case IDIRECTFBDISPLAYLAYER_METHOD_ID_CreateWindow:
               return Dispatch_CreateWindow( dispatcher, real, manager, msg );
     }

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
Construct( IDirectFBDisplayLayer *thiz,     /* Dispatcher interface */
           IDirectFBDisplayLayer *real,     /* Real interface implementation */
           VoodooManager         *manager,  /* Manager of the Voodoo framework */
           VoodooInstanceID       super,    /* Instance ID of the super interface */
           void                  *arg,      /* Optional arguments to constructor */
           VoodooInstanceID      *ret_instance )
{
     DFBResult        ret;
     VoodooInstanceID instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBDisplayLayer_Dispatcher)

     D_ASSERT( real != NULL );
     D_ASSERT( manager != NULL );
     D_ASSERT( super != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_instance != NULL );

     /* Register the dispatcher, getting a new instance ID that refers to it. */
     ret = voodoo_manager_register( manager, false, thiz, real, Dispatch, &instance );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Return the new instance. */
     *ret_instance = instance;

     /* Initialize interface data. */
     data->ref   = 1;
     data->real  = real;
     data->self  = instance;
     data->super = super;

     /* Initialize interface methods. */
     thiz->AddRef                = IDirectFBDisplayLayer_Dispatcher_AddRef;
     thiz->Release               = IDirectFBDisplayLayer_Dispatcher_Release;
     thiz->GetID                 = IDirectFBDisplayLayer_Dispatcher_GetID;
     thiz->GetDescription        = IDirectFBDisplayLayer_Dispatcher_GetDescription;
     thiz->GetSurface            = IDirectFBDisplayLayer_Dispatcher_GetSurface;
     thiz->GetScreen             = IDirectFBDisplayLayer_Dispatcher_GetScreen;
     thiz->SetCooperativeLevel   = IDirectFBDisplayLayer_Dispatcher_SetCooperativeLevel;
     thiz->SetOpacity            = IDirectFBDisplayLayer_Dispatcher_SetOpacity;
     thiz->GetCurrentOutputField = IDirectFBDisplayLayer_Dispatcher_GetCurrentOutputField;
     thiz->SetScreenLocation     = IDirectFBDisplayLayer_Dispatcher_SetScreenLocation;
     thiz->SetSrcColorKey        = IDirectFBDisplayLayer_Dispatcher_SetSrcColorKey;
     thiz->SetDstColorKey        = IDirectFBDisplayLayer_Dispatcher_SetDstColorKey;
     thiz->GetLevel              = IDirectFBDisplayLayer_Dispatcher_GetLevel;
     thiz->SetLevel              = IDirectFBDisplayLayer_Dispatcher_SetLevel;
     thiz->GetConfiguration      = IDirectFBDisplayLayer_Dispatcher_GetConfiguration;
     thiz->TestConfiguration     = IDirectFBDisplayLayer_Dispatcher_TestConfiguration;
     thiz->SetConfiguration      = IDirectFBDisplayLayer_Dispatcher_SetConfiguration;
     thiz->SetBackgroundMode     = IDirectFBDisplayLayer_Dispatcher_SetBackgroundMode;
     thiz->SetBackgroundColor    = IDirectFBDisplayLayer_Dispatcher_SetBackgroundColor;
     thiz->SetBackgroundImage    = IDirectFBDisplayLayer_Dispatcher_SetBackgroundImage;
     thiz->GetColorAdjustment    = IDirectFBDisplayLayer_Dispatcher_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBDisplayLayer_Dispatcher_SetColorAdjustment;
     thiz->CreateWindow          = IDirectFBDisplayLayer_Dispatcher_CreateWindow;
     thiz->GetWindow             = IDirectFBDisplayLayer_Dispatcher_GetWindow;
     thiz->WarpCursor            = IDirectFBDisplayLayer_Dispatcher_WarpCursor;
     thiz->SetCursorAcceleration = IDirectFBDisplayLayer_Dispatcher_SetCursorAcceleration;
     thiz->EnableCursor          = IDirectFBDisplayLayer_Dispatcher_EnableCursor;
     thiz->GetCursorPosition     = IDirectFBDisplayLayer_Dispatcher_GetCursorPosition;
     thiz->SetCursorShape        = IDirectFBDisplayLayer_Dispatcher_SetCursorShape;
     thiz->SetCursorOpacity      = IDirectFBDisplayLayer_Dispatcher_SetCursorOpacity;
     thiz->SetFieldParity        = IDirectFBDisplayLayer_Dispatcher_SetFieldParity;
     thiz->WaitForSync           = IDirectFBDisplayLayer_Dispatcher_WaitForSync;

     return DFB_OK;
}

