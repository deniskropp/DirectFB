/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include "surfaces.h"
#include "state.h"

#include <asm/types.h>
#include <pthread.h>

struct _DisplayLayer;

/*
 * Hidden capability for software cursor,
 * this window will be "super topmost" and cannot get focus/events etc.
 */
#define DWHC_GHOST     0x80000000

typedef struct _CoreWindowStack  CoreWindowStack;

/*
 * linked list of DFBWindowEvents
 */
typedef struct _CoreWindowEvent
{
     DFBWindowEvent event;

     struct _CoreWindowEvent *next;
} CoreWindowEvent;

/*
 * a window
 */
typedef struct _CoreWindow
{
     int                 x;               /* x position in pixels */
     int                 y;               /* y position in pixels */
     int                 width;           /* width in pixels */
     int                 height;          /* width in pixels */

     __u8                opacity;         /* global alpha factor */

     unsigned int        caps;            /* window capabilities, to enable
                                             blending etc. */

     CoreSurface        *surface;         /* backing store surface */

     CoreWindowStack    *stack;           /* window stack the window belongs */

     CoreWindowEvent    *events;          /* event queue for the window */
     pthread_mutex_t     events_mutex;    /* mutex lock for accessing the
                                             event queue */
     pthread_mutex_t     wait;            /* mutex lock for idle waits
                                             used by WaitForEvent() */
} CoreWindow;

/*
 * a window stack
 */
struct _CoreWindowStack
{
     struct _DisplayLayer *layer;

     int                 num_windows;     /* number of windows on the stack */
     CoreWindow        **windows;         /* array of windows */

     CoreWindow         *pointer_window;  /* window grabbing the pointer */
     CoreWindow         *keyboard_window; /* window grabbing the keyboard */
     CoreWindow         *focused_window;  /* window having the focus */
     CoreWindow         *entered_window;  /* window under the pointer */

     int                 cx, cy;          /* cursor position */
     CoreWindow         *cursor;          /* super-toplevel-window for
                                             software cursor */

     CardState           state;           /* state for windowstack repaints */

     pthread_mutex_t     update;          /* mutex lock for repaints */


     int                 wsp_opaque;
     int                 wsp_alpha;

     int                 movemode;
     CoreWindow         *movewin;
};

/*
 * allocates a WindowStack, initializes it, registers it for input events
 */
CoreWindowStack* windowstack_new( struct _DisplayLayer *layer );

void windowstack_destroy( CoreWindowStack *stack );

/*
 * inserts a window into the windowstack pointed to by window->stack,
 * this function is called by window_create.
 */
void window_insert( CoreWindow *window, int before );

/*
 * removes a window from the windowstack pointed to by window->stack,
 * this function is NOT called by window_destroy, it has to be called BEFORE.
 */
void window_remove( CoreWindow *window );

/*
 * creates a window on a given stack
 */
CoreWindow* window_create( CoreWindowStack *stack, int x, int y,
                           unsigned int width, unsigned int height,
                           unsigned int caps );
/*
 * deinitializes a window and removes it from the window stack
 */
void window_destroy( CoreWindow *window );

/*
 * moves a window relative to its current position
 */
int window_move( CoreWindow *window, int deltax, int deltay );

/*
 * resizes a window
 */
int window_resize( CoreWindow *window, int width, int height );

/*
 * move a window up one step in window stack
 */
int window_raise( CoreWindow *window );

/*
 * move a window down one step in window stack
 */
int window_lower( CoreWindow *window );

/*
 * makes a window the first (topmost) window in the window stack
 */
int window_raisetotop( CoreWindow *window );

/*
 * makes a window the last (downmost) window in the window stack
 */
int window_lowertobottom( CoreWindow *window );

/*
 * sets the global alpha factor of a window
 */
int window_set_opacity( CoreWindow *window, __u8 opacity );

/*
 * repaints part of a window, if rect is NULL, whole window is repainted
 */
int window_repaint( CoreWindow *window, DFBRectangle *rect );

/*
 * waits until the window has an event is its queue
 */
int window_waitforevent( CoreWindow *window );

/*
 * gets and removes the next event from the event queue
 */
int window_getevent( CoreWindow *window, DFBWindowEvent *event );

/*
 * gets the next event from the event queue without removing it
 */
int window_peekevent( CoreWindow *window, DFBWindowEvent *event );

/*
 * request a window to gain focus
 */
int window_request_focus( CoreWindow *window );

DFBResult window_grab_keyboard( CoreWindow *window );
DFBResult window_ungrab_keyboard( CoreWindow *window );
DFBResult window_grab_pointer( CoreWindow *window );
DFBResult window_ungrab_pointer( CoreWindow *window );

/*
 * repaints all window on a window stack
 */
void windowstack_repaint_all( CoreWindowStack *stack );

/*
 * appends event to the event queue of a window
 */
inline void window_append_event( CoreWindow *window, DFBWindowEvent *event );

/*
 * moves the cursor and handles events
 */
void windowstack_handle_motion( CoreWindowStack *stack, int dx, int dy );

#endif
