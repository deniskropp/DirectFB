/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <direct/atomic.h>
#include <direct/debug.h>
#include <direct/serial.h>
#include <direct/system.h>
#include <direct/util.h>


D_LOG_DOMAIN( Direct_Serial_Notify, "Direct/Serial/Notify", "Direct Serial Notify" );
D_LOG_DOMAIN( Direct_Serial_Wait,   "Direct/Serial/Wait",   "Direct Serial Wait" );

/**********************************************************************************************************************/

DirectResult
direct_serial_wait( DirectSerial       *serial,
                    const DirectSerial *source )
{
     DirectResult ret = DR_OK;
     u32          value;

     D_ASSERT( serial != NULL );
     D_ASSERT( source != NULL );

     D_DEBUG_AT( Direct_Serial_Wait, " ## ## ## %s( %p '%s', %p '%s' ) ## ## %lu ## %lu ## ## ## ##\n", __FUNCTION__,
                 serial, serial->name, source, source->name, serial->value, source->value );

     D_MAGIC_ASSERT( serial, DirectSerial );
     D_MAGIC_ASSERT( source, DirectSerial );

     D_SYNC_ADD( &serial->waiting, 1 );

     do {
          int wakeup = serial->wakeup;

          D_ASSERT( serial->overflow == source->overflow || serial->value != source->value );

          value = serial->value;

          if (direct_serial_check( serial, source ))
               break;
               
          ret = direct_futex_wait( &serial->wakeup, wakeup );
     } while (ret == DR_OK);

     D_SYNC_ADD( &serial->waiting, -1 );

     return ret;
}

DirectResult
direct_serial_notify( DirectSerial *serial, const DirectSerial *source )
{
     D_ASSERT( serial != NULL );
     D_ASSERT( source != NULL );

     D_DEBUG_AT( Direct_Serial_Notify, " ###### %s( %p '%s', %p '%s' ) ### %lu <-= %lu ### ### ###\n", __FUNCTION__,
                 serial, serial->name, source, source->name, serial->value, source->value );

     D_MAGIC_ASSERT( serial, DirectSerial );
     D_MAGIC_ASSERT( source, DirectSerial );

     D_ASSERT( serial->overflow <= source->overflow );
     D_ASSERT( serial->overflow == source->overflow || serial->value != source->value );

     if (serial->overflow < source->overflow) {
          serial->overflow = source->overflow;
          serial->value    = source->value;
     }
     else if (serial->overflow == source->overflow && serial->value < source->value)
          serial->value = source->value;
     else {
          D_ASSUME( serial->value == source->value );

          return DR_OK;
     }

     if (serial->waiting > 0) {
          D_SYNC_ADD( &serial->wakeup, 1 );

          return direct_futex_wake( &serial->wakeup, 1024 );
     }

     return DR_OK;
}

