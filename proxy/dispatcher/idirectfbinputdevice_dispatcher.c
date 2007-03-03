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

#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include "idirectfbinputdevice_dispatcher.h"

#include <idirectfbeventbuffer_requestor.h>


static DFBResult Probe();
static DFBResult Construct( IDirectFBInputDevice *thiz,
                            IDirectFBInputDevice *real,
                            VoodooManager        *manager,
                            VoodooInstanceID      super,
                            void                 *arg,
                            VoodooInstanceID     *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBInputDevice, Dispatcher )


/**************************************************************************************************/

static void
IDirectFBInputDevice_Dispatcher_Destruct( IDirectFBInputDevice *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DFBResult
IDirectFBInputDevice_Dispatcher_AddRef( IDirectFBInputDevice *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_Release( IDirectFBInputDevice *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (--data->ref == 0)
          IDirectFBInputDevice_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_GetID( IDirectFBInputDevice *thiz,
                                       DFBInputDeviceID     *ret_id )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (!ret_id)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_CreateEventBuffer( IDirectFBInputDevice  *thiz,
                                                   IDirectFBEventBuffer **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_AttachEventBuffer( IDirectFBInputDevice *thiz,
                                                   IDirectFBEventBuffer *buffer )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_DetachEventBuffer( IDirectFBInputDevice *thiz,
                                                   IDirectFBEventBuffer *buffer )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_GetDescription( IDirectFBInputDevice      *thiz,
                                                DFBInputDeviceDescription *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (!ret_desc)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_GetKeymapEntry( IDirectFBInputDevice      *thiz,
                                                int                        keycode,
                                                DFBInputDeviceKeymapEntry *ret_entry )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (!ret_entry)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_GetKeyState( IDirectFBInputDevice        *thiz,
                                             DFBInputDeviceKeyIdentifier  key_id,
                                             DFBInputDeviceKeyState      *ret_state )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (!ret_state)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_GetModifiers( IDirectFBInputDevice       *thiz,
                                              DFBInputDeviceModifierMask *ret_modifiers )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (!ret_modifiers)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_GetLockState( IDirectFBInputDevice    *thiz,
                                              DFBInputDeviceLockState *ret_locks )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (!ret_locks)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_GetButtons( IDirectFBInputDevice     *thiz,
                                            DFBInputDeviceButtonMask *ret_buttons )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (!ret_buttons)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_GetButtonState( IDirectFBInputDevice           *thiz,
                                                DFBInputDeviceButtonIdentifier  button,
                                                DFBInputDeviceButtonState      *ret_state )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (!ret_state || (int)button < DIBI_FIRST || button > DIBI_LAST)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_GetAxis( IDirectFBInputDevice         *thiz,
                                         DFBInputDeviceAxisIdentifier  axis,
                                         int                          *ret_pos )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (!ret_pos || (int)axis < DIAI_FIRST || axis > DIAI_LAST)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBInputDevice_Dispatcher_GetXY( IDirectFBInputDevice *thiz,
                                       int                  *ret_x,
                                       int                  *ret_y )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     if (!ret_x && !ret_y)
          return DFB_INVARG;

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_GetID( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult     ret;
     DFBInputDeviceID id;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     ret = real->GetID( real, &id );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_ID, id,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetDescription( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult              ret;
     DFBInputDeviceDescription desc;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     ret = real->GetDescription( real, &desc );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(desc), &desc,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetKeymapEntry( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult              ret;
     VoodooMessageParser       parser;
     int                       keycode;
     DFBInputDeviceKeymapEntry entry;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_INT( parser, keycode );
     VOODOO_PARSER_END( parser );

     ret = real->GetKeymapEntry( real, keycode, &entry );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, msg->header.serial,
                                    DFB_OK, VOODOO_INSTANCE_NONE,
                                    VMBT_DATA, sizeof(entry), &entry,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_CreateEventBuffer( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                            VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult          ret;
     IDirectFBEventBuffer *buffer;
     IDirectFBEventBuffer *requestor;
     VoodooInstanceID      instance;
     VoodooMessageParser   parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     ret = real->CreateEventBuffer( real, &buffer );
     if (ret)
          return ret;

     ret = voodoo_construct_requestor( manager, "IDirectFBEventBuffer",
                                       instance, buffer, (void**) &requestor );
     if (ret)
          buffer->Release( buffer );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_AttachEventBuffer( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                            VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                         ret;
     IDirectFBEventBuffer                *buffer;
     IDirectFBEventBuffer_Requestor_data *buffer_data;
     VoodooInstanceID                     instance;
     VoodooMessageParser                  parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     ret = voodoo_manager_lookup_remote( manager, instance, (void**) &buffer );
     if (ret)
          return ret;

     DIRECT_INTERFACE_GET_DATA_FROM( buffer, buffer_data, IDirectFBEventBuffer_Requestor );

     ret = real->AttachEventBuffer( real, buffer_data->src );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_DetachEventBuffer( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                            VoodooManager *manager, VoodooRequestMessage *msg )
{
     DirectResult                         ret;
     IDirectFBEventBuffer                *buffer;
     IDirectFBEventBuffer_Requestor_data *buffer_data;
     VoodooInstanceID                     instance;
     VoodooMessageParser                  parser;

     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_END( parser );

     ret = voodoo_manager_lookup_remote( manager, instance, (void**) &buffer );
     if (ret)
          return ret;

     DIRECT_INTERFACE_GET_DATA_FROM( buffer, buffer_data, IDirectFBEventBuffer_Requestor );

     ret = real->DetachEventBuffer( real, buffer_data->src );

     return voodoo_manager_respond( manager, msg->header.serial,
                                    ret, VOODOO_INSTANCE_NONE,
                                    VMBT_NONE );
}

static DirectResult
Dispatch_GetKeyState( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DirectResult
Dispatch_GetModifiers( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DirectResult
Dispatch_GetLockState( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                       VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DirectResult
Dispatch_GetButtons( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                     VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DirectResult
Dispatch_GetButtonState( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DirectResult
Dispatch_GetAxis( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DirectResult
Dispatch_GetXY( IDirectFBInputDevice *thiz, IDirectFBInputDevice *real,
                VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBInputDevice_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBInputDevice/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBINPUTDEVICE_METHOD_ID_GetID:
               return Dispatch_GetID( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_GetDescription:
               return Dispatch_GetDescription( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_GetKeymapEntry:
               return Dispatch_GetKeymapEntry( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_CreateEventBuffer:
               return Dispatch_CreateEventBuffer( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_AttachEventBuffer:
               return Dispatch_AttachEventBuffer( dispatcher, real, manager, msg );
               
          case IDIRECTFBINPUTDEVICE_METHOD_ID_DetachEventBuffer:
               return Dispatch_DetachEventBuffer( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_GetKeyState:
               return Dispatch_GetKeyState( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_GetModifiers:
               return Dispatch_GetModifiers( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_GetLockState:
               return Dispatch_GetLockState( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_GetButtons:
               return Dispatch_GetButtons( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_GetButtonState:
               return Dispatch_GetButtonState( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_GetAxis:
               return Dispatch_GetAxis( dispatcher, real, manager, msg );

          case IDIRECTFBINPUTDEVICE_METHOD_ID_GetXY:
               return Dispatch_GetXY( dispatcher, real, manager, msg );
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
Construct( IDirectFBInputDevice *thiz,     /* Dispatcher interface */
           IDirectFBInputDevice *real,     /* Real interface implementation */
           VoodooManager        *manager,  /* Manager of the Voodoo framework */
           VoodooInstanceID      super,    /* Instance ID of the super interface */
           void                 *arg,      /* Optional arguments to constructor */
           VoodooInstanceID     *ret_instance )
{
     DFBResult        ret;
     VoodooInstanceID instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBInputDevice_Dispatcher)

     D_ASSERT( real != NULL );
     D_ASSERT( manager != NULL );
     D_ASSERT( super != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_instance != NULL );

     /* Register the dispatcher, getting a new instance ID that refers to it. */
     ret = voodoo_manager_register_local( manager, false, thiz, real, Dispatch, &instance );
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
     thiz->AddRef             = IDirectFBInputDevice_Dispatcher_AddRef;
     thiz->Release            = IDirectFBInputDevice_Dispatcher_Release;
     thiz->GetID              = IDirectFBInputDevice_Dispatcher_GetID;
     thiz->GetDescription     = IDirectFBInputDevice_Dispatcher_GetDescription;
     thiz->GetKeymapEntry     = IDirectFBInputDevice_Dispatcher_GetKeymapEntry;
     thiz->CreateEventBuffer  = IDirectFBInputDevice_Dispatcher_CreateEventBuffer;
     thiz->AttachEventBuffer  = IDirectFBInputDevice_Dispatcher_AttachEventBuffer;
     thiz->DetachEventBuffer  = IDirectFBInputDevice_Dispatcher_DetachEventBuffer;
     thiz->GetKeyState        = IDirectFBInputDevice_Dispatcher_GetKeyState;
     thiz->GetModifiers       = IDirectFBInputDevice_Dispatcher_GetModifiers;
     thiz->GetLockState       = IDirectFBInputDevice_Dispatcher_GetLockState;
     thiz->GetButtons         = IDirectFBInputDevice_Dispatcher_GetButtons;
     thiz->GetButtonState     = IDirectFBInputDevice_Dispatcher_GetButtonState;
     thiz->GetAxis            = IDirectFBInputDevice_Dispatcher_GetAxis;
     thiz->GetXY              = IDirectFBInputDevice_Dispatcher_GetXY;

     return DFB_OK;
}

