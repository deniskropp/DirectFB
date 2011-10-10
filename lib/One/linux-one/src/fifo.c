/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
   
   --
   
   Simple FIFO built via plain list
*/

#include <linux/types.h>

#include "fifo.h"

void one_fifo_put(OneFifo * fifo, DirectLink * link)
{
     direct_list_append( &fifo->items, link );

     fifo->count++;
}

DirectLink *one_fifo_get(OneFifo * fifo)
{
     DirectLink *first = fifo->items;

     if (!first) {
          D_ASSERT( fifo->count == 0 );
          return NULL;
     }

     direct_list_remove( &fifo->items, first );

     fifo->count--;

     return first;
}
