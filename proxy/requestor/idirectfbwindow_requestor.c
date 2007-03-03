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

#include <idirectfbeventbuffer_dispatcher.h>
#include <idirectfbwindow_dispatcher.h>

#include "idirectfbsurface_requestor.h"
#include "idirectfbwindow_requestor.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBWindow  *thiz,
                            VoodooManager    *manager,
                            VoodooInstanceID  instance,
                            void             *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBWindow, Requestor )


/**************************************************************************************************/

static void
IDirectFBWindow_Requestor_Destruct( IDirectFBWindow *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBWindow_Requestor_AddRef( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Requestor_Release( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (--data->ref == 0)
          IDirectFBWindow_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Requestor_CreateEventBuffer( IDirectFBWindow       *thiz,
                                             IDirectFBEventBuffer **ret_interface )
{
     DFBResult              ret;
     IDirectFBEventBuffer  *buffer;
     IDirectFBEventBuffer  *dispatcher;
     VoodooInstanceID       instance;
     VoodooResponseMessage *response;
     void                  *ptr;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     /* Create the real interface. */
     DIRECT_ALLOCATE_INTERFACE( buffer, IDirectFBEventBuffer );

     IDirectFBEventBuffer_Construct( buffer, NULL, NULL );

     /* Create the dispatcher. */
     ret = voodoo_construct_dispatcher( data->manager, "IDirectFBEventBuffer",
                                        buffer, data->instance, NULL, &instance, &ptr );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     dispatcher = ptr;

     /* Send the request including the instance ID of the dispatcher. */
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_CreateEventBuffer, VREQ_RESPOND, &response,
                                   VMBT_ID,  instance,
                                   VMBT_NONE );
     if (ret) {
          dispatcher->Release( dispatcher );
          return ret;
     }

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     voodoo_manager_finish_request( data->manager, response );

     /* Return the dispatcher interface. */
     *ret_interface = dispatcher;

     return DFB_OK;
}

static DFBResult
IDirectFBWindow_Requestor_AttachEventBuffer( IDirectFBWindow      *thiz,
                                             IDirectFBEventBuffer *buffer )
{
     DFBResult                             ret;
     IDirectFBEventBuffer_Dispatcher_data *buffer_data;
     VoodooResponseMessage                *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (!buffer)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM( buffer, buffer_data, IDirectFBEventBuffer_Dispatcher );

     /* Send the request including the instance ID of the dispatcher. */
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_AttachEventBuffer, VREQ_RESPOND, &response,
                                   VMBT_ID, buffer_data->self,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_DetachEventBuffer( IDirectFBWindow      *thiz,
                                             IDirectFBEventBuffer *buffer )
{
     DFBResult                             ret;
     IDirectFBEventBuffer_Dispatcher_data *buffer_data;
     VoodooResponseMessage                *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (!buffer)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM( buffer, buffer_data, IDirectFBEventBuffer_Dispatcher );

     /* Send the request including the instance ID of the dispatcher. */
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_DetachEventBuffer, VREQ_RESPOND, &response,
                                   VMBT_ID, buffer_data->self,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_EnableEvents( IDirectFBWindow       *thiz,
                                        DFBWindowEventType     mask )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_DisableEvents( IDirectFBWindow    *thiz,
                                         DFBWindowEventType  mask )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_GetID( IDirectFBWindow *thiz,
                                 DFBWindowID     *ret_id )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     DFBWindowID            id;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (!ret_id)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_GetID, VREQ_RESPOND, &response,
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
IDirectFBWindow_Requestor_GetPosition( IDirectFBWindow *thiz,
                                       int             *x,
                                       int             *y )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     const DFBPoint        *position;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (!x && !y)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_GetPosition, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_DATA( parser, position );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     if (x)
          *x = position->x;

     if (y)
          *y = position->y;

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_GetSize( IDirectFBWindow *thiz,
                                   int             *width,
                                   int             *height )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     const DFBDimension    *size;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (!width && !height)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_GetSize, VREQ_RESPOND, &response,
                                   VMBT_NONE );
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
IDirectFBWindow_Requestor_GetSurface( IDirectFBWindow   *thiz,
                                      IDirectFBSurface **ret_interface )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     void                  *interface = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_GetSurface, VREQ_RESPOND, &response,
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
IDirectFBWindow_Requestor_SetProperty( IDirectFBWindow  *thiz,
                                       const char       *key,
                                       void             *value,
                                       void            **old_value )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_GetProperty( IDirectFBWindow  *thiz,
                                       const char       *key,
                                       void            **value )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_RemoveProperty( IDirectFBWindow  *thiz,
                                          const char       *key,
                                          void            **value )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}


static DFBResult
IDirectFBWindow_Requestor_SetOptions( IDirectFBWindow  *thiz,
                                      DFBWindowOptions  options )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (options & ~DWOP_ALL)
          return DFB_INVARG;

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBWINDOW_METHOD_ID_SetOptions, VREQ_NONE, NULL,
                                    VMBT_INT, options,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBWindow_Requestor_GetOptions( IDirectFBWindow  *thiz,
                                      DFBWindowOptions *ret_options )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     DFBWindowOptions       options;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (!ret_options)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_GetOptions, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_INT( parser, options );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_options = options;

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_SetColorKey( IDirectFBWindow *thiz,
                                       u8               r,
                                       u8               g,
                                       u8               b )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_SetColorKeyIndex( IDirectFBWindow *thiz,
                                            unsigned int     index )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_SetOpaqueRegion( IDirectFBWindow *thiz,
                                           int              x1,
                                           int              y1,
                                           int              x2,
                                           int              y2 )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_SetOpacity( IDirectFBWindow *thiz,
                                      u8               opacity )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBWINDOW_METHOD_ID_SetOpacity, VREQ_NONE, NULL,
                                    VMBT_UINT, opacity,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBWindow_Requestor_GetOpacity( IDirectFBWindow *thiz,
                                      u8              *ret_opacity )
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     u8                     opacity;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (!ret_opacity)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_GetOpacity, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_UINT( parser, opacity );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     *ret_opacity = opacity;

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_SetCursorShape( IDirectFBWindow  *thiz,
                                          IDirectFBSurface *shape,
                                          int               hot_x,
                                          int               hot_y )
{
     DFBPoint                         hot = { hot_x, hot_y };
     DirectResult                     ret;
     VoodooResponseMessage           *response;
     IDirectFBSurface_Requestor_data *shape_data;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     if (!shape)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM( shape, shape_data, IDirectFBSurface_Requestor);

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_SetCursorShape, VREQ_RESPOND, &response,
                                   VMBT_ID, shape_data->instance,
                                   VMBT_DATA, sizeof(DFBPoint), &hot,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_RequestFocus( IDirectFBWindow *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_RequestFocus, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_GrabKeyboard( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_UngrabKeyboard( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_GrabPointer( IDirectFBWindow *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_GrabPointer, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_UngrabPointer( IDirectFBWindow *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_UngrabPointer, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_GrabKey( IDirectFBWindow            *thiz,
                                   DFBInputDeviceKeySymbol     symbol,
                                   DFBInputDeviceModifierMask  modifiers )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_UngrabKey( IDirectFBWindow            *thiz,
                                     DFBInputDeviceKeySymbol     symbol,
                                     DFBInputDeviceModifierMask  modifiers )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_Move( IDirectFBWindow *thiz, int dx, int dy )
{
     DFBPoint point = { dx, dy };

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBWINDOW_METHOD_ID_Move, VREQ_NONE, NULL,
                                    VMBT_DATA, sizeof(point), &point,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBWindow_Requestor_MoveTo( IDirectFBWindow *thiz, int x, int y )
{
     DFBPoint point = { x, y };

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBWINDOW_METHOD_ID_MoveTo, VREQ_NONE, NULL,
                                    VMBT_DATA, sizeof(point), &point,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBWindow_Requestor_Resize( IDirectFBWindow *thiz,
                                  int              width,
                                  int              height )
{
     DirectResult           ret;
     DFBDimension           size = { width, height };
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_Resize, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(size), &size,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_Raise( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBWINDOW_METHOD_ID_Raise, VREQ_NONE, NULL,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBWindow_Requestor_SetStackingClass( IDirectFBWindow        *thiz,
                                            DFBWindowStackingClass  stacking_class )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBWINDOW_METHOD_ID_SetStackingClass, VREQ_NONE, NULL,
                                    VMBT_INT, stacking_class,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBWindow_Requestor_Lower( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBWINDOW_METHOD_ID_Lower, VREQ_NONE, NULL,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBWindow_Requestor_RaiseToTop( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBWINDOW_METHOD_ID_RaiseToTop, VREQ_NONE, NULL,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBWindow_Requestor_LowerToBottom( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBWINDOW_METHOD_ID_LowerToBottom, VREQ_NONE, NULL,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBWindow_Requestor_PutAtop( IDirectFBWindow *thiz,
                                   IDirectFBWindow *lower )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_PutBelow( IDirectFBWindow *thiz,
                                    IDirectFBWindow *upper )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBWindow_Requestor_Close( IDirectFBWindow *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     return voodoo_manager_request( data->manager, data->instance,
                                    IDIRECTFBWINDOW_METHOD_ID_Close, VREQ_NONE, NULL,
                                    VMBT_NONE );
}

static DFBResult
IDirectFBWindow_Requestor_Destroy( IDirectFBWindow *thiz )
{
     DirectResult           ret;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_Destroy, VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_SetBounds( IDirectFBWindow *thiz,
                                     int x, int y, int w, int h )
{
     DirectResult           ret;
     DFBRectangle           bounds = { x, y, w, h };
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_SetBounds, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(bounds), &bounds,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBWindow_Requestor_ResizeSurface( IDirectFBWindow *thiz,
                                         int              width,
                                         int              height )
{
     DirectResult           ret;
     DFBDimension           size = { width, height };
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindow_Requestor)

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOW_METHOD_ID_ResizeSurface, VREQ_RESPOND, &response,
                                   VMBT_DATA, sizeof(size), &size,
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

static DFBResult
Construct( IDirectFBWindow  *thiz,
           VoodooManager    *manager,
           VoodooInstanceID  instance,
           void             *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBWindow_Requestor)

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;

     thiz->AddRef             = IDirectFBWindow_Requestor_AddRef;
     thiz->Release            = IDirectFBWindow_Requestor_Release;
     thiz->GetID              = IDirectFBWindow_Requestor_GetID;
     thiz->GetPosition        = IDirectFBWindow_Requestor_GetPosition;
     thiz->GetSize            = IDirectFBWindow_Requestor_GetSize;
     thiz->CreateEventBuffer  = IDirectFBWindow_Requestor_CreateEventBuffer;
     thiz->AttachEventBuffer  = IDirectFBWindow_Requestor_AttachEventBuffer;
     thiz->DetachEventBuffer  = IDirectFBWindow_Requestor_DetachEventBuffer;
     thiz->EnableEvents       = IDirectFBWindow_Requestor_EnableEvents;
     thiz->DisableEvents      = IDirectFBWindow_Requestor_DisableEvents;
     thiz->GetSurface         = IDirectFBWindow_Requestor_GetSurface;
     thiz->SetProperty        = IDirectFBWindow_Requestor_SetProperty;
     thiz->GetProperty        = IDirectFBWindow_Requestor_GetProperty;
     thiz->RemoveProperty     = IDirectFBWindow_Requestor_RemoveProperty;
     thiz->SetOptions         = IDirectFBWindow_Requestor_SetOptions;
     thiz->GetOptions         = IDirectFBWindow_Requestor_GetOptions;
     thiz->SetColorKey        = IDirectFBWindow_Requestor_SetColorKey;
     thiz->SetColorKeyIndex   = IDirectFBWindow_Requestor_SetColorKeyIndex;
     thiz->SetOpaqueRegion    = IDirectFBWindow_Requestor_SetOpaqueRegion;
     thiz->SetOpacity         = IDirectFBWindow_Requestor_SetOpacity;
     thiz->GetOpacity         = IDirectFBWindow_Requestor_GetOpacity;
     thiz->SetCursorShape     = IDirectFBWindow_Requestor_SetCursorShape;
     thiz->RequestFocus       = IDirectFBWindow_Requestor_RequestFocus;
     thiz->GrabKeyboard       = IDirectFBWindow_Requestor_GrabKeyboard;
     thiz->UngrabKeyboard     = IDirectFBWindow_Requestor_UngrabKeyboard;
     thiz->GrabPointer        = IDirectFBWindow_Requestor_GrabPointer;
     thiz->UngrabPointer      = IDirectFBWindow_Requestor_UngrabPointer;
     thiz->GrabKey            = IDirectFBWindow_Requestor_GrabKey;
     thiz->UngrabKey          = IDirectFBWindow_Requestor_UngrabKey;
     thiz->Move               = IDirectFBWindow_Requestor_Move;
     thiz->MoveTo             = IDirectFBWindow_Requestor_MoveTo;
     thiz->Resize             = IDirectFBWindow_Requestor_Resize;
     thiz->SetStackingClass   = IDirectFBWindow_Requestor_SetStackingClass;
     thiz->Raise              = IDirectFBWindow_Requestor_Raise;
     thiz->Lower              = IDirectFBWindow_Requestor_Lower;
     thiz->RaiseToTop         = IDirectFBWindow_Requestor_RaiseToTop;
     thiz->LowerToBottom      = IDirectFBWindow_Requestor_LowerToBottom;
     thiz->PutAtop            = IDirectFBWindow_Requestor_PutAtop;
     thiz->PutBelow           = IDirectFBWindow_Requestor_PutBelow;
     thiz->Close              = IDirectFBWindow_Requestor_Close;
     thiz->Destroy            = IDirectFBWindow_Requestor_Destroy;
     thiz->SetBounds          = IDirectFBWindow_Requestor_SetBounds;
     thiz->ResizeSurface      = IDirectFBWindow_Requestor_ResizeSurface;

     return DFB_OK;
}

