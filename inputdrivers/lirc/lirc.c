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
#include <sys/socket.h>
#include <sys/un.h>

#include <pthread.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/sig.h>

#include <misc/mem.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( lirc )

typedef struct {
     DFBInputDeviceKeySymbol  symbol;
     char                    *name;
} KeySymbolString;

static const KeySymbolString keysymbol_strings[] = {
     { DIKS_F1,   "F1" },
     { DIKS_F2,   "F2" },
     { DIKS_F3,   "F3" },
     { DIKS_F4,   "F4" },
     { DIKS_F5,   "F5" },
     { DIKS_F6,   "F6" },
     { DIKS_F7,   "F7" },
     { DIKS_F8,   "F8" },
     { DIKS_F9,   "F9" },
     { DIKS_F10, "F10" },
     { DIKS_F11, "F11" },
     { DIKS_F12, "F12" },

     { DIKS_ESCAPE, "ESCAPE" },
     { DIKS_CURSOR_LEFT, "CURSOR_LEFT" },
     { DIKS_CURSOR_RIGHT, "CURSOR_RIGHT" },
     { DIKS_CURSOR_UP, "CURSOR_UP" },
     { DIKS_CURSOR_DOWN, "CURSOR_DOWN" },
     { DIKS_CONTROL, "CONTROL" },
     { DIKS_SHIFT, "SHIFT" },
     { DIKS_ALT, "ALT" },
     { DIKS_ALTGR, "ALTGR" },
     { DIKS_TAB, "TAB" },
     { DIKS_ENTER, "ENTER" },
     { DIKS_SPACE, "SPACE" },
     { DIKS_BACKSPACE, "BACKSPACE" },
     { DIKS_INSERT, "INSERT" },
     { DIKS_DELETE, "DELETE" },
     { DIKS_HOME, "HOME" },
     { DIKS_END, "END" },
     { DIKS_PAGEUP, "PAGEUP" },
     { DIKS_PAGEDOWN, "PAGEDOWN" },
     { DIKS_CAPSLOCK, "CAPSLOCK" },
     { DIKS_NUMLOCK, "NUMLOCK" },
     { DIKS_SCROLLLOCK, "SCROLLLOCK" },
     { DIKS_PRINT, "PRINT" },
     { DIKS_PAUSE, "PAUSE" },
     { DIKS_KP_DIV, "KP_DIV" },
     { DIKS_KP_MULT, "KP_MULT" },
     { DIKS_KP_MINUS, "KP_MINUS" },
     { DIKS_KP_PLUS, "KP_PLUS" },
     { DIKS_KP_ENTER, "KP_ENTER" },

     { DIKS_OK, "OK" },
     { DIKS_CANCEL, "CANCEL" },
     { DIKS_SELECT, "SELECT" },
     { DIKS_GOTO, "GOTO" },
     { DIKS_CLEAR, "CLEAR" },
     { DIKS_POWER, "POWER" },
     { DIKS_POWER2, "POWER2" },
     { DIKS_OPTION, "OPTION" },
     { DIKS_MENU, "MENU" },
     { DIKS_HELP, "HELP" },
     { DIKS_INFO, "INFO" },
     { DIKS_TIME, "TIME" },
     { DIKS_VENDOR, "VENDOR" },

     { DIKS_ARCHIVE, "ARCHIVE" },
     { DIKS_PROGRAM, "PROGRAM" },
     { DIKS_FAVORITES, "FAVORITES" },
     { DIKS_EPG, "EPG" },
     { DIKS_LANGUAGE, "LANGUAGE" },
     { DIKS_TITLE, "TITLE" },
     { DIKS_SUBTITLE, "SUBTITLE" },
     { DIKS_ANGLE, "ANGLE" },
     { DIKS_ZOOM, "ZOOM" },
     { DIKS_MODE, "MODE" },
     { DIKS_KEYBOARD, "KEYBOARD" },
     { DIKS_PC, "PC" },
     { DIKS_SCREEN, "SCREEN" },

     { DIKS_TV, "TV" },
     { DIKS_TV2, "TV2" },
     { DIKS_VCR, "VCR" },
     { DIKS_VCR2, "VCR2" },
     { DIKS_SAT, "SAT" },
     { DIKS_SAT2, "SAT2" },
     { DIKS_CD, "CD" },
     { DIKS_TAPE, "TAPE" },
     { DIKS_RADIO, "RADIO" },
     { DIKS_TUNER, "TUNER" },
     { DIKS_PLAYER, "PLAYER" },
     { DIKS_TEXT, "TEXT" },
     { DIKS_DVD, "DVD" },
     { DIKS_AUX, "AUX" },
     { DIKS_MP3, "MP3" },
     { DIKS_AUDIO, "AUDIO" },
     { DIKS_VIDEO, "VIDEO" },
     { DIKS_INTERNET, "INTERNET" },
     { DIKS_MAIL, "MAIL" },
     { DIKS_NEWS, "NEWS" },

     { DIKS_RED, "RED" },
     { DIKS_GREEN, "GREEN" },
     { DIKS_YELLOW, "YELLOW" },
     { DIKS_BLUE, "BLUE" },

     { DIKS_CHANNELUP, "CHANNELUP" },
     { DIKS_CHANNELDOWN, "CHANNELDOWN" },
     { DIKS_BACK, "BACK" },
     { DIKS_FORWARD, "FORWARD" },
     { DIKS_VOLUMEUP, "VOLUMEUP" },
     { DIKS_VOLUMEDOWN, "VOLUMEDOWN" },
     { DIKS_MUTE, "MUTE" },
     { DIKS_AB, "AB" },

     { DIKS_PLAYPAUSE, "PLAYPAUSE" },
     { DIKS_PLAY, "PLAY" },
     { DIKS_STOP, "STOP" },
     { DIKS_RESTART, "RESTART" },
     { DIKS_SLOW, "SLOW" },
     { DIKS_FAST, "FAST" },
     { DIKS_RECORD, "RECORD" },
     { DIKS_EJECT, "EJECT" },
     { DIKS_SHUFFLE, "SHUFFLE" },
     { DIKS_REWIND, "REWIND" },
     { DIKS_FASTFORWARD, "FASTFORWARD" },
     { DIKS_PREVIOUS, "PREVIOUS" },
     { DIKS_NEXT, "NEXT" },

     { DIKS_DIGITS, "DIGITS" },
     { DIKS_TEEN, "TEEN" },
     { DIKS_TWEN, "TWEN" },
     { DIKS_ASTERISK, "ASTERISK" },
     { DIKS_NUMBER_SIGN, "NUMBER_SIGN" },
     { DIKS_NULL, NULL }
};


typedef struct {
     int          fd;
     InputDevice *device;
     pthread_t    thread;
} LircData;


static DFBInputDeviceKeySymbol lirc_parse_line(const char *line)
{
     const KeySymbolString *keysymbol_string = keysymbol_strings;
     char                  *s, *name;


     s = strchr( line, ' ' );
     if (!s || !s[1])
          return DIKS_NULL;

     s = strchr( ++s, ' ' );
     if (!s|| !s[1])
          return DIKS_NULL;

     name = ++s;

     s = strchr( name, ' ' );
     if (s)
          *s = '\0';

     switch (strlen( name )) {
          case 0:
               return DIKS_NULL;
          case 1:
               return name[0];
          default:
               while (keysymbol_string->symbol != DIKS_NULL) {
                    if (!strcmp( keysymbol_string->name, name ))
                         return keysymbol_string->symbol;

                    keysymbol_string++;
               }
               break;
     }

     return DIKS_NULL;
}

static void*
lircEventThread( void *driver_data )
{
     LircData      *data  = (LircData*) driver_data;
     int            readlen;
     char           buf[128];
     DFBInputEvent  evt;

     /* block all signals, they must not be handled by this thread */
     dfb_sig_block_all();

     memset( &evt, 0, sizeof(DFBInputEvent) );

     evt.flags = DIEF_KEYSYMBOL;

     while ((readlen = read( data->fd, buf, 128 )) > 0 || errno == EINTR) {
          pthread_testcancel();

          if (readlen < 1)
               continue;

          evt.key_symbol = lirc_parse_line( buf );
          if (evt.key_symbol != DIKS_NULL) {
               evt.type = DIET_KEYPRESS;
               dfb_input_dispatch( data->device, &evt );

               evt.type = DIET_KEYRELEASE;
               dfb_input_dispatch( data->device, &evt );
          }
     }

     if (readlen <= 0 && errno != EINTR)
          PERRORMSG ("lirc thread died\n");

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
     int fd;
     struct sockaddr_un addr;

     addr.sun_family = AF_UNIX;
     strcpy( addr.sun_path, "/dev/lircd" );

     fd = socket( PF_UNIX, SOCK_STREAM, 0 );
     if (fd < 0)
          return 0;

     if (connect( fd, (struct sockaddr*)&addr, sizeof(addr) ) < 0) {
          close( fd );
          return 0;
     }

     close( fd );

     return 1;
}

static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "LIRC Driver" );

     snprintf( info->vendor,
               DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 2;
}

static DFBResult
driver_open_device( InputDevice      *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     int                 fd;
     LircData           *data;
     struct sockaddr_un  sa;

     /* create socket */
     sa.sun_family = AF_UNIX;
     strcpy( sa.sun_path, "/dev/lircd" );

     fd = socket( PF_UNIX, SOCK_STREAM, 0 );
     if (fd < 0) {
          PERRORMSG( "DirectFB/LIRC: socket" );
          return DFB_INIT;
     }

     /* initiate connection */
     if (connect( fd, (struct sockaddr*)&sa, sizeof(sa) ) < 0) {
          PERRORMSG( "DirectFB/LIRC: connect" );
          close( fd );
          return DFB_INIT;
     }

     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DEVICE_INFO_NAME_LENGTH, "LIRC Device" );

     snprintf( info->vendor,
               DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH, "Unknown" );

     info->prefered_id = DIDID_REMOTE;

     info->desc.type   = DIDTF_REMOTE;
     info->desc.caps   = DICAPS_KEYS;

     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(LircData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     pthread_create( &data->thread, NULL, lircEventThread, data );

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}

static void
driver_close_device( void *driver_data )
{
     LircData *data = (LircData*) driver_data;

     /* stop input thread */
     pthread_cancel( data->thread );
     pthread_join( data->thread, NULL );

     /* close socket */
     close( data->fd );

     /* free private data */
     DFBFREE( data );
}

