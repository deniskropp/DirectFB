/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECT__SERIAL_H__
#define __DIRECT__SERIAL_H__

#include <direct/types.h>
#include <direct/debug.h>

struct __D_DirectSerial {
     int   magic;

     u32 value;
     u32 overflow;
};

static __inline__ void
direct_serial_init( DirectSerial *serial )
{
     D_ASSERT( serial != NULL );

     serial->value    = 0;
     serial->overflow = 0;

     D_MAGIC_SET( serial, DirectSerial );
}

static __inline__ void
direct_serial_deinit( DirectSerial *serial )
{
     D_MAGIC_CLEAR( serial );
}

static __inline__ void
direct_serial_increase( DirectSerial *serial )
{
     D_MAGIC_ASSERT( serial, DirectSerial );

     if (! ++serial->value)
          serial->overflow++;
}

static __inline__ void
direct_serial_copy( DirectSerial *serial, const DirectSerial *source )
{
     D_MAGIC_ASSERT( serial, DirectSerial );
     D_MAGIC_ASSERT( source, DirectSerial );

     serial->value    = source->value;
     serial->overflow = source->overflow;
}

static __inline__ bool
direct_serial_check( DirectSerial *serial, const DirectSerial *source )
{
     D_MAGIC_ASSERT( serial, DirectSerial );
     D_MAGIC_ASSERT( source, DirectSerial );

     if (serial->overflow < source->overflow)
          return false;
     else if (serial->overflow == source->overflow && serial->value < source->value)
          return false;

     D_ASSUME( serial->value == source->value );

     return true;
}

static __inline__ bool
direct_serial_update( DirectSerial *serial, const DirectSerial *source )
{
     D_MAGIC_ASSERT( serial, DirectSerial );
     D_MAGIC_ASSERT( source, DirectSerial );

     if (serial->overflow < source->overflow) {
          serial->overflow = source->overflow;
          serial->value    = source->value;

          return true;
     }
     else if (serial->overflow == source->overflow && serial->value < source->value) {
          serial->value = source->value;

          return true;
     }

     D_ASSUME( serial->value == source->value );

     return false;
}

#endif

