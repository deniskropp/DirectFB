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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <sys/vt.h>

#include <errno.h>

#include <pthread.h>

#include <linux/keyboard.h>

#include <termios.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/vt.h>
#include <core/sig.h>

#include <misc/conf.h>
#include <misc/mem.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( keyboard )

typedef struct {
     InputDevice                *device;
     struct termios              old_ts;
     DFBInputDeviceModifierMask  modifier_state;
     DFBInputDeviceLockState     lock_state;
     DFBInputDeviceLockState     lock_pressed;
     pthread_t                   thread;
} KeyboardData;

static DFBInputDeviceKeySymbol
keyboard_translate( unsigned short kb_value )
{
     unsigned char key_type = (kb_value & 0xFF00) >> 8;
     unsigned char key_index = kb_value & 0xFF;

     HEAVYDEBUGMSG( "DirectFB/Keyboard: key_type 0x%x, key_index 0x%x\n",
                    key_type, key_index );

     switch (key_type) {
          case KT_FN:
               return DFB_FUNCTION_KEY( key_index + 1 );
          case KT_LETTER:
          case KT_LATIN:
               switch (key_index) {
                    case 0x1c:
                         return DIKS_PRINT;
                    case 0x7f:
                         return DIKS_BACKSPACE;
                    default:
                         return key_index;
               }
               break;
          case KT_PAD:
               if (key_index <= 9)
                    return DIKS_0 + key_index;
               break;
          case 0xe: /* special IPAQ H3600 case - AH */
               switch (key_index) {
                    case 0x20:     return DIKS_CALENDAR;
                    case 0x1a:     return DIKS_BACK;
                    case 0x1c:     return DIKS_MEMO;
                    case 0x21:     return DIKS_POWER;
               }
               break;
          case 0xd: /* another special IPAQ H3600 case - AH */
               switch (key_index) {
                    case 0x2:     return DIKS_DIRECTORY;
                    case 0x1:     return DIKS_MAIL;  /* Q on older iPaqs */
               }
               break;
     }

     switch (kb_value) {
          case K_LEFT:   return DIKS_CURSOR_LEFT;
          case K_RIGHT:  return DIKS_CURSOR_RIGHT;
          case K_UP:     return DIKS_CURSOR_UP;
          case K_DOWN:   return DIKS_CURSOR_DOWN;
          case K_ENTER:  return DIKS_ENTER;
          case K_CTRL:   return DIKS_CONTROL;
          case K_SHIFT:  return DIKS_SHIFT;
          case K_ALT:    return DIKS_ALT;
          case K_ALTGR:  return DIKS_ALTGR;
          case K_INSERT: return DIKS_INSERT;
          case K_REMOVE: return DIKS_DELETE;
          case K_FIND:   return DIKS_HOME;
          case K_SELECT: return DIKS_END;
          case K_PGUP:   return DIKS_PAGE_UP;
          case K_PGDN:   return DIKS_PAGE_DOWN;
          case K_CAPS:   return DIKS_CAPSLOCK;
          case K_NUM:    return DIKS_NUMLOCK;
          case K_HOLD:   return DIKS_SCROLLLOCK;
          case K(1,29):  return DIKS_PAUSE;
#if 0
          case K_PSLASH: return DIKS_KP_DIV;
          case K_PSTAR:  return DIKS_KP_MULT;
          case K_PMINUS: return DIKS_KP_MINUS;
          case K_PPLUS:  return DIKS_KP_PLUS;
          case K_PENTER: return DIKS_KP_ENTER;
          case K_PCOMMA:
          case K_PDOT:   return DIKS_KP_DECIMAL;
#endif
     }

     return DIKS_NULL;
}

static DFBInputDeviceKeySymbol
adjust_pad_keys( DFBInputDeviceKeySymbol symbol )
{
#if 0
     switch (symbol) {
          case DIKS_KP_DECIMAL: return DIKS_KP_DELETE;
          case DIKS_KP_0:       return DIKS_KP_INSERT;
          case DIKS_KP_1:       return DIKS_KP_END;
          case DIKS_KP_2:       return DIKS_KP_DOWN;
          case DIKS_KP_3:       return DIKS_KP_PAGE_DOWN;
          case DIKS_KP_4:       return DIKS_KP_LEFT;
          case DIKS_KP_5:       return DIKS_KP_BEGIN;
          case DIKS_KP_6:       return DIKS_KP_RIGHT;
          case DIKS_KP_7:       return DIKS_KP_HOME;
          case DIKS_KP_8:       return DIKS_KP_UP;
          case DIKS_KP_9:       return DIKS_KP_PAGE_UP;
          default:
               ;
     }
#endif

     return symbol;
}

#define HANDLE_LOCK(flag)     if (keydown) { \
                                   if (! (data->lock_pressed & (flag))) { \
                                        data->lock_state   ^= (flag); \
                                        data->lock_pressed |= (flag); \
                                   } \
                              } \
                              else \
                                   data->lock_pressed &= ~(flag);

static DFBInputEvent
keyboard_handle_code( KeyboardData  *data, unsigned char code )
{
     int keydown;
     struct kbentry entry;
     DFBInputEvent event;

     if (code & 0x80) {
          code &= 0x7f;
          keydown = 0;
     }
     else {
          keydown = 1;
     }

     HEAVYDEBUGMSG( "DirectFB/Keyboard: kb_code 0x%x\n", code );

     /* fetch the keycode */

     entry.kb_table = K_NORMTAB;
     entry.kb_index = code;
     entry.kb_value = 0;

     ioctl( dfb_vt->fd, KDGKBENT, &entry );
     ioctl( dfb_vt->fd, KDGKBLED, &data->lock_state );

     event.type  = keydown ? DIET_KEYPRESS : DIET_KEYRELEASE;
     event.flags = DIEF_KEYCODE | DIEF_KEYSYMBOL | DIEF_MODIFIERS | DIEF_LOCKS;

     event.key_code = code;

     switch (keyboard_translate( entry.kb_value )) {
          case DIKS_SHIFT:
               if (keydown)
                    data->modifier_state |= DIMM_SHIFT;
               else
                    data->modifier_state &= ~DIMM_SHIFT;
               break;
          case DIKS_CONTROL:
               if (keydown)
                    data->modifier_state |= DIMM_CONTROL;
               else
                    data->modifier_state &= ~DIMM_CONTROL;
               break;
          case DIKS_ALT:
               if (keydown)
                    data->modifier_state |= DIMM_ALT;
               else
                    data->modifier_state &= ~DIMM_ALT;
               break;
          case DIKS_ALTGR:
               if (keydown)
                    data->modifier_state |= DIMM_ALTGR;
               else
                    data->modifier_state &= ~DIMM_ALTGR;
               break;
          case DIKS_CAPSLOCK:
               HANDLE_LOCK( DILS_CAPS );
               break;
          case DIKS_NUMLOCK:
               HANDLE_LOCK( DILS_NUM );
               break;
          case DIKS_SCROLLLOCK:
               HANDLE_LOCK( DILS_SCROLL );
               break;
          default:
               break;
     }
     /* Set the lock flags. We rely on these being
      * in the same order as defined by the kernel. */
     ioctl( dfb_vt->fd, KDSKBLED, data->lock_state );

     if (data->modifier_state & DIMM_SHIFT) {
          if (data->modifier_state & DIMM_ALT)
               entry.kb_table = K_ALTSHIFTTAB;
          else
               entry.kb_table = K_SHIFTTAB;

          ioctl( dfb_vt->fd, KDGKBENT, &entry );
     }
     else if (data->modifier_state & DIMM_ALTGR) {
          entry.kb_table = K_ALTTAB;
          ioctl( dfb_vt->fd, KDGKBENT, &entry );
     }
     if ( ((entry.kb_value & 0xFF00) >> 8 == KT_LETTER) &&
           (data->lock_state & DILS_CAPS)) {
          entry.kb_table = K_SHIFTTAB;
          ioctl( dfb_vt->fd, KDGKBENT, &entry );
     }

     event.modifiers = data->modifier_state;
     event.locks = data->lock_state;
     event.key_symbol = keyboard_translate( entry.kb_value );

     if ( ((entry.kb_value & 0xFF00) >> 8 == KT_PAD) &&
           !(data->lock_state & DILS_NUM)) {
          event.key_symbol = adjust_pad_keys(event.key_symbol);
     }
     
     return event;
}

static void*
keyboardEventThread( void *driver_data )
{
     int            readlen;
     unsigned char  buf[256];
     KeyboardData  *data = (KeyboardData*) driver_data;

     /* block all signals, they must not be handled by this thread */
     dfb_sig_block_all();

     /* Read keyboard data */
     while ((readlen = read (dfb_vt->fd, buf, 256)) >= 0 || errno == EINTR) {
          int i;

          pthread_testcancel();

          for (i = 0; i < readlen; i++) {
               DFBInputEvent evt;

               pthread_testcancel();

               evt = keyboard_handle_code( data, buf[i] );
               dfb_input_dispatch( data->device, &evt );
          }
     }

     if (readlen <= 0 && errno != EINTR)
          PERRORMSG ("keyboard thread died\n");

     pthread_testcancel();

     return NULL;
}

/* driver functions */

static int
driver_get_abi_version()
{
     return DFB_INPUT_DRIVER_ABI_VERSION;
}

static int
driver_get_available()
{
     return 1;
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
     KeyboardData   *data;
     struct termios  ts;
     const char      cursoroff_str[] = "\033[?1;0;0c";
     const char      blankoff_str[] = "\033[9;0]";

     /* put keyboard into medium raw mode */
     if (ioctl( dfb_vt->fd, KDSKBMODE, K_MEDIUMRAW ) < 0) {
          PERRORMSG( "DirectFB/Keyboard: K_MEDIUMRAW failed!\n" );
          return DFB_INIT;
     }
     
     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(KeyboardData) );

     data->device = device;

     tcgetattr( dfb_vt->fd, &data->old_ts );

     ts = data->old_ts;
     ts.c_cc[VTIME] = 0;
     ts.c_cc[VMIN] = 1;
     ts.c_lflag &= ~(ICANON|ECHO|ISIG);
     ts.c_iflag = 0;
     tcsetattr( dfb_vt->fd, TCSAFLUSH, &ts );

     tcsetpgrp( dfb_vt->fd, getpgrp() );

     write( dfb_vt->fd, cursoroff_str, strlen(cursoroff_str) );
     write( dfb_vt->fd, blankoff_str, strlen(blankoff_str) );

     /* fill device info structure */
     snprintf( info->name,
               DFB_INPUT_DEVICE_INFO_NAME_LENGTH, "Keyboard" );

     snprintf( info->vendor,
               DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH, "Unknown" );

     info->prefered_id = DIDID_KEYBOARD;

     info->desc.type   = DIDTF_KEYBOARD;
     info->desc.caps   = DICAPS_KEYS;

     /* start input thread */
     pthread_create( &data->thread, NULL, keyboardEventThread, data );

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}

static void
driver_close_device( void *driver_data )
{
     const char    cursoron_str[] = "\033[?0;0;0c";
     const char    blankon_str[] = "\033[9;10]";
     KeyboardData *data = (KeyboardData*) driver_data;

     /* stop input thread */
     pthread_cancel( data->thread );
     pthread_join( data->thread, NULL );

     write( dfb_vt->fd, cursoron_str, strlen(cursoron_str) );
     write( dfb_vt->fd, blankon_str, strlen(blankon_str) );

     if (tcsetattr( dfb_vt->fd, TCSAFLUSH, &data->old_ts ) < 0)
          PERRORMSG("DirectFB/keyboard: tcsetattr for original values failed!\n");

     if (ioctl( dfb_vt->fd, KDSKBMODE, K_XLATE ) < 0)
          PERRORMSG("DirectFB/keyboard: Could not set mode to XLATE!\n");
     if (ioctl( dfb_vt->fd, KDSETMODE, KD_TEXT ) < 0)
          PERRORMSG("DirectFB/keyboard: Could not set terminal mode to text!\n");

     /* free private data */
     DFBFREE( data );
}

