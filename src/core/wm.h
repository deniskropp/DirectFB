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


DECLARE_MODULE_DIRECTORY( dfb_core_wm_modules );


/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFB_CORE_WM_ABI_VERSION           1

#define DFB_CORE_WM_INFO_NAME_LENGTH     60
#define DFB_CORE_WM_INFO_VENDOR_LENGTH   80
#define DFB_CORE_WM_INFO_URL_LENGTH     120
#define DFB_CORE_WM_INFO_LICENSE_LENGTH  40


typedef struct {
     int            major;
     int            minor;
} CoreWMVersion;

typedef struct {
     CoreWMVersion  version;

     char           name   [DFB_CORE_WM_INFO_NAME_LENGTH];
     char           vendor [DFB_CORE_WM_INFO_VENDOR_LENGTH];
     char           url    [DFB_CORE_WM_INFO_URL_LENGTH];
     char           license[DFB_CORE_WM_INFO_LICENSE_LENGTH];

     unsigned int   wm_data_size;
     unsigned int   stack_data_size;
     unsigned int   window_data_size;
} CoreWMInfo;

typedef struct {
   /** Module **/

     void      (*GetWMInfo)        ( CoreWMInfo                   *info );

     DFBResult (*Initialize)       ( CoreDFB                      *core,
                                     void                         *wm_data );

     DFBResult (*Join)             ( CoreDFB                      *core,
                                     void                         *wm_data );

     DFBResult (*Shutdown)         ( bool                          emergency,
                                     void                         *wm_data );

     DFBResult (*Leave)            ( bool                          emergency,
                                     void                         *wm_data );

     DFBResult (*Suspend)          ( void                         *wm_data );
     DFBResult (*Resume)           ( void                         *wm_data );


   /** Stacks **/

     DFBResult (*InitStack)        ( CoreWindowStack              *stack,
                                     void                         *wm_data,
                                     void                         *stack_data );

     DFBResult (*CloseStack)       ( CoreWindowStack              *stack,
                                     void                         *wm_data,
                                     void                         *stack_data );

     DFBResult (*ProcessInput)     ( CoreWindowStack              *stack,
                                     void                         *wm_data,
                                     void                         *stack_data,
                                     const DFBInputEvent          *event );

     DFBResult (*WindowAt)         ( CoreWindowStack              *stack,
                                     void                         *wm_data,
                                     void                         *stack_data,
                                     int                           x,
                                     int                           y,
                                     CoreWindow                  **ret_window );

     DFBResult (*WarpCursor)       ( CoreWindowStack              *stack,
                                     void                         *wm_data,
                                     void                         *stack_data,
                                     int                           x,
                                     int                           y );

     DFBResult (*UpdateFocus)      ( CoreWindowStack              *stack,
                                     void                         *wm_data,
                                     void                         *stack_data );


   /** Windows **/

     DFBResult (*CreateWindow)     ( CoreWindowStack              *stack,
                                     void                         *wm_data,
                                     void                         *stack_data,
                                     void                         *window_data );
} CoreWMFuncs;


void dfb_wm_get_info( CoreWMInfo *info );


DFBResult dfb_wm_init_stack   ( CoreWindowStack      *stack );

DFBResult dfb_wm_close_stack  ( CoreWindowStack      *stack );

DFBResult dfb_wm_process_input( CoreWindowStack      *stack,
                                const DFBInputEvent  *event );

DFBResult dfb_wm_window_at    ( CoreWindowStack      *stack,
                                int                   x,
                                int                   y,
                                CoreWindow          **ret_window );

DFBResult dfb_wm_warp_cursor  ( CoreWindowStack      *stack,
                                int                   x,
                                int                   y );

DFBResult dfb_wm_update_focus ( CoreWindowStack      *stack );

#endif
