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

#include <pthread.h>

#include <core/fusion/shmalloc.h>
#include <core/fusion/reactor.h>
#include <core/fusion/arena.h>
#include <core/fusion/list.h>

#include "directfb.h"

#include "core.h"
#include "coredefs.h"
#include "coretypes.h"

#include "layers.h"
#include "input.h"

#include "misc/mem.h"
#include "misc/util.h"


#define MAX_INPUT_DEVICES 100

typedef struct {
     FusionLink         link;

     InputDriverFuncs  *funcs;

     InputDriverInfo    info;

     int                abi_version;
     int                nr_devices;
} InputDriver;

typedef struct {
     DFBInputDeviceID   id;            /* unique device id */

     InputDeviceInfo    device_info;

     FusionReactor     *reactor;       /* event dispatcher */
} InputDeviceShared;

struct _InputDevice {
     InputDeviceShared *shared;

     InputDriver       *driver;
     void              *driver_data;

     InputDevice       *next;
};

typedef struct {
     int                num;
     InputDeviceShared *devices[MAX_INPUT_DEVICES];
} CoreInputField;

static FusionLink     *input_drivers = NULL;

static CoreInputField *inputfield   = NULL;
static InputDevice    *inputdevices = NULL;


static void init_devices();

#ifdef DFB_DYNAMIC_LINKING
static CoreModuleLoadResult input_driver_handle_func( void *handle,
                                                      char *name,
                                                      void *ctx );
#endif


/** public **/

DFBResult dfb_input_initialize()
{
     inputfield = shcalloc( 1, sizeof (CoreInputField) );

#ifndef FUSION_FAKE
     arena_add_shared_field( dfb_core->arena, inputfield, "Core/Input" );
#endif

#ifdef DFB_DYNAMIC_LINKING
     dfb_core_load_modules( MODULEDIR"/inputdrivers",
                            input_driver_handle_func, NULL );
#endif

     init_devices();

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult dfb_input_join()
{
     int          i;
     FusionResult ret;

     ret = arena_get_shared_field( dfb_core->arena,
                                  (void**) &inputfield, "Core/Input" );
     if (ret) {
          printf("%d\n", ret);
          return DFB_INIT;
     }

     for (i=0; i<inputfield->num; i++) {
          InputDevice       *device;

          device = DFBCALLOC( 1, sizeof(InputDevice) );

          device->shared = inputfield->devices[i];

          /* add it to the list */
          if (!inputdevices) {
               inputdevices = device;
          }
          else {
               InputDevice *dev = inputdevices;

               while (dev->next)
                    dev = dev->next;

               dev->next = device;
          }
     }

     return DFB_OK;
}
#endif

DFBResult dfb_input_shutdown()
{
     InputDevice *d = inputdevices;

     while (d) {
          InputDevice *next = d->next;

          d->driver->funcs->CloseDevice( d->driver_data );
          d->driver->nr_devices--;

          reactor_free( d->shared->reactor );

          if (d->driver->nr_devices == 0)
               DFBFREE( d->driver );

          DFBFREE( d );

          d = next;
     }

     inputdevices = NULL;

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult dfb_input_leave()
{
     InputDevice *d = inputdevices;

     while (d) {
          InputDevice *next = d->next;

          DFBFREE( d );

          d = next;
     }

     inputdevices = NULL;

     return DFB_OK;
}
#endif

#ifdef FUSION_FAKE
DFBResult dfb_input_suspend()
{
     InputDevice *d = inputdevices;

     while (d) {
          d->driver->funcs->CloseDevice( d->driver_data );

          d = d->next;
     }

     return DFB_OK;
}

DFBResult dfb_input_resume()
{
     InputDevice *d = inputdevices;

     while (d) {
          int       i;
          DFBResult ret;

          for (i=0; i<d->driver->nr_devices; i++) {
               ret = d->driver->funcs->OpenDevice( d, i,
                                                   &d->shared->device_info,
                                                   &d->driver_data );
               if (ret)
                    return ret;
          }

          d = d->next;
     }

     return DFB_OK;
}
#endif

void dfb_input_register_module( InputDriverFuncs *funcs )
{
     InputDriver *driver;

     driver = DFBCALLOC( 1, sizeof(InputDriver) );

     driver->funcs       = funcs;
     driver->abi_version = funcs->GetAbiVersion();

     fusion_list_prepend( &input_drivers, &driver->link );
}

void dfb_input_enumerate_devices( InputDeviceCallback  callback,
                                  void                *ctx )
{
     InputDevice *d = inputdevices;

     while (d) {
          if (callback( d, ctx ) == DFENUM_CANCEL)
               break;

          d = d->next;
     }
}

void
dfb_input_attach( InputDevice *device, React react, void *ctx )
{
     reactor_attach( device->shared->reactor, react, ctx );
}

void
dfb_input_detach( InputDevice *device, React react, void *ctx )
{
     reactor_detach( device->shared->reactor, react, ctx );
}

void
dfb_input_dispatch( InputDevice *device, DFBInputEvent *event )
{
     switch (event->type) {
          case DIET_BUTTONPRESS:
          case DIET_BUTTONRELEASE:
               if (dfb_config->lefty) {
                    if (event->button == DIBI_LEFT)
                         event->button = DIBI_RIGHT;
                    else if (event->button == DIBI_RIGHT)
                         event->button = DIBI_LEFT;
               }

               break;

          default:
               ;
     }

     event->device_id = device->shared->id;

     reactor_dispatch( device->shared->reactor, event, true );
}

DFBInputDeviceID dfb_input_device_id( const InputDevice *device )
{
     return device->shared->id;
}

DFBInputDeviceDescription dfb_input_device_description( const InputDevice *device )
{
     return device->shared->device_info.desc;
}

/** internal **/

static void input_add_device( InputDevice *device )
{
     if (inputfield->num == MAX_INPUT_DEVICES) {
          ERRORMSG( "DirectFB/Core/Input: "
                    "Maximum number of devices reached!\n" );
          return;
     }

     if (!inputdevices) {
          inputdevices = device;
     }
     else {
          InputDevice *dev = inputdevices;

          while (dev->next)
               dev = dev->next;

          dev->next = device;
     }

     inputfield->devices[ inputfield->num++ ] = device->shared;
}

#ifdef DFB_DYNAMIC_LINKING
static CoreModuleLoadResult input_driver_handle_func( void *handle,
                                                      char *name,
                                                      void *ctx )
{
     InputDriver *driver = (InputDriver*) input_drivers;

     if (!driver)
          return MODULE_REJECTED;

     if (driver->abi_version != DFB_INPUT_DRIVER_ABI_VERSION) {
          ERRORMSG( "DirectFB/core/input: '%s' "
                    "was built for ABI version %d, but %d is required!\n", name,
                    driver->abi_version, DFB_INPUT_DRIVER_ABI_VERSION );

          fusion_list_remove( &input_drivers, input_drivers );

          DFBFREE( driver );
          
          return MODULE_REJECTED;
     }

     return MODULE_LOADED_CONTINUE;
}
#endif

static void init_devices()
{
     FusionLink *link;

     fusion_list_foreach( link, input_drivers ) {
          int          n;
          InputDriver *driver = (InputDriver*) link;

          driver->funcs->GetDriverInfo( &driver->info );

          driver->nr_devices = driver->funcs->GetAvailable();
          if (!driver->nr_devices)
               continue;

          for (n=0; n<driver->nr_devices; n++) {
               InputDevice     *device;
               InputDeviceInfo  device_info;
               void            *driver_data;

               device = DFBCALLOC( 1, sizeof(InputDevice) );
               device->shared = shcalloc( 1, sizeof(InputDeviceShared) );

               memset( &device_info, 0, sizeof(InputDeviceInfo) );

               device->shared->reactor = reactor_new( sizeof(DFBInputEvent) );

               if (driver->funcs->OpenDevice( device, n,
                                              &device_info, &driver_data )) {
                    reactor_free( device->shared->reactor );
                    shmfree( device->shared );
                    DFBFREE( device );
                    continue;
               }


               device->shared->id          = device_info.prefered_id;
               device->shared->device_info = device_info;

               device->driver       = driver;
               device->driver_data  = driver_data;

               /*  uniquify driver ID  */
               if (inputdevices) {
                    InputDevice *dev = inputdevices;

                    do {
                         if (dev->shared->id == device->shared->id) {
                              /* give it a new one beyond the last predefined id */
                              if (device->shared->id < DIDID_REMOTE)
                                   device->shared->id = DIDID_REMOTE;
                              device->shared->id++;
                              dev = inputdevices;
                              continue;
                         }
                    } while ((dev = dev->next) != NULL);
               }

               if (driver->nr_devices > 1) {
                    INITMSG( "DirectFB/InputDevice: %s (%d) %d.%d (%s)\n",
                             device_info.name, n+1,
                             driver->info.version.major,
                             driver->info.version.minor,
                             driver->info.vendor );
               }
               else {
                    INITMSG( "DirectFB/InputDevice: %s %d.%d (%s)\n",
                             device_info.name,
                             driver->info.version.major,
                             driver->info.version.minor,
                             driver->info.vendor );
               }

               /* add it to the list */
               input_add_device( device );
          }
     }
}
