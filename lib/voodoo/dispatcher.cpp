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
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <voodoo/link.h>
#include <voodoo/message.h>
}

#include <voodoo/connection.h>
#include <voodoo/dispatcher.h>
#include <voodoo/manager.h>
#include <voodoo/packet.h>


D_DEBUG_DOMAIN( Voodoo_Dispatcher, "Voodoo/Dispatcher", "Voodoo Dispatcher" );

/**********************************************************************************************************************/

VoodooDispatcher::VoodooDispatcher( VoodooManager *manager )
     :
     magic(0),
     manager(manager),
     packets(NULL)
{
     D_DEBUG_AT( Voodoo_Dispatcher, "VoodooDispatcher::%s( %p )\n", __func__, this );

     /* Initialize lock. */
     direct_mutex_init( &lock );

     /* Initialize wait queue. */
     direct_waitqueue_init( &queue );

     D_MAGIC_SET( this, VoodooDispatcher );


     dispatch_loop = direct_thread_create( DTT_MESSAGING, DispatchLoopMain, this, "Voodoo Dispatch" );
}

VoodooDispatcher::~VoodooDispatcher()
{
     D_DEBUG_AT( Voodoo_Dispatcher, "VoodooDispatcher::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooDispatcher );

     /* Acquire lock and wake up waiters. */
     direct_mutex_lock( &lock );
     direct_waitqueue_broadcast( &queue );
     direct_mutex_unlock( &lock );

     /* Wait for dispatcher loop exiting. */
     direct_thread_join( dispatch_loop );
     direct_thread_destroy( dispatch_loop );

     /* Destroy queue. */
     direct_waitqueue_deinit( &queue );

     /* Destroy lock. */
     direct_mutex_deinit( &lock );

     D_MAGIC_CLEAR( this );
}

bool
VoodooDispatcher::Ready()
{
     D_DEBUG_AT( Voodoo_Dispatcher, "VoodooDispatcher::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooDispatcher );

     direct_mutex_lock( &lock );

     bool ready = direct_list_count_elements_EXPENSIVE( packets ) < 3;

     direct_mutex_unlock( &lock );

     return ready;
}

void
VoodooDispatcher::PutPacket( VoodooPacket *packet )
{
     D_DEBUG_AT( Voodoo_Dispatcher, "VoodooDispatcher::%s( %p, packet %p )\n", __func__, this, packet );

     D_MAGIC_ASSERT( this, VoodooDispatcher );

     direct_mutex_lock( &lock );

     direct_list_append( &packets, &packet->link );

     direct_waitqueue_broadcast( &queue );

     direct_mutex_unlock( &lock );
}

void
VoodooDispatcher::ProcessMessages( VoodooMessageHeader *first,
                                   size_t               total_length )
{
     D_DEBUG_AT( Voodoo_Dispatcher, "VoodooDispatcher::%s( %p, first %p, total_length " _ZU " )\n",
                 __func__, this, first, total_length );

     D_MAGIC_ASSERT( this, VoodooDispatcher );

     VoodooMessageHeader *header = first;
     size_t               offset = 0;
     size_t               aligned;

     while (offset < total_length) {
          /* Get the message header. */
          header  = (VoodooMessageHeader *)((char*) first + offset);
          aligned = VOODOO_MSG_ALIGN( header->size );

          D_DEBUG_AT( Voodoo_Dispatcher, "  -> Next message has %d (" _ZU ") bytes and is of type %d... (offset " _ZU "/" _ZU ")\n",
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

               case VMSG_DISCOVER:
                    manager->handle_discover( header );
                    break;

               case VMSG_SENDINFO: // should only be received by TCP fake player, not manager
                    D_BUG( "received SENDINFO" );
                    break;

               default:
                    D_BUG( "invalid message type %d", header->type );
                    break;
          }

          offset += aligned;
     }

     D_ASSERT( offset == total_length );
}

/**********************************************************************************************************************/

void *
VoodooDispatcher::DispatchLoop()
{
     D_DEBUG_AT( Voodoo_Dispatcher, "VoodooDispatcher::%s( %p )\n", __func__, this );

     direct_mutex_lock( &lock );

     while (!manager->is_quit) {
          VoodooPacket *packet;

          D_MAGIC_ASSERT( this, VoodooDispatcher );

          if (packets) {
               packet = (VoodooPacket*) packets;

               direct_list_remove( &packets, &packet->link );

               manager->connection->WakeUp();
          }
          else {
               direct_waitqueue_wait( &queue, &lock );

               continue;
          }


          direct_mutex_unlock( &lock );

          ProcessMessages( (VoodooMessageHeader*) packet->data_start(), packet->size() );

          D_FREE( packet );

          direct_mutex_lock( &lock );
     }

     direct_mutex_unlock( &lock );

     return NULL;
}

void *
VoodooDispatcher::DispatchLoopMain( DirectThread *thread, void *arg )
{
     D_DEBUG_AT( Voodoo_Dispatcher, "VoodooDispatcher::%s( %p, thread %p )\n", __func__, arg, thread );

     VoodooDispatcher *dispatcher = (VoodooDispatcher*) arg;

     return dispatcher->DispatchLoop();
}

