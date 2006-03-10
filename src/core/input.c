/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <direct/list.h>
#include <direct/messages.h>


#include <fusion/shmalloc.h>
#include <fusion/reactor.h>
#include <fusion/arena.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core_parts.h>

#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <core/system.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/input.h>
#include <core/windows.h>
#include <core/windows_internal.h>

#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/modules.h>
#include <direct/trace.h>

#include <fusion/build.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <gfx/convert.h>


D_DEBUG_DOMAIN( Core_Input, "Core/Input", "DirectFB Input Core" );


DEFINE_MODULE_DIRECTORY( dfb_input_modules, "inputdrivers",
                         DFB_INPUT_DRIVER_ABI_VERSION );

/**********************************************************************************************************************/

typedef struct {
     DirectLink               link;

     int                      magic;

     DirectModuleEntry       *module;

     const InputDriverFuncs  *funcs;

     InputDriverInfo          info;

     int                      nr_devices;
} InputDriver;

typedef struct {
     int                          min_keycode;
     int                          max_keycode;
     int                          num_entries;
     DFBInputDeviceKeymapEntry   *entries;
} InputDeviceKeymap;

typedef struct {
     int                          magic;

     DFBInputDeviceID             id;            /* unique device id */

     int                          num;

     InputDeviceInfo              device_info;

     InputDeviceKeymap            keymap;

     DFBInputDeviceModifierMask   modifiers_l;
     DFBInputDeviceModifierMask   modifiers_r;
     DFBInputDeviceLockState      locks;
     DFBInputDeviceButtonMask     buttons;

     DFBInputDeviceKeyIdentifier  last_key;      /* last key pressed */
     bool                         first_press;   /* first press of key */

     FusionReactor               *reactor;       /* event dispatcher */
     FusionSkirmish               lock;

     FusionCall                   call;          /* driver call via master */
} InputDeviceShared;

struct __DFB_CoreInputDevice {
     DirectLink          link;

     int                 magic;

     InputDeviceShared  *shared;

     InputDriver        *driver;
     void               *driver_data;
};

typedef struct {
     int                num;
     InputDeviceShared *devices[MAX_INPUTDEVICES];
} CoreInput;

/**********************************************************************************************************************/

static DirectLink *drivers;
static DirectLink *devices;

static CoreInput  *core_input;

DFB_CORE_PART( input, 0, sizeof(CoreInput) )

/**********************************************************************************************************************/

typedef enum {
     CIDC_RELOAD_KEYMAP
} CoreInputDeviceCommand;

/**********************************************************************************************************************/

static void init_devices( CoreDFB *core );

static void allocate_device_keymap( CoreDFB *core, CoreInputDevice *device );

static DFBInputDeviceKeymapEntry *get_keymap_entry( CoreInputDevice *device,
                                                    int              code );

/**********************************************************************************************************************/

static bool lookup_from_table( CoreInputDevice    *device,
                               DFBInputEvent      *event,
                               DFBInputEventFlags  lookup );

static void fixup_key_event  ( CoreInputDevice    *device,
                               DFBInputEvent      *event );

static void fixup_mouse_event( CoreInputDevice    *device,
                               DFBInputEvent      *event );

static void flush_keys       ( CoreInputDevice    *device );

static bool core_input_filter( CoreInputDevice    *device,
                               DFBInputEvent      *event );

/**********************************************************************************************************************/

static DFBInputDeviceKeyIdentifier symbol_to_id( DFBInputDeviceKeySymbol     symbol );

static DFBInputDeviceKeySymbol     id_to_symbol( DFBInputDeviceKeyIdentifier id,
                                                 DFBInputDeviceModifierMask  modifiers,
                                                 DFBInputDeviceLockState     locks );

/**********************************************************************************************************************/

static ReactionFunc dfb_input_globals[MAX_INPUT_GLOBALS+1] = {
/* 0 */   _dfb_windowstack_inputdevice_listener,
          NULL
};

DFBResult
dfb_input_add_global( ReactionFunc  func,
                      int          *ret_index )
{
     int i;

     D_DEBUG_AT( Core_Input, "%s( %p, %p )\n", __FUNCTION__, func, ret_index );

     D_ASSERT( func != NULL );
     D_ASSERT( ret_index != NULL );

     for (i=0; i<MAX_INPUT_GLOBALS; i++) {
          if (!dfb_input_globals[i]) {
               dfb_input_globals[i] = func;

               D_DEBUG_AT( Core_Input, "  -> index %d\n", i );

               *ret_index = i;

               return DFB_OK;
          }
     }

     return DFB_LIMITEXCEEDED;
}

DFBResult
dfb_input_set_global( ReactionFunc func,
                      int          index )
{
     D_DEBUG_AT( Core_Input, "%s( %p, %d )\n", __FUNCTION__, func, index );

     D_ASSERT( func != NULL );
     D_ASSERT( index >= 0 );
     D_ASSERT( index < MAX_INPUT_GLOBALS );

     D_ASSUME( dfb_input_globals[index] == NULL );

     dfb_input_globals[index] = func;

     return DFB_OK;
}

/** public **/

static DFBResult
dfb_input_initialize( CoreDFB *core, void *data_local, void *data_shared )
{
     D_DEBUG_AT( Core_Input, "%s( %p, %p, %p )\n", __FUNCTION__, core, data_local, data_shared );

     D_ASSERT( core_input == NULL );

     core_input = data_shared;

     direct_modules_explore_directory( &dfb_input_modules );

     init_devices( core );

     return DFB_OK;
}

static DFBResult
dfb_input_join( CoreDFB *core, void *data_local, void *data_shared )
{
     int i;

     D_DEBUG_AT( Core_Input, "%s( %p, %p, %p )\n", __FUNCTION__, core, data_local, data_shared );

     D_ASSERT( core_input == NULL );

     core_input = data_shared;

     for (i=0; i<core_input->num; i++) {
          CoreInputDevice *device;

          device = D_CALLOC( 1, sizeof(CoreInputDevice) );
          if (!device) {
               return D_OOM();
          }

          device->shared = core_input->devices[i];

          /* add it to the list */
          direct_list_append( &devices, &device->link );

          D_MAGIC_SET( device, CoreInputDevice );
     }

     return DFB_OK;
}

static DFBResult
dfb_input_shutdown( CoreDFB *core, bool emergency )
{
     DirectLink          *n;
     CoreInputDevice     *device;
     FusionSHMPoolShared *pool = dfb_core_shmpool( core );

     D_DEBUG_AT( Core_Input, "%s( %p, %semergency )\n", __FUNCTION__, core, emergency ? "" : "no " );

     D_ASSERT( core_input != NULL );

     direct_list_foreach_safe (device, n, devices) {
          InputDriver       *driver = device->driver;
          InputDeviceShared *shared = device->shared;

          D_MAGIC_ASSERT( device, CoreInputDevice );

          fusion_call_destroy( &shared->call );
          fusion_skirmish_destroy( &shared->lock );

          driver->funcs->CloseDevice( device->driver_data );

          if (!--driver->nr_devices) {
               direct_module_unref( driver->module );
               D_FREE( driver );
          }

          fusion_reactor_free( shared->reactor );

          if (shared->keymap.entries)
               SHFREE( pool, shared->keymap.entries );

          SHFREE( pool, shared );

          D_MAGIC_CLEAR( device );

          D_FREE( device );
     }

     devices = NULL;
     drivers = NULL;

     core_input = NULL;

     return DFB_OK;
}

static DFBResult
dfb_input_leave( CoreDFB *core, bool emergency )
{
     DirectLink      *n;
     CoreInputDevice *device;

     D_DEBUG_AT( Core_Input, "%s( %p, %semergency )\n", __FUNCTION__, core, emergency ? "" : "no " );

     D_ASSERT( core_input != NULL );

     direct_list_foreach_safe (device, n, devices) {
          D_MAGIC_ASSERT( device, CoreInputDevice );

          D_FREE( device );
     }

     devices    = NULL;
     core_input = NULL;

     return DFB_OK;
}

static DFBResult
dfb_input_suspend( CoreDFB *core )
{
     CoreInputDevice *device;

     D_DEBUG_AT( Core_Input, "%s( %p )\n", __FUNCTION__, core );

     D_ASSERT( core_input != NULL );

     D_DEBUG_AT( Core_Input, "  -> suspending...\n" );

     direct_list_foreach (device, devices) {
          InputDriver       *driver = device->driver;
          InputDeviceShared *shared = device->shared;

          (void) shared;

          D_MAGIC_ASSERT( device, CoreInputDevice );

          D_DEBUG_AT( Core_Input, "  -> closing '%s' (%d) %d.%d (%s)\n",
                      shared->device_info.desc.name, shared->num + 1,
                      driver->info.version.major,
                      driver->info.version.minor, driver->info.vendor );

          driver->funcs->CloseDevice( device->driver_data );

          flush_keys( device );
     }

     D_DEBUG_AT( Core_Input, "  -> suspended.\n" );

     return DFB_OK;
}

static DFBResult
dfb_input_resume( CoreDFB *core )
{
     DFBResult        ret;
     CoreInputDevice *device;

     D_DEBUG_AT( Core_Input, "%s( %p )\n", __FUNCTION__, core );

     D_ASSERT( core_input != NULL );

     D_DEBUG_AT( Core_Input, "  -> resuming...\n" );

     direct_list_foreach (device, devices) {
          D_MAGIC_ASSERT( device, CoreInputDevice );

          D_DEBUG_AT( Core_Input, "  -> reopening '%s' (%d) %d.%d (%s)\n",
                      device->shared->device_info.desc.name, device->shared->num + 1,
                      device->driver->info.version.major,
                      device->driver->info.version.minor,
                      device->driver->info.vendor );

          ret = device->driver->funcs->OpenDevice( device, device->shared->num,
                                                   &device->shared->device_info,
                                                   &device->driver_data );
          if (ret) {
               D_DERROR( ret, "DirectFB/Input: Failed reopening device "
                         "during resume (%s)!\n", device->shared->device_info.desc.name );
          }
     }

     D_DEBUG_AT( Core_Input, "  -> resumed.\n" );

     return DFB_OK;
}

void
dfb_input_enumerate_devices( InputDeviceCallback         callback,
                             void                       *ctx,
                             DFBInputDeviceCapabilities  caps )
{
     CoreInputDevice *device;

     D_ASSERT( core_input != NULL );

     direct_list_foreach (device, devices) {
          D_MAGIC_ASSERT( device, CoreInputDevice );

          if ((device->shared->device_info.desc.caps & caps) && callback( device, ctx ) == DFENUM_CANCEL)
               break;
     }
}

DirectResult
dfb_input_attach( CoreInputDevice *device,
                  ReactionFunc     func,
                  void            *ctx,
                  Reaction        *reaction )
{
     D_DEBUG_AT( Core_Input, "%s( %p, %p, %p, %p )\n", __FUNCTION__, device, func, ctx, reaction );

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     return fusion_reactor_attach( device->shared->reactor, func, ctx, reaction );
}

DirectResult
dfb_input_detach( CoreInputDevice *device,
                  Reaction        *reaction )
{
     D_DEBUG_AT( Core_Input, "%s( %p, %p )\n", __FUNCTION__, device, reaction );

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     return fusion_reactor_detach( device->shared->reactor, reaction );
}

DirectResult
dfb_input_attach_global( CoreInputDevice *device,
                         int              index,
                         void            *ctx,
                         GlobalReaction  *reaction )
{
     D_DEBUG_AT( Core_Input, "%s( %p, %d, %p, %p )\n", __FUNCTION__, device, index, ctx, reaction );

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     return fusion_reactor_attach_global( device->shared->reactor, index, ctx, reaction );
}

DirectResult
dfb_input_detach_global( CoreInputDevice *device,
                         GlobalReaction  *reaction )
{
     D_DEBUG_AT( Core_Input, "%s( %p, %p )\n", __FUNCTION__, device, reaction );

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     return fusion_reactor_detach_global( device->shared->reactor, reaction );
}

void
dfb_input_dispatch( CoreInputDevice *device, DFBInputEvent *event )
{
     D_DEBUG_AT( Core_Input, "%s( %p, %p )\n", __FUNCTION__, device, event );

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( event != NULL );

     D_ASSUME( device->shared != NULL );

     if (!device->shared)
          return;

     D_ASSUME( device->shared->reactor != NULL );

     if (!device->shared->reactor)
          return;

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
               if (dfb_config->capslock_meta) {
                    if (device->shared->keymap.num_entries && (event->flags & DIEF_KEYCODE))
                         lookup_from_table( device, event, (DIEF_KEYID |
                                                            DIEF_KEYSYMBOL) & ~event->flags );

                    if (event->key_id == DIKI_CAPS_LOCK || event->key_symbol == DIKS_CAPS_LOCK) {
                         event->flags     |= DIEF_KEYID | DIEF_KEYSYMBOL;
                         event->key_code   = -1;
                         event->key_id     = DIKI_META_L;
                         event->key_symbol = DIKS_META;
                    }
               }

               fixup_key_event( device, event );

               D_DEBUG_AT( Core_Input, "  -> key code: %x, id: %x, symbol: %x\n",
                           event->key_code, event->key_id, event->key_symbol );
               break;

          default:
               ;
     }

     event->clazz     = DFEC_INPUT;
     event->device_id = device->shared->id;

     if (!core_input_filter( device, event ))
          fusion_reactor_dispatch( device->shared->reactor, event, true, dfb_input_globals );
}

DFBInputDeviceID
dfb_input_device_id( const CoreInputDevice *device )
{
     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     return device->shared->id;
}

CoreInputDevice *
dfb_input_device_at( DFBInputDeviceID id )
{
     CoreInputDevice *device;

     D_ASSERT( core_input != NULL );

     direct_list_foreach (device, devices) {
          D_MAGIC_ASSERT( device, CoreInputDevice );

          if (device->shared->id == id)
               return device;
     }

     return NULL;
}

void
dfb_input_device_description( const CoreInputDevice     *device,
                              DFBInputDeviceDescription *desc )
{
     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     *desc = device->shared->device_info.desc;
}

DFBResult
dfb_input_device_get_keymap_entry( CoreInputDevice           *device,
                                   int                        keycode,
                                   DFBInputDeviceKeymapEntry *entry )
{
     DFBInputDeviceKeymapEntry *keymap_entry;

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( entry != NULL );

     keymap_entry = get_keymap_entry( device, keycode );
     if (!keymap_entry)
          return DFB_FAILURE;

     *entry = *keymap_entry;

     return DFB_OK;
}

DFBResult
dfb_input_device_reload_keymap( CoreInputDevice *device )
{
     int                ret;
     InputDeviceShared *shared;

     D_MAGIC_ASSERT( device, CoreInputDevice );

     shared = device->shared;

     D_ASSERT( shared != NULL );

     D_INFO( "DirectFB/Input: Reloading keymap for '%s' [0x%02x]...\n",
             shared->device_info.desc.name, shared->id );

     if (fusion_call_execute( &shared->call, FCEF_NONE, CIDC_RELOAD_KEYMAP, NULL, &ret ))
          return DFB_FUSION;

     return ret;
}

/** internal **/

static void
input_add_device( CoreInputDevice *device )
{
     D_DEBUG_AT( Core_Input, "%s( %p )\n", __FUNCTION__, device );

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     if (core_input->num == MAX_INPUTDEVICES) {
          D_ERROR( "DirectFB/Input: Maximum number of devices reached!\n" );
          return;
     }

     direct_list_append( &devices, &device->link );

     core_input->devices[ core_input->num++ ] = device->shared;
}

static void
allocate_device_keymap( CoreDFB *core, CoreInputDevice *device )
{
     int                        i;
     DFBInputDeviceKeymapEntry *entries;
     FusionSHMPoolShared       *pool        = dfb_core_shmpool( core );
     InputDeviceShared         *shared      = device->shared;
     DFBInputDeviceDescription *desc        = &shared->device_info.desc;
     int                        num_entries = desc->max_keycode -
                                              desc->min_keycode + 1;

     D_DEBUG_AT( Core_Input, "%s( %p, %p )\n", __FUNCTION__, core, device );

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );

     entries = SHCALLOC( pool, num_entries, sizeof(DFBInputDeviceKeymapEntry) );

     /* write -1 indicating entry is not fetched yet from driver */
     for (i=0; i<num_entries; i++)
          entries[i].code = -1;

     shared->keymap.min_keycode = desc->min_keycode;
     shared->keymap.max_keycode = desc->max_keycode;
     shared->keymap.num_entries = num_entries;
     shared->keymap.entries     = entries;

#if FUSION_BUILD_MULTI
     /* we need to fetch the whole map, otherwise a slave would try to */
     for (i=desc->min_keycode; i<=desc->max_keycode; i++)
          get_keymap_entry( device, i );
#endif
}

static int
make_id( DFBInputDeviceID prefered )
{
     CoreInputDevice *device;

     D_DEBUG_AT( Core_Input, "%s( 0x%02x )\n", __FUNCTION__, prefered );

     D_ASSERT( core_input != NULL );

     direct_list_foreach (device, devices) {
          D_MAGIC_ASSERT( device, CoreInputDevice );

          if (device->shared->id == prefered)
               return make_id( (prefered < DIDID_ANY) ? DIDID_ANY : (prefered + 1) );
     }

     return prefered;
}

static DFBResult
reload_keymap( CoreInputDevice *device )
{
     int                i;
     InputDeviceShared *shared;

     D_DEBUG_AT( Core_Input, "%s( %p )\n", __FUNCTION__, device );

     D_MAGIC_ASSERT( device, CoreInputDevice );

     shared = device->shared;

     D_ASSERT( shared != NULL );

     if (shared->device_info.desc.min_keycode < 0 ||
         shared->device_info.desc.max_keycode < 0)
          return DFB_UNSUPPORTED;

     /* write -1 indicating entry is not fetched yet from driver */
     for (i=0; i<shared->keymap.num_entries; i++)
          shared->keymap.entries[i].code = -1;

     /* fetch the whole map */
     for (i=shared->keymap.min_keycode; i<=shared->keymap.max_keycode; i++)
          get_keymap_entry( device, i );

     D_INFO( "DirectFB/Input: Reloaded keymap for '%s' [0x%02x]\n",
             shared->device_info.desc.name, shared->id );

     return DFB_OK;
}

static int
input_device_call_handler( int   caller,   /* fusion id of the caller */
                           int   call_arg, /* optional call parameter */
                           void *call_ptr, /* optional call parameter */
                           void *ctx )     /* optional handler context */
{
     CoreInputDeviceCommand  command = call_arg;
     CoreInputDevice        *device  = ctx;

     D_DEBUG_AT( Core_Input, "%s( %d, %d, %p, %p )\n", __FUNCTION__, caller, call_arg, call_ptr, ctx );

     D_MAGIC_ASSERT( device, CoreInputDevice );

     switch (command) {
          case CIDC_RELOAD_KEYMAP:
               return reload_keymap( device );

          default:
               D_BUG( "unknown Core Input Device Command '%d'", command );
     }

     return DFB_BUG;
}

static void
init_devices( CoreDFB *core )
{
     DirectLink          *link;
     FusionSHMPoolShared *pool = dfb_core_shmpool( core );

     D_DEBUG_AT( Core_Input, "%s( %p )\n", __FUNCTION__, core );

     D_ASSERT( core_input != NULL );

     direct_list_foreach( link, dfb_input_modules.entries ) {
          int                     n;
          InputDriver            *driver;
          const InputDriverFuncs *funcs;
          DirectModuleEntry      *module = (DirectModuleEntry*) link;

          funcs = direct_module_ref( module );
          if (!funcs)
               continue;

          driver = D_CALLOC( 1, sizeof(InputDriver) );
          if (!driver) {
               direct_module_unref( module );
               continue;
          }

          D_ASSERT( funcs->GetDriverInfo != NULL );

          funcs->GetDriverInfo( &driver->info );

          D_DEBUG_AT( Core_Input, "  -> probing '%s'...\n", driver->info.name );

          driver->nr_devices = funcs->GetAvailable();
          if (!driver->nr_devices) {
               direct_module_unref( module );
               D_FREE( driver );
               continue;
          }

          D_DEBUG_AT( Core_Input, "  -> %d available device(s) provided by '%s'.\n",
                      driver->nr_devices, driver->info.name );

          driver->module = module;
          driver->funcs  = funcs;

          direct_list_prepend( &drivers, &driver->link );


          for (n=0; n<driver->nr_devices; n++) {
               char               buf[128];
               CoreInputDevice   *device;
               InputDeviceInfo    device_info;
               InputDeviceShared *shared;
               void              *driver_data;

               /* FIXME: error handling */
               device = D_CALLOC( 1, sizeof(CoreInputDevice) );
               shared = SHCALLOC( pool, 1, sizeof(InputDeviceShared) );

               memset( &device_info, 0, sizeof(InputDeviceInfo) );

               device_info.desc.min_keycode = -1;
               device_info.desc.max_keycode = -1;

               D_MAGIC_SET( device, CoreInputDevice );

               if (funcs->OpenDevice( device, n, &device_info, &driver_data )) {
                    SHFREE( pool, shared );
                    D_MAGIC_CLEAR( device );
                    D_FREE( device );
                    continue;
               }

               if (driver->nr_devices > 1)
                    snprintf( buf, sizeof(buf), "%s (%d)", device_info.desc.name, n+1 );
               else
                    snprintf( buf, sizeof(buf), "%s", device_info.desc.name );

               /* init skirmish */
               fusion_skirmish_init( &shared->lock, buf, dfb_core_world(core) );

               /* create reactor */
               shared->reactor = fusion_reactor_new( sizeof(DFBInputEvent), buf, dfb_core_world(core) );

               fusion_reactor_set_lock( shared->reactor, &shared->lock );

               /* init call */
               fusion_call_init( &shared->call, input_device_call_handler, device, dfb_core_world(core) );

               /* initialize shared data */
               shared->id          = make_id(device_info.prefered_id);
               shared->num         = n;
               shared->device_info = device_info;
               shared->last_key    = DIKI_UNKNOWN;
               shared->first_press = true;

               /* initialize local data */
               device->shared      = shared;
               device->driver      = driver;
               device->driver_data = driver_data;

               D_INFO( "DirectFB/Input: %s %d.%d (%s)\n",
                       buf, driver->info.version.major,
                       driver->info.version.minor, driver->info.vendor );

               if (device_info.desc.min_keycode > device_info.desc.max_keycode) {
                    D_BUG("min_keycode > max_keycode");
                    device_info.desc.min_keycode = -1;
                    device_info.desc.max_keycode = -1;
               }
               else if (device_info.desc.min_keycode >= 0 &&
                        device_info.desc.max_keycode >= 0)
                    allocate_device_keymap( core, device );

               /* add it to the list */
               input_add_device( device );
          }
     }
}

static DFBInputDeviceKeymapEntry *
get_keymap_entry( CoreInputDevice *device,
                  int              code )
{
     InputDeviceKeymap         *map;
     DFBInputDeviceKeymapEntry *entry;

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     map = &device->shared->keymap;

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
               D_BUG("seem to be a slave with an empty keymap");
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

          if (entry->symbols[DIKSI_BASE_SHIFT] == DIKS_NULL)
               entry->symbols[DIKSI_BASE_SHIFT] = entry->symbols[DIKSI_BASE];

          if (entry->symbols[DIKSI_ALT] == DIKS_NULL)
               entry->symbols[DIKSI_ALT] = entry->symbols[DIKSI_BASE];

          if (entry->symbols[DIKSI_ALT_SHIFT] == DIKS_NULL)
               entry->symbols[DIKSI_ALT_SHIFT] = entry->symbols[DIKSI_ALT];
     }

     return entry;
}

static bool
lookup_from_table( CoreInputDevice    *device,
                   DFBInputEvent      *event,
                   DFBInputEventFlags  lookup )
{
     DFBInputDeviceKeymapEntry *entry;

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );
     D_ASSERT( event != NULL );

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

static int
find_key_code_by_id( CoreInputDevice             *device,
                     DFBInputDeviceKeyIdentifier  id )
{
     int                i;
     InputDeviceKeymap *map;

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     map = &device->shared->keymap;

     for (i=0; i<map->num_entries; i++) {
          DFBInputDeviceKeymapEntry *entry = &map->entries[i];

          if (entry->identifier == id)
               return entry->code;
     }

     return -1;
}

static int
find_key_code_by_symbol( CoreInputDevice         *device,
                         DFBInputDeviceKeySymbol  symbol )
{
     int                i;
     InputDeviceKeymap *map;

     D_MAGIC_ASSERT( device, CoreInputDevice );

     D_ASSERT( core_input != NULL );
     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     map = &device->shared->keymap;

     for (i=0; i<map->num_entries; i++) {
          int                        n;
          DFBInputDeviceKeymapEntry *entry = &map->entries[i];

          for (n=0; n<=DIKSI_LAST; n++)
               if (entry->symbols[n] == symbol)
                    return entry->code;
     }

     return -1;
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
fixup_key_event( CoreInputDevice *device, DFBInputEvent *event )
{
     DFBInputEventFlags  valid   = event->flags & FIXUP_KEY_FIELDS;
     DFBInputEventFlags  missing = valid ^ FIXUP_KEY_FIELDS;
     InputDeviceShared  *shared  = device->shared;

     D_MAGIC_ASSERT( device, CoreInputDevice );

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
          if (valid & DIEF_KEYCODE) {
               lookup_from_table( device, event, missing );

               missing &= ~(DIEF_KEYID | DIEF_KEYSYMBOL);
          }
          else if (valid & DIEF_KEYID) {
               event->key_code = find_key_code_by_id( device, event->key_id );

               if (event->key_code != -1) {
                    lookup_from_table( device, event, missing );

                    missing &= ~(DIEF_KEYCODE | DIEF_KEYSYMBOL);
               }
               else if (missing & DIEF_KEYSYMBOL) {
                    event->key_symbol = id_to_symbol( event->key_id,
                                                      event->modifiers,
                                                      event->locks );
                    missing &= ~DIEF_KEYSYMBOL;
               }
          }
          else if (valid & DIEF_KEYSYMBOL) {
               event->key_code = find_key_code_by_symbol( device,
                                                          event->key_symbol );

               if (event->key_code != -1) {
                    lookup_from_table( device, event, missing );

                    missing &= ~(DIEF_KEYCODE | DIEF_KEYID);
               }
               else {
                    event->key_symbol = symbol_to_id( event->key_symbol );
                    missing &= ~DIEF_KEYSYMBOL;
               }
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

          /* When we receive a new key press, toggle lock flags */
          if (shared->first_press || shared->last_key != event->key_id) {
              switch (event->key_id) {
                   case DIKI_CAPS_LOCK:
                        shared->locks ^= DILS_CAPS;
                        break;
                   case DIKI_NUM_LOCK:
                        shared->locks ^= DILS_NUM;
                        break;
                   case DIKI_SCROLL_LOCK:
                        shared->locks ^= DILS_SCROLL;
                        break;
                   default:
                        ;
              }
          }

          /* write back to event */
          if (missing & DIEF_LOCKS)
               event->locks = shared->locks;

          /* store last pressed key */
          shared->last_key = event->key_id;
          shared->first_press = false;
     }
     else if (event->type == DIET_KEYRELEASE) {
          
          shared->first_press = true;
     }
}

static void
fixup_mouse_event( CoreInputDevice *device, DFBInputEvent *event )
{
     InputDeviceShared *shared = device->shared;

     D_MAGIC_ASSERT( device, CoreInputDevice );

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

     if (id >= DIKI_KP_0 && id <= DIKI_KP_9)
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

          case DIKI_KP_EQUAL:
               return DIKS_EQUALS_SIGN;

          case DIKI_KP_DECIMAL:
               return DIKS_PERIOD;

          case DIKI_KP_SEPARATOR:
               return DIKS_COMMA;

          case DIKI_BACKSLASH:
               return DIKS_BACKSLASH;

          case DIKI_EQUALS_SIGN:
               return DIKS_EQUALS_SIGN;

          case DIKI_LESS_SIGN:
               return DIKS_LESS_THAN_SIGN;

          case DIKI_MINUS_SIGN:
               return DIKS_MINUS_SIGN;

          case DIKI_PERIOD:
               return DIKS_PERIOD;

          case DIKI_QUOTE_LEFT:
          case DIKI_QUOTE_RIGHT:
               return DIKS_QUOTATION;

          case DIKI_SEMICOLON:
               return DIKS_SEMICOLON;

          case DIKI_SLASH:
               return DIKS_SLASH;

          default:
               ;
     }

     return DIKS_NULL;
}

static void
release_key( CoreInputDevice *device, DFBInputDeviceKeyIdentifier id )
{
     DFBInputEvent evt;

     D_MAGIC_ASSERT( device, CoreInputDevice );

     evt.type   = DIET_KEYRELEASE;
     evt.flags  = DIEF_KEYID;
     evt.key_id = id;

     dfb_input_dispatch( device, &evt );
}

static void
flush_keys( CoreInputDevice *device )
{
     D_MAGIC_ASSERT( device, CoreInputDevice );

     if (device->shared->modifiers_l) {
          if (device->shared->modifiers_l & DIMM_ALT)
               release_key( device, DIKI_ALT_L );

          if (device->shared->modifiers_l & DIMM_ALTGR)
               release_key( device, DIKI_ALTGR );

          if (device->shared->modifiers_l & DIMM_CONTROL)
               release_key( device, DIKI_CONTROL_L );

          if (device->shared->modifiers_l & DIMM_HYPER)
               release_key( device, DIKI_HYPER_L );

          if (device->shared->modifiers_l & DIMM_META)
               release_key( device, DIKI_META_L );

          if (device->shared->modifiers_l & DIMM_SHIFT)
               release_key( device, DIKI_SHIFT_L );

          if (device->shared->modifiers_l & DIMM_SUPER)
               release_key( device, DIKI_SUPER_L );
     }

     if (device->shared->modifiers_r) {
          if (device->shared->modifiers_r & DIMM_ALT)
               release_key( device, DIKI_ALT_R );

          if (device->shared->modifiers_r & DIMM_CONTROL)
               release_key( device, DIKI_CONTROL_R );

          if (device->shared->modifiers_r & DIMM_HYPER)
               release_key( device, DIKI_HYPER_R );

          if (device->shared->modifiers_r & DIMM_META)
               release_key( device, DIKI_META_R );

          if (device->shared->modifiers_r & DIMM_SHIFT)
               release_key( device, DIKI_SHIFT_R );

          if (device->shared->modifiers_r & DIMM_SUPER)
               release_key( device, DIKI_SUPER_R );
     }
}

static void
dump_primary_layer_surface()
{
     CoreLayer        *layer = dfb_layer_at( DLID_PRIMARY );
     CoreLayerContext *context;

     /* Get the currently active context. */
     if (dfb_layer_get_active_context( layer, &context ) == DFB_OK) {
          CoreLayerRegion *region;

          /* Get the first region. */
          if (dfb_layer_context_get_primary_region( context,
                                                    false, &region ) == DFB_OK)
          {
               CoreSurface *surface;

               /* Lock the region to avoid tearing due to concurrent updates. */
               dfb_layer_region_lock( region );

               /* Get the surface of the region. */
               if (dfb_layer_region_get_surface( region, &surface ) == DFB_OK) {
                    /* Dump the surface contents. */
                    dfb_surface_dump( surface,
                                      dfb_config->screenshot_dir, "dfb" );

                    /* Release the surface. */
                    dfb_surface_unref( surface );
               }

               /* Unlock the region. */
               dfb_layer_region_unlock( region );

               /* Release the region. */
               dfb_layer_region_unref( region );
          }

          /* Release the context. */
          dfb_layer_context_unref( context );
     }
}

static bool
core_input_filter( CoreInputDevice *device, DFBInputEvent *event )
{
     D_MAGIC_ASSERT( device, CoreInputDevice );

     if (dfb_system_input_filter( device, event ))
          return true;

     if (event->type == DIET_KEYPRESS) {
          switch (event->key_symbol) {
               case DIKS_PRINT:
                    if (!event->modifiers && dfb_config->screenshot_dir) {
                         dump_primary_layer_surface();
                         return true;
                    }
                    break;

               case DIKS_BACKSPACE:
                    if (event->modifiers == DIMM_META)
                         direct_trace_print_stacks();

                    break;

               case DIKS_ESCAPE:
                    if (event->modifiers == DIMM_META) {
#if FUSION_BUILD_MULTI
                         DFBResult         ret;
                         CoreLayer        *layer = dfb_layer_at( DLID_PRIMARY );
                         CoreLayerContext *context;

                         /* Get primary (shared) context. */
                         ret = dfb_layer_get_primary_context( layer,
                                                              false, &context );
                         if (ret)
                              return false;

                         /* Activate the context. */
                         dfb_layer_activate_context( layer, context );

                         /* Release the context. */
                         dfb_layer_context_unref( context );

#else
                         kill( 0, SIGINT );
#endif

                         return true;
                    }
                    break;

               default:
                    break;
          }
     }

     return false;
}

