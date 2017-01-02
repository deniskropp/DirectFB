/*
   (c) Copyright 2000-2002  Fulgid Technology Co., Ltd.

   All rights reserved.

   Written by Simon Ueng <simon@ftech.com.tw>

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

   This driver is a re-write from the MuTouch driver provided in XFree86.
   Extended options in the MuTouch firmware are not used as it wasn't
   an issue during the writing of this driver.
   Baudrate has also been rewritting to automatically adjust to 19200 if
   its support else it uses 9600.

   Feel free to change according to your needs, but changing both
   MuT_MINX as well as MuT_MINY is required to adjust the orientation
   of the touchscreen. We've had numerous occasions where the one
   touchscreen was installed in a different orientation than others.
   Also don't forget to adjust the MuT_SCREENWIDTH and MuT_SCREENHEIGHT
   for the appropriate screen dimension.

   Not much time has been spent writing this driver therefore expect errors.
   Calibration process has also been omitted since there are applications
   to do the calibration process.

   Lastly, please don't email me regarding technical information. I
   don't work for 3M. But if you have any new ideas on improving this
   driver, please don't hesitate to share it with me.
   */
/*
 * Jacques Luder j.luder@laposte.net 
 * Baudrate adjust now realy working ! 
 * version 0.3 10/09/2006 Jacques Luder j.luder@laposte.net
 * Can now work also with a keyspan USB-Serial line.
 * Can start OK even if someone touch the screen during probing.
 */

#include <config.h>

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


DFB_INPUT_DRIVER( mutouch )


#define BAUDRATE         B9600
#define OPTIMAL_BAUDRATE B19200
#define MAX_TIMEOUT      5

#define MuT_REPORT_SIZE  5
#define MuT_PACKET_SIZE  10
#define MuT_SCREENWIDTH  4096
#define MuT_SCREENHEIGHT 4096
#define MuT_MINX         0
#define MuT_MINY         0

#define MuT_LEAD_BYTE    0x01
#define MuT_TRAIL_BYTE   0x0d


#define ERROR_NOT_SUITABLE    -1110  /* The touchpanel firmware is not
                                        suitable for IMP2001 */
#define ERROR_NOT_FOUND       -1111  /* Touchpanel not found */
#define ERROR_INIT            -1112  /* Error occurred while initializing */

#define MUT_PANEL_TOUCH       0x01
#define MUT_PANEL_UNTOUCH     0x00


/* Commands */
#define MuT_RESET             "R"
#define MuT_AUTOBAUD_DISABLE  "AD"
#define MuT_RESTORE_DEFAULTS  "RD"
#define MuT_QUERY             "Z"
#define MuT_FORMAT_TABLET     "FT"
#define MuT_FORMAT_RAW        "FR"
#define MuT_CALIBRATE_RAW     "CR"
#define MuT_CALIBRATE_EXT     "CX"
#define MuT_OUTPUT_IDENT      "OI"
#define MuT_UNIT_TYPE         "UT"
#define MuT_FINGER_ONLY       "FO"
#define MuT_PEN_ONLY          "PO"
#define MuT_PEN_FINGER        "PF"
#define MuT_MODE_STREAM       "MS"
#define MuT_MODE_DOWN_UP      "MDU"

/* Command reply */
#define MuT_OK                '0'
#define MuT_ERROR             '1'

/* Offsets in status byte of touch and motion reports */
#define MuT_WHICH_DEVICE      0x20
#define MuT_CONTACT           0x40

/* Identity and friends */
#define MuT_SMT3_IDENT        "Q1"
#define MuT_THRU_GLASS_IDENT  "T1"

/* Event mask */
#define MuT_PANEL_TOUCH_MASK  0x40
#define MuT_PANEL_SYNC_MASK   0x80

typedef struct __MuTData__ {
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
} MuTData;

/* Global Variables */
static unsigned char packet[MuT_PACKET_SIZE];


static inline void __mdelay(unsigned int msec)
{
     struct timespec delay;

     delay.tv_sec = 0;
     delay.tv_nsec = msec * 1000000;
     nanosleep (&delay, NULL);
}

static inline void MuTSendPacket(int file, char *packet, unsigned char len)
{
     int res;
     unsigned char tmp_packet[MuT_PACKET_SIZE];

     direct_memcpy (&tmp_packet[1], packet, len);
     *tmp_packet = MuT_LEAD_BYTE;
     tmp_packet[len + 1] = MuT_TRAIL_BYTE;
     res = write (file, tmp_packet, len + 2);
     (void)res;
}

static inline void MuTReadPacket(int file, unsigned char *packet)
{
     int n = 0;
     int res;

     while ( n < MuT_REPORT_SIZE ) {
          res = read( file, &packet[n], 1 );
          (void)res;
          if ( (packet[0] & MuT_PANEL_SYNC_MASK) != 0) {
               n++;
          }
     } 
}

static int MuTSetToOptimalCTRL(int file, unsigned long baud)
{
     unsigned char packet[3]={0,0,0};
     struct termios options;
     struct termios save_options;
     /*
     * We are now in a mode who "works" but may be not the good one !
     * So we will make a test if a "good" mode works fine ...
     */

     tcgetattr(file, &options);
     tcgetattr(file, &save_options);   // Save mode who works.
     /*
     * Send a command to change to suitable mode.
     */
     switch (baud) {
          case B19200:
               MuTSendPacket (file, "PN811", 5);
               break;
          case B9600:
               MuTSendPacket (file, "PN812", 5);
               break;
     }
     // Note answer from driver will be lost ! there is no way to catch it with some
     // USB-Serial device. we have only to wait !
     __mdelay (20);

     /*
     * We can't work on CS7 mode messages format are not the same !
     */
     options.c_cflag =  CS8 | CLOCAL | CREAD;
     /*
     * POSIX says speed is ONLY here !
     */
     cfsetospeed( &options, baud);
     cfsetispeed( &options, 0);    // Same as output.

     tcsetattr (file, TCSANOW, &options);
     __mdelay (100);
     tcflush (file, TCIFLUSH);
     /*
     * With some USB-Serial device flush may occurs after sending message and answer from driver 
     * may be lost ! ... so we wait !
     */
     __mdelay (100);

     switch (baud) {
          case B19200:
               MuTSendPacket (file, "PN811", 5);
               break;
          case B9600:
               MuTSendPacket (file, "PN812", 5);
               break;
     }
     /* 
     * note if someone touch screen, we have also to strip "touch" message by testing MuT_LEAD_BYTE.
     */
     __mdelay (100);
     int n = 0;
     while ( n <3 ) {
          if (read( file, &packet[n], 1 ) <=0)
               break;
          if ( packet[n] == MuT_LEAD_BYTE ) {       // new start
               n = 0;
               packet[0] = MuT_LEAD_BYTE;
          }
          if ( (n==1) && (packet[1] != MuT_OK)) {
               n = 0;
               packet[0]= 0;
          }
          if ( (n==2) && (packet[2] != MuT_TRAIL_BYTE) ) {
               n = 0;
               packet[0]= 0;
          }
          if ( packet[0] == MuT_LEAD_BYTE)
               n++;
     }

     if ( n == 3 ) {
          /* Unset O_NONBLOCK */
          int flags;
          flags = fcntl( file,  F_GETFL );
          flags &= ~O_NONBLOCK;
          fcntl( file, F_SETFL, flags );
          /* Unset VTIME */  
          options.c_cc[VMIN] = 1;
          options.c_cc[VTIME] = 0;
          tcsetattr (file, TCSANOW, &options);
          __mdelay (100);

          return 1;   // OK
     }
     // KO return to old termios options.
     tcsetattr (file, TCSANOW, &save_options);
     __mdelay (100);
     return 0;
}


static int MuTPollDevice(char *device)
{
     int file;
     struct termios options;
     unsigned char i, m;
     unsigned char packet[MuT_PACKET_SIZE];
     unsigned long baud[2] = {B9600, B19200};
     unsigned long misc[2] = {CS8, CS7 | CSTOPB};

     file = open (device, O_RDWR | O_NOCTTY);

     /* Set O_NONBLOCK */
     int flags;
     flags = fcntl( file,  F_GETFL );
     flags |= O_NONBLOCK;
     fcntl( file, F_SETFL, flags );


     /* Make raw I/O */
     memset (&options, 0, sizeof (struct termios));

     /* cfmakeraw(&options) */
     options.c_cc[VMIN] = 0;
     options.c_cc[VTIME] = 10;

     /* loop through the bauds */
     for (i = 0; i < 2; i++) {
          /* loop through the misc configs */
          for (m = 0; m < 2; m++) {
               options.c_cflag =  misc[m] | CLOCAL | CREAD;
               /*
               * POSIX
               */
               cfsetospeed( &options, baud[i]);
               cfsetispeed( &options, 0);
               tcsetattr (file, TCSANOW, &options);
               /* wait for some USB-Serial device */
               __mdelay (100);
               tcflush (file, TCIFLUSH);
               __mdelay (100);

               MuTSendPacket (file, MuT_QUERY, strlen (MuT_QUERY));
               /* we are in O_NONBLOCK  need wait before read  */
               __mdelay (100);

               int n = 0;
               while ( n <3 ) {
                    if (read( file, &packet[n], 1 ) <=0)
                         break;
                    if ( packet[n] == MuT_LEAD_BYTE ) {       // new start
                         n = 0;
                         packet[0] = MuT_LEAD_BYTE;
                    }
                    if ( (n==1) && (packet[1] != MuT_OK)) {    // false start
                         n = 0;
                         packet[0]= 0;
                    }
                    if ( (n==2) && (packet[2] != MuT_TRAIL_BYTE) ) {  // false start
                         n = 0;
                         packet[0]= 0;
                    }
                    if ( packet[0] == MuT_LEAD_BYTE)
                         n++;
               }

               if ( n == 3 ) {


                    if (MuTSetToOptimalCTRL (file, B19200)) {
                         return file;
                    }
                    else if (MuTSetToOptimalCTRL (file, B9600)) {
                         return file;
                    }
                    else return ERROR_NOT_SUITABLE;
               }
          }
     }
     close(file);
     return ERROR_NOT_FOUND;
}

static int MuTInitCmd(int file, char *cmd)
{
     unsigned char timeout = 0;
     unsigned char packet[MuT_PACKET_SIZE];

     do {
          MuTSendPacket (file, cmd, strlen (cmd));

          /*
          * Strip out 'touch" message
          */
          int n = 0;
          while ( n <3 ) {
               if (read( file, &packet[n], 1 ) <=0)
                    break;
               if ( packet[0] == MuT_LEAD_BYTE)
                    n++;
          }

          timeout++;
          if (timeout >= MAX_TIMEOUT)
               return ERROR_INIT;
     } while (packet[1] != MuT_OK);

     return 1;
}

static inline int MuTInitialize(int file)
{
     if (!MuTInitCmd (file, MuT_RESET))
          return 0;
     if (!MuTInitCmd (file, MuT_FINGER_ONLY))
          return 0;
     if (!MuTInitCmd (file, MuT_MODE_DOWN_UP))
          return 0;
     if (!MuTInitCmd (file, MuT_FORMAT_TABLET))
          return 0;

     return 1;
}

static int MuTOpenDevice(char *device)
{
     int fd;
     int res;

     fd = MuTPollDevice (device);
     if ((res = MuTInitialize (fd)) == 0) {
          close (fd);
          return res;
     }

     return fd;
}

#define WORD_ASSEMBLY(byte1, byte2)	(((byte2) << 7) | (byte1))
static int MuTGetEvent(MuTData *event)
{
     MuTReadPacket(event->fd, packet);
     
     /* Sync bit always must be set to 1 */
     if ((*packet & MuT_PANEL_SYNC_MASK) == 0)
          return 0;
     
     if (*packet & MuT_PANEL_TOUCH_MASK)
          event->action = MUT_PANEL_TOUCH;
     else
          event->action = MUT_PANEL_UNTOUCH;

     event->x = (event->screen_width *
                 WORD_ASSEMBLY(packet[1], packet[2])) / 16383;
     event->y = (event->screen_height *
                 WORD_ASSEMBLY(packet[3], packet[4])) / 16383;

     if (event->min_x)
          event->x = event->min_x - event->x;
     if (event->min_y)
          event->y = event->min_y - event->y;

     return 1;
}

/* The main routine for MuTouch */
static void *MuTouchEventThread(DirectThread *thread, void *driver_data)
{
     MuTData *data = (MuTData *) driver_data;

     /* Read data */
     while (1) {
          DFBInputEvent evt;

          if (!MuTGetEvent (data))
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
          switch (data->action) {
               case MUT_PANEL_TOUCH:
                    evt.type = DIET_BUTTONPRESS;
                    break;
               case MUT_PANEL_UNTOUCH:
                    evt.type = DIET_BUTTONRELEASE;
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

static int driver_get_available( void )
{
     int fd;

     if (!dfb_config->mut_device)
          return 0;

     fd = MuTOpenDevice (dfb_config->mut_device);
     if (fd < 0)
          return 0;

     close(fd);

     return 1;
}

static void driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf(info->name, DFB_INPUT_DRIVER_INFO_NAME_LENGTH,
              "MuTouch" );
     snprintf(info->vendor, DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
              "Microtouch" );

     info->version.major = 0;
     info->version.minor = 3;
}

static DFBResult driver_open_device(CoreInputDevice *device,
                                    unsigned int number,
                                    InputDeviceInfo *info,
                                    void **driver_data)
{
     int fd;
     MuTData *data;

     /* open device */
     fd = MuTOpenDevice (dfb_config->mut_device);
     D_INFO("MuTouch:driver_open_device %s fd %d\n", dfb_config->mut_device,fd);

     if (fd < 0) {
          return DFB_INIT;
     }

     data = D_CALLOC (1, sizeof(MuTData));
     if (!data) {
          close (fd);
          return D_OOM ();
     }

     data->fd     = fd;
     data->device = device;

     /* Must define the correct resolution of screen */
     data->screen_width  = MuT_SCREENWIDTH;
     data->screen_height = MuT_SCREENHEIGHT;

     /* The following variable are defined to adjust the orientation of
      * the touchscreen. Variables are either max screen height/width or 0.
      */
     data->min_x = MuT_MINX;
     data->min_y = MuT_MINY;

     /* fill device info structure */
     snprintf(info->desc.name, DFB_INPUT_DEVICE_DESC_NAME_LENGTH,
              "MuTouch");
     snprintf(info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH,
              "Microtouch");

     info->prefered_id     = DIDID_MOUSE;
     info->desc.type       = DIDTF_MOUSE;
#ifndef DIRECTFB_DISABLE_DEPRECATED
     info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
#else
     info->desc.caps       = DIDCAPS_AXES | DIDCAPS_BUTTONS;
#endif
     info->desc.max_axis   = DIAI_Y;
     info->desc.max_button = DIBI_LEFT;

     /* start input thread */
     data->thread = direct_thread_create (DTT_INPUT, MuTouchEventThread, data, "MuTouch Input");

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
     MuTData *data = (MuTData *)driver_data;

     /* stop input thread */
     direct_thread_cancel (data->thread);
     direct_thread_join (data->thread);
     direct_thread_destroy (data->thread);

     /* close device */
     close (data->fd);

     /* free private data */
     D_FREE (data);
}

