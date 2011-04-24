/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdarg.h>

#include <directfb.h>
#include <directfb_windows.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/memcpy.h>

#include <fusion/reactor.h>

#include <core/wm.h>

#include "idirectfbwindows_default.h"


D_DEBUG_DOMAIN( IDirectFBWindows_default, "IDirectFBWindows/default", "IDirectFBWindows Interface default Implementation" );

/**********************************************************************************************************************/

static DirectResult
Probe( void *ctx, ... );

static DirectResult
Construct( void *interface, ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBWindows, default )

/**********************************************************************************************************************/

typedef struct {
     DirectLink           link;

     DFBWindowsWatcher    watcher;
     void                *context;

     Reaction             reactions[_CORE_WM_NUM_CHANNELS];
} RegisteredWatcher;

/**********************************************************************************************************************/

static void
IDirectFBWindows_Destruct( IDirectFBWindows *thiz )
{
     IDirectFBWindows_data *data;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     D_ASSERT( thiz != NULL );

     data = thiz->priv;
}

static DirectResult
IDirectFBWindows_AddRef( IDirectFBWindows *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows)

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBWindows_Release( IDirectFBWindows *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows)

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBWindows_Destruct( thiz );

     return DFB_OK;
}

/**********************************************************************************************************************/

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowAdd( const void *msg_data,
                                        void       *ctx )
{
     const CoreWM_WindowAdd *add        = msg_data;
     RegisteredWatcher      *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( msg %p, ctx %p )\n", __FUNCTION__, msg_data, ctx );

     D_ASSERT( registered->watcher.WindowAdd != NULL );

     registered->watcher.WindowAdd( registered->context, &add->info );

     return RS_OK;
}

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowRemove( const void *msg_data,
                                           void       *ctx )
{
     const CoreWM_WindowRemove *remove     = msg_data;
     RegisteredWatcher         *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( msg %p, ctx %p )\n", __FUNCTION__, msg_data, ctx );

     D_ASSERT( registered->watcher.WindowRemove != NULL );

     registered->watcher.WindowRemove( registered->context, remove->window_id );

     return RS_OK;
}

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowConfig( const void *msg_data,
                                           void       *ctx )
{
     const CoreWM_WindowConfig *config     = msg_data;
     RegisteredWatcher         *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( msg %p, ctx %p )\n", __FUNCTION__, msg_data, ctx );

     D_ASSERT( registered->watcher.WindowConfig != NULL );

     registered->watcher.WindowConfig( registered->context, config->window_id, &config->config, config->flags );

     return RS_OK;
}

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowState( const void *msg_data,
                                          void       *ctx )
{
     const CoreWM_WindowState *state      = msg_data;
     RegisteredWatcher        *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( msg %p, ctx %p )\n", __FUNCTION__, msg_data, ctx );

     D_ASSERT( registered->watcher.WindowState != NULL );

     registered->watcher.WindowState( registered->context, state->window_id, &state->state );

     return RS_OK;
}

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowRestack( const void *msg_data,
                                            void       *ctx )
{
     const CoreWM_WindowRestack *restack    = msg_data;
     RegisteredWatcher          *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( msg %p, ctx %p )\n", __FUNCTION__, msg_data, ctx );

     D_ASSERT( registered->watcher.WindowRestack != NULL );

     registered->watcher.WindowRestack( registered->context, restack->window_id, restack->index );

     return RS_OK;
}

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowFocus( const void *msg_data,
                                          void       *ctx )
{
     const CoreWM_WindowFocus *focus      = msg_data;
     RegisteredWatcher        *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( msg %p, ctx %p )\n", __FUNCTION__, msg_data, ctx );

     D_ASSERT( registered->watcher.WindowFocus != NULL );

     registered->watcher.WindowFocus( registered->context, focus->window_id );

     return RS_OK;
}

#define WM_ATTACH( Func, CHANNEL )                                                                  \
     do {                                                                                           \
          if (watcher->Func) {                                                                      \
               ret = dfb_wm_attach( data->core, CORE_WM_##CHANNEL,                                  \
                                    IDirectFBWindows_WM_Reaction_##Func, registered,                \
                                    &registered->reactions[CORE_WM_##CHANNEL] );                    \
               if (ret) {                                                                           \
                    D_DERROR( ret, "IDirectFBWindows_RegisterWatcher: "                             \
                                   "dfb_wm_attach( 'CORE_WM_" #CHANNEL "' ) failed!\n" );           \
                    goto error;                                                                     \
               }                                                                                    \
          }                                                                                         \
     } while (0)

/**********************************************************************************************************************/

static DirectResult
IDirectFBWindows_RegisterWatcher( IDirectFBWindows        *thiz,
                                  const DFBWindowsWatcher *watcher,
                                  void                    *context )
{
     DFBResult          ret;
     int                i;
     RegisteredWatcher *registered;

     DIRECT_INTERFACE_GET_DATA(IDirectFBWindows)

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     if (!watcher)
          return DFB_INVARG;

     if (!watcher->WindowAdd     &&
         !watcher->WindowRemove  &&
         !watcher->WindowConfig  &&
         !watcher->WindowState   &&
         !watcher->WindowRestack &&
         !watcher->WindowFocus)
          return DFB_INVARG;

     registered = D_CALLOC( 1, sizeof(RegisteredWatcher) );
     if (!registered)
          return D_OOM();

     registered->watcher = *watcher;
     registered->context = context;

     WM_ATTACH( WindowAdd,     WINDOW_ADD );
     WM_ATTACH( WindowRemove,  WINDOW_REMOVE );
     WM_ATTACH( WindowConfig,  WINDOW_CONFIG );
     WM_ATTACH( WindowState,   WINDOW_STATE );
     WM_ATTACH( WindowRestack, WINDOW_RESTACK );
     WM_ATTACH( WindowFocus,   WINDOW_FOCUS );

     direct_list_append( &data->watchers, &registered->link );

     return DFB_OK;


error:
     for (i=_CORE_WM_NUM_CHANNELS-1; i>=0; i--) {
          if (registered->reactions[i].func)
               dfb_wm_detach( data->core, &registered->reactions[i] );
     }

     D_FREE( registered );

     return ret;
}

/**********************************************************************************************************************/

static DirectResult
Probe( void *ctx, ... )
{
     D_DEBUG_AT( IDirectFBWindows_default, "%s()\n", __FUNCTION__ );

     (void) ctx;

     /* ... */

     return DFB_OK;
}

static DirectResult
Construct( void *interface, ... )
{
     DFBResult         ret = DFB_INVARG;
     IDirectFB        *dfb;
     IDirectFBWindows *thiz = interface;
     CoreDFB          *core;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBWindows)

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     va_list tag;
     va_start(tag, interface);
     dfb = va_arg(tag, IDirectFB *);
     core = va_arg(tag, CoreDFB *);
     va_end( tag );

     /* Check arguments. */
     if (!thiz)
          goto error;

     /* Initialize interface data. */
     data->ref       = 1;
     data->core      = core;
     data->idirectfb = dfb;


     /* Initialize function pointer table. */
     thiz->AddRef             = IDirectFBWindows_AddRef;
     thiz->Release            = IDirectFBWindows_Release;

     thiz->RegisterWatcher    = IDirectFBWindows_RegisterWatcher;


     return DFB_OK;


error:
     DIRECT_DEALLOCATE_INTERFACE(thiz);

     return ret;
}

