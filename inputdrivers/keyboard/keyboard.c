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
#include <core/reactor.h>
#include <core/vt.h>

#include <misc/conf.h>


static struct termios old_ts;

static DFBInputDeviceModifierKeys modifier_state = 0;

static DFBInputEvent keyboard_handle_code (unsigned char  code);
static unsigned char keyboard_translate   (unsigned short kb_value);
static unsigned char keyboard_get_ascii   (unsigned short kb_value);



static void* keyboardEventThread(void *device)
{
     InputDevice *keyboard = (InputDevice*)device;

     int readlen;
     unsigned char buf[256];

     // Read keyboard data
     while ((readlen = read(vt->fd, buf, 256)) >= 0) {
          int i;

          pthread_testcancel();

          for (i = 0; i < readlen; i++) {
               DFBInputEvent evt;

               pthread_testcancel();

               evt = keyboard_handle_code( buf[i] );
               reactor_dispatch( keyboard->reactor, &evt );
          }
     }

     if (readlen <= 0 && errno != EINTR)
          PERRORMSG ("keyboard thread died\n");

     pthread_testcancel();

     return NULL;
}

static DFBInputEvent keyboard_handle_code(unsigned char code)
{
     int keydown;
     struct kbentry entry;

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

     ioctl( vt->fd,KDGKBENT,&entry );

     {
          DFBInputEvent event;

          event.type = keydown ? DIET_KEYPRESS : DIET_KEYRELEASE;
          event.flags = DIEF_KEYCODE | DIEF_MODIFIERS;

          event.keycode = keyboard_translate( entry.kb_value );

          switch (event.keycode) {
               case DIKC_SHIFT:
                    if (keydown)
                         modifier_state |= DIMK_SHIFT;
                    else
                         modifier_state &= ~DIMK_SHIFT;
                    break;
               case DIKC_CTRL:
                    if (keydown)
                         modifier_state |= DIMK_CTRL;
                    else
                         modifier_state &= ~DIMK_CTRL;
                    break;
               case DIKC_ALT:
                    if (keydown)
                         modifier_state |= DIMK_ALT;
                    else
                         modifier_state &= ~DIMK_ALT;
                    break;
               case DIKC_ALTGR:
                     if (keydown)
                          modifier_state |= DIMK_ALTGR;
                     else
                          modifier_state &= ~DIMK_ALTGR;
                     break;
                default:
                    break;
          }

          if (modifier_state & DIMK_SHIFT) {
               if (modifier_state & DIMK_ALT)
                    entry.kb_table = K_ALTSHIFTTAB;
               else
                    entry.kb_table = K_SHIFTTAB;

               ioctl( vt->fd, KDGKBENT, &entry );
          }
          else if (modifier_state & DIMK_ALTGR) {
               entry.kb_table = K_ALTTAB;
               ioctl( vt->fd,KDGKBENT, &entry );
          }

          event.modifiers = modifier_state;
          event.key_ascii = keyboard_get_ascii( entry.kb_value );

          return event;
     }
}

static unsigned char keyboard_get_ascii(unsigned short kb_value)
{
     unsigned char key_type = (kb_value & 0xFF00) >> 8;
     unsigned char key_index = kb_value & 0xFF;

     switch (key_type) {
          case KT_LETTER:
          case KT_LATIN:
          case KT_PAD:
          case KT_ASCII:
               return key_index;
          /* some special keys also have ascii values */
          case KT_SPEC:
               if (kb_value == K_ENTER) {
                    return 13;
               }
          default:
               HEAVYDEBUGMSG( "key typed has no ascii value (key_type: %d)\n",
                         key_type );
               return 0;
     }
}

static unsigned char keyboard_translate(unsigned short kb_value)
{
     unsigned char key_type = (kb_value & 0xFF00) >> 8;
     unsigned char key_index = kb_value & 0xFF;

     HEAVYDEBUGMSG( "DirectFB/Keyboard: key_type 0x%x, key_index 0x%x\n",
                    key_type, key_index );

     switch (key_type) {
          case KT_FN:
               if (key_index < 12)
                    return DIKC_F1 + key_index;
               break;
          case KT_LETTER:
               if (key_index >= 'a'  &&  key_index <= 'z')
                    return DIKC_A + key_index - 'a';
               break;
          case KT_LATIN:
               switch (key_index) {
                    case 0x09:
                         return DIKC_TAB;
                    case 0x1b:
                         return DIKC_ESCAPE;
                    case 0x1c:
                         return DIKC_PRINT;
                    case 0x20:
                         return DIKC_SPACE;
                    case 0x30 ... 0x39:
                         return DIKC_0 + key_index - 0x30;
                    case 0x7f:
                         return DIKC_BACKSPACE;
               }
               break;
          case KT_PAD:
               if (key_index <= 9)
                    return DIKC_0 + key_index;
               break;
     }

     switch (kb_value) {
          case K_LEFT:   return DIKC_LEFT;
          case K_RIGHT:  return DIKC_RIGHT;
          case K_UP:     return DIKC_UP;
          case K_DOWN:   return DIKC_DOWN;
          case K_ENTER:  return DIKC_ENTER;

          case K_CTRL:   return DIKC_CTRL;
          case K_SHIFT:  return DIKC_SHIFT;
          case K_ALT:    return DIKC_ALT;
          case K_ALTGR:  return DIKC_ALTGR;
          case K_INSERT: return DIKC_INSERT;
          case K_REMOVE: return DIKC_DELETE;
          case K_FIND:   return DIKC_HOME;
          case K_SELECT: return DIKC_END;
          case K_PGUP:   return DIKC_PAGEUP;
          case K_PGDN:   return DIKC_PAGEDOWN;
          case K_CAPS:   return DIKC_CAPSLOCK;
          case K_NUM:    return DIKC_NUMLOCK;
          case K_HOLD:   return DIKC_SCRLOCK;
          case K(1,29):  return DIKC_PAUSE;
          case K_PSLASH: return DIKC_KP_DIV;
          case K_PSTAR:  return DIKC_KP_MULT;
          case K_PMINUS: return DIKC_KP_MINUS;
          case K_PPLUS:  return DIKC_KP_PLUS;
          case K_PENTER: return DIKC_KP_ENTER;
     }

     return DIKC_UNKNOWN;
}


/* exported symbols */

int driver_probe()
{
     return 1;
}

int driver_init(InputDevice *device)
{
//     char buf[32];
     struct termios ts;
     const char cursoroff_str[] = "\033[?1;0;0c";
     const char blankoff_str[] = "\033[9;0]";

/*     sprintf(buf, "/dev/tty%d", vt->num);
     fd = open( buf, O_RDWR );
     if (fd < 0) {
          if (errno == ENOENT) {
               sprintf(buf, "/dev/vc/%d", vt->num);
               fd = open( buf, O_RDWR );
               if (fd < 0) {
                    if (errno == ENOENT) {
                         PERRORMSG( "DirectFB/Keyboard: Couldn't open neither `/dev/tty%d' nor `/dev/vc/%d'!\n", vt->num, vt->num );
                    }
                    else {
                         PERRORMSG( "DirectFB/Keyboard: Error opening `%s'!\n", buf );
                    }

                    return DFB_INIT;
               }
          }
          else {
               PERRORMSG( "DirectFB/Keyboard: Error opening `%s'!\n", buf );
               return DFB_INIT;
          }
     }*/

     if (dfb_config->kd_graphics) {
          if (ioctl( vt->fd, KDSETMODE, KD_GRAPHICS ) < 0) {
               PERRORMSG( "DirectFB/Keyboard: KD_GRAPHICS failed!\n" );
               return DFB_INIT;
          }
     }

     if (ioctl( vt->fd, KDSKBMODE, K_MEDIUMRAW ) < 0) {
          PERRORMSG( "DirectFB/Keyboard: K_MEDIUMRAW failed!\n" );
          return DFB_INIT;
     }

     if (!dfb_config->no_vt_switch) {
          ioctl( 0, TIOCNOTTY, 0 );
          ioctl( vt->fd, TIOCSCTTY, 0 );
     }

     tcgetattr( vt->fd, &old_ts );

     ts = old_ts;
     ts.c_cc[VTIME] = 0;
     ts.c_cc[VMIN] = 1;
     ts.c_lflag &= ~(ICANON|ECHO|ISIG);
     ts.c_iflag = 0;
     tcsetattr( vt->fd, TCSAFLUSH, &ts );

     tcsetpgrp( vt->fd, getpgrp() );

     write( vt->fd, cursoroff_str, strlen(cursoroff_str) );
     write( vt->fd, blankoff_str, strlen(blankoff_str) );


     device->info.driver_name = "Keyboard";
     device->info.driver_vendor = "convergence integrated media GmbH";

     device->info.driver_version.major = 0;
     device->info.driver_version.minor = 9;

     device->id = DIDID_KEYBOARD;
     device->desc.type = DIDTF_KEYBOARD;
     device->desc.caps = DICAPS_KEYS;

     device->EventThread = keyboardEventThread;

     return DFB_OK;
}

void driver_deinit(InputDevice *device)
{
     const char cursoron_str[] = "\033[?0;0;0c";
     const char blankon_str[] = "\033[9;10]";

     write( vt->fd, cursoron_str, strlen(cursoron_str) );
     write( vt->fd, blankon_str, strlen(blankon_str) );

     if (tcsetattr( vt->fd, TCSAFLUSH, &old_ts ) < 0)
          PERRORMSG("DirectFB/keyboard: tcsetattr for original values failed!\n");

     if (ioctl( vt->fd, KDSKBMODE, K_XLATE ) < 0)
          PERRORMSG("DirectFB/keyboard: Could not set mode to XLATE!\n");
     if (ioctl( vt->fd, KDSETMODE, KD_TEXT ) < 0)
          PERRORMSG("DirectFB/keyboard: Could not set terminal mode to text!\n");
}

