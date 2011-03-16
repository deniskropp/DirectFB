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

#include <voodoo/connection_packet.h>
#include <voodoo/manager.h>
#include <voodoo/packet.h>


#include <vector>


#define IN_BUF_MAX ((VOODOO_PACKET_MAX + sizeof(VoodooPacketHeader)) * 1)


//namespace Voodoo {

D_DEBUG_DOMAIN( Voodoo_Connection, "Voodoo/Connection", "Voodoo Connection" );
D_DEBUG_DOMAIN( Voodoo_Input,      "Voodoo/Input",      "Voodoo Input" );
D_DEBUG_DOMAIN( Voodoo_Output,     "Voodoo/Output",     "Voodoo Output" );

/**********************************************************************************************************************/

VoodooConnectionPacket::VoodooConnectionPacket( VoodooManager *manager,
                                                VoodooLink    *link )
     :
     VoodooConnection( manager, link )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p )\n", __func__, this );

     input.start   = 0;
     input.last    = 0;
     input.end     = 0;
     input.max     = 0;

     output.packet  = NULL;
     output.sending = NULL;

     /* Initialize all locks. */
     direct_recursive_mutex_init( &output.lock );

     /* Initialize all wait conditions. */
     direct_waitqueue_init( &output.wait );

     /* Set default buffer limit. */
     input.max = IN_BUF_MAX;

     /* Allocate buffers. */
     size_t input_buffer_size = IN_BUF_MAX + VOODOO_PACKET_MAX + sizeof(VoodooPacketHeader);

     input.buffer = (u8*) D_MALLOC( IN_BUF_MAX + VOODOO_PACKET_MAX + sizeof(VoodooPacketHeader) );

     D_INFO( "VoodooConnection/Packet: Allocated "_ZU" kB input buffer at %p\n", input_buffer_size/1024, input.buffer );

     direct_tls_register( &output.tls, NULL );

     io = direct_thread_create( DTT_DEFAULT, io_loop_main, this, "Voodoo IO" );
}

VoodooConnectionPacket::~VoodooConnectionPacket()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooConnection );

     link->WakeUp( link );

     /* Acquire locks and wake up waiters. */
     direct_mutex_lock( &output.lock );
     direct_waitqueue_broadcast( &output.wait );
     direct_mutex_unlock( &output.lock );

     /* Wait for manager threads exiting. */
     direct_thread_join( io );
     direct_thread_destroy( io );

     /* Destroy conditions. */
     direct_waitqueue_deinit( &output.wait );

     /* Destroy locks. */
     direct_mutex_deinit( &output.lock );

     /* Deallocate buffers. */
     D_FREE( input.buffer );

     // FIXME: delete pending output.packet? tls?
}

/**********************************************************************************************************************/

void *
VoodooConnectionPacket::io_loop_main( DirectThread *thread, void *arg )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p, thread %p )\n", __func__, arg, thread );

     VoodooConnectionPacket *connection = (VoodooConnectionPacket*) arg;

     return connection->io_loop();
}

void *
VoodooConnectionPacket::io_loop()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p )\n", __func__, this );

     while (!manager->is_quit) {
          D_MAGIC_ASSERT( this, VoodooConnection );

          if (input.start == input.max) {
               input.start = 0;
               input.end   = 0;
               input.last  = 0;
               input.max   = IN_BUF_MAX;
          }

          if (!manager->is_quit) {
               DirectResult  ret;
               VoodooChunk   chunks[2];
               VoodooChunk  *chunk_read  = NULL;
               VoodooChunk  *chunk_write = NULL;
               size_t        last        = input.last;
               VoodooPacket *packet      = NULL;

               std::vector<VoodooChunk> chunks_write;
               std::vector<VoodooChunk> chunks_read;

               if (input.end < input.max) {
                    chunk_read = &chunks[0];

                    chunk_read->ptr    = input.buffer + input.end;
                    chunk_read->length = input.max - input.end;
                    chunk_read->done   = 0;

                    chunks_read.push_back( chunks[0] );

                    chunk_read = chunks_read.data();
               }

               if (!output.sending) {
                    direct_mutex_lock( &output.lock );

                    if (output.packet) {
                         if (voodoo_config->compression_min && output.packet->size() >= voodoo_config->compression_min) {
                              output.sending = VoodooPacket::Compressed( output.packet );
                         }
                         else
                              output.sending = output.packet;

                         output.packet = NULL;

                         direct_mutex_unlock( &output.lock );

                         direct_waitqueue_broadcast( &output.wait );
                    }
                    else
                         direct_mutex_unlock( &output.lock );
               }


               if (output.sending) {
                    packet = output.sending;

                    chunk_write = &chunks[1];

                    chunk_write->ptr    = (void*) packet->data_header();
                    chunk_write->length = VOODOO_MSG_ALIGN(packet->size()) + sizeof(VoodooPacketHeader);
                    chunk_write->done   = 0;

                    //if (chunk_write->length > 65536)
                         //chunk_write->length = 65536;

                    chunks_write.push_back( chunks[1] );

                    chunk_write = chunks_write.data();
               }

#if 0
               if (chunk_write) {
                    char buf[chunk_write->length*4/3];

                    size_t comp = direct_fastlz_compress( chunk_write->ptr, chunk_write->length, buf );

                    printf( "  -> Compressed "_ZU"%% ("_ZU" -> "_ZU")\n",
                                comp * 100 / chunk_write->length, chunk_write->length, comp );
               }
#endif

               ret = link->SendReceive( link,
                                        chunks_write.data(), chunks_write.size(),
                                        chunks_read.data(), chunks_read.size() );
               switch (ret) {
                    case DR_OK:
                         if (chunk_write && chunk_write->done) {
                              D_DEBUG_AT( Voodoo_Output, "  -> Sent "_ZD"/"_ZD" bytes...\n", chunk_write->done, chunk_write->length );

                              // FIXME
                              D_ASSERT( chunk_write->done == chunk_write->length );

                              if (packet->flags() & VPHF_COMPRESSED)
                                   delete packet;

                              packet = NULL;
                              output.sending = NULL;


                              // TODO: fetch output.packet already, in case of slow dispatch below
                         }
                         break;

                    case DR_TIMEOUT:
                         //D_WARN("timeout");
                         break;

                    case DR_INTERRUPTED:
                         //D_WARN("interrupted");
                         break;

                    default:
                         D_DERROR( ret, "Voodoo/Manager: Could not receive data!\n" );
                         manager->handle_disconnect();
                         break;
               }

               // FIXME
               D_ASSERT( packet == output.sending );


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

                         D_ASSERT( header->uncompressed >= (int) sizeof(VoodooMessageHeader) );
                         D_ASSERT( header->uncompressed <= VOODOO_PACKET_MAX );

                         if (sizeof(VoodooPacketHeader) + aligned > input.end - last) {
                              D_DEBUG_AT( Voodoo_Input, "  -> ...fetching tail of message.\n" );

                              /* Extend the buffer if the message doesn't fit into the default boundary. */
                              if (sizeof(VoodooPacketHeader) + aligned > input.max - last) {
                                   D_ASSERT( input.max == IN_BUF_MAX );


                                   input.max = last + sizeof(VoodooPacketHeader) + aligned;
                              }

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

                              if (header->flags & VPHF_COMPRESSED) {
                                   size_t uncompressed = direct_fastlz_decompress( header + 1, header->size, input.tmp, header->uncompressed );

                                   D_DEBUG_AT( Voodoo_Input, "  -> Uncompressed "_ZU" bytes (%u compressed)\n", uncompressed, header->size );

                                   (void) uncompressed;

                                   D_ASSERT( uncompressed == header->uncompressed );
     
                                   ProcessMessages( (VoodooMessageHeader *) input.tmp, header->uncompressed );
                              }
                              else
                                   ProcessMessages( (VoodooMessageHeader *)(header + 1), header->uncompressed );

                              input.start += VOODOO_MSG_ALIGN(header->size) + sizeof(VoodooPacketHeader);
                         }
                    }
               }
          }
     }

     return NULL;
}

/**********************************************************************************************************************/

DirectResult
VoodooConnectionPacket::lock_output( int    length,
                                     void **ret_ptr )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p, length %d )\n", __func__, this, length );

     D_MAGIC_ASSERT( this, VoodooConnection );
     D_ASSERT( length >= (int) sizeof(VoodooMessageHeader) );
     D_ASSUME( length <= MAX_MSG_SIZE );
     D_ASSERT( ret_ptr != NULL );

     if (length > MAX_MSG_SIZE) {
          D_WARN( "%d exceeds maximum message size of %d", length, MAX_MSG_SIZE );
          return DR_LIMITEXCEEDED;
     }

     D_UNIMPLEMENTED();

     return DR_UNIMPLEMENTED;
}

DirectResult
VoodooConnectionPacket::unlock_output( bool flush )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p, %sflush )\n", __func__, this, flush ? "" : "NO " );

     D_MAGIC_ASSERT( this, VoodooConnection );

     D_UNIMPLEMENTED();

     return DR_UNIMPLEMENTED;
}

VoodooPacket *
VoodooConnectionPacket::GetPacket( size_t length )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p, length "_ZU" )\n", __func__, this, length );

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
     if (packet)
          packet->reset( aligned );

     return packet;
}

void
VoodooConnectionPacket::PutPacket( VoodooPacket *packet, bool flush )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p, %sflush )\n", __func__, this, flush ? "" : "NO " );

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
VoodooConnectionPacket::Flush( VoodooPacket *packet )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p, packet %p )\n", __func__, this, packet );

     D_MAGIC_ASSERT( this, VoodooConnection );

     direct_mutex_lock( &output.lock );

     D_ASSERT( output.packet != packet );

     while (output.packet)
          direct_waitqueue_wait( &output.wait, &output.lock );

     output.packet = packet;

     direct_mutex_unlock( &output.lock );

     link->WakeUp( link );
}

VoodooPacket *
VoodooConnectionPacket::Packets::Get()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::Packets::%s( %p )\n", __func__, this );

     VoodooPacket *packet;

     if (packets[2]) {
          packet = packets[next];

          D_DEBUG_AT( Voodoo_Connection, "  -> reusing %p\n", packet );
     }
     else if (packets[1]) {
          packet = packets[2] = VoodooPacket::New( 0 );

          D_DEBUG_AT( Voodoo_Connection, "  -> new [2] %p\n", packet );
     }
     else if (packets[0]) {
          packet = packets[1] = VoodooPacket::New( 0 );

          D_DEBUG_AT( Voodoo_Connection, "  -> new [1] %p\n", packet );
     }
     else {
          packet = packets[0] = VoodooPacket::New( 0 );

          D_DEBUG_AT( Voodoo_Connection, "  -> new [0] %p\n", packet );
     }

     if (packet)
          next = (next+1) % 3;

     active = packet;

     return packet;
}

