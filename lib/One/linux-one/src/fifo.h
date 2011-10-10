/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__FIFO_H__
#define __ONE__FIFO_H__

#include "types.h"
#include "list.h"

typedef struct {
     DirectLink *items;
     int         count;
} OneFifo;

void one_fifo_put(OneFifo * fifo, DirectLink * link);

DirectLink *one_fifo_get(OneFifo * fifo);

static inline void one_fifo_reset(OneFifo * fifo)
{
     fifo->items = NULL;
     fifo->count = 0;
}

#endif /* __ONE__FIFO_H__ */
