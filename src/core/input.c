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
#include <dlfcn.h>

#include <string.h>
#include <malloc.h>

#include <directfb.h>

#include "core.h"
#include "coredefs.h"
#include "input.h"

#include "layers.h"


InputDevice *inputdevices = NULL;


static CoreModuleLoadResult input_driver_handle_func( void *handle,
                                                      char *name,
                                                      void *ctx )
{
     int n, nr_devices;
     InputDriver *driver = malloc( sizeof(InputDriver) );
     
     driver->Probe  = dlsym( handle, "driver_probe" );
     if (!driver->Probe) {
          DLERRORMSG( "DirectFB/core/input: "
                      "Could not dlsym `driver_probe' from `%s'!\n", name );
          free( driver );
          return MODULE_REJECTED;
     }

     driver->Init   = dlsym( handle, "driver_init" );
     if (!driver->Init) {
          DLERRORMSG( "DirectFB/core/input: "
                      "Could not dlsym `driver_init' from `%s'!\n", name );
          free( driver );
          return MODULE_REJECTED;
     }

     driver->DeInit = dlsym( handle, "driver_deinit" );
     if (!driver->DeInit) {
          DLERRORMSG( "DirectFB/core/input: "
                      "Could not dlsym `driver_deinit' from `%s'!\n", name );
          free( driver );
          return MODULE_REJECTED;
     }

     
     nr_devices = driver->Probe();
     if (!nr_devices) {
          free( driver );
          return MODULE_REJECTED;
     }

     for (n=0; n<nr_devices; n++) {
          InputDevice *device;

          device = (InputDevice*) calloc( 1, sizeof(InputDevice) );

          device->number = n;

          if (driver->Init( device ) != DFB_OK) {
               free( device );
               continue;
          }

          device->info.driver = driver;

          INITMSG( "DirectFB/InputDevice: %s %d.%d (%s)\n",
                   device->info.driver_name,
                   device->info.driver_version.major,
                   device->info.driver_version.minor,
                   device->info.driver_vendor );

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

     return MODULE_LOADED_CONTINUE;
}

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

          d->info.driver->DeInit( d );

          free( d );

          d = next;
     }

     inputdevices = NULL;
}

DFBResult input_init_devices()
{
     char *driver_dir = LIBDIR"/inputdrivers";

     core_load_modules( driver_dir, input_driver_handle_func, NULL );
     
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

     listener = (InputDeviceListener*) 
          calloc( 1, sizeof(InputDeviceListener) );

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

