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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <sys/vt.h>

#include <errno.h>

#include <fbdev/fbdev.h>

#include <linux/keyboard.h>

#include <termios.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/system.h>

#include <misc/conf.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>

#include <core/input_driver.h>



DFB_INPUT_DRIVER( keyboard )

typedef struct {
     InputDevice     *device;
     DirectThread    *thread;

     struct termios   old_ts;

     VirtualTerminal *vt;
} KeyboardData;

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
          case K_CAPS:    return dfb_config->capslock_meta ? DIKS_META :
                                                             DIKS_CAPS_LOCK;

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
          default:
               ;
     }

     /* special keys not in the map, hack? */
     if (code == 125)         /* left windows key */
          return DIKI_META_L;

     if (code == 126)         /* right windows key */
          return DIKI_META_R;

     if (code == 127)         /* context menu key */
          return DIKI_SUPER_R;

     return DIKI_UNKNOWN;
}

static unsigned short
keyboard_read_value( KeyboardData *data,
                     unsigned char table, unsigned char index )
{
     struct kbentry entry;

     entry.kb_table = table;
     entry.kb_index = index;
     entry.kb_value = 0;

     if (ioctl( data->vt->fd, KDGKBENT, &entry )) {
          D_PERROR("DirectFB/keyboard: KDGKBENT (table: %d, index: %d) "
                    "failed!\n", table, index);
          return 0;
     }

     return entry.kb_value;
}

static void
keyboard_set_lights( KeyboardData *data, DFBInputDeviceLockState locks )
{
     ioctl( data->vt->fd, KDSKBLED, locks );
}

static void*
keyboardEventThread( DirectThread *thread, void *driver_data )
{
     int            readlen;
     unsigned char  buf[64];
     KeyboardData  *data = (KeyboardData*) driver_data;

     /* Read keyboard data */
     while ((readlen = read (data->vt->fd, buf, 64)) >= 0 || errno == EINTR) {
          int i;

          direct_thread_testcancel( thread );

          for (i = 0; i < readlen; i++) {
               DFBInputEvent evt;

               evt.type     = ((buf[i] & 0x80) ?
                               DIET_KEYRELEASE : DIET_KEYPRESS);
               evt.flags    = DIEF_KEYCODE;
               evt.key_code = buf[i] & 0x7f;

               dfb_input_dispatch( data->device, &evt );

               keyboard_set_lights( data, evt.locks );
          }
     }

     if (readlen <= 0 && errno != EINTR)
          D_PERROR ("keyboard thread died\n");

     return NULL;
}

/* driver functions */

static int
driver_get_available()
{
     if (dfb_system_type() == CORE_FBDEV)
          return 1;

     return 0;
}

static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "Keyboard Driver" );

     snprintf( info->vendor,
               DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 9;
}

static DFBResult
driver_open_device( InputDevice      *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     struct termios  ts;
     KeyboardData   *data;
     FBDev          *dfb_fbdev = dfb_system_data();

     /* put keyboard into medium raw mode */
     if (ioctl( dfb_fbdev->vt->fd, KDSKBMODE, K_MEDIUMRAW ) < 0) {
          D_PERROR( "DirectFB/Keyboard: K_MEDIUMRAW failed!\n" );
          return DFB_INIT;
     }

     /* allocate and fill private data */
     data = D_CALLOC( 1, sizeof(KeyboardData) );

     data->device = device;
     data->vt     = dfb_fbdev->vt;

     tcgetattr( data->vt->fd, &data->old_ts );

     ts = data->old_ts;
     ts.c_cc[VTIME] = 0;
     ts.c_cc[VMIN] = 1;
     ts.c_lflag &= ~(ICANON|ECHO|ISIG);
     ts.c_iflag = 0;
     tcsetattr( data->vt->fd, TCSAFLUSH, &ts );

     tcsetpgrp( data->vt->fd, getpgrp() );

     /* fill device info structure */
     snprintf( info->desc.name,
               DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "Keyboard" );

     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "Unknown" );

     /* claim to be the primary keyboard */
     info->prefered_id = DIDID_KEYBOARD;

     /* classify as a keyboard able to produce key events */
     info->desc.type   = DIDTF_KEYBOARD;
     info->desc.caps   = DICAPS_KEYS;

     /* enable translation of raw hardware keycodes */
     info->desc.min_keycode = 0;
     info->desc.max_keycode = 127;

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, keyboardEventThread, data, "Keyboard Input" );

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}

/*
 * Fetch one entry from the kernel keymap.
 */
static DFBResult
driver_get_keymap_entry( InputDevice               *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
     int                         code = entry->code;
     unsigned short              value;
     DFBInputDeviceKeyIdentifier identifier;

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

static void
driver_close_device( void *driver_data )
{
     KeyboardData *data = (KeyboardData*) driver_data;

     /* stop input thread */
     direct_thread_cancel( data->thread );
     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     if (tcsetattr( data->vt->fd, TCSAFLUSH, &data->old_ts ) < 0)
          D_PERROR("DirectFB/keyboard: tcsetattr for original values failed!\n");

     if (ioctl( data->vt->fd, KDSKBMODE, K_XLATE ) < 0)
          D_PERROR("DirectFB/keyboard: Could not set mode to XLATE!\n");
     if (ioctl( data->vt->fd, KDSETMODE, KD_TEXT ) < 0)
          D_PERROR("DirectFB/keyboard: Could not set terminal mode to text!\n");

     /* free private data */
     D_FREE( data );
}

