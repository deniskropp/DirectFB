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

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( dbox2remote )

#define DEVICE "/dev/dbox/rc0"

typedef struct {
     DFBInputDeviceKeySymbol  key;
     __u16                    rccode;
} KeyCode;

/* REMOTE_NEW is the one with _fewer_ buttons */
static KeyCode keycodes_new_remote[] = {
     { DIKS_0, 0x0000 },
     { DIKS_1, 0x0001 },
     { DIKS_2, 0x0002 },
     { DIKS_3, 0x0003 },
     { DIKS_4, 0x0004 },
     { DIKS_5, 0x0005 },
     { DIKS_6, 0x0006 },
     { DIKS_7, 0x0007 },
     { DIKS_8, 0x0008 },
     { DIKS_9, 0x0009 },

     { DIKS_CURSOR_LEFT, 0x000b },
     { DIKS_CURSOR_RIGHT, 0x000a },
     { DIKS_CURSOR_UP, 0x000c },
     { DIKS_CURSOR_DOWN, 0x000d },
     /* FIXME: add support for right-up, right-down, left-down, left-up,
        they occur, too */

     { DIKS_RED, 0x0013 },
     { DIKS_GREEN, 0x0011 },
     { DIKS_YELLOW, 0x0012 },
     { DIKS_BLUE, 0x0014 },

     { DIKS_OK, 0x000e },
     { DIKS_HOME, 0x001F },
     { DIKS_VENDOR, 0x0018 },                     /* "d-box" key */
     { DIKS_POWER, 0x0010 },

     { DIKS_PAGE_DOWN, 0x0053 },                   /* dbox1 only  */
     { DIKS_PAGE_UP, 0x0054 },                     /* dbox1 only  */

     { DIKS_VOLUME_UP, 0x0015 },
     { DIKS_VOLUME_DOWN, 0x0016 },
     { DIKS_MUTE, 0x000f },
     { DIKS_INFO, 0x0017 },
     { DIKS_NULL, 0xFFFF }
};

static KeyCode keycodes_old_remote[] = {
     { DIKS_0, 0x5c00 },
     { DIKS_1, 0x5c01 },
     { DIKS_2, 0x5c02 },
     { DIKS_3, 0x5c03 },
     { DIKS_4, 0x5c04 },
     { DIKS_5, 0x5c05 },
     { DIKS_6, 0x5c06 },
     { DIKS_7, 0x5c07 },
     { DIKS_8, 0x5c08 },
     { DIKS_9, 0x5c09 },

     { DIKS_CURSOR_LEFT, 0x5c2f },
     { DIKS_CURSOR_RIGHT, 0x5c2e },
     { DIKS_CURSOR_UP, 0x5c0e },
     { DIKS_CURSOR_DOWN, 0x5c0f },

     { DIKS_RED, 0x5c2D },
     { DIKS_GREEN, 0x5c55 },
     { DIKS_YELLOW, 0x5c52 },
     { DIKS_BLUE, 0x5c3b },

     { DIKS_OK, 0x5c30 },
     { DIKS_HOME, 0x5c20 },                       /* radio key  */
     { DIKS_VENDOR, 0x5c27 },                     /* TV key     */
     { DIKS_POWER, 0x5c0c },

     { DIKS_PAGE_DOWN, 0x5c53 },                   /* dbox1 only */
     { DIKS_PAGE_UP, 0x5c54 },                     /* dbox1 only */

     { DIKS_VOLUME_UP, 0x5c16 },
     { DIKS_VOLUME_DOWN, 0x5c17 },
     { DIKS_MUTE, 0x5c28 },
     { DIKS_INFO, 0x5c82 },
     { DIKS_NULL, 0xFFFF }
};


/*
 * declaration of private data
 */
typedef struct {
     InputDevice  *device;
     DirectThread *thread;

     int           fd;
} Dbox2remoteData;


/*
 * helper function for translating rccode
 */
static DFBInputDeviceKeySymbol
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

     while (keycode->key != DIKS_NULL) {
          if (keycode->rccode == rccode) {
               return keycode->key;
          }
          keycode++;
     }

     return DIKS_NULL;
}

/*
 * Input thread reading from device.
 * Generates events on incoming data.
 */
static void*
dbox2remoteEventThread( DirectThread *thread, void *driver_data )
{
     Dbox2remoteData *data = (Dbox2remoteData*) driver_data;
     int              readlen;
     __u16            rccode;
     DFBInputEvent    evt;

     while ((readlen = read( data->fd, &rccode, 2 )) == 2) {
          direct_thread_testcancel( thread );

          /* translate rccode to DirectFB keycode */
          evt.key_symbol = dbox2remote_parse_rccode( rccode );
          if (evt.key_symbol != DIKS_NULL) {
               /* set event type and dispatch*/
               evt.type = DIET_KEYPRESS;
               evt.flags = DIEF_KEYSYMBOL;
               dfb_input_dispatch( data->device, &evt );

               /* set event type and dispatch*/
               evt.type = DIET_KEYRELEASE;
               evt.flags = DIEF_KEYSYMBOL;
               dfb_input_dispatch( data->device, &evt );
          }
     }

     if (readlen <= 0 && errno != EINTR)
          D_PERROR ("dbox2remote thread died\n");

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
          D_PERROR( "DirectFB/dbox2remote: could not open device" );
          return DFB_INIT;
     }

     /* apply voodoo */
     ioctl( fd, RC_IOCTL_BCODES, 0 );


     /* set device name */
     snprintf( info->desc.name,
               DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "dbox2 remote control" );

     /* set device vendor */
     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "nokia/sagem/philips" );

     /* set one of the primary input device IDs */
     info->prefered_id = DIDID_REMOTE;

     /* set type flags */
     info->desc.type   = DIDTF_REMOTE;

     /* set capabilities */
     info->desc.caps   = DICAPS_KEYS;


     /* allocate and fill private data */
     data = D_CALLOC( 1, sizeof(Dbox2remoteData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, dbox2remoteEventThread, data, "DBOX2 Input" );

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
     direct_thread_cancel( data->thread );
     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     /* close file */
     close( data->fd );

     /* free private data */
     D_FREE ( data );
}

