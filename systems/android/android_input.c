/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <core/system.h>
#include <core/input_driver.h>

#include "android_system.h"

DFB_INPUT_DRIVER( android )

/**************************************************************************************************/

static int
driver_get_available( void )
{
     return dfb_system_type() == CORE_ANDROID;
}

static void
driver_get_info( InputDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name, DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "Android Input Driver" );
     snprintf( info->vendor, DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "directfb.org" );

     info->version.major = 1;
     info->version.minor = 0;
}

static DFBResult
driver_open_device( CoreInputDevice  *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     AndroidData *system = dfb_system_data();
     if (!system)
          return DFB_INIT;

     system->input = device;

     /* fill device info structure */
     snprintf( info->desc.name, DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "Android Input" );
     snprintf( info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "Android" );

     /* set one of the primary input device IDs */
     info->prefered_id = DIDID_KEYBOARD;

     /* set type flags */
     info->desc.type   = DIDTF_KEYBOARD | DIDTF_MOUSE;

     /* set capabilities */
     info->desc.caps   = DICAPS_ALL;

     /* set private data pointer */
     *driver_data = NULL;

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
}
