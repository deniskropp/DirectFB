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

#include <asm/types.h>
#include <core/coretypes.h>

#include <core/fusion/object.h>
#include <core/fusion/lock.h>
#include <core/fusion/reactor.h>

#include <core/state.h>


/*
 * Hidden capability for software cursor, window will be the "super topmost".
 */
#define DWHC_TOPMOST     0x80000000

/*
 * a window
 */
struct _CoreWindow {
     FusionObject            object;

     DFBWindowID             id;

     int                     x;            /* x position in pixels */
     int                     y;            /* y position in pixels */
     int                     width;        /* width in pixels */
     int                     height;       /* width in pixels */

     DFBWindowCapabilities   caps;         /* window capabilities, to enable
                                              blending etc. */
     
     DFBWindowOptions        options;      /* flags for appearance/behaviour */
     DFBWindowEventType      events;       /* mask of enabled events */

     DFBWindowStackingClass  stacking;

     __u8                    opacity;      /* global alpha factor */
     __u32                   color_key;    /* transparent pixel */

     CoreSurface            *surface;      /* backing store surface */

     CoreWindowStack        *stack;        /* window stack the window belongs */

     bool                    initialized;  /* window has been inserted into
                                              the stack */
     bool                    destroyed;    /* window is (being) destroyed */
};

/*
 * a window stack
 */
struct _CoreWindowStack {
     DFBDisplayLayerID   layer_id;

     int                 width;
     int                 height;

     FusionObjectPool   *pool;            /* window pool */

     int                 num_windows;     /* number of windows on the stack */
     CoreWindow        **windows;         /* array of windows */

     CoreWindow         *pointer_window;  /* window grabbing the pointer */
     CoreWindow         *keyboard_window; /* window grabbing the keyboard */
     CoreWindow         *focused_window;  /* window having the focus */
     CoreWindow         *entered_window;  /* window under the pointer */

     struct {
          int            enabled;         /* is cursor enabled ? */
          int            x, y;            /* cursor position */
          CoreWindow    *window;          /* super-toplevel-window
                                             for software cursor */
          __u8           opacity;         /* cursor opacity */
          DFBRegion      region;          /* cursor is clipped by this region */

          int            numerator;       /* cursor acceleration */
          int            denominator;
          int            threshold;
     } cursor;

     FusionSkirmish      lock;            /* skirmish lock for repaints and
                                             management functions */

     int                 wm_hack;

     int                 wsp_opaque;      /* surface policy for opaque windows */
     int                 wsp_alpha;       /* surface policy for windows with
                                             an alphachannel */
     
     /* stores information on handling the background on exposure */
     struct {
          DFBDisplayLayerBackgroundMode mode;    
                                       /* background handling mode:
                                          don't care, solid color or image */

          DFBColor            color;   /* color for solid background mode */


          CoreSurface        *image;   /* surface for background image mode */
     } bg;    
};

/*
 * allocates a WindowStack, initializes it, registers it for input events
 */
CoreWindowStack*
dfb_windowstack_new( DisplayLayer *layer, int width, int height );

void
dfb_windowstack_destroy( CoreWindowStack *stack );

void
dfb_windowstack_resize( CoreWindowStack *stack,
                        int              width,
                        int              height );

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
 * must be called after dfb_window_create
 */
void
dfb_window_init( CoreWindow *window );

/*
 * must be called before dfb_window_destroy
 */
void
dfb_window_deinit( CoreWindow *window );

/*
 * deinitializes a window and removes it from the window stack
 */
void
dfb_window_destroy( CoreWindow *window, bool unref );

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
                    DFBSurfaceFlipFlags  flags );

/*
 * request a window to gain focus
 */
void
dfb_window_request_focus( CoreWindow *window );

DFBResult dfb_window_grab_keyboard( CoreWindow *window );
DFBResult dfb_window_ungrab_keyboard( CoreWindow *window );
DFBResult dfb_window_grab_pointer( CoreWindow *window );
DFBResult dfb_window_ungrab_pointer( CoreWindow *window );

void dfb_window_attach( CoreWindow *window, React react, void *ctx );
void dfb_window_detach( CoreWindow *window, React react, void *ctx );
void dfb_window_dispatch( CoreWindow *window, DFBWindowEvent *event );

/*
 * repaints all window on a window stack
 */
void dfb_windowstack_repaint_all( CoreWindowStack *stack );

/*
 * moves the cursor and handles events
 */
void dfb_windowstack_handle_motion( CoreWindowStack *stack, int dx, int dy );

#endif
