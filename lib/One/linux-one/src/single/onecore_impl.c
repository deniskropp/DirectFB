/*
   (c) Copyright 2011  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version
   2 of the License, or (at your option) any later version.
*/

#include <linux/version.h>
#include <linux/module.h>
#ifdef HAVE_LINUX_CONFIG_H
#include <linux/config.h>
#endif
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include "debug.h"

#include "onecore.h"
#include "one_udp.h"


int
one_core_enter( OneCore **ret_core )
{
     OneCore *core;

     ONE_DEBUG( "%s()\n", __FUNCTION__ );

     D_ASSERT( ret_core != NULL );

     core = kzalloc( sizeof(OneCore), GFP_KERNEL );
     if (!core)
          return -ENOMEM;

     sema_init( &core->lock, 1 );

     D_MAGIC_SET( core, OneCore );

     *ret_core = core;

     return 0;
}

void
one_core_exit( OneCore *core )
{
     ONE_DEBUG( "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, OneCore );


     D_MAGIC_CLEAR( core );

     kfree( core );
}

void
one_core_cleanup( OneCore *core )
{
     ONE_DEBUG( "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( core, OneCore );
}

pid_t
one_core_pid( OneCore *core )
{
     D_MAGIC_ASSERT( core, OneCore );

     return current->pid;
}

u32
one_core_new_id( OneCore *core )
{
     D_MAGIC_ASSERT( core, OneCore );

     return ++core->ids;
}

bool
one_core_is_local( OneCore *core,
                   u32      id )
{
     return true;
}

int
one_core_dispatch( OneCore               *core,
                   OneApp                *app,
                   const OnePacketHeader *header,
                   const struct iovec    *iov,
                   size_t                 iov_count )
{
     return -EIO;
}

int
one_core_attach( OneCore        *core,
                 OneApp         *app,
                 OneQueueAttach *queue_attach )
{
     return -EIO;
}

int
one_core_detach( OneCore        *core,
                 OneApp         *app,
                 OneQueueDetach *queue_detach )
{
     return -EIO;
}


void *
one_core_malloc( OneCore *core,
                 size_t   size )
{
     D_MAGIC_ASSERT( core, OneCore );

     return kmalloc( size, GFP_KERNEL );
}

void
one_core_free( OneCore *core,
               void    *ptr )
{
     D_MAGIC_ASSERT( core, OneCore );

     kfree( ptr );
}


void
one_core_lock( OneCore *core )
{
     D_MAGIC_ASSERT( core, OneCore );

     down( &core->lock );
}

void
one_core_unlock( OneCore *core )
{
     D_MAGIC_ASSERT( core, OneCore );

     up( &core->lock );
}


int
one_core_wq_init( OneCore      *core,
                  OneWaitQueue *queue )
{
     D_MAGIC_ASSERT( core, OneCore );

     memset( queue, 0, sizeof(OneWaitQueue) );

     init_waitqueue_head( &queue->queue );

     D_MAGIC_SET( queue, OneWaitQueue );

     return 0;
}

void
one_core_wq_deinit( OneCore      *core,
                    OneWaitQueue *queue )
{
     D_MAGIC_ASSERT( core, OneCore );
     D_MAGIC_ASSERT( queue, OneWaitQueue );

     D_MAGIC_CLEAR( queue );
}

void
one_core_wq_wait( OneCore      *core,
                  OneWaitQueue *queue,
                  int          *timeout_ms )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
     DEFINE_WAIT(wait);

     int timeout = 0;

     if (timeout_ms)
          timeout = *timeout_ms * HZ / 1000;


     D_MAGIC_ASSERT( core, OneCore );
     D_MAGIC_ASSERT( queue, OneWaitQueue );

     prepare_to_wait( &queue->queue, &wait, TASK_INTERRUPTIBLE );

     one_core_unlock( core );

     if (timeout_ms) {
          timeout = schedule_timeout(timeout);
          if (timeout) {
               timeout = timeout * 1000 / HZ;

               *timeout_ms = timeout ? timeout : 1;
          }
          else
               *timeout_ms = 0;
     }
     else
          schedule();

     one_core_lock( core );

     finish_wait( &queue->queue, &wait );
#else
     wait_queue_t wait;
     int          timeout = 0;

     if (timeout_ms)
          timeout = *timeout_ms * HZ / 1000;


     D_MAGIC_ASSERT( core, OneCore );
     D_MAGIC_ASSERT( queue, OneWaitQueue );

     init_waitqueue_entry(&wait, current);

     current->state = TASK_INTERRUPTIBLE;

     write_lock( &queue->queue.lock);
     __add_wait_queue( &queue->queue, &wait);
     write_unlock( &queue->queue.lock );

     one_core_unlock( core );

     if (timeout_ms) {
          timeout = schedule_timeout(timeout);
          if (timeout) {
               timeout = timeout * 1000 / HZ;

               *timeout_ms = timeout ? timeout : 1;
          }
          else
               *timeout_ms = 0;
     }
     else
          schedule();

     one_core_lock( core );

     write_lock( &queue->queue.lock );
     __remove_wait_queue( &queue->queue, &wait );
     write_unlock( &queue->queue.lock );
#endif
}

void
one_core_wq_wake( OneCore      *core,
                  OneWaitQueue *queue )
{
     D_MAGIC_ASSERT( core, OneCore );
     D_MAGIC_ASSERT( queue, OneWaitQueue );

     wake_up_all( &queue->queue );
}

