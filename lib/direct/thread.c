/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <config.h>

#include <pthread.h>
#include <signal.h>
#include <sched.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/thread.h>


/* FIXME: DIRECT_THREAD_WAIT_INIT is required, but should be optional. */
#define DIRECT_THREAD_WAIT_INIT

/* FIXME: DIRECT_THREAD_WAIT_CREATE is required, but should be optional. */
#define DIRECT_THREAD_WAIT_CREATE


struct __D_DirectThread {
     pthread_t         thread;   /* The pthread thread identifier. */
     pid_t             tid;

     DirectThreadType  type;     /* The thread's type, e.g. input thread. */
     DirectThreadMain  main;     /* The thread's main routine (or entry point). */
     void             *arg;      /* Custom argument passed to the main routine. */

     bool              canceled; /* Set when direct_thread_cancel() is called. */
     bool              joining;  /* Set when direct_thread_join() is called. */
     bool              joined;   /* Set when direct_thread_join() has finished. */

#ifdef DIRECT_THREAD_WAIT_INIT
     bool              init;     /* Set to true before calling the main routine. */
#endif
};

/*
 * Wrapper around pthread's main routine to pass additional arguments
 * and setup things like signal masks and scheduling priorities.
 */
static void *direct_thread_main( void *arg );

/******************************************************************************/

DirectThread *
direct_thread_create( DirectThreadType  thread_type,
                      DirectThreadMain  thread_main,
                      void             *arg )
{
     DirectThread *thread;

     D_ASSERT( thread_main != NULL );

     D_DEBUG( "Direct/Thread: Creating new thread of type %d...\n",
              thread_type );

     /* Allocate thread structure. */
     thread = D_CALLOC( 1, sizeof(DirectThread) );
     if (!thread)
          return NULL;

     /* Write thread information to structure. */
     thread->type = thread_type;
     thread->main = thread_main;
     thread->arg  = arg;

     /* Initialize to -1 for synchronization. */
     thread->thread = (pthread_t) -1;
     thread->tid    = (pid_t) -1;

     /* Create and run the thread. */
     pthread_create( &thread->thread, NULL, direct_thread_main, thread );

#ifdef DIRECT_THREAD_WAIT_INIT
     D_DEBUG( "Direct/Thread: Waiting for thread to run...\n" );

     /* Wait for completion of the thread's initialization. */
     while (!thread->init)
          sched_yield();

     D_DEBUG( "Direct/Thread: ...thread is running.\n" );
#endif

     D_DEBUG( "Direct/Thread: ...created thread of type %d (%d).\n",
              thread_type, thread->tid );

     return thread;
}

void
direct_thread_cancel( DirectThread *thread )
{
     D_ASSERT( thread != NULL );
     D_ASSERT( thread->thread != -1 );
     D_ASSERT( !pthread_equal( thread->thread, pthread_self() ) );

     D_ASSUME( !thread->canceled );

     D_DEBUG( "Direct/Thread: Canceling %d.\n", thread->tid );

     thread->canceled = true;

     pthread_cancel( thread->thread );
}

bool
direct_thread_is_canceled( DirectThread *thread )
{
     D_ASSERT( thread != NULL );

     return thread->canceled;
}

void
direct_thread_testcancel( DirectThread *thread )
{
     D_ASSERT( thread != NULL );
     D_ASSERT( thread->thread != -1 );
     D_ASSERT( pthread_equal( thread->thread, pthread_self() ) );

     /* Quick check before calling the pthread function. */
     if (thread->canceled)
          pthread_testcancel();
}

void
direct_thread_join( DirectThread *thread )
{
     D_ASSERT( thread != NULL );
     D_ASSERT( thread->thread != -1 );

     D_ASSUME( !pthread_equal( thread->thread, pthread_self() ) );
     D_ASSUME( !thread->joining );
     D_ASSUME( !thread->joined );

     if (!thread->joining && !pthread_equal( thread->thread, pthread_self() )) {
          thread->joining = true;

          D_DEBUG( "Direct/Thread: Joining %d...\n", thread->tid );

          pthread_join( thread->thread, NULL );

          thread->joined = true;

          D_DEBUG( "Direct/Thread: ...joined %d.\n", thread->tid );
     }
}

bool
direct_thread_is_joined( DirectThread *thread )
{
     D_ASSERT( thread != NULL );

     return thread->joined;
}

void
direct_thread_destroy( DirectThread *thread )
{
     D_ASSERT( thread != NULL );

     D_ASSUME( !pthread_equal( thread->thread, pthread_self() ) );
     D_ASSUME( thread->joined );

     if (!thread->joined && !pthread_equal( thread->thread, pthread_self() )) {
          if (thread->canceled)
               D_BUG( "thread canceled but not joined" );
          else
               D_BUG( "thread still running" );

          D_ERROR( "Direct/Thread: Killing %d!\n", thread->tid );

          pthread_kill( thread->thread, SIGKILL );
     }

     D_FREE( thread );
}

/******************************************************************************/

static void *
direct_thread_main( void *arg )
{
     DirectThread *thread = (DirectThread*) arg;

     thread->tid = direct_gettid();

     /* Have all signals handled by the main thread. */
     direct_signals_block_all();

     /* Adjust scheduling priority. */
     switch (thread->type) {
          case DTT_INPUT:
               setpriority( PRIO_PROCESS, 0, -10 );
               break;

          case DTT_CLEANUP:
               setpriority( PRIO_PROCESS, 0, -5 );
               break;

          case DTT_CRITICAL:
               setpriority( PRIO_PROCESS, 0, -20 );
               break;

          case DTT_MESSAGING:
               setpriority( PRIO_PROCESS, 0, -15 );
               break;

          default:
               break;
     }

#ifdef DIRECT_THREAD_WAIT_INIT
     /* Indicate that our initialization has completed. */
     thread->init = true;

     D_DEBUG( "Direct/Thread:     (thread) Initialization done.\n" );

     sched_yield();
#endif

     if (thread->joining) {
          D_DEBUG( "Direct/Thread: "
                   "    (thread) Being joined before entering main routine.\n" );
          return NULL;
     }

#ifdef DIRECT_THREAD_WAIT_CREATE
     if (thread->thread == -1) {
          D_DEBUG( "Direct/Thread: "
                   "    (thread) Waiting for pthread_create()...\n" );

          /* Wait for completion of pthread_create(). */
          while ((int) thread->thread == -1)
               sched_yield();

          D_DEBUG( "Direct/Thread: "
                   "    (thread) ...pthread_create() finished.\n" );
     }
#endif

     /* Call main routine. */
     return thread->main( thread, thread->arg );
}

