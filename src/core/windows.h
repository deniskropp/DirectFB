/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <core/fusion/object.h>


typedef enum {
     CWUF_NONE      = 0x00000000,

     CWUF_SURFACE   = 0x00000001,
     CWUF_POSITION  = 0x00000002,
     CWUF_SIZE      = 0x00000004,
     CWUF_OPTIONS   = 0x00000008,
     CWUF_OPACITY   = 0x00000010,
     CWUF_COLORKEY  = 0x00000020,
     CWUF_PALETTE   = 0x00000040,

     CWUF_ALL       = 0x0000003F
} CoreWindowUpdateFlags;

#define TRANSLUCENT_WINDOW(w) ((w)->opacity < 0xff || \
                               (w)->options & (DWOP_ALPHACHANNEL | \
                                               DWOP_COLORKEYING))

#define VISIBLE_WINDOW(w)     (!((w)->caps & DWCAPS_INPUTONLY) && \
                               (w)->opacity > 0 && !(w)->destroyed)


/*
 * Creates a pool of window objects.
 */
FusionObjectPool *dfb_window_pool_create();

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
                   CorePalette            *palette,
                   CoreWindow            **window );

/*
 * must be called after dfb_window_create
 */
void
dfb_window_init( CoreWindow *window );

/*
 * deinitializes a window and removes it from the window stack
 */
void
dfb_window_destroy( CoreWindow *window );

/*
 * moves a window relative to its current position
 */
void
dfb_window_move( CoreWindow *window,
                 int         deltax,
                 int         deltay );

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
void
dfb_window_change_stacking( CoreWindow             *window,
                            DFBWindowStackingClass  stacking );

/*
 * move a window up one step in window stack
 */
void
dfb_window_raise( CoreWindow *window );

/*
 * move a window down one step in window stack
 */
void
dfb_window_lower( CoreWindow *window );

/*
 * makes a window the first (topmost) window in the window stack
 */
void
dfb_window_raisetotop( CoreWindow *window );

/*
 * makes a window the last (downmost) window in the window stack
 */
void
dfb_window_lowertobottom( CoreWindow *window );

/*
 * stacks the window on top of another one
 */
void
dfb_window_putatop( CoreWindow *window,
                    CoreWindow *lower );

/*
 * stacks the window below another one
 */
void
dfb_window_putbelow( CoreWindow *window,
                     CoreWindow *upper );

/*
 * sets the global alpha factor of a window
 */
void
dfb_window_set_opacity( CoreWindow *window,
                        __u8        opacity );

/*
 * repaints part of a window, if region is NULL the whole window is repainted
 */
void
dfb_window_repaint( CoreWindow          *window,
                    DFBRegion           *region,
                    DFBSurfaceFlipFlags  flags,
                    bool                 force_complete,
                    bool                 force_invisible );

/*
 * request a window to gain focus
 */
void
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

DFBWindowID dfb_window_id( const CoreWindow *window );

#endif
