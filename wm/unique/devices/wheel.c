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

#include <config.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/gfxcard.h>
#include <core/state.h>

#include <misc/util.h>

#include <unique/context.h>
#include <unique/device.h>
#include <unique/internal.h>


D_DEBUG_DOMAIN( UniQuE_Wheel, "UniQuE/Wheel", "UniQuE's Wheel Device Class" );



static void
wheel_connected( UniqueDevice        *device,
                 void                *data,
                 unsigned long        arg,
                 CoreInputDevice     *source )
{
     D_MAGIC_ASSERT( device, UniqueDevice );

     D_DEBUG_AT( UniQuE_Wheel, "wheel_connected( %p, %p, %lu, %p )\n",
                 device, data, arg, source );
}

static void
wheel_disconnected( UniqueDevice        *device,
                    void                *data,
                    unsigned long        arg,
                    CoreInputDevice     *source )
{
     D_MAGIC_ASSERT( device, UniqueDevice );

     D_DEBUG_AT( UniQuE_Wheel, "wheel_disconnected( %p, %p, %lu, %p )\n",
                 device, data, arg, source );
}

static void
wheel_process_event( UniqueDevice        *device,
                     void                *data,
                     unsigned long        arg,
                     const DFBInputEvent *event )
{
     D_MAGIC_ASSERT( device, UniqueDevice );

     D_DEBUG_AT( UniQuE_Wheel, "wheel_process_event( %p, %p, %lu, %p ) <- type 0x%08x\n",
                 device, data, arg, event, event->type );
}


const UniqueDeviceClass unique_wheel_device_class = {
     Connected:     wheel_connected,
     Disconnected:  wheel_disconnected,
     ProcessEvent:  wheel_process_event
};

