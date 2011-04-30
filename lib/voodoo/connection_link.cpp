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
}

#include <voodoo/connection_link.h>
#include <voodoo/manager.h>
#include <voodoo/packet.h>


#include <vector>


//namespace Voodoo {

D_DEBUG_DOMAIN( Voodoo_Connection, "Voodoo/Connection", "Voodoo Connection" );
D_DEBUG_DOMAIN( Voodoo_Input,      "Voodoo/Input",      "Voodoo Input" );
D_DEBUG_DOMAIN( Voodoo_Output,     "Voodoo/Output",     "Voodoo Output" );

/**********************************************************************************************************************/

VoodooConnectionLink::VoodooConnectionLink( VoodooManager *manager,
                                            VoodooLink    *link )
     :
     VoodooConnection( manager, link )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionLink::%s( %p )\n", __func__, this );

     input.start   = 0;
     input.last    = 0;
     input.end     = 0;
     input.max     = 0;

     output.packets = NULL;
     output.sending = NULL;

     /* Initialize all locks. */
     direct_mutex_init( &output.lock );

     /* Initialize all wait conditions. */
     direct_waitqueue_init( &output.wait );

     /* Set default buffer limit. */
     input.max = VOODOO_CONNECTION_LINK_INPUT_BUF_MAX;

     /* Allocate buffers. */
     size_t input_buffer_size = VOODOO_CONNECTION_LINK_INPUT_BUF_MAX + VOODOO_PACKET_MAX + sizeof(VoodooPacketHeader);

     input.buffer = (u8*) D_MALLOC( input_buffer_size );

     D_INFO( "VoodooConnection/Link: Allocated "_ZU" kB input buffer at %p\n", input_buffer_size/1024, input.buffer );

     direct_tls_register( &output.tls, OutputTLS_Destructor );
}

VoodooConnectionLink::~VoodooConnectionLink()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionLink::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooConnection );

     /* Acquire locks and wake up waiters. */
     direct_mutex_lock( &output.lock );
     direct_waitqueue_broadcast( &output.wait );
     direct_mutex_unlock( &output.lock );

     /* Destroy conditions. */
     direct_waitqueue_deinit( &output.wait );

     /* Destroy locks. */
     direct_mutex_deinit( &output.lock );

     /* Deallocate buffers. */
     D_FREE( input.buffer );

     direct_tls_unregister( &output.tls );
}

/**********************************************************************************************************************/

VoodooPacket *
VoodooConnectionLink::GetPacket( size_t length )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionLink::%s( %p, length "_ZU" )\n", __func__, this, length );

     D_MAGIC_ASSERT( this, VoodooConnection );
     D_ASSERT( length >= (int) sizeof(VoodooMessageHeader) );
     D_ASSUME( length <= MAX_MSG_SIZE );

     if (length > MAX_MSG_SIZE) {
          D_WARN( _ZU" exceeds maximum message size of %d", length, MAX_MSG_SIZE );
          return NULL;
     }

     size_t aligned = VOODOO_MSG_ALIGN( length );


     Packets *packets = (Packets*) direct_tls_get( output.tls );

     if (!packets) {
          packets = new Packets();

          direct_tls_set( output.tls, packets );
     }

     VoodooPacket *packet = packets->active;

     if (packet) {
          if (packet->append( aligned ))
               return packet;

          Flush( packet );
     }

     packet = packets->Get();
     if (packet) {
          if (packet->sending) {
               direct_mutex_lock( &output.lock );

               while (packet->sending)
                    direct_waitqueue_wait( &output.wait, &output.lock );

               direct_mutex_unlock( &output.lock );
          }
          packet->reset( aligned );
     }

     return packet;
}

void
VoodooConnectionLink::PutPacket( VoodooPacket *packet, bool flush )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionLink::%s( %p, %sflush )\n", __func__, this, flush ? "" : "NO " );

     D_MAGIC_ASSERT( this, VoodooConnection );

     Packets *packets = (Packets*) direct_tls_get( output.tls );

     D_ASSERT( packets != NULL );
     D_ASSERT( packet == packets->active );

     if (flush) {
          Flush( packet );

          packets->active = NULL;
     }
}

void
VoodooConnectionLink::Flush( VoodooPacket *packet )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionLink::%s( %p, packet %p )\n", __func__, this, packet );

     D_MAGIC_ASSERT( this, VoodooConnection );

     direct_mutex_lock( &output.lock );

     D_ASSERT( !direct_list_contains_element_EXPENSIVE( output.packets, &packet->link ) );

     D_ASSERT( !packet->sending );

     packet->sending = true;

     direct_list_append( &output.packets, &packet->link );

     direct_mutex_unlock( &output.lock );

     link->WakeUp( link );
}

void
VoodooConnectionLink::OutputTLS_Destructor( void *ptr )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionLink::%s( ptr %p )\n", __func__, ptr );

     Packets *packets = (Packets*) ptr;

     delete packets;
}

VoodooConnectionLink::Packets::Packets()
     :
     magic(0),
     next(0),
     num(0),
     active(NULL)
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionLink::Packets::%s( %p )\n", __func__, this );

     memset( packets, 0, sizeof(packets) );

     D_MAGIC_SET( this, Packets );
}

VoodooConnectionLink::Packets::~Packets()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionLink::Packets::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, Packets );

     for (size_t i=0; i<num; i++) {
          if (packets[i]) {
               D_ASSUME( !packets[i]->sending );

               if (!packets[i]->sending)
                    D_FREE( packets[i] );
          }
     }
}

VoodooPacket *
VoodooConnectionLink::Packets::Get()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionLink::Packets::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, Packets );

     VoodooPacket *packet;

     if (num < VOODOO_CONNECTION_PACKET_NUM_OUTPUT) {
          packet = packets[num] = VoodooPacket::New( 0 );

          D_DEBUG_AT( Voodoo_Connection, "  -> new ["_ZU"] %p\n", num, packet );

          num++;
     }
     else {
          packet = packets[next];

          next = (next+1) % VOODOO_CONNECTION_PACKET_NUM_OUTPUT;

          D_DEBUG_AT( Voodoo_Connection, "  -> reusing %p\n", packet );
     }

     active = packet;

     return packet;
}

