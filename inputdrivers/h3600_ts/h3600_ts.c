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
#include <errno.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/h3600_ts.h>

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


DFB_INPUT_DRIVER( h3600_ts )

typedef struct {
     CoreInputDevice  *device;
     DirectThread *thread;

     int           fd;
} H3600TSData;

static void *
h3600tsEventThread( DirectThread *thread, void *driver_data )
{
     H3600TSData *data = (H3600TSData*) driver_data;

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
          D_PERROR ("H3600 Touchscreen thread died\n");

     return NULL;
}


/* exported symbols */

static int driver_get_available()
{
     int fd;

     fd = open( dfb_config->h3600_device ? : "/dev/ts", O_RDONLY | O_NOCTTY );
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
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "H3600 Touchscreen Driver" );

     snprintf( info->vendor,
               DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 2;
}

static DFBResult
driver_open_device( CoreInputDevice      *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     int          fd;
     H3600TSData *data;

     /* open device */
     fd = open( dfb_config->h3600_device ? : "/dev/ts", O_RDONLY | O_NOCTTY );
     if (fd < 0) {
          D_PERROR( "DirectFB/H3600: Error opening `/dev/ts'!\n" );
          return DFB_INIT;
     }

     /* fill device info structure */
     snprintf( info->desc.name,
               DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "H3600 Touchscreen" );

     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "Unknown" );

     info->prefered_id     = DIDID_MOUSE;

     info->desc.type       = DIDTF_MOUSE;
     info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
     info->desc.max_axis   = DIAI_Y;
     info->desc.max_button = DIBI_LEFT;

     /* allocate and fill private data */
     data = D_CALLOC( 1, sizeof(H3600TSData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, h3600tsEventThread, data, "H3600 TS Input" );

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
     H3600TSData *data = (H3600TSData*) driver_data;

     /* stop input thread */
     direct_thread_cancel( data->thread );
     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     /* close device */
     if (close( data->fd ) < 0)
          D_PERROR( "DirectFB/H3600: Error closing `/dev/ts'!\n" );

     /* free private data */
     D_FREE( data );
}

