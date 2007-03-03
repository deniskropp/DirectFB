/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/interface.h>
#include <voodoo/manager.h>
#include <voodoo/message.h>

#include "idirectfb_dispatcher.h"

static DFBResult Probe();
static DFBResult Construct( IDirectFB        *thiz,
                            VoodooManager    *manager,
                            VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFB, Dispatcher )


/**************************************************************************************************/

typedef struct {
     DirectLink            link;

     VoodooInstanceID      instance;
     IDirectFBDataBuffer  *requestor;
} DataBufferEntry;

/*
 * private data struct of IDirectFB_Dispatcher
 */
typedef struct {
     int                    ref;          /* reference counter */

     IDirectFB             *real;

     VoodooInstanceID       self;         /* The instance of this dispatcher itself. */

     DirectLink            *data_buffers; /* list of known data buffers */
} IDirectFB_Dispatcher_data;

/**************************************************************************************************/

static void
IDirectFB_Dispatcher_Destruct( IDirectFB *thiz )
{
     DirectLink                *l, *n;
     IDirectFB_Dispatcher_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     direct_list_foreach_safe (l, n, data->data_buffers) {
          DataBufferEntry *entry = (DataBufferEntry*) l;

          entry->requestor->Release( entry->requestor );

          D_FREE( entry );
     }

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFB_Dispatcher_AddRef( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFB_Dispatcher_Release( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (--data->ref == 0)
          IDirectFB_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFB_Dispatcher_SetCooperativeLevel( IDirectFB           *thiz,
                                          DFBCooperativeLevel  level )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_GetDeviceDescription( IDirectFB                    *thiz,
                                           DFBGraphicsDeviceDescription *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!ret_desc)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_EnumVideoModes( IDirectFB            *thiz,
                                     DFBVideoModeCallback  callbackfunc,
                                     void                 *callbackdata )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!callbackfunc)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_SetVideoMode( IDirectFB    *thiz,
                                   int           width,
                                   int           height,
                                   int           bpp )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_CreateSurface( IDirectFB                    *thiz,
                                    const DFBSurfaceDescription  *desc,
                                    IDirectFBSurface            **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!desc || !interface)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_CreatePalette( IDirectFB                    *thiz,
                                    const DFBPaletteDescription  *desc,
                                    IDirectFBPalette            **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!interface)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_EnumScreens( IDirectFB         *thiz,
                                  DFBScreenCallback  callbackfunc,
                                  void              *callbackdata )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!callbackfunc)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_GetScreen( IDirectFB        *thiz,
                                DFBScreenID       id,
                                IDirectFBScreen **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!interface)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_EnumDisplayLayers( IDirectFB               *thiz,
                                        DFBDisplayLayerCallback  callbackfunc,
                                        void                    *callbackdata )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!callbackfunc)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_GetDisplayLayer( IDirectFB              *thiz,
                                      DFBDisplayLayerID       id,
                                      IDirectFBDisplayLayer **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!interface)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_EnumInputDevices( IDirectFB              *thiz,
                                       DFBInputDeviceCallback  callbackfunc,
                                       void                   *callbackdata )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!callbackfunc)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_GetInputDevice( IDirectFB             *thiz,
                                     DFBInputDeviceID       id,
                                     IDirectFBInputDevice **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!interface)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_CreateEventBuffer( IDirectFB             *thiz,
                                        IDirectFBEventBuffer **interface)
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!interface)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_CreateInputEventBuffer( IDirectFB                   *thiz,
                                             DFBInputDeviceCapabilities   caps,
                                             DFBBoolean                   global,
                                             IDirectFBEventBuffer       **interface)
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!interface)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_CreateImageProvider( IDirectFB               *thiz,
                                          const char              *filename,
                                          IDirectFBImageProvider **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     /* Check arguments */
     if (!filename || !interface)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_CreateVideoProvider( IDirectFB               *thiz,
                                          const char              *filename,
                                          IDirectFBVideoProvider **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     /* Check arguments */
     if (!interface || !filename)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_CreateFont( IDirectFB                 *thiz,
                                 const char                *filename,
                                 const DFBFontDescription  *desc,
                                 IDirectFBFont            **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     /* Check arguments */
     if (!interface)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_CreateDataBuffer( IDirectFB                       *thiz,
                                       const DFBDataBufferDescription  *desc,
                                       IDirectFBDataBuffer            **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!interface)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_SetClipboardData( IDirectFB      *thiz,
                                       const char     *mime_type,
                                       const void     *data,
                                       unsigned int    size,
                                       struct timeval *timestamp )
{
     if (!mime_type || !data || !size)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_GetClipboardData( IDirectFB     *thiz,
                                       char         **mime_type,
                                       void         **clip_data,
                                       unsigned int  *size )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!mime_type && !clip_data && !size)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_GetClipboardTimeStamp( IDirectFB      *thiz,
                                            struct timeval *timestamp )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     if (!timestamp)
          return DFB_INVARG;



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_Suspend( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_Resume( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_WaitIdle( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_WaitForSync( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)



     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Dispatcher_GetInterface( IDirectFB   *thiz,
                                   const char  *type,
                                   const char  *implementation,
                                   void        *arg,
                                   void       **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)



     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_SetCooperativeLevel( IDirectFB *thiz, IDirectFB *real,
                              VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     DFBCooperativeLevel level;
     VoodooMessageParser parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, level );
     VOODOO_PARSER_END( parser );

     ret = real->SetCooperativeLevel( real, level );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetDeviceDescription( IDirectFB *thiz, IDirectFB *real,
                               VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                 ret;
     DFBGraphicsDeviceDescription desc;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     ret = real->GetDeviceDescription( real, &desc );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(DFBGraphicsDeviceDescription), &desc,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_SetVideoMode( IDirectFB *thiz, IDirectFB *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     VoodooMessageParser parser;
     int                 width;
     int                 height;
     int                 bpp;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, width );
     VOODOO_PARSER_GET_INT( parser, height );
     VOODOO_PARSER_GET_INT( parser, bpp );
     VOODOO_PARSER_END( parser );

     ret = real->SetVideoMode( real, width, height, bpp );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_CreateEventBuffer( IDirectFB *thiz, IDirectFB *real,
                            VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult          ret;
     IDirectFBEventBuffer *buffer;
     IDirectFBEventBuffer *requestor;
     VoodooInstanceID      instance;
     VoodooMessageParser   parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     ret = real->CreateEventBuffer( real, &buffer );
     if (ret)
          return ret;

     ret = voodoo_construct_requestor( manager, "IDirectFBEventBuffer",
                                       instance, buffer, (void**) &requestor );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     return DFB_OK;
}

static DirectResult
Dispatch_CreateInputEventBuffer( IDirectFB *thiz, IDirectFB *real,
                                 VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                ret;
     IDirectFBEventBuffer       *buffer;
     IDirectFBEventBuffer       *requestor;
     VoodooInstanceID            instance;
     DFBInputDeviceCapabilities  caps;
     DFBBoolean                  global;
     VoodooMessageParser         parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_GET_INT( parser, caps );
     VOODOO_PARSER_GET_INT( parser, global );
     VOODOO_PARSER_END( parser );

     ret = real->CreateInputEventBuffer( real, caps, global, &buffer );
     if (ret)
          return ret;

     ret = voodoo_construct_requestor( manager, "IDirectFBEventBuffer",
                                       instance, buffer, (void**) &requestor );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     return DFB_OK;
}

static DirectResult
Dispatch_CreateImageProvider( IDirectFB *thiz, IDirectFB *real,
                              VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult            ret;
     DirectLink             *l;
     VoodooMessageParser     parser;
     VoodooInstanceID        instance;
     IDirectFBImageProvider *provider;
     IDirectFBDataBuffer    *buffer = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     direct_list_foreach (l, data->data_buffers) {
          DataBufferEntry *entry = (DataBufferEntry*) l;

          if (entry->instance == instance) {
               buffer = entry->requestor;
               break;
          }
     }

     ret = buffer->CreateImageProvider( buffer, &provider );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBImageProvider",
                                        provider, data->self, NULL, &instance, NULL );
     if (ret) {
          provider->Release( provider );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_CreateSurface( IDirectFB *thiz, IDirectFB *real,
                        VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                 ret;
     const DFBSurfaceDescription *desc;
     IDirectFBSurface            *surface;
     VoodooInstanceID             instance;
     VoodooMessageParser          parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, desc );
     VOODOO_PARSER_END( parser );

     ret = real->CreateSurface( real, desc, &surface );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBSurface",
                                        surface, data->self, NULL, &instance, NULL );
     if (ret) {
          surface->Release( surface );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

#define MAX_SCREENS 32

typedef struct {
     int                                   num;
     IDirectFB_Dispatcher_EnumScreens_Item items[MAX_SCREENS];
} EnumScreens_Context;

static DFBEnumerationResult
EnumScreens_Callback( DFBScreenID           screen_id,
                      DFBScreenDescription  desc,
                      void                 *callbackdata )
{
     int                  index;
     EnumScreens_Context *context = callbackdata;

     if (context->num == MAX_SCREENS) {
          D_WARN( "maximum number of %d screens reached", MAX_SCREENS );
          return DFENUM_CANCEL;
     }

     index = context->num++;

     context->items[index].screen_id = screen_id;
     context->items[index].desc      = desc;

     return DFENUM_OK;
}

static DirectResult
Dispatch_EnumScreens( IDirectFB *thiz, IDirectFB *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult        ret;
     EnumScreens_Context context = { 0 };

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     ret = real->EnumScreens( real, EnumScreens_Callback, &context );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, context.num,
                                    VMBT_DATA, context.num * sizeof(IDirectFB_Dispatcher_EnumScreens_Item), context.items,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetDisplayLayer( IDirectFB *thiz, IDirectFB *real,
                          VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult             ret;
     DFBDisplayLayerID        id;
     IDirectFBDisplayLayer   *layer;
     VoodooInstanceID         instance;
     VoodooMessageParser      parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, id );
     VOODOO_PARSER_END( parser );

     ret = real->GetDisplayLayer( real, id, &layer );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBDisplayLayer",
                                        layer, data->self, NULL, &instance, NULL );
     if (ret) {
          layer->Release( layer );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

#define MAX_INPUT_DEVICES 64

typedef struct {
     int                                        num;
     IDirectFB_Dispatcher_EnumInputDevices_Item items[MAX_INPUT_DEVICES];
} EnumInputDevices_Context;

static DFBEnumerationResult
EnumInputDevices_Callback( DFBInputDeviceID           device_id,
                           DFBInputDeviceDescription  desc,
                           void                      *callbackdata )
{
     int                       index;
     EnumInputDevices_Context *context = callbackdata;

     if (context->num == MAX_INPUT_DEVICES) {
          D_WARN( "maximum number of %d input devices reached", MAX_INPUT_DEVICES );
          return DFENUM_CANCEL;
     }

     index = context->num++;

     context->items[index].device_id = device_id;
     context->items[index].desc      = desc;

     return DFENUM_OK;
}

static DirectResult
Dispatch_EnumInputDevices( IDirectFB *thiz, IDirectFB *real,
                           VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult             ret;
     EnumInputDevices_Context context = { 0 };

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     ret = real->EnumInputDevices( real, EnumInputDevices_Callback, &context );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_INT, context.num,
                                    VMBT_DATA, context.num * sizeof(IDirectFB_Dispatcher_EnumInputDevices_Item), context.items,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetInputDevice( IDirectFB *thiz, IDirectFB *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult          ret;
     DFBScreenID           device_id;
     IDirectFBInputDevice *device;
     VoodooInstanceID      instance;
     VoodooMessageParser   parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, device_id );
     VOODOO_PARSER_END( parser );

     ret = real->GetInputDevice( real, device_id, &device );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBInputDevice",
                                        device, data->self, NULL, &instance, NULL );
     if (ret) {
          device->Release( device );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetScreen( IDirectFB *thiz, IDirectFB *real,
                    VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     DFBScreenID          screen_id;
     IDirectFBScreen     *screen;
     VoodooInstanceID     instance;
     VoodooMessageParser  parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, screen_id );
     VOODOO_PARSER_END( parser );

     ret = real->GetScreen( real, screen_id, &screen );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBScreen",
                                        screen, data->self, NULL, &instance, NULL );
     if (ret) {
          screen->Release( screen );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_CreateFont( IDirectFB *thiz, IDirectFB *real,
                     VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult              ret;
     IDirectFBFont            *font;
     VoodooInstanceID          instance;
     VoodooMessageParser       parser;
     const char               *filename;
     const DFBFontDescription *desc;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_STRING( parser, filename );
     VOODOO_PARSER_GET_DATA( parser, desc );
     VOODOO_PARSER_END( parser );

     ret = real->CreateFont( real, filename, desc, &font );
     if (ret)
          return ret;

     ret = voodoo_construct_dispatcher( manager, "IDirectFBFont",
                                        font, data->self, NULL, &instance, NULL );
     if (ret) {
          font->Release( font );
          return ret;
     }

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, instance,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_CreateDataBuffer( IDirectFB *thiz, IDirectFB *real,
                           VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult         ret;
     IDirectFBDataBuffer *requestor;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     DataBufferEntry     *entry;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     ret = voodoo_construct_requestor( manager, "IDirectFBDataBuffer",
                                       instance, NULL, (void**) &requestor );
     if (ret)
          return ret;

     entry = D_CALLOC( 1, sizeof(DataBufferEntry) );
     if (!entry) {
          D_WARN( "out of memory" );
          requestor->Release( requestor );
          return DFB_NOSYSTEMMEMORY;
     }

     entry->instance  = instance;
     entry->requestor = requestor;

     direct_list_prepend( &data->data_buffers, &entry->link );

     return DFB_OK;
}

static DirectResult
Dispatch_WaitIdle( IDirectFB *thiz, IDirectFB *real,
                   VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult ret;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Dispatcher)

     ret = real->WaitIdle( real );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFB/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFB_METHOD_ID_SetCooperativeLevel:
               return Dispatch_SetCooperativeLevel( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_GetDeviceDescription:
               return Dispatch_GetDeviceDescription( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_SetVideoMode:
               return Dispatch_SetVideoMode( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_CreateSurface:
               return Dispatch_CreateSurface( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_EnumScreens:
               return Dispatch_EnumScreens( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_GetScreen:
               return Dispatch_GetScreen( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_GetDisplayLayer:
               return Dispatch_GetDisplayLayer( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_EnumInputDevices:
               return Dispatch_EnumInputDevices( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_GetInputDevice:
               return Dispatch_GetInputDevice( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_CreateEventBuffer:
               return Dispatch_CreateEventBuffer( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_CreateInputEventBuffer:
               return Dispatch_CreateInputEventBuffer( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_CreateImageProvider:
               return Dispatch_CreateImageProvider( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_CreateFont:
               return Dispatch_CreateFont( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_CreateDataBuffer:
               return Dispatch_CreateDataBuffer( dispatcher, real, manager, msg );

          case IDIRECTFB_METHOD_ID_WaitIdle:
               return Dispatch_WaitIdle( dispatcher, real, manager, msg );
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

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
static DFBResult
Construct( IDirectFB *thiz, VoodooManager *manager, VoodooInstanceID *ret_instance )
{
     DFBResult         ret;
     IDirectFB        *real;
     VoodooInstanceID  instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFB_Dispatcher)

     ret = DirectFBCreate( &real );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ret = voodoo_manager_register_local( manager, true, thiz, real, Dispatch, &instance );
     if (ret) {
          real->Release( real );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     *ret_instance = instance;

     data->ref  = 1;
     data->real = real;
     data->self = instance;

     thiz->AddRef                  = IDirectFB_Dispatcher_AddRef;
     thiz->Release                 = IDirectFB_Dispatcher_Release;
     thiz->SetCooperativeLevel     = IDirectFB_Dispatcher_SetCooperativeLevel;
     thiz->GetDeviceDescription    = IDirectFB_Dispatcher_GetDeviceDescription;
     thiz->EnumVideoModes          = IDirectFB_Dispatcher_EnumVideoModes;
     thiz->SetVideoMode            = IDirectFB_Dispatcher_SetVideoMode;
     thiz->CreateSurface           = IDirectFB_Dispatcher_CreateSurface;
     thiz->CreatePalette           = IDirectFB_Dispatcher_CreatePalette;
     thiz->EnumScreens             = IDirectFB_Dispatcher_EnumScreens;
     thiz->GetScreen               = IDirectFB_Dispatcher_GetScreen;
     thiz->EnumDisplayLayers       = IDirectFB_Dispatcher_EnumDisplayLayers;
     thiz->GetDisplayLayer         = IDirectFB_Dispatcher_GetDisplayLayer;
     thiz->EnumInputDevices        = IDirectFB_Dispatcher_EnumInputDevices;
     thiz->GetInputDevice          = IDirectFB_Dispatcher_GetInputDevice;
     thiz->CreateEventBuffer       = IDirectFB_Dispatcher_CreateEventBuffer;
     thiz->CreateInputEventBuffer  = IDirectFB_Dispatcher_CreateInputEventBuffer;
     thiz->CreateImageProvider     = IDirectFB_Dispatcher_CreateImageProvider;
     thiz->CreateVideoProvider     = IDirectFB_Dispatcher_CreateVideoProvider;
     thiz->CreateFont              = IDirectFB_Dispatcher_CreateFont;
     thiz->CreateDataBuffer        = IDirectFB_Dispatcher_CreateDataBuffer;
     thiz->SetClipboardData        = IDirectFB_Dispatcher_SetClipboardData;
     thiz->GetClipboardData        = IDirectFB_Dispatcher_GetClipboardData;
     thiz->GetClipboardTimeStamp   = IDirectFB_Dispatcher_GetClipboardTimeStamp;
     thiz->Suspend                 = IDirectFB_Dispatcher_Suspend;
     thiz->Resume                  = IDirectFB_Dispatcher_Resume;
     thiz->WaitIdle                = IDirectFB_Dispatcher_WaitIdle;
     thiz->WaitForSync             = IDirectFB_Dispatcher_WaitForSync;
     thiz->GetInterface            = IDirectFB_Dispatcher_GetInterface;

     return DFB_OK;
}

