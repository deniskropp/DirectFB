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

#include <sys/time.h>

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
     int                          min_keycode;
     int                          max_keycode;
     int                          num_entries;
     DFBInputDeviceKeymapEntry   *entries;
} InputDeviceKeymap;

typedef struct {
     DFBInputDeviceID             id;            /* unique device id */

     InputDeviceInfo              device_info;

     InputDeviceKeymap            keymap;
     
     DFBInputDeviceModifierMask   modifiers_l;
     DFBInputDeviceModifierMask   modifiers_r;
     DFBInputDeviceLockState      locks;
     DFBInputDeviceLockState      locks_pressed;
     DFBInputDeviceButtonMask     buttons;

     FusionReactor               *reactor;       /* event dispatcher */
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


static void
init_devices();

static void
allocate_device_keymap( InputDevice *device );

static DFBInputDeviceKeymapEntry *
get_keymap_entry( InputDevice *device,
                  int          code );

static bool
lookup_from_table( InputDevice        *device,
                   DFBInputEvent      *event,
                   DFBInputEventFlags  lookup );

static void
fixup_key_event( InputDevice   *device,
                 DFBInputEvent *event );

static void
fixup_mouse_event( InputDevice   *device,
                   DFBInputEvent *event );

static DFBInputDeviceKeyIdentifier
symbol_to_id( DFBInputDeviceKeySymbol symbol );

static DFBInputDeviceKeySymbol
id_to_symbol( DFBInputDeviceKeyIdentifier id,
              DFBInputDeviceModifierMask  modifiers,
              DFBInputDeviceLockState     locks );

#ifdef DFB_DYNAMIC_LINKING
static CoreModuleLoadResult
input_driver_handle_func( void *handle, char *name, void *ctx );
#endif


/** public **/

DFBResult
dfb_input_initialize()
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
DFBResult
dfb_input_join()
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

DFBResult
dfb_input_shutdown( bool emergency )
{
     InputDevice *d = inputdevices;

     while (d) {
          InputDevice       *next   = d->next;
          InputDeviceShared *shared = d->shared;
          InputDriver       *driver = d->driver;

          driver->funcs->CloseDevice( d->driver_data );
          
          if (!--driver->nr_devices)
               DFBFREE( driver );

          reactor_free( shared->reactor );

          if (shared->keymap.entries)
               shfree( shared->keymap.entries );
          
          shfree( shared );
          
          DFBFREE( d );

          d = next;
     }

     inputdevices = NULL;

     return DFB_OK;
}

#ifndef FUSION_FAKE
DFBResult
dfb_input_leave( bool emergency )
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
DFBResult
dfb_input_suspend()
{
     InputDevice *d = inputdevices;

     while (d) {
          d->driver->funcs->CloseDevice( d->driver_data );

          d = d->next;
     }

     return DFB_OK;
}

DFBResult
dfb_input_resume()
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

void
dfb_input_register_module( InputDriverFuncs *funcs )
{
     InputDriver *driver;

     driver = DFBCALLOC( 1, sizeof(InputDriver) );

     driver->funcs       = funcs;
     driver->abi_version = funcs->GetAbiVersion();

     fusion_list_prepend( &input_drivers, &driver->link );
}

void
dfb_input_enumerate_devices( InputDeviceCallback  callback,
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
     if (!(event->flags & DIEF_TIMESTAMP)) {
          gettimeofday( &event->timestamp, NULL );
          event->flags |= DIEF_TIMESTAMP;
     }

     switch (event->type) {
          case DIET_BUTTONPRESS:
          case DIET_BUTTONRELEASE:
               if (dfb_config->lefty) {
                    if (event->button == DIBI_LEFT)
                         event->button = DIBI_RIGHT;
                    else if (event->button == DIBI_RIGHT)
                         event->button = DIBI_LEFT;
               }
               /* fallthru */
          case DIET_AXISMOTION:
               fixup_mouse_event( device, event );
               break;

          case DIET_KEYPRESS:
          case DIET_KEYRELEASE:
               fixup_key_event( device, event );
               DEBUGMSG("DirectFB/core/input: key code: %x, id: %x, symbol: %x\n",
                        event->key_code, event->key_id, event->key_symbol);
               break;
          
          default:
               ;
     }

     event->clazz     = DFEC_INPUT;
     event->device_id = device->shared->id;

     reactor_dispatch( device->shared->reactor, event, true );
}

DFBInputDeviceID
dfb_input_device_id( const InputDevice *device )
{
     return device->shared->id;
}

void
dfb_input_device_description( const InputDevice         *device,
                              DFBInputDeviceDescription *desc )
{
     *desc = device->shared->device_info.desc;
}

DFBResult
dfb_input_device_get_keymap_entry( InputDevice               *device,
                                   int                        keycode,
                                   DFBInputDeviceKeymapEntry *entry )
{
     DFBInputDeviceKeymapEntry *keymap_entry;

     keymap_entry = get_keymap_entry( device, keycode );
     if (!keymap_entry)
          return DFB_FAILURE;

     *entry = *keymap_entry;
     
     return DFB_OK;
}

/** internal **/

static void
input_add_device( InputDevice *device )
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
static CoreModuleLoadResult
input_driver_handle_func( void *handle, char *name, void *ctx )
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

static void
allocate_device_keymap( InputDevice *device )
{
     int                        i;
     DFBInputDeviceKeymapEntry *entries;
     InputDeviceShared         *shared      = device->shared;
     DFBInputDeviceDescription *desc        = &shared->device_info.desc;
     int                        num_entries = desc->max_keycode -
                                              desc->min_keycode + 1;

     entries = shcalloc( num_entries, sizeof(DFBInputDeviceKeymapEntry) );
     
     /* write -1 indicating entry is not fetched yet from driver */
     for (i=0; i<num_entries; i++)
          entries[i].code = -1;
     
     shared->keymap.min_keycode = desc->min_keycode;
     shared->keymap.max_keycode = desc->max_keycode;
     shared->keymap.num_entries = num_entries;
     shared->keymap.entries     = entries;

#ifndef FUSION_FAKE
     /* we need to fetch the whole map, otherwise a slave would try to */
     for (i=desc->min_keycode; i<=desc->max_keycode; i++)
          get_keymap_entry( device, i );
#endif
}

static void
init_devices()
{
     FusionLink *link;

     fusion_list_foreach( link, input_drivers ) {
          int               n;
          InputDriver      *driver = (InputDriver*) link;
          InputDriverFuncs *funcs  = driver->funcs;

          funcs->GetDriverInfo( &driver->info );

          driver->nr_devices = funcs->GetAvailable();
          if (!driver->nr_devices)
               continue;

          for (n=0; n<driver->nr_devices; n++) {
               InputDevice     *device;
               InputDeviceInfo  device_info;
               void            *driver_data;

               device = DFBCALLOC( 1, sizeof(InputDevice) );
               device->shared = shcalloc( 1, sizeof(InputDeviceShared) );

               memset( &device_info, 0, sizeof(InputDeviceInfo) );

               device_info.desc.min_keycode = -1;
               device_info.desc.max_keycode = -1;

               device->shared->reactor = reactor_new( sizeof(DFBInputEvent) );

               if (funcs->OpenDevice( device, n, &device_info, &driver_data )) {
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

               if (device_info.desc.min_keycode > device_info.desc.max_keycode) {
                    BUG("min_keycode > max_keycode");
                    device_info.desc.min_keycode = -1;
                    device_info.desc.max_keycode = -1;
               }
               else if (device_info.desc.min_keycode >= 0 &&
                        device_info.desc.max_keycode >= 0)
                    allocate_device_keymap( device );

               /* add it to the list */
               input_add_device( device );
          }
     }
}

static DFBInputDeviceKeymapEntry *
get_keymap_entry( InputDevice *device,
                  int          code )
{
     InputDeviceKeymap         *map = &device->shared->keymap;
     DFBInputDeviceKeymapEntry *entry;

     /* safety check */
     if (code < map->min_keycode || code > map->max_keycode)
          return NULL;

     /* point to right array index */
     entry = &map->entries[code - map->min_keycode];

     /* need to initialize? */
     if (entry->code != code) {
          DFBResult    ret;
          InputDriver *driver = device->driver;

          if (!driver) {
               BUG("seem to be a slave with an empty keymap");
               return NULL;
          }

          /* write keycode to entry */
          entry->code = code;

          /* fetch entry from driver */
          ret = driver->funcs->GetKeymapEntry( device,
                                               device->driver_data, entry );
          if (ret)
               return NULL;

          /* drivers may leave this blank */
          if (entry->identifier == DIKI_UNKNOWN)
               entry->identifier = symbol_to_id( entry->symbols[DIKSI_BASE] );
     }

     return entry;
}

static bool
lookup_from_table( InputDevice        *device,
                   DFBInputEvent      *event,
                   DFBInputEventFlags  lookup )
{
     DFBInputDeviceKeymapEntry *entry;
     
     /* fetch the entry from the keymap, possibly calling the driver */
     entry = get_keymap_entry( device, event->key_code );
     if (!entry)
          return false;

     /* lookup identifier */
     if (lookup & DIEF_KEYID)
          event->key_id = entry->identifier;

     /* lookup symbol */
     if (lookup & DIEF_KEYSYMBOL) {
          DFBInputDeviceKeymapSymbolIndex index =
               (event->modifiers & DIMM_ALTGR) ? DIKSI_ALT : DIKSI_BASE;

          if ((event->modifiers & DIMM_SHIFT) || (entry->locks & event->locks))
               index++;

          /* don't modify modifiers */
          if (DFB_KEY_TYPE( entry->symbols[DIKSI_BASE] ) == DIKT_MODIFIER)
               event->key_symbol = entry->symbols[DIKSI_BASE];
          else
               event->key_symbol = entry->symbols[index];
     }

     return true;
}

#define FIXUP_KEY_FIELDS     (DIEF_MODIFIERS | DIEF_LOCKS | \
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
fixup_key_event( InputDevice *device, DFBInputEvent *event )
{
     DFBInputEventFlags  valid   = event->flags & FIXUP_KEY_FIELDS;
     DFBInputEventFlags  missing = valid ^ FIXUP_KEY_FIELDS;
     InputDeviceShared  *shared  = device->shared;

     /* Add missing flags */
     event->flags |= missing;
     
     /*
      * Use cached values for modifiers/locks if they are missing.
      */
     if (missing & DIEF_MODIFIERS)
          event->modifiers = shared->modifiers_l | shared->modifiers_r;

     if (missing & DIEF_LOCKS)
          event->locks = shared->locks;

     /*
      * With translation table
      */
     if (device->shared->keymap.num_entries) {
          /* FIXME: 3. only, 4. and 5. not implemented yet (see above) */
          if (valid & DIEF_KEYCODE) {
               lookup_from_table( device, event, missing );

               missing &= ~(DIEF_KEYID | DIEF_KEYSYMBOL);
          }
     }
     else {
          /*
           * Without translation table
           */
          if (valid & DIEF_KEYID) {
               if (missing & DIEF_KEYSYMBOL) {
                    event->key_symbol = id_to_symbol( event->key_id,
                                                      event->modifiers,
                                                      event->locks );
                    missing &= ~DIEF_KEYSYMBOL;
               }
          }
          else if (valid & DIEF_KEYSYMBOL) {
               event->key_id = symbol_to_id( event->key_symbol );
               missing &= ~DIEF_KEYID;
          }
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

     /*
      * Update cached values for modifiers.
      */
     if (DFB_KEY_TYPE(event->key_symbol) == DIKT_MODIFIER) {
          if (event->type == DIET_KEYPRESS) {
               switch (event->key_id) {
                    case DIKI_SHIFT_L:
                         shared->modifiers_l |= DIMM_SHIFT;
                         break;
                    case DIKI_SHIFT_R:
                         shared->modifiers_r |= DIMM_SHIFT;
                         break;
                    case DIKI_CONTROL_L:
                         shared->modifiers_l |= DIMM_CONTROL;
                         break;
                    case DIKI_CONTROL_R: 
                         shared->modifiers_r |= DIMM_CONTROL;
                         break;
                    case DIKI_ALT_L:
                         shared->modifiers_l |= DIMM_ALT;
                         break;
                    case DIKI_ALT_R:
                         shared->modifiers_r |= DIMM_ALT;
                         break;
                    case DIKI_ALTGR:
                         shared->modifiers_l |= DIMM_ALTGR;
                         break;
                    case DIKI_META_L:
                         shared->modifiers_l |= DIMM_META;
                         break;
                    case DIKI_META_R:
                         shared->modifiers_r |= DIMM_META;
                         break;
                    case DIKI_SUPER_L:
                         shared->modifiers_l |= DIMM_SUPER;
                         break;
                    case DIKI_SUPER_R:
                         shared->modifiers_r |= DIMM_SUPER;
                         break;
                    case DIKI_HYPER_L:
                         shared->modifiers_l |= DIMM_HYPER;
                         break;
                    case DIKI_HYPER_R:
                         shared->modifiers_r |= DIMM_HYPER;
                         break;
                    default:
                         ;
               }
          }
          else {
               switch (event->key_id) {
                    case DIKI_SHIFT_L:
                         shared->modifiers_l &= ~DIMM_SHIFT;
                         break;
                    case DIKI_SHIFT_R:
                         shared->modifiers_r &= ~DIMM_SHIFT;
                         break;
                    case DIKI_CONTROL_L:
                         shared->modifiers_l &= ~DIMM_CONTROL;
                         break;
                    case DIKI_CONTROL_R: 
                         shared->modifiers_r &= ~DIMM_CONTROL;
                         break;
                    case DIKI_ALT_L:
                         shared->modifiers_l &= ~DIMM_ALT;
                         break;
                    case DIKI_ALT_R:
                         shared->modifiers_r &= ~DIMM_ALT;
                         break;
                    case DIKI_ALTGR:
                         shared->modifiers_l &= ~DIMM_ALTGR;
                         break;
                    case DIKI_META_L:
                         shared->modifiers_l &= ~DIMM_META;
                         break;
                    case DIKI_META_R:
                         shared->modifiers_r &= ~DIMM_META;
                         break;
                    case DIKI_SUPER_L:
                         shared->modifiers_l &= ~DIMM_SUPER;
                         break;
                    case DIKI_SUPER_R:
                         shared->modifiers_r &= ~DIMM_SUPER;
                         break;
                    case DIKI_HYPER_L:
                         shared->modifiers_l &= ~DIMM_HYPER;
                         break;
                    case DIKI_HYPER_R:
                         shared->modifiers_r &= ~DIMM_HYPER;
                         break;
                    default:
                         ;
               }
          }
          
          /* write back to event */
          if (missing & DIEF_MODIFIERS)
               event->modifiers = shared->modifiers_l | shared->modifiers_r;
     }
     
     /*
      * Update cached values for locks.
      */
     if (event->type == DIET_KEYPRESS) {
          switch (event->key_id) {
               case DIKI_CAPS_LOCK:
                    shared->locks ^= (DILS_CAPS & ~shared->locks_pressed);
                    shared->locks_pressed |= DILS_CAPS;
                    break;
               case DIKI_NUM_LOCK:
                    shared->locks ^= (DILS_NUM & ~shared->locks_pressed);
                    shared->locks_pressed |= DILS_NUM;
                    break;
               case DIKI_SCROLL_LOCK:
                    shared->locks ^= (DILS_SCROLL & ~shared->locks_pressed);
                    shared->locks_pressed |= DILS_SCROLL;
                    break;
               default:
                    ;
          }
          
          /* write back to event */
          if (missing & DIEF_LOCKS)
               event->locks = shared->locks;
     }
     else {
          switch (event->key_id) {
               case DIKI_CAPS_LOCK:
                    shared->locks_pressed &= ~DILS_CAPS;
                    break;
               case DIKI_NUM_LOCK:
                    shared->locks_pressed &= ~DILS_NUM;
                    break;
               case DIKI_SCROLL_LOCK:
                    shared->locks_pressed &= ~DILS_SCROLL;
                    break;
               default:
                    ;
          }
     }
}

static void
fixup_mouse_event( InputDevice *device, DFBInputEvent *event )
{
     InputDeviceShared *shared = device->shared;

     if (event->flags & DIEF_BUTTONS) {
          shared->buttons = event->buttons;
     }
     else {
          switch (event->type) {
               case DIET_BUTTONPRESS:
                    shared->buttons |= (1 << event->button);
                    break;
               case DIET_BUTTONRELEASE:
                    shared->buttons &= ~(1 << event->button);
                    break;
               default:
                    ;
          }

          /* Add missing flag */
          event->flags |= DIEF_BUTTONS;

          event->buttons = shared->buttons;
     }
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
          
          case DIKS_ALTGR:
               return DIKI_ALTGR;
          
          case DIKS_CONTROL:
               return DIKI_CONTROL_L;
          
          case DIKS_SHIFT:
               return DIKI_SHIFT_L;
          
          case DIKS_ALT:
               return DIKI_ALT_L;
          
          case DIKS_META:
               return DIKI_META_L;
          
          case DIKS_SUPER:
               return DIKI_SUPER_L;
          
          case DIKS_HYPER:
               return DIKI_HYPER_L;
          
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
          
          case DIKS_CAPS_LOCK:
               return DIKI_CAPS_LOCK;
          
          case DIKS_NUM_LOCK:
               return DIKI_NUM_LOCK;
          
          case DIKS_SCROLL_LOCK:
               return DIKI_SCROLL_LOCK;
          
          case DIKS_PRINT:
               return DIKI_PRINT;
          
          case DIKS_PAUSE:
               return DIKI_PAUSE;

          case DIKS_BACKSLASH:
               return DIKI_BACKSLASH;

          case DIKS_PERIOD:
               return DIKI_PERIOD;

          case DIKS_COMMA:
               return DIKI_COMMA;

          default:
               ;
     }
     
     return DIKI_UNKNOWN;
}

static DFBInputDeviceKeySymbol
id_to_symbol( DFBInputDeviceKeyIdentifier id,
              DFBInputDeviceModifierMask  modifiers,
              DFBInputDeviceLockState     locks )
{
     bool shift = (modifiers & DIMM_SHIFT) || (locks & DILS_CAPS);

     if (id >= DIKI_A && id <= DIKI_Z)
          return (shift ? DIKS_CAPITAL_A : DIKS_SMALL_A) + id - DIKI_A;
     
     if (id >= DIKI_0 && id <= DIKI_9)
          return DIKS_0 + id - DIKI_0;

     if ((locks & DILS_NUM) && id >= DIKI_KP_0 && id <= DIKI_KP_9)
          return DIKS_0 + id - DIKI_KP_0;

     if (id >= DIKI_F1 && id <= DIKI_F12)
          return DIKS_F1 + id - DIKI_F1;

     switch (id) {
          case DIKI_ESCAPE:
               return DIKS_ESCAPE;
          
          case DIKI_LEFT:
               return DIKS_CURSOR_LEFT;
          
          case DIKI_RIGHT:
               return DIKS_CURSOR_RIGHT;

          case DIKI_UP:
               return DIKS_CURSOR_UP;

          case DIKI_DOWN:
               return DIKS_CURSOR_DOWN;
          
          case DIKI_ALTGR:
               return DIKS_ALTGR;
          
          case DIKI_CONTROL_L:
          case DIKI_CONTROL_R:
               return DIKS_CONTROL;
          
          case DIKI_SHIFT_L:
          case DIKI_SHIFT_R:
               return DIKS_SHIFT;
          
          case DIKI_ALT_L:
          case DIKI_ALT_R:
               return DIKS_ALT;
          
          case DIKI_META_L:
          case DIKI_META_R:
               return DIKS_META;
          
          case DIKI_SUPER_L:
          case DIKI_SUPER_R:
               return DIKS_SUPER;
          
          case DIKI_HYPER_L:
          case DIKI_HYPER_R:
               return DIKS_HYPER;
          
          case DIKI_TAB:
               return DIKS_TAB;
          
          case DIKI_ENTER:
               return DIKS_ENTER;
          
          case DIKI_SPACE:
               return DIKS_SPACE;
          
          case DIKI_BACKSPACE:
               return DIKS_BACKSPACE;
          
          case DIKI_INSERT:
               return DIKS_INSERT;
          
          case DIKI_DELETE:
               return DIKS_DELETE;
          
          case DIKI_HOME:
               return DIKS_HOME;
          
          case DIKI_END:
               return DIKS_END;
          
          case DIKI_PAGE_UP:
               return DIKS_PAGE_UP;
          
          case DIKI_PAGE_DOWN:
               return DIKS_PAGE_DOWN;
          
          case DIKI_CAPS_LOCK:
               return DIKS_CAPS_LOCK;
          
          case DIKI_NUM_LOCK:
               return DIKS_NUM_LOCK;
          
          case DIKI_SCROLL_LOCK:
               return DIKS_SCROLL_LOCK;
          
          case DIKI_PRINT:
               return DIKS_PRINT;
          
          case DIKI_PAUSE:
               return DIKS_PAUSE;

          case DIKI_KP_DIV:
               return DIKS_SLASH;
               
          case DIKI_KP_MULT:
               return DIKS_ASTERISK;

          case DIKI_KP_MINUS:
               return DIKS_MINUS_SIGN;
              
          case DIKI_KP_PLUS:
               return DIKS_PLUS_SIGN;

          case DIKI_KP_ENTER:
               return DIKS_ENTER;
          
          case DIKI_KP_SPACE:
               return DIKS_SPACE;
          
          case DIKI_KP_TAB:
               return DIKS_TAB;
          
          case DIKI_KP_F1:
               return DIKS_F1;
          
          case DIKI_KP_F2:
               return DIKS_F2;
          
          case DIKI_KP_F3:
               return DIKS_F3;
          
          case DIKI_KP_F4:
               return DIKS_F4;
          
          case DIKI_KP_EQUAL:
               return DIKS_EQUALS_SIGN;
          
          case DIKI_KP_DECIMAL:
               return DIKS_PERIOD;
          
          case DIKI_KP_SEPARATOR:
               return DIKS_COMMA;

          case DIKI_KP_0:
               return DIKS_INSERT;
          
          case DIKI_KP_1:
               return DIKS_END;
          
          case DIKI_KP_2:
               return DIKS_CURSOR_DOWN;
          
          case DIKI_KP_3:
               return DIKS_PAGE_DOWN;
          
          case DIKI_KP_4:
               return DIKS_CURSOR_LEFT;
          
          case DIKI_KP_5:
               return DIKS_BEGIN;
          
          case DIKI_KP_6:
               return DIKS_CURSOR_RIGHT;
          
          case DIKI_KP_7:
               return DIKS_HOME;
          
          case DIKI_KP_8:
               return DIKS_CURSOR_UP;
          
          case DIKI_KP_9:
               return DIKS_PAGE_UP;

          default:
               ;
     }
     
     return DIKS_NULL;
}


