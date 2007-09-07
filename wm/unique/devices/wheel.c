/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
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


typedef struct {
     int magic;
} WheelData;

/**************************************************************************************************/

static DFBResult
wheel_initialize( UniqueDevice    *device,
                  void            *data,
                  void            *ctx )
{
     WheelData *wheel = data;

     D_MAGIC_ASSERT( device, UniqueDevice );

     D_DEBUG_AT( UniQuE_Wheel, "wheel_initialize( %p, %p, %p )\n", device, data, ctx );

     D_MAGIC_SET( wheel, WheelData );

     return DFB_OK;
}

static void
wheel_shutdown( UniqueDevice    *device,
                void            *data,
                void            *ctx )
{
     WheelData *wheel = data;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( wheel, WheelData );

     D_DEBUG_AT( UniQuE_Wheel, "wheel_shutdown( %p, %p, %p )\n", device, data, ctx );

     D_MAGIC_CLEAR( wheel );
}

static void
wheel_connected( UniqueDevice        *device,
                 void                *data,
                 void                *ctx,
                 CoreInputDevice     *source )
{
     WheelData *wheel = data;

     (void) wheel;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( wheel, WheelData );

     D_ASSERT( source != NULL );

     D_DEBUG_AT( UniQuE_Wheel, "wheel_connected( %p, %p, %p, %p )\n",
                 device, data, ctx, source );
}

static void
wheel_disconnected( UniqueDevice        *device,
                    void                *data,
                    void                *ctx,
                    CoreInputDevice     *source )
{
     WheelData *wheel = data;

     (void) wheel;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( wheel, WheelData );

     D_ASSERT( source != NULL );

     D_DEBUG_AT( UniQuE_Wheel, "wheel_disconnected( %p, %p, %p, %p )\n",
                 device, data, ctx, source );
}

static void
wheel_process_event( UniqueDevice        *device,
                     void                *data,
                     void                *ctx,
                     const DFBInputEvent *event )
{
     UniqueInputEvent  evt;
     WheelData        *wheel = data;

     (void) wheel;

     D_MAGIC_ASSERT( device, UniqueDevice );
     D_MAGIC_ASSERT( wheel, WheelData );

     D_ASSERT( event != NULL );

     D_DEBUG_AT( UniQuE_Wheel, "wheel_process_event( %p, %p, %p, %p ) <- type 0x%08x\n",
                 device, data, ctx, event, event->type );

     switch (event->type) {
          case DIET_AXISMOTION:
               switch (event->axis) {
                    case DIAI_Z:
                         evt.type = UIET_WHEEL;

                         evt.wheel.device_id = event->device_id;

                         if (event->flags & DIEF_AXISREL)
                              evt.wheel.value = -event->axisrel;
                         else if (event->flags & DIEF_AXISABS)
                              evt.wheel.value = event->axisabs;
                         else
                              break;

                         unique_device_dispatch( device, &evt );
                         break;

                    default:
                         break;
               }
               break;

          default:
               break;
     }
}


const UniqueDeviceClass unique_wheel_device_class = {
     data_size:     sizeof(WheelData),

     Initialize:    wheel_initialize,
     Shutdown:      wheel_shutdown,
     Connected:     wheel_connected,
     Disconnected:  wheel_disconnected,
     ProcessEvent:  wheel_process_event
};

