/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__APP_H__
#define __ONE__APP_H__

#include <linux/one.h>

#include "types.h"
#include "list.h"
#include "onecore.h"


struct __One_OneApp {
     int            magic;

     OneDev        *dev;

     OneID          one_id;

     OneWaitQueue   wait_for_packets;
     OneWaitQueue   wait_for_free;

     DirectLink    *recv_data;
};


int  OneApp_New    ( OneDev  *dev,
                     OneID    one_id,
                     OneApp **ret_oneapp );

void OneApp_Destroy( OneApp  *oneapp );


int  OneApp_CreateTarget( OneApp              *oneapp,
                          OneQueue            *queue,
                          OneTarget          **ret_target );

int  OneApp_Receive     ( OneApp              *oneapp,
                          const OneQID        *ids,
                          size_t               ids_count,
                          const struct iovec  *iov,
                          size_t               iov_count,
                          size_t              *ret_received );

#endif

