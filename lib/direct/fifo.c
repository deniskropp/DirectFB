/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

     D_MAGIC_SET( fifo, DirectFifo );
}

void
direct_fifo_destroy( DirectFifo *fifo )
{
     D_DEBUG_AT( Direct_Fifo, "%s( %p )\n", __FUNCTION__, fifo );

     D_MAGIC_ASSERT( fifo, DirectFifo );

     D_MAGIC_CLEAR( fifo );
}

/**********************************************************************************************************************/

__attribute__((unused))
__attribute__((noinline))
static int
d_sync_add_and_fetch( int *p, int v )
{
      return D_SYNC_ADD_AND_FETCH( p, v );
}

__attribute__((noinline))
static void
d_sync_push( void *f, void *i )
{
      D_SYNC_PUSH( f, i );
}

__attribute__((noinline))
static void *
d_sync_fetch_and_clear( void **p )
{
      return D_SYNC_FETCH_AND_CLEAR( p );
}

int
direct_fifo_push( DirectFifo *fifo, DirectFifoItem *item )
{
     int index;

     D_DEBUG_AT( Direct_Fifo, "%s( %p, %p )\n", __FUNCTION__, fifo, item );

     D_MAGIC_ASSERT( fifo, DirectFifo );

     D_MAGIC_SET( item, DirectFifoItem );

     index = 0;//d_sync_add_and_fetch( &fifo->count, 1 ) - 1;
     D_ASSERT( index >= 0 );

     d_sync_push( &fifo->in, item );

     D_DEBUG_AT( Direct_Fifo, "  * * * %p [%d] <--= %p\n", fifo, index, item );


     if (index == 0) {
#if 0
          if (fifo->up) {
               DirectFifo *down = D_SYNC_FETCH_AND_CLEAR( &fifo->down );
               
               if (down) {
                    D_MAGIC_ASSERT( down, DirectFifo );
                    D_ASSERT( down == fifo->up );
     
                    direct_fifo_push( down, &fifo->item );
               }
          }
          else
#endif
          if (fifo->waiting && fifo->in /* && fifo->count*/) {
               direct_futex_wake( &fifo->waiting, 1 );

               //D_DEBUG_AT__( Direct_Fifo, "  -> index %d, waiting %d, count %d\n", index, fifo->waiting, fifo->count );
          }
          else {
               //D_DEBUG_AT__( Direct_Fifo, "  -> index %d, none waiting (%d), count %d\n", index, fifo->waiting, fifo->count );
          }
     }

     return index;
}

void *
direct_fifo_pull( DirectFifo *fifo )
{
     int             index;
     DirectFifoItem *tmp, *out;

     D_DEBUG_AT( Direct_Fifo, "%s( %p )\n", __FUNCTION__, fifo );

     D_MAGIC_ASSERT( fifo, DirectFifo );

     tmp = fifo->out;
     if (tmp) {
          D_MAGIC_ASSERT( tmp, DirectFifoItem );

          fifo->out = tmp->next;
     }
     else {
          tmp = d_sync_fetch_and_clear( (void**) &fifo->in );
          if (!tmp)
               return NULL;
               
          D_MAGIC_ASSERT( tmp, DirectFifoItem );

          out = NULL;

          while (tmp->next) {
               DirectFifoItem *next;

               next = tmp->next;

               D_MAGIC_ASSERT( next, DirectFifoItem );

               tmp->next = out;

               out = tmp;

               tmp = next;
          }

          fifo->out = out;
     }

     D_MAGIC_CLEAR( tmp );

     index = 0;//d_sync_add_and_fetch( &fifo->count, -1 );

     D_DEBUG_AT( Direct_Fifo, "  : : : %p [%d] =--> %p\n", fifo, index, tmp );

     D_ASSERT( index >= 0 );

     return tmp;
}

void *
direct_fifo_pop( DirectFifo *fifo )
{
     int                    index;
     DirectFifoItem        *item;

     D_DEBUG_AT( Direct_Fifo, "%s( %p )\n", __FUNCTION__, fifo );

     D_MAGIC_ASSERT( fifo, DirectFifo );

//     D_ASSUME( fifo->up == NULL );

     if (!fifo->in) {
          D_DEBUG_AT( Direct_Fifo, "  -> fifo->in = NULL\n" );
          return NULL;
     }

     item = D_SYNC_POP( &fifo->in );
     if (!item) {
          D_DEBUG_AT( Direct_Fifo, "  -> item NULL\n" );
          return NULL;
     }

     D_DEBUG_AT( Direct_Fifo, "  -> item %p\n", item );

     D_MAGIC_ASSERT( item, DirectFifoItem );

     item->next = NULL;

     D_MAGIC_CLEAR( item );

     index = 0;//D_SYNC_ADD_AND_FETCH( &fifo->count, -1 );

     D_DEBUG_AT( Direct_Fifo, "  # # # %p [%d] =--> %p\n", fifo, index, item );

     D_ASSERT( index >= 0 );

     return item;
}

/**********************************************************************************************************************/

DirectResult
direct_fifo_wait( DirectFifo *fifo )
{
     DirectResult    ret = DR_OK;

     D_MAGIC_ASSERT( fifo, DirectFifo );

     D_DEBUG_AT( Direct_Fifo, "%s( %p ) ## ## %p # %p\n", __FUNCTION__, fifo, fifo->in, fifo->out );

     D_SYNC_ADD( &fifo->waiting, 1 );

     while (!fifo->in) {//          count) {
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
     DirectResult    ret = DR_OK;
     struct timespec timeout;

     D_MAGIC_ASSERT( fifo, DirectFifo );

     D_DEBUG_AT( Direct_Fifo, "%s( %p ) ## ## %p # %p\n", __FUNCTION__, fifo, fifo->in, fifo->out );

     timeout.tv_sec  =  timeout_ms / 1000;
     timeout.tv_nsec = (timeout_ms % 1000) * 1000000;

     D_SYNC_ADD( &fifo->waiting, 1 );

     while (!fifo->in) {
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

