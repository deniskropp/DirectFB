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


#include <linux/input.h>
#include "input_fake.h"

#include <directfb.h>
#include <directfb_keyboard.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/input.h>
#include <core/thread.h>

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
     InputDevice *device;
     CoreThread  *thread;
     
     int          fd;
} LinuxInputData;


#define MAX_LINUX_INPUT_DEVICES 16

static int num_devices = 0;
static int device_nums[MAX_LINUX_INPUT_DEVICES];


static
int basic_keycodes [] = {
     DIKI_UNKNOWN, DIKI_ESCAPE, DIKI_1, DIKI_2, DIKI_3, DIKI_4, DIKI_5, 
     DIKI_6, DIKI_7, DIKI_8, DIKI_9, DIKI_0, DIKI_MINUS_SIGN, 
     DIKI_EQUALS_SIGN, DIKI_BACKSPACE,

     DIKI_TAB, DIKI_Q, DIKI_W, DIKI_E, DIKI_R, DIKI_T, DIKI_Y, DIKI_U, 
     DIKI_I, DIKI_O, DIKI_P, DIKI_BRACKET_LEFT, DIKI_BRACKET_RIGHT, 
     DIKI_ENTER,

     DIKI_CONTROL_L, DIKI_A, DIKI_S, DIKI_D, DIKI_F, DIKI_G, DIKI_H, DIKI_J,
     DIKI_K, DIKI_L, DIKI_SEMICOLON, DIKI_QUOTE_RIGHT, DIKI_QUOTE_LEFT,

     DIKI_SHIFT_L, DIKI_BACKSLASH, DIKI_Z, DIKI_X, DIKI_C, DIKI_V, DIKI_B,
     DIKI_N, DIKI_M, DIKI_COMMA, DIKI_PERIOD, DIKI_SLASH, DIKI_SHIFT_R,
     DIKI_KP_MULT, DIKI_ALT_L, DIKI_SPACE, DIKI_CAPS_LOCK, 

     DIKI_F1, DIKI_F2, DIKI_F3, DIKI_F4, DIKI_F5, DIKI_F6, DIKI_F7, DIKI_F8,
     DIKI_F9, DIKI_F10, DIKI_NUM_LOCK, DIKI_SCROLL_LOCK,

     DIKI_KP_7, DIKI_KP_8, DIKI_KP_9, DIKI_KP_MINUS,
     DIKI_KP_4, DIKI_KP_5, DIKI_KP_6, DIKI_KP_PLUS,
     DIKI_KP_1, DIKI_KP_2, DIKI_KP_3, DIKI_KP_0, DIKI_KP_DECIMAL,

     /*KEY_103RD,*/ DIKI_BACKSLASH,
     /*KEY_F13,*/ DFB_FUNCTION_KEY(13),
     /*KEY_102ND*/ DIKI_LESS_SIGN,
	     
     DIKI_F11, DIKI_F12, DFB_FUNCTION_KEY(14), DFB_FUNCTION_KEY(15),
     DFB_FUNCTION_KEY(16), DFB_FUNCTION_KEY(17), DFB_FUNCTION_KEY(18),
     DFB_FUNCTION_KEY(19), DFB_FUNCTION_KEY(20),

     DIKI_KP_ENTER, DIKI_CONTROL_R, DIKI_KP_DIV, DIKI_PRINT, DIKI_ALT_R,

     /*KEY_LINEFEED*/ DIKI_UNKNOWN,
	
     DIKI_HOME, DIKI_UP, DIKI_PAGE_UP, DIKI_LEFT, DIKI_RIGHT,
     DIKI_END, DIKI_DOWN, DIKI_PAGE_DOWN, DIKI_INSERT, DIKI_DELETE, 

     /*KEY_MACRO,*/ DIKI_UNKNOWN,

     DIKS_MUTE, DIKS_VOLUME_DOWN, DIKS_VOLUME_UP, DIKS_POWER, DIKI_KP_EQUAL,

     /*KEY_KPPLUSMINUS,*/ DIKI_UNKNOWN,

     DIKS_PAUSE, DFB_FUNCTION_KEY(21), DFB_FUNCTION_KEY(22),
     DFB_FUNCTION_KEY(23), DFB_FUNCTION_KEY(24),

     DIKI_KP_SEPARATOR, DIKI_META_L, DIKI_META_R, DIKI_SUPER_L,

     DIKS_STOP,

     /*DIKS_AGAIN, DIKS_PROPS, DIKS_UNDO, DIKS_FRONT, DIKS_COPY,
     DIKS_OPEN, DIKS_PASTE, DIKS_FIND, DIKS_CUT,*/
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, 
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, 

     DIKS_HELP, DIKS_MENU, DIKS_CALCULATOR, DIKS_SETUP,

     /*KEY_SLEEP, KEY_WAKEUP, KEY_FILE, KEY_SENDFILE, KEY_DELETEFILE,
     KEY_XFER,*/
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, 
     DIKI_UNKNOWN,

     /*KEY_PROG1, KEY_PROG2,*/
     DIKS_CUSTOM1, DIKS_CUSTOM2, 

     DIKS_INTERNET,

     /*KEY_MSDOS, KEY_COFFEE, KEY_DIRECTION, KEY_CYCLEWINDOWS,*/
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,

     DIKS_MAIL,

     /*KEY_BOOKMARKS, KEY_COMPUTER, */
     DIKI_UNKNOWN, DIKI_UNKNOWN,

     DIKS_BACK, DIKS_FORWARD,

     /*KEY_CLOSECD, KEY_EJECTCD, KEY_EJECTCLOSECD,*/
     DIKS_EJECT, DIKS_EJECT, DIKS_EJECT, 

     DIKS_NEXT, DIKS_PLAYPAUSE, DIKS_PREVIOUS, DIKS_STOP, DIKS_RECORD, 
     DIKS_REWIND, DIKS_PHONE,

     /*KEY_ISO,*/ DIKI_UNKNOWN,
     /*KEY_CONFIG,*/ DIKS_SETUP,
     /*KEY_HOMEPAGE, KEY_REFRESH,*/ DIKI_UNKNOWN, DIKI_UNKNOWN,

     DIKS_EXIT, /*KEY_MOVE,*/ DIKI_UNKNOWN, DIKS_EDITOR,

     /*KEY_SCROLLUP,*/ DIKS_PAGE_UP,
     /*KEY_SCROLLDOWN,*/ DIKS_PAGE_DOWN,
     /*KEY_KPLEFTPAREN,*/ DIKI_UNKNOWN,
     /*KEY_KPRIGHTPAREN,*/ DIKI_UNKNOWN,

     /*KEY_INTL1,  KEY_INTL2, KEY_INTL3, KEY_INTL4, KEY_INTL5, KEY_INTL6, 
     KEY_INTL7, KEY_INTL8, KEY_INTL9,*/
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,

     /*KEY_LANG1, KEY_LANG2, KEY_LANG3, KEY_LANG4, KEY_LANG5, KEY_LANG6,
     KEY_LANG7, KEY_LANG8, KEY_LANG9,*/
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,

     DIKS_PLAY, DIKS_PAUSE, 

     /*KEY_PROG3, KEY_PROG4,*/
     DIKS_CUSTOM3, DIKS_CUSTOM4, 

     /*KEY_SUSPEND, KEY_CLOSE*/ DIKI_UNKNOWN, DIKI_UNKNOWN
};

static
int ext_keycodes [] = {
     DIKS_OK, DIKS_SELECT, DIKS_GOTO, DIKS_CLEAR, DIKS_POWER2, DIKS_OPTION,
     DIKS_INFO, DIKS_TIME, DIKS_VENDOR, DIKS_ARCHIVE, DIKS_PROGRAM, 
     DIKS_CHANNEL, DIKS_FAVORITES, DIKS_EPG, DIKS_PVR, DIKS_MHP,
     DIKS_LANGUAGE, DIKS_TITLE, DIKS_SUBTITLE, DIKS_ANGLE, DIKS_ZOOM, 
     DIKS_MODE, DIKS_KEYBOARD, DIKS_SCREEN, DIKS_PC, DIKS_TV, DIKS_TV2,
     DIKS_VCR, DIKS_VCR2, DIKS_SAT, DIKS_SAT2, DIKS_CD, DIKS_TAPE,
     DIKS_RADIO, DIKS_TUNER, DIKS_PLAYER, DIKS_TEXT, DIKS_DVD, DIKS_AUX,
     DIKS_MP3, DIKS_AUDIO, DIKS_VIDEO, DIKS_DIRECTORY, DIKS_LIST, DIKS_MEMO,
     DIKS_CALENDAR, DIKS_RED, DIKS_GREEN, DIKS_YELLOW, DIKS_BLUE,
     DIKS_CHANNEL_UP, DIKS_CHANNEL_DOWN, DIKS_FIRST, DIKS_LAST, DIKS_AB,
     DIKS_PLAY, DIKS_RESTART, DIKS_SLOW, DIKS_SHUFFLE, DIKS_FASTFORWARD,
     DIKS_PREVIOUS, DIKS_NEXT, DIKS_DIGITS, DIKS_TEEN, DIKS_TWEN, DIKS_BREAK
};

/*
 * Translates a Linux input keycode into a DirectFB keycode.
 */
static int
translate_key( unsigned short code )
{
     if (code < sizeof(basic_keycodes)/sizeof(basic_keycodes[0]))
          return basic_keycodes[code];

     if (code >= KEY_OK)
          if (code - KEY_OK < sizeof(ext_keycodes)/sizeof(ext_keycodes[0]))
               return ext_keycodes[code-KEY_OK];

     return DIKI_UNKNOWN;
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
          /* don't set DIEF_BUTTONS, it will be set by the input core */
          devt->button = DIBI_FIRST + levt->code - BTN_MOUSE;
     }
     else {
          int key = translate_key( levt->code );

          if (key == DIKI_UNKNOWN)
               return 0;

          devt->type = levt->value ? DIET_KEYPRESS : DIET_KEYRELEASE;

          if (DFB_KEY_TYPE(key) == DIKT_IDENTIFIER) {
               devt->key_id = key;
               devt->flags |= DIEF_KEYID;
          }
          else {
               devt->key_symbol = key;
               devt->flags |= DIEF_KEYSYMBOL;
          }
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
linux_input_EventThread( CoreThread *thread, void *driver_data )
{
     LinuxInputData    *data = (LinuxInputData*) driver_data;
     int                readlen;
     struct input_event levt;
     DFBInputEvent      devt;

     while ((readlen = read( data->fd, &levt, sizeof(levt) )) == sizeof(levt)
            || (readlen < 0 && errno == EINTR))
     {
          dfb_thread_testcancel( thread );

          if (readlen <= 0)
               continue;

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
     data->thread = dfb_thread_create( CTT_INPUT,
                                       linux_input_EventThread, data );

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
     /* XXX FIXME: get keymap from tty... */
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
     dfb_thread_cancel( data->thread );
     dfb_thread_join( data->thread );
     dfb_thread_destroy( data->thread );

     /* close file */
     close( data->fd );

     /* free private data */
     DFBFREE ( data );
}

