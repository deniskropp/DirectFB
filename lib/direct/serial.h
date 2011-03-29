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

#ifndef __DIRECT__SERIAL_H__
#define __DIRECT__SERIAL_H__

#include <direct/atomic.h>
#include <direct/debug.h>
#include <direct/thread.h>

struct __D_DirectSerial {
     int            magic;

     u32            overflow;

     unsigned long  value;

     int  waiting;
     int  wakeup;
};


#if D_DEBUG_ENABLED
#define DIRECT_SERIAL_ASSERT( serial )                                     \
     do {                                                                  \
          D_MAGIC_ASSERT( serial, DirectSerial );                          \
     } while (0)
#else
#define DIRECT_SERIAL_ASSERT( serial )                                     \
     do {                                                                  \
     } while (0)
#endif


D_LOG_DOMAIN( Direct_Serial, "Direct/Serial", "Direct Serial" );

static __inline__ void
direct_serial_initialize( DirectSerial *serial, const char *name )
{
     D_DEBUG_AT( Direct_Serial, "%s( %p '%s' ) <<\n", __FUNCTION__, (void*) serial, name );

     D_ASSERT( serial != NULL );

     serial->value    = 0;
     serial->overflow = 0;
     serial->waiting  = 0;

     D_MAGIC_SET( serial, DirectSerial );
}

// @deprecated
static __inline__ void
direct_serial_init( DirectSerial *serial )
{
     D_DEBUG_AT( Direct_Serial, "%s( %p ) <<\n", __FUNCTION__, (void*) serial );

     D_ASSERT( serial != NULL );

     direct_serial_initialize( serial, "unnamed" );
}

static __inline__ void
direct_serial_init_from( DirectSerial *serial, const DirectSerial *source )
{
     D_ASSERT( source != NULL );

     D_DEBUG_AT( Direct_Serial, "%s( %p, %p ) <<-- <%lu>\n", __FUNCTION__, (void*) serial, (void*) source, source->value );

     D_ASSERT( serial != NULL );
     D_MAGIC_ASSERT( source, DirectSerial );

     serial->value    = source->value;
     serial->overflow = source->overflow;
     serial->waiting  = 0;

     D_MAGIC_SET( serial, DirectSerial );
}

static __inline__ void
direct_serial_init_from_counting( DirectSerial *serial, DirectSerial *counter )
{
     unsigned long value;

     D_ASSERT( serial != NULL );
     D_MAGIC_ASSERT( counter, DirectSerial );

     value = D_SYNC_ADD_AND_FETCH( &counter->value, 1 );

     D_DEBUG_AT( Direct_Serial, "%s( %p, %p ) <<-- <%lu>\n", __FUNCTION__, (void*) serial, (void*) counter, value );

     serial->value    = value;
     serial->overflow = counter->overflow;
     serial->waiting  = 0;

     D_MAGIC_SET( serial, DirectSerial );
}

static __inline__ void
direct_serial_deinit( DirectSerial *serial )
{
     D_ASSERT( serial != NULL );

     D_DEBUG_AT( Direct_Serial, "%s( %p ) <- (%lu)\n", __FUNCTION__, (void*) serial, serial->value );

     D_MAGIC_ASSERT( serial, DirectSerial );

     D_ASSUME( serial->waiting == 0 );

     D_MAGIC_CLEAR( serial );
}

static __inline__ void
direct_serial_increase( DirectSerial *serial )
{
     D_ASSERT( serial != NULL );

     D_DEBUG_AT( Direct_Serial, "%s( %p ) <- %lu ++\n", __FUNCTION__, (void*) serial, serial->value );

     D_MAGIC_ASSERT( serial, DirectSerial );

     if (! ++serial->value)
          serial->overflow++;

     D_DEBUG_AT( Direct_Serial, "  -> %lu\n", serial->value );
}

static __inline__ void
direct_serial_copy( DirectSerial *serial, const DirectSerial *source )
{
     D_ASSERT( serial != NULL );
     D_ASSERT( source != NULL );

     D_DEBUG_AT( Direct_Serial, "%s( %p, %p ) <- %lu = (%lu)\n", __FUNCTION__,
                 (void*) serial, (void*) source, source->value, serial->value );

     D_MAGIC_ASSERT( serial, DirectSerial );
     D_MAGIC_ASSERT( source, DirectSerial );

     serial->value    = source->value;
     serial->overflow = source->overflow;
}

static __inline__ bool
direct_serial_check( const DirectSerial *serial, const DirectSerial *source )
{
     D_ASSERT( serial != NULL );
     D_ASSERT( source != NULL );

     D_DEBUG_AT( Direct_Serial, "%s( %p, %p ) -- %lu == %lu\n", __FUNCTION__,
                 (void*) serial, (void*) source, serial->value, source->value );

     D_MAGIC_ASSERT( serial, DirectSerial );
     D_MAGIC_ASSERT( source, DirectSerial );

     if (serial->overflow < source->overflow)
          return false;
     else if (serial->overflow == source->overflow && serial->value < source->value)
          return false;

//     D_ASSUME( serial->value == source->value );

     return true;
}

static __inline__ bool
direct_serial_update( DirectSerial *serial, const DirectSerial *source )
{
     D_ASSERT( serial != NULL );
     D_ASSERT( source != NULL );

     D_DEBUG_AT( Direct_Serial, "%s( %p, %p ) <- %lu <-= %lu\n", __FUNCTION__,
                 (void*) serial, (void*) source, serial->value, source->value );

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

DirectResult DIRECT_API direct_serial_wait  ( DirectSerial       *serial,
                                              const DirectSerial *source );
DirectResult DIRECT_API direct_serial_notify( DirectSerial       *serial,
                                              const DirectSerial *source );


static __inline__ int
direct_serial_diff( const DirectSerial *a, const DirectSerial *b )
{
     int ret;

     D_ASSERT( a != NULL );
     D_ASSERT( b != NULL );

     D_DEBUG_AT( Direct_Serial, "%s( %p, %p ) <- %lu - %lu\n", __FUNCTION__,
                 (void*) a, (void*) b, a->value, b->value );

     D_MAGIC_ASSERT( a, DirectSerial );
     D_MAGIC_ASSERT( b, DirectSerial );

     if (a->overflow > b->overflow)
          ret = INT_MAX;
     else if (b->overflow > a->overflow)
          ret = INT_MIN;
     else
          ret = (int) a->value - (int) b->value;

     D_DEBUG_AT( Direct_Serial, "  -> %d\n", ret );

     return ret;
}

#endif

