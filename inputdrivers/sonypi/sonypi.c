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

#include <linux/sonypi.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/input.h>
#include <core/thread.h>

#include <misc/mem.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( sonypi )

#define DEVICE "/dev/sonypi"


/*
 * declaration of private data
 */
typedef struct {
     InputDevice *device;
     CoreThread  *thread;
     
     int          fd;
} SonypiData;


/*
 * Input thread reading from device.
 * Generates events on incoming data.
 */
static void*
sonypiEventThread( CoreThread *thread, void *driver_data )
{
     SonypiData    *data = (SonypiData*) driver_data;
     int            readlen;
     __u8           buffer[16];

     /* loop until error occurs except EINTR */
     while ((readlen = read( data->fd, buffer, 16 )) > 0 || errno == EINTR) {
          int           i;
          DFBInputEvent evt;

          dfb_thread_testcancel( thread );

          /* process each byte */
          for (i=0; i<readlen; i++) {

               /* check for jogdial events */
               switch (buffer[i]) {
                    case SONYPI_EVENT_JOGDIAL_DOWN:
                    case SONYPI_EVENT_JOGDIAL_UP:
                    case SONYPI_EVENT_JOGDIAL_DOWN_PRESSED:
                    case SONYPI_EVENT_JOGDIAL_UP_PRESSED:
                         evt.type  = DIET_AXISMOTION;
                         evt.axis  = DIAI_Z;
                         evt.flags = DIEF_AXISREL;

                         if (buffer[i] == SONYPI_EVENT_JOGDIAL_DOWN ||
                             buffer[i] == SONYPI_EVENT_JOGDIAL_DOWN_PRESSED)
                              evt.axisrel = 1;
                         else
                              evt.axisrel = -1;

                         dfb_input_dispatch( data->device, &evt );
                         break;

                    case SONYPI_EVENT_JOGDIAL_PRESSED:
                    case SONYPI_EVENT_JOGDIAL_RELEASED:
                         if (buffer[i] == SONYPI_EVENT_JOGDIAL_PRESSED)
                              evt.type = DIET_BUTTONPRESS;
                         else
                              evt.type = DIET_BUTTONRELEASE;
                         
                         evt.flags  = DIEF_NONE; /* button is always valid */
                         evt.button = DIBI_MIDDLE;

                         dfb_input_dispatch( data->device, &evt );
                         break;

                    default:
                         ;
               }
          }
     }

     if (readlen < 0)
          PERRORMSG ("sonypi thread died\n");

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
     int fd;

     /* Check if we are able to open device for reading */
     if ((fd = open( DEVICE, O_RDONLY )) < 0)
        return 0;

     close(fd);

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
     snprintf ( info->name,
                DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "SonyPI Jogdial Driver" );
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
     int         fd;
     SonypiData *data;

     /* open device */
     fd = open( DEVICE, O_RDONLY);
     if (fd < 0) {
          PERRORMSG( "DirectFB/sonypi: could not open device" );
          return DFB_INIT;
     }

     /* set device name */
     snprintf( info->name,
               DFB_INPUT_DEVICE_INFO_NAME_LENGTH, "PI Jogdial" );

     /* set device vendor */
     snprintf( info->vendor,
               DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH, "Sony" );

     /* set one of the primary input device IDs */
     info->prefered_id = DIDID_MOUSE;

     /* set type flags */
     info->desc.type   = DIDTF_MOUSE;

     /* set capabilities */
     info->desc.caps       = DICAPS_BUTTONS | DICAPS_AXES;
     info->desc.max_axis   = DIAI_Z;
     info->desc.max_button = DIBI_MIDDLE;


     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(SonypiData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     data->thread = dfb_thread_create( CTT_INPUT, sonypiEventThread, data );

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
     SonypiData *data = (SonypiData*) driver_data;

     /* stop input thread */
     dfb_thread_cancel( data->thread );
     dfb_thread_join( data->thread );
     dfb_thread_destroy( data->thread );

     /* close file */
     close( data->fd );

     /* free private data */
     DFBFREE ( data );
}

