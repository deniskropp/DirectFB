/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <pthread.h>
#include <signal.h>
#include <sched.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/util.h>

D_DEBUG_DOMAIN( Direct_Thread, "Direct/Thread", "Thread management" );


/* FIXME: DIRECT_THREAD_WAIT_INIT is required, but should be optional. */
#define DIRECT_THREAD_WAIT_INIT

/* FIXME: DIRECT_THREAD_WAIT_CREATE is required, but should be optional. */
#define DIRECT_THREAD_WAIT_CREATE


struct __D_DirectThread {
     int                   magic;

     pthread_t             thread;   /* The pthread thread identifier. */
     pid_t                 tid;

     char                 *name;

     DirectThreadType      type;     /* The thread's type, e.g. input thread. */
     DirectThreadMainFunc  main;     /* The thread's main routine (or entry point). */
     void                 *arg;      /* Custom argument passed to the main routine. */

     bool                  canceled; /* Set when direct_thread_cancel() is called. */
     bool                  joining;  /* Set when direct_thread_join() is called. */
     bool                  joined;   /* Set when direct_thread_join() has finished. */

#ifdef DIRECT_THREAD_WAIT_INIT
     bool                  init;     /* Set to true before calling the main routine. */
#endif
};

struct __D_DirectThreadInitHandler {
     DirectLink            link;

     int                   magic;

     DirectThreadInitFunc  func;
     void                 *arg;
};

/******************************************************************************/

/*
 * Wrapper around pthread's main routine to pass additional arguments
 * and setup things like signal masks and scheduling priorities.
 */
static void *direct_thread_main( void *arg );

static const char *thread_type_name( DirectThreadType type )  D_CONST_FUNC;

/******************************************************************************/

static pthread_mutex_t  handler_lock = PTHREAD_MUTEX_INITIALIZER;
static DirectLink      *handlers     = NULL;

/******************************************************************************/

DirectThreadInitHandler *
direct_thread_add_init_handler( DirectThreadInitFunc  func,
                                void                 *arg )
{
     DirectThreadInitHandler *handler;

     handler = D_CALLOC( 1, sizeof(DirectThreadInitHandler) );
     if (!handler) {
          D_WARN( "out of memory" );
          return NULL;
     }

     handler->func = func;
     handler->arg  = arg;

     D_MAGIC_SET( handler, DirectThreadInitHandler );

     pthread_mutex_lock( &handler_lock );

     direct_list_append( &handlers, &handler->link );

     pthread_mutex_unlock( &handler_lock );

     return handler;
}

void
direct_thread_remove_init_handler( DirectThreadInitHandler *handler )
{
     D_MAGIC_ASSERT( handler, DirectThreadInitHandler );

     pthread_mutex_lock( &handler_lock );

     direct_list_remove( &handlers, &handler->link );

     pthread_mutex_unlock( &handler_lock );

     D_MAGIC_CLEAR( handler );

     D_FREE( handler );
}

/******************************************************************************/

static pthread_mutex_t key_lock   = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t   thread_key = -1;

/******************************************************************************/

DirectThread *
direct_thread_create( DirectThreadType      thread_type,
                      DirectThreadMainFunc  thread_main,
                      void                 *arg,
                      const char           *name )
{
     DirectThread *thread;

     D_ASSERT( thread_main != NULL );
     D_ASSERT( name != NULL );

     D_HEAVYDEBUG( "Direct/Thread: Creating '%s' (type %d)...\n", name, thread_type );

     (void) thread_type_name;

     /* Create the key for the TSD (thread specific data). */
     pthread_mutex_lock( &key_lock );

     if (thread_key == -1)
          pthread_key_create( &thread_key, NULL );

     pthread_mutex_unlock( &key_lock );

     /* Allocate thread structure. */
     thread = D_CALLOC( 1, sizeof(DirectThread) );
     if (!thread) {
          D_OOM();
          return NULL;
     }

     /* Write thread information to structure. */
     thread->name = D_STRDUP( name );
     thread->type = thread_type;
     thread->main = thread_main;
     thread->arg  = arg;

     /* Initialize to -1 for synchronization. */
     thread->thread = (pthread_t) -1;
     thread->tid    = (pid_t) -1;

     D_MAGIC_SET( thread, DirectThread );

     /* Create and run the thread. */
     pthread_create( &thread->thread, NULL, direct_thread_main, thread );

#ifdef DIRECT_THREAD_WAIT_INIT
     D_HEAVYDEBUG( "Direct/Thread: Waiting for thread to run...\n" );

     /* Wait for completion of the thread's initialization. */
     while (!thread->init)
          sched_yield();

     D_HEAVYDEBUG( "Direct/Thread: ...thread is running.\n" );
#endif

     D_INFO( "Direct/Thread: Running '%s' (%s, %d)...\n",
             name, thread_type_name(thread_type), thread->tid );

     return thread;
}

DirectThread *
direct_thread_self()
{
     DirectThread *thread = pthread_getspecific( thread_key );

     if (thread)
          D_MAGIC_ASSERT( thread, DirectThread );

     return thread;
}

const char *
direct_thread_get_name( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->name != NULL );

     return thread->name;
}

__attribute__((no_instrument_function))
const char *
direct_thread_self_name()
{
     DirectThread *thread = pthread_getspecific( thread_key );

     /*
      * This function is called by debugging functions, e.g. debug messages, assertions etc.
      * Therefore no assertions are made here, because they would loop forever if they fail.
      */

     return thread ? thread->name : NULL;
}

void
direct_thread_set_name( const char *name )
{
     char         *copy;
     DirectThread *thread = pthread_getspecific( thread_key );

     /* Support this function for non-direct threads. */
     if (!thread) {
          /* Create the key for the TSD (thread specific data). */
          pthread_mutex_lock( &key_lock );

          if (thread_key == -1)
               pthread_key_create( &thread_key, NULL );

          pthread_mutex_unlock( &key_lock );


          thread = D_CALLOC( 1, sizeof(DirectThread) );
          if (!thread) {
               D_OOM();
               return;
          }

          thread->thread = pthread_self();
          thread->tid    = direct_gettid();

          pthread_setspecific( thread_key, thread );
     }

     /* Duplicate string. */
     copy = D_STRDUP( name );
     if (!copy) {
          D_OOM();
          return;
     }

     /* Free old string. */
     if (thread->name)
          D_FREE( thread->name );

     /* Keep the copy. */
     thread->name = copy;
}

void
direct_thread_cancel( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );
     D_ASSERT( !pthread_equal( thread->thread, pthread_self() ) );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "Canceling %d.\n", thread->tid );

     thread->canceled = true;

     pthread_cancel( thread->thread );
}

bool
direct_thread_is_canceled( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );

     return thread->canceled;
}

void
direct_thread_testcancel( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );
     D_ASSERT( pthread_equal( thread->thread, pthread_self() ) );

     /* Quick check before calling the pthread function. */
     if (thread->canceled)
          pthread_testcancel();
}

void
direct_thread_join( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );

     D_ASSUME( !pthread_equal( thread->thread, pthread_self() ) );
     D_ASSUME( !thread->joining );
     D_ASSUME( !thread->joined );

     if (!thread->joining && !pthread_equal( thread->thread, pthread_self() )) {
          thread->joining = true;

          D_HEAVYDEBUG( "Direct/Thread: Joining %d...\n", thread->tid );

          pthread_join( thread->thread, NULL );

          thread->joined = true;

          D_HEAVYDEBUG( "Direct/Thread: ...joined %d.\n", thread->tid );
     }
}

bool
direct_thread_is_joined( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );

     return thread->joined;
}

void
direct_thread_destroy( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );

     D_ASSUME( !pthread_equal( thread->thread, pthread_self() ) );
     //D_ASSUME( thread->joined );

     if (!thread->joined && !pthread_equal( thread->thread, pthread_self() )) {
          if (thread->canceled)
               D_DEBUG_AT( Direct_Thread, "'%s' (%d) is canceled but not joined!\n",
                           thread->name, thread->tid );
          else {
               D_DEBUG_AT( Direct_Thread, "'%s' (%d) is still running!\n",
                           thread->name, thread->tid );

               if (thread->name)
                    D_ERROR( "Direct/Thread: Canceling '%s' (%d)!\n", thread->name, thread->tid );
               else
                    D_ERROR( "Direct/Thread: Canceling %d!\n", thread->tid );

               pthread_cancel( thread->thread );
          }
     }

     D_MAGIC_CLEAR( thread );

     D_FREE( thread->name );
     D_FREE( thread );
}

/******************************************************************************/

static const char *
thread_type_name( DirectThreadType type )
{
     switch (type) {
          case DTT_DEFAULT:
               return "DEFAULT";

          case DTT_CLEANUP:
               return "CLEANUP";

          case DTT_INPUT:
               return "INPUT";

          case DTT_OUTPUT:
               return "OUTPUT";

          case DTT_MESSAGING:
               return "MESSAGING";

          case DTT_CRITICAL:
               return "CRITICAL";
     }

     return "unknown type!";
}

/******************************************************************************/

__attribute__((no_instrument_function))
static void *
direct_thread_main( void *arg )
{
     void                    *ret;
     DirectThread            *thread = arg;
     DirectThreadInitHandler *handler;

     D_MAGIC_ASSERT( thread, DirectThread );

     pthread_setspecific( thread_key, thread );

     thread->tid = direct_gettid();


     /* Call all init handlers. */
     pthread_mutex_lock( &handler_lock );

     direct_list_foreach (handler, handlers)
          handler->func( thread, handler->arg );

     pthread_mutex_unlock( &handler_lock );


     /* Have all signals handled by the main thread. */
     direct_signals_block_all();

     /* Adjust scheduling priority. */
     switch (thread->type) {
          case DTT_CLEANUP:
          case DTT_INPUT:
          case DTT_OUTPUT:
          case DTT_MESSAGING:
          case DTT_CRITICAL:
               setpriority( PRIO_PROCESS, 0, thread->type );
               break;

          default:
               break;
     }

#ifdef DIRECT_THREAD_WAIT_INIT
     /* Indicate that our initialization has completed. */
     thread->init = true;

     D_HEAVYDEBUG( "Direct/Thread:   (thread) Initialization done.\n" );

     sched_yield();
#endif

     D_MAGIC_ASSERT( thread, DirectThread );

     if (thread->joining) {
          D_HEAVYDEBUG( "Direct/Thread:   (thread) Being joined before entering main routine.\n" );
          return NULL;
     }

#ifdef DIRECT_THREAD_WAIT_CREATE
     if (thread->thread == -1) {
          D_HEAVYDEBUG( "Direct/Thread:   (thread) Waiting for pthread_create()...\n" );

          /* Wait for completion of pthread_create(). */
          while ((int) thread->thread == -1)
               sched_yield();

          D_HEAVYDEBUG( "Direct/Thread:   (thread) ...pthread_create() finished.\n" );
     }
#endif

     D_MAGIC_ASSERT( thread, DirectThread );

     /* Call main routine. */
     ret = thread->main( thread, thread->arg );

     D_DEBUG_AT( Direct_Thread, "Returning %p from '%s' (%s, %d)...\n",
                 ret, thread->name, thread_type_name(thread->type), thread->tid );

     D_MAGIC_ASSERT( thread, DirectThread );

     return ret;
}

