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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <malloc.h>
#include <errno.h>

#include <sys/time.h>

#include <pthread.h>

#include <core/fusion/reactor.h>
#include <core/fusion/list.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/windows.h>

#include "misc/util.h"
#include "misc/mem.h"

#include "idirectfbinputbuffer.h"

typedef struct _EventBufferItem
{
     DFBEvent                 evt;
     struct _EventBufferItem *next;
} IDirectFBEventBuffer_item;

typedef struct {
     FusionLink   link;

     InputDevice *device;       /* pointer to input core device struct */
} AttachedDevice;

typedef struct {
     FusionLink   link;

     CoreWindow  *window;       /* pointer to core window struct */
} AttachedWindow;

/*
 * private data struct of IDirectFBInputDevice
 */
typedef struct {
     int                           ref;           /* reference counter */

     FusionLink                   *devices;       /* attached devices */

     FusionLink                   *windows;       /* attached windows */

     IDirectFBEventBuffer_item    *events;        /* linked list containing
                                                     events */

     pthread_mutex_t               events_mutex;  /* mutex lock for accessing
                                                     the event queue */

     pthread_cond_t                wait_condition;/* condition used for idle
                                                     wait in WaitForEvent() */
} IDirectFBEventBuffer_data;

/*
 * adds an event to the event queue
 */
static void IDirectFBEventBuffer_AddItem( IDirectFBEventBuffer_data *data,
                                          IDirectFBEventBuffer_item *item );

static ReactionResult IDirectFBEventBuffer_InputReact( const void *msg_data,
                                                       void       *ctx );

static ReactionResult IDirectFBEventBuffer_WindowReact( const void *msg_data,
                                                        void       *ctx );



static void IDirectFBEventBuffer_Destruct( IDirectFBEventBuffer *thiz )
{
     IDirectFBEventBuffer_data *data = (IDirectFBEventBuffer_data*)thiz->priv;

     while (data->devices) {
          AttachedDevice *device = (AttachedDevice*) data->devices;

          dfb_input_detach( device->device, IDirectFBEventBuffer_InputReact, data );
          fusion_list_remove( &data->devices, data->devices );
          DFBFREE( device );
     }

     while (data->windows) {
          AttachedWindow *window = (AttachedWindow*) data->windows;

          dfb_window_detach( window->window, IDirectFBEventBuffer_WindowReact, data );
          fusion_list_remove( &data->windows, data->windows );
          DFBFREE( window );
     }

     while (data->events) {
          IDirectFBEventBuffer_item *next = data->events->next;

          DFBFREE( data->events );

          data->events = next;
     }

     pthread_cond_destroy( &data->wait_condition );
     pthread_mutex_destroy( &data->events_mutex );

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult IDirectFBEventBuffer_AddRef( IDirectFBEventBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBEventBuffer)

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBEventBuffer_Release( IDirectFBEventBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBEventBuffer)

     if (--data->ref == 0)
          IDirectFBEventBuffer_Destruct( thiz );

     return DFB_OK;
}

static DFBResult IDirectFBEventBuffer_Reset( IDirectFBEventBuffer *thiz )
{
     IDirectFBEventBuffer_item     *e;

     INTERFACE_GET_DATA(IDirectFBEventBuffer)


     pthread_mutex_lock( &data->events_mutex );

     e = data->events;
     while (e) {
          IDirectFBEventBuffer_item *next = e->next;
          DFBFREE( e );
          e = next;
     }
     data->events = NULL;

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

static DFBResult IDirectFBEventBuffer_WaitForEvent( IDirectFBEventBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBEventBuffer)

     pthread_mutex_lock( &data->events_mutex );

     if (!data->events)
          pthread_cond_wait( &data->wait_condition, &data->events_mutex );

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

static DFBResult IDirectFBEventBuffer_WaitForEventWithTimeout(
                                                  IDirectFBEventBuffer *thiz,
                                                  unsigned int          seconds,
                                                  unsigned int          milli_seconds )
{
     struct timeval  now;
     struct timespec timeout;
     DFBResult       ret    = DFB_OK;
     int             locked = 0;
     long int        nano_seconds = milli_seconds * 1000000;

     INTERFACE_GET_DATA(IDirectFBEventBuffer)

     if (pthread_mutex_trylock( &data->events_mutex ) == 0) {
          if (data->events) {
               pthread_mutex_unlock ( &data->events_mutex );
               return ret;
          }
          locked = 1;
     }

     gettimeofday( &now, NULL );

     timeout.tv_sec  = now.tv_sec + seconds;
     timeout.tv_nsec = (now.tv_usec * 1000) + nano_seconds;

     timeout.tv_sec  += timeout.tv_nsec / 1000000000;
     timeout.tv_nsec %= 1000000000;

     if (!locked)
          pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          if (pthread_cond_timedwait( &data->wait_condition,
                                      &data->events_mutex,
                                      &timeout ) == ETIMEDOUT)
               ret = DFB_TIMEOUT;
     }

     pthread_mutex_unlock( &data->events_mutex );

     return ret;
}

static DFBResult IDirectFBEventBuffer_GetEvent( IDirectFBEventBuffer *thiz,
                                                DFBEvent             *event )
{
     IDirectFBEventBuffer_item     *e;

     INTERFACE_GET_DATA(IDirectFBEventBuffer)

     pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          pthread_mutex_unlock( &data->events_mutex );
          return DFB_BUFFEREMPTY;
     }

     e = data->events;

     switch (e->evt.clazz) {
          case DFEC_INPUT:
               event->input = e->evt.input;
               break;

          case DFEC_WINDOW:
               event->window = e->evt.window;
               break;

          case DFEC_USER:
               event->user = e->evt.user;
               break;

          default:
               BUG("unknown event class");
     }

     data->events = e->next;
     DFBFREE( e );

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

static DFBResult IDirectFBEventBuffer_PeekEvent( IDirectFBEventBuffer *thiz,
                                                 DFBEvent             *event )
{
     INTERFACE_GET_DATA(IDirectFBEventBuffer)

     pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          pthread_mutex_unlock( &data->events_mutex );
          return DFB_BUFFEREMPTY;
     }

     switch (data->events->evt.clazz) {
          case DFEC_INPUT:
               event->input = data->events->evt.input;
               break;

          case DFEC_WINDOW:
               event->window = data->events->evt.window;
               break;

          case DFEC_USER:
               event->user = data->events->evt.user;
               break;

          default:
               BUG("unknown event class");
     }

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

static DFBResult IDirectFBEventBuffer_HasEvent( IDirectFBEventBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBEventBuffer)

     return (data->events ? DFB_OK : DFB_BUFFEREMPTY);
}

static DFBResult IDirectFBEventBuffer_PostEvent( IDirectFBEventBuffer *thiz,
                                                 DFBEvent             *event )
{
     IDirectFBEventBuffer_item *item;
     
     INTERFACE_GET_DATA(IDirectFBEventBuffer)

     item = (IDirectFBEventBuffer_item*)
          DFBCALLOC( 1, sizeof(IDirectFBEventBuffer_item) );

     item->evt = *event;

     IDirectFBEventBuffer_AddItem( data, item );
     
     return DFB_OK;
}

DFBResult IDirectFBEventBuffer_Construct( IDirectFBEventBuffer *thiz )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBEventBuffer)

     data->ref = 1;

     pthread_mutex_init( &data->events_mutex, NULL );
     pthread_cond_init( &data->wait_condition, NULL );

     thiz->AddRef = IDirectFBEventBuffer_AddRef;
     thiz->Release = IDirectFBEventBuffer_Release;
     thiz->Reset = IDirectFBEventBuffer_Reset;
     thiz->WaitForEvent = IDirectFBEventBuffer_WaitForEvent;
     thiz->WaitForEventWithTimeout =
          IDirectFBEventBuffer_WaitForEventWithTimeout;
     thiz->GetEvent = IDirectFBEventBuffer_GetEvent;
     thiz->PeekEvent = IDirectFBEventBuffer_PeekEvent;
     thiz->HasEvent = IDirectFBEventBuffer_HasEvent;
     thiz->PostEvent = IDirectFBEventBuffer_PostEvent;

     return DFB_OK;
}

/* directfb internals */

DFBResult IDirectFBEventBuffer_AttachInputDevice( IDirectFBEventBuffer *thiz,
                                                  InputDevice          *device )
{
     AttachedDevice *attached;

     INTERFACE_GET_DATA(IDirectFBEventBuffer)

     attached = DFBCALLOC( 1, sizeof(AttachedDevice) );
     attached->device = device;

     dfb_input_attach( device, IDirectFBEventBuffer_InputReact, data );

     fusion_list_prepend( &data->devices, &attached->link );

     return DFB_OK;
}

DFBResult IDirectFBEventBuffer_AttachWindow( IDirectFBEventBuffer *thiz,
                                             CoreWindow           *window )
{
     AttachedWindow *attached;

     INTERFACE_GET_DATA(IDirectFBEventBuffer)

     attached = DFBCALLOC( 1, sizeof(AttachedWindow) );
     attached->window = window;

     dfb_window_attach( window, IDirectFBEventBuffer_WindowReact, data );

     fusion_list_prepend( &data->windows, &attached->link );

     return DFB_OK;
}

/* file internals */

static void IDirectFBEventBuffer_AddItem( IDirectFBEventBuffer_data *data,
                                          IDirectFBEventBuffer_item *item )
{
     pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          data->events = item;
     }
     else {
          IDirectFBEventBuffer_item *e = data->events;

          while (e->next)
               e = e->next;

          e->next = item;
     }

     pthread_cond_broadcast( &data->wait_condition );

     pthread_mutex_unlock( &data->events_mutex );
}

static ReactionResult IDirectFBEventBuffer_InputReact( const void *msg_data,
                                                       void       *ctx )
{
     const DFBInputEvent       *evt = (DFBInputEvent*)msg_data;
     IDirectFBEventBuffer_item *item;
     IDirectFBEventBuffer_data *data = (IDirectFBEventBuffer_data*)ctx;

     item = (IDirectFBEventBuffer_item*)
          DFBCALLOC( 1, sizeof(IDirectFBEventBuffer_item) );

     item->evt.input = *evt;
     item->evt.clazz = DFEC_INPUT;

     IDirectFBEventBuffer_AddItem( data, item );

     return RS_OK;
}

static ReactionResult IDirectFBEventBuffer_WindowReact( const void *msg_data,
                                                        void       *ctx )
{
     const DFBWindowEvent      *evt = (DFBWindowEvent*)msg_data;
     IDirectFBEventBuffer_item *item;
     IDirectFBEventBuffer_data *data = (IDirectFBEventBuffer_data*)ctx;

     item = (IDirectFBEventBuffer_item*)
          DFBCALLOC( 1, sizeof(IDirectFBEventBuffer_item) );

     item->evt.window = *evt;
     item->evt.clazz  = DFEC_WINDOW;

     IDirectFBEventBuffer_AddItem( data, item );

     if (evt->type == DWET_DESTROYED) {
          AttachedWindow *window = (AttachedWindow*) data->windows;

          while (window) {
               if (window->window->id == evt->window_id) {
                    fusion_list_remove( &data->windows, &window->link );
                    DFBFREE( window );
                    break;
               }

               window = (AttachedWindow*) window->link.next;
          }

          return RS_REMOVE;
     }

     return RS_OK;
}

