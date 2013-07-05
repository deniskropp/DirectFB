/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/client.h>
#include <voodoo/interface.h>
#include <voodoo/manager.h>

#include "idirectfbwindows_dispatcher.h"

#include "idirectfbwindows_requestor.h"


static DFBResult Probe( void );
static DFBResult Construct( IDirectFBWindows *thiz,
                            VoodooManager    *manager,
                            VoodooInstanceID  instance,
                            void             *arg );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBWindows, Requestor )


/**********************************************************************************************************************/

static void
IDirectFBWindows_Requestor_Destruct( IDirectFBWindows *thiz )
{
     IDirectFBWindows_Requestor_data *data = thiz->priv;

     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     voodoo_manager_request( data->manager, data->instance,
                             IDIRECTFBWINDOWS_METHOD_ID_Release, VREQ_NONE, NULL,
                             VMBT_NONE );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**********************************************************************************************************************/

static DirectResult
IDirectFBWindows_Requestor_AddRef( IDirectFBWindows *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows_Requestor)

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBWindows_Requestor_Release( IDirectFBWindows *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows_Requestor)

     if (--data->ref == 0)
          IDirectFBWindows_Requestor_Destruct( thiz );

     return DFB_OK;
}

/**********************************************************************************************************************/

typedef struct {
     DirectLink           link;

     DFBWindowsWatcher    watcher;
     void                *context;

     VoodooInstanceID     instance;
} RegisteredWatcher;

static DirectResult
IDirectFBWindows_Requestor_DispatchWatcher_WindowAdd( IDirectFBWindows *thiz, RegisteredWatcher *registered,
                                                      VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser  parser;
     const DFBWindowInfo *info;

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_DATA( parser, info );
     VOODOO_PARSER_END( parser );

     D_ASSERT( registered->watcher.WindowAdd != NULL );

     registered->watcher.WindowAdd( registered->context, info );

     return DR_OK;
}

static DirectResult
IDirectFBWindows_Requestor_DispatchWatcher_WindowRemove( IDirectFBWindows *thiz, RegisteredWatcher *registered,
                                                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser parser;
     VoodooInstanceID    window_id;

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, window_id );
     VOODOO_PARSER_END( parser );

     D_ASSERT( registered->watcher.WindowRemove != NULL );

     registered->watcher.WindowRemove( registered->context, window_id );

     return DR_OK;
}

static DirectResult
IDirectFBWindows_Requestor_DispatchWatcher_WindowConfig( IDirectFBWindows *thiz, RegisteredWatcher *registered,
                                                         VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser    parser;
     VoodooInstanceID       window_id;
     const DFBWindowConfig *config;
     DFBWindowConfigFlags   flags;

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, window_id );
     VOODOO_PARSER_GET_DATA( parser, config );
     VOODOO_PARSER_GET_INT( parser, flags );
     VOODOO_PARSER_END( parser );

     D_ASSERT( registered->watcher.WindowConfig != NULL );

     registered->watcher.WindowConfig( registered->context, window_id, config, flags );

     return DR_OK;
}

static DirectResult
IDirectFBWindows_Requestor_DispatchWatcher_WindowState( IDirectFBWindows *thiz, RegisteredWatcher *registered,
                                                        VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser   parser;
     VoodooInstanceID      window_id;
     const DFBWindowState *state;

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, window_id );
     VOODOO_PARSER_GET_DATA( parser, state );
     VOODOO_PARSER_END( parser );

     D_ASSERT( registered->watcher.WindowState != NULL );

     registered->watcher.WindowState( registered->context, window_id, state );

     return DR_OK;
}

static DirectResult
IDirectFBWindows_Requestor_DispatchWatcher_WindowRestack( IDirectFBWindows *thiz, RegisteredWatcher *registered,
                                                          VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser parser;
     VoodooInstanceID    window_id;
     unsigned int        index;

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, window_id );
     VOODOO_PARSER_GET_UINT( parser, index );
     VOODOO_PARSER_END( parser );

     D_ASSERT( registered->watcher.WindowRestack != NULL );

     registered->watcher.WindowRestack( registered->context, window_id, index );

     return DR_OK;
}

static DirectResult
IDirectFBWindows_Requestor_DispatchWatcher_WindowFocus( IDirectFBWindows *thiz, RegisteredWatcher *registered,
                                                        VoodooManager *manager, VoodooRequestMessage *msg )
{
     VoodooMessageParser parser;
     VoodooInstanceID    window_id;

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, window_id );
     VOODOO_PARSER_END( parser );

     D_ASSERT( registered->watcher.WindowFocus != NULL );

     registered->watcher.WindowFocus( registered->context, window_id );

     return DR_OK;
}

static DirectResult
IDirectFBWindows_Requestor_DispatchWatcher( void                 *dispatcher,
                                            void                 *real,
                                            VoodooManager        *manager,
                                            VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBWindows_Requestor/DispatchWatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowAdd:
               return IDirectFBWindows_Requestor_DispatchWatcher_WindowAdd( dispatcher, real, manager, msg );

          case IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowRemove:
               return IDirectFBWindows_Requestor_DispatchWatcher_WindowRemove( dispatcher, real, manager, msg );

          case IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowConfig:
               return IDirectFBWindows_Requestor_DispatchWatcher_WindowConfig( dispatcher, real, manager, msg );

          case IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowState:
               return IDirectFBWindows_Requestor_DispatchWatcher_WindowState( dispatcher, real, manager, msg );

          case IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowRestack:
               return IDirectFBWindows_Requestor_DispatchWatcher_WindowRestack( dispatcher, real, manager, msg );

          case IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowFocus:
               return IDirectFBWindows_Requestor_DispatchWatcher_WindowFocus( dispatcher, real, manager, msg );
     }

     return DFB_NOSUCHMETHOD;
}

/**********************************************************************************************************************/

static DFBResult
IDirectFBWindows_Requestor_RegisterWatcher( IDirectFBWindows        *thiz,
                                            const DFBWindowsWatcher *watcher,
                                            void                    *context )
{
     DFBResult              ret;
     RegisteredWatcher     *registered;
     VoodooResponseMessage *response;
     unsigned int           mask = 0;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows_Requestor)

     if (!watcher)
          return DFB_INVARG;

     if (watcher->WindowAdd)
          mask |= 1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowAdd;

     if (watcher->WindowRemove)
          mask |= 1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowRemove;

     if (watcher->WindowConfig)
          mask |= 1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowConfig;

     if (watcher->WindowState)
          mask |= 1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowState;

     if (watcher->WindowRestack)
          mask |= 1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowRestack;

     if (watcher->WindowFocus)
          mask |= 1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowFocus;

     if (!mask)
          return DFB_INVARG;


     registered = D_CALLOC( 1, sizeof(RegisteredWatcher) );
     if (!registered)
          return D_OOM();

     registered->watcher = *watcher;
     registered->context = context;

     ret = voodoo_manager_register_local( data->manager, false, thiz, registered,
                                          IDirectFBWindows_Requestor_DispatchWatcher, &registered->instance );
     if (ret) {
          D_DERROR( ret, "IDirectFBWindows_Requestor_RegisterWatcher: Could not register local instance!\n" );
          goto error;
     }

     ret = voodoo_manager_request( data->manager, data->instance,
                                   IDIRECTFBWINDOWS_METHOD_ID_RegisterWatcher, VREQ_RESPOND, &response,
                                   VMBT_ID, registered->instance,
                                   VMBT_UINT, mask,
                                   VMBT_NONE );
     if (ret)
          goto error;

     ret = response->result;

     voodoo_manager_finish_request( data->manager, response );

     if (ret)
          goto error;


     direct_list_append( &data->watchers, &registered->link );

     return DFB_OK;


error:
     if (registered->instance)
          voodoo_manager_unregister_local( data->manager, registered->instance );

     D_FREE( registered );

     return ret;
}

/**********************************************************************************************************************/

static DFBResult
Probe()
{
     /* This implementation has to be loaded explicitly. */
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBWindows *thiz,
           VoodooManager    *manager,
           VoodooInstanceID  instance,
           void             *arg )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBWindows_Requestor)

     data->ref       = 1;
     data->manager   = manager;
     data->instance  = instance;
     data->idirectfb = arg;

     thiz->AddRef          = IDirectFBWindows_Requestor_AddRef;
     thiz->Release         = IDirectFBWindows_Requestor_Release;
     thiz->RegisterWatcher = IDirectFBWindows_Requestor_RegisterWatcher;

     return DFB_OK;
}

