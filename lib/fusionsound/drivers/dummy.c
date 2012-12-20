/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusionsound.h>

#include <core/sound_device.h>
#include <core/sound_driver.h>

#include <misc/sound_conf.h>


FS_SOUND_DRIVER( dummy )

/******************************************************************************/

static DirectResult
device_probe( void )
{
     /* load only when requested */
     if (strcmp( fs_config->driver, "dummy" ))
          return DR_UNSUPPORTED;

     return DR_OK;
}

static void
device_get_driver_info( SoundDriverInfo *info )
{
     snprintf( info->name,
               FS_SOUND_DRIVER_INFO_NAME_LENGTH,
               "Dummy" );

     snprintf( info->vendor,
               FS_SOUND_DRIVER_INFO_VENDOR_LENGTH,
               "directfb.org" );

     snprintf( info->url,
               FS_SOUND_DRIVER_INFO_URL_LENGTH,
               "http://www.directfb.org" );

     snprintf( info->license,
               FS_SOUND_DRIVER_INFO_LICENSE_LENGTH,
               "LGPL" );

     info->version.major = 1;
     info->version.minor = 0;

     info->device_data_size = 16384;
}

static DirectResult
device_open( void                  *device_data,
             SoundDeviceInfo       *device_info,
             CoreSoundDeviceConfig *config )
{
     /* device name */
     snprintf( device_info->name,
               FS_SOUND_DEVICE_INFO_NAME_LENGTH,
               "dummy" );

     /* device capabilities */
     device_info->caps = 0;

     return DR_OK;
}

static DirectResult
device_get_buffer( void *device_data, u8 **addr, unsigned int *avail )
{
     *addr  = device_data;
     *avail = 16384;
     
     return DR_OK;
}

static DirectResult
device_commit_buffer( void *device_data, unsigned int frames )
{
     return DR_OK;
}

static void
device_get_output_delay( void *device_data, int *delay )
{
     *delay = 0;
}

static DirectResult
device_get_volume( void *device_data, float *level )
{
     return DR_UNSUPPORTED;
}

static DirectResult
device_set_volume( void *device_data, float level )
{
     return DR_UNSUPPORTED;
}

static DirectResult
device_suspend( void *device_data )
{
     return DR_OK;
}

static DirectResult
device_resume( void *device_data )
{
     return DR_OK;
}

static void
device_handle_fork( void             *device_data,
                    FusionForkAction  action,
                    FusionForkState   state )
{
}

static void
device_close( void *device_data )
{
}

