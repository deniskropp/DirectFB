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
#include <errno.h>

#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/vt.h>

#include <linux/joystick.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <misc/mem.h>

#include <core/input.h>
#include <core/thread.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( joystick )

typedef struct {
     InputDevice *device;
     CoreThread  *thread;

     int          fd;
} JoystickData;


static DFBInputEvent joystick_handle_event(struct js_event jse)
{
     DFBInputEvent event;

     switch (jse.type) {
          case JS_EVENT_BUTTON:
               event.type    = (jse.value ?
                                DIET_BUTTONPRESS : DIET_BUTTONRELEASE);
               event.flags   = DIEF_NONE; /* button is always valid */
               event.button  = jse.number;
               break;
          case JS_EVENT_AXIS:
               event.type    = DIET_AXISMOTION;
               event.flags   = DIEF_AXISABS;
               event.axis    = jse.number;
               event.axisabs = jse.value;
               break;
          default:
               PERRORMSG ("unknown joystick event type\n");
     }

     return event;
}

static void*
joystickEventThread( CoreThread *thread, void *driver_data )
{
     int              len;
     struct js_event  jse;
     JoystickData    *data = (JoystickData*) driver_data;

     while ((len = read( data->fd, &jse,
                         sizeof(struct js_event) )) > 0 || errno == EINTR)
     {
          DFBInputEvent evt;

          dfb_thread_testcancel( thread );

          if (len != sizeof(struct js_event))
               continue;

          evt = joystick_handle_event( jse );

          dfb_input_dispatch( data->device, &evt );
     }

     if (len <= 0 && errno != EINTR)
          PERRORMSG ("joystick thread died\n");

     return NULL;
}

/* exported symbols */

static int
driver_get_abi_version()
{
     return DFB_INPUT_DRIVER_ABI_VERSION;
}

static int
driver_get_available()
{
     int  i, fd;
     int  joy_count = 0;
     char devicename[20];

     for (i=0; i<8; i++) {
          snprintf( devicename, 20, "/dev/js%d", i );

          fd = open( devicename, O_RDONLY );
          if (fd < 0)
               break;

          close (fd);

          joy_count++;
     }

     if (!joy_count) {
          /* try new-style device names */

          for (i=0; i<8; i++) {
               snprintf( devicename, 20, "/dev/input/js%d", i );

               fd = open( devicename, O_RDONLY );
               if (fd < 0)
                    break;

               close (fd);

               joy_count++;
          }
     }

     return joy_count;
}

static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "Joystick Driver" );

     snprintf( info->vendor,
               DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 9;
}

static DFBResult
driver_open_device( InputDevice      *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     int           fd, buttons, axes;
     JoystickData *data;
     char          devicename[20];

     /* open the right device */
     snprintf( devicename, 20, "/dev/js%d", number );

     fd = open( devicename, O_RDONLY );
     if (fd < 0) {
          /* try new-style device names */
          snprintf( devicename, 20, "/dev/input/js%d", number );

          fd = open( devicename, O_RDONLY );
          if (fd < 0) {
               PERRORMSG( "DirectFB/Joystick: Could not open `%s'!\n",
                          devicename );
               return DFB_INIT; /* no joystick available */
          }
     }

     /* query number of buttons and axes */
     ioctl( fd, JSIOCGBUTTONS, &buttons );
     ioctl( fd, JSIOCGAXES, &axes );

     /* fill device info structure */
     snprintf( info->name,
               DFB_INPUT_DEVICE_INFO_NAME_LENGTH, "Joystick" );

     snprintf( info->vendor,
               DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH, "Unknown" );

     info->prefered_id     = DIDID_JOYSTICK;

     info->desc.type       = DIDTF_JOYSTICK;
     info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
     info->desc.max_button = buttons - 1;
     info->desc.max_axis   = axes - 1;


     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(JoystickData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     data->thread = dfb_thread_create( CTT_INPUT, joystickEventThread, data );

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

static void
driver_close_device( void *driver_data )
{
     JoystickData *data = (JoystickData*) driver_data;

     /* stop input thread */
     dfb_thread_cancel( data->thread );
     dfb_thread_join( data->thread );
     dfb_thread_destroy( data->thread );

     /* close device */
     close( data->fd );

     /* free private data */
     DFBFREE( data );
}

