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

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <directfb.h>
#include <directfb_keynames.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/input.h>
#include <core/thread.h>

#include <misc/mem.h>

#include <core/input_driver.h>


DFB_INPUT_DRIVER( lirc )

static DirectFBKeySymbolNames(keynames);
static bool keynames_sorted = false;

typedef struct {
     InputDevice *device;
     CoreThread  *thread;

     int          fd;
} LircData;


static int keynames_compare (const void *key,
                             const void *base)
{
  return strcmp ((const char *) key,
                 ((const struct DFBKeySymbolName *) base)->name);
}

static int keynames_sort_compare (const void *d1,
                                  const void *d2)
{
  return strcmp (((const struct DFBKeySymbolName *) d1)->name,
                 ((const struct DFBKeySymbolName *) d2)->name);
}

static DFBInputDeviceKeySymbol lirc_parse_line(const char *line)
{
     struct DFBKeySymbolName *symbol_name;
     char *s, *name;

     if (!keynames_sorted) {
          qsort ( keynames,
                  sizeof(keynames) / sizeof(keynames[0]),
                  sizeof(keynames[0]),
                  keynames_sort_compare );
          keynames_sorted = true;
     }

     s = strchr( line, ' ' );
     if (!s || !s[1])
          return DIKS_NULL;

     s = strchr( ++s, ' ' );
     if (!s|| !s[1])
          return DIKS_NULL;

     name = ++s;

     s = strchr( name, ' ' );
     if (s)
          *s = '\0';

     switch (strlen( name )) {
          case 0:
               return DIKS_NULL;
          case 1:
               return (DFBInputDeviceKeySymbol) name[0];
          default:
               symbol_name = bsearch( name, keynames,
                                      sizeof(keynames)/sizeof(keynames[0]),
                                      sizeof(keynames[0]), keynames_compare );
               if (symbol_name)
                    return symbol_name->symbol;
               break;
     }

     return DIKS_NULL;
}

static void*
lircEventThread( CoreThread *thread, void *driver_data )
{
     LircData      *data  = (LircData*) driver_data;
     int            readlen;
     char           buf[128];
     DFBInputEvent  evt;

     memset( &evt, 0, sizeof(DFBInputEvent) );

     while ((readlen = read( data->fd, buf, 128 )) > 0 || errno == EINTR) {
          dfb_thread_testcancel( thread );

          if (readlen < 1)
               continue;

          evt.key_symbol = lirc_parse_line( buf );

          if (evt.key_symbol != DIKS_NULL) {
               evt.type = DIET_KEYPRESS;
               evt.flags = DIEF_KEYSYMBOL;
               dfb_input_dispatch( data->device, &evt );

               evt.type = DIET_KEYRELEASE;
               evt.flags = DIEF_KEYSYMBOL;
               dfb_input_dispatch( data->device, &evt );
          }
     }

     if (readlen <= 0 && errno != EINTR)
          PERRORMSG ("lirc thread died\n");

     return NULL;
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
     int fd;
     struct sockaddr_un addr;

     addr.sun_family = AF_UNIX;
     strcpy( addr.sun_path, "/dev/lircd" );

     fd = socket( PF_UNIX, SOCK_STREAM, 0 );
     if (fd < 0)
          return 0;

     if (connect( fd, (struct sockaddr*)&addr, sizeof(addr) ) < 0) {
          close( fd );
          return 0;
     }

     close( fd );

     return 1;
}

static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "LIRC Driver" );

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
     int                 fd;
     LircData           *data;
     struct sockaddr_un  sa;

     /* create socket */
     sa.sun_family = AF_UNIX;
     strcpy( sa.sun_path, "/dev/lircd" );

     fd = socket( PF_UNIX, SOCK_STREAM, 0 );
     if (fd < 0) {
          PERRORMSG( "DirectFB/LIRC: socket" );
          return DFB_INIT;
     }

     /* initiate connection */
     if (connect( fd, (struct sockaddr*)&sa, sizeof(sa) ) < 0) {
          PERRORMSG( "DirectFB/LIRC: connect" );
          close( fd );
          return DFB_INIT;
     }

     /* fill driver info structure */
     snprintf( info->name,
               DFB_INPUT_DEVICE_INFO_NAME_LENGTH, "LIRC Device" );

     snprintf( info->vendor,
               DFB_INPUT_DEVICE_INFO_VENDOR_LENGTH, "Unknown" );

     info->prefered_id = DIDID_REMOTE;

     info->desc.type   = DIDTF_REMOTE;
     info->desc.caps   = DICAPS_KEYS;

     /* allocate and fill private data */
     data = DFBCALLOC( 1, sizeof(LircData) );

     data->fd     = fd;
     data->device = device;

     /* start input thread */
     data->thread = dfb_thread_create( CTT_INPUT, lircEventThread, data );

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
     LircData *data = (LircData*) driver_data;

     /* stop input thread */
     dfb_thread_cancel( data->thread );
     dfb_thread_join( data->thread );
     dfb_thread_destroy( data->thread );

     /* close socket */
     close( data->fd );

     /* free private data */
     DFBFREE( data );
}

