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
#include <idirectfbinputdevice_dispatcher.h>

#include "idirectfbinputdevice_requestor.h"


static DFBResult Probe();
static DFBResult Construct( IDirectFBInputDevice *thiz,
                            VoodooManager        *manager,
                            VoodooInstanceID      instance,
                            void                 *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBInputDevice, Requestor )


/**************************************************************************************************/

static void
IDirectFBInputDevice_Requestor_Destruct( IDirectFBInputDevice *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBInputDevice_Requestor_AddRef( IDirectFBInputDevice *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_Requestor_Release( IDirectFBInputDevice *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (--data->ref == 0)
          IDirectFBInputDevice_Requestor_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_Requestor_GetID( IDirectFBInputDevice *thiz,
                                      DFBInputDeviceID     *ret_id )
{
     DFBResult              ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;
     DFBInputDeviceID       id;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_id)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBINPUTDEVICE_METHOD_ID_GetID, VREQ_RESPOND, &response,
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

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_Requestor_CreateEventBuffer( IDirectFBInputDevice  *thiz,
                                                  IDirectFBEventBuffer **ret_interface )
{
     DFBResult              ret;
     IDirectFBEventBuffer  *buffer;
     IDirectFBEventBuffer  *dispatcher;
     VoodooInstanceID       instance;
     VoodooResponseMessage *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_interface)
          return DFB_INVARG;

     /* Create the real interface. */
     DIRECT_ALLOCATE_INTERFACE( buffer, IDirectFBEventBuffer );

     IDirectFBEventBuffer_Construct( buffer, NULL, NULL );

     /* Create the dispatcher. */
     ret = voodoo_construct_dispatcher( data->manager, "IDirectFBEventBuffer",
                                        buffer, data->instance, NULL, &instance, (void**) &dispatcher );
     if (ret) {
          buffer->Release( buffer );
          return ret;
     }

     /* Send the request including the instance ID of the dispatcher. */
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBINPUTDEVICE_METHOD_ID_CreateEventBuffer,
                                   VREQ_RESPOND, &response,
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
IDirectFBInputDevice_Requestor_AttachEventBuffer( IDirectFBInputDevice *thiz,
                                                  IDirectFBEventBuffer *buffer )
{
     DFBResult                             ret;
     IDirectFBEventBuffer_Dispatcher_data *buffer_data;
     VoodooResponseMessage                *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!buffer)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM( buffer, buffer_data, IDirectFBEventBuffer_Dispatcher );

     /* Send the request including the instance ID of the dispatcher. */
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBINPUTDEVICE_METHOD_ID_AttachEventBuffer,
                                   VREQ_RESPOND, &response,
                                   VMBT_ID, buffer_data->self,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBInputDevice_Requestor_DetachEventBuffer( IDirectFBInputDevice *thiz,
                                                  IDirectFBEventBuffer *buffer )
{
     DFBResult                             ret;
     IDirectFBEventBuffer_Dispatcher_data *buffer_data;
     VoodooResponseMessage                *response;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!buffer)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM( buffer, buffer_data, IDirectFBEventBuffer_Dispatcher );

     /* Send the request including the instance ID of the dispatcher. */
     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBINPUTDEVICE_METHOD_ID_DetachEventBuffer,
                                   VREQ_RESPOND, &response,
                                   VMBT_ID, buffer_data->self,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     return ret;
}

static DFBResult
IDirectFBInputDevice_Requestor_GetDescription( IDirectFBInputDevice      *thiz,
                                               DFBInputDeviceDescription *ret_desc )
{
     DFBResult              ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_desc)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBINPUTDEVICE_METHOD_ID_GetDescription,
                                   VREQ_RESPOND, &response,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_READ_DATA( parser, ret_desc, sizeof(DFBInputDeviceDescription) );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_Requestor_GetKeymapEntry( IDirectFBInputDevice      *thiz,
                                               int                        keycode,
                                               DFBInputDeviceKeymapEntry *ret_entry )
{
     DFBResult              ret;
     VoodooResponseMessage *response;
     VoodooMessageParser    parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_entry)
          return DFB_INVARG;

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBINPUTDEVICE_METHOD_ID_GetKeymapEntry,
                                   VREQ_RESPOND, &response,
                                   VMBT_INT, keycode,
                                   VMBT_NONE );
     if (ret)
          return ret;

     ret = response->result;
     if (ret) {
          voodoo_manager_finish_request( data->manager, response );
          return ret;
     }

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_READ_DATA( parser, ret_entry, sizeof(DFBInputDeviceKeymapEntry) );
     VOODOO_PARSER_END( parser );

     voodoo_manager_finish_request( data->manager, response );

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_Requestor_GetKeyState( IDirectFBInputDevice        *thiz,
                                            DFBInputDeviceKeyIdentifier  key_id,
                                            DFBInputDeviceKeyState      *ret_state )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_state)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Requestor_GetModifiers( IDirectFBInputDevice       *thiz,
                                             DFBInputDeviceModifierMask *ret_modifiers )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_modifiers)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Requestor_GetLockState( IDirectFBInputDevice    *thiz,
                                             DFBInputDeviceLockState *ret_locks )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_locks)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Requestor_GetButtons( IDirectFBInputDevice     *thiz,
                                           DFBInputDeviceButtonMask *ret_buttons )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_buttons)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Requestor_GetButtonState( IDirectFBInputDevice           *thiz,
                                               DFBInputDeviceButtonIdentifier  button,
                                               DFBInputDeviceButtonState      *ret_state )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_state || (int)button < DIBI_FIRST || button > DIBI_LAST)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Requestor_GetAxis( IDirectFBInputDevice         *thiz,
                                        DFBInputDeviceAxisIdentifier  axis,
                                        int                          *ret_pos )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_pos || (int)axis < DIAI_FIRST || axis > DIAI_LAST)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Requestor_GetXY( IDirectFBInputDevice *thiz,
                                      int                  *ret_x,
                                      int                  *ret_y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Requestor)

     if (!ret_x && !ret_y)
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
Construct( IDirectFBInputDevice *thiz,
           VoodooManager        *manager,
           VoodooInstanceID      instance,
           void                 *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBInputDevice_Requestor)

     data->ref      = 1;
     data->manager  = manager;
     data->instance = instance;

     thiz->AddRef             = IDirectFBInputDevice_Requestor_AddRef;
     thiz->Release            = IDirectFBInputDevice_Requestor_Release;
     thiz->GetID              = IDirectFBInputDevice_Requestor_GetID;
     thiz->GetDescription     = IDirectFBInputDevice_Requestor_GetDescription;
     thiz->GetKeymapEntry     = IDirectFBInputDevice_Requestor_GetKeymapEntry;
     thiz->CreateEventBuffer  = IDirectFBInputDevice_Requestor_CreateEventBuffer;
     thiz->AttachEventBuffer  = IDirectFBInputDevice_Requestor_AttachEventBuffer;
     thiz->DetachEventBuffer  = IDirectFBInputDevice_Requestor_DetachEventBuffer;
     thiz->GetKeyState        = IDirectFBInputDevice_Requestor_GetKeyState;
     thiz->GetModifiers       = IDirectFBInputDevice_Requestor_GetModifiers;
     thiz->GetLockState       = IDirectFBInputDevice_Requestor_GetLockState;
     thiz->GetButtons         = IDirectFBInputDevice_Requestor_GetButtons;
     thiz->GetButtonState     = IDirectFBInputDevice_Requestor_GetButtonState;
     thiz->GetAxis            = IDirectFBInputDevice_Requestor_GetAxis;
     thiz->GetXY              = IDirectFBInputDevice_Requestor_GetXY;

     return DFB_OK;
}

