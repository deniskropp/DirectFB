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

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <fcntl.h>

#include <directfb.h>

#include <SDL.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/input.h>
#include <core/system.h>
#include <core/thread.h>

#include <misc/mem.h>

#include "sdl.h"

#include <core/input_driver.h>


DFB_INPUT_DRIVER( sdlinput )

/*
 * declaration of private data
 */
typedef struct {
     InputDevice *device;
     CoreThread  *thread;
} SDLInputData;


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
motion_realize( SDLInputData *data )
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

/*
 * Input thread reading from device.
 * Generates events on incoming data.
 */
static void*
sdlEventThread( CoreThread *thread, void *driver_data )
{
     SDLInputData *data = (SDLInputData*) driver_data;

     while (true) {
          DFBInputEvent evt;
          SDL_Event     event;

          skirmish_prevail( &dfb_sdl_lock );
          
          /* Check for events */
          while ( SDL_PollEvent(&event) ) {
               skirmish_dismiss( &dfb_sdl_lock );
               
               switch (event.type) {
                    case SDL_MOUSEMOTION:
                         motion_compress( event.motion.x, event.motion.y );
                         break;

                    case SDL_MOUSEBUTTONUP:
                    case SDL_MOUSEBUTTONDOWN:
                         if (event.type == SDL_MOUSEBUTTONDOWN)
                              evt.type = DIET_BUTTONPRESS;
                         else
                              evt.type = DIET_BUTTONRELEASE;

                         evt.flags = DIEF_NONE;

                         switch (event.button.button) {
                              case SDL_BUTTON_LEFT:
                                   evt.button = DIBI_LEFT;
                                   break;
                              case SDL_BUTTON_MIDDLE:
                                   evt.button = DIBI_MIDDLE;
                                   break;
                              case SDL_BUTTON_RIGHT:
                                   evt.button = DIBI_RIGHT;
                                   break;
                              default:
                                   skirmish_prevail( &dfb_sdl_lock );
                                   continue;
                         }

                         dfb_input_dispatch( data->device, &evt );
                         break;
                    
                    case SDL_KEYUP:
                    case SDL_KEYDOWN:
                         motion_realize( data );
                         
                         if (event.type == SDL_KEYDOWN)
                              evt.type = DIET_KEYPRESS;
                         else
                              evt.type = DIET_KEYRELEASE;

                         evt.flags = DIEF_KEYSYMBOL;

                         if (event.key.keysym.unicode) {
                              evt.key_symbol = event.key.keysym.unicode;

                              dfb_input_dispatch( data->device, &evt );
                         }

                         break;
                    
                    case SDL_QUIT:
                         evt.type       = DIET_KEYPRESS;
                         evt.flags      = DIEF_KEYSYMBOL;
                         evt.key_symbol = DIKS_ESCAPE;
                         
                         dfb_input_dispatch( data->device, &evt );
                         
                         evt.type       = DIET_KEYRELEASE;
                         evt.flags      = DIEF_KEYSYMBOL;
                         evt.key_symbol = DIKS_ESCAPE;
                         
                         dfb_input_dispatch( data->device, &evt );
                         break;
                    
                    default:
                         break;
               }
          }

          skirmish_dismiss( &dfb_sdl_lock );
          
          motion_realize( data );

          usleep(20000);
     
          dfb_thread_testcancel( thread );
     }
     
     return NULL;
}

/* exported symbols */

/*
 * Return the version number of the 'binary interface'.
 * Called immediately after input module has been opened
 * and this function has been found.
 * Input drivers with differing compiled in version won't be used.
 */
static int
driver_get_abi_version()
{
     return DFB_INPUT_DRIVER_ABI_VERSION;
}

/*
 * Return the number of available devices.
 * Called once during initialization of DirectFB.
 */
static int
driver_get_available()
{
     if (dfb_system_type() == CORE_SDL)
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
                DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "SDL Input Driver" );
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
     SDLInputData *data;

     skirmish_prevail( &dfb_sdl_lock );
     
     SDL_EnableUNICODE( true );

     skirmish_dismiss( &dfb_sdl_lock );
     
     /* set device name */
     snprintf( info->name,
               DFB_INPUT_DEVICE_INFO_NAME_LENGTH, "SDL Input" );

     /* set device vendor */
     snprintf( info->vendor,
               DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH, "SDL" );

     /* set one of the primary input device IDs */
     info->prefered_id = DIDID_KEYBOARD;

     /* set type flags */
     info->desc.type   = DIDTF_JOYSTICK | DIDTF_KEYBOARD | DIDTF_MOUSE;

     /* set capabilities */
     info->desc.caps   = DICAPS_ALL;


     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(SDLInputData) );

     data->device = device;

     /* start input thread */
     data->thread = dfb_thread_create( CTT_INPUT, sdlEventThread, data );

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
     SDLInputData *data = (SDLInputData*) driver_data;

     /* stop input thread */
     dfb_thread_cancel( data->thread );
     dfb_thread_join( data->thread );
     dfb_thread_destroy( data->thread );

     /* free private data */
     DFBFREE ( data );
}

