/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__QUEUE_H__
#define __ONE__QUEUE_H__

#include "onedev.h"
#include "types.h"



struct __One_OneQueue {
     OneEntry       entry;

     OneID          one_id;

     OneQueueFlags  flags;

     DirectLink    *nodes;         /* target nodes for a virtual queue (forwards) */

     int            dispatch_count;

     bool           destroyed;

     OneTarget     *target;

     long long      receive_us;
     int            receive_tid;

     long long      dispatch_us;
     int            dispatch_tid;
};


/* module init/cleanup */

int  one_queue_init  ( OneDev *dev );
void one_queue_deinit( OneDev *dev );


/* public API */

int  one_queue_new             ( OneApp                  *oneapp,
                                 OneQueueNew             *queue_new );

int  one_queue_destroy         ( OneApp                  *oneapp,
                                 OneQueueDestroy         *queue_destroy );

int  one_queue_attach          ( OneApp                  *oneapp,
                                 OneQueueAttach          *queue_attach );

int  one_queue_detach          ( OneApp                  *oneapp,
                                 OneQueueDetach          *queue_detach );

int  one_queue_dispatch        ( OneApp                  *oneapp,
                                 OneQueueDispatch        *queue_dispatch );

int  one_queue_receive         ( OneApp                  *oneapp,
                                 OneQueueReceive         *queue_receive );

int  one_queue_dispatch_receive( OneApp                  *oneapp,
                                 OneQueueDispatchReceive *queue_dispatch_receive );

int  one_queue_wakeup          ( OneApp                  *oneapp,
                                 OneQueueWakeUp          *queue_wakeup );

int  one_queue_stamp_receive   ( OneApp                  *oneapp,
                                 OneQID                   queue_id );


/* internal functions */

void one_queue_destroy_all     ( OneApp                  *oneapp );

void one_queue_detach_all      ( OneDev                  *dev,
                                 OneQID                   queue_id );


int  one_queue_add_target      ( OneQID                   queue_id,
                                 OneQID                   target_id,
                                 OneTarget               *target );

int  one_queue_remove_target   ( OneQID                   queue_id,
                                 OneQID                   target_id,
                                 OneTarget               *target );

#endif
