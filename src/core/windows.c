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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <malloc.h>

#include <directfb.h>

#include <pthread.h>

#include <misc/conf.h>
#include <misc/util.h>
#include <gfx/util.h>

#include "coredefs.h"
#include "windows.h"

#include "layers.h"
#include "gfxcard.h"
#include "input.h"

#define min(a,b)     ((a) < (b) ? (a) : (b))

static void windowstack_repaint( CoreWindowStack *stack, int x, int y,
                                 int width, int height, int erase );
static CoreWindow* window_at_pointer( CoreWindowStack *stack, int x, int y );
static void windowstack_input_notify( DFBInputEvent *evt, void *ctx );
static int windowstack_handle_enter_leave_focus( CoreWindowStack *stack );


CoreWindowStack* windowstack_new( DisplayLayer *layer )
{
     InputDevice *inputdevice = inputdevices;
     CoreWindowStack *stack;

     stack = (CoreWindowStack*) calloc ( 1, sizeof(CoreWindowStack) );

     stack->layer = layer;

     stack->wsp_opaque = stack->wsp_alpha = CSP_SYSTEMONLY;

     if (card->caps.accel & DFXL_BLIT) {
          stack->wsp_opaque = CSP_VIDEOHIGH;
          if (card->caps.blitting & DSBLIT_BLEND_ALPHACHANNEL)
               stack->wsp_alpha = CSP_VIDEOHIGH;
     }

     if (config->window_policy != -1) {
          stack->wsp_opaque = stack->wsp_alpha = config->window_policy;
     }

     pthread_mutex_init( &stack->update, NULL );

     while (inputdevice) {
          switch (DIDID_TYPE(inputdevice->id)) {
               case DIDT_KEYBOARD:
               case DIDT_MOUSE:
                    input_add_listener( inputdevice,
                                        windowstack_input_notify, stack );
                    break;
               default:
                    break;
          }
          inputdevice = inputdevice->next;
     }

     /* initialize state for repaints */
     stack->state.modified = SMF_ALL;
     state_set_destination( &stack->state, layer->surface );

     return stack;
}

void windowstack_destroy( CoreWindowStack *stack )
{
     int i;
     InputDevice *inputdevice = inputdevices;

     state_set_destination( &stack->state, NULL );

     while (inputdevice) {
          switch (DIDID_TYPE(inputdevice->id)) {
               case DIDT_KEYBOARD:
               case DIDT_MOUSE:
                    input_remove_listener( inputdevice, stack );
                    break;
               default:
                    break;
          }
          inputdevice = inputdevice->next;
     }

     pthread_mutex_lock( &stack->update );

     for (i=0; i<stack->num_windows; i++)
          window_destroy( stack->windows[i] );

     if (stack->windows) {
          stack->num_windows = 0;
          free( stack->windows );
          stack->windows = NULL;
     }

     pthread_mutex_unlock( &stack->update );

     pthread_mutex_destroy( &stack->update );

     free( stack );
}

void window_insert( CoreWindow *window, int before )
{
     int i;
     CoreWindowStack *stack = window->stack;

     if (before < 0  ||  before > stack->num_windows)
          before = stack->num_windows;

     pthread_mutex_lock( &stack->update );

     stack->windows = realloc( stack->windows,
                               sizeof(CoreWindow*) * (stack->num_windows + 1) );

     for (i=stack->num_windows; i>before; i--)
          stack->windows[i] = stack->windows[i-1];

     stack->windows[before] = window;

     stack->num_windows++;

     pthread_mutex_unlock( &stack->update );

     if (!(window->caps & DWHC_GHOST)) {
          DFBWindowEvent evt;

          evt.type = DWET_POSITION_SIZE;
          evt.x = window->x;
          evt.y = window->y;
          evt.w = window->width;
          evt.h = window->height;
          window_append_event( window, &evt );
     }

     if (window->opacity)
          windowstack_handle_enter_leave_focus( stack );
}

void window_remove( CoreWindow *window )
{
     int i;
     CoreWindowStack *stack = window->stack;

     pthread_mutex_lock( &stack->update );

     if (stack->entered_window == window)
          stack->entered_window = NULL;

     if (stack->focused_window == window)
          stack->focused_window = NULL;

     if (stack->keyboard_window == window)
          stack->keyboard_window = NULL;

     if (stack->pointer_window == window)
          stack->pointer_window = NULL;

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i] == window)
               break;

     if (i < stack->num_windows) {
          stack->num_windows--;

          for (; i<stack->num_windows; i++)
               stack->windows[i] = stack->windows[i+1];

          if (stack->num_windows)
               stack->windows = realloc( stack->windows,
                                         sizeof(CoreWindow*) * (stack->num_windows) );
          else {
               free( stack->windows );
               stack->windows = NULL;
          }
     }

     pthread_mutex_unlock( &stack->update );

     windowstack_repaint( stack, window->x, window->y,
                          window->width, window->height, 1 );

     {
          DFBWindowEvent evt;

          evt.type = DWET_CLOSE;

          window_append_event( window, &evt );
     }

     if (window->opacity)
          windowstack_handle_enter_leave_focus( stack );
}

CoreWindow* window_create( CoreWindowStack *stack, int x, int y,
                           unsigned int width, unsigned int height,
                           unsigned int caps )
{
     int i;
     CoreWindow* window;

     window = (CoreWindow*) calloc( 1, sizeof(CoreWindow) );

     window->x = x;
     window->y = y;
     window->width = width;
     window->height = height;

     window->caps = caps;
     window->opacity = 0;

     window->stack = stack;

     if (caps & DWCAPS_ALPHACHANNEL)
          surface_create( width, height, DSPF_ARGB, stack->wsp_alpha,
                          DSCAPS_NONE, &window->surface );
     else
          surface_create( width, height, stack->layer->surface->format,
                          stack->wsp_opaque, DSCAPS_NONE, &window->surface );


     pthread_mutex_init( &window->events_mutex, NULL );
     pthread_mutex_init( &window->wait, NULL );

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i]->caps & DWHC_GHOST)
               break;

     window_insert( window, i );

     return window;
}

void window_destroy( CoreWindow *window )
{
     DFBWindowEvent evt;

     evt.type = DWET_CLOSE;
     window_append_event( window, &evt );

     surface_destroy( window->surface );

     /* unlock in case a thread does WaitForEvent */
     pthread_mutex_unlock( &window->wait );
     pthread_mutex_destroy( &window->wait );

     pthread_mutex_destroy( &window->events_mutex );

     free( window );
}

int window_raise( CoreWindow *window )
{
     int i;
     int update = 0;
     CoreWindowStack *stack = window->stack;

     pthread_mutex_lock( &stack->update );

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i] == window)
               break;

     if (i < stack->num_windows-1  &&  !(stack->windows[i+1]->caps & DWHC_GHOST)) {
          CoreWindow *temp = stack->windows[i+1];
          stack->windows[i+1] = stack->windows[i];
          stack->windows[i] = temp;

          update = 1;
     }

     pthread_mutex_unlock( &stack->update );

     if (update && window->opacity) {
          windowstack_repaint( stack, window->x, window->y,
                               window->width, window->height, 1 /*FIXME*/);

          windowstack_handle_enter_leave_focus( stack );
     }

     return DFB_OK;
}

int window_lower( CoreWindow *window )
{
     int i;
     int update = 0;
     CoreWindowStack *stack = window->stack;

     pthread_mutex_lock( &stack->update );

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i] == window)
               break;

     if (i > 0) {
          CoreWindow *temp = stack->windows[i];
          stack->windows[i] = stack->windows[i-1];
          stack->windows[i-1] = temp;

          update = 1;
     }

     pthread_mutex_unlock( &stack->update );

     if (update && window->opacity) {
          windowstack_repaint( stack, window->x, window->y,
                               window->width, window->height, 1 /*FIXME*/);

          windowstack_handle_enter_leave_focus( stack );
     }

     return DFB_OK;
}

int window_raisetotop( CoreWindow *window )
{
     int i;
     int update = 0;
     CoreWindowStack *stack = window->stack;

     pthread_mutex_lock( &stack->update );

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i] == window)
               break;

     for (; i<stack->num_windows-1; i++) {
          if (!(stack->windows[i+1]->caps & DWHC_GHOST)) {
               CoreWindow *temp = stack->windows[i];
               stack->windows[i] = stack->windows[i+1];
               stack->windows[i+1] = temp;

               update = 1;
          }
          else
               break;
     }

     pthread_mutex_unlock( &stack->update );

     if (update && window->opacity) {
          windowstack_repaint( stack, window->x, window->y,
                               window->width, window->height, 1 /*FIXME*/);

          windowstack_handle_enter_leave_focus( stack );
     }

     return DFB_OK;
}

int window_lowertobottom( CoreWindow *window )
{
     int i;
     int update = 0;
     CoreWindowStack *stack = window->stack;

     pthread_mutex_lock( &stack->update );

     for (i=0; i<stack->num_windows; i++)
          if (stack->windows[i] == window)
               break;

     for (; i>0; i--) {
          CoreWindow *temp = stack->windows[i];
          stack->windows[i] = stack->windows[i-1];
          stack->windows[i-1] = temp;

          update = 1;
     }

     pthread_mutex_unlock( &stack->update );

     if (update && window->opacity) {
          windowstack_repaint( stack, window->x, window->y,
                               window->width, window->height, 1 /*FIXME*/);

          windowstack_handle_enter_leave_focus( stack );
     }

     return DFB_OK;
}

int window_move( CoreWindow *window, int dx, int dy )
{
     CoreWindowStack *stack = window->stack;
     int rx = window->x;
     int ry = window->y;
     int rw = window->width;
     int rh = window->height;

     window->x += dx;
     window->y += dy;

     if (window->opacity) {

          if (dx > 0)
               rw += dx;
          if (dx < 0) {
               rw -= dx;
               rx += dx;
          }

          if (dy > 0)
               rh += dy;
          if (dy < 0) {
               rh -= dy;
               ry += dy;
          }

          windowstack_repaint( stack, rx, ry, rw, rh, 1 );
     }

     if (!(window->caps & DWHC_GHOST)) {
          DFBWindowEvent evt;

          evt.type = DWET_POSITION;
          evt.x = window->x;
          evt.y = window->y;
          window_append_event( window, &evt );
     }

     return DFB_OK;
}

int window_resize( CoreWindow *window, unsigned int width, unsigned int height )
{
     CoreWindowStack *stack = window->stack;
     int ow = window->width;
     int oh = window->height;

     surface_reformat( window->surface, width, height,
                       window->surface->format );

     window->width = window->surface->width;
     window->height = window->surface->height;

     if (window->opacity) {
          windowstack_repaint( stack, window->x, window->y,
                               MAX(ow, width), MAX(oh, height), 1 );
     }

     if (!(window->caps & DWHC_GHOST)) {
          DFBWindowEvent evt;

          evt.type = DWET_SIZE;
          evt.w = window->width;
          evt.h = window->height;
          window_append_event( window, &evt );
     }

     return DFB_OK;
}

int window_set_opacity( CoreWindow *window, __u8 opacity )
{
     int old_opacity = window->opacity;
     CoreWindowStack *stack = window->stack;

     if (old_opacity != opacity) {
          int erase = 0;

          window->opacity = opacity;

          if ((window->caps & DWCAPS_ALPHACHANNEL) || opacity != 0xFF)
               erase = 1;

          /* window_repaint() refuses repainting invisible windows */
          windowstack_repaint( stack, window->x, window->y,
                               window->width, window->height, erase );

          if ((old_opacity && !opacity) || (!old_opacity && opacity))
               windowstack_handle_enter_leave_focus( stack );
     }

     return DFB_OK;
}

int window_repaint( CoreWindow *window, DFBRectangle *rect )
{
     if (window->opacity) {
          int erase = 0;
          CoreWindowStack *stack = window->stack;

          if ((window->caps & DWCAPS_ALPHACHANNEL) || window->opacity != 0xFF)
               erase = 1;

          if (!rect)
               windowstack_repaint( stack, window->x, window->y,
                                    window->width, window->height, erase );
          else
               windowstack_repaint( stack, rect->x + window->x,
                                    rect->y + window->y, rect->w, rect->h,
                                    erase );
     }

     return DFB_OK;
}

int window_waitforevent( CoreWindow *window )
{
//     pthread_mutex_lock( &window->events_mutex );

     if (window->events)
          return DFB_OK;

     pthread_mutex_lock( &window->wait );

//     pthread_mutex_unlock( &window->events_mutex );

     pthread_mutex_lock( &window->wait );
     pthread_mutex_unlock( &window->wait );

     return DFB_OK;
}

int window_getevent( CoreWindow *window, DFBWindowEvent *event )
{
     pthread_mutex_lock( &window->events_mutex );

     if (!window->events) {
          pthread_mutex_unlock( &window->events_mutex );
          return DFB_BUFFEREMPTY;
     }

     *event = window->events->event;

     {
          CoreWindowEvent *e = window->events;
          window->events = window->events->next;
          free( e );
     }

     pthread_mutex_unlock( &window->events_mutex );

     return DFB_OK;
}

int window_peekevent( CoreWindow *window, DFBWindowEvent *event )
{
     if (!window->events)
          return DFB_BUFFEREMPTY;

     *event = window->events->event;

     return DFB_OK;
}

DFBResult window_grab_keyboard( CoreWindow *window )
{
     DFBResult retval = DFB_OK;
     CoreWindowStack *stack = window->stack;

     pthread_mutex_lock( &stack->update );

     if (stack->keyboard_window)
          retval = DFB_LOCKED;
     else
          stack->keyboard_window = window;

     pthread_mutex_unlock( &stack->update );

     return retval;
}

DFBResult window_ungrab_keyboard( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     pthread_mutex_lock( &stack->update );

     if (stack->keyboard_window == window)
          stack->keyboard_window = NULL;

     pthread_mutex_unlock( &stack->update );

     return DFB_OK;
}

DFBResult window_grab_pointer( CoreWindow *window )
{
     DFBResult retval = DFB_OK;
     CoreWindowStack *stack = window->stack;

     pthread_mutex_lock( &stack->update );

     if (stack->pointer_window)
          retval = DFB_LOCKED;
     else
          stack->pointer_window = window;

     pthread_mutex_unlock( &stack->update );

     return retval;
}

DFBResult window_ungrab_pointer( CoreWindow *window )
{
     CoreWindowStack *stack = window->stack;

     pthread_mutex_lock( &stack->update );

     if (stack->pointer_window == window)
          stack->pointer_window = NULL;

     pthread_mutex_unlock( &stack->update );

     return DFB_OK;
}

int window_request_focus( CoreWindow *window )
{
     DFBWindowEvent evt;

     CoreWindowStack *stack   = window->stack;
     CoreWindow      *current = stack->focused_window;


     if (window->caps & DWHC_GHOST) {
          BUG( "Ghost Window requested focus!" );
          return DFB_BUG;
     }

     if (current == window)
          return DFB_OK;

     if (current) {
          evt.type = DWET_LOSTFOCUS;
          window_append_event( current, &evt );
     }

     evt.type = DWET_GOTFOCUS;
     window_append_event( window, &evt );

     stack->focused_window = window;

     return DFB_OK;
}

void windowstack_repaint_all( CoreWindowStack *stack )
{
     CoreSurface *surface = stack->layer->surface;

     windowstack_repaint( stack, 0, 0, surface->width, surface->height, 1 );
}

inline void window_append_event( CoreWindow *window, DFBWindowEvent *event )
{
     CoreWindowEvent *evt;

     if (window->caps & DWHC_GHOST) {
          BUG( "Sending event to Ghost Window!!!" );
          return;
     }

     evt = (CoreWindowEvent*) calloc( 1, sizeof(CoreWindowEvent) );

     evt->event = *event;

     pthread_mutex_lock( &window->events_mutex );

     if (!window->events) {
          window->events = evt;
     }
     else {
          CoreWindowEvent *e = window->events;

          while (e->next)
               e = e->next;

          e->next = evt;
     }

     pthread_mutex_unlock( &window->events_mutex );

     pthread_mutex_unlock( &window->wait );
}


/*
 * internals
 */

static void windowstack_repaint( CoreWindowStack *stack, int x, int y,
                                 int width, int height, int erase )
{
     int i;
     DisplayLayer *layer = stack->layer;
     CoreSurface *surface = layer->surface;
     DFBRegion update_region = { x, y, x + width - 1, y + height - 1 };

     if (layer->exclusive)
          return;

     if (!region_intersect( &update_region, 0, 0, surface->width - 1,
                                                  surface->height - 1 ))
          return;

     pthread_mutex_lock( &stack->update );

     stack->state.clip = update_region;
     stack->state.src_blend = DSBF_SRCALPHA;
     stack->state.dst_blend = DSBF_INVSRCALPHA;
     stack->state.modified |= SMF_CLIP | SMF_SRC_BLEND | SMF_DST_BLEND;

     if (erase) {
          DFBRectangle rect = { update_region.x1, update_region.y1,
                                update_region.x2 - update_region.x1 + 1,
                                update_region.y2 - update_region.y1 + 1 };

          switch (layer->bg.mode) {
               case DLBM_DONTCARE:
                    break;
               case DLBM_COLOR:
                    stack->state.drawingflags = DSDRAW_NOFX;
                    stack->state.color = layer->bg.color;
                    stack->state.modified |= SMF_COLOR | SMF_DRAWING_FLAGS;
                    gfxcard_fillrectangle( &rect, &stack->state );
                    break;
               case DLBM_IMAGE:
                    stack->state.source = layer->bg.image;
                    stack->state.blittingflags = DSBLIT_NOFX;
                    stack->state.modified |= SMF_SOURCE | SMF_BLITTING_FLAGS;
                    gfxcard_blit( &rect, update_region.x1, update_region.y1,
                                  &stack->state );
                    break;
          }
     }

     for (i=0; i<stack->num_windows; i++) {
          CoreWindow *window = stack->windows[i];
          DFBRectangle sr = { 0, 0, window->width, window->height };

          if (window->opacity == 0)
               continue;

          stack->state.source = window->surface;
          stack->state.blittingflags = DSBLIT_NOFX;
          stack->state.modified |= SMF_SOURCE | SMF_BLITTING_FLAGS;

          if (window->caps & DWCAPS_ALPHACHANNEL)
               stack->state.blittingflags |= DSBLIT_BLEND_ALPHACHANNEL;

          if (window->opacity != 0xFF) {
               stack->state.blittingflags |= DSBLIT_BLEND_COLORALPHA;

               if (stack->state.color.a != window->opacity) {
                    stack->state.color.a = window->opacity;
                    stack->state.modified |= SMF_COLOR;
               }
          }

          gfxcard_blit( &sr, window->x, window->y, &stack->state );
     }

     if (surface->caps & DSCAPS_FLIPPING) {
          if (update_region.x1 == 0 &&
              update_region.y1 == 0 &&
              update_region.x2 == layer->width - 1 &&
              update_region.y2 == layer->height - 1)
          {
               layer->FlipBuffers( layer );
          }
          else {
               DFBRectangle rect = { update_region.x1, update_region.y1,
                                     update_region.x2 - update_region.x1 + 1,
                                     update_region.y2 - update_region.y1 + 1 };

               back_to_front_copy( surface, &rect );
          }
     }

     pthread_mutex_unlock( &stack->update );
}

static CoreWindow* window_at_pointer( CoreWindowStack *stack, int x, int y )
{
     int i;

     if (x < 0)
          x = stack->cx;
     if (y < 0)
          y = stack->cy;

     for (i=stack->num_windows-1; i>=0; i--) {
          CoreWindow *w = stack->windows[i];

          if (!(w->caps & DWHC_GHOST)  &&  w->opacity  &&
                                           x >= w->x  &&  x < w->x+w->width &&
                                           y >= w->y  &&  y < w->y+w->height)
               return w;
     }

     return NULL;
}

static void windowstack_input_notify( DFBInputEvent *evt, void *ctx )
{
     CoreWindow *window = NULL;
     DFBWindowEvent we;
     CoreWindowStack *stack = (CoreWindowStack*)ctx;

     switch (evt->type) {
          case DIET_KEYPRESS:
          case DIET_KEYRELEASE:
               window         = stack->keyboard_window ?
                                stack->keyboard_window : stack->focused_window;

               if (window) {
                    we.type = (evt->type == DIET_KEYPRESS) ? DWET_KEYDOWN :
                                                             DWET_KEYUP;
                    we.key_ascii   = evt->key_ascii;
                    we.key_unicode = evt->key_unicode;
                    we.keycode     = evt->keycode;
                    we.modifiers   = evt->modifiers;

                    window_append_event( window, &we );
               }

               break;
          case DIET_BUTTONPRESS:
          case DIET_BUTTONRELEASE:
               window         = stack->pointer_window ?
                                stack->pointer_window : stack->focused_window;

               if (window) {
                    we.type = (evt->type == DIET_BUTTONPRESS) ? DWET_BUTTONDOWN :
                                                                DWET_BUTTONUP;
                    we.button = evt->button;
                    we.cx     = stack->cx;
                    we.cy     = stack->cy;
                    we.x      = we.cx - window->x;
                    we.y      = we.cy - window->y;

                    window_append_event( window, &we );
               }

               break;
          case DIET_AXISMOTION:
               if (evt->flags & DIEF_AXISREL) {
                    switch (evt->axis) {
                         case DIAI_X:
                              windowstack_handle_motion( stack,
                                                         evt->axisrel, 0 );
                              break;
                         case DIAI_Y:
                              windowstack_handle_motion( stack,
                                                         0, evt->axisrel );
                              break;
                         default:
                              return;
                    }
               }
               break;
          default:
               break;
     }
}

void windowstack_handle_motion( CoreWindowStack *stack, int dx, int dy )
{
     DFBWindowEvent we;

     if (!stack->cursor)
          return;

     stack->cx += dx;
     stack->cy += dy;

     window_move( stack->cursor, dx, dy );


     we.cx   = stack->cx;
     we.cy   = stack->cy;

     if (stack->pointer_window) {
          we.type = DWET_MOTION;
          we.x    = we.cx - stack->pointer_window->x;
          we.y    = we.cy - stack->pointer_window->y;

          window_append_event( stack->pointer_window, &we );
     }
     else {
          if (!windowstack_handle_enter_leave_focus( stack )
              && stack->entered_window)
          {
               we.type = DWET_MOTION;
               we.x    = we.cx - stack->entered_window->x;
               we.y    = we.cy - stack->entered_window->y;

               window_append_event( stack->entered_window, &we );
          }
     }

     HEAVYDEBUGMSG("DirectFB/windows: mouse at %d, %d\n", stack->cx, stack->cy);
}

static int windowstack_handle_enter_leave_focus( CoreWindowStack *stack )
{
     CoreWindow    *before = stack->entered_window;
     CoreWindow    *after = window_at_pointer( stack, -1, -1 );

     if (before != after) {
          DFBWindowEvent we;

          we.cx   = stack->cx;
          we.cy   = stack->cy;

          if (before) {
               we.type = DWET_LEAVE;
               we.x    = we.cx - before->x;
               we.y    = we.cy - before->y;

               window_append_event( before, &we );
          }

          if (after) {
               window_request_focus( after );

               we.type = DWET_ENTER;
               we.x    = we.cx - after->x;
               we.y    = we.cy - after->y;

               window_append_event( after, &we );
          }

          stack->entered_window = after;

          return 1;
     }

     return 0;
}

