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
#include <linux/serial.h>

#include <directfb.h>

#include <misc/conf.h>

#include <core/coredefs.h>
#include <core/input.h>


static int fd = -1;


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

static void mouse_setspeed ()
{
     struct termios tty;

     tcgetattr (fd, &tty);

     tty.c_iflag = IGNBRK | IGNPAR;
     tty.c_oflag = 0;
     tty.c_lflag = 0;
     tty.c_line = 0;
     tty.c_cc[VTIME] = 0;
     tty.c_cc[VMIN] = 1;
     tty.c_cflag = CS7|CREAD|CLOCAL|HUPCL|B1200;

     tcsetattr (fd, TCSAFLUSH, &tty);

     write (fd, "*n", 2);
     usleep (100000);
}

void* mouseEventThread(void *device)
{
     InputDevice *mouse = (InputDevice*)device;

     unsigned char buf[256];
     unsigned char packet[3];
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

               if (pos == 0  && !(buf[i] & 0x40))
                    continue;

               packet[pos++] = buf[i];

               if (pos == 3) {
                    int dx, dy;
                    int buttons;

                    pos = 0;
                    
                    buttons = packet[0];
                    dx = (signed char) 
                         (((packet[0] & 0x03) << 6) | (packet[1] & 0x3f));
                    dy = (signed char) 
                         (((packet[0] & 0x0C) << 4) | (packet[2] & 0x3f));
                    
                    mouse_motion_compress( dx, dy );
                    
                    if (!dfb_config->mouse_motion_compression)
                         mouse_motion_realize( device );
                         
                    if (last_buttons != buttons) {
                         DFBInputEvent evt;
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

                         last_buttons = buttons;
                    }
                    else
                         if (i == readlen-1) {
                           /* test */
                           continue;
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

     if (!dfb_config->mouse_protocol || 
         strcmp (dfb_config->mouse_protocol, "ms"))
          return 0;

     fd = open( "/dev/mouse", O_RDWR | O_NONBLOCK );
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
     FD_ZERO(&set);
     FD_SET(fd, &set);
     timeout.tv_sec=0;
     timeout.tv_usec=100000;

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

     fd = open( "/dev/mouse", O_RDWR | O_NONBLOCK );
     if (fd < 0) {
          PERRORMSG( "DirectFB/SerialMouse: Error opening `/dev/mouse'!\n" );
          return DFB_INIT;
     }

     /* reset the O_NONBLOCK flag */
     fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) & ~O_NONBLOCK);

     mouse_setspeed ();

     driver_name = malloc( strlen("Serial Mouse ()") +
                           strlen(dfb_config->mouse_protocol) + 1 );
     sprintf( driver_name, "Serial Mouse (%s)", dfb_config->mouse_protocol );
     
     device->info.driver_name   = driver_name;
     device->info.driver_vendor = "convergence integrated media GmbH";

     device->info.driver_version.major = 0;
     device->info.driver_version.minor = 0;

     device->id = DIDID_MOUSE;

     device->desc.type = DIDTF_MOUSE;
     device->desc.caps = DICAPS_AXIS | DICAPS_BUTTONS;
     device->desc.max_axis = DIAI_Y;
     device->desc.max_button = DIBI_RIGHT; /* DIBI_MIDDLE */

     device->EventThread = mouseEventThread;

     return DFB_OK;
}

void driver_deinit(InputDevice *device)
{
     if (device->number != 0)
          return;

     free( device->info.driver_name );

     close( fd );
}
