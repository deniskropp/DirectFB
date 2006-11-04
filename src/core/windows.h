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

#ifndef __WINDOWS_H__
#define __WINDOWS_H__

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <fusion/object.h>


typedef enum {
     CWCF_NONE      = 0x00000000,

     CWCF_POSITION  = 0x00000001,
     CWCF_SIZE      = 0x00000002,
     CWCF_OPACITY   = 0x00000004,
     CWCF_STACKING  = 0x00000008,

     CWCF_OPTIONS   = 0x00000010,
     CWCF_EVENTS    = 0x00000020,

     CWCF_COLOR_KEY = 0x00000100,
     CWCF_OPAQUE    = 0x00000200,

     CWCF_ALL       = 0x0000033F
} CoreWindowConfigFlags;

struct __DFB_CoreWindowConfig {
     DFBRectangle            bounds;         /* position and size */
     int                     opacity;        /* global alpha factor */
     DFBWindowStackingClass  stacking;       /* level boundaries */
     DFBWindowOptions        options;        /* flags for appearance/behaviour */
     DFBWindowEventType      events;         /* mask of enabled events */
     u32                     color_key;      /* transparent pixel */
     DFBRegion               opaque;         /* region of the window forced to be opaque */
};


#define TRANSLUCENT_WINDOW(w) ((w)->config.opacity < 0xff || \
                               (w)->config.options & (DWOP_ALPHACHANNEL | DWOP_COLORKEYING))

#define VISIBLE_WINDOW(w)     (!((w)->caps & DWCAPS_INPUTONLY) && \
                               (w)->config.opacity > 0 && !DFB_WINDOW_DESTROYED((w)))


/*
 * Creates a pool of window objects.
 */
FusionObjectPool *dfb_window_pool_create( const FusionWorld *world );

/*
 * Generates dfb_window_ref(), dfb_window_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreWindow, dfb_window )

/*
 * creates a window on a given stack
 */
DFBResult
dfb_window_create( CoreWindowStack        *stack,
                   int                     x,
                   int                     y,
                   int                     width,
                   int                     height,
                   DFBWindowCapabilities   caps,
                   DFBSurfaceCapabilities  surface_caps,
                   DFBSurfacePixelFormat   pixelformat,
                   CoreWindow            **window );

/*
 * deinitializes a window and removes it from the window stack
 */
void
dfb_window_destroy( CoreWindow *window );

/*
 * moves a window relative to its current position
 */
DFBResult
dfb_window_move( CoreWindow *window,
                 int         x,
                 int         y,
                 bool        relative );

/*
 * resizes a window
 */
DFBResult
dfb_window_resize( CoreWindow   *window,
                   int           width,
                   int           height );

/*
 * changes stacking class
 */
DFBResult
dfb_window_change_stacking( CoreWindow             *window,
                            DFBWindowStackingClass  stacking );

/*
 * move a window up one step in window stack
 */
DFBResult
dfb_window_raise( CoreWindow *window );

/*
 * move a window down one step in window stack
 */
DFBResult
dfb_window_lower( CoreWindow *window );

/*
 * makes a window the first (topmost) window in the window stack
 */
DFBResult
dfb_window_raisetotop( CoreWindow *window );

/*
 * makes a window the last (downmost) window in the window stack
 */
DFBResult
dfb_window_lowertobottom( CoreWindow *window );

/*
 * stacks the window on top of another one
 */
DFBResult
dfb_window_putatop( CoreWindow *window,
                    CoreWindow *lower );

/*
 * stacks the window below another one
 */
DFBResult
dfb_window_putbelow( CoreWindow *window,
                     CoreWindow *upper );

/*
 * sets the source color key
 */
DFBResult
dfb_window_set_colorkey( CoreWindow *window,
                         u32         color_key );

/*
 * sets the global alpha factor
 */
DFBResult
dfb_window_set_opacity( CoreWindow *window,
                        u8          opacity );

/*
 * sets the window options
 */
DFBResult
dfb_window_change_options( CoreWindow       *window,
                           DFBWindowOptions  disable,
                           DFBWindowOptions  enable );

/*
 * sets the window options
 */
DFBResult
dfb_window_set_opaque( CoreWindow      *window,
                       const DFBRegion *region );

/*
 * manipulates the event mask
 */
DFBResult
dfb_window_change_events( CoreWindow         *window,
                          DFBWindowEventType  disable,
                          DFBWindowEventType  enable );

/*
 * repaints part of a window, if region is NULL the whole window is repainted
 */
DFBResult
dfb_window_repaint( CoreWindow          *window,
                    const DFBRegion     *region,
                    DFBSurfaceFlipFlags  flags );

/*
 * request a window to gain focus
 */
DFBResult
dfb_window_request_focus( CoreWindow *window );

DFBResult dfb_window_grab_keyboard  ( CoreWindow                 *window );
DFBResult dfb_window_ungrab_keyboard( CoreWindow                 *window );
DFBResult dfb_window_grab_pointer   ( CoreWindow                 *window );
DFBResult dfb_window_ungrab_pointer ( CoreWindow                 *window );
DFBResult dfb_window_grab_key       ( CoreWindow                 *window,
                                      DFBInputDeviceKeySymbol     symbol,
                                      DFBInputDeviceModifierMask  modifiers );
DFBResult dfb_window_ungrab_key     ( CoreWindow                 *window,
                                      DFBInputDeviceKeySymbol     symbol,
                                      DFBInputDeviceModifierMask  modifiers );

void dfb_window_post_event( CoreWindow *window, DFBWindowEvent *event );

DFBResult dfb_window_send_configuration( CoreWindow *window );

DFBWindowID dfb_window_id( const CoreWindow *window );

CoreSurface *dfb_window_surface( const CoreWindow *window );

#endif
