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

#ifndef __DFB__CORE__WM_H__
#define __DFB__CORE__WM_H__

#include <directfb.h>

#include <direct/modules.h>

#include <core/coretypes.h>
#include <core/windows.h>


DECLARE_MODULE_DIRECTORY( dfb_core_wm_modules );


/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFB_CORE_WM_ABI_VERSION           7

#define DFB_CORE_WM_INFO_NAME_LENGTH     60
#define DFB_CORE_WM_INFO_VENDOR_LENGTH   80
#define DFB_CORE_WM_INFO_URL_LENGTH     120
#define DFB_CORE_WM_INFO_LICENSE_LENGTH  40


typedef struct {
     int            major;
     int            minor;

     int            binary;
} CoreWMVersion;

typedef struct {
     CoreWMVersion  version;

     char           name   [DFB_CORE_WM_INFO_NAME_LENGTH];
     char           vendor [DFB_CORE_WM_INFO_VENDOR_LENGTH];
     char           url    [DFB_CORE_WM_INFO_URL_LENGTH];
     char           license[DFB_CORE_WM_INFO_LICENSE_LENGTH];

     unsigned int   wm_data_size;
     unsigned int   wm_shared_size;
     unsigned int   stack_data_size;
     unsigned int   window_data_size;
} CoreWMInfo;

typedef enum {
     CWMGT_KEYBOARD,
     CWMGT_POINTER,
     CWMGT_KEY
} CoreWMGrabTarget;

typedef struct {
     CoreWMGrabTarget            target;

     /* Both for CWMGT_KEY only. */
     DFBInputDeviceKeySymbol     symbol;
     DFBInputDeviceModifierMask  modifiers;
} CoreWMGrab;

typedef DFBEnumerationResult (*CoreWMWindowCallback) (CoreWindow *window,
                                                      void       *ctx);

typedef struct {

   /** Module **/

     void      (*GetWMInfo)        ( CoreWMInfo             *info );

     DFBResult (*Initialize)       ( CoreDFB                *core,
                                     void                   *wm_data,
                                     void                   *shared_data );

     DFBResult (*Join)             ( CoreDFB                *core,
                                     void                   *wm_data,
                                     void                   *shared_data );

     DFBResult (*Shutdown)         ( bool                    emergency,
                                     void                   *wm_data,
                                     void                   *shared_data );

     DFBResult (*Leave)            ( bool                    emergency,
                                     void                   *wm_data,
                                     void                   *shared_data );

     DFBResult (*Suspend)          ( void                   *wm_data,
                                     void                   *shared_data );

     DFBResult (*Resume)           ( void                   *wm_data,
                                     void                   *shared_data );


   /** Stack **/

     DFBResult (*InitStack)        ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data );

     DFBResult (*CloseStack)       ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data );

     DFBResult (*SetActive)        ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     bool                    active );

     DFBResult (*ResizeStack)      ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     int                     width,
                                     int                     height );

     DFBResult (*ProcessInput)     ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     const DFBInputEvent    *event );

     DFBResult (*FlushKeys)        ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data );

     DFBResult (*WindowAt)         ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     int                     x,
                                     int                     y,
                                     CoreWindow            **ret_window );

     DFBResult (*WindowLookup)     ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     DFBWindowID             window_id,
                                     CoreWindow            **ret_window );

     DFBResult (*EnumWindows)      ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     CoreWMWindowCallback    callback,
                                     void                   *callback_ctx );

     DFBResult (*WarpCursor)       ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     int                     x,
                                     int                     y );


   /** Window **/

     DFBResult (*AddWindow)        ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     CoreWindow             *window,
                                     void                   *window_data );

     DFBResult (*RemoveWindow)     ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     CoreWindow             *window,
                                     void                   *window_data );

     DFBResult (*SetWindowConfig)  ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data,
                                     const CoreWindowConfig *config,
                                     CoreWindowConfigFlags   flags );

     DFBResult (*RestackWindow)    ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data,
                                     CoreWindow             *relative,
                                     void                   *relative_data,
                                     int                     relation );

     DFBResult (*Grab)             ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data,
                                     CoreWMGrab             *grab );

     DFBResult (*Ungrab)           ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data,
                                     CoreWMGrab             *grab );

     DFBResult (*RequestFocus)     ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data );


   /** Updates **/

     DFBResult (*UpdateStack)      ( CoreWindowStack        *stack,
                                     void                   *wm_data,
                                     void                   *stack_data,
                                     const DFBRegion        *region,
                                     DFBSurfaceFlipFlags     flags );

     DFBResult (*UpdateWindow)     ( CoreWindow             *window,
                                     void                   *wm_data,
                                     void                   *window_data,
                                     const DFBRegion        *region,
                                     DFBSurfaceFlipFlags     flags );
} CoreWMFuncs;


void dfb_wm_get_info( CoreWMInfo *info );


DFBResult dfb_wm_init_stack    ( CoreWindowStack        *stack );

DFBResult dfb_wm_close_stack   ( CoreWindowStack        *stack,
                                 bool                    final );

DFBResult dfb_wm_set_active    ( CoreWindowStack        *stack,
                                 bool                    active );

DFBResult dfb_wm_resize_stack  ( CoreWindowStack        *stack,
                                 int                     width,
                                 int                     height );

DFBResult dfb_wm_process_input ( CoreWindowStack        *stack,
                                 const DFBInputEvent    *event );

DFBResult dfb_wm_flush_keys    ( CoreWindowStack        *stack );

DFBResult dfb_wm_window_at     ( CoreWindowStack        *stack,
                                 int                     x,
                                 int                     y,
                                 CoreWindow            **ret_window );

DFBResult dfb_wm_window_lookup ( CoreWindowStack        *stack,
                                 DFBWindowID             window_id,
                                 CoreWindow            **ret_window );

DFBResult dfb_wm_enum_windows  ( CoreWindowStack        *stack,
                                 CoreWMWindowCallback    callback,
                                 void                   *callback_ctx );

DFBResult dfb_wm_warp_cursor   ( CoreWindowStack        *stack,
                                 int                     x,
                                 int                     y );


DFBResult dfb_wm_add_window    ( CoreWindowStack        *stack,
                                 CoreWindow             *window );

DFBResult dfb_wm_remove_window ( CoreWindowStack        *stack,
                                 CoreWindow             *window );

DFBResult dfb_wm_set_window_config( CoreWindow             *window,
                                    const CoreWindowConfig *config,
                                    CoreWindowConfigFlags   flags );

DFBResult dfb_wm_restack_window( CoreWindow             *window,
                                 CoreWindow             *relative,
                                 int                     relation );

DFBResult dfb_wm_grab          ( CoreWindow             *window,
                                 CoreWMGrab             *grab );

DFBResult dfb_wm_ungrab        ( CoreWindow             *window,
                                 CoreWMGrab             *grab );

DFBResult dfb_wm_request_focus ( CoreWindow             *window );


DFBResult dfb_wm_update_stack  ( CoreWindowStack        *stack,
                                 const DFBRegion        *region,
                                 DFBSurfaceFlipFlags     flags );

DFBResult dfb_wm_update_window ( CoreWindow             *window,
                                 const DFBRegion        *region,
                                 DFBSurfaceFlipFlags     flags );

#endif
