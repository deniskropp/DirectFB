/*
   (c) Copyright 2003  Commercial Timesharing Inc.

   All rights reserved.

   Written by Byron Stanoszek <bstanoszek@comtime.com> and
              Brandon Reynolds <bmr@comtime.com>

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
#include <sys/file.h>

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


DFB_INPUT_DRIVER( elo )

#define elo_REPORT_SIZE		5
#define elo_PACKET_SIZE		10
#define elo_DEVICE		"/dev/ttyS0"
#define elo_SCREENWIDTH		800
#define elo_SCREENHEIGHT	600
#define elo_MINX		0
#define elo_MINY		0


/* Mode 1 Bit Definitions */
#define ELO_M_INITIAL 0x01  /* Enables initial pressing detection */
#define ELO_M_STREAM  0x02  /* Enables stream touch (when finger moves) */
#define ELO_M_UNTOUCH 0x04  /* Enables untouch detection */
#define ELO_M_RANGECK 0x40  /* Range checking mode */

/* Mode 2 Bit Definitions */
#define ELO_M_TRIM    0x02  /* Trim Mode */
#define ELO_M_CALIB   0x04  /* Calibration Mode */
#define ELO_M_SCALE   0x08  /* Scaling Mode */
#define ELO_M_TRACK   0x40  /* Tracking Mode */


typedef struct __eloData__ {
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
} eloData;


// Write a 10-byte character packet to the touch device.
//
static inline void elo_putbuf(int fd, unsigned char *data)
{
  unsigned char packet[10];
  int i;

  packet[0]='U';  /* Special serial lead-in byte */
  memcpy(packet+1, data, 8);
  packet[9]=0;

  /* Calculate checksum */
  for(i=1;i<9;i++)
    packet[9]+=packet[i];
  packet[9]--;

  write(fd, packet, 10);
}
// Read a packet from the touch device.
//
static inline unsigned char *elo_getbuf(int fd)
{
  static unsigned char packet[10];
  static int len=0, start=0;

  fd_set set;
  unsigned char checksum;
  int i, j;

  while(1) {
    /* look ahead to see if there is any data to be gotten */
    FD_ZERO(&set);
    FD_SET(fd, &set);
    if(!select(fd+1, &set, NULL, NULL, NULL))
      return NULL;

    /* read the next byte from the stream */
    if(read(fd, &packet[len++], 1) < 1)
      exit(1);
    if(!start && packet[len-1] != 0x55) {  /* search for `begin-packet' 0x55 */
      len=0;
      continue;
    }

    start=1;
    if(len < 10)  /* wait until we get 10 full bytes first */
      continue;

    /* check packet checksum when finished */
    for(i=1,checksum=0;i<9;i++)
      checksum+=packet[i];
    checksum--;

    /* checksum does not match */
    if(checksum != packet[9]) {
      /* scan buffer for next `begin-packet' byte 0x55 */
      for(i=1;i<10;i++)
        if(packet[i] == 0x55)
          break;
      if(i == 10) {  /* No other 0x55 found */
        len=0;
        continue;
      }
      for(j=0;i+j < 10;j++)  /* Move 0x55 to front of buffer */
        packet[j]=packet[i+j];
      len=j;
      continue;
    }

    /* We have a match. Return buffer string */
    len=0;  /* set for next match */

    return packet+1;
  }
}

// Remove all input translations over tty serial controller.
//
// set=1:  Saves current settings then turns rawmode on.
// set=0:  Restores controller to previous saved value.
//
static void tty_rawmode(int fd, int set)
{
  static struct termio tbuf, termio_save;

  if(set) {
    ioctl(fd, TCGETA, &termio_save);
    tbuf=termio_save;

    /* For complete explanation of the flags set/unset below, see termios(3)
       unix programmers reference manual. */

    tbuf.c_iflag = 0; /* No input processing */
    tbuf.c_oflag = 0; /* No output processing */
    tbuf.c_lflag = 0; /* Disable erase/kill, signals, and echo */

    tbuf.c_cflag = B9600|CS8|CLOCAL|CREAD;  /* Set baud & 1-char read mode */

    ioctl(fd, TCSETAF, &tbuf);
  } else
    ioctl(fd, TCSETAF, &termio_save);
}
// Set scaling info to touch device. Axis can be either 'X', 'Y', or 'Z'.
//
static int elo_set_scale(int fd, unsigned char axis, short low, short high)
{
  unsigned char packet[8], *ptr;

  if(axis < 'X' || axis > 'Z')
    return -1;

  memset(packet, 0, 8);
  packet[0]='S';
  packet[1]=axis;
  packet[2]=low;
  packet[3]=low >> 8;
  packet[4]=high;
  packet[5]=high >> 8;

  /* send packet until proper ack is received */
  while(1) {
    elo_putbuf(fd, packet);

    if(!(ptr=elo_getbuf(fd)))
      return -2;
    if(!strncmp("A0000", ptr, 5))
      return 0;
  }
}
// Set touch screen response packet mode (see #ifdefs in beginning of file).
//
static int elo_set_mode(int fd, unsigned char mode1, unsigned char mode2)
{
  unsigned char packet[8], *ptr;

  /* create packet */
  memset(packet, 0, 8);
  packet[0]='M';
  packet[2]=mode1;
  packet[3]=mode2;

  /* send packet until proper ack is received */
  while(1) {
    elo_putbuf(fd, packet);

    if(!(ptr=elo_getbuf(fd)))
      return -2;
    if(!strncmp("A0000", ptr, 5))
      return 0;
  }
}
// Wait for acknowledgment. Returns 0 if no packet received or 1 for success.
static int elo_check_ack(int fd)
{
  return elo_getbuf(fd)?1:0;
}
// Reset the touch-screen interface.
//
static int elo_reset_touch(int fd)
{
  fd_set set;
  struct timeval timeout={0, 100000};  /* 0.1 seconds */
  unsigned char packet[8];

  /* Send reset command */
  memset(packet, 0, 8);
  packet[0]='R';
  packet[1]=1;    /* 0=Hard reset, 1=Soft */
  elo_putbuf(fd, packet);

  /* Wait for response */
  FD_ZERO(&set);
  FD_SET(fd, &set);
  if(!select(fd+1, &set, NULL, NULL, &timeout))
    return -1;  /* no communications */

  if(!elo_check_ack(fd))
    return -1;  /* no valid packet received */

  /* Set the proper mode of operation:
      Initial Touch, Range Checking, Calibration, Scaling, and Trim. */

  elo_set_mode(fd,ELO_M_INITIAL|ELO_M_UNTOUCH|ELO_M_RANGECK,
                  ELO_M_CALIB|ELO_M_SCALE|ELO_M_TRIM);

  /* Set scaling to 80 x 25 */
  elo_set_scale(fd, 'X', elo_MINX, elo_SCREENWIDTH-1);
  elo_set_scale(fd, 'Y', elo_MINY, elo_SCREENHEIGHT-1);

  return 0;
}

static int eloOpenDevice(unsigned char *device)
{
  int fd;
  int res;

  if((fd = open(device, O_RDWR|O_NOCTTY)) == -1)
    return -1;

  if((flock(fd, LOCK_EX|LOCK_NB)) == -1) {
    D_PERROR("DirectFB/elo: Error locking '%s'!\n",device);
    close(fd);
    return -1;
  }

  tty_rawmode(fd,1);

  if((res=elo_reset_touch(fd))) {
    close(fd);
    return -1;
  }

  return fd;
}
//
//
static int eloGetEvent(eloData *event)
{
  unsigned char *ptr;

  /* read in the packet */
  if(!(ptr=elo_getbuf(event->fd)))
    return -1;

  if(ptr[0] != 'T')
    return -1;

  /* Get touch coordinates */
  event->x = ptr[2]+(ptr[3] << 8);
  event->y = ptr[4]+(ptr[5] << 8);

  /* Check for invalid range -- reset touch device if so */
  if(event->x >= event->screen_width
     || event->y >= event->screen_height) {
    elo_reset_touch(event->fd);
    return -1;
  }

  event->action = ptr[1];

  return 0; // valid touch
}

/* The main routine for elo */
static void *eloTouchEventThread(DirectThread *thread, void *driver_data)
{
  eloData *data = (eloData *) driver_data;

   /* Read data */
  while (1) {
    DFBInputEvent evt;

    if(eloGetEvent(data) == -1) continue;
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
    if(data->action & ELO_M_UNTOUCH)
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

  fd = eloOpenDevice (elo_DEVICE);
  if (fd < 0)
    return 0;

  close(fd);

  return 1;
}

static void driver_get_info( InputDriverInfo *info )
{
  /* fill driver info structure */
  snprintf(info->name, DFB_INPUT_DRIVER_INFO_NAME_LENGTH,
           "elo" );
  snprintf(info->vendor, DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
           "elo Systems" );

  info->version.major = 0;
  info->version.minor = 1;
}

static DFBResult driver_open_device(CoreInputDevice *device,
                                    unsigned int number,
                                    InputDeviceInfo *info,
                                    void **driver_data)
{
  int fd;
  eloData *data;

  /* open device */
  fd = eloOpenDevice(elo_DEVICE);
  if(fd < 0) {
    D_PERROR("DirectFB/elo: Error opening '"elo_DEVICE"'!\n");
    return DFB_INIT;
  }

  data = D_CALLOC(1, sizeof(eloData));

  data->fd     = fd;
  data->device = device;

  /* Must define the correct resolution of screen */
  data->screen_width  = elo_SCREENWIDTH;
  data->screen_height = elo_SCREENHEIGHT;

  /* The following variable are defined to adjust the orientation of
   * the touchscreen. Variables are either max screen height/width or 0.
   */
  data->min_x = elo_MINX;
  data->min_y = elo_MINY;

  /* fill device info structure */
  snprintf(info->desc.name, DFB_INPUT_DEVICE_DESC_NAME_LENGTH,
           "elo");
  snprintf(info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH,
           "elo Systems");

  info->prefered_id     = DIDID_MOUSE;
  info->desc.type       = DIDTF_MOUSE;
  info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
  info->desc.max_axis   = DIAI_Y;
  info->desc.max_button = DIBI_LEFT;

  /* start input thread */
  data->thread = direct_thread_create (DTT_INPUT, eloTouchEventThread, data, "ELO Touch Input");

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
  eloData *data = (eloData *)driver_data;

  /* stop input thread */
  direct_thread_cancel(data->thread);
  direct_thread_join(data->thread);
  direct_thread_destroy(data->thread);

  /* restore termnial settings for the port */
  tty_rawmode(data->fd,0);

  /* close device */
  close(data->fd);

  /* free private data */
  D_FREE(data);
}
