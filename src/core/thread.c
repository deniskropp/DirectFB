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

#include <core/core.h>
#include <core/sig.h>
#include <core/system.h>
#include <core/thread.h>

#include <misc/mem.h>


/* FIXME: DFB_THREAD_WAIT_INIT is required, but should be optional. */
#define DFB_THREAD_WAIT_INIT

/* FIXME: DFB_THREAD_WAIT_CREATE is required, but should be optional. */
#define DFB_THREAD_WAIT_CREATE


struct _CoreThread {
     pthread_t       thread;   /* The pthread thread identifier. */
     pid_t           tid;

     CoreThreadType  type;     /* The thread's type, e.g. input thread. */
     CoreThreadMain  main;     /* The thread's main routine (or entry point). */
     void           *arg;      /* Custom argument passed to the main routine. */

     bool            canceled; /* Set when dfb_thread_cancel() is called. */
     bool            joining;  /* Set when dfb_thread_join() is called. */
     bool            joined;   /* Set when dfb_thread_join() has finished. */

#ifdef DFB_THREAD_WAIT_INIT
     bool            init;     /* Set to true before calling the main routine. */
#endif
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

     DEBUGMSG( "DirectFB/core/threads: Creating new thread of type %d...\n",
               thread_type );

     /* Allocate core thread structure. */
     thread = DFBCALLOC( 1, sizeof(CoreThread) );
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
     pthread_create( &thread->thread, NULL, dfb_thread_main, thread );

#ifdef DFB_THREAD_WAIT_INIT
     DEBUGMSG( "DirectFB/core/threads: Waiting for thread to run...\n" );

     /* Wait for completion of the thread's initialization. */
     while (!thread->init)
          sched_yield();

     DEBUGMSG( "DirectFB/core/threads: ...thread is running.\n" );
#endif

     DEBUGMSG( "DirectFB/core/threads: ...created thread of type %d (%d).\n",
               thread_type, thread->tid );

     return thread;
}

void
dfb_thread_cancel( CoreThread *thread )
{
     DFB_ASSERT( thread != NULL );
     DFB_ASSERT( thread->thread != -1 );
     DFB_ASSERT( !pthread_equal( thread->thread, pthread_self() ) );

     DFB_ASSUME( !thread->canceled );

     DEBUGMSG( "DirectFB/core/threads: Canceling %d.\n", thread->tid );

     thread->canceled = true;

     pthread_cancel( thread->thread );
}

bool
dfb_thread_is_canceled( CoreThread *thread )
{
     DFB_ASSERT( thread != NULL );

     return thread->canceled;
}

void
dfb_thread_testcancel( CoreThread *thread )
{
     DFB_ASSERT( thread != NULL );
     DFB_ASSERT( thread->thread != -1 );
     DFB_ASSERT( pthread_equal( thread->thread, pthread_self() ) );

     /* Quick check before calling the pthread function. */
     if (thread->canceled)
          pthread_testcancel();
}

void
dfb_thread_join( CoreThread *thread )
{
     DFB_ASSERT( thread != NULL );
     DFB_ASSERT( thread->thread != -1 );

     DFB_ASSUME( !pthread_equal( thread->thread, pthread_self() ) );
     DFB_ASSUME( !thread->joining );
     DFB_ASSUME( !thread->joined );

     if (!thread->joining && !pthread_equal( thread->thread, pthread_self() )) {
          thread->joining = true;

          DEBUGMSG( "DirectFB/core/threads: Joining %d...\n", thread->tid );

          pthread_join( thread->thread, NULL );

          thread->joined = true;

          DEBUGMSG( "DirectFB/core/threads: ...joined %d.\n", thread->tid );
     }
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

     DFB_ASSUME( !pthread_equal( thread->thread, pthread_self() ) );
     DFB_ASSUME( thread->joined );

     if (!thread->joined && !pthread_equal( thread->thread, pthread_self() )) {
          if (thread->canceled)
               BUG("thread canceled but not joined");
          else
               BUG("thread still running");

          ERRORMSG( "DirectFB/core/threads: Killing %d!\n", thread->tid );

          pthread_kill( thread->thread, SIGKILL );
     }

     DFBFREE( thread );
}

/******************************************************************************/

static void *
dfb_thread_main( void *arg )
{
     CoreThread *thread = (CoreThread*) arg;

     thread->tid = gettid();

     dfb_system_thread_init();

     /* Have all signals handled by the main thread. */
     dfb_sig_block_all();

     /* Adjust scheduling priority. */
     switch (thread->type) {
          case CTT_INPUT:
               setpriority( PRIO_PROCESS, 0, -10 );
               break;

          case CTT_CLEANUP:
               setpriority( PRIO_PROCESS, 0, -5 );
               break;

          case CTT_CRITICAL:
               setpriority( PRIO_PROCESS, 0, -20 );
               break;

          case CTT_MESSAGING:
               setpriority( PRIO_PROCESS, 0, -15 );
               break;

          default:
               break;
     }

#ifdef DFB_THREAD_WAIT_INIT
     /* Indicate that our initialization has completed. */
     thread->init = true;

     DEBUGMSG( "DirectFB/core/threads:     (thread) Initialization done.\n" );

     sched_yield();
#endif

     if (thread->joining) {
          DEBUGMSG( "DirectFB/core/threads: "
                    "    (thread) Being joined before entering main routine.\n" );
          return NULL;
     }

#ifdef DFB_THREAD_WAIT_CREATE
     if (thread->thread == -1) {
          DEBUGMSG( "DirectFB/core/threads: "
                    "    (thread) Waiting for pthread_create()...\n" );

          /* Wait for completion of pthread_create(). */
          while ((int) thread->thread == -1)
               sched_yield();

          DEBUGMSG( "DirectFB/core/threads: "
                    "    (thread) ...pthread_create() finished.\n" );
     }
#endif

     /* Call main routine. */
     return thread->main( thread, thread->arg );
}

