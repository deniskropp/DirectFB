/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#ifndef __ONE__ONECORE_IMPL_H__
#define __ONE__ONECORE_IMPL_H__

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif

#include <linux/wait.h>

#include "types.h"


struct __One_OneCore {
     int                 magic;

     u32                 ids;

     struct semaphore    lock;
};


typedef struct {
     int                 magic;

     wait_queue_head_t   queue;
} OneWaitQueue;


#endif
