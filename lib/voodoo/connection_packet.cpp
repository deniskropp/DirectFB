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
#include <direct/fastlz.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/conf.h>
#include <voodoo/link.h>
}

#include <voodoo/connection_packet.h>
#include <voodoo/manager.h>
#include <voodoo/packet.h>


#include <vector>


//namespace Voodoo {

D_DEBUG_DOMAIN( Voodoo_Connection, "Voodoo/Connection", "Voodoo Connection" );
D_DEBUG_DOMAIN( Voodoo_Input,      "Voodoo/Input",      "Voodoo Input" );
D_DEBUG_DOMAIN( Voodoo_Output,     "Voodoo/Output",     "Voodoo Output" );

/**********************************************************************************************************************/

VoodooConnectionPacket::VoodooConnectionPacket( VoodooManager *manager,
                                                VoodooLink    *link )
     :
     VoodooConnectionLink( manager, link ),
     stop( false ),
     closed( false )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p )\n", __func__, this );
}

VoodooConnectionPacket::~VoodooConnectionPacket()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooConnection );
}

void
VoodooConnectionPacket::Start()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooConnection );

     io = direct_thread_create( DTT_DEFAULT, io_loop_main, this, "Voodoo IO" );
}

void
VoodooConnectionPacket::Stop()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooConnection );

     direct_mutex_lock( &output.lock );

     while (!closed && output.packets) {
          D_DEBUG_AT( Voodoo_Connection, "  -> waiting for output packets to be sent...\n" );

          direct_waitqueue_wait( &output.wait, &output.lock );
     }

     direct_mutex_unlock( &output.lock );

     stop = true;

     link->WakeUp( link );

     /* Wait for manager threads exiting. */
     direct_thread_join( io );
     direct_thread_destroy( io );

     VoodooConnectionLink::Stop();
}

/**********************************************************************************************************************/

void *
VoodooConnectionPacket::io_loop()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p )\n", __func__, this );

     while (!stop) {
          D_MAGIC_ASSERT( this, VoodooConnection );

          if (input.start == input.max) {
               input.start = 0;
               input.end   = 0;
               input.last  = 0;
               input.max   = VOODOO_CONNECTION_LINK_INPUT_BUF_MAX;
          }

          if (!stop) {
               DirectResult  ret;
               VoodooChunk   chunks[2];
               VoodooChunk  *chunk_read  = NULL;
               VoodooChunk  *chunk_write = NULL;
               size_t        last        = input.last;
               VoodooPacket *packet      = NULL;

               std::vector<VoodooChunk> chunks_write;
               std::vector<VoodooChunk> chunks_read;

               if (!output.sending) {
                    direct_mutex_lock( &output.lock );

                    if (output.packets) {
                         VoodooPacket *packet = (VoodooPacket*) output.packets;

                         D_ASSERT( packet->sending );

                         if (voodoo_config->compression_min && packet->size() >= voodoo_config->compression_min) {
                              output.sending = VoodooPacket::Compressed( packet );

                              if (output.sending->flags() & VPHF_COMPRESSED) {
                                   D_DEBUG_AT( Voodoo_Output, "  -> Compressed %u to %u bytes... (packet %p)\n",
                                               output.sending->uncompressed(), output.sending->size(), packet );

                                   output.sending->sending = true;

                                   packet->sending = false;

                                   direct_list_remove( &output.packets, &packet->link );

                                   direct_waitqueue_broadcast( &output.wait );
                              }
                         }
                         else
                              output.sending = packet;

                         output.sent = 0;
                    }

                    direct_mutex_unlock( &output.lock );
               }

               if (output.sending) {
                    packet = output.sending;

                    D_ASSERT( packet->sending );

                    chunk_write = &chunks[1];

                    chunk_write->ptr    = (char*) packet->data_header() + output.sent;
                    chunk_write->length = VOODOO_MSG_ALIGN(packet->size() + sizeof(VoodooPacketHeader)) - output.sent;
                    chunk_write->done   = 0;

                    chunks_write.push_back( chunks[1] );

                    chunk_write = chunks_write.data();
               }

               if (input.end < input.max && manager->DispatchReady()) {
                    chunk_read = &chunks[0];

                    chunk_read->ptr    = input.buffer + input.end;
                    chunk_read->length = input.max - input.end;
                    chunk_read->done   = 0;

                    chunks_read.push_back( chunks[0] );

                    chunk_read = chunks_read.data();
               }


               ret = link->SendReceive( link,
                                        chunks_write.data(), chunks_write.size(),
                                        chunks_read.data(), chunks_read.size() );
               switch (ret) {
                    case DR_OK:
                         if (chunk_write && chunk_write->done) {
                              D_DEBUG_AT( Voodoo_Output, "  -> Sent "_ZD"/"_ZD" bytes... (packet %p)\n", chunk_write->done, chunk_write->length, packet );

                              output.sent += chunk_write->done;

                              if (output.sent == VOODOO_MSG_ALIGN(packet->size() + sizeof(VoodooPacketHeader))) {
                                   output.sending = NULL;

                                   if (packet->flags() & VPHF_COMPRESSED) {
                                        packet->sending = false;

                                        D_FREE( packet );
                                   }
                                   else {
                                        direct_mutex_lock( &output.lock );

                                        packet->sending = false;

                                        direct_list_remove( &output.packets, &packet->link );

                                        direct_mutex_unlock( &output.lock );

                                        direct_waitqueue_broadcast( &output.wait );
                                   }
                              }
                         }
                         break;

                    case DR_TIMEOUT:
                         //D_DEBUG_AT( Voodoo_Connection, "  -> timeout\n" );
                         break;

                    case DR_INTERRUPTED:
                         D_DEBUG_AT( Voodoo_Connection, "  -> interrupted\n" );
                         break;

                    default:
                         if (ret == DR_IO)
                              D_DEBUG_AT( Voodoo_Connection, "  -> Connection closed!\n" );
                         else
                              D_DERROR( ret, "Voodoo/ConnectionPacket: Could not receive data!\n" );

                         closed = true;

                         manager->handle_disconnect();

                         return NULL;
               }


               if (chunk_read && chunk_read->done) {
                    D_DEBUG_AT( Voodoo_Input, "  -> Received "_ZD" bytes...\n", chunk_read->done );

                    input.end += (size_t) chunk_read->done;

                    do {
                         VoodooPacketHeader *header;
                         size_t              aligned;

                         /* Get the packet header. */
                         header  = (VoodooPacketHeader *)(input.buffer + last);
                         aligned = VOODOO_MSG_ALIGN( header->size );

                         D_DEBUG_AT( Voodoo_Input, "  -> Next packet has %u ("_ZU") -> %u bytes (flags 0x%04x)...\n",
                                     header->size, aligned, header->uncompressed, header->flags );

                         if (input.end - last >= sizeof(VoodooPacketHeader)) {
                              D_ASSERT( header->uncompressed >= (int) sizeof(VoodooMessageHeader) );
                              D_ASSERT( header->uncompressed <= VOODOO_PACKET_MAX );
                         }

                         if (sizeof(VoodooPacketHeader) + aligned > input.end - last) {
                              D_DEBUG_AT( Voodoo_Input, "  -> ...fetching tail of message.\n" );

                              /* Extend the buffer if the message doesn't fit into the default boundary. */
                              if (sizeof(VoodooPacketHeader) + aligned > input.max - last)
                                   input.max = last + sizeof(VoodooPacketHeader) + aligned;

                              break;
                         }

                         last += sizeof(VoodooPacketHeader) + aligned;
                    } while (last < input.end);

                    if (last != input.last) {

                         input.last = last;

                         D_DEBUG_AT( Voodoo_Input, "  { START "_ZD", LAST "_ZD", END "_ZD", MAX "_ZD" }\n",
                                     input.start, input.last, input.end, input.max );

                         while (input.start < input.last) {
                              /* Get the packet header. */
                              VoodooPacketHeader *header = (VoodooPacketHeader *)(input.buffer + input.start);

                              VoodooPacket *p;

                              if (header->flags & VPHF_COMPRESSED) {
                                   size_t uncompressed = direct_fastlz_decompress( header + 1, header->size, tmp, header->uncompressed );

                                   D_DEBUG_AT( Voodoo_Input, "  -> Uncompressed "_ZU" bytes (%u compressed)\n", uncompressed, header->size );

                                   (void) uncompressed;

                                   D_ASSERT( uncompressed == header->uncompressed );

                                   // FIXME: don't copy, but read into packet directly, maybe call manager->GetPacket() at the top of this loop
                                   p = VoodooPacket::Copy( header->uncompressed, VPHF_NONE,
                                                           header->uncompressed, tmp );
                              }
                              else {
                                   // FIXME: don't copy, but read into packet directly, maybe call manager->GetPacket() at the top of this loop
                                   p = VoodooPacket::Copy( header->uncompressed, VPHF_NONE,
                                                           header->uncompressed, header + 1 );
                              }

                              manager->DispatchPacket( p );

                              input.start += VOODOO_MSG_ALIGN(header->size) + sizeof(VoodooPacketHeader);
                         }
                    }
               }
          }
     }

     return NULL;
}

/**********************************************************************************************************************/

void *
VoodooConnectionPacket::io_loop_main( DirectThread *thread, void *arg )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p, thread %p )\n", __func__, arg, thread );

     VoodooConnectionPacket *connection = (VoodooConnectionPacket*) arg;

     return connection->io_loop();
}

