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



#ifndef __DIRECT__OS__WIN32__WAITQUEUE_H__
#define __DIRECT__OS__WIN32__WAITQUEUE_H__

#include <direct/util.h>

#include "mutex.h"


#if 0
/**********  Windows Vista  ********/

/**********************************************************************************************************************/

struct __D_DirectWaitQueue {
     CONDITION_VARIABLE variable;
};

/**********************************************************************************************************************/

static __inline__ DirectResult
direct_waitqueue_init( DirectWaitQueue *queue )
{
     InitializeConditionVariable( &queue->variable );

     return DR_OK;
}

static __inline__ DirectResult
direct_waitqueue_wait( DirectWaitQueue *queue, DirectMutex *mutex )
{
     SleepConditionVariableCS( &queue->variable, &mutex->section, INFINITE );

     return DR_OK;
}

static __inline__ DirectResult
direct_waitqueue_wait_timeout( DirectWaitQueue *queue, DirectMutex *mutex, unsigned long micros )
{
     SleepConditionVariableCS( &queue->variable, &mutex->section, micros / 1000 );

     return DR_OK;
}

static __inline__ DirectResult
direct_waitqueue_signal( DirectWaitQueue *queue )
{
     WakeConditionVariable( &queue->variable );

     return DR_OK;
}

static __inline__ DirectResult
direct_waitqueue_broadcast( DirectWaitQueue *queue )
{
     WakeAllConditionVariable( &queue->variable );

     return DR_OK;
}

static __inline__ DirectResult
direct_waitqueue_deinit( DirectWaitQueue *queue )
{
     //DeleteConditionVariable( &queue->variable );

     return DR_OK;
}


#else

/**********  Windows 2000  ********/

/**********************************************************************************************************************/

struct __D_DirectWaitQueue {
     int waiters_count_;
     // Number of waiting threads.

     CRITICAL_SECTION waiters_count_lock_;
     // Serialize access to <waiters_count_>.

     HANDLE sema_;
     // Semaphore used to queue up threads waiting for the condition to
     // become signaled. 

     HANDLE waiters_done_;
     // An auto-reset event used by the broadcast/signal thread to wait
     // for all the waiting thread(s) to wake up and be released from the
     // semaphore. 

     size_t was_broadcast_;
     // Keeps track of whether we were broadcasting or signaling.  This
     // allows us to optimize the code if we're just signaling.
};

/**********************************************************************************************************************/

static __inline__ DirectResult
direct_waitqueue_init( DirectWaitQueue *queue )
{
     queue->waiters_count_ = 0;
     queue->was_broadcast_ = 0;
     queue->sema_ = CreateSemaphore (NULL,       // no security
                                   0,          // initially 0
                                   0x7fffffff, // max count
                                   NULL);      // unnamed 
     InitializeCriticalSection (&queue->waiters_count_lock_);
     queue->waiters_done_ = CreateEvent (NULL,  // no security
                                      FALSE, // auto-reset
                                      FALSE, // non-signaled initially
                                      NULL); // unnamed

     return DR_OK;
}

static __inline__ DirectResult
direct_waitqueue_wait( DirectWaitQueue *queue, DirectMutex *mutex )
{
     int last_waiter;

     // Avoid race conditions.
     EnterCriticalSection (&queue->waiters_count_lock_);
     queue->waiters_count_++;
     LeaveCriticalSection (&queue->waiters_count_lock_);

     // This call atomically releases the mutex and waits on the
     // semaphore until <pthread_cond_signal> or <pthread_cond_broadcast>
     // are called by another thread.
     SignalObjectAndWait (mutex->handle, queue->sema_, INFINITE, FALSE);

     // Reacquire lock to avoid race conditions.
     EnterCriticalSection (&queue->waiters_count_lock_);

     // We're no longer waiting...
     queue->waiters_count_--;

     // Check to see if we're the last waiter after <pthread_cond_broadcast>.
     last_waiter = queue->was_broadcast_ && queue->waiters_count_ == 0;

     LeaveCriticalSection (&queue->waiters_count_lock_);

     // If we're the last waiter thread during this particular broadcast
     // then let all the other threads proceed.
     if (last_waiter)
          // This call atomically signals the <waiters_done_> event and waits until
          // it can acquire the <external_mutex>.  This is required to ensure fairness. 
          SignalObjectAndWait (queue->waiters_done_, mutex->handle, INFINITE, FALSE);
     else
          // Always regain the external mutex since that's the guarantee we
          // give to our callers. 
          WaitForSingleObject (mutex->handle, INFINITE);

     return DR_OK;
}

static __inline__ DirectResult
direct_waitqueue_wait_timeout( DirectWaitQueue *queue, DirectMutex *mutex, unsigned long micros )
{
	DirectResult ret = DR_OK;
     int last_waiter;

     // Avoid race conditions.
     EnterCriticalSection (&queue->waiters_count_lock_);
     queue->waiters_count_++;
     LeaveCriticalSection (&queue->waiters_count_lock_);

     // This call atomically releases the mutex and waits on the
     // semaphore until <pthread_cond_signal> or <pthread_cond_broadcast>
     // are called by another thread.
     switch (SignalObjectAndWait (mutex->handle, queue->sema_, micros/1000, FALSE)) {
	 case WAIT_TIMEOUT:
		 ret = DR_TIMEOUT;	// FIXME: skip some code below?
		 break;
	 }

     // Reacquire lock to avoid race conditions.
     EnterCriticalSection (&queue->waiters_count_lock_);

     // We're no longer waiting...
     queue->waiters_count_--;

     // Check to see if we're the last waiter after <pthread_cond_broadcast>.
     last_waiter = queue->was_broadcast_ && queue->waiters_count_ == 0;

     LeaveCriticalSection (&queue->waiters_count_lock_);

     // If we're the last waiter thread during this particular broadcast
     // then let all the other threads proceed.
     if (last_waiter)
          // This call atomically signals the <waiters_done_> event and waits until
          // it can acquire the <external_mutex>.  This is required to ensure fairness. 
          SignalObjectAndWait (queue->waiters_done_, mutex->handle, INFINITE, FALSE);
     else
          // Always regain the external mutex since that's the guarantee we
          // give to our callers. 
          WaitForSingleObject (mutex->handle, INFINITE);

     return ret;
}

static __inline__ DirectResult
direct_waitqueue_signal( DirectWaitQueue *queue )
{
     int have_waiters;

     EnterCriticalSection (&queue->waiters_count_lock_);
     have_waiters = queue->waiters_count_ > 0;
     LeaveCriticalSection (&queue->waiters_count_lock_);

     // If there aren't any waiters, then this is a no-op.  
     if (have_waiters)
          ReleaseSemaphore (queue->sema_, 1, 0);

     return DR_OK;
}

static __inline__ DirectResult
direct_waitqueue_broadcast( DirectWaitQueue *queue )
{
     int have_waiters = 0;

     // This is needed to ensure that <waiters_count_> and <was_broadcast_> are
     // consistent relative to each other.
     EnterCriticalSection (&queue->waiters_count_lock_);

     if (queue->waiters_count_ > 0) {
          // We are broadcasting, even if there is just one waiter...
          // Record that we are broadcasting, which helps optimize
          // <pthread_cond_wait> for the non-broadcast case.
          queue->was_broadcast_ = 1;
          have_waiters = 1;
     }

     if (have_waiters) {
          // Wake up all the waiters atomically.
          ReleaseSemaphore (queue->sema_, queue->waiters_count_, 0);

          LeaveCriticalSection (&queue->waiters_count_lock_);

          // Wait for all the awakened threads to acquire the counting
          // semaphore. 
          WaitForSingleObject (queue->waiters_done_, INFINITE);
          // This assignment is okay, even without the <waiters_count_lock_> held 
          // because no other waiter threads can wake up to access it.
          queue->was_broadcast_ = 0;
     }
     else
          LeaveCriticalSection (&queue->waiters_count_lock_);

     return DR_OK;
}

static __inline__ DirectResult
direct_waitqueue_deinit( DirectWaitQueue *queue )
{
     DeleteCriticalSection( &queue->waiters_count_lock_ );
	 CloseHandle( queue->sema_ );
	 CloseHandle( queue->waiters_done_ );

     return DR_OK;
}

#endif


#define DIRECT_OS_WAITQUEUE_DEFINED

#endif

