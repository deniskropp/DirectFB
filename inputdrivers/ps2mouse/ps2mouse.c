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
#include <sys/time.h>

#include <termios.h>

#include <pthread.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>

#include <misc/conf.h>
#include <misc/mem.h>

/* Stolen from the linux kernel (pc_keyb.h) */
#define PS2_SET_RES             0xE8    /* Set resolution */
#define PS2_SET_SCALE11         0xE6    /* Set 1:1 scaling */
#define PS2_SET_SCALE21         0xE7    /* Set 2:1 scaling */
#define PS2_GET_SCALE           0xE9    /* Get scaling factor */
#define PS2_SET_STREAM          0xEA    /* Set stream mode */
#define PS2_SET_SAMPLE          0xF3    /* Set sample rate */
#define PS2_ENABLE_DEV          0xF4    /* Enable aux device */
#define PS2_DISABLE_DEV         0xF5    /* Disable aux device */
#define PS2_RESET               0xFF    /* Reset aux device */
#define PS2_ACK                 0xFA    /* Command byte ACK. */

/*** mouse commands ***/

#define PS2_SEND_ID    0xF2
#define PS2_ID_ERROR   -1
#define PS2_ID_PS2     0
#define PS2_ID_IMPS2   3


typedef struct {
     int            fd;
     InputDevice   *device;
     pthread_t      thread;

     int            mouseId;
     int            packetLength;

     DFBInputEvent  x_motion;
     DFBInputEvent  y_motion;
     DFBInputEvent  z_motion;
} PS2MouseData;


static inline void
ps2mouse_motion_initialize( PS2MouseData *data )
{
     data->x_motion.type    =
     data->y_motion.type    =
     data->z_motion.type    = DIET_AXISMOTION;

     data->x_motion.flags   =
     data->y_motion.flags   =
     data->z_motion.flags   = DIEF_AXISREL | DIEF_TIMESTAMP;

     data->x_motion.axisrel =
     data->y_motion.axisrel =
     data->z_motion.axisrel = 0;

     data->x_motion.axis    = DIAI_X;
     data->y_motion.axis    = DIAI_Y;
     data->z_motion.axis    = DIAI_Z;
}

static inline void
ps2mouse_motion_compress( PS2MouseData *data, int dx, int dy, int dz )
{
     data->x_motion.axisrel += dx;
     data->y_motion.axisrel += dy;
     data->z_motion.axisrel += dz;
}

static inline void
ps2mouse_motion_realize( PS2MouseData *data )
{
     if (data->x_motion.axisrel) {
          gettimeofday( &data->x_motion.timestamp, NULL );
          input_dispatch( data->device, &data->x_motion );
          data->x_motion.axisrel = 0;
     }

     if (data->y_motion.axisrel) {
          gettimeofday( &data->y_motion.timestamp, NULL );
          input_dispatch( data->device, &data->y_motion );
          data->y_motion.axisrel = 0;
     }

     if (data->z_motion.axisrel) {
          gettimeofday( &data->z_motion.timestamp, NULL );
          input_dispatch( data->device, &data->z_motion );
          data->z_motion.axisrel = 0;
     }
}

static void*
ps2mouseEventThread( void *driver_data )
{
     PS2MouseData *data  = (PS2MouseData*) driver_data;

    unsigned char packet[4];
    unsigned char pos = 0;
    unsigned char last_buttons = 0;

    int readlen;
    unsigned char buf[256];

    ps2mouse_motion_initialize( data );

    while ( (readlen = read(data->fd, buf, 256)) > 0 ) {
        int i;

        pthread_testcancel();

        for ( i = 0; i < readlen; i++ ) {

            if ( pos == 0  &&  (buf[i] & 0xc0) ) {
                continue;
            }

            packet[pos++] = buf[i];
            if ( pos == data->packetLength ) {
                int dx, dy, dz;
                int buttons;

                pos = 0;

                if ( !(packet[0] & 0x08) ) {    
                    /* We've lost sync! */
                    i--;    /* does this make sense? oh well, it will resync eventually (or will it!?)*/
                    continue;
                }

                buttons = packet[0] & 0x07;
                dx = (packet[0] & 0x10) ?   packet[1]-256  :  packet[1];
                dy = (packet[0] & 0x20) ? -(packet[2]-256) : -packet[2];
				if (data->mouseId == PS2_ID_IMPS2){
					dz = (packet[4] & 0x80) ? packet[4] | 0xf0 : packet[4] & 0x0f;  /* Just strip off the extra buttons if present and sign extend the 4 bit value */
				}
				else {
					dz = 0;
				}

                ps2mouse_motion_compress( data, dx, dy, dz );

                if ( !dfb_config->mouse_motion_compression )
                    ps2mouse_motion_realize( data );

                if ( last_buttons != buttons ) {
                    DFBInputEvent evt;
                    unsigned char changed_buttons = last_buttons ^ buttons;

                    /* make sure the compressed motion event is dispatched
                       before any button change */
                    ps2mouse_motion_realize( data );

                    evt.flags = DIEF_BUTTON | DIEF_TIMESTAMP;

                    gettimeofday( &evt.timestamp, NULL );

                    if ( changed_buttons & 0x01 ) {
                        evt.type = (buttons & 0x01) ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                        evt.button = DIBI_LEFT;
                        input_dispatch( data->device, &evt );
                    }
                    if ( changed_buttons & 0x02 ) {
                        evt.type = (buttons & 0x02) ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                        evt.button = DIBI_RIGHT;
                        input_dispatch( data->device, &evt );
                    }
                    if ( changed_buttons & 0x04 ) {
                        evt.type = (buttons & 0x04) ? DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                        evt.button = DIBI_MIDDLE;
                        input_dispatch( data->device, &evt );
                    }

                    last_buttons = buttons;
                }
            }
        }
        /* make sure the compressed motion event is dispatched,
           necessary if the last packet was a motion event */
        ps2mouse_motion_realize( data );
    }

    if ( readlen <= 0 && errno != EINTR )
        PERRORMSG ("psmouse thread died\n");

    pthread_testcancel();

    return (NULL);
}

/* 
    These 2 functions were HEAVILY inpsired (*cough*)
    from the gpm package (written by the great 
    Alessandro Rubini and others) 
*/
static int ps2GetId(int fd) {
    unsigned char c = PS2_SEND_ID;

    write(fd, &c, 1);
    read(fd, &c, 1);
    if ( c != PS2_ACK ) {
        return(-1);
    }
    read(fd, &c, 1);

    return(c);
}

static int ps2Write( int fd, unsigned char *data, size_t len) {
    int i;
    int error = 0;

    for ( i = 0; i < len; i++ ) {
        unsigned char c;
        write(fd, &data[i], 1);
        read(fd, &c, 1);
        if ( c != PS2_ACK ) {
            error++;
        }
    }
  usleep (30000);
  tcflush (fd, TCIFLUSH);

    return(error);
}


/* exported symbols */

int
driver_get_abi_version()
{
     return DFB_INPUT_DRIVER_ABI_VERSION;
}

int
driver_get_available()
{
     int fd;

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


void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "PS/2 Mouse Driver" );

     snprintf( info->vendor,
               DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 9;
}

DFBResult
driver_open_device( InputDevice      *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     static unsigned char basic_init[] = { PS2_ENABLE_DEV, PS2_SET_SAMPLE, 100};
     static unsigned char imps2_init[] = { PS2_SET_SAMPLE, 200, PS2_SET_SAMPLE, 100, PS2_SET_SAMPLE, 80,};
     static unsigned char ps2_init[] = { PS2_SET_SCALE11, PS2_ENABLE_DEV, PS2_SET_SAMPLE, 100, PS2_SET_RES, 3,};
     
     int           fd, mouseId;
     int           packetLength = 3;
     PS2MouseData *data;

     /* open device */
     fd = open( "/dev/psaux", O_RDWR );
     if (fd < 0) {
          fd = open( "/dev/input/mice", O_RDWR );
          if (fd < 0) {
               PERRORMSG( "DirectFB/PS2Mouse: Error opening `/dev/psaux' or `/dev/input/mice' !\n" );
               return DFB_INIT;
          }
     }

     /* Probe for Intellimouse - this should eventually be rolled into the main PS2 driver */

     /* Do a basic init in case the mouse is confused */
     ps2Write(fd, basic_init, sizeof (basic_init));

     /* Now try again and make sure we have a PS/2 mouse */
     if ( ps2Write(fd, basic_init, sizeof (basic_init)) != 0 ) {
         fprintf(stderr, "imps2: PS/2 mouse failed second init");
         return (DFB_INIT);
     }

     /* Try to switch to 3 button mode */
     if ( ps2Write(fd, imps2_init, sizeof (imps2_init)) != 0 ) {
         fprintf(stderr, "imps2: PS/2 mouse failed 3 button mode switch");
         return (DFB_INIT);
     }

     mouseId = ps2GetId(fd);
     if ( mouseId == (-1) ) {
         fprintf(stderr, "imps2: PS/2 mouse failed to read id, assuming standard PS/2");
         mouseId = PS2_ID_PS2;
     }

     /* And do the real initialisation */
     if ( ps2Write(fd, ps2_init, sizeof (ps2_init)) != 0 ) {
         fprintf(stderr, "imps2: PS/2 mouse failed setup, continuing...");
     }

     if ( mouseId == PS2_ID_IMPS2 ){
         packetLength = 4;
     }
     
     
     /* fill device info structure */
     snprintf( info->name,
               DFB_INPUT_DEVICE_INFO_NAME_LENGTH,
               (mouseId == PS2_ID_IMPS2) ? "IMPS/2 Mouse" : "PS/2 Mouse" );

     snprintf( info->vendor,
               DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH, "Unknown" );
     
     info->prefered_id     = DIDID_MOUSE;
     
     info->desc.type       = DIDTF_MOUSE;
     info->desc.caps       = DICAPS_AXIS | DICAPS_BUTTONS;
     info->desc.max_axis   = (mouseId == PS2_ID_IMPS2) ? DIAI_Z : DIAI_Y;
     info->desc.max_button = DIBI_MIDDLE;     /* TODO: probe!? */

     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(PS2MouseData) );
     
     data->fd           = fd;
     data->device       = device;
     data->mouseId      = mouseId;
     data->packetLength = packetLength;
     
     /* start input thread */
     pthread_create( &data->thread, NULL, ps2mouseEventThread, data );

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}


void
driver_close_device( void *driver_data )
{
     PS2MouseData *data = (PS2MouseData*) driver_data;

     /* stop input thread */
     pthread_cancel( data->thread );
     pthread_join( data->thread, NULL );
     
     /* close device */
     close( data->fd );

     /* free private data */
     DFBFREE( data );
}

