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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/vt.h>

#include <directfb.h>

#include <core/coredefs.h>

#include <linux/joystick.h>
#include "joystick.h"

static int fd[8];
char devicename[10] ="/dev/jsX";

DFBInputEvent joystick_handle_event(struct js_event jse);


int joystick_probe()
{
     int i;
     int joy_count = 0;

     for (i=0; i<8; i++) {
          devicename[7] = (char)(i+48);
          fd[i] = -1;
          fd[i] = open( devicename, O_RDONLY );
          if (fd[i] == -1)
               break;
          close (fd[i]);
          joy_count++;
     }
     return joy_count;
}

int joystick_init(InputDevice *device)
{
     devicename[7] = (char)(device->number+48);
     fd[device->number] = open( devicename, O_RDONLY);
     if (fd[device->number] == -1) {
          PERRORMSG ( "DirectFB/Joystick: Could not open `%s'!\n", devicename );
          return DFB_INIT; // no joystick available
     }

     sprintf( device->driver.name, "Joystick" );
     sprintf( device->driver.vendor, "convergence integrated media GmbH" );

     device->driver.version.major = 0;
     device->driver.version.minor = 9;

     device->id = DIDID_JOYSTICK + device->number;

     device->desc.caps = DICAPS_AXIS | DICAPS_BUTTONS;
     ioctl( fd[device->number], JSIOCGBUTTONS, &device->desc.max_button );
     ioctl( fd[device->number], JSIOCGAXES, &device->desc.max_axis );
     device->desc.max_button--;
     device->desc.max_axis--;

     device->EventThread = joystickEventThread;

     return DFB_OK;
}

void* joystickEventThread(void *device)
{
     InputDevice *joystick = (InputDevice*)device;

     struct js_event jse;

     while (read( fd[((InputDevice*)device)->number], &jse,
                  sizeof(struct js_event) ) > 0) {

          input_dispatch( joystick, joystick_handle_event(jse) );
     }

     return NULL;
}

DFBInputEvent joystick_handle_event(struct js_event jse)
{     
     DFBInputEvent event;
     switch (jse.type) {
          case JS_EVENT_BUTTON:
               if (jse.value)
                    event.type = DIET_BUTTONPRESS;                    
               else 
                    event.type = DIET_BUTTONRELEASE;
               
               event.flags = DIEF_BUTTON;
               event.button = jse.number;
               break;
          case JS_EVENT_AXIS:
               event.type = DIET_AXISMOTION;
               event.flags = DIEF_AXISABS;
               event.axis = jse.number;
               event.axisabs = jse.value;
               break;
     }
     return event;
}


void joystick_deinit(InputDevice *device)
{
     close( fd[device->number] );
}
