/*
   (c) Copyright 2005  Marcel Siegert

   All rights reserved.

   Written by Marcel Siegert <mws@directfb.org>

   Mainly based on dbox2remote:

   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

DFB_INPUT_DRIVER( dreamboxremote )

#define DEVICE "/dev/rawir2"

typedef struct {
     DFBInputDeviceKeySymbol  key;
     u16                      rccode;
} KeyCode;

static KeyCode keycodes_remote[] = {
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
     { DIKS_VOLUME_UP, 0x000a },
     { DIKS_VOLUME_DOWN, 0x000b },
     { DIKS_SELECT, 0x000c }, // DM500 Mute
// TODO find appropriate DIKS codes 
/*
     { DIKS_TV??, 0x000c }, // TV
     { DIKS_, 0x000d }, // bouquet up
     { DIKS_, 0x000e }, // bouquet down
     { DIKS_, 0x000f }, // power on/off standby
*/
     { DIKS_MENU, 0x0020 }, // DREAM 
     { DIKS_CURSOR_UP, 0x0021 },
     { DIKS_CURSOR_DOWN, 0x0022 },
     { DIKS_CURSOR_LEFT, 0x0023 },
     { DIKS_CURSOR_RIGHT, 0x0024 },
     { DIKS_OK, 0x0025 },
// TODO find appropriate DIKS codes 
//     { DIKS_, 0x0026 }, // audio
//     { DIKS_, 0x0027 }, // video
     { DIKS_INFO, 0x0028 },
     { DIKS_RED, 0x0040 },
     { DIKS_GREEN, 0x0041 },
     { DIKS_YELLOW, 0x0042 },
     { DIKS_BLUE, 0x0043 },
     { DIKS_MUTE, 0x0044 },
// TODO find appropriate DIKS codes 
//     { DIKS_, 0x0045 }, // text
//     { DIKS_, 0x0050 }, // forward
//     { DIKS_, 0x0051 }, // back
     { DIKS_HOME, 0x0052 }, // lame
     { DIKS_SLOW, 0x0054 }, // dm500 lame
//     { DIKS_, 0x0053 }, // text
//     { DIKS_, 0x0054 }, // help

     { DIKS_NULL, 0xFFFF }
};


/*
 * declaration of private data
 */
typedef struct {
     CoreInputDevice  *device;
     DirectThread *thread;

     int           fd;
} DreamboxremoteData;


/*
 * helper function for translating rccode
 */
static DFBInputDeviceKeySymbol
dreamboxremote_parse_rccode( u16 rccode )
{
	KeyCode *keycode;
	static  u16 rccodeOld = 0;

	keycode = keycodes_remote;
	/* 0x00ff indicates key was released
	   so reset rccodeOld and do nothing
	*/
	if ( rccode == 0x00ff ) {
		rccodeOld = 0;
		return DIKS_NULL;
	}
	/* check for a new keycode
	   the drivers return msb clear if a key is
	   pressed - msb set if pressed
	   ignore key down - otherwise 2 events are 
	   generated for one press
	*/
	rccode &= 0x7fff;
	if (rccodeOld != rccode ) {
		rccodeOld = rccode;
		return DIKS_NULL;
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
dreamboxremoteEventThread( DirectThread *thread, void *driver_data )
{
     DreamboxremoteData *data = (DreamboxremoteData*) driver_data;
     int              readlen;
     u16              rccode;
     DFBInputEvent    evt;

     while ((readlen = read( data->fd, &rccode, 2 )) == 2) {
          direct_thread_testcancel( thread );

          /* translate rccode to DirectFB keycode */
          evt.key_symbol = dreamboxremote_parse_rccode( rccode );
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
          D_PERROR ("dreamboxremote thread died\n");

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
                DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "dreambox remote" );
     snprintf ( info->vendor,
                DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "mws" );

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
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     int                 fd;
     DreamboxremoteData    *data;

     /* open device */
     fd = open( DEVICE, O_RDONLY);
     if (fd < 0) {
          D_PERROR( "DirectFB/dreamboxremote: could not open device" );
          return DFB_INIT;
     }

     /* apply voodoo */
     //ioctl( fd, RC_IOCTL_BCODES, 0 );


     /* set device name */
     snprintf( info->desc.name,
               DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "dreambox remote control" );

     /* set device vendor */
     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "DM7000/DM56XX/DM500" );

     /* set one of the primary input device IDs */
     info->prefered_id = DIDID_REMOTE;

     /* set type flags */
     info->desc.type   = DIDTF_REMOTE;

     /* set capabilities */
     info->desc.caps   = DICAPS_KEYS;


     /* allocate and fill private data */
     data = D_CALLOC( 1, sizeof(DreamboxremoteData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, dreamboxremoteEventThread, data, "DreamBox Input" );

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
     DreamboxremoteData *data = (DreamboxremoteData*) driver_data;

     /* stop input thread */
     direct_thread_cancel( data->thread );
     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     /* close file */
     close( data->fd );

     /* free private data */
     D_FREE ( data );
}
