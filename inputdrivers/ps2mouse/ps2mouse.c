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
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <sys/vt.h>

#include <linux/keyboard.h>

#include <termios.h>

#include <directfb.h>

#include <misc/conf.h>

#include <core/coredefs.h>

#include <core/vt.h>
#include "ps2mouse.h"

static int fd = -1;


int driver_probe()
{
     fd = open( "/dev/psaux", O_RDONLY );
     if (fd < 0) {
          fd = open( "/dev/input/mice", O_RDONLY );
          if (fd < 0) {
               return 0;
          }
     }
     close( fd );

     return 1;
}

int driver_init(InputDevice *device)
{
     fd = open( "/dev/psaux", O_RDONLY );
     if (fd < 0) {
          fd = open( "/dev/input/mice", O_RDONLY );
          if (fd < 0) {
               PERRORMSG( "DirectFB/PS2Mouse: Error opening `/dev/psaux' or `/dev/input/mice' !\n" );
               return DFB_INIT;
          }
     }

     sprintf( device->info.driver_name, "PS/2 Mouse" );
     sprintf( device->info.driver_vendor, "convergence integrated media GmbH" );

     device->info.driver_version.major = 0;
     device->info.driver_version.minor = 9;

     device->id = DIDID_MOUSE;
     device->desc.caps = DICAPS_AXIS | DICAPS_BUTTONS;
     device->desc.max_axis = DIAI_Y;
     device->desc.max_button = DIBI_MIDDLE;     /* TODO: probe!? */

     device->EventThread = ps2mouseEventThread;

     return DFB_OK;
}

void driver_deinit(InputDevice *device)
{
     if (device->number != 0)
          return;

     close( fd );
}

static DFBInputEvent x_motion;
static DFBInputEvent y_motion;

static inline void ps2mouse_motion_initialize()
{
     x_motion.type = y_motion.type = DIET_AXISMOTION;
     x_motion.flags = y_motion.flags = DIEF_AXISREL;
     x_motion.axisrel = y_motion.axisrel = 0;

     x_motion.axis = DIAI_X;
     y_motion.axis = DIAI_Y;
}

static inline void ps2mouse_motion_compress( int dx, int dy )
{
     x_motion.axisrel += dx;
     y_motion.axisrel += dy;
}

static inline void ps2mouse_motion_realize(void* device)
{
     if (x_motion.axisrel) {
          input_dispatch( device, x_motion );
          x_motion.axisrel = 0;
     }
     if (y_motion.axisrel) {
          input_dispatch( device, y_motion );
          y_motion.axisrel = 0;
     }
}

void* ps2mouseEventThread(void *device)
{
     InputDevice *ps2mouse = (InputDevice*)device;

     unsigned char packet[3];
     unsigned char pos = 0;
     unsigned char last_buttons = 0;

     int readlen;
     unsigned char buf[256];

     if (ps2mouse->number != 0)
          return NULL;

     ps2mouse_motion_initialize();

     // Read ps2 data
     while ((readlen = read(fd, buf, 256)) > 0) {
          int i;

          pthread_testcancel();
          
          for (i = 0; i < readlen; i++) {
//               DEBUGMSG( "--- %02x ---\n", buf[i] );

               if (pos == 0  &&  (buf[i] & 0xc0))
                    continue;

               packet[pos++] = buf[i];
               if (pos == 3) {
                    int dx, dy;
                    int buttons;

                    pos = 0;

                    buttons = packet[0] & 0x07;
                    dx = (packet[0] & 0x10) ?   packet[1]-256  :  packet[1];
                    dy = (packet[0] & 0x20) ? -(packet[2]-256) : -packet[2];

                    ps2mouse_motion_compress( dx, dy );

                    if (!config->ps2mouse_motion_compression)
                         ps2mouse_motion_realize( device );

                    if (last_buttons != buttons) {
                         DFBInputEvent evt;
                         unsigned char changed_buttons = last_buttons ^ buttons;

                         /* make sure the compressed motion event is dispatched
                            before any button change */
                         ps2mouse_motion_realize( device );

                         if (changed_buttons & 0x01) {
                              evt.type = (buttons & 0x01) ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_BUTTON;
                              evt.button = DIBI_LEFT;
                              input_dispatch( ps2mouse, evt );
                         }
                         if (changed_buttons & 0x02) {
                              evt.type = (buttons & 0x02) ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_BUTTON;
                              evt.button = DIBI_RIGHT;
                              input_dispatch( ps2mouse, evt );
                         }
                         if (changed_buttons & 0x04) {
                              evt.type = (buttons & 0x04) ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_BUTTON;
                              evt.button = DIBI_MIDDLE;
                              input_dispatch( ps2mouse, evt );
                         }

                         last_buttons = buttons;
                    } else
                    if (i == readlen-1) {
                         /* test */
                         continue;
                    }
               }
          }
          /* make sure the compressed motion event is dispatched,
             necessary if the last packet was a motion event */
          ps2mouse_motion_realize( device );
     }

     if (readlen <= 0 && errno != EINTR)
          PERRORMSG ("psmouse thread died\n");

     pthread_testcancel();

     return NULL;
}
