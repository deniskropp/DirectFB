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

#ifndef __DFB__CORE__WM_MODULE_H__
#define __DFB__CORE__WM_MODULE_H__

#include <core/wm.h>


/** Module **/

static void      wm_get_info       ( CoreWMInfo             *info );

static DFBResult wm_initialize     ( CoreDFB                *core,
                                     void                   *wm_data,
                                     void                   *shared_data );

static DFBResult wm_join           ( CoreDFB                *core,
                                     void                   *wm_data,
                                     void                   *shared_data );

static DFBResult wm_shutdown       ( bool                    emergency,
                                     void                   *wm_data,
                                     void                   *shared_data );

static DFBResult wm_leave          ( bool                    emergency,
                                     void                   *wm_data,
                                     void                   *shared_data );

static DFBResult wm_suspend        ( void                   *wm_data,
                                     void                   *shared_data );

static DFBResult wm_resume         ( void                   *wm_data,
                                     void                   *shared_data );

static DFBResult wm_post_init      ( void                   *wm_data,
                                     void                   *shared_data );


/** Stack **/

static DFBResult wm_init_stack     ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data );

static DFBResult wm_close_stack    ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data );

static DFBResult wm_set_active     ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     bool                    active );

static DFBResult wm_resize_stack   ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     int                     width,
                                     int                     height );

static DFBResult wm_process_input  ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     const DFBInputEvent    *event );

static DFBResult wm_flush_keys     ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data );

static DFBResult wm_window_at      ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     int                     x,
                                     int                     y,
                                     CoreWindow            **ret_window );

static DFBResult wm_window_lookup  ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     DFBWindowID             window_id,
                                     CoreWindow            **ret_window );

static DFBResult wm_enum_windows   ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     CoreWMWindowCallback    callback,
                                     void                   *callback_ctx );

/** Window **/
static DFBResult wm_get_insets     ( CoreWindowStack        *stack,
                                     CoreWindow             *window,
                                     DFBInsets              *insets );

static DFBResult wm_preconfigure_window     ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     CoreWindow             *window,
                                     void                   *window_data );

static DFBResult wm_set_window_property( CoreWindowStack        *stack,
                                         void                   *wm_data,
                                         void                   *stack_data,
                                         CoreWindow             *window,
                                         void                   *window_data,
                                         const char             *key,
                                         void                   *value, 
                                         void                  **ret_old_value );

static DFBResult wm_get_window_property( CoreWindowStack        *stack,
                                         void                   *wm_data,
                                         void                   *stack_data,
                                         CoreWindow             *window,
                                         void                   *window_data,
                                         const char             *key,
                                         void                  **ret_value );

static DFBResult wm_remove_window_property( CoreWindowStack        *stack,
                                            void                   *wm_data,
                                            void                   *stack_data,
                                            CoreWindow             *window,
                                            void                   *window_data,
                                            const char             *key,
                                            void                  **ret_value );

static DFBResult wm_add_window     ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     CoreWindow             *window,
                                     void                   *window_data );

static DFBResult wm_remove_window  ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     CoreWindow             *window,
                                     void                   *window_data );

static DFBResult wm_set_window_config( CoreWindow             *window,
                                       void                   *wm_data,
                                       void                   *window_data,
                                       const CoreWindowConfig *config,
                                       CoreWindowConfigFlags   flags );

static DFBResult wm_restack_window ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data,
                                     CoreWindow             *relative,
                                     void                   *relative_data,
                                     int                     relation );

static DFBResult wm_grab           ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data,
                                     CoreWMGrab             *grab );

static DFBResult wm_ungrab         ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data,
                                     CoreWMGrab             *grab );

static DFBResult wm_request_focus  ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data );

static DFBResult wm_begin_updates  ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data,
                                     const DFBRegion        *update );

static DFBResult wm_set_cursor_position( CoreWindow             *window,
                                         void                   *wm_data,
                                         void                   *window_data,
                                         int                     x,
                                         int                     y );

/** Updates **/

static DFBResult wm_update_stack   ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     const DFBRegion        *region,
                                     DFBSurfaceFlipFlags     flags );

static DFBResult wm_update_window  ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data,
                                     const DFBRegion        *left_region,
                                     const DFBRegion        *right_region,
                                     DFBSurfaceFlipFlags     flags );

static DFBResult wm_update_cursor  ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     CoreCursorUpdateFlags   flags );


static CoreWMFuncs wm_funcs = {
     .GetWMInfo            = wm_get_info,

     .Initialize           = wm_initialize,
     .Join                 = wm_join,
     .Shutdown             = wm_shutdown,
     .Leave                = wm_leave,
     .Suspend              = wm_suspend,
     .Resume               = wm_resume,
     .PostInit             = wm_post_init,

     .InitStack            = wm_init_stack,
     .CloseStack           = wm_close_stack,
     .SetActive            = wm_set_active,
     .ResizeStack          = wm_resize_stack,
     .ProcessInput         = wm_process_input,
     .FlushKeys            = wm_flush_keys,
     .WindowAt             = wm_window_at,
     .WindowLookup         = wm_window_lookup,
     .EnumWindows          = wm_enum_windows,

     .GetInsets            = wm_get_insets,
     .PreConfigureWindow   = wm_preconfigure_window,
     .SetWindowProperty    = wm_set_window_property,
     .GetWindowProperty    = wm_get_window_property,
     .RemoveWindowProperty = wm_remove_window_property,
     .AddWindow            = wm_add_window,
     .RemoveWindow         = wm_remove_window,
     .SetWindowConfig      = wm_set_window_config,
     .RestackWindow        = wm_restack_window,
     .Grab                 = wm_grab,
     .Ungrab               = wm_ungrab,
     .RequestFocus         = wm_request_focus,
     .BeginUpdates         = wm_begin_updates,
     .SetCursorPosition    = wm_set_cursor_position,

     .UpdateStack          = wm_update_stack,
     .UpdateWindow         = wm_update_window,
     .UpdateCursor         = wm_update_cursor
};


#define DFB_WINDOW_MANAGER(shortname)                             \
__attribute__((constructor)) void directfbwm_##shortname( void ); \
                                                                  \
void                                                              \
directfbwm_##shortname( void )                                    \
{                                                                 \
     direct_modules_register( &dfb_core_wm_modules,               \
                              DFB_CORE_WM_ABI_VERSION,            \
                              #shortname, &wm_funcs );            \
}

#endif

