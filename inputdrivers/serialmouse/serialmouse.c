/*
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

#include <termios.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <linux/serial.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/thread.h>

#include <misc/conf.h>
#include <misc/mem.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( serialmouse )

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

static const char *protocol_names[LAST_PROTOCOL] =
{
     "MS",
     "MS3",
     "MouseMan",
     "MouseSystems"
};

typedef struct {
     InputDevice   *device;
     CoreThread    *thread;

     int            fd;
     
     MouseProtocol  protocol;

     DFBInputEvent  x_motion;
     DFBInputEvent  y_motion;
} SerialMouseData;


static inline void
mouse_motion_initialize( SerialMouseData *data )
{
     data->x_motion.type    = data->y_motion.type    = DIET_AXISMOTION;
     data->x_motion.axisrel = data->y_motion.axisrel = 0;

     data->x_motion.axis    = DIAI_X;
     data->y_motion.axis    = DIAI_Y;
}

static inline void
mouse_motion_compress( SerialMouseData *data, int dx, int dy )
{
     data->x_motion.axisrel += dx;
     data->y_motion.axisrel += dy;
}

static inline void
mouse_motion_realize( SerialMouseData *data )
{
     if (data->x_motion.axisrel) {
          data->x_motion.flags = DIEF_AXISREL;
          dfb_input_dispatch( data->device, &data->x_motion );
          data->x_motion.axisrel = 0;
     }

     if (data->y_motion.axisrel) {
          data->y_motion.flags = DIEF_AXISREL;
          dfb_input_dispatch( data->device, &data->y_motion );
          data->y_motion.axisrel = 0;
     }
}


static void
mouse_setspeed( SerialMouseData *data )
{
     struct termios tty;

     tcgetattr (data->fd, &tty);

     tty.c_iflag     = IGNBRK | IGNPAR;
     tty.c_oflag     = 0;
     tty.c_lflag     = 0;
     tty.c_line      = 0;
     tty.c_cc[VTIME] = 0;
     tty.c_cc[VMIN]  = 1;
     tty.c_cflag     = CREAD|CLOCAL|HUPCL|B1200;
     tty.c_cflag    |= ((data->protocol == PROTOCOL_MOUSESYSTEMS) ?
                        CS8|CSTOPB : CS7);

     tcsetattr (data->fd, TCSAFLUSH, &tty);

     write (data->fd, "*n", 2);
}

/* the main routine for MS mice (plus extensions) */
static void*
mouseEventThread_ms( CoreThread *thread, void *driver_data )
{
     SerialMouseData *data = (SerialMouseData*) driver_data;
     DFBInputEvent    evt;

     unsigned char buf[256];
     unsigned char packet[4];
     unsigned char pos = 0;
     unsigned char last_buttons = 0;
     int dx, dy;
     int buttons;
     int readlen;
     int i;

     mouse_motion_initialize( data );

     /* Read data */
     while ((readlen = read( data->fd, buf, 256 )) >= 0 || errno == EINTR) {

          dfb_thread_testcancel( thread );

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
                    if (data->protocol != PROTOCOL_MOUSEMAN)
                         pos = 0;

                    buttons = packet[0] & 0x30;
                    dx = (signed char)
                         (((packet[0] & 0x03) << 6) | (packet[1] & 0x3f));
                    dy = (signed char)
                         (((packet[0] & 0x0C) << 4) | (packet[2] & 0x3f));

                    mouse_motion_compress( data, dx, dy );

                    if (data->protocol == PROTOCOL_MS3) {
                         if (!dx && !dy && buttons == (last_buttons & ~MIDDLE))
                              buttons = last_buttons ^ MIDDLE;  /* toggle    */
                         else
                              buttons |= last_buttons & MIDDLE; /* preserve  */
                    }

                    if (!dfb_config->mouse_motion_compression)
                         mouse_motion_realize( data );

                    if (last_buttons != buttons) {
                         unsigned char changed_buttons = last_buttons ^ buttons;

                         /* make sure the compressed motion event is dispatched
                            before any button change */
                         mouse_motion_realize( data );

                         if (changed_buttons & 0x20) {
                              evt.type = (buttons & 0x20) ?
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_NONE;
                              evt.button = DIBI_LEFT;
                              dfb_input_dispatch( data->device, &evt );
                         }
                         if (changed_buttons & 0x10) {
                              evt.type = (buttons & 0x10) ?
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_NONE;
                              evt.button = DIBI_RIGHT;
                              dfb_input_dispatch( data->device, &evt );
                         }
                         if (changed_buttons & MIDDLE) {
                              evt.type = (buttons & MIDDLE) ?
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_NONE;
                              evt.button = DIBI_MIDDLE;
                              dfb_input_dispatch( data->device, &evt );
                         }

                         last_buttons = buttons;
                    }
                    break;

               case 4:
                    pos = 0;

                    evt.type = (packet[3] & 0x20) ?
                         DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                    evt.flags = DIEF_NONE; /* button is always valid */
                    evt.button = DIBI_MIDDLE;
                    dfb_input_dispatch( data->device, &evt );
                    break;

               default:
                    break;
               }
          }

          /* make sure the compressed motion event is dispatched,
             necessary if the last packet was a motion event */
          if (readlen > 0)
               mouse_motion_realize( data );

          dfb_thread_testcancel( thread );
     }

     PERRORMSG ("serial mouse thread died\n");

     return NULL;
}

/* the main routine for MouseSystems */
static void*
mouseEventThread_mousesystems( CoreThread *thread, void *driver_data )
{
     SerialMouseData *data = (SerialMouseData*) driver_data;

     unsigned char buf[256];
     unsigned char packet[5];
     unsigned char pos = 0;
     unsigned char last_buttons = 0;
     int i;
     int readlen;

     mouse_motion_initialize( data );

     /* Read data */
     while ((readlen = read( data->fd, buf, 256 )) >= 0 || errno == EINTR) {

          dfb_thread_testcancel( thread );

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

                    mouse_motion_compress( data, dx, dy );

                    if (!dfb_config->mouse_motion_compression)
                         mouse_motion_realize( data );

                    if (last_buttons != buttons) {
                         DFBInputEvent evt;
                         unsigned char changed_buttons = last_buttons ^ buttons;

                         /* make sure the compressed motion event is dispatched
                            before any button change */
                         mouse_motion_realize( data );

                         if (changed_buttons & 0x04) {
                              evt.type = (buttons & 0x04) ?
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_NONE; /* button is always valid */
                              evt.button = DIBI_LEFT;
                              dfb_input_dispatch( data->device, &evt );
                         }
                         if (changed_buttons & 0x01) {
                              evt.type = (buttons & 0x01) ?
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_NONE; /* button is always valid */
                              evt.button = DIBI_MIDDLE;
                              dfb_input_dispatch( data->device, &evt );
                         }
                         if (changed_buttons & 0x02) {
                              evt.type = (buttons & 0x02) ?
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_NONE; /* button is always valid */
                              evt.button = DIBI_RIGHT;
                              dfb_input_dispatch( data->device, &evt );
                         }

                         last_buttons = buttons;
                    }
               }
          }

          /* make sure the compressed motion event is dispatched,
             necessary if the last packet was a motion event */
          if (readlen > 0)
               mouse_motion_realize( data );

          dfb_thread_testcancel( thread );
     }

     PERRORMSG ("serial mouse thread died\n");

     return NULL;
}

static MouseProtocol mouse_get_protocol()
{
     MouseProtocol protocol;

     if (!dfb_config->mouse_protocol)
          return LAST_PROTOCOL;

     for (protocol = 0; protocol < LAST_PROTOCOL; protocol++) {
          if (strcasecmp (dfb_config->mouse_protocol,
                          protocol_names[protocol]) == 0)
               break;
     }

     return protocol;
}

/* exported symbols */

static int
driver_get_abi_version()
{
     return DFB_INPUT_DRIVER_ABI_VERSION;
}

static int
driver_get_available()
{
     struct serial_struct serial_info;
     struct timeval       timeout;
     MouseProtocol        protocol;
     fd_set               set;
     int                  fd;
     char                 buf[8];
     int                  readlen;
     int                  lines;

     protocol = mouse_get_protocol();
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

static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "Serial Mouse Driver" );

     snprintf( info->vendor,
               DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 2;
}

static DFBResult
driver_open_device( InputDevice      *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     int              fd;
     MouseProtocol    protocol;
     SerialMouseData *data;

     protocol = mouse_get_protocol();
     if (protocol == LAST_PROTOCOL) /* shouldn't happen */
          return DFB_BUG;

     /* open device */
     fd = open( DEV_NAME, O_RDWR | O_NONBLOCK );
     if (fd < 0) {
          PERRORMSG( "DirectFB/SerialMouse: Error opening '"DEV_NAME"'!\n" );
          return DFB_INIT;
     }

     /* reset the O_NONBLOCK flag */
     fcntl (fd, F_SETFL, fcntl (fd, F_GETFL) & ~O_NONBLOCK);

     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(SerialMouseData) );

     data->fd       = fd;
     data->device   = device;
     data->protocol = protocol;

     mouse_setspeed( data );

     /* fill device info structure */
     snprintf( info->name, DFB_INPUT_DEVICE_INFO_NAME_LENGTH,
               "Serial Mouse (%s)", protocol_names[protocol] );

     snprintf( info->vendor, DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH, "Unknown" );

     info->prefered_id     = DIDID_MOUSE;

     info->desc.type       = DIDTF_MOUSE;
     info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
     info->desc.max_axis   = DIAI_Y;
     info->desc.max_button = (protocol > PROTOCOL_MS) ? DIBI_MIDDLE : DIBI_RIGHT;

     /* start input thread */
     data->thread = dfb_thread_create( CTT_INPUT,
                                       protocol == PROTOCOL_MOUSESYSTEMS ?
                                       mouseEventThread_mousesystems :
                                       mouseEventThread_ms, data );

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}

/*
 * Fetch one entry from the device's keymap if supported.
 */
static DFBResult
driver_get_keymap_entry( InputDevice               *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
     return DFB_UNSUPPORTED;
}

static void
driver_close_device( void *driver_data )
{
     SerialMouseData *data = (SerialMouseData*) driver_data;

     /* stop input thread */
     dfb_thread_cancel( data->thread );
     dfb_thread_join( data->thread );
     dfb_thread_destroy( data->thread );

     /* close device */
     close( data->fd );

     /* free private data */
     DFBFREE( data );
}

