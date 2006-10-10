/*
   Written by Pär Degerman <parde@ikp.liu.se>

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

	
	NOTES
	=====
	This driver is based heavily on code from two other DirectFB
	drivers, namely mutouch.c by Simon Ueng and elo.c by Byron 
	Stanoszek and Brandon Reynolds, so a lot of credit should go 
	to those people and not me.

	INSTRUCTIONS
	============
	You should change DYNAPRO_MIN_X and DYNAPRO_MIN_Y to match
	the orientation of your touchscreen.

*/

#include <config.h>

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

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

DFB_INPUT_DRIVER( dynapro )

#define DYNAPRO_DEVICE "/dev/ttyS0"
#define DYNAPRO_BAUD B9600
#define DYNAPRO_PACKET_SIZE 5

#define DYNAPRO_SCREENWIDTH 800
#define DYNAPRO_SCREENHEIGHT 600
#define DYNAPRO_MINX 800
#define DYNAPRO_MINY 600

#define DYNAPRO_CMD_TOUCH 0x81
#define DYNAPRO_CMD_UNTOUCH 0x80

typedef struct __dynaproData__ {
  int fd;
  DirectThread *thread;
  CoreInputDevice *device;

  unsigned short screen_width;
  unsigned short screen_height;
  unsigned short min_x;
  unsigned short min_y;

  unsigned short x;
  unsigned short y;
  unsigned char action;
} dynaproData;

/* Read a packet from touchcontroller */
static inline unsigned char *dynapro_getpck(int fd)
{
  static unsigned char packet[DYNAPRO_PACKET_SIZE];
  static unsigned int len = 0, start = 0;

  while(1) {
	 if (read(fd, &packet[len++], 1) < 1) {
		break;
	 }

	 if (0 == start) {
		if (packet[len-1] & 0x80) {
			/* Packet start found */
			start = 1;
			continue;
		} else {
			/* Continue searching for packet start */
			len = 0;
			start = 0;
			continue;
		}
	 } else if (len < DYNAPRO_PACKET_SIZE) {
		/* Continue until we have a full packet */
		start = 1;
		continue;
	 }

	 /* A full packet received */
	 len = 0;
	 start = 0;
	 return packet;
  }
}

/* Remove all input translations over tty serial controller.
 *
 * set=1:  Saves current settings then turns rawmode on.
 * set=0:  Restores controller to previous saved value.
 */
static void tty_rawmode(int fd, int set)
{
  static struct termios tbuf, termios_save;

  if(set) {
	 tcgetattr(fd, &termios_save);
    tbuf = termios_save;

    tbuf.c_iflag = 0; /* No input processing */
    tbuf.c_oflag = 0; /* No output processing */
    tbuf.c_lflag = 0; /* Disable erase/kill, signals, and echo */

	 /* Set baud & 1-char read mode */
    tbuf.c_cflag = DYNAPRO_BAUD | CS8 | CLOCAL | CREAD;

    tcsetattr(fd, TCSANOW, &tbuf);
  } else {
    tcsetattr(fd, TCSANOW, &termios_save);
  }
}

/* Open file descriptor to touch device */
static int dynaproOpenDevice(unsigned char *device)
{
  int fd;

  if((fd = open(device, O_RDWR|O_NOCTTY)) == -1) {
    D_PERROR("DirectFB/dynbapro: Error opening '%s'!\n",device);
    return -1;
  }

  if((flock(fd, LOCK_EX|LOCK_NB)) == -1) {
    D_PERROR("DirectFB/dynbapro: Error locking '%s'!\n",device);
    close(fd);
    return -1;
  }

  tty_rawmode(fd,1);

  return fd;
}


static int dynaproGetEvent(dynaproData *event)
{
  unsigned char *ptr;
  unsigned int cmd, x, y;

  /* read packet */
  if(!(ptr = dynapro_getpck(event->fd))) {
    return -1;
  }

  /* Get command (touch/untouch) and coordinates */
  cmd = ptr[0];
  x = (event->screen_width * ((ptr[3] << 8) + ptr[4])) / 0x0fff;
  y = (event->screen_height* ((ptr[1] << 8) + ptr[2])) / 0x0fff;

  if (event->min_x)
	 x = event->min_x - x;
  if (event->min_y)
	 y = event->min_y - y;

  event->action = cmd;
  event->x = x;
  event->y = y;

  return 0;
}


/* The main routine for dynapro */
static void *dynaproTouchEventThread(DirectThread *thread, void *driver_data)
{
  dynaproData *data = (dynaproData *) driver_data;

  /* Read data */
  while (1) {
    DFBInputEvent evt;

    if(dynaproGetEvent(data) == -1) {
		continue;
	 }
    direct_thread_testcancel(thread);

    /* Dispatch axis */
    evt.type    = DIET_AXISMOTION;
    evt.flags   = DIEF_AXISABS;
    evt.axis    = DIAI_X;
    evt.axisabs = data->x;
    dfb_input_dispatch(data->device, &evt);

    evt.type    = DIET_AXISMOTION;
    evt.flags   = DIEF_AXISABS;
    evt.axis    = DIAI_Y;
    evt.axisabs = data->y;
    dfb_input_dispatch(data->device, &evt);

    /* Dispatch touch event */
    if (DYNAPRO_CMD_UNTOUCH == data->action)
      evt.type = DIET_BUTTONRELEASE;
    else
      evt.type = DIET_BUTTONPRESS;

    evt.flags  = DIEF_NONE;
    evt.button = DIBI_LEFT;

    dfb_input_dispatch(data->device, &evt);
    direct_thread_testcancel(thread);
  }

  return NULL;
}


/* exported symbols */
static int driver_get_available( void )
{
  int fd;

  fd = dynaproOpenDevice(DYNAPRO_DEVICE);
  if (fd < 0) {
    return 0;
  }
  close(fd);

  return 1;
}

static void driver_get_info( InputDriverInfo *info )
{
  /* fill driver info structure */
  snprintf(info->name, DFB_INPUT_DRIVER_INFO_NAME_LENGTH,
           "dynapro" );
  snprintf(info->vendor, DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
           "3M" );

  info->version.major = 0;
  info->version.minor = 1;
}

static DFBResult driver_open_device(CoreInputDevice *device,
                                    unsigned int number,
                                    InputDeviceInfo *info,
                                    void **driver_data)
{
  int fd;
  dynaproData *data;

  /* open device */
  fd = dynaproOpenDevice(DYNAPRO_DEVICE);
  if(fd < 0) {
    D_PERROR("DirectFB/dynapro: Error opening '"DYNAPRO_DEVICE"'!\n");
    return DFB_INIT;
  }

  data = D_CALLOC(1, sizeof(dynaproData));
  data->fd     = fd;
  data->device = device;

  /* FIXME! Use settings instead? */
  data->screen_width  = DYNAPRO_SCREENWIDTH;
  data->screen_height = DYNAPRO_SCREENHEIGHT;
  data->min_x = DYNAPRO_MINX;
  data->min_y = DYNAPRO_MINY;

  /* fill device info structure */
  snprintf(info->desc.name, DFB_INPUT_DEVICE_DESC_NAME_LENGTH,
           "dynapro");
  snprintf(info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH,
           "3M");

  info->prefered_id     = DIDID_MOUSE;
  info->desc.type       = DIDTF_MOUSE;
  info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
  info->desc.max_axis   = DIAI_Y;
  info->desc.max_button = DIBI_LEFT;

  /* start input thread */
  data->thread = direct_thread_create(DTT_INPUT, 
												  dynaproTouchEventThread, 
												  data, 
												  "Dynapro Touch Input");

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

static void driver_close_device(void *driver_data)
{
  dynaproData *data = (dynaproData *)driver_data;

  /* stop input thread */
  direct_thread_cancel(data->thread);
  direct_thread_join(data->thread);
  direct_thread_destroy(data->thread);

  /* restore termnial settings for the port */
  tty_rawmode(data->fd, 0);

  /* close device */
  close(data->fd);

  /* free private data */
  D_FREE(data);
}
