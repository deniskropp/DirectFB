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

#include <linux/keyboard.h>

#include <termios.h>

#include <directfb.h>

#include <core/coredefs.h>

#include <misc/conf.h>

#include <core/vt.h>
#include "keyboard.h"

static int fd = -1;

#ifdef OLD_ACTIVY
static unsigned char lshift = 0;
static unsigned char rshift = 0;
#endif

#ifndef NO_NEW_ACTIVY
static unsigned char lshift = 0;
static unsigned char rshift = 0;
#endif

DFBInputDeviceModifierKeys modifier_state = 0;

DFBInputEvent keyboard_handle_code(char code);
char keyboard_translate(int kb_value);
char keyboard_get_ascii(int kb_value);


int keyboard_probe()
{
     return 1;
}

int keyboard_init(InputDevice *device)
{
     char buf[32];
     struct termios ts;
     const char cursoroff_str[] = "\033[?1;0;0c";

     sprintf(buf, "/dev/tty%d", vt->num);
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
     }
     
     if (config->kd_graphics) {
          if (ioctl( fd, KDSETMODE, KD_GRAPHICS ) < 0) {
               PERRORMSG( "DirectFB/Keyboard: KD_GRAPHICS failed!\n" );
               return DFB_INIT;
          }
     }

     if (ioctl( fd, KDSKBMODE, K_MEDIUMRAW ) < 0) {
          PERRORMSG( "DirectFB/Keyboard: K_MEDIUMRAW failed!\n" );
          return DFB_INIT;
     }

     ioctl( 0, TIOCNOTTY, 0 );
     ioctl( fd, TIOCSCTTY, 0 );

     tcgetattr( fd, &ts );
     ts.c_cc[VTIME] = 0;
     ts.c_cc[VMIN] = 1;
     ts.c_lflag &= ~(ICANON|ECHO|ISIG);
     ts.c_iflag = 0;
     tcsetattr( fd, TCSAFLUSH, &ts );

     tcsetpgrp( fd, getpgrp() );

     write( fd, cursoroff_str, strlen(cursoroff_str) );



     sprintf( device->driver.name, "Keyboard" );
     sprintf( device->driver.vendor, "convergence integrated media GmbH" );

     device->driver.version.major = 0;
     device->driver.version.minor = 9;
     
     device->id = DIDID_KEYBOARD;
     device->desc.caps = DICAPS_KEYS;
     
     device->EventThread = keyboardEventThread;
     
     return DFB_OK;
}

void keyboard_deinit(InputDevice *device)
{
     if (fd < 0)
          return;

     ioctl( fd, KDSKBMODE, K_XLATE );
     ioctl( fd, KDSETMODE, KD_TEXT );
     close( fd );

     fd = -1;
}

void* keyboardEventThread(void *device)
{
     InputDevice *keyboard = (InputDevice*)device;
     
     int readlen;
     unsigned char buf[256];
     
     // Read keyboard data
     while ((readlen = read(fd, buf, 256)) > 0) {
          int i;
          
          pthread_testcancel();
          
          for (i = 0; i < readlen; i++) {
               input_dispatch( keyboard, keyboard_handle_code( buf[i] ) );
          }
     }
     
     if (readlen <= 0 && errno != EINTR)
          PERRORMSG ("keyboard thread died\n");
     
     pthread_testcancel();
     
     return NULL;
}

DFBInputEvent keyboard_handle_code(char code)
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
     
#ifdef OLD_ACTIVY
     switch (code) {
          case 0x2a:
               lshift = keydown;
               break;
          case 0x36:
               rshift = keydown;
               break;
     }
#endif
#ifndef NO_NEW_ACTIVY
     switch (code) {
          case 0x2a:
               lshift = keydown;
               break;
          case 0x36:
               rshift = keydown;
               break;
     }
#endif
               
     //fetch the keycode
                    
     entry.kb_table = K_NORMTAB;
     entry.kb_index = code;

     ioctl( fd,KDGKBENT,&entry );
     
     {
          DFBInputEvent event;
               
          event.type = keydown ? DIET_KEYPRESS : DIET_KEYRELEASE;
          event.flags = DIEF_KEYCODE;
          
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

               ioctl( fd, KDGKBENT, &entry );
          }  
          else if (modifier_state & DIMK_ALTGR) {
               entry.kb_table = K_ALTTAB;
               ioctl( fd,KDGKBENT, &entry );
          }

          event.modifiers = modifier_state;
          event.key_ascii = keyboard_get_ascii( entry.kb_value );

          return event;
     }
}

char keyboard_get_ascii(int kb_value)
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

char keyboard_translate(int kb_value)
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
#ifndef NO_NEW_ACTIVY
               if (lshift && rshift) {
                    switch (key_index) {
                         case 0x7a:
                              return DIKC_ESCAPE; //DIKC_RC_BACK;
                         case 0x61:
                              return DIKC_PAGEUP; //DIKC_RC_SCROLLUP;
                         case 0x69:
                              return DIKC_PAGEDOWN; //DIKC_RC_SCROLLDOWN;
                         case 0x78:
                              return DIKC_G; //DIKC_RC_GOTO;
                         case 0x6f:
                              return DIKC_K; //DIKC_RC_KEYBOARD;
                         case 0x79:
                              return DIKC_F1; //DIKC_RC_TV;
                         case 0x6d:
                              return DIKC_F2; //DIKC_RC_INTERNET;
                         case 0x67:
                              return DIKC_F3; //DIKC_RC_MAIL;
                         case 0x63:
                              return DIKC_F4; //DIKC_RC_PLAYER;
                         case 0x6b:
                              return DIKC_M; //DIKC_RC_MENU;
                         case 0x64:
                              return DIKC_ENTER; //DIKC_RC_OK;
                         case 0x68:
                              return DIKC_KP_PLUS; //DIKC_RC_CHANNELUP;
                         case 0x6a:
                              return DIKC_KP_MINUS; //DIKC_RC_CHANNELDOWN;
                         case 0x65:
                              return DIKC_H; //DIKC_RC_HELP;
                         case 0x76:
                              return DIKC_RC_MUTE; /* FIXME: period */
                         case 0x6c:
                              return DIKC_X; //DIKC_RC_VOLUMEUP;
                         case 0x77:
                              return DIKC_Y; //DIKC_RC_VOLUMEDOWN;
                         case 0x66:
                              return DIKC_F7; //DIKC_RC_REWIND;
                         case 0x73:
                              return DIKC_F5; //DIKC_RC_PLAYPAUSE;
                         case 0x72:
                              return DIKC_F8; //DIKC_RC_FORWARD;
                         case 0x74:
                              return DIKC_F11; //DIKC_RC_RECORD;
                         case 0x62:
                              return DIKC_F10; //DIKC_RC_TRACKPREV;
                         case 0x70:
                              return DIKC_F6; //DIKC_RC_STOP;
                         case 0x6e:
                              return DIKC_F9; //DIKC_RC_TRACKNEXT;
                         case 0x75:
                              return DIKC_F12; //DIKC_RC_EJECT;
                    }
               }
#endif
#ifdef OLD_ACTIVY
               if (lshift && rshift) {
                    switch (key_index) {
                         case 0x79:
                              return DIKC_TV;
                         case 0x6d:
                              return DIKC_INTERNET;
                         case 0x67:
                              return DIKC_MAIL;
                         case 0x71:
                              return DIKC_POWER;
                         case 0x63:
                              return DIKC_AUDIO;
                         case 0x6f:
                              return DIKC_VIDEO;
                         case 0x69:
                              return DIKC_FAVORITES;
                         case 0x74:
                              return DIKC_ZOOM;
                         case 0x66:
                              return DIKC_REWIND;
                         case 0x73:
                              return DIKC_PLAYPAUSE;
                         case 0x72:
                              return DIKC_FORWARD;
                         case 0x62:
                              return DIKC_PREVIOUS;
                         case 0x70:
                              return DIKC_STOP;
                         case 0x6e:
                              return DIKC_NEXT;
                         case 0x6c:
                              return DIKC_VOL_UP;
                         case 0x77:
                              return DIKC_VOL_DOWN;
                         case 0x76:
                              return DIKC_MUTE;
                         case 0x68:
                              return DIKC_CHANNEL_UP;
                         case 0x6a:
                              return DIKC_CHANNEL_DOWN;
                         case 0x6b:
                              return DIKC_MENU;
                         case 0x64:
                              return DIKC_OK;
                         case 0x61:
                              return DIKC_MODE;
                         case 0x38:
                              return DIKC_ASTERISK;
                         case 0x33:
                              return DIKC_HASHMARK;
                    }
               }
#endif
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
                    case 0x31:
#ifndef NO_NEW_ACTIVY
                         if (rshift && lshift)
                              return DIKC_HOME; //DIKC_RC_HOME;
#endif
                    case 0x30:
                    case 0x32:
                    case 0x33:
                    case 0x34:
                    case 0x35:
                    case 0x36:
                    case 0x37:
                    case 0x38:
                    case 0x39:
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
