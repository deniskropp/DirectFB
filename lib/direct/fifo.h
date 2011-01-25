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

#ifndef __DIRECT__FIFO_H__
#define __DIRECT__FIFO_H__

#include <direct/debug.h>
#include <direct/types.h>


struct __D_DirectFifoItem {
     DirectFifoItem     *next;     /* MUST BE FIRST for D_SYNC_PUSH!!! */

     int                 magic;
};

struct __D_DirectFifo {
//     DirectFifoItem      item;

     int                 magic;

//     int                 count;
     int                 waiting;

     DirectFifoItem     *in;
     DirectFifoItem     *out;

//     DirectFifo         *up;
//     DirectFifo         *down;

     bool                wake;
};

/**********************************************************************************************************************/

void DIRECT_API  direct_fifo_init   ( DirectFifo *fifo );
void DIRECT_API  direct_fifo_destroy( DirectFifo *fifo );

/**********************************************************************************************************************/

int  DIRECT_API  direct_fifo_push( DirectFifo *fifo, DirectFifoItem *item );
void DIRECT_API *direct_fifo_pull( DirectFifo *fifo );
void DIRECT_API *direct_fifo_pop ( DirectFifo *fifo );

/**********************************************************************************************************************/

DirectResult DIRECT_API direct_fifo_wait      ( DirectFifo *fifo );
DirectResult DIRECT_API direct_fifo_wait_timed( DirectFifo *fifo, int timeout_ms );
DirectResult DIRECT_API direct_fifo_wakeup    ( DirectFifo *fifo );


#endif

