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
#include <sys/time.h>
#include <errno.h>

#include <pthread.h>

#include "directfb.h"
#include "directfb_internals.h"

#include "core/core.h"
#include "core/coredefs.h"
#include "core/coretypes.h"

#include "core/reactor.h"
#include "core/state.h"
#include "core/surfaces.h"
#include "core/windows.h"

#include "display/idirectfbsurface.h"
#include "display/idirectfbsurface_window.h"

#include "idirectfbwindow.h"

/*
 * adds an window event to the event queue
 */
static ReactionResult IDirectFBWindow_React( const void *msg_data,
                                             void       *ctx );


typedef struct _WindowBufferItem
{
     DFBWindowEvent           evt;
     struct _WindowBufferItem *next;
} IDirectFBWindowBuffer_item;


typedef struct {
     int                            ref;
     CoreWindow                    *window;

     IDirectFBSurface              *surface;

     IDirectFBWindowBuffer_item    *events;         /* linked list containing
                                                       events */

     pthread_mutex_t                events_mutex;   /* mutex lock for accessing
                                                       the event queue */

     pthread_cond_t                 wait_condition; /* condition used for idle
                                                       wait in WaitForEvent() */
} IDirectFBWindow_data;


static void IDirectFBWindow_Destruct( IDirectFBWindow *thiz )
{
     IDirectFBWindow_data *data = (IDirectFBWindow_data*)thiz->priv;

     if (data->surface)
          data->surface->Release( data->surface );

     reactor_detach( data->window->reactor, IDirectFBWindow_React, data );

     window_remove( data->window );
     window_destroy( data->window );

     pthread_cond_destroy( &data->wait_condition );
     pthread_mutex_destroy( &data->events_mutex );

     free( data );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

static DFBResult IDirectFBWindow_AddRef( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBWindow_Release( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (--data->ref == 0)
          IDirectFBWindow_Destruct( thiz );

     return DFB_OK;
}

static DFBResult IDirectFBWindow_GetPosition( IDirectFBWindow *thiz,
                                              int *x, int *y )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!x && !y)
          return DFB_INVARG;

     if (x)
          *x = data->window->x;

     if (y)
          *y = data->window->y;

     return DFB_OK;
}

static DFBResult IDirectFBWindow_GetSize( IDirectFBWindow *thiz,
                                          unsigned int    *width,
                                          unsigned int    *height )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!width && !height)
          return DFB_INVARG;

     if (width)
          *width = data->window->width;

     if (height)
          *height = data->window->height;

     return DFB_OK;
}

static DFBResult IDirectFBWindow_GetSurface( IDirectFBWindow   *thiz,
                                             IDirectFBSurface **surface )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!surface)
          return DFB_INVARG;

     if (!data->surface) {
          DFBResult ret;

          DFB_ALLOCATE_INTERFACE( *surface, IDirectFBSurface );

          ret = IDirectFBSurface_Window_Construct( *surface, NULL, NULL,
                                                   data->window, 0 );
          if (ret) {
               free( *surface );
               return ret;
          }

          data->surface = *surface;
     }
     else
          *surface = data->surface;

     data->surface->AddRef( data->surface );

     return DFB_OK;
}

static DFBResult IDirectFBWindow_SetOpacity( IDirectFBWindow *thiz,
                                             __u8 opacity )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->window->opacity != opacity)
          return window_set_opacity( data->window, opacity );

     return DFB_OK;
}

static DFBResult IDirectFBWindow_GetOpacity( IDirectFBWindow *thiz,
                                             __u8 *opacity )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (!opacity)
          return DFB_INVARG;

     *opacity = data->window->opacity;

     return DFB_OK;
}

static DFBResult IDirectFBWindow_RequestFocus( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     return window_request_focus( data->window );
}

static DFBResult IDirectFBWindow_GrabKeyboard( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     return window_grab_keyboard( data->window );
}

static DFBResult IDirectFBWindow_UngrabKeyboard( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     return window_ungrab_keyboard( data->window );
}

static DFBResult IDirectFBWindow_GrabPointer( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     return window_grab_pointer( data->window );
}

static DFBResult IDirectFBWindow_UngrabPointer( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     return window_ungrab_pointer( data->window );
}

static DFBResult IDirectFBWindow_Move( IDirectFBWindow *thiz, int dx, int dy )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (dx == 0  &&  dy == 0)
          return DFB_OK;

     return window_move( data->window, dx, dy );
}

static DFBResult IDirectFBWindow_MoveTo( IDirectFBWindow *thiz, int x, int y )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->window->x == x  &&  data->window->y == y)
          return DFB_OK;

     return window_move( data->window,
                         x - data->window->x, y - data->window->y );
}

static DFBResult IDirectFBWindow_Resize( IDirectFBWindow *thiz,
                                         unsigned int     width,
                                         unsigned int     height )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     if (data->window->width == width  &&  data->window->height == height)
          return DFB_OK;

     window_resize( data->window, width, height );

/*     if (data->surface) {
          IDirectFBSurface_data *surface_data =
                                    (IDirectFBSurface_data*)data->surface->priv;

          if (surface_data->state.clip.x2 <= data->window->surface->width) {
               surface_data->state.clip.x2 = data->window->surface->width - 1;
               surface_data->state.modified |= SMF_CLIP;
          }
          if (surface_data->state.clip.y2 <= data->window->surface->height) {
               surface_data->state.clip.y2 = data->window->surface->height - 1;
               surface_data->state.modified |= SMF_CLIP;
          }

          surface_data->req_rect.w = data->window->surface->width;
          surface_data->req_rect.h = data->window->surface->height;
          surface_data->clip_rect.w = data->window->surface->width;
          surface_data->clip_rect.h = data->window->surface->height;
     }*/

     return DFB_OK;
}

static DFBResult IDirectFBWindow_Raise( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     return window_raise( data->window );
}

static DFBResult IDirectFBWindow_Lower( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     return window_lower( data->window );
}

static DFBResult IDirectFBWindow_RaiseToTop( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     return window_raisetotop( data->window );
}

static DFBResult IDirectFBWindow_LowerToBottom( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     return window_lowertobottom( data->window );
}

static DFBResult IDirectFBWindow_WaitForEvent( IDirectFBWindow *thiz )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     pthread_mutex_lock( &data->events_mutex );

     if (!data->events)
          pthread_cond_wait( &data->wait_condition, &data->events_mutex );

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

static DFBResult IDirectFBWindow_WaitForEventWithTimeout(
                                                  IDirectFBWindow *thiz,
                                                  long int             seconds,
                                                  long int        nano_seconds )
{
     DFBResult ret = DFB_OK;

     INTERFACE_GET_DATA(IDirectFBWindow)


     pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          struct timeval  now;
          struct timespec timeout;

          gettimeofday( &now, NULL );

          timeout.tv_sec  = now.tv_sec + seconds;
          timeout.tv_nsec = (now.tv_usec * 1000) + nano_seconds;

          timeout.tv_sec  += timeout.tv_nsec / 1000000000;
          timeout.tv_nsec %= 1000000000;

          if (pthread_cond_timedwait( &data->wait_condition,
                                      &data->events_mutex,
                                      &timeout ) == ETIMEDOUT)
               ret = DFB_TIMEOUT;
     }

     pthread_mutex_unlock( &data->events_mutex );

     return ret;
}

static DFBResult IDirectFBWindow_GetEvent( IDirectFBWindow *thiz,
                                           DFBWindowEvent  *event )
{
     IDirectFBWindowBuffer_item *e;

     INTERFACE_GET_DATA(IDirectFBWindow)


     pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          pthread_mutex_unlock( &data->events_mutex );
          return DFB_BUFFEREMPTY;
     }
     e = data->events;

     *event = e->evt;

     data->events = e->next;
     free( e );

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

static DFBResult IDirectFBWindow_PeekEvent( IDirectFBWindow *thiz,
                                            DFBWindowEvent  *event )
{
     INTERFACE_GET_DATA(IDirectFBWindow)

     pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          pthread_mutex_unlock( &data->events_mutex );
          return DFB_BUFFEREMPTY;
     }

     *event = data->events->evt;

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

DFBResult IDirectFBWindow_Construct( IDirectFBWindow *thiz,
                                     CoreWindow      *window )
{
     IDirectFBWindow_data *data;

     DEBUGMSG( "IDirectFBWindow_Construct: window at %d %d, size %dx%d\n",
                window->x, window->y, window->width, window->height );

     data = (IDirectFBWindow_data*) calloc( 1, sizeof(IDirectFBWindow_data) );

     thiz->priv = data;

     data->ref = 1;
     data->window = window;

     pthread_mutex_init( &data->events_mutex, NULL );
     pthread_cond_init( &data->wait_condition, NULL );

     reactor_attach( data->window->reactor, IDirectFBWindow_React, data );

     window_init( data->window );

     thiz->AddRef = IDirectFBWindow_AddRef;
     thiz->Release = IDirectFBWindow_Release;
     thiz->GetPosition = IDirectFBWindow_GetPosition;
     thiz->GetSize = IDirectFBWindow_GetSize;
     thiz->GetSurface = IDirectFBWindow_GetSurface;
     thiz->SetOpacity = IDirectFBWindow_SetOpacity;
     thiz->GetOpacity = IDirectFBWindow_GetOpacity;
     thiz->RequestFocus = IDirectFBWindow_RequestFocus;
     thiz->GrabKeyboard = IDirectFBWindow_GrabKeyboard;
     thiz->UngrabKeyboard = IDirectFBWindow_UngrabKeyboard;
     thiz->GrabPointer = IDirectFBWindow_GrabPointer;
     thiz->UngrabPointer = IDirectFBWindow_UngrabPointer;
     thiz->Move = IDirectFBWindow_Move;
     thiz->MoveTo = IDirectFBWindow_MoveTo;
     thiz->Resize = IDirectFBWindow_Resize;
     thiz->Raise = IDirectFBWindow_Raise;
     thiz->Lower = IDirectFBWindow_Lower;
     thiz->RaiseToTop = IDirectFBWindow_RaiseToTop;
     thiz->LowerToBottom = IDirectFBWindow_LowerToBottom;
     thiz->WaitForEvent = IDirectFBWindow_WaitForEvent;
     thiz->WaitForEventWithTimeout = IDirectFBWindow_WaitForEventWithTimeout;
     thiz->GetEvent = IDirectFBWindow_GetEvent;
     thiz->PeekEvent = IDirectFBWindow_PeekEvent;

     return DFB_OK;
}



/* internals */

static ReactionResult IDirectFBWindow_React( const void *msg_data,
                                             void       *ctx )
{
     const DFBWindowEvent       *evt = (DFBWindowEvent*)msg_data;
     IDirectFBWindowBuffer_item *item;
     IDirectFBWindow_data       *data = (IDirectFBWindow_data*)ctx;

     item = (IDirectFBWindowBuffer_item*)
          calloc( 1, sizeof(IDirectFBWindowBuffer_item) );

     item->evt = *evt;

     pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          data->events = item;
     }
     else {
          IDirectFBWindowBuffer_item *e = data->events;

          while (e->next)
               e = e->next;

          e->next = item;
     }

     pthread_cond_broadcast( &data->wait_condition );

     pthread_mutex_unlock( &data->events_mutex );

     if (evt->type == DWET_CLOSE)
          return RS_REMOVE;

     return RS_OK;
}

