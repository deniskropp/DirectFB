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

#include <linux/h3600_ts.h>

#include <pthread.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/reactor.h>

#include <misc/conf.h>


static int fd = -1;

static void* h3600_tsEventThread(void *device)
{
     InputDevice *h3600_ts = (InputDevice*)device;

     TS_EVENT ts_event;

     int readlen;

     unsigned short old_x = -1;
     unsigned short old_y = -1;
     unsigned short old_pressure = 0;

     while ((readlen = read(fd, &ts_event, sizeof(TS_EVENT))) > 0  ||
            errno == EINTR)
     {
          DFBInputEvent evt;

          pthread_testcancel();

          if (ts_event.pressure) {
               if (ts_event.x != old_x) {
                    evt.type    = DIET_AXISMOTION;
                    evt.flags   = DIEF_AXISABS;
                    evt.axis    = DIAI_X;
                    evt.axisabs = ts_event.x;

                    reactor_dispatch( h3600_ts->reactor, &evt );

                    old_x = ts_event.x;
               }

               if (ts_event.y != old_y) {
                    evt.type    = DIET_AXISMOTION;
                    evt.flags   = DIEF_AXISABS;
                    evt.axis    = DIAI_Y;
                    evt.axisabs = ts_event.y;

                    reactor_dispatch( h3600_ts->reactor, &evt );

                    old_y = ts_event.y;
               }
          }

          if ((ts_event.pressure && !old_pressure) ||
              (!ts_event.pressure && old_pressure)) {
               evt.type    = ts_event.pressure ?
                             DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
               evt.flags   = DIEF_BUTTON;
               evt.button  = DIBI_LEFT;

               reactor_dispatch( h3600_ts->reactor, &evt );

               old_pressure = ts_event.pressure;
          }
     }

     if (readlen <= 0)
          PERRORMSG ("H3600 Touchscreen thread died\n");

     pthread_testcancel();

     return NULL;
}


/* exported symbols */

int driver_probe()
{
     int fd;

     fd = open( "/dev/ts", O_RDONLY | O_NOCTTY );
     if (fd < 0)
          return 0;

     close( fd );

     return 1;
}

int driver_init(InputDevice *device)
{
     fd = open( "/dev/ts", O_RDONLY | O_NOCTTY );
     if (fd < 0) {
          PERRORMSG( "DirectFB/H3600: Error opening `/dev/ts'!\n" );
          return DFB_INIT;
     }

     device->info.driver_name = "H3600 Touchscreen";
     device->info.driver_vendor = "convergence integrated media GmbH";

     device->info.driver_version.major = 0;
     device->info.driver_version.minor = 1;

     device->id = DIDID_MOUSE;

     device->desc.type = DIDTF_MOUSE;
     device->desc.caps = DICAPS_AXIS | DICAPS_BUTTONS;
     device->desc.max_axis = DIAI_Y;
     device->desc.max_button = DIBI_LEFT;

     device->EventThread = h3600_tsEventThread;

     return DFB_OK;
}

void driver_deinit(InputDevice *device)
{
     if (close( fd ) < 0)
          PERRORMSG( "DirectFB/H3600: Error closing `/dev/ts'!\n" );
     fd = -1;
}

