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

#include <termios.h>

#include <sys/utsname.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/system.h>

#include <misc/conf.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( ps2mouse )


/* Stolen from the linux kernel (pc_keyb.h) */
#define PS2_SET_RES      0xE8    /* Set resolution     */
#define PS2_SET_SCALE11  0xE6    /* Set 1:1 scaling    */
#define PS2_SET_SCALE21  0xE7    /* Set 2:1 scaling    */
#define PS2_GET_SCALE    0xE9    /* Get scaling factor */
#define PS2_SET_STREAM   0xEA    /* Set stream mode    */
#define PS2_SET_SAMPLE   0xF3    /* Set sample rate    */
#define PS2_ENABLE_DEV   0xF4    /* Enable aux device  */
#define PS2_DISABLE_DEV  0xF5    /* Disable aux device */
#define PS2_RESET        0xFF    /* Reset aux device   */
#define PS2_ACK          0xFA    /* Command byte ACK   */

/*** mouse commands ***/

#define PS2_SEND_ID      0xF2
#define PS2_ID_ERROR     -1
#define PS2_ID_PS2       0
#define PS2_ID_IMPS2     3

static int         ndev;
static const char *devlist[8];

typedef struct {
     int            fd;
     CoreInputDevice   *device;
     DirectThread  *thread;

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
          data->x_motion.flags = DIEF_AXISREL;
          dfb_input_dispatch( data->device, &data->x_motion );
          data->x_motion.axisrel = 0;
     }

     if (data->y_motion.axisrel) {
          data->y_motion.flags = DIEF_AXISREL;
          dfb_input_dispatch( data->device, &data->y_motion );
          data->y_motion.axisrel = 0;
     }

     if (data->z_motion.axisrel) {
          data->z_motion.flags = DIEF_AXISREL;
          dfb_input_dispatch( data->device, &data->z_motion );
          data->z_motion.axisrel = 0;
     }
}

static void*
ps2mouseEventThread( DirectThread *thread, void *driver_data )
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

          direct_thread_testcancel( thread );

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
                         i--;    /* does this make sense? oh well,
                                    it will resync eventually (will it ?)*/
                         continue;
                    }

                    buttons = packet[0] & 0x07;
                    dx = (packet[0] & 0x10) ?   packet[1]-256  :  packet[1];
                    dy = (packet[0] & 0x20) ? -(packet[2]-256) : -packet[2];
                    if (data->mouseId == PS2_ID_IMPS2) {
                         /* Just strip off the extra buttons if present
                            and sign extend the 4 bit value */
                         dz = (__s8)((packet[3] & 0x80) ?
                                     packet[3] | 0xf0 : packet[3] & 0x0F);
                    }
                    else {
                         dz = 0;
                    }
                    ps2mouse_motion_compress( data, dx, dy, dz );

                    if ( !dfb_config->mouse_motion_compression )
                         ps2mouse_motion_realize( data );

                    if ( last_buttons != buttons ) {
                         DFBInputEvent evt;
                         unsigned char changed_buttons;

                         changed_buttons = last_buttons ^ buttons;

                         /* make sure the compressed motion event is dispatched
                            before any button change */
                         ps2mouse_motion_realize( data );

                         if ( changed_buttons & 0x01 ) {
                              evt.type = (buttons & 0x01) ?
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_NONE;
                              evt.button = DIBI_LEFT;
                              dfb_input_dispatch( data->device, &evt );
                         }
                         if ( changed_buttons & 0x02 ) {
                              evt.type = (buttons & 0x02) ?
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_NONE;
                              evt.button = DIBI_RIGHT;
                              dfb_input_dispatch( data->device, &evt );
                         }
                         if ( changed_buttons & 0x04 ) {
                              evt.type = (buttons & 0x04) ?
                                   DIET_BUTTONPRESS : DIET_BUTTONRELEASE;
                              evt.flags = DIEF_NONE;
                              evt.button = DIBI_MIDDLE;
                              dfb_input_dispatch( data->device, &evt );
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
          D_PERROR ("psmouse thread died\n");

     return NULL;
}


static int
ps2WriteChar( int fd, unsigned char c, bool verbose )
{
     struct timeval tv;
     fd_set fds;

     tv.tv_sec = 0;
     tv.tv_usec = 200000;       /*  timeout 200 ms  */

     FD_ZERO( &fds );
     FD_SET( fd, &fds );

     write( fd, &c, 1 );

     if ( select(fd+1, &fds, NULL, NULL, &tv) == 0 ) {
          if ( verbose )
               D_ERROR( "DirectFB/PS2Mouse: timeout waiting for ack!!\n" );
          return -1;
     }

     read( fd, &c, 1 );

     if ( c != PS2_ACK )
          return -2;

     return 0;
}


static int
ps2GetId( int fd, bool verbose )
{
     unsigned char c;

     if ( ps2WriteChar(fd, PS2_SEND_ID, verbose) < 0 )
          return PS2_ID_ERROR;

     read( fd, &c, 1 );

     return( c );
}


static int
ps2Write( int fd, const unsigned char *data, size_t len, bool verbose)
{
     size_t i;
     int    error = 0;

     for ( i = 0; i < len; i++ ) {
          if ( ps2WriteChar(fd, data[i], verbose) < 0 ) {
               if ( verbose )
                    D_ERROR( "DirectFB/PS2Mouse: error @byte %i\n", i );
               error++;
          }
     }

     if ( error && verbose )
          D_ERROR( "DirectFB/PS2Mouse: missed %i ack's!\n", error);

     return( error );
}


static int
init_ps2( int fd, bool verbose )
{
     static const unsigned char basic_init[] =
     { PS2_ENABLE_DEV, PS2_SET_SAMPLE, 100 };
     static const unsigned char imps2_init[] =
     { PS2_SET_SAMPLE, 200, PS2_SET_SAMPLE, 100, PS2_SET_SAMPLE, 80 };
     static const unsigned char ps2_init[] =
     { PS2_SET_SCALE11, PS2_ENABLE_DEV, PS2_SET_SAMPLE, 100, PS2_SET_RES, 3 };
     int mouseId;

     struct timeval tv;
     fd_set fds;
     int count = 100;

     /* read all data from the file descriptor before initializing the mouse */
     while (true) {
          unsigned char buf[64];

          tv.tv_sec  = 0;
          tv.tv_usec = 50000;       /*  timeout 1/50 sec  */

          FD_ZERO( &fds );
          FD_SET( fd, &fds );

          if (select( fd+1, &fds, NULL, NULL, &tv ))
               read( fd, buf, sizeof(buf) );
          else
               break;

          if (! --count) {
               D_ERROR( "DirectFB/PS2Mouse: "
                         "PS/2 mouse keeps sending data, "
                         "initialization failed\n" );
               return -1;
          }
     }

     ps2Write( fd, basic_init, sizeof (basic_init), verbose );
     /* Do a basic init in case the mouse is confused */
     if (ps2Write( fd, basic_init, sizeof (basic_init), verbose ) != 0) {
          if (verbose)
               D_ERROR( "DirectFB/PS2Mouse: PS/2 mouse failed init\n" );
          return -1;
     }

     ps2Write( fd, ps2_init, sizeof (ps2_init), verbose );

     if (ps2Write(fd, imps2_init, sizeof (imps2_init), verbose) != 0) {
          if (verbose)
               D_ERROR ("DirectFB/PS2Mouse: mouse failed IMPS/2 init\n");
          return -2;
     }

     if ((mouseId = ps2GetId( fd, verbose )) < 0)
          return mouseId;

     if ( mouseId != PS2_ID_IMPS2 )   /*  unknown id, assume PS/2  */
          mouseId = PS2_ID_PS2;

     return mouseId;
}

/**************************************************************************************************/

static void
check_devices( const char *devnames[], int num )
{
     int i, fd;

     for (i=0; i<num; i++) {
          if ((fd = open( devnames[i], O_RDWR | O_SYNC )) < 0)
               continue;

          if (init_ps2( fd, false ) < 0) {
               close( fd );
               continue;
          }

          devlist[ndev++] = devnames[i];

          close( fd );

          break;
     }
}

#define CHECK_DEVICES(n)  check_devices( n, D_ARRAY_SIZE(n) )

/**************************************************************************************************/

static const char *dev_psaux[2] = { "/dev/psaux", "/dev/misc/psaux" };
static const char *dev_input[1] = { "/dev/input/mice" };

/**************************************************************************************************/

static int
driver_get_available()
{
     struct utsname uts;
     bool           check_psaux = true;
     bool           check_input = true;

     if (dfb_system_type() != CORE_FBDEV)
          return 0;

     if (dfb_config->mouse_gpm_source &&
         dfb_config->mouse_protocol   &&
        (strcasecmp( dfb_config->mouse_protocol, "PS/2")   == 0 ||
         strcasecmp( dfb_config->mouse_protocol, "IMPS/2") == 0))
     {
          int fd = open( dfb_config->mouse_source, O_RDONLY | O_NONBLOCK );

          if (fd < 0) {
               D_PERROR( "DirectFB/PS2Mouse: opening %s failed!\n",
                         dfb_config->mouse_source );
               return 0;
          }
          
          devlist[0] = dfb_config->mouse_source;
          close( fd );
          return 1;
     }

     if (uname( &uts ) < 0)
          D_PERROR( "DirectFB/PS2Mouse: uname() failed!\n" );
     else if (!strncmp( uts.release, "2.6.", 4 ) || !strncmp( uts.release, "2.5.", 4 ))
          check_psaux = false;

     if (check_psaux)
          CHECK_DEVICES( dev_psaux );

     if (check_input)
          CHECK_DEVICES( dev_input );

     return ndev;
}

static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "PS/2 Mouse Driver" );

     snprintf( info->vendor,
               DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH,
               "Convergence GmbH" );

     info->version.major = 1;
     info->version.minor = 0;
}

static DFBResult
driver_open_device( CoreInputDevice  *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     int           fd;
     int           mouseId = -1;
     int           flags;
     PS2MouseData *data;

     /* open device */

     flags = (dfb_config->mouse_gpm_source)
             ? (O_RDONLY | O_NONBLOCK)
             : (O_RDWR | O_SYNC | O_EXCL);
     
     fd = open( devlist[number], flags );
     if (fd < 0) {
          D_PERROR( "DirectFB/PS2Mouse: failed opening `%s' !\n",
                     devlist[number] );
          close( fd );
          return DFB_INIT;
     }

     fcntl( fd, F_SETFL, fcntl ( fd, F_GETFL ) & ~O_NONBLOCK );
 
     if (!dfb_config->mouse_gpm_source) {
          mouseId = init_ps2( fd, true );
          
          if (mouseId  < 0) {
               D_PERROR( "DirectFB/PS2Mouse: could not initialize mouse on `%s'!\n",
                          devlist[number] );
               close( fd );
               return DFB_INIT;
          }
     }

     if (dfb_config->mouse_protocol) {
          if (strcasecmp( dfb_config->mouse_protocol, "IMPS/2" ) == 0) {
               mouseId = PS2_ID_IMPS2;
          } 
          else if (strcasecmp( dfb_config->mouse_protocol, "PS/2" ) == 0) {
               mouseId = PS2_ID_PS2;
          } 
          else {
               D_ERROR( "DirectFB/PS2Mouse: unsupported protocol `%s' !\n",
                         dfb_config->mouse_protocol );
               close( fd );
               return DFB_INIT;
          }
     }

     /* fill device info structure */
     snprintf( info->desc.name, DFB_INPUT_DEVICE_DESC_NAME_LENGTH,
               (mouseId == PS2_ID_IMPS2) ? "IMPS/2 Mouse" : "PS/2 Mouse" );

     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "Unknown" );

     info->prefered_id     = DIDID_MOUSE;
     info->desc.type       = DIDTF_MOUSE;
     info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
     info->desc.max_axis   = (mouseId == PS2_ID_IMPS2) ? DIAI_Z : DIAI_Y;
     info->desc.max_button = DIBI_MIDDLE;     /* TODO: probe!? */

     /* allocate and fill private data */
     data = D_CALLOC( 1, sizeof(PS2MouseData) );

     data->fd           = fd;
     data->device       = device;
     data->mouseId      = mouseId;
     data->packetLength = (mouseId == PS2_ID_IMPS2) ? 4 : 3;

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, ps2mouseEventThread, data, "PS/2 Input" );

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
     PS2MouseData *data = (PS2MouseData*) driver_data;

     /* stop input thread */
     direct_thread_cancel( data->thread );
     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     /* close device */
     close( data->fd );

     /* free private data */
     D_FREE( data );
}
