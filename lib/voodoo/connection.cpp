/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

extern "C" {
#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/link.h>
}

#include <voodoo/connection.h>
#include <voodoo/manager.h>


D_DEBUG_DOMAIN( Voodoo_Connection, "Voodoo/Connection", "Voodoo Connection" );

/**********************************************************************************************************************/

VoodooConnection::VoodooConnection( VoodooLink *link )
     :
     magic(0),
     manager(NULL),
     link(link),
     delay(0),
     time_diff(0)
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnection::%s( %p )\n", __func__, this );

     D_MAGIC_SET( this, VoodooConnection );
}

VoodooConnection::~VoodooConnection()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnection::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooConnection );

     link->Close( link );

     D_MAGIC_CLEAR( this );
}

void
VoodooConnection::SetupTime()
{
     DirectResult           ret;
     VoodooResponseMessage *response;
     long long              t1, t2;

     D_DEBUG_AT( Voodoo_Connection, "VoodooConnection::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooConnection );


     t1 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

     ret = manager->do_request( manager->remote_time_service_id, 0, VREQ_RESPOND, &response );
     if (ret) {
          D_DERROR( ret, "Voodoo/Connection: Failed to call TimeService!\n" );
          return;
     }

     t2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );


     VoodooMessageParser parser;
     unsigned int        high, low;
     long long           micros;

     VOODOO_PARSER_BEGIN( parser, response );
     VOODOO_PARSER_GET_UINT( parser, high );
     VOODOO_PARSER_GET_UINT( parser, low );
     VOODOO_PARSER_END( parser );

     micros = (long long) (((unsigned long long) high << 32) | low);

     delay     = t2 - t1;
     time_diff = micros - t1 - delay / 2;

     D_INFO( "Voodoo/Connection: Delay %lld us, time difference to remote is %lld us\n", delay, time_diff );

     manager->finish_request( response );
}

