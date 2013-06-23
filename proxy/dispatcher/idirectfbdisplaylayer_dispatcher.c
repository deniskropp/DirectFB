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

#include <core/surface.h>

#include <misc/conf.h>

#include <voodoo/conf.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include "idirectfbdisplaylayer_dispatcher.h"


static DFBResult Probe( void );
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
     IDirectFBDisplayLayer_Dispatcher_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     data->real->Release( data->real );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IDirectFBDisplayLayer_Dispatcher_AddRef( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DirectResult
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
                                             IDirectFBSurface      **interface_ptr )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_GetScreen( IDirectFBDisplayLayer  *thiz,
                                            IDirectFBScreen       **interface_ptr )
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
                                             u8                     opacity )
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
                                                 u8                     r,
                                                 u8                     g,
                                                 u8                     b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetDstColorKey( IDirectFBDisplayLayer *thiz,
                                                 u8                     r,
                                                 u8                     g,
                                                 u8                     b )
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
IDirectFBDisplayLayer_Dispatcher_TestConfiguration( IDirectFBDisplayLayer       *thiz,
                                                    const DFBDisplayLayerConfig *config,
                                                    DFBDisplayLayerConfigFlags  *failed )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDisplayLayer_Dispatcher_SetConfiguration( IDirectFBDisplayLayer       *thiz,
                                                   const DFBDisplayLayerConfig *config )
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
                                                     u8 r, u8 g, u8 b, u8 a )
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
                                                   u8                     opacity )
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
IDirectFBDisplayLayer_Dispatcher_SetColorAdjustment( IDirectFBDisplayLayer    *thiz,
                                                     const DFBColorAdjustment *adj )
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
Dispatch_Release( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     return voodoo_manager_unregister_local( manager, data->self );
}

static DirectResult
Dispatch_GetID( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult      ret;
     DFBDisplayLayerID id;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     ret = real->GetID( real, &id );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_ID, id,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetScreen( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult      ret;
     IDirectFBScreen  *screen;
     VoodooInstanceID  instance;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     ret = real->GetScreen( real, &screen );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBScreen",
                                        screen, data->super, NULL, &instance, NULL );
     if (ret) {
          screen->Release( screen );
          return ret;
     }

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

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

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(DFBDisplayLayerConfig), &config,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_TestConfiguration( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                            VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                 ret;
     const DFBDisplayLayerConfig *config;
     VoodooMessageParser          parser;
     DFBDisplayLayerConfigFlags   failed;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, config );
     VOODOO_PARSER_END( parser );

     ret = real->TestConfiguration( real, config, &failed );

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_UINT, failed,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetConfiguration( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                           VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                 ret;
     const DFBDisplayLayerConfig *config;
     VoodooMessageParser          parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, config );
     VOODOO_PARSER_END( parser );

     ret = real->SetConfiguration( real, config );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetCooperativeLevel( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                              VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                    ret;
     VoodooMessageParser             parser;
     DFBDisplayLayerCooperativeLevel level;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_UINT( parser, level );
     VOODOO_PARSER_END( parser );

     ret = real->SetCooperativeLevel( real, level );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
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
     bool                        force_system = (voodoo_config->resource_id != 0);
     bool                        force_video  = false;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, desc );
     VOODOO_PARSER_END( parser );

     if (voodoo_config->resource_id) {
          if (desc->flags & DWDESC_RESOURCE_ID) {
               if (desc->resource_id == voodoo_config->resource_id) {
                    force_system = false;

                    if (dfb_config->window_policy == CSP_VIDEOONLY)
                         force_video = true;
               }
          }
     }

     if (desc->flags & DWDESC_STACKING) {
          if (voodoo_config->stacking_mask && !(voodoo_config->stacking_mask & (1 << desc->stacking))) {
               D_ERROR( "Stacking class not permitted!\n" );
               return DR_ACCESSDENIED;
          }
     }

     if (force_system) {
          DFBWindowDescription wd = *desc;

          if (wd.flags & DWDESC_SURFACE_CAPS) {
               wd.surface_caps &= ~DSCAPS_VIDEOONLY;
               wd.surface_caps |=  DSCAPS_SYSTEMONLY;
          }
          else {
               wd.flags        |= DWDESC_SURFACE_CAPS;
               wd.surface_caps  = DSCAPS_SYSTEMONLY;
          }

          ret = real->CreateWindow( real, &wd, &window );
     }
     else if (force_video) {
          DFBWindowDescription wd = *desc;

          if (wd.flags & DWDESC_SURFACE_CAPS) {
               wd.surface_caps &= ~DSCAPS_SYSTEMONLY;
               wd.surface_caps |=  DSCAPS_VIDEOONLY;
          }
          else {
               wd.flags        |= DWDESC_SURFACE_CAPS;
               wd.surface_caps  = DSCAPS_VIDEOONLY;
          }

          ret = real->CreateWindow( real, &wd, &window );
     }
     else {
          ret = real->CreateWindow( real, desc, &window );
     }
     if (ret)
          return ret;

     if (!force_video) {
          unsigned int w = 256, h = 256, b = 2, size;

          if (desc->flags & DWDESC_WIDTH)
               w = desc->width;

          if (desc->flags & DWDESC_HEIGHT)
               h = desc->height;

          if (desc->flags & DWDESC_PIXELFORMAT)
               b = DFB_BYTES_PER_PIXEL( desc->pixelformat ) ? DFB_BYTES_PER_PIXEL( desc->pixelformat ) : 2;

          size = w * h * b;

          D_INFO( "Checking creation of %u kB window\n", size / 1024 );

          if (voodoo_config->surface_max && voodoo_config->surface_max < size) {
               D_ERROR( "Allocation of %u kB window not permitted (limit %u kB)\n",
                        size / 1024, voodoo_config->surface_max / 1024 );
               return DR_LIMITEXCEEDED;
          }

          ret = voodoo_manager_check_allocation( manager, size );
          if (ret) {
               D_ERROR( "Allocation not permitted!\n" );
               return ret;
          }
     }

     ret = voodoo_construct_dispatcher( manager, "IDirectFBWindow",
                                        window, data->super, NULL, &instance, NULL );
     if (ret) {
          window->Release( window );
          return ret;
     }

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetWindow( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     DFBWindowID          id;
     IDirectFBWindow     *window;
     VoodooInstanceID     instance;
     VoodooMessageParser  parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, id );
     VOODOO_PARSER_END( parser );

     ret = real->GetWindow( real, id, &window );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBWindow",
                                        window, data->super, NULL, &instance, NULL );
     if (ret) {
          window->Release( window );
          return ret;
     }

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetWindowByResourceID( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                                VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     DFBWindowID          id;
     IDirectFBWindow     *window;
     VoodooInstanceID     instance;
     VoodooMessageParser  parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, id );
     VOODOO_PARSER_END( parser );

     ret = real->GetWindowByResourceID( real, id, &window );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBWindow",
                                        window, data->super, NULL, &instance, NULL );
     if (ret) {
          window->Release( window );
          return ret;
     }

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetRotation( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;
     int          rotation;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     ret = real->GetRotation( real, &rotation );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, true, msg->header.serial,
                                    DR_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, rotation,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetBackgroundMode( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                            VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser           parser;
     DFBDisplayLayerBackgroundMode background_mode;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, background_mode );
     VOODOO_PARSER_END( parser );

     real->SetBackgroundMode( real, background_mode );

     return DFB_OK;
}

static DirectResult
Dispatch_SetBackgroundImage( IDirectFBDisplayLayer *thiz, IDirectFBDisplayLayer *real,
                             VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     void                *surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBDisplayLayer_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     ret = voodoo_manager_lookup_local( manager, instance, NULL, &surface );
     if (ret)
          return ret;

     real->SetBackgroundImage( real, surface );

     return DFB_OK;
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBDisplayLayer/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBDISPLAYLAYER_METHOD_ID_Release:
               return Dispatch_Release( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_GetID:
               return Dispatch_GetID( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_GetScreen:
               return Dispatch_GetScreen( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_SetCooperativeLevel:
               return Dispatch_SetCooperativeLevel( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_GetConfiguration:
               return Dispatch_GetConfiguration( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_TestConfiguration:
               return Dispatch_TestConfiguration( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_SetConfiguration:
               return Dispatch_SetConfiguration( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_CreateWindow:
               return Dispatch_CreateWindow( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_GetWindow:
               return Dispatch_GetWindow( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_GetWindowByResourceID:
               return Dispatch_GetWindowByResourceID( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_GetRotation:
               return Dispatch_GetRotation( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_SetBackgroundMode:
               return Dispatch_SetBackgroundMode( dispatcher, real, manager, msg );

          case IDIRECTFBDISPLAYLAYER_METHOD_ID_SetBackgroundImage:
               return Dispatch_SetBackgroundImage( dispatcher, real, manager, msg );
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
     ret = voodoo_manager_register_local( manager, super, thiz, real, Dispatch, &instance );
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

