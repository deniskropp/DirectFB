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

#include <config.h>

#include <direct/clock.h>
#include <direct/debug.h>


D_LOG_DOMAIN( Direct_Clock, "Direct/Clock", "Time measurement etc." );

/**********************************************************************************************************************/

__no_instrument_function__
long long
direct_clock_get_micros( void )
{
     return direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
}

__no_instrument_function__
long long
direct_clock_get_millis( void )
{
     return direct_clock_get_time( DIRECT_CLOCK_MONOTONIC ) / 1000LL;
}

__no_instrument_function__
long long
direct_clock_get_abs_micros( void )
{
     return direct_clock_get_time( DIRECT_CLOCK_REALTIME );
}

__no_instrument_function__
long long
direct_clock_get_abs_millis( void )
{
     return direct_clock_get_time( DIRECT_CLOCK_REALTIME ) / 1000LL;
}

