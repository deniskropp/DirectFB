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
     int       (*GetAbiVersion)  ();
     int       (*GetAvailable)   ();
     void      (*GetDriverInfo)  (InputDriverInfo  *driver_info);
     DFBResult (*OpenDevice)     (InputDevice      *device,
                                  unsigned int      number,
                                  InputDeviceInfo  *device_info,
                                  void            **driver_data);
     void      (*CloseDevice)    (void             *driver_data);
     int                         nr_devices;
} InputDriverModule;

typedef struct {
     unsigned int       id;            /* unique device id */

     InputDriverInfo    driver_info;
     InputDeviceInfo    device_info;

     FusionReactor     *reactor;       /* event dispatcher */
} InputDeviceShared;

struct _InputDevice {
     InputDeviceShared *shared;

     InputDriverModule *driver;
     void              *driver_data;

     InputDevice       *next;
};

typedef struct {
     int                num;
     InputDeviceShared *devices[MAX_INPUT_DEVICES];
} CoreInputField;

static CoreInputField *inputfield   = NULL;
static InputDevice    *inputdevices = NULL;


static CoreModuleLoadResult input_driver_handle_func( void *handle,
                                                      char *name,
                                                      void *ctx );


/** public **/

DFBResult input_initialize()
{
     char *driver_dir = MODULEDIR"/inputdrivers";

     inputfield = shcalloc( 1, sizeof (CoreInputField) );

#ifndef FUSION_FAKE
     arena_add_shared_field( dfb_core->arena, inputfield, "Core/Input" );
#endif

     core_load_modules( driver_dir, input_driver_handle_func, NULL );

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult input_join()
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

DFBResult input_shutdown()
{
     InputDevice *d = inputdevices;

     while (d) {
          InputDevice *next = d->next;

          d->driver->CloseDevice( d->driver_data );
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
DFBResult input_leave()
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

void input_enumerate_devices( InputDeviceCallback  callback,
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
input_attach( InputDevice *device, React react, void *ctx )
{
     reactor_attach( device->shared->reactor, react, ctx );
}

void
input_detach( InputDevice *device, React react, void *ctx )
{
     reactor_detach( device->shared->reactor, react, ctx );
}

void
input_dispatch( InputDevice *device, DFBInputEvent *event )
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
     
     reactor_dispatch( device->shared->reactor, event, true );
}

unsigned int input_device_id( const InputDevice *device )
{
     return device->shared->id;
}

DFBInputDeviceDescription input_device_description( const InputDevice *device )
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

static CoreModuleLoadResult input_driver_handle_func( void *handle,
                                                      char *name,
                                                      void *ctx )
{
     int n;
     InputDriverModule *driver = DFBCALLOC( 1, sizeof(InputDriverModule) );

     driver->GetAbiVersion = dlsym( handle, "driver_get_abi_version" );
     if (!driver->GetAbiVersion) {
          DLERRORMSG( "DirectFB/core/input: "
                      "Could not dlsym `driver_get_abi_version' from `%s'!\n", name );
          DFBFREE( driver );
          return MODULE_REJECTED;
     }

     driver->GetAvailable = dlsym( handle, "driver_get_available" );
     if (!driver->GetAvailable) {
          DLERRORMSG( "DirectFB/core/input: "
                      "Could not dlsym `driver_get_available' from `%s'!\n", name );
          DFBFREE( driver );
          return MODULE_REJECTED;
     }

     driver->GetDriverInfo = dlsym( handle, "driver_get_info" );
     if (!driver->GetDriverInfo) {
          DLERRORMSG( "DirectFB/core/input: "
                      "Could not dlsym `driver_get_info' from `%s'!\n", name );
          DFBFREE( driver );
          return MODULE_REJECTED;
     }

     driver->OpenDevice = dlsym( handle, "driver_open_device" );
     if (!driver->OpenDevice) {
          DLERRORMSG( "DirectFB/core/input: "
                      "Could not dlsym `driver_open_device' from `%s'!\n", name );
          DFBFREE( driver );
          return MODULE_REJECTED;
     }

     driver->CloseDevice = dlsym( handle, "driver_close_device" );
     if (!driver->CloseDevice) {
          DLERRORMSG( "DirectFB/core/input: "
                      "Could not dlsym `driver_close_device' from `%s'!\n", name );
          DFBFREE( driver );
          return MODULE_REJECTED;
     }

     if (driver->GetAbiVersion() != DFB_INPUT_DRIVER_ABI_VERSION) {
          ERRORMSG( "DirectFB/core/input: '%s' "
                    "was built for ABI version %d, but %d is required!\n", name,
                    driver->GetAbiVersion(), DFB_INPUT_DRIVER_ABI_VERSION );
          DFBFREE( driver );
          return MODULE_REJECTED;
     }

     driver->nr_devices = driver->GetAvailable();
     if (!driver->nr_devices) {
          DFBFREE( driver );
          return MODULE_REJECTED;
     }

     for (n=0; n<driver->nr_devices; n++) {
          InputDevice     *device;
          InputDeviceInfo  device_info;
          void            *driver_data;
          InputDriverInfo  driver_info;

          device = DFBCALLOC( 1, sizeof(InputDevice) );
          device->shared = shcalloc( 1, sizeof(InputDeviceShared) );

          memset( &device_info, 0, sizeof(InputDeviceInfo) );

          device->shared->reactor = reactor_new( sizeof(DFBInputEvent) );
          
          if (driver->OpenDevice( device, n, &device_info, &driver_data )) {
               reactor_free( device->shared->reactor );
               shmfree( device->shared );
               DFBFREE( device );
               continue;
          }

          memset( &driver_info, 0, sizeof(InputDriverInfo) );

          driver->GetDriverInfo( &driver_info );

          device->shared->id          = device_info.prefered_id;
          device->shared->driver_info = driver_info;
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
               INITMSG( "DirectFB/InputDevice: %s(%d) %d.%d (%s)\n",
                        device->shared->driver_info.name, n+1,
                        device->shared->driver_info.version.major,
                        device->shared->driver_info.version.minor,
                        device->shared->driver_info.vendor );
          }
          else {
               INITMSG( "DirectFB/InputDevice: %s %d.%d (%s)\n",
                        device->shared->driver_info.name,
                        device->shared->driver_info.version.major,
                        device->shared->driver_info.version.minor,
                        device->shared->driver_info.vendor );
          }

          /* add it to the list */
          input_add_device( device );
     }

     return MODULE_LOADED_CONTINUE;
}

