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

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <directfb.h>


#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/input.h>
#include <core/system.h>

#include <direct/mem.h>
#include <direct/thread.h>

#include <Carbon/Carbon.h>

#include "osx.h"

#include <core/input_driver.h>


DFB_INPUT_DRIVER( osxinput )

/*
 * declaration of private data
 */
typedef struct {
     InputDevice  *device;
     DirectThread *thread;
     DFBOSX       *dfb_osx;
     int           stop;
} OSXInputData;

static bool
translate_key( char key, DFBInputEvent *evt )
{
     if ((key >= 'a') || (key <= 'z')) {
          evt->flags = DIEF_KEYID;
          evt->key_id = DIKI_A + key - 'a';
          return true;
     }
     if (key == 27) {
          evt->flags = DIEF_KEYID;
          evt->key_id = DIKI_ESCAPE;
          return true;
     }
     return false;
}

/*
 * Input thread reading from device.
 * Generates events on incoming data.
 */
static void*
osxEventThread( DirectThread *thread, void *driver_data )
{
     OSXInputData *data    = (OSXInputData*) driver_data;
     DFBOSX       *dfb_osx = data->dfb_osx;

     while (!data->stop) {
          DFBInputEvent evt;
          EventRecord   event;

          fusion_skirmish_prevail( &dfb_osx->lock );

          /* Check for events */
          while ( WaitNextEvent( everyEvent, &event, 0, nil) ) {
               fusion_skirmish_dismiss( &dfb_osx->lock );

               switch (event.what) {
                    case keyDown:
                    case keyUp:
                         if (event.what == keyDown)
                              evt.type = DIET_KEYPRESS;
                         else
                              evt.type = DIET_KEYRELEASE;

                         if (translate_key( event.message & charCodeMask, &evt )) {
                              dfb_input_dispatch( data->device, &evt );
                         }

                         break;

                    default:
                         break;
               }

               fusion_skirmish_prevail( &dfb_osx->lock );
          }

          fusion_skirmish_dismiss( &dfb_osx->lock );

          usleep(10000);

          direct_thread_testcancel( thread );
     }

     return NULL;
}

/* exported symbols */

/*
 * Return the number of available devices.
 * Called once during initialization of DirectFB.
 */
static int
driver_get_available()
{
     if (dfb_system_type() == CORE_OSX)
          return 1;

     return 0;
}

/*
 * Fill out general information about this driver.
 * Called once during initialization of DirectFB.
 */
static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf ( info->name,
                DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "OSX Input Driver" );
     snprintf ( info->vendor,
                DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "Denis Oliver Kropp" );

     info->version.major = 0;
     info->version.minor = 1;
}

/*
 * Open the device, fill out information about it,
 * allocate and fill private data, start input thread.
 * Called during initialization, resuming or taking over mastership.
 */
static DFBResult
driver_open_device( InputDevice      *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     OSXInputData *data;
     DFBOSX       *dfb_osx = dfb_system_data();

     fusion_skirmish_prevail( &dfb_osx->lock );

     fusion_skirmish_dismiss( &dfb_osx->lock );

     /* set device name */
     snprintf( info->desc.name,
               DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "OSX Input" );

     /* set device vendor */
     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "OSX" );

     /* set one of the primary input device IDs */
     info->prefered_id = DIDID_KEYBOARD;

     /* set type flags */
     info->desc.type   = DIDTF_KEYBOARD;

     /* set capabilities */
     info->desc.caps   = DICAPS_ALL;


     /* allocate and fill private data */
     data = D_CALLOC( 1, sizeof(OSXInputData) );

     data->device  = device;
     data->dfb_osx = dfb_osx;

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, osxEventThread, data, "OSX Input" );

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}

/*
 * Fetch one entry from the device's keymap if supported.
 */
static DFBResult
driver_get_keymap_entry( InputDevice               *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
     return DFB_UNSUPPORTED;
}

/*
 * End thread, close device and free private data.
 */
static void
driver_close_device( void *driver_data )
{
     OSXInputData *data = (OSXInputData*) driver_data;

     /* stop input thread */
     data->stop = 1;

     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     /* free private data */
     D_FREE ( data );
}
