/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <pthread.h>
#include <signal.h>
#include <sched.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <core/coredefs.h>
#include <core/sig.h>
#include <core/thread.h>

#include <misc/mem.h>

struct _CoreThread {
     pthread_t       thread;  /* The pthread thread identifier. */

     CoreThreadType  type;    /* The thread's type, e.g. input thread. */
     CoreThreadMain  main;    /* The thread's main routine (or entry point). */
     void           *arg;     /* Custom argument passed to the main routine. */

     bool            cancel;  /* True if dfb_thread_cancel() has been called. */
     bool            joined;  /* True if dfb_thread_join() succeeded. */
};

/*
 * Wrapper around pthread's main routine to pass additional arguments
 * and setup things like signal masks and scheduling priorities.
 */
static void *dfb_thread_main( void *arg );

/******************************************************************************/

CoreThread *
dfb_thread_create( CoreThreadType  thread_type,
                   CoreThreadMain  thread_main,
                   void           *arg )
{
     CoreThread *thread;

     DFB_ASSERT( thread_main != NULL );

     /* Allocate core thread structure. */
     thread = DFBCALLOC( 1, sizeof(CoreThread) );
     if (!thread)
          return NULL;

     /* Write thread information to structure. */
     thread->type = thread_type;
     thread->main = thread_main;
     thread->arg  = arg;

     /* Initialize to -1 for synchronization. */
     thread->thread = -1;

     /* Create and run the thread. */
     pthread_create( &thread->thread, NULL, dfb_thread_main, thread );

     return thread;
}

void
dfb_thread_cancel( CoreThread *thread )
{
     DFB_ASSERT( thread != NULL );

     thread->cancel = true;

     pthread_cancel( thread->thread );
}

bool
dfb_thread_is_canceled( CoreThread *thread )
{
     DFB_ASSERT( thread != NULL );

     return thread->cancel;
}

void
dfb_thread_testcancel( CoreThread *thread )
{
     DFB_ASSERT( thread != NULL );

     DFB_ASSERT( pthread_equal( thread->thread, pthread_self() ) );

     /* Quick check before calling the pthread function. */
     if (thread->cancel)
          pthread_testcancel();
}

void
dfb_thread_join( CoreThread *thread )
{
     DFB_ASSERT( thread != NULL );

     pthread_join( thread->thread, NULL );

     thread->joined = true;
}

bool
dfb_thread_is_joined( CoreThread *thread )
{
     DFB_ASSERT( thread != NULL );

     return thread->joined;
}

void
dfb_thread_destroy( CoreThread *thread )
{
     DFB_ASSERT( thread != NULL );

     if (!thread->joined) {
          if (thread->cancel)
               BUG("thread canceled but not joined");
          else
               BUG("thread still running");

          pthread_kill( thread->thread, SIGKILL );
     }

     DFBFREE( thread );
}

/******************************************************************************/

static void *
dfb_thread_main( void *arg )
{
     CoreThread *thread = (CoreThread*) arg;

     /* Wait for completion of pthread_create(). */
     while (thread->thread == -1)
          sched_yield();

     /* Have all signals handled by the main thread. */
     dfb_sig_block_all();

     /* Adjust scheduling priority. */
     switch (thread->type) {
          case CTT_INPUT:
               setpriority( PRIO_PROCESS, 0, -10 );
               break;

          default:
               break;
     }

     /* Call main routine. */
     return thread->main( thread, thread->arg );
}

