/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Shane Volpe <shanevolpe@gmail.com

   Based on usb1x00_ts writen by:
              Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrj��<syrjala@sci.fi>.

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

#include <config.h>

#include <stdlib.h>
#include <stdio.h>

#include <tslib.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>

#include <core/input_driver.h>

#include <misc/conf.h>

DFB_INPUT_DRIVER( tslib )

typedef struct {
     CoreInputDevice *device;
     DirectThread    *thread;
     struct tsdev    *ts;
} tslibData;

#define MAX_TSLIB_DEVICES 16

static int num_devices = 0;
static char *device_names[MAX_TSLIB_DEVICES];

static void *
tslibEventThread( DirectThread *thread, void *driver_data )
{
     tslibData *data = (tslibData *) driver_data;
     struct ts_sample ts_event;
     int readlen;
     int old_x = -1;
     int old_y = -1;
     unsigned int old_pressure = 0;

     while ((readlen = ts_read( data->ts, &ts_event, 1 )) >= 0) {
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

          if (!ts_event.pressure != !old_pressure) {
               evt.type   = ts_event.pressure ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
               evt.flags  = DIEF_NONE;
               evt.button = DIBI_LEFT;

               dfb_input_dispatch( data->device, &evt );

               old_pressure = ts_event.pressure;
          }
     }

     if (readlen < 0)
          D_ERROR( "tslib Input thread died\n" );

     return NULL;
}

static bool
check_device( const char *device )
{
     struct tsdev *ts;

     ts = ts_open( device, 0 );
     if (!ts)
          return false;

     if (ts_config( ts )) {
          ts_close( ts );
          return false;
     }

     ts_close( ts );

     return true;
}

/* exported symbols */

static int
driver_get_available(void)
{
     int   i;
     char *tsdev;

     /* Use the devices specified in the configuration. */
     if (fusion_vector_has_elements( &dfb_config->tslib_devices )) {
          const char *device;

          fusion_vector_foreach (device, i, dfb_config->tslib_devices) {
               if (check_device( device ))
                    device_names[num_devices++] = D_STRDUP( device );
          }

          return num_devices;
     }

     /* Check for environment variable. */
     tsdev = getenv( "TSLIB_TSDEVICE" );
     if (tsdev && check_device( tsdev ))
          device_names[num_devices++] = D_STRDUP( tsdev );

     /* Try to guess some (more) devices. */
     for (i = 0; i < MAX_TSLIB_DEVICES; i++) {
          char buf[32];

          snprintf( buf, 32, "/dev/input/tslib%d", i );

          /* Already handled above. */
          if (tsdev && !strcmp( tsdev, buf ))
               continue;

          if (check_device( buf ))
               device_names[num_devices++] = D_STRDUP( buf );
     }

     return num_devices;
}

static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */

     snprintf( info->name,
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH,
               "tslib Input Driver" );

     snprintf( info->vendor,
               DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
               "tslib" );

     info->version.major = 0;
     info->version.minor = 1;
}

static DFBResult
driver_open_device( CoreInputDevice  *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     tslibData *data;
     struct tsdev *ts;

     /* open device */
     ts = ts_open( device_names[number], 0 );
     if (!ts) {
          D_ERROR( "DirectFB/tslib: Error opening `%s'!\n", device_names[number] );
          return DFB_INIT;
     }

     /* configure device */
     if (ts_config( ts )) {
          D_ERROR( "DirectFB/tslib: Error configuring `%s'!\n", device_names[number] );
          ts_close( ts );
          return DFB_INIT;
     }

     /* fill device info structure */
     snprintf( info->desc.name,
               DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "tslib touchscreen %d", number );

     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "tslib" );

     info->prefered_id     = DIDID_MOUSE;

     info->desc.type       = DIDTF_MOUSE;
     info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
     info->desc.max_axis   = DIAI_Y;
     info->desc.max_button = DIBI_LEFT;

     /* allocate and fill private data */
     data = D_CALLOC( 1, sizeof(tslibData) );

     data->ts     = ts;
     data->device = device;

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, tslibEventThread, data, "tslib Input" );

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}

/*
 * Fetch one entry from the device's keymap if supported.
 */
static DFBResult
driver_get_keymap_entry( CoreInputDevice           *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
     return DFB_UNSUPPORTED;
}

static void
driver_close_device( void *driver_data )
{
     tslibData *data = (tslibData*) driver_data;

     /* stop input thread */
     direct_thread_cancel( data->thread );
     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     /* close device */
     ts_close( data->ts );

     /* free private data */
     D_FREE( data );
}
