/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

extern "C" {
#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/link.h>
}

#include <voodoo/connection.h>


D_DEBUG_DOMAIN( Voodoo_Connection, "Voodoo/Connection", "Voodoo Connection" );

/**********************************************************************************************************************/

VoodooConnection::VoodooConnection( VoodooLink *link )
     :
     magic(0),
     manager(NULL),
     link(link)
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

