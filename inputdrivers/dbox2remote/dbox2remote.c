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

#include <dbox/fp.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/input.h>
#include <core/thread.h>

#include <misc/mem.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( dbox2remote )

#define DEVICE "/dev/dbox/rc0"

typedef struct {
     DFBInputDeviceKeyIdentifier  key;
     __u16                        rccode;
} KeyCode;

/* REMOTE_NEW is the one with _fewer_ buttons */
static KeyCode keycodes_new_remote[] = {
     { DIKC_0, 0x0000 },
     { DIKC_1, 0x0001 },
     { DIKC_2, 0x0002 },
     { DIKC_3, 0x0003 },
     { DIKC_4, 0x0004 },
     { DIKC_5, 0x0005 },
     { DIKC_6, 0x0006 },
     { DIKC_7, 0x0007 },
     { DIKC_8, 0x0008 },
     { DIKC_9, 0x0009 },

     { DIKC_LEFT, 0x000b },
     { DIKC_RIGHT, 0x000a },
     { DIKC_UP, 0x000c },
     { DIKC_DOWN, 0x000d },
     /* FIXME: add support for right-up, right-down, left-down, left-up,
        they occur, too */

     { DIKC_RED, 0x0013 },
     { DIKC_GREEN, 0x0011 },
     { DIKC_YELLOW, 0x0012 },
     { DIKC_BLUE, 0x0014 },

     { DIKC_OK, 0x000e },
     { DIKC_HOME, 0x001F },
     { DIKC_VENDOR, 0x0018 },                     /* "d-box" key */
     { DIKC_POWER, 0x0010 },

     { DIKC_PAGEDOWN, 0x0053 },                   /* dbox1 only  */
     { DIKC_PAGEUP, 0x0054 },                     /* dbox1 only  */

     { DIKC_VOLUMEUP, 0x0015 },
     { DIKC_VOLUMEDOWN, 0x0016 },
     { DIKC_MUTE, 0x000f },
     { DIKC_INFO, 0x0017 },
     { DIKC_UNKNOWN, 0xFFFF }
};

static KeyCode keycodes_old_remote[] = {
     { DIKC_0, 0x5c00 },
     { DIKC_1, 0x5c01 },
     { DIKC_2, 0x5c02 },
     { DIKC_3, 0x5c03 },
     { DIKC_4, 0x5c04 },
     { DIKC_5, 0x5c05 },
     { DIKC_6, 0x5c06 },
     { DIKC_7, 0x5c07 },
     { DIKC_8, 0x5c08 },
     { DIKC_9, 0x5c09 },

     { DIKC_LEFT, 0x5c2f },
     { DIKC_RIGHT, 0x5c2e },
     { DIKC_UP, 0x5c0e },
     { DIKC_DOWN, 0x5c0f },

     { DIKC_RED, 0x5c2D },
     { DIKC_GREEN, 0x5c55 },
     { DIKC_YELLOW, 0x5c52 },
     { DIKC_BLUE, 0x5c3b },

     { DIKC_OK, 0x5c30 },
     { DIKC_HOME, 0x5c20 },                       /* radio key  */
     { DIKC_VENDOR, 0x5c27 },                     /* TV key     */
     { DIKC_POWER, 0x5c0c },

     { DIKC_PAGEDOWN, 0x5c53 },                   /* dbox1 only */
     { DIKC_PAGEUP, 0x5c54 },                     /* dbox1 only */

     { DIKC_VOLUMEUP, 0x5c16 },
     { DIKC_VOLUMEDOWN, 0x5c17 },
     { DIKC_MUTE, 0x5c28 },
     { DIKC_INFO, 0x5c82 },
     { DIKC_UNKNOWN, 0xFFFF }
};


/*
 * declaration of private data
 */
typedef struct {
     InputDevice *device;
     CoreThread  *thread;
     
     int          fd;
} Dbox2remoteData;


/*
 * helper function for translating rccode
 */
static DFBInputDeviceKeyIdentifier
dbox2remote_parse_rccode( __u16 rccode )
{
     KeyCode *keycode;

     if ((rccode & 0xff00) == 0x5c00) {
          keycode = keycodes_old_remote;
     }
     else {
          keycode = keycodes_new_remote;
          rccode &= 0x003f;
     }

     while (keycode->key != DIKC_UNKNOWN) {
          if (keycode->rccode == rccode) {
               return keycode->key;
          }
          keycode++;
     }

     return DIKC_UNKNOWN;
}

/*
 * Input thread reading from device.
 * Generates events on incoming data.
 */
static void*
dbox2remoteEventThread( CoreThread *thread, void *driver_data )
{
     Dbox2remoteData *data = (Dbox2remoteData*) driver_data;
     int              readlen;
     __u16            rccode;
     DFBInputEvent    evt;

     while ((readlen = read( data->fd, &rccode, 2 )) == 2) {
          dfb_thread_testcancel( thread );

          /* translate rccode to DirectFB keycode */
          evt.keycode = dbox2remote_parse_rccode( rccode );
          if (evt.keycode != DIKC_UNKNOWN) {
               /* set event type and dispatch*/
               evt.type = DIET_KEYPRESS;
               evt.flags = DIEF_KEYCODE;
               dfb_input_dispatch( data->device, &evt );

               /* set event type and dispatch*/
               evt.type = DIET_KEYRELEASE;
               evt.flags = DIEF_KEYCODE;
               dfb_input_dispatch( data->device, &evt );
          }
     }

     if (readlen <= 0 && errno != EINTR)
          PERRORMSG ("dbox2remote thread died\n");

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
     /* Check if we are able to read from device */
     if (access( DEVICE, R_OK ))
        return 0;

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
                DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "dbox2 remote" );
     snprintf ( info->vendor,
                DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "fischlustig" );

     info->version.major = 0;
     info->version.minor = 9;
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
     int                 fd;
     Dbox2remoteData    *data;

     /* open device */
     fd = open( DEVICE, O_RDONLY);
     if (fd < 0) {
          PERRORMSG( "DirectFB/dbox2remote: could not open device" );
          return DFB_INIT;
     }

     /* apply voodoo */
     ioctl( fd, RC_IOCTL_BCODES, 0 );


     /* set device name */
     snprintf( info->name,
               DFB_INPUT_DEVICE_INFO_NAME_LENGTH, "dbox2 remote control" );

     /* set device vendor */
     snprintf( info->vendor,
               DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH, "nokia/sagem/philips" );

     /* set one of the primary input device IDs */
     info->prefered_id = DIDID_REMOTE;

     /* set type flags */
     info->desc.type   = DIDTF_REMOTE;

     /* set capabilities */
     info->desc.caps   = DICAPS_KEYS;


     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(Dbox2remoteData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     data->thread = dfb_thread_create( CTT_INPUT,
                                       dbox2remoteEventThread, data );

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
     Dbox2remoteData *data = (Dbox2remoteData*) driver_data;

     /* stop input thread */
     dfb_thread_cancel( data->thread );
     dfb_thread_join( data->thread );
     dfb_thread_destroy( data->thread );

     /* close file */
     close( data->fd );

     /* free private data */
     DFBFREE ( data );
}

