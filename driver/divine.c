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
#include <sys/poll.h>

#include <fcntl.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/input.h>
#include <core/thread.h>

#include <misc/mem.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( divine )

#define PIPE_PATH "/tmp/divine"

/*
 * declaration of private data
 */
typedef struct {
     int          fd;
     InputDevice *device;
     CoreThread  *thread;
} DiVineData;


/*
 * Input thread reading from pipe.
 * Directly passes read events to input core.
 */
static void*
divineEventThread( CoreThread *thread, void *driver_data )
{
     DiVineData     *data = (DiVineData*) driver_data;
     DFBInputEvent   event;
     struct pollfd   pfd;

     /* fill poll info */
     pfd.fd     = data->fd;
     pfd.events = POLLIN;

     /* wait for the next event */
     while (poll( &pfd, 1, -1 ) > 0 || errno == EINTR) {
          dfb_thread_testcancel( thread );

          /* read the next event from the pipe */
          if (read( data->fd, &event, sizeof(DFBInputEvent) ) == sizeof(DFBInputEvent)) {
               /* directly dispatch the event */
               dfb_input_dispatch( data->device, &event );
          } else
               usleep( 20000 ); /* avoid 100% CPU usage in case poll() doesn't work */
     }

     PERRORMSG( "divine thread died\n" );

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
     int fd;

     /* create the pipe if not already existent */
     if (mkfifo( PIPE_PATH, 0660 ) && errno != EEXIST) {
          PERRORMSG( "DirectFB/DiVine: could not create pipe '%s'\n", PIPE_PATH );
          return 0;
     }

     /* try to open pipe */
     fd = open( PIPE_PATH, O_RDONLY | O_NONBLOCK );
     if (fd < 0) {
          PERRORMSG( "DirectFB/DiVine: could not open pipe '%s'\n", PIPE_PATH );
          return 0;
     }

     /* close pipe */
     close( fd );

     return 1;
}

/*
 * Fill out general information about this driver.
 * Called once during initialization of DirectFB.
 */
static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "DiVine Driver" );
     snprintf( info->vendor,
               DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "Convergence GmbH" );

     info->version.major = 0;
     info->version.minor = 1;
}

/*
 * Open the pipe, fill out information about device,
 * allocate and fill private data, start input thread.
 * Called during initialization, resuming or taking over mastership.
 */
static DFBResult
driver_open_device( InputDevice      *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     int         fd;
     DiVineData *data;

     /* open pipe */
     fd = open( PIPE_PATH, O_RDONLY | O_NONBLOCK );
     if (fd < 0) {
          PERRORMSG( "DirectFB/DiVine: could not open pipe '%s'\n", PIPE_PATH );
          return DFB_INIT;
     }

     /* set device name */
     snprintf( info->desc.name,
               DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "Virtual Input" );

     /* set device vendor */
     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "DirectFB" );

     /* set one of the primary input device IDs */
     info->prefered_id = DIDID_ANY;

     /* set type flags */
     info->desc.type = DIDTF_KEYBOARD | DIDTF_MOUSE |
                       DIDTF_JOYSTICK | DIDTF_REMOTE | DIDTF_VIRTUAL;

     /* set capabilities */
     info->desc.caps = DICAPS_ALL;


     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(DiVineData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     data->thread = dfb_thread_create( CTT_INPUT, divineEventThread, data );

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
     DiVineData *data = (DiVineData*) driver_data;

     /* stop input thread */
     dfb_thread_cancel( data->thread );
     dfb_thread_join( data->thread );
     dfb_thread_destroy( data->thread );

     /* close pipe */
     close( data->fd );

     /* free private data */
     DFBFREE( data );
}
