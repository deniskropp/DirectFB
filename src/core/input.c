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

#include <malloc.h>

#include <directfb.h>

#include "core.h"
#include "coredefs.h"
#include "input.h"

#include "inputdevices/keyboard.h"
#include "inputdevices/joystick.h"
#include "inputdevices/ps2mouse.h"

#include "layers.h"

static struct       /* TODO: port to dynamic driver modules */
{
     int  (*Probe)();
     int  (*Init)(InputDevice *device);
     void (*DeInit)(InputDevice *device);
} drivers[] = {
     { keyboard_probe, keyboard_init, keyboard_deinit },
     { ps2mouse_probe, ps2mouse_init, ps2mouse_deinit },
     { joystick_probe, joystick_init, joystick_deinit },
     { NULL, NULL, NULL }
};

InputDevice *inputdevices = NULL;


/*
 * cancels input threads, deinitializes drivers, deallocates device structs
 */
void input_deinit()
{
     InputDevice *d = inputdevices;

     while (d) {
          InputDevice *next = d->next;

          pthread_mutex_lock( &d->listeners_mutex );
          while (d->listeners) {
               InputDeviceListener *l = d->listeners;
               d->listeners = l->next;
               free( l );
          }
          pthread_mutex_unlock( &d->listeners_mutex );
          
          pthread_cancel( d->event_thread );
          pthread_join( d->event_thread, NULL );
          d->deinit( d );
          free( d );

          d = next;
     }

     inputdevices = NULL;
}

DFBResult input_init_devices()
{
     int i;

     for (i=0; drivers[i].Probe; i++) {
          int n, nr_devices = drivers[i].Probe();

          for (n=0; n<nr_devices; n++) {
               InputDevice *device;

               device = (InputDevice*)malloc( sizeof(InputDevice) );
               memset( device, 0, sizeof(InputDevice) );

               device->number = n;

               if (drivers[i].Init( device ) != DFB_OK) {
                    free( device );
                    continue;
               }

               /* remember (de)init function */
               device->init = drivers[i].Init;
               device->deinit = drivers[i].DeInit;

               INITMSG( "DirectFB/InputDevice: %s %d.%d (%s)\n",
                        device->driver.name, device->driver.version.major,
                        device->driver.version.minor, device->driver.vendor );

               /* start input thread */
               if (device->EventThread) {
#ifdef KRANK
                    int                 policy;
                    struct sched_param  sp;
#endif

                    pthread_create( &device->event_thread, NULL,
                                    device->EventThread, device );

#ifdef KRANK
                    pthread_getschedparam( device->event_thread, &policy, &sp );

                    policy = SCHED_FIFO;

                    sp.sched_priority = sched_get_priority_max( policy );

                    pthread_setschedparam( device->event_thread, policy, &sp );
#endif
               }

               /* add it to the list */
               if (!inputdevices) {
                    inputdevices = device;
               }
               else {
                    InputDevice *dev = inputdevices;

                    while (dev->next) {
                         dev = dev->next;
                    }
                    dev->next = device;
               }
          }
     }

     core_cleanup_push( input_deinit );

     return DFB_OK;
}

DFBResult input_suspend()
{
     InputDevice *d = inputdevices;

     DEBUGMSG( "DirectFB/core/input: suspending...\n" );

     while (d) {
          pthread_cancel( d->event_thread );
          pthread_join( d->event_thread, NULL );
//          d->deinit( d );
          
          d = d->next;
     }

     DEBUGMSG( "DirectFB/core/input: ...suspended\n" );
     
     return DFB_OK;
}

DFBResult input_resume()
{
     InputDevice *d = inputdevices;

     DEBUGMSG( "DirectFB/core/input: resuming...\n" );
     
     while (d) {
//          d->init( d );
          pthread_create( &d->event_thread, NULL,
                           d->EventThread, d );
          
          d = d->next;
     }

     DEBUGMSG( "DirectFB/core/input: ...resumed\n" );
     
     return DFB_OK;
}

DFBResult input_add_listener( InputDevice *device,
                               InputDeviceNotify notify, void *ctx )
{
     InputDeviceListener *listener;

     listener = (InputDeviceListener*)malloc( sizeof(InputDeviceListener) );
     memset( listener, 0, sizeof(InputDeviceListener) );

     listener->notify = notify;
     listener->ctx = ctx;     

     pthread_mutex_lock( &device->listeners_mutex );

     if (!device->listeners) {
          device->listeners = listener;
     }
     else {
          InputDeviceListener *l = device->listeners;

          while (l->next)
               l = l->next;

          l->next = listener;
     }

     pthread_mutex_unlock( &device->listeners_mutex );

     return DFB_OK;
}

DFBResult input_remove_listener( InputDevice *device, void *ctx )
{
     pthread_mutex_lock( &device->listeners_mutex );

     if (device->listeners) {
          InputDeviceListener *prev = NULL;
          InputDeviceListener *l = device->listeners;

          while (l) {
               if (l->ctx == ctx) {
                    if (prev)
                         prev->next = l->next;
                    else
                         device->listeners = NULL;

                    free( l );
                    pthread_mutex_unlock( &device->listeners_mutex );
                    return DFB_OK;
               }
               prev = l;
               l = l->next;
          }
     }

     pthread_mutex_unlock( &device->listeners_mutex );

     return DFB_BUG;
}

void input_dispatch( InputDevice *device, DFBInputEvent event )
{
     pthread_mutex_lock( &device->listeners_mutex );
     {
          InputDeviceListener *l = device->listeners;

          while (l) {
               l->notify( &event, l->ctx );
               l = l->next;
          }
     }
     pthread_mutex_unlock( &device->listeners_mutex );
}

