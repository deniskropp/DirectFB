/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,
              Andreas Hundt <andi@convergence.de> and
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
#include <errno.h>
#include <fcntl.h>

#include <termios.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <linux/serial.h>

#include <directfb.h>

#include <misc/conf.h>

#include <core/coredefs.h>
#include <core/input.h>

#define DEV_NAME "/dev/mouse"
#define MIDDLE   0x08

typedef enum
{
     PROTOCOL_MS,           /* two buttons MS protocol                            */
     PROTOCOL_MS3,          /* MS with ugly 3-button extension                    */
     PROTOCOL_MOUSEMAN,     /* referred to as MS + Logitech extension in mouse(4) */
     PROTOCOL_MOUSESYSTEMS, /* most commonly used serial mouse protocol nowadays  */
     LAST_PROTOCOL
} MouseProtocol;

static char *protocol_names[LAST_PROTOCOL] = 
{
     "MS",
     "MS3",
     "MouseMan",
     "MouseSystems"
};

static int fd = -1;
static MouseProtocol protocol = LAST_PROTOCOL;

static DFBInputEvent x_motion;
static DFBInputEvent y_motion;


static inline void mouse_motion_initialize()
{
     x_motion.type = y_motion.type = DIET_AXISMOTION;
     x_motion.flags = y_motion.flags = DIEF_AXISREL;
     x_motion.axisrel = y_motion.axisrel = 0;

     x_motion.axis = DIAI_X;
     y_motion.axis = DIAI_Y;
}

static inline void mouse_motion_compress( int dx, int dy )
{
     x_motion.axisrel += dx;
     y_motion.axisrel += dy;
}

static inline void mouse_motion_realize(InputDevice *device)
{
     if (x_motion.axisrel) {
          reactor_dispatch( device->reactor, &x_motion );
          x_motion.axisrel = 0;
     }
     if (y_motion.axisrel) {
          reactor_dispatch( device->reactor, &y_motion );
          y_motion.axisrel = 0;
     }
}

static void mouse_setspeed()
{
     struct termios tty;

     tcgetattr (fd, &tty);

     tty.c_iflag = IGNBRK | IGNPAR;
     tty.c_oflag = 0;
     tty.c_lflag = 0;
     tty.c_line = 0;
     tty.c_cc[VTIME] = 0;
     tty.c_cc[VMIN] = 1;
     tty.c_cflag = CREAD|CLOCAL|HUPCL|B1200;

     tty.c_cflag |= (protocol == PROTOCOL_MOUSESYSTEMS) ? CS8|CSTOPB : CS7;

     tcsetattr (fd, TCSAFLUSH, &tty);

     write (fd, "*n", 2);
}

/* the main routine for MS mice (plus extensions) */
void* mouseEventThread_ms(void *device)
{
     InputDevice *mouse = (InputDevice*)device;
     DFBInputEvent evt;

     unsigned char buf[256];
     unsigned char packet[4];
     unsigned char pos = 0;
     unsigned char last_buttons = 0;
     int dx, dy;
     int buttons;
     int readlen;
     int i;

     if (mouse->number != 0)
          return NULL;

     mouse_motion_initialize();

     /* Read data */
     while ((readlen = read( fd, buf, 256 )) >= 0 || errno == EINTR) {

          pthread_testcancel();

          for (i = 0; i < readlen; i++) {

               if (pos == 0  && !(buf[i] & 0x40))
                    continue;

               /* We did not reset the position in the mouse event handler
                  since a forth byte may follow. We check for it now and 
                  reset the position if this is a start byte. */
               if (pos == 3 && buf[i] & 0x40)
                    pos = 0;

               packet[pos++] = buf[i];

               switch (pos) {
               case 3:
                    if (protocol != PROTOCOL_MOUSEMAN)
                         pos = 0;
                    
                    buttons = packet[0] & 0x30;
                    dx = (signed char) 
                         (((packet[0] & 0x03) << 6) | (packet[1] & 0x3f));
                    dy = (signed char) 
                         (((packet[0] & 0x0C) << 4) | (packet[2] & 0x3f));
                    
                    mouse_motion_compress( dx, dy );
                    
                    if (protocol == PROTOCOL_MS3) {
                         if (!dx && !dy && buttons == (last_buttons & ~MIDDLE))
                              buttons = last_buttons ^ MIDDLE;  /* toggle    */
                         else
                              buttons |= last_buttons & MIDDLE; /* preserve  */
                    }   

                    if (!dfb_config->mouse_motion_compression)
                         mouse_motion_realize( device );
                         
                    if (last_buttons != buttons) {
                         unsigned char changed_buttons = last_buttons ^ buttons;
                           
                         /* make sure the compressed motion event is dispatched
                            before any button change */
                         mouse_motion_realize( device );

                         if (changed_buttons & 0x20) {
                              evt.type = (buttons & 0x20) ? 
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_BUTTON;
                              evt.button = DIBI_LEFT;
                              reactor_dispatch( mouse->reactor, &evt );
                         }
                         if (changed_buttons & 0x10) {
                              evt.type = (buttons & 0x10) ? 
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_BUTTON;
                              evt.button = DIBI_RIGHT;
                              reactor_dispatch( mouse->reactor, &evt );
                         }
                         if (changed_buttons & MIDDLE) {
                              evt.type = (buttons & MIDDLE) ? 
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_BUTTON;
                              evt.button = DIBI_MIDDLE;
                              reactor_dispatch( mouse->reactor, &evt );
                         }

                         last_buttons = buttons;
                    }
                    break;

               case 4:
                    pos = 0;
                 
                    evt.type = (packet[3] & 0x20) ? 
                         DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                    evt.flags = DIEF_BUTTON;
                    evt.button = DIBI_MIDDLE;
                    reactor_dispatch( mouse->reactor, &evt );
                    break;
                    
               default:
                    break;
               }
          }

          /* make sure the compressed motion event is dispatched,
             necessary if the last packet was a motion event */
          if (readlen > 0)
               mouse_motion_realize( device );

          pthread_testcancel();
     }

     PERRORMSG ("serial mouse thread died\n");

     return NULL;
}

/* the main routine for MouseSystems */
void* mouseEventThread_mousesystems(void *device)
{
     InputDevice *mouse = (InputDevice*)device;

     unsigned char buf[256];
     unsigned char packet[5];
     unsigned char pos = 0;
     unsigned char last_buttons = 0;
     int i;
     int readlen;

     if (mouse->number != 0)
          return NULL;

     mouse_motion_initialize();

     /* Read data */
     while ((readlen = read( fd, buf, 256 )) >= 0 || errno == EINTR) {

          pthread_testcancel();

          for (i = 0; i < readlen; i++) {

               if (pos == 0  && (buf[i] & 0xf8) != 0x80)
                    continue;

               packet[pos++] = buf[i];

               if (pos == 5) {
                    int dx, dy;
                    int buttons;

                    pos = 0;
                    
                    buttons= (~packet[0]) & 0x07;
                    dx =    (signed char) (packet[1]) + (signed char)(packet[3]);
                    dy = - ((signed char) (packet[2]) + (signed char)(packet[4]));
                    
                    mouse_motion_compress( dx, dy );
                    
                    if (!dfb_config->mouse_motion_compression)
                         mouse_motion_realize( device );
                         
                    if (last_buttons != buttons) {
                         DFBInputEvent evt;
                         unsigned char changed_buttons = last_buttons ^ buttons;
                           
                         /* make sure the compressed motion event is dispatched
                            before any button change */
                         mouse_motion_realize( device );

                         if (changed_buttons & 0x04) {
                              evt.type = (buttons & 0x04) ? 
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_BUTTON;
                              evt.button = DIBI_LEFT;
                              reactor_dispatch( mouse->reactor, &evt );
                         }
                         if (changed_buttons & 0x01) {
                              evt.type = (buttons & 0x01) ? 
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_BUTTON;
                              evt.button = DIBI_MIDDLE;
                              reactor_dispatch( mouse->reactor, &evt );
                         }
                         if (changed_buttons & 0x02) {
                              evt.type = (buttons & 0x02) ? 
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_BUTTON;
                              evt.button = DIBI_RIGHT;
                              reactor_dispatch( mouse->reactor, &evt );
                         }

                         last_buttons = buttons;
                    }
               }
          }

          /* make sure the compressed motion event is dispatched,
             necessary if the last packet was a motion event */
          if (readlen > 0)
               mouse_motion_realize( device );

          pthread_testcancel();
     }

     PERRORMSG ("serial mouse thread died\n");

     return NULL;
}


/* exported symbols */

int driver_probe()
{
     struct serial_struct serial_info;
     struct timeval timeout;
     fd_set set;
     char buf[8];
     int readlen;
     int lines;

     if (!dfb_config->mouse_protocol)
          return 0;

     for (protocol = 0; protocol < LAST_PROTOCOL; protocol++) {
          if (strcasecmp (dfb_config->mouse_protocol, 
                          protocol_names[protocol]) == 0)
               break;
     }
     if (protocol == LAST_PROTOCOL)
          return 0;

     fd = open( DEV_NAME, O_RDWR | O_NONBLOCK );
     if (fd < 0)
          return 0;

     /*  test if this is a serial device  */
     if (ioctl( fd, TIOCGSERIAL, &serial_info ))
          goto error;

     /*  test if there's a mouse connected by lowering and raising RTS  */
     if (ioctl( fd, TIOCMGET, &lines ))
          goto error;

     lines ^= TIOCM_RTS;
     if (ioctl( fd, TIOCMSET, &lines ))
          goto error;
     usleep (1000);
     lines |= TIOCM_RTS;
     if (ioctl( fd, TIOCMSET, &lines ))
          goto error;

     /*  wait for the mouse to send 0x4D  */
     FD_ZERO (&set);
     FD_SET (fd, &set);
     timeout.tv_sec  = 0;
     timeout.tv_usec = 50000;

     while (select (fd+1, &set, NULL, NULL, &timeout) < 0 && errno == EINTR);
     if (FD_ISSET (fd, &set) && (readlen = read (fd, buf, 8) > 0)) {
          while (readlen--) {
               if (buf[8-readlen] == 0x4D)
                    break;
          }
          if (readlen) {
               close (fd);
               return 1;
          }
     }

 error:
     close (fd);
     return 0;
}

int driver_init(InputDevice *device)
{
     char *driver_name;

     fd = open( DEV_NAME, O_RDWR | O_NONBLOCK );
     if (fd < 0) {
          PERRORMSG( "DirectFB/SerialMouse: Error opening '"DEV_NAME"'!\n" );
          return DFB_INIT;
     }

     /* reset the O_NONBLOCK flag */
     fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) & ~O_NONBLOCK);

     mouse_setspeed ();

     driver_name = malloc( strlen("Serial Mouse ()") +
                           strlen(protocol_names[protocol]) + 1 );
     sprintf( driver_name, "Serial Mouse (%s)", protocol_names[protocol]);
     
     device->info.driver_name   = driver_name;
     device->info.driver_vendor = "convergence integrated media GmbH";

     device->info.driver_version.major = 0;
     device->info.driver_version.minor = 1;

     device->id = DIDID_MOUSE;

     device->desc.type = DIDTF_MOUSE;
     device->desc.caps = DICAPS_AXIS | DICAPS_BUTTONS;
     device->desc.max_axis = DIAI_Y;
     device->desc.max_button = 
          (protocol > PROTOCOL_MS) ? DIBI_MIDDLE : DIBI_RIGHT;
     
     if (protocol == PROTOCOL_MOUSESYSTEMS)
          device->EventThread = mouseEventThread_mousesystems;
     else
          device->EventThread = mouseEventThread_ms;

     return DFB_OK;
}

void driver_deinit(InputDevice *device)
{
     if (device->number != 0)
          return;

     free( device->info.driver_name );

     close( fd );
}
