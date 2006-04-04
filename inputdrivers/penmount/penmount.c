/*
   (c) Copyright 2000-2002  Fulgid Technology Co., Ltd.

   All rights reserved.

   Written by Simon Ueng <simon@ftech.com.tw>
   Modified by Nikita Egorov <nikego@gmail.com>

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

   This driver is a re-write from the MuTouch driver.
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
#include <core/system.h>

#include <misc/conf.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/memcpy.h>
#include <direct/thread.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( PenMount )

#define BAUDRATE		B19200

#define PeM_REPORT_SIZE		5
#define PeM_PACKET_SIZE		10
#define PeM_SCREENWIDTH		640
#define PeM_SCREENHEIGHT	480
#define PeM_MINX		0
#define PeM_MINY		0

#define PEM_PANEL_TOUCH		0x01
#define PEM_PANEL_UNTOUCH	0x00


/* Event mask */
#define PeM_PANEL_TOUCH_MASK	0x40
#define PeM_PANEL_SYNC_MASK	0x80

typedef struct __PeMData__ {
     int fd;
     DirectThread *thread;
     CoreInputDevice *device;
     unsigned short x;
     unsigned short y;
     unsigned short screen_width;
     unsigned short screen_height;
     unsigned short min_x;
     unsigned short min_y;
     unsigned char action;
} PeMData;

/* Global Variables */
static unsigned char packet[PeM_PACKET_SIZE];

	/* touchscreen values of left/top position  */
static int min_x=19,min_y=1001;

	/* touchscreen values of right/bottom position  */
static int max_x=946,max_y=62;


static inline void __mdelay(unsigned int msec){
     struct timespec delay;

     delay.tv_sec = 0;
     delay.tv_nsec = msec * 1000000;
     nanosleep (&delay, NULL);
}

static inline void PeMSendPacket(int file, unsigned char *packet, unsigned char len){
	
     write (file, packet, len);
}

static inline void PeMReadPacket(int file, unsigned char *packet){
     int n = 0;
     memset (packet, 0, PeM_PACKET_SIZE);
     while ((n += read (file, packet+n, PeM_REPORT_SIZE-n)) != PeM_REPORT_SIZE);
}

static int PeMPollDevice(unsigned char *device){
     int file;
     struct termios options;
	
     /* Make raw I/O */
     memset (&options, 0, sizeof (struct termios));
     
	/* Open I/O port */
	 file = open (device, O_RDWR | O_NOCTTY);
	 
	 options.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
     options.c_cc[VMIN] = 1;
     options.c_cc[VTIME] = 0;
     
     tcflush (file, TCIFLUSH);
     tcsetattr (file, TCSANOW, &options);

     return file;
}

static inline int PeMInitialize(int file){
	/* it's a stub */
	/* I dont know how get information about PenMount device... */
	
     return 1;
}

static int PeMOpenDevice(unsigned char *device){
     int fd;
     int res;
     
	 unsigned char *pos = strstr(device, ":raw");
	 if (pos) {// raw data 
	 	  max_x = min_x;
	 	  max_y = min_y;
	 	  *pos = 0;
	 }
     fd = PeMPollDevice (device);
     if ((res = PeMInitialize (fd)) == 0) {
          close (fd);
          return res;
     }
     return fd;
}

static int convert_x(int x,PeMData *event){
	if (max_x != min_x)
		return .5+event->screen_width*((double)x - min_x)/(max_x - min_x);
	else	
		return x;
}

static int convert_y(int y,PeMData *event){
	if (max_y != min_y)
		return .5+event->screen_height*((double)y - min_y)/(max_y - min_y);
	else
		return y;
}

#define WORD_ASSEMBLY(byte1, byte2)	(((byte2) << 7) | (byte1))

static int PeMGetEvent(PeMData *event){
	
     PeMReadPacket(event->fd, packet);
     /* Sync bit always must be set to 1 */
     if ((*packet & PeM_PANEL_SYNC_MASK) == 0)
          return 0;
     
     if (*packet & PeM_PANEL_TOUCH_MASK)
          event->action = PEM_PANEL_TOUCH;
     else
          event->action = PEM_PANEL_UNTOUCH;
          
	 event->y = convert_y( WORD_ASSEMBLY(packet[2], packet[1]),event);
	 event->x = convert_x( WORD_ASSEMBLY(packet[4], packet[3]),event);

     if (event->min_x)
          event->x = event->min_x - event->x;
     if (event->min_y)
          event->y = event->min_y - event->y;

     return 1;
}

/* The main routine for PenMount */
static void *PenMountEventThread(DirectThread *thread, void *driver_data){
	
     PeMData *data = (PeMData *) driver_data;

     /* Read data */
     while (1) {
          DFBInputEvent evt;

          if (!PeMGetEvent (data))
	       continue;
	      direct_thread_testcancel (thread);

          /* Dispatch axis */
          evt.type    = DIET_AXISMOTION;
          evt.flags   = DIEF_AXISABS;
          evt.axis    = DIAI_X;
          evt.axisabs = data->x;
          dfb_input_dispatch (data->device, &evt);

          evt.type    = DIET_AXISMOTION;
          evt.flags   = DIEF_AXISABS;
          evt.axis    = DIAI_Y;
          evt.axisabs = data->y;
          dfb_input_dispatch (data->device, &evt);

          /* Dispatch touch event */
          static int pressed = 0;
          switch (data->action) {
               case PEM_PANEL_TOUCH:
               		if (!pressed)
                    	evt.type = DIET_BUTTONPRESS;
                    pressed = 1;
               		break;
               case PEM_PANEL_UNTOUCH:
               		if (pressed)
                    	evt.type = DIET_BUTTONRELEASE;
                    pressed = 0;	
                    break;
          }

          evt.flags  = DIEF_NONE;
          evt.button = DIBI_LEFT;

          dfb_input_dispatch (data->device, &evt);
          direct_thread_testcancel (thread);
     }

     return NULL;
}

/* exported symbols */

static int driver_get_available( void ){
     int fd;

     if (!dfb_config->penmount_device)
          return 0;

     fd = PeMOpenDevice (dfb_config->penmount_device);
     if (fd < 0)
          return 0;

     close(fd);

     return 1;
}

static void driver_get_info( InputDriverInfo *info ){
     /* fill driver info structure */
     snprintf(info->name, DFB_INPUT_DRIVER_INFO_NAME_LENGTH,
              "PenMount 9509" );
     snprintf(info->vendor, DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
              "AMT" );

     info->version.major = 0;
     info->version.minor = 1;
}

static DFBResult driver_open_device(CoreInputDevice *device,
                                    unsigned int number,
                                    InputDeviceInfo *info,
                                    void **driver_data){
     int fd;
     PeMData *data;
    
     /* open device */
     fd = PeMOpenDevice (dfb_config->penmount_device);
     if (fd < 0) {
          D_PERROR("DirectFB/PenMount: Error opening '%s'!\n", dfb_config->penmount_device);
          return DFB_INIT;
     }

     data = D_CALLOC (1, sizeof(PeMData));

     data->fd     = fd;
     data->device = device;

     /* Must define the correct resolution of screen */
     data->screen_width  = PeM_SCREENWIDTH;
     data->screen_height = PeM_SCREENHEIGHT;

     /* The following variable are defined to adjust the orientation of
      * the touchscreen. Variables are either max screen height/width or 0.
      */
     data->min_x = PeM_MINX;
     data->min_y = PeM_MINY;

     /* fill device info structure */
     snprintf(info->desc.name, DFB_INPUT_DEVICE_DESC_NAME_LENGTH,
              "PenMount 9509");
     snprintf(info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH,
              "AMT");

     info->prefered_id		= DIDID_MOUSE;
     info->desc.type		= DIDTF_MOUSE;
     info->desc.caps		= DICAPS_AXES | DICAPS_BUTTONS;
     info->desc.max_axis	= DIAI_Y;
     info->desc.max_button	= DIBI_LEFT;

     /* start input thread */
     data->thread = direct_thread_create (DTT_INPUT, PenMountEventThread, data, "PenMount Input");

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}

/*
 * Fetch one entry from the device's keymap if supported.
 */
static DFBResult driver_get_keymap_entry(CoreInputDevice *device,
                                         void        *driver_data,
                                         DFBInputDeviceKeymapEntry *entry)
{
     return DFB_UNSUPPORTED;
}

static void driver_close_device(void *driver_data){
     PeMData *data = (PeMData *)driver_data;

     /* stop input thread */
     direct_thread_cancel (data->thread);
     direct_thread_join (data->thread);
     direct_thread_destroy (data->thread);

     /* close device */
     close (data->fd);

     /* free private data */
     D_FREE (data);
}
