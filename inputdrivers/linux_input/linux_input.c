/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>
              Andreas Hundt <andi@convergence.de> 
              Holger Waechtler <holger@convergence.de>

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
#include <fcntl.h>

#include <asm/types.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>


#include <pthread.h>

#include <linux/input.h>

#include <directfb.h>
#include <directfb_keyboard.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/input.h>

#include <misc/mem.h>
#include <misc/util.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( linux_input )


#define BITS_PER_LONG        (sizeof(long) * 8)
#define NBITS(x)             ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)               ((x)%BITS_PER_LONG)
#define BIT(x)               (1UL<<OFF(x))
#define LONG(x)              ((x)/BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

/*
 * declaration of private data
 */
typedef struct {
     int          fd;
     InputDevice *device;
     pthread_t    thread;
} LinuxInputData;


#define MAX_LINUX_INPUT_DEVICES 16

static int num_devices = 0;
static int device_nums[MAX_LINUX_INPUT_DEVICES];

#define DIKI_ DIKI_UNKNOWN

#define KEY_OK          (KEY_MAX-17)
#define KEY_LAST        (KEY_MAX-16)
#define KEY_INFO        (KEY_MAX-15)
#define KEY_CHANNELUP   (KEY_MAX-14)
#define KEY_CHANNELDOWN (KEY_MAX-13)
#define KEY_TEXT        (KEY_MAX-12)
#define KEY_TV          (KEY_MAX-11)
#define KEY_SUBTITLES   (KEY_MAX-10)
#define KEY_LANGUAGE    (KEY_MAX-9)
#define KEY_RADIO       (KEY_MAX-8)
#define KEY_LIST        (KEY_MAX-7)
#define KEY_RED         (KEY_MAX-6)
#define KEY_GREEN       (KEY_MAX-5)
#define KEY_YELLOW      (KEY_MAX-4)
#define KEY_BLUE        (KEY_MAX-3)
#define KEY_EPG         (KEY_MAX-2)
#define KEY_MHP         (KEY_MAX-1)

/*
 * Translates a Linux input keycode into a DirectFB keycode.
 */
static DFBInputDeviceKeyIdentifier
translate_key( unsigned short code, DFBInputEvent *devt )
{
	switch (code) {
          case KEY_ESC:
               devt->key_symbol = DIKS_ESCAPE;
	       break;

          case KEY_1:
               devt->key_symbol = DIKS_1;
	       break;

          case KEY_2:
               devt->key_symbol = DIKS_2;
	       break;

          case KEY_3:
               devt->key_symbol = DIKS_3;
	       break;

          case KEY_4:
               devt->key_symbol = DIKS_4;
	       break;

          case KEY_5:
               devt->key_symbol = DIKS_5;
	       break;

          case KEY_6:
               devt->key_symbol = DIKS_6;
	       break;

          case KEY_7:
               devt->key_symbol = DIKS_7;
	       break;

          case KEY_8:
               devt->key_symbol = DIKS_8;
	       break;

          case KEY_9:
               devt->key_symbol = DIKS_9;
	       break;

          case KEY_0:
               devt->key_symbol = DIKS_0;
	       break;
#if 0
          case KEY_MINUS:
               return DIKI_;

          case KEY_EQUAL:
               return DIKI_;

          case KEY_BACKSPACE:
               return DIKI_BACKSPACE;

          case KEY_TAB:
               return DIKI_TAB;

          case KEY_Q:
               return DIKI_Q;

          case KEY_W:
               return DIKI_W;

          case KEY_E:
               return DIKI_E;

          case KEY_R:
               return DIKI_R;

          case KEY_T:
               return DIKI_T;

          case KEY_Y:
               return DIKI_Y;

          case KEY_U:
               return DIKI_U;

          case KEY_I:
               return DIKI_I;

          case KEY_O:
               return DIKI_O;

          case KEY_P:
               return DIKI_P;

          case KEY_LEFTBRACE:
               return DIKI_;

          case KEY_RIGHTBRACE:
               return DIKI_;
#endif
	  case KEY_ENTER:
               devt->key_symbol = DIKS_ENTER;
	       break;
#if 0
          case KEY_LEFTCTRL:
               return DIKI_CONTROL_L;

          case KEY_A:
               return DIKI_A;

          case KEY_S:
               return DIKI_S;

          case KEY_D:
               return DIKI_D;

          case KEY_F:
               return DIKI_F;

          case KEY_G:
               return DIKI_G;

          case KEY_H:
               return DIKI_H;

          case KEY_J:
               return DIKI_J;

          case KEY_K:
               return DIKI_K;

          case KEY_L:
               return DIKI_L;

          case KEY_SEMICOLON:
               return DIKI_;

          case KEY_APOSTROPHE:
               return DIKI_;

          case KEY_GRAVE:
               return DIKI_;

          case KEY_LEFTSHIFT:
               return DIKI_SHIFT_L;

          case KEY_BACKSLASH:
               return DIKI_;

          case KEY_Z:
               return DIKI_Z;

          case KEY_X:
               return DIKI_X;

          case KEY_C:
               return DIKI_C;

          case KEY_V:
               return DIKI_V;

          case KEY_B:
               return DIKI_B;

          case KEY_N:
               return DIKI_N;

          case KEY_M:
               return DIKI_M;

          case KEY_COMMA:
               return DIKI_;

          case KEY_DOT:
               return DIKI_;

          case KEY_SLASH:
               return DIKI_;

          case KEY_RIGHTSHIFT:
               return DIKI_SHIFT_R;

          case KEY_KPASTERISK:
               return DIKI_KP_MULT;

          case KEY_LEFTALT:
               return DIKI_ALT_R;

          case KEY_SPACE:
               return DIKI_SPACE;

          case KEY_CAPSLOCK:
               return DIKS_CAPS_LOCK;

          case KEY_F1:
               return DIKI_F1;

          case KEY_F2:
               return DIKI_F2;

          case KEY_F3:
               return DIKI_F3;

          case KEY_F4:
               return DIKI_F4;

          case KEY_F5:
               return DIKI_F5;

          case KEY_F6:
               return DIKI_F6;

          case KEY_F7:
               return DIKI_F7;

          case KEY_F8:
               return DIKI_F8;

          case KEY_F9:
               return DIKI_F9;

          case KEY_F10:
               return DIKI_F10;

          case KEY_NUMLOCK:
               return DIKI_NUM_LOCK;

          case KEY_SCROLLLOCK:
               return DIKI_SCROLL_LOCK;

          case KEY_KP7:
               return DIKI_;

          case KEY_KP8:
               return DIKI_;

          case KEY_KP9:
               return DIKI_;

          case KEY_KPMINUS:
               return DIKI_KP_MINUS;

          case KEY_KP4:
               return DIKI_;

          case KEY_KP5:
               return DIKI_;

          case KEY_KP6:
               return DIKI_;

          case KEY_KPPLUS:
               return DIKI_KP_PLUS;

          case KEY_KP1:
               return DIKI_;

          case KEY_KP2:
               return DIKI_;

          case KEY_KP3:
               return DIKI_;

          case KEY_KP0:
               return DIKI_;

          case KEY_KPDOT:
               return DIKI_;

          case KEY_103RD:
               return DIKI_;

          case KEY_F13:
               return DIKI_;

          case KEY_102ND:
               return DIKI_;

          case KEY_F11:
               return DIKI_F11;

          case KEY_F12:
               return DIKI_F12;

          case KEY_F14:
               return DIKI_;

          case KEY_F15:
               return DIKI_;

          case KEY_F16:
               return DIKI_;

          case KEY_F17:
               return DIKI_;

          case KEY_F18:
               return DIKI_;

          case KEY_F19:
               return DIKI_;

          case KEY_F20:
               return DIKI_;

          case KEY_KPENTER:
               return DIKI_KP_ENTER;

          case KEY_RIGHTCTRL:
               return DIKI_CONTROL_R;

          case KEY_KPSLASH:
               return DIKI_KP_DIV;

          case KEY_SYSRQ:
               return DIKI_;

          case KEY_RIGHTALT:
               return DIKI_ALT_R;

          case KEY_LINEFEED:
               return DIKI_ENTER;

          case KEY_HOME:
               return DIKI_HOME;
#endif
	  case KEY_UP:
               devt->key_symbol = DIKS_CURSOR_UP;
	       break;

          case KEY_PAGEUP:
               devt->key_symbol = DIKS_PAGE_UP;
	       break;

          case KEY_LEFT:
               devt->key_symbol = DIKS_CURSOR_LEFT;
	       break;

          case KEY_RIGHT:
               devt->key_symbol = DIKS_CURSOR_RIGHT;
	       break;
#if 0
          case KEY_END:
               return DIKI_END;
#endif
          case KEY_DOWN:
               devt->key_symbol = DIKS_CURSOR_DOWN;
	       break;
#if 0
          case KEY_PAGEDOWN:
               return DIKS_PAGE_DOWN;

          case KEY_INSERT:
               return DIKI_INSERT;

          case KEY_DELETE:
               return DIKI_DELETE;

          case KEY_MACRO:
               return DIKI_;
#endif
          case KEY_MUTE:
               devt->key_symbol = DIKS_MUTE;
	       break;

          case KEY_VOLUMEDOWN:
               devt->key_symbol = DIKS_VOLUME_DOWN;
	       break;

          case KEY_VOLUMEUP:
               devt->key_symbol = DIKS_VOLUME_UP;
	       break;

          case KEY_POWER:
               devt->key_symbol = DIKS_POWER;
	       break;
#if 0
          case KEY_KPEQUAL:
               return DIKI_;

          case KEY_KPPLUSMINUS:
               return DIKI_;

          case KEY_PAUSE:
               return DIKS_PAUSE;

          case KEY_F21:
               return DIKI_;

          case KEY_F22:
               return DIKI_;

          case KEY_F23:
               return DIKI_;

          case KEY_F24:
               return DIKI_;

          case KEY_KPCOMMA:
               return DIKI_;

          case KEY_LEFTMETA:
               return DIKI_;

          case KEY_RIGHTMETA:
               return DIKI_;

          case KEY_COMPOSE:
               return DIKI_;


          case KEY_STOP:
               return DIKS_STOP;

          case KEY_AGAIN:
               return DIKI_;

          case KEY_PROPS:
               return DIKI_;

          case KEY_UNDO:
               return DIKI_;

          case KEY_FRONT:
               return DIKI_;

          case KEY_COPY:
               return DIKI_;

          case KEY_OPEN:
               return DIKI_;

          case KEY_PASTE:
               return DIKI_;

          case KEY_FIND:
               return DIKI_;

          case KEY_CUT:
               return DIKI_;

          case KEY_HELP:
               return DIKS_HELP;
#endif
          case KEY_MENU:
               devt->key_symbol = DIKS_MENU;
	       break;
#if 0
          case KEY_CALC:
               return DIKI_;
#endif
          case KEY_SETUP:
               devt->key_symbol = DIKS_SETUP;
	       break;
#if 0
          case KEY_SLEEP:
               return DIKI_;

          case KEY_WAKEUP:
               return DIKI_;

          case KEY_FILE:
               return DIKI_;

          case KEY_SENDFILE:
               return DIKI_;

          case KEY_DELETEFILE:
               return DIKI_;

          case KEY_XFER:
               return DIKI_;

          case KEY_PROG1:
               return DIKI_;

          case KEY_PROG2:
               return DIKI_;

          case KEY_WWW:
               return DIKS_INTERNET;

          case KEY_MSDOS:
               return DIKI_;

          case KEY_COFFEE:
               return DIKI_;

          case KEY_DIRECTION:
               return DIKI_;

          case KEY_CYCLEWINDOWS:
               return DIKI_;

          case KEY_MAIL:
               return DIKS_MAIL;

          case KEY_BOOKMARKS:
               return DIKI_;

          case KEY_COMPUTER:
               return DIKI_;

          case KEY_BACK:
               return DIKS_BACK;

          case KEY_FORWARD:
               return DIKS_FORWARD;

          case KEY_CLOSECD:
               return DIKI_;

          case KEY_EJECTCD:
               return DIKS_EJECT;

          case KEY_EJECTCLOSECD:
               return DIKS_EJECT;

          case KEY_NEXTSONG:
               return DIKS_NEXT;

          case KEY_PLAYPAUSE:
               return DIKS_PLAYPAUSE;

          case KEY_PREVIOUSSONG:
               return DIKS_PREVIOUS;

          case KEY_STOPCD:
               return DIKS_STOP;

          case KEY_RECORD:
               return DIKS_RECORD;

          case KEY_REWIND:
               return DIKS_REWIND;

          case KEY_PHONE:
               return DIKI_;

          case KEY_ISO:
               return DIKI_;

          case KEY_CONFIG:
               return DIKS_OPTION;

          case KEY_HOMEPAGE:
               return DIKI_;

          case KEY_REFRESH:
               return DIKI_;
#endif
          case KEY_EXIT:
               devt->key_symbol = DIKS_EXIT;
	       break;

#if 0
          case KEY_MOVE:
               return DIKI_;

          case KEY_EDIT:
               return DIKI_;

          case KEY_SCROLLUP:
               return DIKS_PAGE_UP;

          case KEY_SCROLLDOWN:
               return DIKS_PAGE_DOWN;

          case KEY_KPLEFTPAREN:
               return DIKI_;

          case KEY_KPRIGHTPAREN:
               return DIKI_;


          case KEY_INTL1:
               return DIKI_;

          case KEY_INTL2:
               return DIKI_;

          case KEY_INTL3:
               return DIKI_;

          case KEY_INTL4:
               return DIKI_;

          case KEY_INTL5:
               return DIKI_;

          case KEY_INTL6:
               return DIKI_;

          case KEY_INTL7:
               return DIKI_;

          case KEY_INTL8:
               return DIKI_;

          case KEY_INTL9:
               return DIKI_;

          case KEY_LANG1:
               return DIKI_;

          case KEY_LANG2:
               return DIKI_;

          case KEY_LANG3:
               return DIKI_;

          case KEY_LANG4:
               return DIKI_;

          case KEY_LANG5:
               return DIKI_;

          case KEY_LANG6:
               return DIKI_;

          case KEY_LANG7:
               return DIKI_;

          case KEY_LANG8:
               return DIKI_;

          case KEY_LANG9:
               return DIKI_;


          case KEY_PLAYCD:
               return DIKS_PLAY;

          case KEY_PAUSECD:
               return DIKS_PAUSE;

          case KEY_PROG3:
               return DIKI_;

          case KEY_PROG4:
               return DIKI_;

          case KEY_SUSPEND:
               return DIKI_;

          case KEY_CLOSE:
               return DIKI_;

          case KEY_PLAY:
               return DIKI_PLAY;

          case KEY_FASTFORWARD:
               return DIKI_FASTFORWARD;

          case KEY_BASSBOOST:
               return DIKI_;

          case KEY_PRINT:
               return DIKI_PRINT;

          case KEY_HP:
               return DIKI_;

          case KEY_CAMERA:
               return DIKI_;

          case KEY_SOUND:
               return DIKI_AUDIO;

          case KEY_QUESTION:
               return DIKI_;

          case KEY_EMAIL:
               return DIKI_MAIL;

          case KEY_CHAT:
               return DIKI_;

          case KEY_SEARCH:
               return DIKI_;

          case KEY_CONNECT:
               return DIKI_;

          case KEY_FINANCE:
               return DIKI_;

          case KEY_SPORT:
               return DIKI_;

          case KEY_SHOP:
               return DIKI_;
#endif
          case KEY_OK:
               devt->key_symbol = DIKS_OK;
	       break;

          case KEY_LAST:
               devt->key_symbol = DIKS_LAST;
	       break;

          case KEY_INFO:
               devt->key_symbol = DIKS_INFO;
	       break;

          case KEY_CHANNELUP:
               devt->key_symbol = DIKS_CHANNEL_UP;
	       break;

          case KEY_CHANNELDOWN:
               devt->key_symbol = DIKS_CHANNEL_DOWN;
	       break;

          case KEY_TEXT:
               devt->key_symbol = DIKS_TEXT;
	       break;

          case KEY_TV:
               devt->key_symbol = DIKS_TV;
	       break;

          case KEY_SUBTITLES:
               devt->key_symbol = DIKS_SUBTITLE;
	       break;

          case KEY_LANGUAGE:
               devt->key_symbol = DIKS_LANGUAGE;
	       break;

          case KEY_RADIO:
               devt->key_symbol = DIKS_RADIO;
	       break;

          case KEY_LIST:
               devt->key_symbol = DIKS_LIST;
	       break;

          case KEY_RED:
               devt->key_symbol = DIKS_RED;
	       break;

          case KEY_GREEN:
               devt->key_symbol = DIKS_GREEN;
	       break;

          case KEY_YELLOW:
               devt->key_symbol = DIKS_YELLOW;
	       break;

          case KEY_BLUE:
               devt->key_symbol = DIKS_BLUE;
	       break;

          case KEY_EPG:
               devt->key_symbol = DIKS_EPG;
	       break;

          case KEY_MHP:
               devt->key_symbol = DIKS_MHP;
	       break;

          default:
               return 0;
     }

     return 1;
}

/*
 * Translates key and button events.
 */
static int
key_event( struct input_event *levt,
           DFBInputEvent      *devt )
{
     if (levt->code >= BTN_MOUSE && levt->code <= BTN_STYLUS2) {
          devt->type   = levt->value ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
          devt->flags |= DIEF_BUTTONS;
          devt->button = DIBI_FIRST + levt->code - BTN_MOUSE;
     }
     else {
          if (!translate_key( levt->code, devt ))
               return 0;
          devt->type   = levt->value ? DIET_KEYPRESS : DIET_KEYRELEASE;
          if (devt->key_code)
             devt->flags |= DIEF_KEYCODE;
          if (devt->key_symbol)
             devt->flags |= DIEF_KEYSYMBOL;
//          if (devt->key_id)
//             devt->flags |= DIEF_KEYID;
     }

     return 1;
}

/*
 * Translates relative axis events.
 */
static int
rel_event( struct input_event *levt,
           DFBInputEvent      *devt )
{
     switch (levt->code) {
          case REL_X:
               devt->axis = DIAI_X;
               break;

          case REL_Y:
               devt->axis = DIAI_Y;
               break;

          case REL_Z:
          case REL_WHEEL:
               devt->axis = DIAI_Z;
               break;

          default:
               return 0;
     }

     devt->type    = DIET_AXISMOTION;
     devt->flags  |= DIEF_AXISREL;
     devt->axisrel = levt->value;

     return 1;
}

/*
 * Translates absolute axis events.
 */
static int
abs_event( struct input_event *levt,
           DFBInputEvent      *devt )
{
     switch (levt->code) {
          case ABS_X:
               devt->axis = DIAI_X;
               break;

          case ABS_Y:
               devt->axis = DIAI_Y;
               break;

          case ABS_Z:
          case ABS_WHEEL:
               devt->axis = DIAI_Z;
               break;

          default:
               return 0;
     }

     devt->type    = DIET_AXISMOTION;
     devt->flags  |= DIEF_AXISABS;
     devt->axisabs = levt->value;

     return 1;
}

/*
 * Translates a Linux input event into a DirectFB input event.
 */
static int
translate_event( struct input_event *levt,
                 DFBInputEvent      *devt )
{
     devt->flags     = DIEF_TIMESTAMP;
     devt->timestamp = levt->time;

     switch (levt->type) {
          case EV_KEY:
               return key_event( levt, devt );

          case EV_REL:
               return rel_event( levt, devt );

          case EV_ABS:
               return abs_event( levt, devt );

          default:
               ;
     }

     return 0;
}

/*
 * Input thread reading from device.
 * Generates events on incoming data.
 */
static void*
linux_input_EventThread( void *driver_data )
{
     LinuxInputData    *data = (LinuxInputData*) driver_data;
     int                readlen;
     struct input_event levt;
     DFBInputEvent      devt;

     while ((readlen = read( data->fd, &levt, sizeof(levt) )) == sizeof(levt)
            || (readlen < 0 && errno == EINTR))
     {
          pthread_testcancel();

          if (readlen <= 0)
               continue;

	  devt.flags = 0;
	  devt.key_code = 0;
	  devt.key_id = 0;
	  devt.key_symbol = 0;

          if (translate_event( &levt, &devt))
               dfb_input_dispatch( data->device, &devt );
     }

     if (readlen <= 0)
          PERRORMSG ("linux_input thread died\n");

     return NULL;
}

/*
 * Fill device information.
 * Queries the input device and tries to classify it.
 */
static void
get_device_info( int              fd,
                 InputDeviceInfo *info )
{
     unsigned int  num_keys    = 0;
     unsigned int  num_buttons = 0;
     unsigned int  num_rels    = 0;
     unsigned int  num_abs     = 0;

     unsigned long evbit[NBITS(EV_MAX)];
     unsigned long keybit[NBITS(KEY_MAX)];
     unsigned long relbit[NBITS(REL_MAX)];
     unsigned long absbit[NBITS(ABS_MAX)];

     /* get device name */
     ioctl( fd, EVIOCGNAME(DFB_INPUT_DEVICE_INFO_NAME_LENGTH), info->name );

     /* set device vendor */
     snprintf( info->vendor,
               DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH, "Linux" );

     /* get event type bits */
     ioctl( fd, EVIOCGBIT(0, EV_MAX), evbit );

     if (test_bit( EV_KEY, evbit )) {
          int i;

          /* get keyboard bits */
          ioctl( fd, EVIOCGBIT(EV_KEY, KEY_MAX), keybit );

          for (i=0; i<KEY_UNKNOWN; i++)
               if (test_bit( i, keybit ))
                    num_keys++;

          for (i=BTN_MISC; i<KEY_MAX; i++)
               if (test_bit( i, keybit ))
                    num_buttons++;
     }

     if (test_bit( EV_REL, evbit )) {
          int i;

          /* get bits for relative axes */
          ioctl( fd, EVIOCGBIT(EV_REL, REL_MAX), relbit );

          for (i=0; i<REL_MAX; i++)
               if (test_bit( i, relbit ))
                    num_rels++;
     }

     if (test_bit( EV_ABS, evbit )) {
          int i;

          /* get bits for absolute axes */
          ioctl( fd, EVIOCGBIT(EV_ABS, ABS_MAX), absbit );

          for (i=0; i<ABS_MAX; i++)
               if (test_bit( i, absbit ))
                    num_abs++;
     }

     /* Mouse or Touchscreen? */
     if ((num_rels && num_buttons)  ||  (num_abs && (num_buttons == 1)))
          info->desc.type |= DIDTF_MOUSE;
     else if (num_abs && num_buttons) /* Or a Joystick? */
          info->desc.type |= DIDTF_JOYSTICK;

     /* A Keyboard? */
     if (num_keys) {
          info->desc.type |= DIDTF_KEYBOARD;
          info->desc.caps |= DICAPS_KEYS;
     }

     /* Buttons */
     if (num_buttons) {
          info->desc.caps       |= DICAPS_BUTTONS;
          info->desc.max_button  = DIBI_FIRST + num_buttons - 1;
     }

     /* Axes */
     if (num_rels || num_abs) {
          info->desc.caps       |= DICAPS_AXES;
          info->desc.max_axis    = DIAI_FIRST + MAX(num_rels, num_abs) - 1;
     }

     /* Decide which primary input device to be. */
     if (info->desc.type & DIDTF_KEYBOARD)
          info->prefered_id = DIDID_KEYBOARD;
     else if (info->desc.type & DIDTF_JOYSTICK)
          info->prefered_id = DIDID_JOYSTICK;
     else
          info->prefered_id = DIDID_MOUSE;
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
     int i;

     for (i=0; i<MAX_LINUX_INPUT_DEVICES; i++) {
          int  fd;
          char buf[32];

          snprintf( buf, 32, "/dev/input/event%d", i );

          /* Check if we are able to open the device */
          fd = open( buf, O_RDONLY );
          if (fd < 0) {
             if (errno == ENODEV)
                  break;
          }
          else {
               close( fd );
               device_nums[num_devices++] = i;
          }
     }

     return num_devices;
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
                DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "Linux Input Driver" );
     snprintf ( info->vendor,
                DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
                "convergence integrated media GmbH" );

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
     int              fd;
     char             buf[32];
     LinuxInputData  *data;

     snprintf( buf, 32, "/dev/input/event%d", device_nums[number] );

     /* open device */
     fd = open( buf, O_RDONLY );
     if (fd < 0) {
          PERRORMSG( "DirectFB/linux_input: could not open device" );
          return DFB_INIT;
     }

     /* fill device info structure */
     get_device_info( fd, info );

     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(LinuxInputData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     pthread_create( &data->thread, NULL, linux_input_EventThread, data );

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
     LinuxInputData *data = (LinuxInputData*) driver_data;

     /* stop input thread */
     pthread_cancel( data->thread );
     pthread_join( data->thread, NULL );

     /* close file */
     close( data->fd );

     /* free private data */
     DFBFREE ( data );
}

