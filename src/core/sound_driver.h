/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2006  convergence GmbH.

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

#ifndef __FUSIONSOUND_CORE_SOUND_DRIVER_H__
#define __FUSIONDOUND_CORE_SOUND_DRIVER_H__

#include <direct/modules.h>

#include <core/sound_device.h>


static DFBResult
device_probe( void );

static void
device_get_driver_info( SoundDriverInfo *info );

static DFBResult
device_open( void                  *device_data,
             CoreSoundDeviceConfig *config );

static void
device_write( void         *device_data, 
              void         *samples, 
              unsigned int  size );

static void
device_get_output_delay( void *device_data,
                         int  *delay );

static void
device_close( void *device_data );
                  

static const SoundDriverFuncs driver_funcs = {
     Probe:              device_probe,
     GetDriverInfo:      device_get_driver_info,
     OpenDevice:         device_open,
     Write:              device_write,
     GetOutputDelay:     device_get_output_delay,
     CloseDevice:        device_close,
};


#define FS_SOUND_DRIVER(shortname)                               \
__attribute__((constructor)) void fusionsound_##shortname();     \
                                                                 \
void                                                             \
fusionsound_##shortname()                                        \
{                                                                \
     direct_modules_register( &fs_sound_drivers,                 \
                              FS_SOUND_DRIVER_ABI_VERSION,       \
                              #shortname, &driver_funcs );       \
}

#endif

                  
