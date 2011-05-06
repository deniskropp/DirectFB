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

#include "idirectfbwindows_requestor.h"

#include "idirectfbwindows_dispatcher.h"


static DFBResult Probe(void);
static DFBResult Construct( IDirectFBWindows *thiz,
                            IDirectFBWindows *real,
                            VoodooManager    *manager,
                            VoodooInstanceID  super,
                            void             *arg,
                            VoodooInstanceID *ret_instance );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBWindows, Dispatcher )


/**************************************************************************************************/

static void
IDirectFBWindows_Dispatcher_Destruct( IDirectFBWindows *thiz )
{
     D_DEBUG( "%s (%p)\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

/**************************************************************************************************/

static DirectResult
IDirectFBWindows_Dispatcher_AddRef( IDirectFBWindows *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows_Dispatcher)

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBWindows_Dispatcher_Release( IDirectFBWindows *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows_Dispatcher)

     if (--data->ref == 0)
          IDirectFBWindows_Dispatcher_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBWindows_Dispatcher_RegisterWatcher( IDirectFBWindows        *thiz,
                                             const DFBWindowsWatcher *watcher,
                                             void                    *context )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows_Dispatcher)

     D_UNIMPLEMENTED();

     return DFB_UNIMPLEMENTED;
}

/**************************************************************************************************/

static DirectResult
Dispatch_Release( IDirectFBWindows *thiz, IDirectFBWindows *real,
                  VoodooManager *manager, VoodooRequestMessage *msg )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows_Dispatcher)

     return thiz->Release( thiz );
}

/**********************************************************************************************************************/

typedef struct {
     VoodooManager       *manager;

     VoodooInstanceID     instance;
} WatcherContext;

static void
IDirectFBWindows_Dispatcher_WindowAdd( void                *context,
                                       const DFBWindowInfo *info )
{
     WatcherContext *ctx = context;

     voodoo_manager_request( ctx->manager, ctx->instance,
                             IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowAdd, VREQ_NONE, NULL,
                             VMBT_DATA, sizeof(DFBWindowInfo), info,
                             VMBT_NONE );
}

static void
IDirectFBWindows_Dispatcher_WindowRemove( void        *context,
                                          DFBWindowID  window_id )
{
     WatcherContext *ctx = context;

     voodoo_manager_request( ctx->manager, ctx->instance,
                             IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowRemove, VREQ_NONE, NULL,
                             VMBT_ID, window_id,
                             VMBT_NONE );
}

static void
IDirectFBWindows_Dispatcher_WindowConfig( void                  *context,
                                          DFBWindowID            window_id,
                                          const DFBWindowConfig *config,
                                          DFBWindowConfigFlags   flags )
{
     WatcherContext *ctx = context;

     voodoo_manager_request( ctx->manager, ctx->instance,
                             IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowConfig, VREQ_NONE, NULL,
                             VMBT_ID, window_id,
                             VMBT_DATA, sizeof(DFBWindowConfig), config,
                             VMBT_INT, flags,
                             VMBT_NONE );
}

static void
IDirectFBWindows_Dispatcher_WindowState( void                 *context,
                                         DFBWindowID           window_id,
                                         const DFBWindowState *state )
{
     WatcherContext *ctx = context;

     voodoo_manager_request( ctx->manager, ctx->instance,
                             IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowState, VREQ_NONE, NULL,
                             VMBT_ID, window_id,
                             VMBT_DATA, sizeof(DFBWindowState), state,
                             VMBT_NONE );
}

static void
IDirectFBWindows_Dispatcher_WindowRestack( void         *context,
                                           DFBWindowID   window_id,
                                           unsigned int  index )
{
     WatcherContext *ctx = context;

     voodoo_manager_request( ctx->manager, ctx->instance,
                             IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowRestack, VREQ_NONE, NULL,
                             VMBT_ID, window_id,
                             VMBT_UINT, index,
                             VMBT_NONE );
}

static void
IDirectFBWindows_Dispatcher_WindowFocus( void        *context,
                                         DFBWindowID  window_id )
{
     WatcherContext *ctx = context;

     voodoo_manager_request( ctx->manager, ctx->instance,
                             IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowFocus, VREQ_NONE, NULL,
                             VMBT_ID, window_id,
                             VMBT_NONE );
}

/**********************************************************************************************************************/

static DirectResult
Dispatch_RegisterWatcher( IDirectFBWindows *thiz, IDirectFBWindows *real,
                          VoodooManager *manager, VoodooRequestMessage *msg )
{
     DFBResult            ret;
     VoodooMessageParser  parser;
     VoodooInstanceID     instance;
     unsigned int         mask;
     DFBWindowsWatcher    watcher;
     WatcherContext      *context;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows_Dispatcher)

     VOODOO_PARSER_BEGIN( parser, msg );
     VOODOO_PARSER_GET_ID( parser, instance );
     VOODOO_PARSER_GET_UINT( parser, mask );
     VOODOO_PARSER_END( parser );

     memset( &watcher, 0, sizeof(watcher) );

     if (mask & (1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowAdd))
          watcher.WindowAdd = IDirectFBWindows_Dispatcher_WindowAdd;

     if (mask & (1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowRemove))
          watcher.WindowRemove = IDirectFBWindows_Dispatcher_WindowRemove;

     if (mask & (1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowConfig))
          watcher.WindowConfig = IDirectFBWindows_Dispatcher_WindowConfig;

     if (mask & (1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowState))
          watcher.WindowState = IDirectFBWindows_Dispatcher_WindowState;

     if (mask & (1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowRestack))
          watcher.WindowRestack = IDirectFBWindows_Dispatcher_WindowRestack;

     if (mask & (1 << IDIRECTFBWINDOWS_REQUESTOR_METHOD_ID_DispatchWatcher_WindowFocus))
          watcher.WindowFocus = IDirectFBWindows_Dispatcher_WindowFocus;

     context = D_CALLOC( 1, sizeof(WatcherContext) );
     if (!context)
          return D_OOM();

     context->manager  = manager;
     context->instance = instance;

     ret = real->RegisterWatcher( real, &watcher, context );
     if (ret)
          return ret;

     return voodoo_manager_respond( manager, true, msg->header.serial, DR_OK, VOODOO_INSTANCE_NONE, VMBT_NONE );
}

static DirectResult
Dispatch( void *dispatcher, void *real, VoodooManager *manager, VoodooRequestMessage *msg )
{
     D_DEBUG( "IDirectFBWindows/Dispatcher: "
              "Handling request for instance %u with method %u...\n", msg->instance, msg->method );

     switch (msg->method) {
          case IDIRECTFBWINDOWS_METHOD_ID_Release:
               return Dispatch_Release( dispatcher, real, manager, msg );

          case IDIRECTFBWINDOWS_METHOD_ID_RegisterWatcher:
               return Dispatch_RegisterWatcher( dispatcher, real, manager, msg );
     }

     return DFB_NOSUCHMETHOD;
}

/**************************************************************************************************/

static DFBResult
Probe(void)
{
     /* This implementation has to be loaded explicitly. */
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBWindows *thiz,     /* Dispatcher interface */
           IDirectFBWindows *real,     /* Real interface implementation */
           VoodooManager    *manager,  /* Manager of the Voodoo framework */
           VoodooInstanceID  super,    /* Instance ID of the super interface */
           void             *arg,      /* Optional arguments to constructor */
           VoodooInstanceID *ret_instance )
{
     DFBResult        ret;
     VoodooInstanceID instance;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBWindows_Dispatcher)

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
     thiz->AddRef          = IDirectFBWindows_Dispatcher_AddRef;
     thiz->Release         = IDirectFBWindows_Dispatcher_Release;
     thiz->RegisterWatcher = IDirectFBWindows_Dispatcher_RegisterWatcher;

     return DFB_OK;
}

