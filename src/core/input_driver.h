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

#ifndef __INPUT_DRIVER_H__
#define __INPUT_DRIVER_H__

#include <core/input.h>


static int
driver_get_abi_version();

static int
driver_get_available();

static void
driver_get_info( InputDriverInfo *info );

static DFBResult
driver_open_device( InputDevice      *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data );

static DFBResult
driver_get_keymap_entry( InputDevice               *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry );

static void
driver_close_device( void *driver_data );

static InputDriverFuncs driver_funcs = {
     GetAbiVersion:      driver_get_abi_version,
     GetAvailable:       driver_get_available,
     GetDriverInfo:      driver_get_info,
     OpenDevice:         driver_open_device,
     GetKeymapEntry:     driver_get_keymap_entry,
     CloseDevice:        driver_close_device
};

#define DFB_INPUT_DRIVER(shortname)               \
__attribute__((constructor))                      \
void                                              \
directfb_##shortname (void)                       \
{                                                 \
     dfb_input_register_module( &driver_funcs );  \
}

#endif
