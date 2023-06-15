/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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



#include <config.h>


#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
typedef unsigned long kernel_ulong_t;
#define BITS_PER_LONG    (sizeof(long)*8)
#endif

#include <linux/input.h>

#ifndef KEY_OK
/* Linux kernel 2.5.42+ defines additional keys in linux/input.h */
#include "input_fake.h"
#endif

#ifndef EV_CNT
#define EV_CNT (EV_MAX+1)
#define KEY_CNT (KEY_MAX+1)
#define REL_CNT (REL_MAX+1)
#define ABS_CNT (ABS_MAX+1)
#define LED_CNT (LED_MAX+1)
#endif

/* compat defines for older kernel like 2.4.x */
#ifndef EV_SYN
#define EV_SYN			0x00
#define SYN_REPORT              0
#define SYN_CONFIG              1
#define ABS_TOOL_WIDTH		0x1c
#define BTN_TOOL_DOUBLETAP	0x14d
#define BTN_TOOL_TRIPLETAP	0x14e
#endif

#ifndef EVIOCGLED
#define EVIOCGLED(len) _IOC(_IOC_READ, 'E', 0x19, len)
#endif

#ifndef EVIOCGRAB
#define EVIOCGRAB _IOW('E', 0x90, int)
#endif

#include <linux/keyboard.h>


#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <dfb_types.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <stdlib.h>

#define DFB_INPUTDRIVER_HAS_AXIS_INFO
#define DFB_INPUTDRIVER_HAS_SET_CONFIGURATION

#include <directfb.h>
#include <directfb_keyboard.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/input.h>
#include <core/system.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <sys/socket.h>
#include <sys/un.h>

/* Exclude hot-plug stub functionality from this input provider. */
#define DISABLE_INPUT_HOTPLUG_FUNCTION_STUB

#include <fbdev/fbdev.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( linux_input )

D_DEBUG_DOMAIN( Debug_LinuxInput, "Input/Linux", "Linux input driver" );

#ifndef BITS_PER_LONG
#define BITS_PER_LONG        (sizeof(long) * 8)
#endif
#define NBITS(x)             ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)               ((x)%BITS_PER_LONG)
#define BIT(x)               (1UL<<OFF(x))
#define LONG(x)              ((x)/BITS_PER_LONG)
#undef test_bit
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

#define MAX_LENGTH_OF_EVENT_STRING 1024

/* compat for 2.4.x kernel - just a compile fix */
#ifndef HAVE_INPUT_ABSINFO
typedef struct {
        s32 value;
        s32 minimum;
        s32 maximum;
        s32 fuzz;
        s32 flat;
} Input_AbsInfo;
#else
typedef struct input_absinfo Input_AbsInfo;
#endif


/*
 * declaration of private data
 */
typedef struct {
     CoreInputDevice         *device;
     DirectThread            *thread;

     int                      fd;
     int                      quitpipe[2];

     bool                     has_keys;
     bool                     has_leds;
     unsigned long            led_state[NBITS(LED_CNT)];
     DFBInputDeviceLockState  locks;

     int                      vt_fd;

     int                      dx;
     int                      dy;

     bool                     touchpad;

     /* Indice of the associated device_nums and device_names array entry.
      * Used as the second parameter of the driver_open_device function.
      */
     int                      index;

     int                      sensitivity;
} LinuxInputData;


#define MAX_LINUX_INPUT_DEVICES 16

static int num_devices = 0;
static char *device_names[MAX_LINUX_INPUT_DEVICES];
/* The entries with the same index in device_names and device_nums are the same
 * are used in two different forms (one is "/dev/input/eventX", the other is
 * X).
 */
static int               device_nums[MAX_LINUX_INPUT_DEVICES] = { 0 };
/* Socket file descriptor for getting udev events. */
static int               socket_fd = 0;
/* Pipe file descriptor for terminating the hotplug thread. */
static int               hotplug_quitpipe[2];
/* The hot-plug thread that is launched by the launch_hotplug() function. */
static DirectThread     *hotplug_thread = NULL;
/* The driver suspended lock mutex. */
static pthread_mutex_t   driver_suspended_lock;
/* Flag that indicates if the driver is suspended when true. */
static bool              driver_suspended = false;


static const
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

     DIKI_KP_ENTER, DIKI_CONTROL_R, DIKI_KP_DIV, DIKI_PRINT, DIKS_ALTGR,

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
     /*KEY_HOMEPAGE, KEY_REFRESH,*/ DIKI_UNKNOWN, DIKS_SHUFFLE,

     DIKS_EXIT, /*KEY_MOVE,*/ DIKI_UNKNOWN, DIKS_EDITOR,

     /*KEY_SCROLLUP,*/ DIKS_PAGE_UP,
     /*KEY_SCROLLDOWN,*/ DIKS_PAGE_DOWN,
     /*KEY_KPLEFTPAREN,*/ DIKI_UNKNOWN,
     /*KEY_KPRIGHTPAREN,*/ DIKI_UNKNOWN,

     /* unused codes 181-182: */
     DIKI_UNKNOWN, DIKI_UNKNOWN,

     DFB_FUNCTION_KEY(13), DFB_FUNCTION_KEY(14), DFB_FUNCTION_KEY(15),
     DFB_FUNCTION_KEY(16), DFB_FUNCTION_KEY(17), DFB_FUNCTION_KEY(18),
     DFB_FUNCTION_KEY(19), DFB_FUNCTION_KEY(20), DFB_FUNCTION_KEY(21),
     DFB_FUNCTION_KEY(22), DFB_FUNCTION_KEY(23), DFB_FUNCTION_KEY(24),

     /* unused codes 195-199: */
     DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN, DIKI_UNKNOWN,

     /* KEY_PLAYCD, KEY_PAUSECD */
     DIKS_PLAY, DIKS_PAUSE,

     /*KEY_PROG3, KEY_PROG4,*/
     DIKS_CUSTOM3, DIKS_CUSTOM4,

     DIKI_UNKNOWN,

     /*KEY_SUSPEND, KEY_CLOSE*/
     DIKI_UNKNOWN, DIKI_UNKNOWN,

     /* KEY_PLAY */
     DIKS_PLAY,

     /* KEY_FASTFORWARD */
     DIKS_FASTFORWARD,

     /* KEY_BASSBOOST */
     DIKI_UNKNOWN,

     /* KEY_PRINT */
     DIKS_PRINT,

     /* KEY_HP             */  DIKI_UNKNOWN,
     /* KEY_CAMERA         */  DIKI_UNKNOWN,
     /* KEY_SOUND          */  DIKS_AUDIO,
     /* KEY_QUESTION       */  DIKS_HELP,
     /* KEY_EMAIL          */  DIKS_MAIL,
     /* KEY_CHAT           */  DIKI_UNKNOWN,
     /* KEY_SEARCH         */  DIKI_UNKNOWN,
     /* KEY_CONNECT        */  DIKI_UNKNOWN,
     /* KEY_FINANCE        */  DIKI_UNKNOWN,
     /* KEY_SPORT          */  DIKI_UNKNOWN,
     /* KEY_SHOP           */  DIKI_UNKNOWN,
     /* KEY_ALTERASE       */  DIKI_UNKNOWN,
     /* KEY_CANCEL         */  DIKS_CANCEL,
     /* KEY_BRIGHTNESSDOWN */  DIKI_UNKNOWN,
     /* KEY_BRIGHTNESSUP   */  DIKI_UNKNOWN,
     /* KEY_MEDIA          */  DIKI_UNKNOWN,
};

static const
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
     DIKS_NEXT, DIKS_RESTART, DIKS_SLOW, DIKS_SHUFFLE, DIKS_FASTFORWARD,
     DIKS_PREVIOUS, DIKS_NEXT, DIKS_DIGITS, DIKS_TEEN, DIKS_TWEN, DIKS_BREAK
};

/*
 * Touchpads related stuff
 */
enum {
     TOUCHPAD_FSM_START,
     TOUCHPAD_FSM_MAIN,
     TOUCHPAD_FSM_DRAG_START,
     TOUCHPAD_FSM_DRAG_MAIN,
};
struct touchpad_axis {
     int old, min, max;
};
struct touchpad_fsm_state {
     int fsm_state;
     struct touchpad_axis x;
     struct touchpad_axis y;
     struct timeval timeout;
};

static void
touchpad_fsm_init( struct touchpad_fsm_state *state );
static int
touchpad_fsm( struct touchpad_fsm_state *state,
              const struct input_event  *levt,
              DFBInputEvent             *devt );

static bool
timeout_passed( const struct timeval *timeout, const struct timeval *current );
static bool
timeout_is_set( const struct timeval *timeout );
static void
timeout_sub( struct timeval *timeout, const struct timeval *sub );

/*
 * Translates a Linux input keycode into a DirectFB keycode.
 */
static int
translate_key( unsigned short code )
{
     if (code < D_ARRAY_SIZE( basic_keycodes ))
          return basic_keycodes[code];

     if (code >= KEY_OK)
          if (code - KEY_OK < D_ARRAY_SIZE( ext_keycodes ))
               return ext_keycodes[code-KEY_OK];

     return DIKI_UNKNOWN;
}

static DFBInputDeviceKeySymbol
keyboard_get_symbol( int                             code,
                     unsigned short                  value,
                     DFBInputDeviceKeymapSymbolIndex level )
{
     unsigned char type  = KTYP(value);
     unsigned char index = KVAL(value);
     int           base  = (level == DIKSI_BASE);

     switch (type) {
          case KT_FN:
               if (index < 20)
                    return DFB_FUNCTION_KEY( index + 1 );
               break;
          case KT_LETTER:
          case KT_LATIN:
               switch (index) {
                    case 0x1c:
                         return DIKS_PRINT;
                    case 0x7f:
                         return DIKS_BACKSPACE;
                    case 0xa4:
                         return 0x20ac; /* euro currency sign */
                    default:
                         return index;
               }
               break;
          case KT_DEAD:
               switch (value) {
                    case K_DGRAVE:
                         return DIKS_DEAD_GRAVE;

                    case K_DACUTE:
                         return DIKS_DEAD_ACUTE;

                    case K_DCIRCM:
                         return DIKS_DEAD_CIRCUMFLEX;

                    case K_DTILDE:
                         return DIKS_DEAD_TILDE;

                    case K_DDIERE:
                         return DIKS_DEAD_DIAERESIS;

                    case K_DCEDIL:
                         return DIKS_DEAD_CEDILLA;

                    default:
                         break;
               }
               break;
          case KT_PAD:
               if (index <= 9 && level != DIKSI_BASE)
                    return DIKS_0 + index;
               break;
          case 0xe: /* special IPAQ H3600 case - AH */
               switch (index) {
                    case 0x20:     return DIKS_CALENDAR;
                    case 0x1a:     return DIKS_BACK;
                    case 0x1c:     return DIKS_MEMO;
                    case 0x21:     return DIKS_POWER;
               }
               break;
          case 0xd: /* another special IPAQ H3600 case - AH */
               switch (index) {
                    case 0x2:     return DIKS_DIRECTORY;
                    case 0x1:     return DIKS_MAIL;  /* Q on older iPaqs */
               }
               break;
     }

     switch (value) {
          case K_LEFT:    return DIKS_CURSOR_LEFT;
          case K_RIGHT:   return DIKS_CURSOR_RIGHT;
          case K_UP:      return DIKS_CURSOR_UP;
          case K_DOWN:    return DIKS_CURSOR_DOWN;
          case K_ENTER:   return DIKS_ENTER;
          case K_CTRL:    return DIKS_CONTROL;
          case K_SHIFT:   return DIKS_SHIFT;
          case K_ALT:     return DIKS_ALT;
          case K_ALTGR:   return DIKS_ALTGR;
          case K_INSERT:  return DIKS_INSERT;
          case K_REMOVE:  return DIKS_DELETE;
          case K_FIND:    return DIKS_HOME;
          case K_SELECT:  return DIKS_END;
          case K_PGUP:    return DIKS_PAGE_UP;
          case K_PGDN:    return DIKS_PAGE_DOWN;
          case K_NUM:     return DIKS_NUM_LOCK;
          case K_HOLD:    return DIKS_SCROLL_LOCK;
          case K_PAUSE:   return DIKS_PAUSE;
          case K_BREAK:   return DIKS_BREAK;
          case K_CAPS:    return DIKS_CAPS_LOCK;

          case K_P0:      return DIKS_INSERT;
          case K_P1:      return DIKS_END;
          case K_P2:      return DIKS_CURSOR_DOWN;
          case K_P3:      return DIKS_PAGE_DOWN;
          case K_P4:      return DIKS_CURSOR_LEFT;
          case K_P5:      return DIKS_BEGIN;
          case K_P6:      return DIKS_CURSOR_RIGHT;
          case K_P7:      return DIKS_HOME;
          case K_P8:      return DIKS_CURSOR_UP;
          case K_P9:      return DIKS_PAGE_UP;
          case K_PPLUS:   return DIKS_PLUS_SIGN;
          case K_PMINUS:  return DIKS_MINUS_SIGN;
          case K_PSTAR:   return DIKS_ASTERISK;
          case K_PSLASH:  return DIKS_SLASH;
          case K_PENTER:  return DIKS_ENTER;
          case K_PCOMMA:  return base ? DIKS_DELETE : DIKS_COMMA;
          case K_PDOT:    return base ? DIKS_DELETE : DIKS_PERIOD;
          case K_PPARENL: return DIKS_PARENTHESIS_LEFT;
          case K_PPARENR: return DIKS_PARENTHESIS_RIGHT;
     }

     /* special keys not in the map, hack? */
     if (code == 99)
          return DIKS_PRINT;

     if (code == 124)         /* keypad equal key */
          return DIKS_EQUALS_SIGN;

     if (code == 125)         /* left windows key */
          return DIKS_META;

     if (code == 126)         /* right windows key */
          return DIKS_META;

     if (code == 127)         /* context menu key */
          return DIKS_SUPER;

     return DIKS_NULL;
}

static DFBInputDeviceKeyIdentifier
keyboard_get_identifier( int code, unsigned short value )
{
     unsigned char type  = KTYP(value);
     unsigned char index = KVAL(value);

     if (type == KT_PAD) {
          if (index <= 9)
               return DIKI_KP_0 + index;

          switch (value) {
               case K_PSLASH: return DIKI_KP_DIV;
               case K_PSTAR:  return DIKI_KP_MULT;
               case K_PMINUS: return DIKI_KP_MINUS;
               case K_PPLUS:  return DIKI_KP_PLUS;
               case K_PENTER: return DIKI_KP_ENTER;
               case K_PCOMMA:
               case K_PDOT:   return DIKI_KP_DECIMAL;
          }
     }

     /* Looks like a hack, but don't know a better way yet. */
     switch (code) {
          case 12: return DIKI_MINUS_SIGN;
          case 13: return DIKI_EQUALS_SIGN;
          case 26: return DIKI_BRACKET_LEFT;
          case 27: return DIKI_BRACKET_RIGHT;
          case 39: return DIKI_SEMICOLON;
          case 40: return DIKI_QUOTE_RIGHT;
          case 41: return DIKI_QUOTE_LEFT;
          case 43: return DIKI_BACKSLASH;
          case 51: return DIKI_COMMA;
          case 52: return DIKI_PERIOD;
          case 53: return DIKI_SLASH;
          case 54: return DIKI_SHIFT_R;
          case 97: return DIKI_CONTROL_R;
          case 100: return DIKI_ALT_R;
          default:
               ;
     }

     /* special keys not in the map, hack? */
     if (code == 124)         /* keypad equal key */
          return DIKI_KP_EQUAL;

     if (code == 125)         /* left windows key */
          return DIKI_META_L;

     if (code == 126)         /* right windows key */
          return DIKI_META_R;

     if (code == 127)         /* context menu key */
          return DIKI_SUPER_R;

     return DIKI_UNKNOWN;
}

static unsigned short
keyboard_read_value( const LinuxInputData *data,
                     unsigned char table, unsigned char index )
{
     struct kbentry entry;

     entry.kb_table = table;
     entry.kb_index = index;
     entry.kb_value = 0;

     if (ioctl( data->vt_fd, KDGKBENT, &entry )) {
          D_PERROR("DirectFB/keyboard: KDGKBENT (table: %d, index: %d) "
                    "failed!\n", table, index);
          return 0;
     }

     return entry.kb_value;
}

/*
 * Translates key and button events.
 */
static bool
key_event( const struct input_event *levt,
           DFBInputEvent            *devt )
{
     int code = levt->code;

     /* map touchscreen and smartpad events to button mouse */
     if (code == BTN_TOUCH || code == BTN_TOOL_FINGER)
          code = BTN_MOUSE;

     if ((code >= BTN_MOUSE && code < BTN_JOYSTICK) || code == BTN_TOUCH) {
          /* ignore repeat events for buttons */
          if (levt->value == 2)
               return false;

          devt->type   = levt->value ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
          /* don't set DIEF_BUTTONS, it will be set by the input core */
          devt->button = DIBI_FIRST + code - BTN_MOUSE;
     }
     else {
          int key = translate_key( code );

          if (key == DIKI_UNKNOWN)
               return false;

          devt->type = levt->value ? DIET_KEYPRESS : DIET_KEYRELEASE;

          if (DFB_KEY_TYPE(key) == DIKT_IDENTIFIER) {
               devt->key_id = key;
               devt->flags |= DIEF_KEYID;
          }
          else {
               devt->key_symbol = key;
               devt->flags |= DIEF_KEYSYMBOL;
          }

          devt->flags |= DIEF_KEYCODE;
          devt->key_code = code;
     }

     if (levt->value == 2)
          devt->flags |= DIEF_REPEAT;

     return true;
}

/*
 * Translates relative axis events.
 */
static bool
rel_event( const LinuxInputData     *data,
           const struct input_event *levt,
           DFBInputEvent            *devt )
{
     switch (levt->code) {
          case REL_X:
               devt->axis = DIAI_X;
               devt->axisrel = levt->value * data->sensitivity / 0x100;
               break;

          case REL_Y:
               devt->axis = DIAI_Y;
               devt->axisrel = levt->value * data->sensitivity / 0x100;
               break;

          case REL_Z:
          case REL_WHEEL:
               devt->axis = DIAI_Z;
               devt->axisrel = -levt->value;
               break;

          default:
               if (levt->code > REL_MAX || levt->code > DIAI_LAST)
                    return false;
               devt->axis = levt->code;
               devt->axisrel = levt->value;
     }

     devt->type    = DIET_AXISMOTION;
     devt->flags  |= DIEF_AXISREL;

     return true;
}

/*
 * Translates absolute axis events.
 */
static bool
abs_event( const struct input_event *levt,
           DFBInputEvent            *devt )
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
               if (levt->code >= ABS_PRESSURE || levt->code > DIAI_LAST)
                    return false;
               devt->axis = levt->code;
     }

     devt->type    = DIET_AXISMOTION;
     devt->flags  |= DIEF_AXISABS;
     devt->axisabs = levt->value;

     return true;
}

/*
 * Translates a Linux input event into a DirectFB input event.
 */
static bool
translate_event( const LinuxInputData     *data,
                 const struct input_event *levt,
                 DFBInputEvent            *devt )
{
     devt->flags     = DIEF_TIMESTAMP;
     devt->timestamp = levt->time;

     switch (levt->type) {
          case EV_KEY:
               return key_event( levt, devt );

          case EV_REL:
               return rel_event( data, levt, devt );

          case EV_ABS:
               return abs_event( levt, devt );

          default:
               ;
     }

     return false;
}

static void
set_led( const LinuxInputData *data, int led, int state )
{
     int res;

     struct input_event levt;

     levt.type = EV_LED;
     levt.code = led;
     levt.value = !!state;

     res = write( data->fd, &levt, sizeof(levt) );
     (void)res;
}

static void
flush_xy( LinuxInputData *data, bool last )
{
     DFBInputEvent evt = { .type = DIET_UNKNOWN };

     if (data->dx) {
          evt.type    = DIET_AXISMOTION;
          evt.flags   = DIEF_AXISREL;
          evt.axis    = DIAI_X;
          evt.axisrel = data->dx;

          /* Signal immediately following event. */
          if (!last || data->dy)
               evt.flags |= DIEF_FOLLOW;

          dfb_input_dispatch( data->device, &evt );

          data->dx = 0;
     }

     if (data->dy) {
          evt.type    = DIET_AXISMOTION;
          evt.flags   = DIEF_AXISREL;
          evt.axis    = DIAI_Y;
          evt.axisrel = data->dy;

          /* Signal immediately following event. */
          if (!last)
               evt.flags |= DIEF_FOLLOW;

          dfb_input_dispatch( data->device, &evt );

          data->dy = 0;
     }
}

/*
 * Input thread reading from device.
 * Generates events on incoming data.
 */
static void*
linux_input_EventThread( DirectThread *thread, void *driver_data )
{
     LinuxInputData    *data = (LinuxInputData*) driver_data;
     int                readlen, status;
     unsigned int       i;
     int                fdmax;
     struct input_event levt[64];
     fd_set             set;
     struct touchpad_fsm_state fsm_state;

     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     fdmax = MAX( data->fd, data->quitpipe[0] );

     /* Query min/max coordinates. */
     if (data->touchpad) {
          Input_AbsInfo absinfo;

          touchpad_fsm_init( &fsm_state );

          ioctl( data->fd, EVIOCGABS(ABS_X), &absinfo );
          fsm_state.x.min = absinfo.minimum;
          fsm_state.x.max = absinfo.maximum;

          ioctl( data->fd, EVIOCGABS(ABS_Y), &absinfo );
          fsm_state.y.min = absinfo.minimum;
          fsm_state.y.max = absinfo.maximum;
     }

     /* Query key states. */
     if (data->has_keys) {
          unsigned long keybit[NBITS(KEY_CNT)];
          unsigned long keystate[NBITS(KEY_CNT)];
          int i;

          /* get keyboard bits */
          ioctl( data->fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit );

          /* get key states */
          ioctl( data->fd, EVIOCGKEY(sizeof(keystate)), keystate );

          /* for each key,
             synthetize a press or release event depending on the key state */
          for (i=0; i<=KEY_CNT; i++) {
               if (test_bit( i, keybit )) {
                    const int key = translate_key( i );

                    if (DFB_KEY_TYPE(key) == DIKT_IDENTIFIER) {
                         DFBInputEvent devt;

                         devt.type     = (test_bit( i, keystate )
                                          ? DIET_KEYPRESS : DIET_KEYRELEASE);
                         devt.flags    = DIEF_KEYID | DIEF_KEYCODE;
                         devt.key_id   = key;
                         devt.key_code = i;

                         dfb_input_dispatch( data->device, &devt );
                    }
               }
          }
     }

     while (1) {
          DFBInputEvent devt = { .type = DIET_UNKNOWN };

          FD_ZERO( &set );
          FD_SET( data->fd, &set );
          FD_SET( data->quitpipe[0], &set );

          if (data->touchpad && timeout_is_set( &fsm_state.timeout )) {
               struct timeval time;
               gettimeofday( &time, NULL );

               if (!timeout_passed( &fsm_state.timeout, &time )) {
                    struct timeval timeout = fsm_state.timeout;
                    timeout_sub( &timeout, &time );
                    status = select( fdmax + 1, &set, NULL, NULL, &timeout );
               } else {
                    status = 0;
               }
          }
          else {
               status = select( fdmax + 1, &set, NULL, NULL, NULL );
          }

          if (status < 0 && errno != EINTR)
               break;

          if (status > 0 && FD_ISSET( data->quitpipe[0], &set ))
               break;

          direct_thread_testcancel( thread );

          if (status < 0)
               continue;

          /* timeout? */
          if (status == 0) {
               if (data->touchpad && touchpad_fsm( &fsm_state, NULL, &devt ) > 0)
                    dfb_input_dispatch( data->device, &devt );

               continue;
          }

          readlen = read( data->fd, levt, sizeof(levt) );

          if (readlen < 0 && errno != EINTR)
               break;

          direct_thread_testcancel( thread );

          if (readlen <= 0)
               continue;

          for (i=0; i<readlen / sizeof(levt[0]); i++) {
               DFBInputEvent temp = { .type = DIET_UNKNOWN };

               if (data->touchpad) {
                    status = touchpad_fsm( &fsm_state, &levt[i], &temp );
                    if (status < 0) {
                         /* Not handled. Try the direct approach. */
                         if (!translate_event( data, &levt[i], &temp ))
                              continue;
                    }
                    else if (status == 0) {
                         /* Handled but no further processing is necessary. */
                         continue;
                    }
               }
               else {
                    if (!translate_event( data, &levt[i], &temp ))
                         continue;
               }

               /* Flush previous event with DIEF_FOLLOW? */
               if (devt.type != DIET_UNKNOWN) {
                    flush_xy( data, false );

                    /* Signal immediately following event. */
                    devt.flags |= DIEF_FOLLOW;

                    dfb_input_dispatch( data->device, &devt );

                    if (data->has_leds && (devt.locks != data->locks)) {
                         set_led( data, LED_SCROLLL, devt.locks & DILS_SCROLL );
                         set_led( data, LED_NUML, devt.locks & DILS_NUM );
                         set_led( data, LED_CAPSL, devt.locks & DILS_CAPS );
                         data->locks = devt.locks;
                    }

                    devt.type  = DIET_UNKNOWN;
                    devt.flags = DIEF_NONE;
               }

               devt = temp;

               if (D_FLAGS_IS_SET( devt.flags, DIEF_AXISREL ) && devt.type == DIET_AXISMOTION &&
                   dfb_config->mouse_motion_compression)
               {
                    switch (devt.axis) {
                         case DIAI_X:
                              data->dx += devt.axisrel;
                              continue;

                         case DIAI_Y:
                              data->dy += devt.axisrel;
                              continue;

                         default:
                              break;
                    }
               }

               /* Event is dispatched in next round of loop. */
          }

          /* Flush last event without DIEF_FOLLOW. */
          if (devt.type != DIET_UNKNOWN) {
               flush_xy( data, false );

               dfb_input_dispatch( data->device, &devt );

               if (data->has_leds && (devt.locks != data->locks)) {
                    set_led( data, LED_SCROLLL, devt.locks & DILS_SCROLL );
                    set_led( data, LED_NUML, devt.locks & DILS_NUM );
                    set_led( data, LED_CAPSL, devt.locks & DILS_CAPS );
                    data->locks = devt.locks;
               }
          }
          else
               flush_xy( data, true );
     }

     if (status <= 0)
          D_PERROR ("linux_input thread died\n");

     return NULL;
}

/*
 * Fill device information.
 * Queries the input device and tries to classify it.
 */
static void
get_device_info( int              fd,
                 InputDeviceInfo *info,
                 bool            *touchpad )
{
     unsigned int  num_keys     = 0;
     unsigned int  num_ext_keys = 0;
     unsigned int  num_buttons  = 0;
     unsigned int  num_rels     = 0;
     unsigned int  num_abs      = 0;

     unsigned long evbit[NBITS(EV_CNT)];
     unsigned long keybit[NBITS(KEY_CNT)];
     unsigned long relbit[NBITS(REL_CNT)];
     unsigned long absbit[NBITS(ABS_CNT)];

     struct input_id devinfo;

     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     /* get device name */
     ioctl( fd, EVIOCGNAME(DFB_INPUT_DEVICE_DESC_NAME_LENGTH - 1), info->desc.name );

     D_DEBUG_AT( Debug_LinuxInput, "  -> name '%s'\n", info->desc.name );

     /* set device vendor */
     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "Linux" );

     /* get event type bits */
     ioctl( fd, EVIOCGBIT(0, sizeof(evbit)), evbit );

     if (test_bit( EV_KEY, evbit )) {
          int i;

          /* get keyboard bits */
          ioctl( fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit );

          /* count typical keyboard keys only */
          for (i=KEY_Q; i<=KEY_M; i++)
               if (test_bit( i, keybit ))
                    num_keys++;

          /* this might be a keyboard with just cursor keys, typically found
             on front panels - handle as remote control and make sure not to
             treat normal (full key) keyboards likewise */
          if (!num_keys)
               for (i=KEY_HOME; i<=KEY_PAGEDOWN; i++)
                    if (test_bit( i, keybit ))
                         num_ext_keys++;

          for (i=KEY_OK; i<KEY_CNT; i++)
               if (test_bit( i, keybit ))
                    num_ext_keys++;

          for (i=BTN_MOUSE; i<BTN_JOYSTICK; i++)
               if (test_bit( i, keybit ))
                    num_buttons++;

          if (num_keys || num_ext_keys)
#ifndef DIRECTFB_DISABLE_DEPRECATED
               info->desc.caps |= DICAPS_KEYS;
#else
               info->desc.caps |= DIDCAPS_KEYS;
#endif

     }

     if (test_bit( EV_REL, evbit )) {
          int i;

          /* get bits for relative axes */
          ioctl( fd, EVIOCGBIT(EV_REL, sizeof(relbit)), relbit );

          for (i=0; i<REL_CNT; i++)
               if (test_bit( i, relbit ))
                    num_rels++;
     }

     if (test_bit( EV_ABS, evbit )) {
          int i;

          /* get bits for absolute axes */
          ioctl( fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit );

          for (i=0; i<ABS_PRESSURE; i++)
               if (test_bit( i, absbit ))
                    num_abs++;
     }

     /* Touchpad? */
     /* FIXME: can we rely on BTN_TOUCH? xorg synaptics driver doesn't use it. */
     if (test_bit( EV_KEY, evbit ) &&
         test_bit( BTN_TOUCH, keybit ) &&
         test_bit( BTN_TOOL_FINGER, keybit) &&
         test_bit( EV_ABS, evbit ) &&
         test_bit( ABS_X, absbit ) &&
         test_bit( ABS_Y, absbit ) &&
         test_bit( ABS_PRESSURE, absbit ))
          *touchpad = true;
     else
          *touchpad = false;

     /* Mouse, Touchscreen or Smartpad ? */
     if ((test_bit( EV_KEY, evbit ) &&
          (test_bit( BTN_TOUCH, keybit ) || test_bit( BTN_TOOL_FINGER, keybit ))) ||
          ((num_rels >= 2 && num_buttons)  ||  (num_abs == 2 && (num_buttons == 1))))
          info->desc.type |= DIDTF_MOUSE;
     else if (num_abs && num_buttons) /* Or a Joystick? */
          info->desc.type |= DIDTF_JOYSTICK;

     /* A Keyboard, do we have at least some letters? */
     if (num_keys > 20) {
          info->desc.type |= DIDTF_KEYBOARD;

          info->desc.min_keycode = 0;
          info->desc.max_keycode = 127;
     }

     /* A Remote Control? */
     if (num_ext_keys) {
          info->desc.type |= DIDTF_REMOTE;
     }

     /* Buttons */
     if (num_buttons) {
#ifndef DIRECTFB_DISABLE_DEPRECATED
          info->desc.caps       |= DICAPS_BUTTONS;
#else
          info->desc.caps       |= DIDCAPS_BUTTONS;
#endif
          info->desc.max_button  = DIBI_FIRST + num_buttons - 1;
     }

     /* Axes */
     if (num_rels || num_abs) {
#ifndef DIRECTFB_DISABLE_DEPRECATED
          info->desc.caps       |= DICAPS_AXES;
#else
          info->desc.caps       |= DIDCAPS_AXES;
#endif
          info->desc.max_axis    = DIAI_FIRST + MAX(num_rels, num_abs) - 1;
     }

     /* Decide which primary input device to be. */
     if (info->desc.type & DIDTF_KEYBOARD)
          info->prefered_id = DIDID_KEYBOARD;
     else if (info->desc.type & DIDTF_REMOTE)
          info->prefered_id = DIDID_REMOTE;
     else if (info->desc.type & DIDTF_JOYSTICK)
          info->prefered_id = DIDID_JOYSTICK;
     else if (info->desc.type & DIDTF_MOUSE)
          info->prefered_id = DIDID_MOUSE;
     else
          info->prefered_id = DIDID_ANY;

     /* Get VID and PID information */
     ioctl( fd, EVIOCGID, &devinfo );

     info->desc.vendor_id  = devinfo.vendor;
     info->desc.product_id = devinfo.product;

     D_DEBUG_AT( Debug_LinuxInput, "  -> ids %d/%d\n", info->desc.vendor_id, info->desc.product_id );
}

static bool
check_device( const char *device )
{
     int  fd;

     D_DEBUG_AT( Debug_LinuxInput, "%s( '%s' )\n", __FUNCTION__, device );

     /* Check if we are able to open the device */
     fd = open( device, O_RDWR );
     if (fd < 0) {
          D_DEBUG_AT( Debug_LinuxInput, "  -> open failed!\n" );
          return false;
     }
     else {
          InputDeviceInfo info;
          bool touchpad;

          /* try to grab the device */
          if (dfb_config->linux_input_grab) {
               /* 2.4 kernels don't have EVIOCGRAB so ignore EINVAL */
               if (ioctl( fd, EVIOCGRAB, 1 ) && errno != EINVAL) {
                    close( fd );
                    return false;
               }
          }

          memset( &info, 0, sizeof(InputDeviceInfo) );

          get_device_info( fd, &info, &touchpad );

          if (dfb_config->linux_input_grab)
               ioctl( fd, EVIOCGRAB, 0 );
          close( fd );

          if (!info.desc.caps) {
              D_DEBUG_AT( Debug_LinuxInput, "  -> no caps!\n" );
              return false;
          }

          if (!dfb_config->linux_input_ir_only ||
              (info.desc.type & DIDTF_REMOTE))
               return true;
     }

     D_DEBUG_AT( Debug_LinuxInput, "  -> returning false!\n" );

     return false;
}

/* exported symbols */

/*
 * Return the number of available devices.
 * Called once during initialization of DirectFB.
 */
static int
driver_get_available( void )
{
     int   i;
     char *tsdev;

     if (!(dfb_config->linux_input_force || (dfb_system_type() == CORE_FBDEV) || (dfb_system_type() == CORE_MESA) || (dfb_system_type() == CORE_DRMKMS)  ))
          return 0;

     if (dfb_system_type() == CORE_FBDEV && !dfb_config->linux_input_force) {
          FBDev *dfb_fbdev = (FBDev*) dfb_system_data();
          D_ASSERT( dfb_fbdev );

          // Only allow USB keyboard and mouse support if the systems driver has
          // the Virtual Terminal file ("/dev/tty") open and available for use.
          // FIXME:  Additional logic needed for system drivers not similar to fbdev?
          if (!dfb_fbdev->vt || dfb_fbdev->vt->fd < 0)
               return 0;
     }

     /* Use the devices specified in the configuration. */
     if (fusion_vector_has_elements( &dfb_config->linux_input_devices )) {
          const char *device;

          fusion_vector_foreach (device, i, dfb_config->linux_input_devices) {
               if (num_devices >= MAX_LINUX_INPUT_DEVICES)
                    break;

               /* Update the device_names and device_nums array entries too. */
               if (check_device( device )){
                    D_ASSERT( device_names[num_devices] == NULL );
                    device_names[num_devices] = D_STRDUP( device );
                    device_nums[num_devices] = i;
                    num_devices++;
               }
          }

          return num_devices;
     }

     /* Check for tslib device being used. */
     tsdev = getenv( "TSLIB_TSDEVICE" );

     /* No devices specified. Try to guess some. */
     for (i=0; i<MAX_LINUX_INPUT_DEVICES; i++) {
          char buf[32];

          snprintf( buf, 32, "/dev/input/event%d", i );

          /* Initialize device_names and device_nums array entries. */
          device_nums[i] = MAX_LINUX_INPUT_DEVICES;
          device_names[i] = NULL;

          /* Let tslib driver handle its device. */
          if (tsdev && !strcmp( tsdev, buf ))
               continue;

          /* Update the device_names and device_nums array entries too. */
          if (check_device( buf )){
               D_ASSERT( device_names[num_devices] == NULL );
               device_names[num_devices] = D_STRDUP( buf );
               device_nums[num_devices] = i;
               num_devices++;
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
                DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "directfb.org" );

     info->version.major = 0;
     info->version.minor = 1;
}

/*
 * Enter the driver suspended state by setting the driver_suspended Boolean
 * to prevent hotplug events from being handled.
 */
static DFBResult
driver_suspend( void )
{
     if (pthread_mutex_lock(&driver_suspended_lock))
          return DFB_FAILURE;

     driver_suspended = true;

     pthread_mutex_unlock(&driver_suspended_lock);

     return DFB_OK;
}

/*
 * Leave the driver suspended state by clearing the driver_suspended Boolean
 * which will allow hotplug events to be handled again.
 */
static DFBResult
driver_resume( void )
{
     if (pthread_mutex_lock(&driver_suspended_lock))
          return DFB_FAILURE;

     driver_suspended = false;

     pthread_mutex_unlock(&driver_suspended_lock);

     return DFB_OK;
}

/*
 * Register /dev/input/eventX device node into the driver.  Called when a new
 * device node is created. The device node indicated by event_num should never
 * be registered before registering it into the driver or should be
 * unregistered by unregister_device_node() beforehand.
 */
static DFBResult
register_device_node( int event_num, int *index)
{
     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     D_ASSERT( index != NULL );

     int i;
     char buf[32];

     for (i=0; i<MAX_LINUX_INPUT_DEVICES; i++) {
          if (device_nums[i] == MAX_LINUX_INPUT_DEVICES) {
               device_nums[i] = event_num;
               *index = i;
               num_devices++;

               snprintf( buf, 32, "/dev/input/event%d", event_num);
               D_ASSERT( device_names[i] == NULL );
               device_names[i] = D_STRDUP( buf );

               return DFB_OK;
          }
     }

     /* Too many input devices plugged in to be handled by linux_input driver. */
     D_DEBUG_AT( Debug_LinuxInput,
                 "The amount of devices registered exceeds the limit (%u) "
                 "supported by linux input provider.\n",
                 MAX_LINUX_INPUT_DEVICES );
     return DFB_UNSUPPORTED;
}

/*
 * Unregister /dev/input/eventX device node from the driver.  Called when a new
 * device node is removed.
 */
static DFBResult
unregister_device_node( int event_num, int *index)
{
     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     D_ASSERT( index != NULL );

     int i;

     for (i=0; i<MAX_LINUX_INPUT_DEVICES; i++) {
          if (device_nums[i] == event_num) {
               device_nums[i] = MAX_LINUX_INPUT_DEVICES;
               num_devices--;

               *index = i;
               D_FREE(device_names[i]);
               device_names[i] = NULL;

               return DFB_OK;
          }
     }

     return DFB_UNSUPPORTED;
}

/*
 * Check if /dev/input/eventX is handled by the input device.  If so, return
 * DFB_OK.  Otherwise, return DFB_UNSUPPORTED.
 */
static DFBResult
is_created( int index, void *data)
{
     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     D_ASSERT( data != NULL );

     if (index < 0 || index >= MAX_LINUX_INPUT_DEVICES) {
          return DFB_UNSUPPORTED;
     }

     if (index != ((LinuxInputData *)data)->index) {
          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

/*
 * Indicate that the hotplug detection capability is supported by this input
 * provider if the Virtual Terminal was opened for use by the systems driver.
 *
 * Note:  The systems driver will open the Virtual Terminal file
 *        ("/dev/tty") based on the directfbrc commands "vt" and "no-vt".
 */
static InputDriverCapability
get_capability( void )
{
     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     InputDriverCapability   capabilities = IDC_NONE;

     if (!(dfb_config->linux_input_force || (dfb_system_type() == CORE_FBDEV) || (dfb_system_type() == CORE_MESA) ))
          goto exit;

     if (dfb_system_type() == CORE_FBDEV) {
          FBDev *dfb_fbdev = (FBDev*) dfb_system_data();
          D_ASSERT( dfb_fbdev );

          // Only allow USB keyboard and mouse support if the systems driver has
          // the Virtual Terminal file ("/dev/tty") open and available for use.
          // FIXME:  Additional logic needed for system drivers not similar to fbdev?
          if (!dfb_fbdev->vt || dfb_fbdev->vt->fd < 0) {
               D_DEBUG_AT( Debug_LinuxInput, "  -> no VT\n" );
               goto exit;
          }
     }

     D_DEBUG_AT( Debug_LinuxInput, "  -> returning HOTPLUG\n" );

     capabilities |= IDC_HOTPLUG;

exit:
     return capabilities;
}

/*
 * Detect udev hotplug events from socket /org/kernel/udev/monitor and act
 * according to hotplug events received.
 */
static void *
udev_hotplug_EventThread(DirectThread *thread, void * hotplug_data)
{
     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     CoreDFB           *core;
     void              *driver;
     HotplugThreadData *data = (HotplugThreadData *)hotplug_data;
     int                rt;
     struct sockaddr_un sock_addr;
     int                fdmax;

     D_ASSERT( data != NULL );
     D_ASSERT( data->core != NULL );
     D_ASSERT( data->driver != NULL );

     core = data->core;
     driver = data->driver;

     /* Free no needed data packet */
     D_FREE(data);

     /* Open and bind the socket /org/kernel/udev/monitor */

     socket_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
     if (socket_fd == -1) {
          D_PERROR( "DirectFB/linux_input: socket() failed: %s\n",
                    strerror(errno) );
          goto errorExit;
     }

     fdmax = MAX( socket_fd, hotplug_quitpipe[0] );

     memset(&sock_addr, 0, sizeof(sock_addr));
     sock_addr.sun_family = AF_UNIX;
     strncpy(&sock_addr.sun_path[1],
             "/org/kernel/udev/monitor",
             sizeof(sock_addr.sun_path) - 1);

     rt = bind(socket_fd, &sock_addr,
               sizeof(sock_addr.sun_family)+1+strlen(&sock_addr.sun_path[1]));
     if (rt < 0) {
          D_PERROR( "DirectFB/linux_input: bind() failed: %s\n",
                    strerror(errno) );
          goto errorExit;
     }

     while(1) {
          char      udev_event[MAX_LENGTH_OF_EVENT_STRING];
          char     *pos;
          char     *event_cont; //udev event content
          int       device_num, number_file, recv_len, index;
          DFBResult ret;
          fd_set    rset;

          /* get udev event */
          FD_ZERO(&rset);
          FD_SET(socket_fd, &rset);
          FD_SET(hotplug_quitpipe[0], &rset);

          number_file = select(fdmax+1, &rset, NULL, NULL, NULL);

          if (number_file < 0 && errno != EINTR)
               break;

          if (FD_ISSET( hotplug_quitpipe[0], &rset ))
               break;

          /* check cancel thread */
          direct_thread_testcancel( thread );

          if (FD_ISSET(socket_fd, &rset)) {
               recv_len = recv(socket_fd, udev_event, sizeof(udev_event), 0);
               if (recv_len <= 0) {
                    D_DEBUG_AT( Debug_LinuxInput,
                                "error receiving uevent message: %s\n",
                                strerror(errno) );
                    continue;
               }
               /* check cancel thread */
               direct_thread_testcancel( thread );
          }
          /* analysize udev event */

          pos = strchr(udev_event, '@');
          if (pos == NULL)
               continue;

          /* replace '@' with '\0' to separate event type and event content */
          *pos = '\0';

          event_cont = pos + 1;

          pos = strstr(event_cont, "/event");
          if (pos == NULL)
               continue;

          /* get event device number */
          device_num = atoi(pos + 6);

          /* Attempt to lock the driver suspended mutex. */
          pthread_mutex_lock(&driver_suspended_lock);
          if (driver_suspended)
          {
               /* Release the lock and quit handling hotplug events. */
               D_DEBUG_AT( Debug_LinuxInput, "Driver is suspended\n" );
               pthread_mutex_unlock(&driver_suspended_lock);
               continue;
          }

          /* Handle hotplug events since the driver is not suspended. */
          if (!strcmp(udev_event, "add")) {
               D_DEBUG_AT( Debug_LinuxInput,
                           "Device node /dev/input/event%d is created by udev\n",
                           device_num);

               ret = register_device_node( device_num, &index);
               if ( DFB_OK == ret) {
                    /* Handle the event that the input device node is created */
                    ret = dfb_input_create_device(index, core, driver);

                    /* If cannot create the device within Linux Input
                     * provider, inform the user.
                     */
                    if ( DFB_OK != ret) {
                         D_DEBUG_AT( Debug_LinuxInput,
                                     "Linux/Input: Failed to create the "
                                     "device for /dev/input/event%d\n",
                                     device_num );
                    }
               }
          }
          else if (!strcmp(udev_event, "remove")) {
               D_DEBUG_AT( Debug_LinuxInput,
                           "Device node /dev/input/event%d is removed by udev\n",
                           device_num );
               ret = unregister_device_node( device_num, &index );

               if ( DFB_OK == ret) {
                    /* Handle the event that the input device node is removed */
                    ret = dfb_input_remove_device( index, driver );

                    /* If unable to remove the device within the Linux Input
                     * provider, just print the info.
                     */
                    if ( DFB_OK != ret) {
                         D_DEBUG_AT( Debug_LinuxInput,
                                     "Linux/Input: Failed to remove the "
                                     "device for /dev/input/event%d\n",
                                     device_num );
                    }
               }
          }

          /* Hotplug event handling is complete so release the lock. */
          pthread_mutex_unlock(&driver_suspended_lock);
     }

     D_DEBUG_AT( Debug_LinuxInput,
                 "Finished hotplug detection thread within Linux Input "
                 "provider.\n" );
     return NULL;

errorExit:
     D_INFO( "Linux/Input: Fail to open udev socket, disable detecting "
             "hotplug with Linux Input provider\n" );

     if (socket_fd != -1) {
          close(socket_fd);
     }

     return NULL;
}

/*
 * Stop hotplug detection thread.
 */
static DFBResult
stop_hotplug( void )
{
     int res;

     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     /* Exit immediately if the hotplug thread is not created successfully in
      * launch_hotplug().
      */
     if (!hotplug_thread)
          goto exit;

     /* Write to the hotplug quit pipe to cause the thread to terminate */
     res = write( hotplug_quitpipe[1], " ", 1 );
     (void)res;
     /* Shutdown the hotplug detection thread. */
     direct_thread_join(hotplug_thread);
     direct_thread_destroy(hotplug_thread);
     close( hotplug_quitpipe[0] );
     close( hotplug_quitpipe[1] );

     hotplug_thread = NULL;

     /* Destroy the suspended mutex. */
     pthread_mutex_destroy(&driver_suspended_lock);

     /* shutdown the connection of the socket */
     if (socket_fd > 0) {
          int rt = shutdown(socket_fd, SHUT_RDWR);
          if (rt < 0) {
               D_PERROR( "DirectFB/linux_input: Socket shutdown failed: %s\n",
                         strerror(errno) );
               return DFB_FAILURE;
          }
     }
     if (socket_fd > 0) {
          close(socket_fd);
          socket_fd = 0;
     }

exit:
     D_DEBUG_AT( Debug_LinuxInput, "%s() closed\n", __FUNCTION__ );
     return DFB_OK;
}

/*
 * Launch hotplug detection thread.
 */
static DFBResult
launch_hotplug(CoreDFB         *core,
               void            *input_driver)
{
     int ret;

     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     HotplugThreadData  *data;
     DFBResult           result;

     D_ASSERT( core != NULL );
     D_ASSERT( input_driver != NULL );
     D_ASSERT( hotplug_thread == NULL );

     data = D_CALLOC(1, sizeof(HotplugThreadData));

     if (!data) {
          D_OOM();
          result = DFB_UNSUPPORTED;
          goto errorExit;
     }

     data->core        = core;
     data->driver      = input_driver;

     /* open a pipe to awake the reader thread when we want to quit */
     ret = pipe( hotplug_quitpipe );
     if (ret < 0) {
          D_PERROR( "DirectFB/linux_input: could not open quitpipe for hotplug" );
          D_FREE( data );
          result = DFB_INIT;
          goto errorExit;
     }
     socket_fd = 0;

     /* Initialize a mutex used to communicate to the hotplug handling thread
      * when the driver is suspended.
      */
     pthread_mutex_init(&driver_suspended_lock, NULL);

     /* Create a thread to handle hotplug events. */
     hotplug_thread = direct_thread_create( DTT_INPUT,
                                             udev_hotplug_EventThread,
                                             data,
                                             "Hotplug with Linux Input" );
     if (!hotplug_thread) {
          pthread_mutex_destroy(&driver_suspended_lock);

          /* The hotplug thread normally deallocates the HotplugThreadData
           * memory, however since it could not be created it must be done
           * here.
           */
          D_FREE( data );

          result = DFB_UNSUPPORTED;
     }
     else
          result = DFB_OK;

errorExit:
     return result;
}

/*
 * Open the device, fill out information about it,
 * allocate and fill private data, start input thread.
 * Called during initialization, resuming or taking over mastership.
 */
static DFBResult
driver_open_device( CoreInputDevice  *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     int              fd, ret;
     bool             touchpad;
     unsigned long    ledbit[NBITS(LED_CNT)];
     LinuxInputData  *data;

     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     /* open device */
     fd = open( device_names[number], O_RDWR );
     if (fd < 0) {
          D_DEBUG_AT( Debug_LinuxInput,
                      "DirectFB/linux_input: could not open device %s\n",
                      device_names[number] );
          return DFB_INIT;
     }

     /* grab device */
     if (dfb_config->linux_input_grab) {
          ret = ioctl( fd, EVIOCGRAB, 1 );
          /* 2.4 kernels don't have EVIOCGRAB so ignore EINVAL */
          if (ret && errno != EINVAL) {
               D_PERROR( "DirectFB/linux_input: could not grab device" );
               close( fd );
               return DFB_INIT;
          }
     }

     /* fill device info structure */
     get_device_info( fd, info, &touchpad );

     /* allocate and fill private data */
     data = D_CALLOC( 1, sizeof(LinuxInputData) );
     if (!data) {
          if (dfb_config->linux_input_grab)
               ioctl( fd, EVIOCGRAB, 0 );
          close( fd );
          return D_OOM();
     }

     data->fd          = fd;
     data->device      = device;
#ifndef DIRECTFB_DISABLE_DEPRECATED
     data->has_keys    = (info->desc.caps & DICAPS_KEYS) != 0;
#else
     data->has_keys    = (info->desc.caps & DIDCAPS_KEYS) != 0;
#endif
     data->touchpad    = touchpad;
     data->vt_fd       = -1;
     data->sensitivity = 0x100;

      /* Track associated entry in device_nums and device_names array. */
      data->index = number;

     if (info->desc.min_keycode >= 0 && info->desc.max_keycode >= info->desc.min_keycode) {
          if (dfb_system_type() == CORE_FBDEV) {
               FBDev *dfb_fbdev = dfb_system_data();

               if (dfb_fbdev->vt)
                    data->vt_fd = dup( dfb_fbdev->vt->fd );
          }
          if (data->vt_fd < 0)
               data->vt_fd = open( "/dev/tty", O_RDWR | O_NOCTTY );

          if (data->vt_fd < 0)
               D_WARN( "no keymap support (requires /dev/tty - CONFIG_VT)" );
     }

     /* check if the device has LEDs */
     ret = ioctl( fd, EVIOCGBIT(EV_LED, sizeof(ledbit)), ledbit );
     if (ret < 0)
          D_PERROR( "DirectFB/linux_input: could not get LED bits" );
     else
          data->has_leds = test_bit( LED_SCROLLL, ledbit ) ||
                           test_bit( LED_NUML, ledbit ) ||
                           test_bit( LED_CAPSL, ledbit );

     if (data->has_leds) {
          /* get LED state */
          ret = ioctl( fd, EVIOCGLED(sizeof(data->led_state)), data->led_state );
          if (ret < 0) {
               D_PERROR( "DirectFB/linux_input: could not get LED state" );
               goto driver_open_device_error;
          }

          /* turn off LEDs */
          set_led( data, LED_SCROLLL, 0 );
          set_led( data, LED_NUML, 0 );
          set_led( data, LED_CAPSL, 0 );
     }

     /* open a pipe to awake the reader thread when we want to quit */
     ret = pipe( data->quitpipe );
     if (ret < 0) {
          D_PERROR( "DirectFB/linux_input: could not open quitpipe" );
          goto driver_open_device_error;
     }

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, linux_input_EventThread, data, "Linux Input" );

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;

driver_open_device_error:
     if (dfb_config->linux_input_grab)
          ioctl( fd, EVIOCGRAB, 0 );
     if (data->vt_fd >= 0)
          close( data->vt_fd );
     close( fd );
     D_FREE( data );

     return DFB_INIT;
}

/*
 * Obtain information about an axis (only absolute axis so far).
 */
static DFBResult
driver_get_axis_info( CoreInputDevice              *device,
                      void                         *driver_data,
                      DFBInputDeviceAxisIdentifier  axis,
                      DFBInputDeviceAxisInfo       *ret_info )
{
     LinuxInputData *data = (LinuxInputData*) driver_data;

     if (data->touchpad && !dfb_config->linux_input_touch_abs)
          return DFB_OK;

     if (axis <= ABS_PRESSURE && axis < DIAI_LAST) {
          unsigned long absbit[NBITS(ABS_CNT)];

          /* check if we have an absolute axes */
          ioctl( data->fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit );

          if (test_bit (axis, absbit)) {
               Input_AbsInfo absinfo;

               if (ioctl( data->fd, EVIOCGABS(axis), &absinfo ) == 0 &&
                   (absinfo.minimum || absinfo.maximum)) {
                    ret_info->flags  |= DIAIF_ABS_MIN | DIAIF_ABS_MAX;
                    ret_info->abs_min = absinfo.minimum;
                    ret_info->abs_max = absinfo.maximum;
               }
          }
     }

     return DFB_OK;
}

static DFBResult
driver_set_configuration( CoreInputDevice              *device,
                          void                         *driver_data,
                          const DFBInputDeviceConfig   *config )
{
     LinuxInputData *data = (LinuxInputData*) driver_data;

     if (config->flags & DIDCONF_SENSITIVITY)
          data->sensitivity = config->sensitivity;

     return DFB_OK;
}

/*
 * Fetch one entry from the kernel keymap.
 */
static DFBResult
driver_get_keymap_entry( CoreInputDevice           *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
     LinuxInputData             *data = (LinuxInputData*) driver_data;
     int                         code = entry->code;
     unsigned short              value;
     DFBInputDeviceKeyIdentifier identifier;

     if (data->vt_fd < 0)
          return DFB_UNSUPPORTED;

     /* fetch the base level */
     value = keyboard_read_value( driver_data, K_NORMTAB, code );

     /* get the identifier for basic mapping */
     identifier = keyboard_get_identifier( code, value );

     /* is CapsLock effective? */
     if (KTYP(value) == KT_LETTER)
          entry->locks |= DILS_CAPS;

     /* is NumLock effective? */
     if (identifier >= DIKI_KP_DECIMAL && identifier <= DIKI_KP_9)
          entry->locks |= DILS_NUM;

     /* write identifier to entry */
     entry->identifier = identifier;

     /* write base level symbol to entry */
     entry->symbols[DIKSI_BASE] = keyboard_get_symbol( code, value, DIKSI_BASE );


     /* fetch the shifted base level */
     value = keyboard_read_value( driver_data, K_SHIFTTAB, entry->code );

     /* write shifted base level symbol to entry */
     entry->symbols[DIKSI_BASE_SHIFT] = keyboard_get_symbol( code, value,
                                                             DIKSI_BASE_SHIFT );


     /* fetch the alternative level */
     value = keyboard_read_value( driver_data, K_ALTTAB, entry->code );

     /* write alternative level symbol to entry */
     entry->symbols[DIKSI_ALT] = keyboard_get_symbol( code, value, DIKSI_ALT );


     /* fetch the shifted alternative level */
     value = keyboard_read_value( driver_data, K_ALTSHIFTTAB, entry->code );

     /* write shifted alternative level symbol to entry */
     entry->symbols[DIKSI_ALT_SHIFT] = keyboard_get_symbol( code, value,
                                                            DIKSI_ALT_SHIFT );

     return DFB_OK;
}

/*
 * End thread, close device and free private data.
 */
static void
driver_close_device( void *driver_data )
{
     int res;
     LinuxInputData *data = (LinuxInputData*) driver_data;

     D_DEBUG_AT( Debug_LinuxInput, "%s()\n", __FUNCTION__ );

     /* stop input thread */
     res = write( data->quitpipe[1], " ", 1 );
     (void)res;
     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );
     close( data->quitpipe[0] );
     close( data->quitpipe[1] );

     if (data->has_leds) {
          /* restore LED state */
          set_led( data, LED_SCROLLL, test_bit( LED_SCROLLL, data->led_state ) );
          set_led( data, LED_NUML, test_bit( LED_NUML, data->led_state ) );
          set_led( data, LED_CAPSL, test_bit( LED_CAPSL, data->led_state ) );
     }

     /* release device */
     if (dfb_config->linux_input_grab)
          ioctl( data->fd, EVIOCGRAB, 0 );

  if (data->vt_fd >= 0)
          close( data->vt_fd );

     /* close file */
     close( data->fd );

     /* free private data */
     D_FREE( data );

     D_DEBUG_AT( Debug_LinuxInput, "%s() closed\n", __FUNCTION__ );
}

static bool
timeout_is_set( const struct timeval *timeout )
{
     return timeout->tv_sec || timeout->tv_usec;
}

static bool
timeout_passed( const struct timeval *timeout, const struct timeval *current )
{
     return !timeout_is_set( timeout ) ||
          current->tv_sec > timeout->tv_sec ||
          (current->tv_sec == timeout->tv_sec && current->tv_usec > timeout->tv_usec);
}

static void
timeout_clear( struct timeval *timeout )
{
     timeout->tv_sec  = 0;
     timeout->tv_usec = 0;
}

static void
timeout_add( struct timeval *timeout, const struct timeval *add )
{
     timeout->tv_sec += add->tv_sec;
     timeout->tv_usec += add->tv_usec;
     while (timeout->tv_usec >= 1000000) {
          timeout->tv_sec++;
          timeout->tv_usec -= 1000000;
     }
}

static void
timeout_sub( struct timeval *timeout, const struct timeval *sub )
{
     timeout->tv_sec -= sub->tv_sec;
     timeout->tv_usec -= sub->tv_usec;
     while (timeout->tv_usec < 0) {
          timeout->tv_sec--;
          timeout->tv_usec += 1000000;
     }
}

static void
touchpad_fsm_init( struct touchpad_fsm_state *state )
{
     state->x.old = -1;
     state->y.old = -1;
     state->fsm_state = TOUCHPAD_FSM_START;
     timeout_clear( &state->timeout );
}

static int
touchpad_normalize( const struct touchpad_axis *axis, int value )
{
     return ((value - axis->min) << 9) / (axis->max - axis->min);
}

static int
touchpad_translate( struct touchpad_fsm_state *state,
                    const struct input_event  *levt,
                    DFBInputEvent             *devt )
{
     struct touchpad_axis *axis = NULL;
     int abs, rel;

     devt->flags     = DIEF_TIMESTAMP | (dfb_config->linux_input_touch_abs ? DIEF_AXISABS : DIEF_AXISREL);
     devt->timestamp = levt->time;
     devt->type      = DIET_AXISMOTION;

     switch (levt->code) {
     case ABS_X:
          axis       = &state->x;
          devt->axis = DIAI_X;
          break;
     case ABS_Y:
          axis       = &state->y;
          devt->axis = DIAI_Y;
          break;
     default:
          return 0;
     }

     abs = touchpad_normalize( axis, levt->value );
     if (axis->old == -1)
          axis->old = abs;
     rel = abs - axis->old;

#define ACCEL_THRESHOLD 25
#define ACCEL_NUM       3
#define ACCEL_DENOM     1

     if (rel > ACCEL_THRESHOLD)
          rel += (rel - ACCEL_THRESHOLD) * ACCEL_NUM / ACCEL_DENOM;
     else if (rel < -ACCEL_THRESHOLD)
          rel += (rel + ACCEL_THRESHOLD) * ACCEL_NUM / ACCEL_DENOM;

     axis->old     = abs;
     devt->axisrel = rel;
     devt->axisabs = levt->value;

     return 1;
}

static bool
touchpad_finger_landing( const struct input_event *levt )
{
     return levt->type == EV_KEY && levt->code == BTN_TOUCH && levt->value == 1;
}

static bool
touchpad_finger_leaving( const struct input_event *levt )
{
     return levt->type == EV_KEY && levt->code == BTN_TOUCH && levt->value == 0;
}

static bool
touchpad_finger_moving( const struct input_event *levt )
{
     return levt->type == EV_ABS && (levt->code == ABS_X || levt->code == ABS_Y);
}

/*
 * This FSM takes into accout finger landing on touchpad and leaving and
 * translates absolute DFBInputEvent into a relative one
 */
static int
touchpad_fsm( struct touchpad_fsm_state *state,
              const struct input_event  *levt,
              DFBInputEvent             *devt )
{
     struct timeval timeout = { 0, 125000 };

     /* select() timeout? */
     if (!levt) {
          /* Check if button release is due. */
          if (state->fsm_state == TOUCHPAD_FSM_DRAG_START) {
               devt->flags     = DIEF_TIMESTAMP;
               devt->timestamp = state->timeout; /* timeout of current time? */
               devt->type      = DIET_BUTTONRELEASE;
               devt->button    = DIBI_FIRST;

               touchpad_fsm_init( state );
               return 1;
          }

          /* Already passed, clear it so select() won't return until there is something to do. */
          timeout_clear( &state->timeout );
          return 0;
     }

     /* More or less ignore these events for now */
     if ((levt->type == EV_SYN && levt->code == SYN_REPORT) ||
         (levt->type == EV_ABS && levt->code == ABS_PRESSURE) ||
         (levt->type == EV_ABS && levt->code == ABS_TOOL_WIDTH) ||
         (levt->type == EV_KEY && levt->code == BTN_TOOL_FINGER) ||
         (levt->type == EV_KEY && levt->code == BTN_TOOL_DOUBLETAP) ||
         (levt->type == EV_KEY && levt->code == BTN_TOOL_TRIPLETAP)) {

          /* Check if button release is due. */
          if (state->fsm_state == TOUCHPAD_FSM_DRAG_START &&
              timeout_passed( &state->timeout, &levt->time )) {
               devt->flags     = DIEF_TIMESTAMP;
               devt->timestamp = state->timeout; /* timeout of levt->time? */
               devt->type      = DIET_BUTTONRELEASE;
               devt->button    = DIBI_FIRST;

               touchpad_fsm_init( state );
               return 1;
          }

          return 0;
     }

     /* Use translate_event() for other events. */
     if (!(levt->type == EV_KEY && levt->code == BTN_TOUCH) &&
         !(levt->type == EV_ABS && (levt->code == ABS_X || levt->code == ABS_Y)))
          return -1;

     switch (state->fsm_state) {
     case TOUCHPAD_FSM_START:
          if (touchpad_finger_landing( levt )) {
               state->fsm_state = TOUCHPAD_FSM_MAIN;
               state->timeout = levt->time;
               timeout_add( &state->timeout, &timeout );
          }
          return 0;

     case TOUCHPAD_FSM_MAIN:
          if (touchpad_finger_moving( levt )) {
               if (1){//timeout_passed( &state->timeout, &levt->time )) {
                    //timeout_clear( &state->timeout );
                    return touchpad_translate( state, levt, devt );
               }
          }
          else if (touchpad_finger_leaving( levt )) {
               if (!timeout_passed( &state->timeout, &levt->time )) {
                    devt->flags     = DIEF_TIMESTAMP;
                    devt->timestamp = levt->time;
                    devt->type      = DIET_BUTTONPRESS;
                    devt->button    = DIBI_FIRST;

                    touchpad_fsm_init( state );
                    state->fsm_state = TOUCHPAD_FSM_DRAG_START;
                    state->timeout = levt->time;
                    timeout_add( &state->timeout, &timeout );
                    return 1;
               }
               else {
                    touchpad_fsm_init( state );
               }
          }
          return 0;

     case TOUCHPAD_FSM_DRAG_START:
          if (timeout_passed( &state->timeout, &levt->time )){
               devt->flags     = DIEF_TIMESTAMP;
               devt->timestamp = state->timeout; /* timeout of levt->time? */
               devt->type      = DIET_BUTTONRELEASE;
               devt->button    = DIBI_FIRST;

               touchpad_fsm_init(state);
               return 1;
          }
          else {
               if (touchpad_finger_landing( levt )) {
                    state->fsm_state = TOUCHPAD_FSM_DRAG_MAIN;
                    state->timeout = levt->time;
                    timeout_add( &state->timeout, &timeout );
               }
          }
          return 0;

     case TOUCHPAD_FSM_DRAG_MAIN:
          if (touchpad_finger_moving( levt )) {
               if (1){//timeout_passed( &state->timeout, &levt->time )) {
                   //timeout_clear( &state->timeout );
                    return touchpad_translate( state, levt, devt );
               }
          }
          else if (touchpad_finger_leaving( levt )) {
               devt->flags     = DIEF_TIMESTAMP;
               devt->timestamp = levt->time;
               devt->type      = DIET_BUTTONRELEASE;
               devt->button    = DIBI_FIRST;

               touchpad_fsm_init( state );
               return 1;
          }
          return 0;

     default:
          return 0;
     }

     return 0;
}
