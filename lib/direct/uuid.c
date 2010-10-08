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

#include <direct/debug.h>
#include <direct/uuid.h>


D_LOG_DOMAIN( Direct_UUID, "Direct/UUID", "Direct UUID" );

/**********************************************************************************************************************/

// FIXME: FAKE UUID IMPLEMENTATION!

static volatile unsigned long long uuid_pool = 18446744073709551557ULL;

void
direct_uuid_generate( DirectUUID *ret_id )
{
     D_DEBUG_AT( Direct_UUID, "%s( %p )\n", __FUNCTION__, ret_id );

     D_ASSERT( ret_id != NULL );

     ret_id->___u64_x2[0] = uuid_pool;
     ret_id->___u64_x2[1] = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

     uuid_pool = ret_id->___u64_x2[1] + (uuid_pool << 7);

     D_DEBUG_AT( Direct_UUID, "  ** =-=> [ "D_UUID_FORMAT" ] <=-= **\n", D_UUID_VALS( ret_id ) );
}

