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

#ifndef __DFB__CORE__WM_MODULE_H__
#define __DFB__CORE__WM_MODULE_H__

#include <core/wm.h>

static void      wm_get_info       ( CoreWMInfo             *info );


static DFBResult wm_initialize     ( CoreDFB                *core,
                                     void                   *wm_data );

static DFBResult wm_join           ( CoreDFB                *core,
                                     void                   *wm_data );

static DFBResult wm_shutdown       ( bool                    emergency,
                                     void                   *wm_data );

static DFBResult wm_leave          ( bool                    emergency,
                                     void                   *wm_data );

static DFBResult wm_suspend        ( void                   *wm_data );

static DFBResult wm_resume         ( void                   *wm_data );


static DFBResult wm_init_stack     ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data );

static DFBResult wm_close_stack    ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data );

static DFBResult wm_process_input  ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     const DFBInputEvent    *event );

static DFBResult wm_window_at      ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     int                     x,
                                     int                     y,
                                     CoreWindow            **ret_window );

static DFBResult wm_warp_cursor    ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     int                     x,
                                     int                     y );

static DFBResult wm_update_focus   ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data );


static CoreWMFuncs wm_funcs = {
     GetWMInfo:           wm_get_info,

     Initialize:          wm_initialize,
     Join:                wm_join,
     Shutdown:            wm_shutdown,
     Leave:               wm_leave,
     Suspend:             wm_suspend,
     Resume:              wm_resume,

     InitStack:           wm_init_stack,
     CloseStack:          wm_close_stack,
     ProcessInput:        wm_process_input,
     WindowAt:            wm_window_at,
     WarpCursor:          wm_warp_cursor,
     UpdateFocus:         wm_update_focus
};


#define DFB_WINDOW_MANAGER(shortname)                       \
__attribute__((constructor)) void directfbwm_##shortname(); \
                                                            \
void                                                        \
directfbwm_##shortname()                                    \
{                                                           \
     direct_modules_register( &dfb_core_wm_modules,         \
                              DFB_CORE_WM_ABI_VERSION,      \
                              #shortname, &wm_funcs );      \
}

#endif

