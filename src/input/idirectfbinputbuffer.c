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

#include "misc/util.h"
#include "misc/mem.h"

#include "idirectfbinputbuffer.h"

/*
 * adds an event to the event queue (function is added to the event listeners)
 */
static ReactionResult IDirectFBInputBuffer_React( const void *msg_data,
                                                  void       *ctx );


typedef struct _InputBufferItem
{
     DFBInputEvent            evt;
     struct _InputBufferItem *next;
} IDirectFBInputBuffer_item;

typedef struct {
     FusionLink   link;

     InputDevice *device;       /* pointer to input core device struct */
} AttachedDevice;

/*
 * private data struct of IDirectFBInputDevice
 */
typedef struct {
     int                           ref;           /* reference counter */

     FusionLink                   *devices;       /* attached devices */

     IDirectFBInputBuffer_item    *events;        /* linked list containing
                                                     events */

     pthread_mutex_t               events_mutex;  /* mutex lock for accessing
                                                     the event queue */

     pthread_cond_t                wait_condition;/* condition used for idle
                                                     wait in WaitForEvent() */
} IDirectFBInputBuffer_data;



static void IDirectFBInputBuffer_Destruct( IDirectFBInputBuffer *thiz )
{
     IDirectFBInputBuffer_data *data = (IDirectFBInputBuffer_data*)thiz->priv;

     while (data->devices) {
          AttachedDevice *device = (AttachedDevice*) data->devices;

          input_detach( device->device, IDirectFBInputBuffer_React, data );
          fusion_list_remove( &data->devices, data->devices );
          DFBFREE( device );
     }

     pthread_cond_destroy( &data->wait_condition );
     pthread_mutex_destroy( &data->events_mutex );

     DFBFREE( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     DFBFREE( thiz );
#endif
}

static DFBResult IDirectFBInputBuffer_AddRef( IDirectFBInputBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBInputBuffer)

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBInputBuffer_Release( IDirectFBInputBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBInputBuffer)

     if (--data->ref == 0)
          IDirectFBInputBuffer_Destruct( thiz );

     return DFB_OK;
}

static DFBResult IDirectFBInputBuffer_Reset( IDirectFBInputBuffer *thiz )
{
     IDirectFBInputBuffer_item     *e;

     INTERFACE_GET_DATA(IDirectFBInputBuffer)


     pthread_mutex_lock( &data->events_mutex );

     e = data->events;
     while (e) {
          IDirectFBInputBuffer_item *next = e->next;
          DFBFREE( e );
          e = next;
     }
     data->events = NULL;

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

static DFBResult IDirectFBInputBuffer_WaitForEvent( IDirectFBInputBuffer *thiz )
{
     INTERFACE_GET_DATA(IDirectFBInputBuffer)

     pthread_mutex_lock( &data->events_mutex );

     if (!data->events)
          pthread_cond_wait( &data->wait_condition, &data->events_mutex );

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

static DFBResult IDirectFBInputBuffer_WaitForEventWithTimeout(
                                                  IDirectFBInputBuffer *thiz,
                                                  long int             seconds,
                                                  long int        nano_seconds )
{
     struct timeval  now;
     struct timespec timeout;
     DFBResult       ret    = DFB_OK;
     int             locked = 0;

     INTERFACE_GET_DATA(IDirectFBInputBuffer)

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

static DFBResult IDirectFBInputBuffer_GetEvent( IDirectFBInputBuffer *thiz,
                                                DFBInputEvent *event )
{
     IDirectFBInputBuffer_item     *e;

     INTERFACE_GET_DATA(IDirectFBInputBuffer)

     pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          pthread_mutex_unlock( &data->events_mutex );
          return DFB_BUFFEREMPTY;
     }

     e = data->events;

     *event = e->evt;

     data->events = e->next;
     DFBFREE( e );

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

static DFBResult IDirectFBInputBuffer_PeekEvent( IDirectFBInputBuffer *thiz,
                                                 DFBInputEvent *event )
{
     INTERFACE_GET_DATA(IDirectFBInputBuffer)

     pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          pthread_mutex_unlock( &data->events_mutex );
          return DFB_BUFFEREMPTY;
     }

     *event = data->events->evt;

     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

DFBResult IDirectFBInputBuffer_Construct( IDirectFBInputBuffer *thiz,
                                          InputDevice          *device )
{
     IDirectFBInputBuffer_data *data;

     if (!thiz->priv)
          thiz->priv = DFBCALLOC( 1, sizeof(IDirectFBInputBuffer_data) );

     data = (IDirectFBInputBuffer_data*)(thiz->priv);

     data->ref = 1;

     pthread_mutex_init( &data->events_mutex, NULL );
     pthread_cond_init( &data->wait_condition, NULL );

     IDirectFBInputBuffer_Attach( thiz, device );

     thiz->AddRef = IDirectFBInputBuffer_AddRef;
     thiz->Release = IDirectFBInputBuffer_Release;
     thiz->Reset = IDirectFBInputBuffer_Reset;
     thiz->WaitForEvent = IDirectFBInputBuffer_WaitForEvent;
     thiz->WaitForEventWithTimeout =
          IDirectFBInputBuffer_WaitForEventWithTimeout;
     thiz->GetEvent = IDirectFBInputBuffer_GetEvent;
     thiz->PeekEvent = IDirectFBInputBuffer_PeekEvent;

     return DFB_OK;
}

DFBResult IDirectFBInputBuffer_Attach( IDirectFBInputBuffer *thiz,
                                       InputDevice          *device )
{
     AttachedDevice *attached;

     INTERFACE_GET_DATA(IDirectFBInputBuffer)

     attached = DFBCALLOC( 1, sizeof(AttachedDevice) );
     attached->device = device;

     input_attach( device, IDirectFBInputBuffer_React, data );

     fusion_list_prepend( &data->devices, &attached->link );

     return DFB_OK;
}

/* internals */

static ReactionResult IDirectFBInputBuffer_React( const void *msg_data,
                                                  void       *ctx )
{
     const DFBInputEvent       *evt = (DFBInputEvent*)msg_data;
     IDirectFBInputBuffer_item *item;
     IDirectFBInputBuffer_data *data = (IDirectFBInputBuffer_data*)ctx;

     item = (IDirectFBInputBuffer_item*)
          DFBCALLOC( 1, sizeof(IDirectFBInputBuffer_item) );

     item->evt = *evt;

     pthread_mutex_lock( &data->events_mutex );

     if (!data->events) {
          data->events = item;
     }
     else {
          IDirectFBInputBuffer_item *e = data->events;

          while (e->next)
               e = e->next;

          e->next = item;
     }

     pthread_cond_broadcast( &data->wait_condition );

     pthread_mutex_unlock( &data->events_mutex );

     return RS_OK;
}
