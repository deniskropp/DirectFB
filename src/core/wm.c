/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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
#include <string.h>

#include <directfb.h>

#include <direct/list.h>

#include <fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/core_parts.h>
#include <core/windows_internal.h>
#include <core/wm.h>

#include <misc/conf.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/modules.h>

DEFINE_MODULE_DIRECTORY( dfb_core_wm_modules, "wm", DFB_CORE_WM_ABI_VERSION );

typedef struct {
     DirectModuleEntry *module;
     const CoreWMFuncs *funcs;
     void              *data;
} CoreWMLocal;

typedef struct {
     char       *name;
     CoreWMInfo  info;
} CoreWMShared;

DFB_CORE_PART( wm, sizeof(CoreWMLocal), sizeof(CoreWMShared) )

static CoreWMLocal  *wm_local  = NULL;
static CoreWMShared *wm_shared = NULL;

/**************************************************************************************************/

static DFBResult
load_module( const void *name )
{
     DirectLink *l;

     D_ASSERT( wm_local != NULL );

     direct_modules_explore_directory( &dfb_core_wm_modules );

     direct_list_foreach( l, dfb_core_wm_modules.entries ) {
          DirectModuleEntry *module = (DirectModuleEntry*) l;
          const CoreWMFuncs *funcs;

          funcs = direct_module_ref( module );
          if (!funcs)
               continue;

          if (!name || !strcasecmp( name, module->name )) {
               if (wm_local->module)
                    direct_module_unref( wm_local->module );

               wm_local->module = module;
               wm_local->funcs  = funcs;
          }
          else
               direct_module_unref( module );
     }

     if (!wm_local->module) {
          if (name)
               D_ERROR( "DirectFB/WM: Window manager module '%s' not found!\n", name );
          else
               D_ERROR( "DirectFB/WM: No window manager module found!\n" );

          return DFB_NOIMPL;
     }

     return DFB_OK;
}

/**************************************************************************************************/

static DFBResult
dfb_wm_initialize( CoreDFB *core, void *data_local, void *data_shared )
{
     DFBResult ret;

     D_ASSERT( wm_local == NULL );
     D_ASSERT( wm_shared == NULL );

     wm_local  = data_local;
     wm_shared = data_shared;

     /* Load the module. */
     ret = load_module( dfb_config->wm );
     if (ret) {
          wm_local = NULL;
          wm_shared = NULL;
          return ret;
     }

     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Initialize != NULL );

     /* Query module information. */
     wm_local->funcs->GetWMInfo( &wm_shared->info );

     D_ASSERT( wm_shared->info.wm_data_size > 0 );
     D_ASSERT( wm_shared->info.stack_data_size > 0 );
     D_ASSERT( wm_shared->info.window_data_size > 0 );

     D_INFO( "DirectFB/WM: %s %d.%d (%s)\n",
             wm_shared->info.name, wm_shared->info.version.major,
             wm_shared->info.version.minor, wm_shared->info.vendor );

     /* Store module name in shared memory. */
     wm_shared->name = SHSTRDUP( wm_local->module->name );
     if (!wm_shared->name) {
          D_WARN( "out of (shared) memory" );
          wm_local = NULL;
          wm_shared = NULL;
          return ret;
     }

     /* Allocate window manager data. */
     wm_local->data = D_CALLOC( 1, wm_shared->info.wm_data_size );
     if (!wm_local->data) {
          D_WARN( "out of memory" );
          SHFREE( wm_shared->name );
          wm_local = NULL;
          wm_shared = NULL;
          return ret;
     }

     /* Initialize window manager. */
     ret = wm_local->funcs->Initialize( core, wm_local->data );
     if (ret) {
          SHFREE( wm_shared->name );
          D_FREE( wm_local->data );
          wm_local = NULL;
          wm_shared = NULL;
          return ret;
     }

     return DFB_OK;
}

static DFBResult
dfb_wm_join( CoreDFB *core, void *data_local, void *data_shared )
{
     DFBResult ret;

     D_ASSERT( wm_local == NULL );
     D_ASSERT( wm_shared == NULL );

     wm_local  = data_local;
     wm_shared = data_shared;

     /* Load the module that is used by the running session. */
     ret = load_module( wm_shared->name );
     if (ret) {
          wm_local = NULL;
          wm_shared = NULL;
          return ret;
     }

     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Join != NULL );

     /* Allocate window manager data. */
     wm_local->data = D_CALLOC( 1, wm_shared->info.wm_data_size );
     if (!wm_local->data) {
          D_WARN( "out of memory" );
          wm_local = NULL;
          wm_shared = NULL;
          return ret;
     }

     /* Join window manager. */
     ret = wm_local->funcs->Join( core, wm_local->data );
     if (ret) {
          D_FREE( wm_local->data );
          wm_local = NULL;
          wm_shared = NULL;
          return ret;
     }

     return DFB_OK;
}

static DFBResult
dfb_wm_shutdown( CoreDFB *core, bool emergency )
{
     DFBResult ret;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Shutdown != NULL );
     D_ASSERT( wm_shared != NULL );

     /* Shutdown window manager. */
     ret = wm_local->funcs->Shutdown( emergency, wm_local->data );

     /* Unload the module. */
     direct_module_unref( wm_local->module );

     /* Deallocate window manager data. */
     D_FREE( wm_local->data );

     /* Free module name in shared memory. */
     SHFREE( wm_shared->name );

     wm_local = NULL;
     wm_shared = NULL;

     return ret;
}

static DFBResult
dfb_wm_leave( CoreDFB *core, bool emergency )
{
     DFBResult ret;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Leave != NULL );
     D_ASSERT( wm_shared != NULL );

     /* Leave window manager. */
     ret = wm_local->funcs->Leave( emergency, wm_local->data );

     /* Unload the module. */
     direct_module_unref( wm_local->module );

     /* Deallocate window manager data. */
     D_FREE( wm_local->data );

     wm_local = NULL;
     wm_shared = NULL;

     return ret;
}

static DFBResult
dfb_wm_suspend( CoreDFB *core )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Suspend != NULL );

     return wm_local->funcs->Suspend( wm_local->data );
}

static DFBResult
dfb_wm_resume( CoreDFB *core )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Resume != NULL );

     return wm_local->funcs->Resume( wm_local->data );
}

/**************************************************************************************************/

void
dfb_wm_get_info( CoreWMInfo *info )
{
     D_ASSERT( wm_shared != NULL );

     D_ASSERT( info != NULL );

     *info = wm_shared->info;
}

/**************************************************************************************************/

DFBResult
dfb_wm_init_stack( CoreWindowStack *stack )
{
     DFBResult  ret;
     void      *stack_data;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->InitStack != NULL );
     D_ASSERT( wm_shared != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data == NULL );

     /* Allocate shared stack data. */
     stack_data = SHCALLOC( 1, wm_shared->info.stack_data_size );
     if (!stack_data) {
          D_WARN( "out of (shared) memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     /* Window manager specific initialization. */
     ret = wm_local->funcs->InitStack( stack, wm_local->data, stack_data );
     if (ret) {
          SHFREE( stack_data );
          return ret;
     }

     /* Keep shared stack data. */
     stack->stack_data = stack_data;

     return DFB_OK;
}

DFBResult
dfb_wm_close_stack( CoreWindowStack *stack )
{
     DFBResult ret;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->CloseStack != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data != NULL );

     /* Window manager specific deinitialization. */
     ret = wm_local->funcs->CloseStack( stack, wm_local->data, stack->stack_data );

     /* Deallocate shared stack data. */
     SHFREE( stack->stack_data );
     stack->stack_data = NULL;

     return ret;
}

DFBResult
dfb_wm_process_input( CoreWindowStack     *stack,
                      const DFBInputEvent *event )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->ProcessInput != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data != NULL );

     D_ASSERT( event != NULL );

     /* Dispatch input event via window manager. */
     return wm_local->funcs->ProcessInput( stack, wm_local->data, stack->stack_data, event );
}

DFBResult
dfb_wm_flush_keys( CoreWindowStack *stack )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->FlushKeys != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data != NULL );

     return wm_local->funcs->FlushKeys( stack, wm_local->data, stack->stack_data );
}

DFBResult
dfb_wm_window_at( CoreWindowStack  *stack,
                  int               x,
                  int               y,
                  CoreWindow      **ret_window )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->WindowAt != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data != NULL );

     D_ASSERT( ret_window != NULL );

     return wm_local->funcs->WindowAt( stack, wm_local->data, stack->stack_data, x, y, ret_window );
}

DFBResult
dfb_wm_window_lookup( CoreWindowStack  *stack,
                      DFBWindowID       window_id,
                      CoreWindow      **ret_window )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->WindowLookup != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data != NULL );

     D_ASSERT( ret_window != NULL );

     return wm_local->funcs->WindowLookup( stack, wm_local->data,
                                           stack->stack_data, window_id, ret_window );
}

DFBResult
dfb_wm_enum_windows( CoreWindowStack      *stack,
                     CoreWMWindowCallback  callback,
                     void                 *callback_ctx )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->EnumWindows != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data != NULL );

     D_ASSERT( callback != NULL );

     return wm_local->funcs->EnumWindows( stack, wm_local->data,
                                          stack->stack_data, callback, callback_ctx );
}

DFBResult
dfb_wm_warp_cursor( CoreWindowStack  *stack,
                    int               x,
                    int               y )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->WarpCursor != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data != NULL );

     return wm_local->funcs->WarpCursor( stack, wm_local->data, stack->stack_data, x, y );
}

DFBResult
dfb_wm_add_window( CoreWindowStack *stack,
                   CoreWindow      *window )
{
     DFBResult  ret;
     void      *window_data;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->AddWindow != NULL );
     D_ASSERT( wm_shared != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data != NULL );

     D_ASSERT( window != NULL );
     D_ASSERT( window->window_data == NULL );

     /* Allocate shared window data. */
     window_data = SHCALLOC( 1, wm_shared->info.window_data_size );
     if (!window_data) {
          D_WARN( "out of (shared) memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     /* Tell window manager about the new window. */
     ret = wm_local->funcs->AddWindow( stack, wm_local->data,
                                       stack->stack_data, window, window_data );
     if (ret) {
          SHFREE( window_data );
          return ret;
     }

     /* Keep shared window data. */
     window->window_data = window_data;

     return DFB_OK;
}

DFBResult
dfb_wm_remove_window( CoreWindowStack *stack,
                      CoreWindow      *window )
{
     DFBResult ret;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->RemoveWindow != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data != NULL );

     D_ASSERT( window != NULL );
     D_ASSERT( window->window_data != NULL );

     /* Remove window from window manager. */
     ret = wm_local->funcs->RemoveWindow( stack, wm_local->data,
                                          stack->stack_data, window, window->window_data );

     /* Deallocate shared stack data. */
     SHFREE( window->window_data );
     window->window_data = NULL;

     return ret;
}

DFBResult
dfb_wm_move_window( CoreWindow *window,
                    int         dx,
                    int         dy )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->MoveWindow != NULL );

     D_ASSERT( window != NULL );
     D_ASSERT( window->window_data != NULL );

     return wm_local->funcs->MoveWindow( window, wm_local->data,
                                         window->window_data, dx, dy );
}

DFBResult
dfb_wm_resize_window( CoreWindow *window,
                      int         width,
                      int         height )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->ResizeWindow != NULL );

     D_ASSERT( window != NULL );
     D_ASSERT( window->window_data != NULL );

     return wm_local->funcs->ResizeWindow( window, wm_local->data,
                                           window->window_data, width, height );
}

DFBResult
dfb_wm_restack_window( CoreWindow             *window,
                       CoreWindow             *relative,
                       int                     relation,
                       DFBWindowStackingClass  stacking )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->RestackWindow != NULL );

     D_ASSERT( window != NULL );
     D_ASSERT( window->window_data != NULL );

     D_ASSERT( relative == NULL || relative->window_data != NULL );

     D_ASSERT( relative == NULL || relative == window || relation != 0);

     return wm_local->funcs->RestackWindow( window, wm_local->data, window->window_data,
                                            relative, relative ? relative->window_data : NULL,
                                            relation, stacking );
}

DFBResult
dfb_wm_set_opacity( CoreWindow *window,
                    __u8        opacity )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->SetOpacity != NULL );

     D_ASSERT( window != NULL );
     D_ASSERT( window->window_data != NULL );

     return wm_local->funcs->SetOpacity( window, wm_local->data, window->window_data, opacity );
}

DFBResult
dfb_wm_grab( CoreWindow *window,
             CoreWMGrab *grab )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Grab != NULL );

     D_ASSERT( window != NULL );
     D_ASSERT( window->window_data != NULL );

     D_ASSERT( grab != NULL );

     return wm_local->funcs->Grab( window, wm_local->data, window->window_data, grab );
}

DFBResult
dfb_wm_ungrab( CoreWindow *window,
               CoreWMGrab *grab )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Ungrab != NULL );

     D_ASSERT( window != NULL );
     D_ASSERT( window->window_data != NULL );

     D_ASSERT( grab != NULL );

     return wm_local->funcs->Ungrab( window, wm_local->data, window->window_data, grab );
}

DFBResult
dfb_wm_request_focus( CoreWindow *window )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->RequestFocus != NULL );

     D_ASSERT( window != NULL );
     D_ASSERT( window->window_data != NULL );

     return wm_local->funcs->RequestFocus( window, wm_local->data, window->window_data );
}

DFBResult
dfb_wm_update_stack( CoreWindowStack     *stack,
                     DFBRegion           *region,
                     DFBSurfaceFlipFlags  flags )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->UpdateStack != NULL );

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->stack_data != NULL );

     D_ASSERT( region != NULL );

     return wm_local->funcs->UpdateStack( stack, wm_local->data,
                                          stack->stack_data, region, flags );
}

DFBResult
dfb_wm_update_window( CoreWindow          *window,
                      DFBRegion           *region,
                      DFBSurfaceFlipFlags  flags,
                      bool                 force_complete,
                      bool                 force_invisible )
{
     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->data != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->UpdateWindow != NULL );

     D_ASSERT( window != NULL );
     D_ASSERT( window->window_data != NULL );

     D_ASSERT( region != NULL );

     return wm_local->funcs->UpdateWindow( window, wm_local->data, window->window_data,
                                           region, flags, force_complete, force_invisible );
}

