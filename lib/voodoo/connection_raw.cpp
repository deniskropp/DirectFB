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

#ifdef VOODOO_CONNECTION_RAW_DUMP
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include <unistd.h>
#include <fcntl.h>
#endif

extern "C" {
#include <direct/debug.h>
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

#include <voodoo/connection_raw.h>
#include <voodoo/manager.h>
#include <voodoo/packet.h>


#include <vector>


//namespace Voodoo {

D_DEBUG_DOMAIN( Voodoo_Connection, "Voodoo/Connection", "Voodoo Connection" );
D_DEBUG_DOMAIN( Voodoo_Input,      "Voodoo/Input",      "Voodoo Input" );
D_DEBUG_DOMAIN( Voodoo_Output,     "Voodoo/Output",     "Voodoo Output" );

/**********************************************************************************************************************/

VoodooConnectionRaw::VoodooConnectionRaw( VoodooLink *link )
     :
     VoodooConnectionLink( link ),
     stop( false ),
     closed( false )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionRaw::%s( %p )\n", __func__, this );

     if (link->code) {
          input.end = 4;

          memcpy( input.buffer, &link->code, sizeof(u32) );
     }
}

VoodooConnectionRaw::~VoodooConnectionRaw()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionRaw::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooConnection );
}

void
VoodooConnectionRaw::Start( VoodooManager *manager )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionRaw::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooConnection );

     D_ASSERT( manager != NULL );
     D_ASSERT( this->manager == NULL );

     this->manager = manager;

     io = direct_thread_create( DTT_DEFAULT, io_loop_main, this, "Voodoo IO" );
}

void
VoodooConnectionRaw::Stop()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionRaw::%s( %p )\n", __func__, this );

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
VoodooConnectionRaw::io_loop()
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionRaw::%s( %p )\n", __func__, this );

#ifdef VOODOO_CONNECTION_RAW_DUMP
     int dump_fd = open("voodoo_write.raw", O_TRUNC|O_CREAT|O_WRONLY, 0660 );
     int dump_read_fd = open("voodoo_read.raw", O_TRUNC|O_CREAT|O_WRONLY, 0660 );
#endif

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

                         output.sending = packet;
                         output.sent    = 0;
                    }

                    direct_mutex_unlock( &output.lock );
               }

               if (output.sending) {
                    packet = output.sending;

                    D_ASSERT( packet->sending );

                    chunk_write = &chunks[1];

                    chunk_write->ptr    = (char*) packet->data_start() + output.sent;
                    chunk_write->length = VOODOO_MSG_ALIGN(packet->size()) - output.sent;
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
                              D_DEBUG_AT( Voodoo_Output, "  -> Sent "_ZD"/"_ZD" bytes...\n", chunk_write->done, chunk_write->length );

#ifdef VOODOO_CONNECTION_RAW_DUMP
                              write( dump_fd, chunk_write->ptr, chunk_write->done );
#endif

                              output.sent += chunk_write->done;

                              if (output.sent == VOODOO_MSG_ALIGN(packet->size())) {
                                   output.sending = NULL;

                                   direct_mutex_lock( &output.lock );

                                   packet->sending = false;

                                   direct_list_remove( &output.packets, &packet->link );

                                   direct_mutex_unlock( &output.lock );

                                   direct_waitqueue_broadcast( &output.wait );
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
                              D_DERROR( ret, "Voodoo/ConnectionRaw: Could not receive data!\n" );

                         closed = true;

                         manager->handle_disconnect();

                         return NULL;
               }


               if (chunk_read && chunk_read->done) {
                    D_DEBUG_AT( Voodoo_Input, "  -> Received "_ZD" bytes...\n", chunk_read->done );

#ifdef VOODOO_CONNECTION_RAW_DUMP
                    write( dump_read_fd, chunk_read->ptr, chunk_read->done );
#endif

                    input.end += (size_t) chunk_read->done;

                    do {
                         VoodooMessageHeader *header;
                         size_t               aligned;

                         D_DEBUG_AT( Voodoo_Input, "  { LAST "_ZD", INPUT LAST "_ZD" }\n", last, input.last );

                         if (input.end - last < 4) {
                              D_DEBUG_AT( Voodoo_Input, "  -> ...only "_ZU" bytes left\n", input.end - last );
                              break;
                         }

                         /* Get the message header. */
                         header  = (VoodooMessageHeader *)(input.buffer + last);
                         aligned = VOODOO_MSG_ALIGN( header->size );

                         D_DEBUG_AT( Voodoo_Input, "  -> Next message has %d ("_ZD") bytes and is of type %d...\n",
                                     header->size, aligned, header->type );

                         D_ASSERT( header->size >= (int) sizeof(VoodooMessageHeader) );
                         D_ASSERT( header->size <= MAX_MSG_SIZE );

                         if (aligned > input.end - last) {
                              D_DEBUG_AT( Voodoo_Input, "  -> ...fetching tail of message.\n" );

                              /* Extend the buffer if the message doesn't fit into the default boundary. */
                              if (aligned > input.max - last)
                                   input.max = last + aligned;

                              break;
                         }

                         last += aligned;
                    } while (last < input.end);

                    if (last != input.last) {
                         input.last = last;

                         D_DEBUG_AT( Voodoo_Input, "  { START "_ZD", LAST "_ZD", END "_ZD", MAX "_ZD" }\n",
                                     input.start, input.last, input.end, input.max );

                         // FIXME: don't copy, but read into packet directly, maybe call manager->GetPacket() at the top of this loop
                         VoodooPacket *p = VoodooPacket::Copy( input.last - input.start, VPHF_NONE,
                                                               input.last - input.start, input.buffer + input.start );

                         manager->DispatchPacket( p );

                         input.start = input.last;
                    }
               }
          }
     }

     return NULL;
}

/**********************************************************************************************************************/

void *
VoodooConnectionRaw::io_loop_main( DirectThread *thread, void *arg )
{
     D_DEBUG_AT( Voodoo_Connection, "VoodooConnectionRaw::%s( %p, thread %p )\n", __func__, arg, thread );

     VoodooConnectionRaw *connection = (VoodooConnectionRaw*) arg;

     return connection->io_loop();
}

