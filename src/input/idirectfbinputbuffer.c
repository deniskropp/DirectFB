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

#include <core/coredefs.h>
#include <core/input.h>

#include "idirectfbinputbuffer.h"

/* 
 * adds an event to the event queue (function is added to the event listeners)
 */
static void IDirectFBInputBuffer_React( const void *msg_data, void *ctx );


typedef struct _InputBufferItem
{
     DFBInputEvent       evt;
     struct _InputBufferItem  *next;
} IDirectFBInputBuffer_item;

/*
 * private data struct of IDirectFBInputDevice
 */
typedef struct {
     int                           ref;           /* reference counter */
     
     InputDevice                   *device;       /* pointer to input core
                                                     device struct */
                                                     
     IDirectFBInputBuffer_item     *events;       /* linked list containing 
                                                     events */
                                                                                                      
     pthread_mutex_t               events_mutex;  /* mutex lock for accessing 
                                                     the event queue */
                                                     
     pthread_mutex_t               wait;          /* mutex lock for idle waits
                              	                     used by WaitForEvent() */
} IDirectFBInputBuffer_data;



static void IDirectFBInputBuffer_Destruct( IDirectFBInputBuffer *thiz )
{
     IDirectFBInputBuffer_data *data = (IDirectFBInputBuffer_data*)thiz->priv;

     reactor_detach( data->device->reactor, IDirectFBInputBuffer_React, data );
     
     free( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

static DFBResult IDirectFBInputBuffer_AddRef( IDirectFBInputBuffer *thiz )
{
     IDirectFBInputBuffer_data *data = (IDirectFBInputBuffer_data*)thiz->priv;
     
     if (!data)
          return DFB_DEAD;
     
     data->ref++;
     
     return DFB_OK;
}

static DFBResult IDirectFBInputBuffer_Release( IDirectFBInputBuffer *thiz )
{
     IDirectFBInputBuffer_data *data = (IDirectFBInputBuffer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;
     
     if (--data->ref == 0) {
          IDirectFBInputBuffer_Destruct( thiz );
     }
     
     return DFB_OK;
}

static DFBResult IDirectFBInputBuffer_Reset( IDirectFBInputBuffer *thiz )
{
     IDirectFBInputBuffer_item     *e;
     IDirectFBInputBuffer_data     *data = 
                                        (IDirectFBInputBuffer_data*)thiz->priv;
     
     if (!data)
          return DFB_DEAD;

     pthread_mutex_lock( &data->events_mutex );

     e = data->events;
     while (e) {
          IDirectFBInputBuffer_item *next = e->next;
          free( e );
          e = next;
     }
     data->events = NULL;
     
     pthread_mutex_unlock( &data->events_mutex );

     return DFB_OK;
}

static DFBResult IDirectFBInputBuffer_WaitForEvent( IDirectFBInputBuffer *thiz )
{
     IDirectFBInputBuffer_data *data = (IDirectFBInputBuffer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->events)
          return DFB_OK;

     /* TODO: this gap can still cause a deadlock */

     pthread_mutex_lock( &data->wait );

     pthread_mutex_lock( &data->wait );
     pthread_mutex_unlock( &data->wait );
     
     return DFB_OK;
}

static DFBResult IDirectFBInputBuffer_GetEvent( IDirectFBInputBuffer *thiz, 
                                                DFBInputEvent *event )
{
     IDirectFBInputBuffer_item     *e;
     IDirectFBInputBuffer_data     *data 
                                      = (IDirectFBInputBuffer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!data->events)
          return DFB_BUFFEREMPTY;
          
     pthread_mutex_lock( &data->events_mutex );

     e = data->events;
     
     *event = e->evt;
     
     data->events = e->next;
     free( e );
     
     pthread_mutex_unlock( &data->events_mutex );
     
     return DFB_OK;
}

static DFBResult IDirectFBInputBuffer_PeekEvent( IDirectFBInputBuffer *thiz,
                                                 DFBInputEvent *event )
{
     IDirectFBInputBuffer_data *data = (IDirectFBInputBuffer_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;
     
     if (!data->events)
          return DFB_BUFFEREMPTY;
          
     *event = data->events->evt;
     
     return DFB_OK;
}

DFBResult IDirectFBInputBuffer_Construct( IDirectFBInputBuffer *thiz,
                                          InputDevice *device )
{
     IDirectFBInputBuffer_data *data;

     data = (IDirectFBInputBuffer_data*)
         calloc( 1, sizeof(IDirectFBInputBuffer_data) );

     thiz->priv = data;
     
     data->ref = 1;
     
     data->device = device;
     
     pthread_mutex_init( &data->events_mutex, NULL );
     pthread_mutex_init( &data->wait, NULL );
     
     reactor_attach( data->device->reactor, IDirectFBInputBuffer_React, data );

     thiz->AddRef = IDirectFBInputBuffer_AddRef;
     thiz->Release = IDirectFBInputBuffer_Release;
     thiz->Reset = IDirectFBInputBuffer_Reset;
     thiz->WaitForEvent = IDirectFBInputBuffer_WaitForEvent;
     thiz->GetEvent = IDirectFBInputBuffer_GetEvent;
     thiz->PeekEvent = IDirectFBInputBuffer_PeekEvent;
     
     return DFB_OK;
}


/* internals */

static void IDirectFBInputBuffer_React( const void *msg_data, void *ctx )
{
     const DFBInputEvent       *evt = (DFBInputEvent*)msg_data;
     IDirectFBInputBuffer_item *item;
     IDirectFBInputBuffer_data *data = (IDirectFBInputBuffer_data*)ctx;
     
     item = (IDirectFBInputBuffer_item*)
          calloc( 1, sizeof(IDirectFBInputBuffer_item) );

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

     pthread_mutex_unlock( &data->events_mutex );

     pthread_mutex_unlock( &data->wait );
}
