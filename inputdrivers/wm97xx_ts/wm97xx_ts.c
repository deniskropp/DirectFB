/*

   Driver for Wolfson WM9705/WM9712 touchscreen controllers as
   found in the Dell Axim and Toshiba e740.

   liam.girdwood@wolfsonmicro.com

   NOTE: Please make sure that pressure measurement is enable in the
         kernel driver. (i.e. pil != 0) Please read
		  wolfson-touchscreen.txt in your kernel documentation for
		  details.

   Based on the h3600_ts.c driver by convergence GmbH.

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
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/wm97xx.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>

#include <misc/conf.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( wm97xx_ts )

typedef struct {
     CoreInputDevice   *device;
     DirectThread  *thread;

     int            fd;
} WM97xxTSData;

/* I copied this from the kernel driver since there is no include file */
typedef struct {
     __u16             pressure;
     __u16             x;
     __u16             y;
     __u16             pad;
     struct timeval  stamp;
} TS_EVENT;

static void *
wm97xxtsEventThread( DirectThread *thread, void *driver_data )
{
     WM97xxTSData *data = (WM97xxTSData*) driver_data;

     TS_EVENT ts_event;

     int readlen;

     unsigned short old_x = -1;
     unsigned short old_y = -1;
     unsigned short old_pressure = 0;

     while ((readlen = read(data->fd, &ts_event, sizeof(TS_EVENT))) > 0  ||
            errno == EINTR)
     {
          DFBInputEvent evt;

          direct_thread_testcancel( thread );

          if (readlen < 1)
               continue;

          if (ts_event.pressure) {
               if (ts_event.x != old_x) {
                    evt.type    = DIET_AXISMOTION;
                    evt.flags   = DIEF_AXISABS;
                    evt.axis    = DIAI_X;
                    evt.axisabs = ts_event.x;

                    dfb_input_dispatch( data->device, &evt );

                    old_x = ts_event.x;
               }

               if (ts_event.y != old_y) {
                    evt.type    = DIET_AXISMOTION;
                    evt.flags   = DIEF_AXISABS;
                    evt.axis    = DIAI_Y;
                    evt.axisabs = ts_event.y;

                    dfb_input_dispatch( data->device, &evt );

                    old_y = ts_event.y;
               }
          }

          if ((ts_event.pressure && !old_pressure) ||
              (!ts_event.pressure && old_pressure)) {
               evt.type   = (ts_event.pressure ?
                             DIET_BUTTONPRESS : DIET_BUTTONRELEASE);
               evt.flags  = DIEF_NONE;
               evt.button = DIBI_LEFT;

               dfb_input_dispatch( data->device, &evt );

               old_pressure = ts_event.pressure;
          }
     }

     if (readlen <= 0)
          D_PERROR ("WM97xx Touchscreen thread died\n");

     return NULL;
}


/* exported symbols */

static int driver_get_available()
{
     int fd;

     fd = open( "/dev/touchscreen/wm97xx", O_RDONLY | O_NOCTTY );
     if (fd < 0)
          return 0;

     close( fd );

     return 1;
}

static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "WM97xx Touchscreen Driver" );

     snprintf( info->vendor,
               DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 1;
}

static DFBResult
driver_open_device( CoreInputDevice      *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     int          fd;
     WM97xxTSData *data;

     /* open device */
     fd = open( "/dev/touchscreen/wm97xx", O_RDONLY | O_NOCTTY );
     if (fd < 0) {
          D_PERROR( "DirectFB/WM97xx: Error opening `/dev/touchscreen/wm97xx'!\n" );
          return DFB_INIT;
     }

     /* fill device info structure */
     snprintf( info->desc.name,
               DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "WM97xx Touchscreen" );

     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "Wolfson Microelectronics" );

     info->prefered_id     = DIDID_MOUSE;

     info->desc.type       = DIDTF_MOUSE;
     info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
     info->desc.max_axis   = DIAI_Y;
     info->desc.max_button = DIBI_LEFT;

     /* allocate and fill private data */
     data = D_CALLOC( 1, sizeof(WM97xxTSData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, wm97xxtsEventThread, data, "WM97xx TS Input" );

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}

/*
 * Fetch one entry from the device's keymap if supported.
 */
static DFBResult
driver_get_keymap_entry( CoreInputDevice               *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
     return DFB_UNSUPPORTED;
}

static void
driver_close_device( void *driver_data )
{
     WM97xxTSData *data = (WM97xxTSData*) driver_data;

     /* stop input thread */
     direct_thread_cancel( data->thread );
     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     /* close device */
     if (close( data->fd ) < 0)
          D_PERROR( "DirectFB/WM97xx: Error closing `/dev/touchscreen/wm97xx'!\n" );

     /* free private data */
     D_FREE( data );
}
