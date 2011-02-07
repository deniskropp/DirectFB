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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

extern "C" {
#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/fastlz.h>
#include <direct/hash.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/conf.h>
#include <voodoo/internal.h>
#include <voodoo/link.h>
#include <voodoo/message.h>
}

#include <voodoo/connection.h>
#include <voodoo/manager.h>


D_DEBUG_DOMAIN( Voodoo_Connection, "Voodoo/Connection", "Voodoo Connection" );

/**********************************************************************************************************************/

VoodooConnection::VoodooConnection( VoodooManager *manager,
                                    VoodooLink    *link )
     :
     magic(0),
     manager(manager),
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

void
VoodooConnection::ProcessMessages( VoodooMessageHeader *first,
                                   size_t               total_length )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnection::%s( %p, first %p, total_length "_ZU" )\n",
                 __func__, this, first, total_length );

     D_MAGIC_ASSERT( this, VoodooConnection );

     VoodooMessageHeader *header = first;
     size_t               offset = 0;
     size_t               aligned;

     while (offset < total_length) {
          /* Get the message header. */
          header  = (VoodooMessageHeader *)((char*) first + offset);
          aligned = VOODOO_MSG_ALIGN( header->size );

          D_DEBUG_AT( Voodoo_Connection, "  -> Next message has %d ("_ZU") bytes and is of type %d... (offset "_ZU"/"_ZU")\n",
                      header->size, aligned, header->type, offset, total_length );

          D_ASSERT( header->size >= (int) sizeof(VoodooMessageHeader) );
          D_ASSERT( header->size <= MAX_MSG_SIZE );

          D_ASSERT( offset + aligned <= total_length );

          switch (header->type) {
               case VMSG_SUPER:
                    manager->handle_super( (VoodooSuperMessage*) header );
                    break;

               case VMSG_REQUEST:
                    manager->handle_request( (VoodooRequestMessage*) header );
                    break;

               case VMSG_RESPONSE:
                    manager->handle_response( (VoodooResponseMessage*) header );
                    break;

               default:
                    D_BUG( "invalid message type %d", header->type );
                    break;
          }

          offset += aligned;
     }

     D_ASSERT( offset == total_length );
}

