/*
   (c) Copyright 2005  Gnat Solutions, Inc.

   All rights reserved.

   Written by Nathanael D. Noblet <nathanael@gnat.ca>

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

   This driver is a re-write from the gunze driver provided in XFree86. 
   Using the MuTouch driver as a skeleton.

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

DFB_INPUT_DRIVER( gunze )

#define BUFFER_SIZE         64          /* size of reception buffer */
#define GUNZE_MAXPHYSCOORD  1023
#define GUNZE_MAXCOORD      (64*1024-1) /* oversampled, synthetic value */
#define FLAG_TAPPING        1
#define FLAG_WAS_UP         2
#define BAUDRATE            B9600

#define GunzeT_PACKET_SIZE     10
#define GunzeT_SCREENWIDTH     1024
#define GunzeT_SCREENHEIGHT    768
#define GunzeT_MINX            0
#define GunzeT_MINY            0
#define GunzeT_DEFAULT_DEVICE   "/dev/ttyS0"

#define GUNZE_PANEL_TOUCH       T
#define GUNZE_PANEL_UNTOUCH     R

#define GUNZE_SERIAL_DLEN 11
#define GUNZE_PS2_DLEN     3

#define GUNZE_SECTION_NAME    "gunze"
#define GUNZE_DEFAULT_CFGFILE "/etc/gunzets.calib"

#define SYSCALL(call) while(((call) == -1) && (errno == EINTR))

/*
* Serial protocol (11 b):  <T-or-R> <4-bytes-for-X> , <4-bytes-for-Y> 0x0d
*/
typedef struct __GunzeTData__
{
    const char *gunDevice;	/* device file name */
    DirectThread *thread;
    CoreInputDevice *device;
    unsigned int x;
    unsigned int y;
    unsigned int screen_width;
    unsigned int screen_height;
    int     flags;                 /* various flags */
    int     gunType;               /* TYPE_SERIAL, etc */
    int     gunBaud;               /* 9600 or 19200 */
    int     gunDlen;               /* data length (3 or 11) */
    int     gunPrevX;              /* previous X position */
    int     gunPrevY;              /* previous Y position */
    int     gunSmooth;             /* how smooth the motion is */
    int     gunTapping;            /* move-and-tap (or press-only) not implemented */
    int     gunPrevButtonState;    /* previous button state */
    int     gunBytes;              /* number of bytes read */
    unsigned char gunData[16];     /* data read on the device */
    int         gunCalib[4];       /* calibration data */
    char	*gunConfig;            /* filename for configuration */
    int     fd;
} GunzeTData;

enum devicetypeitems {
    TYPE_UNKNOWN = 0,
    TYPE_SERIAL = 1,
    TYPE_PS2,
    TYPE_USB
};

static int GunzeReadCalib(GunzeTData *priv)
{
    int i=1;
    int err = 1;
    FILE *f;
    priv->gunCalib[0]=priv->gunCalib[1]=priv->gunCalib[2]=priv->gunCalib[3]=0;
    f = fopen(priv->gunConfig, "r");
    if (f)
    {
        char s[80];
        fgets(s, 80, f); /* discard the comment */
        if (fscanf(f, "%d %d %d %d", priv->gunCalib, priv->gunCalib+1, priv->gunCalib+2, priv->gunCalib+3) == 4)
        {
            D_PERROR("DirectFB/gunze: Calibration invalid 0 i:%d gunCalib[0]:%d \n",i,priv->gunCalib[0]);
            err = 0;
        }

        for (i=0; i<4; i++) 
        {
            if (priv->gunCalib[i] & ~1023)
            {
                D_PERROR("DirectFB/gunze: Calibration invalid 0 i:%d gunCalib[i]:%d \n",i,priv->gunCalib[i]);
                err++;
            }
            if (abs(priv->gunCalib[0] - priv->gunCalib[2]) < 100)
            {
                D_PERROR("DirectFB/gunze: Calibration invalid 1 0:%d 2:%d abs(%d)\n",priv->gunCalib[0],priv->gunCalib[2],abs(priv->gunCalib[0] - priv->gunCalib[2]));
                err++;
            }
            if (abs(priv->gunCalib[1] - priv->gunCalib[3]) < 100)
            {
                D_PERROR("DirectFB/gunze: Calibration invalid 2 1:%d 3:%d abs(%d)\n",priv->gunCalib[1],priv->gunCalib[3],abs(priv->gunCalib[1] - priv->gunCalib[3]));
                err++;
            }
            fclose(f);
        }
    }
    if (err)
    {
      D_PERROR("DirectFB/gunze: Calibration data absent or invalid, using defaults\n");
      priv->gunCalib[0] = priv->gunCalib[1] = 128; /* 1/8 */
      priv->gunCalib[2] = priv->gunCalib[3] = 896; /* 7/8 */
    }
    return 0;
}

static int GunzeSetToOptimalCTRL(int file)
{
    struct termios  newtio;
    int err;

    SYSCALL(err = tcgetattr(file, &newtio));

    if (err == -1) {
        D_PERROR("DirectFB/gunze: Gunze touch screen tcgetattr failed\n");
        return 0;
    }

    memset(&newtio,0, sizeof(newtio)); /* clear struct for new port settings */
/*
    BAUDRATE: Set bps rate. You could also use cfsetispeed and cfsetospeed.
    CRTSCTS : output hardware flow control (only used if the cable has
            all necessary lines. See sect. 7 of Serial-HOWTO)
    CS8     : 8n1 (8bit,no parity,1 stopbit)
    CLOCAL  : local connection, no modem contol
    CREAD   : enable receiving characters
*/
    newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
/*
    IGNPAR  : ignore bytes with parity errors
    ICRNL   : map CR to NL (otherwise a CR input on the other computer will not terminate input)
    otherwise make device raw (no other input processing)
*/
    newtio.c_iflag = IGNPAR | ICRNL;
/*  Raw output */
    newtio.c_oflag = 0;
/*
    ICANON  : enable canonical input
    disable all echo functionality, and don't send signals to calling program
*/
    newtio.c_lflag = ICANON;

/*
    initialize all control characters 
    default values can be found in /usr/include/termios.h, and are given
    in the comments, but we don't need them here
*/
    newtio.c_cc[VINTR]    = 0;     /* Ctrl-c */ 
    newtio.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
    newtio.c_cc[VERASE]   = 0;     /* del */
    newtio.c_cc[VKILL]    = 0;     /* @ */
    newtio.c_cc[VEOF]     = 4;     /* Ctrl-d */
    newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
    newtio.c_cc[VSWTC]    = 0;     /* '\0' */
    newtio.c_cc[VSTART]   = 0;     /* Ctrl-q */ 
    newtio.c_cc[VSTOP]    = 0;     /* Ctrl-s */
    newtio.c_cc[VSUSP]    = 0;     /* Ctrl-z */
    newtio.c_cc[VEOL]     = 0;     /* '\0' */
    newtio.c_cc[VREPRINT] = 0;     /* Ctrl-r */
    newtio.c_cc[VDISCARD] = 0;     /* Ctrl-u */
    newtio.c_cc[VWERASE]  = 0;     /* Ctrl-w */
    newtio.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
    newtio.c_cc[VEOL2]    = 0;     /* '\0' */

    /* now clean the modem line and activate the settings for the port */

    tcflush(file, TCIFLUSH);
    err = tcsetattr(file, TCSANOW, &newtio);
    if (err == -1) {
        D_PERROR("DirectFB/gunze: Gunze touch screen tcsetattr TCSANOW failed\n");
        return 0;
    }

    return 1;
}

static int GunzeOpenDevice(const char *device)
{
    int fd;
    int res;

    /* opens device */
    SYSCALL(fd = open(device, O_RDWR | O_NOCTTY));
    if (fd == -1)
    {
        D_PERROR("DirectFB/gunze: Error opening device %s\n",device);
        return fd;
    }

    /* setup termios settings for communication */
    if((res = GunzeSetToOptimalCTRL(fd)) == 0)
    {
        close(fd);
        return res;
    }

    return fd;
}

/* The main routine for GunzeTouch */
static void *GunzeTouchEventThread(DirectThread *thread, void *driver_data)
{
    GunzeTData *priv = (GunzeTData *) driver_data;
    unsigned char *pkt = priv->gunData;
    int len, loop;
    int x =0;
    int y =0;
    int button =0;
    int *calib = priv->gunCalib;
    unsigned char buffer[BUFFER_SIZE];

    while ((len = read( priv->fd, buffer, BUFFER_SIZE)) >= 0 || errno == EINTR)
    {

        if (len <= 0) 
        {
            D_PERROR("DirectFB/gunze: error reading Gunze touch screen device %d %d\n",errno,priv->fd);
            perror(NULL);
            return NULL;
        }

        for(loop=0; loop<len; loop++) 
        {
            /* if first byte, ensure that the packet is syncronized */
            if (priv->gunBytes == 0) 
            {
                int error  = 0;
                if (priv->gunDlen == GUNZE_SERIAL_DLEN) 
                {
                    /* First byte is 'R' (0x52) or 'T' (0x54) */
                    if ((buffer[loop] != 'R') && (buffer[loop] != 'T'))
                        error = 1;
                }
                /* PS/2 / USB Unsupported for now (basically I don't have one, and didn't test adding support will be trivial
                else  
                {
                    if ( !(buffer[loop] & 0x80) || (len > loop+1 && !(buffer[loop+1] & 0x80)) || (len > loop+2 && (buffer[loop+2]  & 0x80)))
                        error = 1;
                }
                */
                if (error)
                {
                    D_PERROR("DirectFB/gunze: GunzeReadInput: bad first byte 0x%x %c\n",buffer[loop],buffer[loop]);
                    continue;
                }
            }

            pkt[priv->gunBytes++] = buffer[loop];

            /* Hack: sometimes a serial packet gets corrupted. If so, drop it */
            if (buffer[loop] == 0x0d && priv->gunBytes != priv->gunDlen && priv->gunDlen == GUNZE_SERIAL_DLEN) 
            {
                pkt[priv->gunBytes-1] = '\0';
                D_PERROR("DirectFB/gunze: Bad packet \"%s\" dropping it\n", pkt);
                priv->gunBytes = 0;
                continue;
            }

            /* if whole packet collected, decode it */
            if (priv->gunBytes == priv->gunDlen) 
            {
                priv->gunBytes = 0;
                if (priv->gunDlen == GUNZE_SERIAL_DLEN) 
                {
                    /* if T button == true */
                    button = (pkt[0] == 'T');
                    x = atoi((char *)pkt+1);
                    y = atoi((char *)pkt+6);
                }
                /* USB version which I haven't added support for 
                else
                {
                    button = (pkt[2] & 0x40);
                    x = ((pkt[0] & 0x7f) << 3) | ((pkt[1] & 0x70) >> 4);
                    y = ((pkt[1] & 0x0f) << 6) | ((pkt[2] & 0x3f));
                }
                */

                if (x>1023 || x<0 || y>1023 || y<0) 
                {
                    D_PERROR("DirectFB/gunze: Bad packet \"%s\" -> %i,%i\n", pkt, x, y);

                    priv->gunBytes = 0;
                    continue;
                }

                /*
                Ok, now that we have raw data, turn it to real data
                according to calibration, smoothness, tapping and debouncing
                calibrate and rescale (by multiplying by 64) 

                I don't fully understand this, came from xf86Gunze.c
                x = 64*128 + 64*768 * (x - calib[0])/(calib[2]-calib[0]);
                y = 64*128 + 64*768 * (y - calib[1])/(calib[3]-calib[1]);
                */

                x = 8192 + 49152 * (x - calib[0])/(calib[2]-calib[0]);
                y = 8192 + 49152 * (y - calib[1])/(calib[3]-calib[1]);
                y = GUNZE_MAXCOORD - y;
                /* smooth it down, unless first touch */
                if (!(priv->flags & FLAG_WAS_UP)) 
                {
                    x = (priv->gunPrevX * priv->gunSmooth + x)/(priv->gunSmooth+1);
                    y = (priv->gunPrevY * priv->gunSmooth + y)/(priv->gunSmooth+1);
                }

                /* convert gunze x/y to screen dimensions */
                x = x * priv->screen_width  / (GUNZE_MAXCOORD);
                y = y * priv->screen_height / (GUNZE_MAXCOORD);
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                if (x > priv->screen_width)  x = priv->screen_width;
                if (y > priv->screen_height) y = priv->screen_height;

                if (!button)
                    priv->flags |= FLAG_WAS_UP;
                else
                    priv->flags &= ~FLAG_WAS_UP;

                /* Now send events */
                DFBInputEvent evt;

                /* only post new x/y if different from previous tap */
                if ( (priv->gunPrevX != x) || (priv->gunPrevY != y) )
                {
                    direct_thread_testcancel (thread);
                    /* Dispatch axis */
                    evt.type    = DIET_AXISMOTION;
                    evt.flags   = DIEF_AXISABS;
                    evt.axis    = DIAI_X;
                    evt.axisabs = x;
                    dfb_input_dispatch (priv->device, &evt);

                    evt.type    = DIET_AXISMOTION;
                    evt.flags   = DIEF_AXISABS;
                    evt.axis    = DIAI_Y;
                    evt.axisabs = y;
                    dfb_input_dispatch (priv->device, &evt);
                    direct_thread_testcancel (thread);
                    /*printf("dispatched motion x %d y %d\n",x,y);*/
                }
                /* post button state change at x/y */
                if (priv->gunPrevButtonState != button)
                {
                    direct_thread_testcancel (thread);

                    /* Dispatch axis */
                    evt.type    = DIET_AXISMOTION;
                    evt.flags   = DIEF_AXISABS;
                    evt.axis    = DIAI_X;
                    evt.axisabs = x;
                    dfb_input_dispatch (priv->device, &evt);

                    evt.type    = DIET_AXISMOTION;
                    evt.flags   = DIEF_AXISABS;
                    evt.axis    = DIAI_Y;
                    evt.axisabs = y;
                    dfb_input_dispatch (priv->device, &evt);

                    /* Dispatch touch event */
                    evt.type = (button) ? DIET_BUTTONPRESS:DIET_BUTTONRELEASE;
                    evt.flags  = DIEF_NONE;
                    evt.button = DIBI_LEFT;

                    dfb_input_dispatch (priv->device, &evt);
                    direct_thread_testcancel (thread);
                    /* printf("dispatched button x %d y %d\n",x,y); */
                }

                /* remember data */
                priv->gunPrevButtonState = button;
                priv->gunPrevX = x;
                priv->gunPrevY = y;

            }
        }
    }

    return NULL;
}

/* exported symbols */

static int driver_get_available( void )
{
    int fd;
    /*
    if (!dfb_config->gunze_device){
        D_PERROR("DirectFB/GunzeTouch: Missing Gunze device file using default %s!\n",GunzeT_DEFAULT_DEVICE);
        fflush(NULL);
    }*/

/*    
    if (!dfb_config->gunze_device)
        return 0;
    */
    /* TODO gotta clean this up... seems odd to open & close it like this... */
     //fd = GunzeOpenDevice (dfb_config->gunze_device);
    fd = GunzeOpenDevice(GunzeT_DEFAULT_DEVICE);
    if (fd < 0)
        return 0;

    close(fd);

    return 1;
}

static void driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf(info->name, DFB_INPUT_DRIVER_INFO_NAME_LENGTH,"gunze" );
     snprintf(info->vendor, DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,"Gunze" );

     info->version.major = 0;
     info->version.minor = 1;
}

static DFBResult driver_open_device(CoreInputDevice *device,
                                    unsigned int number,
                                    InputDeviceInfo *info,
                                    void **driver_data)
{
    int fd;
    GunzeTData *data = NULL;
    const char *devname = GunzeT_DEFAULT_DEVICE;


//    if (!dfb_config->gunze_device)
//        D_PERROR("DirectFB/GunzeTouch: Missing Gunze device file using default %s!\n",GunzeT_DEFAULT_DEVICE);


    /* open device */
    fd = GunzeOpenDevice (devname);

    if (fd < 0) {
        D_PERROR("DirectFB/GunzeTouch: Error opening '%s'!\n", devname);
        return DFB_INIT;
    }

    data = D_CALLOC (1, sizeof(GunzeTData));

    data->fd = fd;
    data->device = device;
    data->gunDevice= devname;
    /* hard coded... */
    data->gunType = TYPE_SERIAL;
    data->gunDlen = GUNZE_SERIAL_DLEN;
    /* Must define the correct resolution of screen */
    data->screen_width  = GunzeT_SCREENWIDTH;
    data->screen_height = GunzeT_SCREENHEIGHT;
    data->gunConfig = GUNZE_DEFAULT_CFGFILE;//(!dfb_config->gunze_calibration_file)?GUNZE_DEFAULT_CFGFILE:dfb_config->gunze_calibration_file;

    /* The following variable are defined to adjust the orientation of
    * the touchscreen. Variables are either max screen height/width or 0.
    * don't know how to use this as it was taken from mutouch likely not needed...

    data->min_x = GunzeT_MINX;
    data->min_y = GunzeT_MINY;
*/
    /* grab calibration data */
    GunzeReadCalib(data);

    /* fill device info structure */
    snprintf(info->desc.name, DFB_INPUT_DEVICE_DESC_NAME_LENGTH,"gunze");
    snprintf(info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH,"Gunze");

    info->prefered_id     = DIDID_MOUSE;
    info->desc.type        = DIDTF_MOUSE;
    info->desc.caps        = DICAPS_AXES | DICAPS_BUTTONS;
    info->desc.max_axis   = DIAI_Y;
    info->desc.max_button = DIBI_LEFT;

    /* start input thread */
    data->thread = direct_thread_create (DTT_INPUT, GunzeTouchEventThread, data, "GunzeTouch Input");

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
     GunzeTData *data = (GunzeTData *)driver_data;

     /* stop input thread */
     direct_thread_cancel (data->thread);
     direct_thread_join (data->thread);
     direct_thread_destroy (data->thread);

     /* close device */
     close (data->fd);

     /* free private data */
     D_FREE (data);
}

