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
static void fixup_key_event( DFBInputEvent *event );

static DFBInputDeviceKeyIdentifier
symbol_to_id( DFBInputDeviceKeySymbol symbol );

static DFBInputDeviceKeySymbol
id_to_symbol( DFBInputDeviceKeyIdentifier id,
              DFBInputDeviceModifierMask  modifiers );

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

DFBResult dfb_input_shutdown( bool emergency )
{
     InputDevice *d = inputdevices;

     while (d) {
          InputDevice *next = d->next;

          d->driver->funcs->CloseDevice( d->driver_data );
          d->driver->nr_devices--;

          reactor_free( d->shared->reactor );

          DFBFREE( d );

          d = next;
     }

     inputdevices = NULL;

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult dfb_input_leave( bool emergency )
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

          case DIET_KEYPRESS:
          case DIET_KEYRELEASE:
               fixup_key_event( event );
               break;
          
          default:
               ;
     }

     event->clazz     = DFEC_INPUT;
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
                    shfree( device->shared );
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


#define FIXUP_FIELDS     (DIEF_MODIFIERS | DIEF_LOCKS | \
                          DIEF_KEYCODE | DIEF_KEYID | DIEF_KEYSYMBOL)

/*
 * Fill partially missing values for key_code, key_id and key_symbol by
 * translating those that are set. Fix modifiers/locks before if not set.
 *
 *
 * There are five valid constellations that give reasonable values.
 * (not counting the constellation where everything is set)
 *
 * Device has no translation table
 *   1. key_id is set, key_symbol not
 *      -> key_code defaults to -1, key_symbol from key_id (up-translation)
 *   2. key_symbol is set, key_id not
 *      -> key_code defaults to -1, key_id from key_symbol (down-translation)
 *
 * Device has a translation table
 *   3. key_code is set
 *      -> look up key_id and/or key_symbol (key_code being the index)
 *   4. key_id is set
 *      -> look up key_code and possibly key_symbol (key_id being searched for)
 *   5. key_symbol is set
 *      -> look up key_code and key_id (key_symbol being searched for)
 *
 * Fields remaining will be set to the default, e.g. key_code to -1.
 */
static void
fixup_key_event( DFBInputEvent *event )
{
     DFBInputEventFlags valid   = event->flags & FIXUP_FIELDS;
     DFBInputEventFlags missing = valid ^ FIXUP_FIELDS;

     /* None is missing? */
     if (missing == DIEF_NONE)
          return;

     /* Add missing flags */
     event->flags |= missing;
     
     /*
      * Use computed values for modifiers and locks if they are missing.
      */
     if (missing & DIEF_MODIFIERS) {
          event->modifiers = 0; /* TODO */
          missing &= ~DIEF_MODIFIERS;
     }

     if (missing & DIEF_LOCKS) {
          event->locks = 0; /* TODO */
          missing &= ~DIEF_LOCKS;
     }

     /*
      * Return if the rest is ok.
      */
     if (missing == DIEF_NONE)
          return;

     /*
      * Without translation table
      */
     if (valid & DIEF_KEYID) {
          if (missing & DIEF_KEYSYMBOL) {
               event->key_symbol = 0; /*id_to_symbol( event->key_id,
                                                 event->modifiers ); FIXME */
               missing &= ~DIEF_KEYSYMBOL;
          }
     }
     else if (valid & DIEF_KEYSYMBOL) {
          event->key_id = symbol_to_id( event->key_symbol );
          missing &= ~DIEF_KEYID;
     }

     /*
      * Clear remaining fields.
      */
     if (missing & DIEF_KEYCODE)
          event->key_code = -1;

     if (missing & DIEF_KEYID)
          event->key_id = DIKI_UNKNOWN;

     if (missing & DIEF_KEYSYMBOL)
          event->key_symbol = DIKS_NULL;
}

static DFBInputDeviceKeyIdentifier
symbol_to_id( DFBInputDeviceKeySymbol symbol )
{
     if (symbol >= 'a' && symbol <= 'z')
          return DIKI_A + symbol - 'a';
     
     if (symbol >= 'A' && symbol <= 'Z')
          return DIKI_A + symbol - 'A';
     
     if (symbol >= '0' && symbol <= '9')
          return DIKI_0 + symbol - '0';

     if (symbol >= DIKS_F1 && symbol <= DIKS_F12)
          return DIKI_F1 + symbol - DIKS_F1;

     switch (symbol) {
          case DIKS_ESCAPE:
               return DIKI_ESCAPE;
          
          case DIKS_CURSOR_LEFT:
               return DIKI_LEFT;
          
          case DIKS_CURSOR_RIGHT:
               return DIKI_RIGHT;

          case DIKS_CURSOR_UP:
               return DIKI_UP;

          case DIKS_CURSOR_DOWN:
               return DIKI_DOWN;
          
          case DIKS_CONTROL:
               return DIKI_CTRL;
          
          case DIKS_SHIFT:
               return DIKI_SHIFT;
          
          case DIKS_ALT:
               return DIKI_ALT;
          
          case DIKS_ALTGR:
               return DIKI_ALTGR;
          
          case DIKS_TAB:
               return DIKI_TAB;
          
          case DIKS_ENTER:
               return DIKI_ENTER;
          
          case DIKS_SPACE:
               return DIKI_SPACE;
          
          case DIKS_BACKSPACE:
               return DIKI_BACKSPACE;
          
          case DIKS_INSERT:
               return DIKI_INSERT;
          
          case DIKS_DELETE:
               return DIKI_DELETE;
          
          case DIKS_HOME:
               return DIKI_HOME;
          
          case DIKS_END:
               return DIKI_END;
          
          case DIKS_PAGE_UP:
               return DIKI_PAGE_UP;
          
          case DIKS_PAGE_DOWN:
               return DIKI_PAGE_DOWN;
          
          case DIKS_CAPSLOCK:
               return DIKI_CAPSLOCK;
          
          case DIKS_NUMLOCK:
               return DIKI_NUMLOCK;
          
          case DIKS_SCROLLLOCK:
               return DIKI_SCRLOCK;
          
          case DIKS_PRINT:
               return DIKI_PRINT;
          
          case DIKS_PAUSE:
               return DIKI_PAUSE;

          default:
               ;
     }
     
     return DIKI_UNKNOWN;
}

static DFBInputDeviceKeySymbol
id_to_symbol( DFBInputDeviceKeyIdentifier id,
              DFBInputDeviceModifierMask  modifiers )
{
     return DIKS_NULL;
}

