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

#include <directfb.h>

#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include <input/idirectfbinputbuffer.h>
#include <media/idirectfbdatabuffer.h>

#include <idirectfb_dispatcher.h>


static DFBResult Probe();
static DFBResult Construct( IDirectFB *thiz, const char *host, int session );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFB, Requestor )


/**************************************************************************************************/

/*
 * private data struct of IDirectFB_Requestor
 */
typedef struct {
     int                  ref;      /* reference counter */

     VoodooClient        *client;
     VoodooManager       *manager;

     VoodooInstanceID     instance;

     DFBCooperativeLevel  level;    /* current cooperative level */
} IDirectFB_Requestor_data;

/**************************************************************************************************/

static void
IDirectFB_Requestor_Destruct( IDirectFB *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFB_Requestor_AddRef( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFB_Requestor_Release( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (--data->ref == 0)
          IDirectFB_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFB_Requestor_SetCooperativeLevel( IDirectFB           *thiz,
                                         DFBCooperativeLevel  level )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (level == data->level)
          return DFB_OK;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_SetCooperativeLevel, VREQ_RESPOND, &response,
                                   VMBT_INT, level,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK)
          data->level = level;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFB_Requestor_GetDeviceDescription( IDirectFB                    *thiz,
                                          DFBGraphicsDeviceDescription *ret_desc )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_GetDeviceDescription, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK) {
          VoodooMessageParser parser;

          VOODOO_PARSER_BEGIN( parser, response );
          VOODOO_PARSER_READ_DATA( parser, ret_desc, sizeof(DFBGraphicsDeviceDescription) );
          VOODOO_PARSER_END( parser );
     }

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFB_Requestor_EnumVideoModes( IDirectFB            *thiz,
                                    DFBVideoModeCallback  callbackfunc,
                                    void                 *callbackdata )
{
     DirectResult                              ret;
     VoodooResponseMessage                    *response;
     VoodooMessageParser                       parser;
     int                                       i, num;
     IDirectFB_Dispatcher_EnumVideoModes_Item *items;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!callbackfunc)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_EnumVideoModes, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_INT( parser, num );
     VOODOO_PARSER_COPY_DATA( parser, items );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     for (i=0; i<num; i++) {
          if (callbackfunc( items[i].width, items[i].height, items[i].bpp, callbackdata ) == DFENUM_CANCEL)
               return DFB_OK;
     }

     return DFB_OK;
}

static DFBResult
IDirectFB_Requestor_SetVideoMode( IDirectFB    *thiz,
                                  int           width,
                                  int           height,
                                  int           bpp )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (width < 1 || height < 1 || bpp < 1)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_SetVideoMode, VREQ_RESPOND, &response,
                                   VMBT_INT, width,
                                   VMBT_INT, height,
                                   VMBT_INT, bpp,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFB_Requestor_CreateSurface( IDirectFB                    *thiz,
                                   const DFBSurfaceDescription  *desc,
                                   IDirectFBSurface            **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!desc || !ret_interface)
          return DFB_INVARG;

     if (desc->flags & (DSDESC_PALETTE | DSDESC_PREALLOCATED))
          D_ONCE( "DSDESC_PALETTE and DSDESC_PREALLOCATED not supported yet" );

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_CreateSurface, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(DFBSurfaceDescription), desc,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBSurface",
                                            response->instance, NULL, &interface );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;

     return ret;
}

static DFBResult
IDirectFB_Requestor_CreatePalette( IDirectFB                    *thiz,
                                   const DFBPaletteDescription  *desc,
                                   IDirectFBPalette            **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!interface)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Requestor_EnumScreens( IDirectFB         *thiz,
                                 DFBScreenCallback  callbackfunc,
                                 void              *callbackdata )
{
     DirectResult                           ret;
     VoodooResponseMessage                 *response;
     VoodooMessageParser                    parser;
     int                                    i, num;
     IDirectFB_Dispatcher_EnumScreens_Item *items;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!callbackfunc)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_EnumScreens, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_INT( parser, num );
     VOODOO_PARSER_COPY_DATA( parser, items );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     for (i=0; i<num; i++) {
          if (callbackfunc( items[i].screen_id, items[i].desc, callbackdata ) == DFENUM_CANCEL)
               return DFB_OK;
     }

     return DFB_OK;
}

static DFBResult
IDirectFB_Requestor_GetScreen( IDirectFB        *thiz,
                               DFBScreenID       id,
                               IDirectFBScreen **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_GetScreen, VREQ_RESPOND, &response,
                                   VMBT_ID, id,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBScreen",
                                            response->instance, NULL, &interface );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;

     return ret;
}

static DFBResult
IDirectFB_Requestor_EnumDisplayLayers( IDirectFB               *thiz,
                                       DFBDisplayLayerCallback  callbackfunc,
                                       void                    *callbackdata )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!callbackfunc)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Requestor_GetDisplayLayer( IDirectFB              *thiz,
                                     DFBDisplayLayerID       id,
                                     IDirectFBDisplayLayer **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_GetDisplayLayer, VREQ_RESPOND, &response,
                                   VMBT_ID, id,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBDisplayLayer",
                                            response->instance, NULL, &interface );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;

     return ret;
}

static DFBResult
IDirectFB_Requestor_EnumInputDevices( IDirectFB              *thiz,
                                      DFBInputDeviceCallback  callbackfunc,
                                      void                   *callbackdata )
{
     DirectResult                                ret;
     VoodooResponseMessage                      *response;
     VoodooMessageParser                         parser;
     int                                         i, num;
     IDirectFB_Dispatcher_EnumInputDevices_Item *items;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!callbackfunc)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_EnumInputDevices, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     if (response->result) {
          voodoo_manager_finish_request( data->manager, response );
          return response->result;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_INT( parser, num );
     VOODOO_PARSER_COPY_DATA( parser, items );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     for (i=0; i<num; i++) {
          if (callbackfunc( items[i].device_id, items[i].desc, callbackdata ) == DFENUM_CANCEL)
               return DFB_OK;
     }

     return ret;
}

static DFBResult
IDirectFB_Requestor_GetInputDevice( IDirectFB             *thiz,
                                    DFBInputDeviceID       id,
                                    IDirectFBInputDevice **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_GetInputDevice, VREQ_RESPOND, &response,
                                   VMBT_ID, id,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBInputDevice",
                                            response->instance, NULL, &interface );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;

     return ret;
}

static DFBResult
IDirectFB_Requestor_CreateEventBuffer( IDirectFB             *thiz,
                                       IDirectFBEventBuffer **ret_interface)
{
     DFBResult             ret;
     IDirectFBEventBuffer *buffer;
     IDirectFBEventBuffer *dispatcher;
     VoodooInstanceID      instance;
     void                 *ptr;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     /* Create the real interface. */
     DIRECT_ALLOCATE_INTERFACE( buffer, IDirectFBEventBuffer );

     IDirectFBEventBuffer_Construct( buffer, NULL, NULL );

     /* Create the dispatcher. */
     ret = voodoo_construct_dispatcher( data->manager, "IDirectFBEventBuffer", buffer,
                                        data->instance, NULL, &instance, &ptr );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     dispatcher = ptr;

     /* Send the request including the instance ID of the dispatcher. */
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_CreateEventBuffer, VREQ_NONE, NULL,
                                   VMBT_ID,  instance,
                                   VMBT_NONE );
     if (ret) {
          dispatcher->Release( dispatcher );
          return ret;
     }

     /* Return the dispatcher interface. */
     *ret_interface = dispatcher;

     return DFB_OK;
}

static DFBResult
IDirectFB_Requestor_CreateInputEventBuffer( IDirectFB                   *thiz,
                                            DFBInputDeviceCapabilities   caps,
                                            DFBBoolean                   global,
                                            IDirectFBEventBuffer       **ret_interface)
{
     DFBResult             ret;
     IDirectFBEventBuffer *buffer;
     IDirectFBEventBuffer *dispatcher;
     VoodooInstanceID      instance;
     void                 *ptr;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     /* Create the real interface. */
     DIRECT_ALLOCATE_INTERFACE( buffer, IDirectFBEventBuffer );

     IDirectFBEventBuffer_Construct( buffer, NULL, NULL );

     /* Create the dispatcher. */
     ret = voodoo_construct_dispatcher( data->manager, "IDirectFBEventBuffer", buffer,
                                        data->instance, NULL, &instance, &ptr );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     dispatcher = ptr;

     /* Send the request including the instance ID of the dispatcher. */
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_CreateInputEventBuffer, VREQ_NONE, NULL,
                                   VMBT_ID,  instance,
                                   VMBT_INT, caps,
                                   VMBT_INT, global,
                                   VMBT_NONE );
     if (ret) {
          dispatcher->Release( dispatcher );
          return ret;
     }

     /* Return the dispatcher interface. */
     *ret_interface = dispatcher;

     return DFB_OK;
}

static DFBResult
IDirectFB_Requestor_CreateImageProvider( IDirectFB               *thiz,
                                         const char              *filename,
                                         IDirectFBImageProvider **ret_interface )
{
     DFBResult                 ret;
     DFBDataBufferDescription  desc;
     IDirectFBDataBuffer      *buffer;
     IDirectFBImageProvider   *provider;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     /* Check arguments */
     if (!filename || !ret_interface)
          return DFB_INVARG;

     /* Create a data buffer. */
     desc.flags = DBDESC_FILE;
     desc.file  = filename;

     ret = thiz->CreateDataBuffer( thiz, &desc, &buffer );
     if (ret)
          return ret;

     /* Create (probing) the image provider. */
     ret = buffer->CreateImageProvider( buffer, &provider );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     /* We don't need it anymore, image provider has its own reference. */
     buffer->Release( buffer );

     /* Return the new provider. */
     *ret_interface = provider;

     return ret;
}

static DFBResult
IDirectFB_Requestor_CreateVideoProvider( IDirectFB               *thiz,
                                         const char              *filename,
                                         IDirectFBVideoProvider **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     /* Check arguments */
     if (!interface || !filename)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Requestor_CreateFont( IDirectFB                 *thiz,
                                const char                *filename,
                                const DFBFontDescription  *desc,
                                IDirectFBFont            **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     if (!filename || !desc) {
          D_ONCE( "unimplemented" );
          return DFB_UNSUPPORTED;
     }

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_CreateFont, VREQ_RESPOND, &response,
                                   VMBT_STRING, filename,
                                   VMBT_DATA, sizeof(DFBFontDescription), desc,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret == DFB_OK)
          ret = voodoo_construct_requestor( data->manager, "IDirectFBFont",
                                            response->instance, NULL, &interface );

     voodoo_manager_finish_request( data->manager, response );

     *ret_interface = interface;

     return ret;
}

static DFBResult
IDirectFB_Requestor_CreateDataBuffer( IDirectFB                       *thiz,
                                      const DFBDataBufferDescription  *desc,
                                      IDirectFBDataBuffer            **ret_interface )
{
     DFBResult            ret;
     IDirectFBDataBuffer *buffer;
     IDirectFBDataBuffer *dispatcher;
     VoodooInstanceID     instance;
     void                *ptr;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     /* Create the real interface. */
     if (!desc) {
          DIRECT_ALLOCATE_INTERFACE( buffer, IDirectFBDataBuffer );

          ret = IDirectFBDataBuffer_Streamed_Construct( buffer, NULL );
     }
     else if (desc->flags & DBDESC_FILE) {
          if (!desc->file)
               return DFB_INVARG;

          DIRECT_ALLOCATE_INTERFACE( buffer, IDirectFBDataBuffer );

          ret = IDirectFBDataBuffer_File_Construct( buffer, desc->file, NULL );
     }
     else if (desc->flags & DBDESC_MEMORY) {
          if (!desc->memory.data || !desc->memory.length)
               return DFB_INVARG;

          DIRECT_ALLOCATE_INTERFACE( buffer, IDirectFBDataBuffer );

          ret = IDirectFBDataBuffer_Memory_Construct( buffer,
                                                      desc->memory.data,
                                                      desc->memory.length,
                                                      NULL );
     }
     else
          return DFB_INVARG;

     if (ret)
          return ret;

     /* Create the dispatcher. */
     ret = voodoo_construct_dispatcher( data->manager, "IDirectFBDataBuffer",
                                        buffer, data->instance, NULL, &instance, &ptr );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     dispatcher = ptr;

     /* Send the request including the instance ID of the dispatcher. */
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_CreateDataBuffer, VREQ_NONE, NULL,
                                   VMBT_ID, instance,
                                   VMBT_NONE );
     if (ret) {
          dispatcher->Release( dispatcher );
          return ret;
     }

     /* Return the dispatcher(!) interface. */
     *ret_interface = dispatcher;

     return DFB_OK;
}

static DFBResult
IDirectFB_Requestor_SetClipboardData( IDirectFB      *thiz,
                                      const char     *mime_type,
                                      const void     *data,
                                      unsigned int    size,
                                      struct timeval *timestamp )
{
     if (!mime_type || !data || !size)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Requestor_GetClipboardData( IDirectFB     *thiz,
                                      char         **mime_type,
                                      void         **clip_data,
                                      unsigned int  *size )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!mime_type && !clip_data && !size)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Requestor_GetClipboardTimeStamp( IDirectFB      *thiz,
                                           struct timeval *timestamp )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     if (!timestamp)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Requestor_Suspend( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Requestor_Resume( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Requestor_WaitIdle( IDirectFB *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFB_METHOD_ID_WaitIdle, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFB_Requestor_WaitForSync( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFB_Requestor_GetInterface( IDirectFB   *thiz,
                                  const char  *type,
                                  const char  *implementation,
                                  void        *arg,
                                  void       **interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB_Requestor)

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

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
static DFBResult
Construct( IDirectFB *thiz, const char *host, int session )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFB_Requestor)

     data->ref   = 1;
     data->level = DFSCL_NORMAL;

     ret = voodoo_client_create( host, session, &data->client );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->manager = voodoo_client_manager( data->client );

     ret = voodoo_manager_super( data->manager, "IDirectFB", &data->instance );
     if (ret) {
          voodoo_client_destroy( data->client );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     thiz->AddRef                  = IDirectFB_Requestor_AddRef;
     thiz->Release                 = IDirectFB_Requestor_Release;
     thiz->SetCooperativeLevel     = IDirectFB_Requestor_SetCooperativeLevel;
     thiz->GetDeviceDescription    = IDirectFB_Requestor_GetDeviceDescription;
     thiz->EnumVideoModes          = IDirectFB_Requestor_EnumVideoModes;
     thiz->SetVideoMode            = IDirectFB_Requestor_SetVideoMode;
     thiz->CreateSurface           = IDirectFB_Requestor_CreateSurface;
     thiz->CreatePalette           = IDirectFB_Requestor_CreatePalette;
     thiz->EnumScreens             = IDirectFB_Requestor_EnumScreens;
     thiz->GetScreen               = IDirectFB_Requestor_GetScreen;
     thiz->EnumDisplayLayers       = IDirectFB_Requestor_EnumDisplayLayers;
     thiz->GetDisplayLayer         = IDirectFB_Requestor_GetDisplayLayer;
     thiz->EnumInputDevices        = IDirectFB_Requestor_EnumInputDevices;
     thiz->GetInputDevice          = IDirectFB_Requestor_GetInputDevice;
     thiz->CreateEventBuffer       = IDirectFB_Requestor_CreateEventBuffer;
     thiz->CreateInputEventBuffer  = IDirectFB_Requestor_CreateInputEventBuffer;
     thiz->CreateImageProvider     = IDirectFB_Requestor_CreateImageProvider;
     thiz->CreateVideoProvider     = IDirectFB_Requestor_CreateVideoProvider;
     thiz->CreateFont              = IDirectFB_Requestor_CreateFont;
     thiz->CreateDataBuffer        = IDirectFB_Requestor_CreateDataBuffer;
     thiz->SetClipboardData        = IDirectFB_Requestor_SetClipboardData;
     thiz->GetClipboardData        = IDirectFB_Requestor_GetClipboardData;
     thiz->GetClipboardTimeStamp   = IDirectFB_Requestor_GetClipboardTimeStamp;
     thiz->Suspend                 = IDirectFB_Requestor_Suspend;
     thiz->Resume                  = IDirectFB_Requestor_Resume;
     thiz->WaitIdle                = IDirectFB_Requestor_WaitIdle;
     thiz->WaitForSync             = IDirectFB_Requestor_WaitForSync;
     thiz->GetInterface            = IDirectFB_Requestor_GetInterface;

     return DFB_OK;
}

