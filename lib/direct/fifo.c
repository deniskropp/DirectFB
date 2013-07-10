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

#include <direct/atomic.h>
#include <direct/debug.h>
#include <direct/fifo.h>
#include <direct/util.h>

D_LOG_DOMAIN( Direct_Fifo, "Direct/Fifo", "Direct FIFO" );

/**********************************************************************************************************************/

void
direct_fifo_init( DirectFifo *fifo )
{
     D_DEBUG_AT( Direct_Fifo, "%s( %p )\n", __FUNCTION__, fifo );

     memset( fifo, 0, sizeof(DirectFifo) );

     direct_mutex_init( &fifo->lock );
     direct_waitqueue_init( &fifo->wq );

     D_MAGIC_SET( fifo, DirectFifo );
}

void
direct_fifo_destroy( DirectFifo *fifo )
{
     D_DEBUG_AT( Direct_Fifo, "%s( %p )\n", __FUNCTION__, fifo );

     D_MAGIC_ASSERT( fifo, DirectFifo );

     direct_mutex_deinit( &fifo->lock );
     direct_waitqueue_deinit( &fifo->wq );

     D_MAGIC_CLEAR( fifo );
}

/**********************************************************************************************************************/

int
direct_fifo_push( DirectFifo *fifo, DirectFifoItem *item )
{
     D_DEBUG_AT( Direct_Fifo, "%s( %p, %p )\n", __FUNCTION__, fifo, item );

     D_MAGIC_ASSERT( fifo, DirectFifo );

     D_MAGIC_SET( item, DirectFifoItem );

     direct_mutex_lock( &fifo->lock );

     D_DEBUG_AT( Direct_Fifo, "  * * * %p <--= %p\n", fifo, item );

     direct_list_append( &fifo->items, &item->link );

     direct_mutex_unlock( &fifo->lock );

     direct_waitqueue_broadcast( &fifo->wq );

     return 0;
}

void *
direct_fifo_pull( DirectFifo *fifo )
{
     DirectFifoItem *tmp;

     D_DEBUG_AT( Direct_Fifo, "%s( %p )\n", __FUNCTION__, fifo );

     D_MAGIC_ASSERT( fifo, DirectFifo );

     direct_mutex_lock( &fifo->lock );

     while (!fifo->items)
          direct_waitqueue_wait( &fifo->wq, &fifo->lock );

     tmp = (DirectFifoItem*) fifo->items;

     direct_list_remove( &fifo->items, &tmp->link );
     D_MAGIC_ASSERT( tmp, DirectFifoItem );

     direct_mutex_unlock( &fifo->lock );

     D_MAGIC_CLEAR( tmp );

     D_DEBUG_AT( Direct_Fifo, "  : : : %p [%d] =--> %p\n", fifo, 0, tmp );

     return tmp;
}

void *
direct_fifo_pop( DirectFifo *fifo )
{
     int                    index;
     DirectFifoItem        *item;

     (void)index;

     D_DEBUG_AT( Direct_Fifo, "%s( %p )\n", __FUNCTION__, fifo );

     D_MAGIC_ASSERT( fifo, DirectFifo );

     if (!fifo->items) {
          D_DEBUG_AT( Direct_Fifo, "  -> fifo->items = NULL\n" );
          return NULL;
     }

     item = (DirectFifoItem*) fifo->items;

     D_MAGIC_ASSERT( item, DirectFifoItem );


     D_MAGIC_CLEAR( item );

     D_DEBUG_AT( Direct_Fifo, "  # # # %p [%d] =--> %p\n", fifo, 0, item );

     return item;
}

/**********************************************************************************************************************/

DirectResult
direct_fifo_wait( DirectFifo *fifo )
{
     DirectResult    ret = DR_OK;

     D_MAGIC_ASSERT( fifo, DirectFifo );

     D_DEBUG_AT( Direct_Fifo, "%s( %p ) ## ## %p\n", __FUNCTION__, fifo, fifo->items );

     D_SYNC_ADD( &fifo->waiting, 1 );

     while (!fifo->items) {
          if (fifo->wake) {
               D_DEBUG_AT( Direct_Fifo, "    ### ### WAKE UP ### ###\n" );
               fifo->wake = false;
               break;
          }

          //ret = direct_futex_wait( &fifo->count, 0 );
          ret = direct_futex_wait( &fifo->waiting, 1 );
          if (ret)
               break;
     }

     D_SYNC_ADD( &fifo->waiting, -1 );

     return ret;
}

DirectResult
direct_fifo_wait_timed( DirectFifo *fifo, int timeout_ms )
{
     DirectResult ret = DR_OK;

     D_MAGIC_ASSERT( fifo, DirectFifo );

     D_DEBUG_AT( Direct_Fifo, "%s( %p ) ## ## %p\n", __FUNCTION__, fifo, fifo->items );

     D_SYNC_ADD( &fifo->waiting, 1 );

     while (!fifo->items) {
          if (fifo->wake) {
               D_DEBUG_AT( Direct_Fifo, "    ### ### WAKE UP ### ###\n" );
               fifo->wake = false;
               break;
          }

          //ret = direct_futex_wait_timed( &fifo->count, 0, timeout_ms );
          ret = direct_futex_wait_timed( &fifo->waiting, 1, timeout_ms );
          if (ret)
               break;
     }

     D_SYNC_ADD( &fifo->waiting, -1 );

     return ret;
}

DirectResult
direct_fifo_wakeup( DirectFifo *fifo )
{
     D_DEBUG_AT( Direct_Fifo, "    # # # # WAKE UP # # # #\n" );

     D_MAGIC_ASSERT( fifo, DirectFifo );

     fifo->wake = true;

     return direct_futex_wake( &fifo->waiting, 1 );
}

