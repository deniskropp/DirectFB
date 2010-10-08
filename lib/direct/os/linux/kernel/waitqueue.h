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

#ifndef __DIRECT__OS__LINUX__KERNEL__WAITQUEUE_H__
#define __DIRECT__OS__LINUX__KERNEL__WAITQUEUE_H__

#include <asm/atomic.h>

#include <linux/sched.h>

#include <direct/debug.h>

#include "mutex.h"

/**********************************************************************************************************************/

struct __D_DirectWaitQueue {
     wait_queue_head_t   queue;

     int                 magic;

     atomic_t            wakeups;
};

/**********************************************************************************************************************/

#define DIRECT_WAITQUEUE_INITIALIZER(name)             { .queue = __WAIT_QUEUE_HEAD_INITIALIZER(name.queue) }

/**********************************************************************************************************************/

static inline DirectResult
direct_waitqueue_init( DirectWaitQueue *queue )
{
     init_waitqueue_head( &queue->queue );

     queue->wakeups = (atomic_t) ATOMIC_INIT(0);

     D_MAGIC_SET( queue, DirectWaitQueue );

     return DR_OK;
}

static inline DirectResult
direct_waitqueue_wait( DirectWaitQueue *queue, DirectMutex *mutex )
{
     DirectResult ret = DR_OK;
     int          wakeups;

     DEFINE_WAIT(__wait);

     D_MAGIC_ASSERT( queue, DirectWaitQueue );

     wakeups = atomic_read(&queue->wakeups);

     for (;;) {
          prepare_to_wait( &queue->queue, &__wait, TASK_INTERRUPTIBLE );

          if (atomic_read(&queue->wakeups) != wakeups)
               break;

          up( &mutex->sema );

          if (signal_pending( current ) || ({schedule(); signal_pending( current );}) || down_interruptible( &mutex->sema )) {
               ret = DR_SIGNALLED;
               break;
          }

          D_MAGIC_ASSERT( queue, DirectWaitQueue );
     }

     finish_wait( &queue->queue, &__wait );

     return ret;
}

static inline DirectResult
direct_waitqueue_wait_timeout( DirectWaitQueue *queue, DirectMutex *mutex, unsigned long micros )
{
     DirectResult  ret = DR_OK;
     int           wakeups;
     unsigned long jiff = usecs_to_jiffies( micros );

     DEFINE_WAIT(__wait);

     D_MAGIC_ASSERT( queue, DirectWaitQueue );

     wakeups = atomic_read(&queue->wakeups);

     do {
          prepare_to_wait( &queue->queue, &__wait, TASK_INTERRUPTIBLE );

          if (atomic_read(&queue->wakeups) != wakeups)
               break;

          up( &mutex->sema );

          if (signal_pending( current )) {
               ret = DR_SIGNALLED;
               break;
          }

          jiff = schedule_timeout( jiff );

          if (signal_pending( current )) {
               ret = DR_SIGNALLED;
               break;
          }

          D_MAGIC_ASSERT( queue, DirectWaitQueue );

          if (!jiff) {
               ret = DR_TIMEOUT;
               break;
          }

          if (down_interruptible( &mutex->sema )) {
               ret = DR_SIGNALLED;
               break;
          }

          D_MAGIC_ASSERT( queue, DirectWaitQueue );
     } while (jiffies > 0 && ret == DR_OK);

     finish_wait( &queue->queue, &__wait );

     return ret;
}

static inline DirectResult
direct_waitqueue_signal( DirectWaitQueue *queue )
{
     D_MAGIC_ASSERT( queue, DirectWaitQueue );

     atomic_inc( &queue->wakeups );

     wake_up( &queue->queue );

     return DR_OK;
}

static inline DirectResult
direct_waitqueue_broadcast( DirectWaitQueue *queue )
{
     D_MAGIC_ASSERT( queue, DirectWaitQueue );

     atomic_inc( &queue->wakeups );

     wake_up_all( &queue->queue );

     return DR_OK;
}

static inline DirectResult
direct_waitqueue_deinit( DirectWaitQueue *queue )
{
     D_MAGIC_ASSERT( queue, DirectWaitQueue );


     D_MAGIC_CLEAR( queue );

     return DR_OK;
}

#endif

