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
     CoreInputDevice  *device;
     DirectThread *thread;
     DFBOSX       *dfb_osx;
     int           stop;
} OSXInputData;

static DFBInputEvent motionX = {
     type:     DIET_UNKNOWN,
     axisabs:  0
};

static DFBInputEvent motionY = {
     type:     DIET_UNKNOWN,
     axisabs:  0
};

static void
motion_compress( int x, int y )
{
     if (motionX.axisabs != x) {
          motionX.type    = DIET_AXISMOTION;
          motionX.flags   = DIEF_AXISABS;
          motionX.axis    = DIAI_X;
          motionX.axisabs = x;
     }

    if (motionY.axisabs != y) {
          motionY.type    = DIET_AXISMOTION;
          motionY.flags   = DIEF_AXISABS;
          motionY.axis    = DIAI_Y;
          motionY.axisabs = y;
     }
}

static void
motion_realize( OSXInputData *data )
{
     if (motionX.type != DIET_UNKNOWN) {
          dfb_input_dispatch( data->device, &motionX );

          motionX.type = DIET_UNKNOWN;
     }

     if (motionY.type != DIET_UNKNOWN) {
          dfb_input_dispatch( data->device, &motionY );

          motionY.type = DIET_UNKNOWN;
     }
}


static bool
translate_key( unsigned short key, DFBInputEvent *evt )
{
     unsigned char charcode = (unsigned char)key;
     unsigned char keycode  = (unsigned char)(key>>8);

     printf("keycode: %d char: %d\n",keycode,charcode);

     if (charcode) {
          evt->flags = DIEF_KEYSYMBOL;
          switch (charcode) {
               case 28:  evt->key_symbol = DIKS_CURSOR_LEFT; break;
               case 29:  evt->key_symbol = DIKS_CURSOR_RIGHT; break;
               case 30:  evt->key_symbol = DIKS_CURSOR_UP; break;
               case 31:  evt->key_symbol = DIKS_CURSOR_DOWN; break;
               default:
                   evt->key_symbol = charcode;
                   break;
          }
          return true;
     }
     else if (keycode) {
          evt->flags = DIEF_KEYID;
          evt->key_id = keycode;
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
                    case autoKey:
                         if (event.what == keyUp)
                              evt.type = DIET_KEYRELEASE;
                         else
                              evt.type = DIET_KEYPRESS;

                         if (translate_key( event.message & (charCodeMask | keyCodeMask), &evt )) {
                              dfb_input_dispatch( data->device, &evt );
                         }

                         break;
                    case mouseDown:
                         evt.type = DIET_BUTTONPRESS;
                         evt.button = DIBI_LEFT;
                         dfb_input_dispatch( data->device, &evt );
                         break;
                    case mouseUp:
                         evt.type = DIET_BUTTONRELEASE;
                         evt.button = DIBI_LEFT;
                         dfb_input_dispatch( data->device, &evt );
                         break;
                    default:
                         printf("%d\n",event.what);
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
                DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "Andreas Hundt" );

     info->version.major = 0;
     info->version.minor = 1;
}

/*
 * Open the device, fill out information about it,
 * allocate and fill private data, start input thread.
 * Called during initialization, resuming or taking over mastership.
 */
static DFBResult
driver_open_device( CoreInputDevice      *device,
                    unsigned int      number,
                    CoreInputDeviceInfo  *info,
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
     info->desc.type   = DIDTF_KEYBOARD | DIDTF_MOUSE;

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
driver_get_keymap_entry( CoreInputDevice               *device,
                         void                      *driver_data,
                         DFBCoreInputDeviceKeymapEntry *entry )
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
