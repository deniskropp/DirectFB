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


#define IN_BUF_MAX   (640 * 1024)
#define OUT_BUF_MAX  (640 * 1024)


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

     input.start  = 0;
     input.last   = 0;
     input.end    = 0;
     input.max    = 0;

     output.start = 0;
     output.end   = 0;

     /* Initialize all locks. */
     direct_recursive_mutex_init( &input.lock );
     direct_recursive_mutex_init( &output.lock );

     /* Initialize all wait conditions. */
     direct_waitqueue_init( &input.wait );
     direct_waitqueue_init( &output.wait );

     /* Set default buffer limit. */
     input.max = IN_BUF_MAX;

     /* Allocate buffers. */
     input.buffer  = (u8*) D_MALLOC( IN_BUF_MAX + VOODOO_PACKET_MAX + sizeof(VoodooPacketHeader) );
     output.buffer = (u8*) D_MALLOC( OUT_BUF_MAX );

     io = direct_thread_create( DTT_DEFAULT, io_loop_main, this, "Voodoo IO" );
}

VoodooConnectionPacket::~VoodooConnectionPacket()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooConnection );

     link->WakeUp( link );

     /* Acquire locks and wake up waiters. */
     direct_mutex_lock( &input.lock );
     direct_waitqueue_broadcast( &input.wait );
     direct_mutex_unlock( &input.lock );

     direct_mutex_lock( &output.lock );
     direct_waitqueue_broadcast( &output.wait );
     direct_mutex_unlock( &output.lock );

     /* Wait for manager threads exiting. */
     direct_thread_join( io );
     direct_thread_destroy( io );

     /* Destroy conditions. */
     direct_waitqueue_deinit( &input.wait );
     direct_waitqueue_deinit( &output.wait );

     /* Destroy locks. */
     direct_mutex_deinit( &input.lock );
     direct_mutex_deinit( &output.lock );

     /* Deallocate buffers. */
     D_FREE( output.buffer );
     D_FREE( input.buffer );
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

//          direct_mutex_lock( &manager->input.lock );

          if (input.start == input.max) {
               input.start = 0;
               input.end   = 0;
               input.last  = 0;
               input.max   = IN_BUF_MAX;
          }

//          direct_mutex_unlock( &input.lock );

          if (!manager->is_quit) {
               DirectResult  ret;
               VoodooChunk   chunks[2];
               VoodooChunk  *chunk_read  = NULL;
               VoodooChunk  *chunk_write = NULL;
               size_t        last        = input.last;

               std::vector<VoodooChunk> chunks_write;
               std::vector<VoodooChunk> chunks_read;

               direct_mutex_lock( &output.lock );

               if (input.end < input.max) {
                    chunk_read = &chunks[0];

                    chunk_read->ptr    = input.buffer + input.end;
                    chunk_read->length = input.max - input.end;
                    chunk_read->done   = 0;

                    chunks_read.push_back( chunks[0] );

                    chunk_read = chunks_read.data();
               }

               if (output.end > output.start) {
                    chunk_write = &chunks[1];

                    chunk_write->ptr    = output.buffer + output.start;
                    chunk_write->length = output.end - output.start;
                    chunk_write->done   = 0;

                    if (chunk_write->length > 65536) {
                         chunk_write->length = 65536;
                    }

                    chunks_write.push_back( chunks[1] );

                    chunk_write = chunks_write.data();
               }

               if (!chunk_write)
                    direct_mutex_unlock( &output.lock );

#if 0
               if (chunk_write) {
                    char buf[chunk_write->length*4/3];

                    size_t comp = direct_fastlz_compress( chunk_write->ptr, chunk_write->length, buf );

                    D_DEBUG_AT( Voodoo_Output, "  -> Compressed "_ZU"%% ("_ZU" -> "_ZU")\n",
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

                              output.start += (size_t) chunk_write->done;

                              //direct_mutex_lock( &output.lock );

                              if (output.start == output.end) {
                                   output.start = output.end = 0;

                                   direct_waitqueue_broadcast( &output.wait );
                              }

                              //direct_mutex_unlock( &output.lock );
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

               if (chunk_write)
                    direct_mutex_unlock( &output.lock );



               if (chunk_read && chunk_read->done) {
                    D_DEBUG_AT( Voodoo_Input, "  -> Received "_ZD" bytes...\n", chunk_read->done );

                    input.end += (size_t) chunk_read->done;

                    do {
                         VoodooPacketHeader *header;

                         /* Get the packet header. */
                         header = (VoodooPacketHeader *)(input.buffer + last);

                         D_DEBUG_AT( Voodoo_Input, "  -> Next packet has %u bytes...\n", header->size );

                         D_ASSERT( header->size >= (int) sizeof(VoodooMessageHeader) );
                         D_ASSERT( header->size <= MAX_MSG_SIZE );

                         if (sizeof(VoodooPacketHeader) + header->size > input.end - last) {
                              D_DEBUG_AT( Voodoo_Input, "  -> ...fetching tail of message.\n" );

                              /* Extend the buffer if the message doesn't fit into the default boundary. */
                              if (sizeof(VoodooPacketHeader) + header->size > input.max - last) {
                                   D_ASSERT( input.max == IN_BUF_MAX );


                                   input.max = last + sizeof(VoodooPacketHeader) + header->size;
                              }

                              break;
                         }

                         last += sizeof(VoodooPacketHeader) + header->size;
                    } while (last < input.end);

                    if (last != input.last) {

                         input.last = last;

                         D_DEBUG_AT( Voodoo_Input, "  { START "_ZD", LAST "_ZD", END "_ZD", MAX "_ZD" }\n",
                                     input.start, input.last, input.end, input.max );

                         while (input.start < input.last) {
                              /* Get the packet header. */
                              VoodooPacketHeader *header = (VoodooPacketHeader *)(input.buffer + input.start);

                              ProcessMessages( (VoodooMessageHeader *)(header + 1), header->uncompressed );

                              input.start += header->size + sizeof(VoodooPacketHeader);
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

     int aligned = VOODOO_MSG_ALIGN( length );

     direct_mutex_lock( &output.lock );

     while (output.end + aligned > OUT_BUF_MAX) {
          link->WakeUp( link );

          direct_waitqueue_wait( &output.wait, &output.lock );

          if (manager->is_quit) {
               direct_mutex_lock( &output.lock );
               return DR_DESTROYED;
          }
     }

     *ret_ptr = output.buffer + output.end;

     D_DEBUG_AT( Voodoo_Output, "  -> offset "_ZD", aligned length %d\n", output.end, aligned );

     output.end += aligned;

     return DR_OK;
}

DirectResult
VoodooConnectionPacket::unlock_output( bool flush )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p, %sflush )\n", __func__, this, flush ? "" : "NO " );

     D_MAGIC_ASSERT( this, VoodooConnection );

     direct_mutex_unlock( &output.lock );

     if (flush)
          link->WakeUp( link );

     return DR_OK;
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

     int aligned = sizeof(VoodooPacketHeader) + VOODOO_MSG_ALIGN( length );

     direct_mutex_lock( &output.lock );

     while (output.end + aligned > OUT_BUF_MAX) {
          link->WakeUp( link );

          direct_waitqueue_wait( &output.wait, &output.lock );

          if (manager->is_quit) {
               direct_mutex_lock( &output.lock );
               return NULL;
          }
     }

     D_DEBUG_AT( Voodoo_Output, "  -> offset "_ZD", aligned length %d\n", output.end, aligned );

     output.end += aligned;

     return VoodooPacket::New( output.buffer + output.end - aligned, aligned - sizeof(VoodooPacketHeader) );
}

void
VoodooConnectionPacket::PutPacket( VoodooPacket *packet, bool flush )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionPacket::%s( %p, %sflush )\n", __func__, this, flush ? "" : "NO " );

     D_MAGIC_ASSERT( this, VoodooConnection );

     direct_mutex_unlock( &output.lock );

     if (flush)
          link->WakeUp( link );

//     delete packet;
}

