/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <misc/mem.h>


typedef struct {
     DFBInputDeviceKeyIdentifier  key;
     char                        *name;
} KeyCodeString;

static const KeyCodeString keycode_strings[] = {
     { DIKC_A, "A" },
     { DIKC_B, "B" },
     { DIKC_C, "C" },
     { DIKC_D, "D" },
     { DIKC_E, "E" },
     { DIKC_F, "F" },
     { DIKC_G, "G" },
     { DIKC_H, "H" },
     { DIKC_I, "I" },
     { DIKC_J, "J" },
     { DIKC_K, "K" },
     { DIKC_L, "L" },
     { DIKC_M, "M" },
     { DIKC_N, "N" },
     { DIKC_O, "O" },
     { DIKC_P, "P" },
     { DIKC_Q, "Q" },
     { DIKC_R, "R" },
     { DIKC_S, "S" },
     { DIKC_T, "T" },
     { DIKC_U, "U" },
     { DIKC_V, "V" },
     { DIKC_W, "W" },
     { DIKC_X, "X" },
     { DIKC_Y, "Y" },
     { DIKC_Z, "Z" },

     { DIKC_0, "0" },
     { DIKC_1, "1" },
     { DIKC_2, "2" },
     { DIKC_3, "3" },
     { DIKC_4, "4" },
     { DIKC_5, "5" },
     { DIKC_6, "6" },
     { DIKC_7, "7" },
     { DIKC_8, "8" },
     { DIKC_9, "9" },

     { DIKC_F1,   "F1" },
     { DIKC_F2,   "F2" },
     { DIKC_F3,   "F3" },
     { DIKC_F4,   "F4" },
     { DIKC_F5,   "F5" },
     { DIKC_F6,   "F6" },
     { DIKC_F7,   "F7" },
     { DIKC_F8,   "F8" },
     { DIKC_F9,   "F9" },
     { DIKC_F10, "F10" },
     { DIKC_F11, "F11" },
     { DIKC_F12, "F12" },

     { DIKC_ESCAPE, "ESCAPE" },
     { DIKC_LEFT, "LEFT" },
     { DIKC_RIGHT, "RIGHT" },
     { DIKC_UP, "UP" },
     { DIKC_DOWN, "DOWN" },
     { DIKC_CTRL, "CTRL" },
     { DIKC_SHIFT, "SHIFT" },
     { DIKC_ALT, "ALT" },
     { DIKC_ALTGR, "ALTGR" },
     { DIKC_TAB, "TAB" },
     { DIKC_ENTER, "ENTER" },
     { DIKC_SPACE, "SPACE" },
     { DIKC_BACKSPACE, "BACKSPACE" },
     { DIKC_INSERT, "INSERT" },
     { DIKC_DELETE, "DELETE" },
     { DIKC_HOME, "HOME" },
     { DIKC_END, "END" },
     { DIKC_PAGEUP, "PAGEUP" },
     { DIKC_PAGEDOWN, "PAGEDOWN" },
     { DIKC_CAPSLOCK, "CAPSLOCK" },
     { DIKC_NUMLOCK, "NUMLOCK" },
     { DIKC_SCRLOCK, "SCRLOCK" },
     { DIKC_PRINT, "PRINT" },
     { DIKC_PAUSE, "PAUSE" },
     { DIKC_KP_DIV, "KP_DIV" },
     { DIKC_KP_MULT, "KP_MULT" },
     { DIKC_KP_MINUS, "KP_MINUS" },
     { DIKC_KP_PLUS, "KP_PLUS" },
     { DIKC_KP_ENTER, "KP_ENTER" },

     { DIKC_OK, "OK" },
     { DIKC_CANCEL, "CANCEL" },
     { DIKC_SELECT, "SELECT" },
     { DIKC_GOTO, "GOTO" },
     { DIKC_CLEAR, "CLEAR" },
     { DIKC_POWER, "POWER" },
     { DIKC_POWER2, "POWER2" },
     { DIKC_OPTION, "OPTION" },
     { DIKC_MENU, "MENU" },
     { DIKC_HELP, "HELP" },
     { DIKC_INFO, "INFO" },
     { DIKC_TIME, "TIME" },
     { DIKC_VENDOR, "VENDOR" },

     { DIKC_ARCHIVE, "ARCHIVE" },
     { DIKC_PROGRAM, "PROGRAM" },
     { DIKC_FAVORITES, "FAVORITES" },
     { DIKC_EPG, "EPG" },
     { DIKC_LANGUAGE, "LANGUAGE" },
     { DIKC_TITLE, "TITLE" },
     { DIKC_SUBTITLE, "SUBTITLE" },
     { DIKC_ANGLE, "ANGLE" },
     { DIKC_ZOOM, "ZOOM" },
     { DIKC_MODE, "MODE" },
     { DIKC_KEYBOARD, "KEYBOARD" },
     { DIKC_PC, "PC" },
     { DIKC_SCREEN, "SCREEN" },

     { DIKC_TV, "TV" },
     { DIKC_TV2, "TV2" },
     { DIKC_VCR, "VCR" },
     { DIKC_VCR2, "VCR2" },
     { DIKC_SAT, "SAT" },
     { DIKC_SAT2, "SAT2" },
     { DIKC_CD, "CD" },
     { DIKC_TAPE, "TAPE" },
     { DIKC_RADIO, "RADIO" },
     { DIKC_TUNER, "TUNER" },
     { DIKC_PLAYER, "PLAYER" },
     { DIKC_TEXT, "TEXT" },
     { DIKC_DVD, "DVD" },
     { DIKC_AUX, "AUX" },
     { DIKC_MP3, "MP3" },
     { DIKC_AUDIO, "AUDIO" },
     { DIKC_VIDEO, "VIDEO" },
     { DIKC_INTERNET, "INTERNET" },
     { DIKC_MAIL, "MAIL" },
     { DIKC_NEWS, "NEWS" },

     { DIKC_RED, "RED" },
     { DIKC_GREEN, "GREEN" },
     { DIKC_YELLOW, "YELLOW" },
     { DIKC_BLUE, "BLUE" },

     { DIKC_CHANNELUP, "CHANNELUP" },
     { DIKC_CHANNELDOWN, "CHANNELDOWN" },
     { DIKC_BACK, "BACK" },
     { DIKC_FORWARD, "FORWARD" },
     { DIKC_VOLUMEUP, "VOLUMEUP" },
     { DIKC_VOLUMEDOWN, "VOLUMEDOWN" },
     { DIKC_MUTE, "MUTE" },
     { DIKC_AB, "AB" },

     { DIKC_PLAYPAUSE, "PLAYPAUSE" },
     { DIKC_PLAY, "PLAY" },
     { DIKC_STOP, "STOP" },
     { DIKC_RESTART, "RESTART" },
     { DIKC_SLOW, "SLOW" },
     { DIKC_FAST, "FAST" },
     { DIKC_RECORD, "RECORD" },
     { DIKC_EJECT, "EJECT" },
     { DIKC_SHUFFLE, "SHUFFLE" },
     { DIKC_REWIND, "REWIND" },
     { DIKC_FASTFORWARD, "FASTFORWARD" },
     { DIKC_PREVIOUS, "PREVIOUS" },
     { DIKC_NEXT, "NEXT" },

     { DIKC_DIGITS, "DIGITS" },
     { DIKC_TEEN, "TEEN" },
     { DIKC_TWEN, "TWEN" },
     { DIKC_ASTERISK, "ASTERISK" },
     { DIKC_HASH, "HASH" },
     { DIKC_UNKNOWN, NULL }
};


typedef struct {
     int          fd;
     InputDevice *device;
     pthread_t    thread;
} LircData;


static DFBInputDeviceKeyIdentifier lirc_parse_line(const char *line)
{
     const KeyCodeString *keycode_string = keycode_strings;
     char                *s, *name;


     s = strchr( line, ' ' );
     if (!s || !s[1])
          return DIKC_UNKNOWN;

     s = strchr( ++s, ' ' );
     if (!s|| !s[1])
          return DIKC_UNKNOWN;

     name = ++s;

     s = strchr( name, ' ' );
     if (s)
          *s = '\0';

     while (keycode_string->key != DIKC_UNKNOWN) {
          if (!strcmp( keycode_string->name, name ))
               return keycode_string->key;

          keycode_string++;
     }

     return DIKC_UNKNOWN;
}

static void*
lircEventThread( void *driver_data )
{
     LircData      *data  = (LircData*) driver_data;
     int            readlen;
     char           buf[128];
     DFBInputEvent  evt;

     memset( &evt, 0, sizeof(DFBInputEvent) );

     evt.flags = DIEF_KEYCODE;

     while ((readlen = read( data->fd, buf, 128 )) > 0 || errno == EINTR) {
          pthread_testcancel();

          if (readlen < 1)
               continue;
          
          evt.keycode = lirc_parse_line( buf );
          if (evt.keycode != DIKC_UNKNOWN) {
               evt.type = DIET_KEYPRESS;
               input_dispatch( data->device, &evt );

               evt.type = DIET_KEYRELEASE;
               input_dispatch( data->device, &evt );
          }
     }

     if (readlen <= 0 && errno != EINTR)
          PERRORMSG ("lirc thread died\n");

     return NULL;
}

/* exported symbols */

int
driver_get_abi_version()
{
     return DFB_INPUT_DRIVER_ABI_VERSION;
}

int
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

void
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

DFBResult
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

void
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

