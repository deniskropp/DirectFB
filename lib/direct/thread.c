/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

D_DEBUG_DOMAIN( Direct_Thread,     "Direct/Thread",      "Thread management" );
D_DEBUG_DOMAIN( Direct_ThreadInit, "Direct/Thread/Init", "Thread initialization" );


/* FIXME: DIRECT_THREAD_WAIT_INIT is required, but should be optional. */
#define DIRECT_THREAD_WAIT_INIT


struct __D_DirectThread {
     int                   magic;

     pthread_t             thread;      /* The pthread thread identifier. */
     pid_t                 tid;

     char                 *name;

     DirectThreadType      type;        /* The thread's type, e.g. input thread. */
     DirectThreadMainFunc  main;        /* The thread's main routine (or entry point). */
     void                 *arg;         /* Custom argument passed to the main routine. */

     bool                  canceled;    /* Set when direct_thread_cancel() is called. */
     bool                  joining;     /* Set when direct_thread_join() is called. */
     bool                  joined;      /* Set when direct_thread_join() has finished. */
     bool                  detached;    /* Set when direct_thread_detach() is called. */
     bool                  terminated;  /* Set when direct_thread_terminate() is called. */

#ifdef DIRECT_THREAD_WAIT_INIT
     bool                  init;        /* Set to true before calling the main routine. */
#endif

     pthread_mutex_t       lock;
     pthread_cond_t        cond;

     unsigned int          counter;
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
     DirectThread       *thread;
     pthread_attr_t      attr;
     struct sched_param  param;
     int                 policy;
     int                 priority;
     size_t              stack_size;

     D_ASSERT( thread_main != NULL );
     D_ASSERT( name != NULL );

     D_DEBUG_AT( Direct_Thread, "%s( %s, %p(%p), '%s' )\n", __FUNCTION__,
                 direct_thread_type_name(thread_type), thread_main, arg, name );

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

     /* Initialize mutex and condition. */
     direct_util_recursive_pthread_mutex_init( &thread->lock );
     pthread_cond_init( &thread->cond, NULL );

     D_MAGIC_SET( thread, DirectThread );

     /* Initialize scheduling and other parameters. */
     pthread_attr_init( &attr );

#ifdef PTHREAD_EXPLICIT_SCHED
     pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );
#endif

     /* Select scheduler. */
     switch (direct_config->thread_scheduler) {
          case DCTS_FIFO:
               policy = SCHED_FIFO;
               break;

          case DCTS_RR:
               policy = SCHED_RR;
               break;

          default:
               policy = SCHED_OTHER;
               break;
     }

     if (pthread_attr_setschedpolicy( &attr, policy ))
          D_PERROR( "Direct/Thread: Could not set scheduling policy to %s!\n", direct_thread_policy_name(policy) );

     /* Read (back) value. */
     pthread_attr_getschedpolicy( &attr, &policy );

     /* Select priority. */
     switch (thread->type) {
          case DTT_CLEANUP:
          case DTT_INPUT:
          case DTT_OUTPUT:
          case DTT_MESSAGING:
          case DTT_CRITICAL:
               priority = thread->type * direct_config->thread_priority_scale / 100;
               break;

          default:
               priority = direct_config->thread_priority;
               break;
     }

     D_DEBUG_AT( Direct_ThreadInit, "  -> %s (%d) [%d;%d]\n", direct_thread_policy_name(policy), priority,
                 sched_get_priority_min( policy ), sched_get_priority_max( policy ) );

     if (priority < sched_get_priority_min( policy ))
          priority = sched_get_priority_min( policy );

     if (priority > sched_get_priority_max( policy ))
          priority = sched_get_priority_max( policy );

     param.sched_priority = priority;

     if (pthread_attr_setschedparam( &attr, &param ))
          D_PERROR( "Direct/Thread: Could not set scheduling priority to %d!\n", priority );

     /* Select stack size? */
     if (direct_config->thread_stack_size > 0) {
          if (pthread_attr_setstacksize( &attr, direct_config->thread_stack_size ))
               D_PERROR( "Direct/Thread: Could not set stack size to %d!\n", direct_config->thread_stack_size );
     }

     /* Read (back) value. */
     pthread_attr_getstacksize( &attr, &stack_size );

     /* Lock the thread mutex. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> locking...\n" );
     pthread_mutex_lock( &thread->lock );

     /* Create and run the thread. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> creating...\n" );
     pthread_create( &thread->thread, &attr, direct_thread_main, thread );

     pthread_attr_destroy( &attr );

     pthread_getschedparam( thread->thread, &policy, &param );

     D_INFO( "Direct/Thread: Started '%s' (%d) [%s %s/%s %d/%d] <%zu>...\n",
             name, thread->tid, direct_thread_type_name(thread_type),
             direct_thread_policy_name(policy), direct_thread_scheduler_name(direct_config->thread_scheduler),
             param.sched_priority, priority, stack_size );

#ifdef DIRECT_THREAD_WAIT_INIT
     /* Wait for completion of the thread's initialization. */
     while (!thread->init) {
          D_DEBUG_AT( Direct_ThreadInit, "  -> waiting...\n" );
          pthread_cond_wait( &thread->cond, &thread->lock );
     }

     D_DEBUG_AT( Direct_ThreadInit, "  -> ...thread is running.\n" );
#endif

     /* Unlock the thread mutex. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> unlocking...\n" );
     pthread_mutex_unlock( &thread->lock );

     D_DEBUG_AT( Direct_ThreadInit, "  -> returning %p\n", thread );

     return thread;
}

DirectThread *
direct_thread_self( void )
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

pid_t
direct_thread_get_tid( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );

     return thread->tid;
}

__attribute__((no_instrument_function))
const char *
direct_thread_self_name( void )
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

     D_DEBUG_AT( Direct_Thread, "%s( '%s' )\n", __FUNCTION__, name );

     /* Support this function for non-direct threads. */
     if (!thread) {
          D_DEBUG_AT( Direct_Thread, "  -> attaching unknown thread %d\n", direct_gettid() );

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

          D_MAGIC_SET( thread, DirectThread );

          pthread_setspecific( thread_key, thread );
     }
     else
          D_DEBUG_AT( Direct_Thread, "  -> was '%s' (%d)\n", thread->name, direct_gettid() );

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

DirectResult
direct_thread_wait( DirectThread *thread, int timeout_ms )
{
     unsigned int old_counter = thread->counter;

     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d, %dms )\n", __FUNCTION__,
                 thread->main, thread->name, thread->tid, timeout_ms );

     while (old_counter == thread->counter && !thread->terminated)
          pthread_cond_wait( &thread->cond, &thread->lock );

     if (thread->terminated)
          return DR_DEAD;

     return DR_OK;
}

void
direct_thread_notify( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     pthread_mutex_lock( &thread->lock );

     thread->counter++;

     pthread_mutex_unlock( &thread->lock );

     pthread_cond_broadcast( &thread->cond );
}

void
direct_thread_lock( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     pthread_mutex_lock( &thread->lock );
}

void
direct_thread_unlock( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     pthread_mutex_unlock( &thread->lock );
}

void
direct_thread_terminate( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );
     D_ASSUME( !pthread_equal( thread->thread, pthread_self() ) );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     thread->terminated = true;

     direct_thread_notify( thread );
}

void
direct_thread_cancel( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );
     D_ASSERT( !pthread_equal( thread->thread, pthread_self() ) );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     thread->canceled = true;

#if DIRECT_BUILD_NO_PTHREAD_CANCEL
     D_UNIMPLEMENTED();
#else
     pthread_cancel( thread->thread );
#endif
}

bool
direct_thread_is_canceled( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );

     return thread->canceled;
}

void
direct_thread_detach( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );
     D_ASSERT( !pthread_equal( thread->thread, pthread_self() ) );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     thread->detached = true;

     pthread_detach( thread->thread );
}

bool
direct_thread_is_detached( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );

     return thread->detached;
}

void
direct_thread_testcancel( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );
     D_ASSERT( pthread_equal( thread->thread, pthread_self() ) );

#if DIRECT_BUILD_NO_PTHREAD_CANCEL
     D_UNIMPLEMENTED();
#else
     /* Quick check before calling the pthread function. */
     if (thread->canceled)
          pthread_testcancel();
#endif
}

void
direct_thread_join( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->thread != -1 );

     D_ASSUME( !pthread_equal( thread->thread, pthread_self() ) );
     D_ASSUME( !thread->joining );
     D_ASSUME( !thread->joined );
     D_ASSUME( !thread->detached );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     if (thread->detached) {
          D_DEBUG_AT( Direct_Thread, "  -> DETACHED!\n" );
          return;
     }

     if (!thread->joining && !pthread_equal( thread->thread, pthread_self() )) {
          thread->joining = true;

          D_DEBUG_AT( Direct_Thread, "  -> joining...\n" );

          pthread_join( thread->thread, NULL );

          thread->joined = true;

          D_DEBUG_AT( Direct_Thread, "  -> joined.\n" );
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
     D_ASSUME( !thread->detached );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     if (thread->detached) {
          D_DEBUG_AT( Direct_Thread, "  -> DETACHED!\n" );
          return;
     }

     if (!thread->joined && !pthread_equal( thread->thread, pthread_self() )) {
          if (thread->canceled)
               D_DEBUG_AT( Direct_Thread, "  -> cancled but not joined!\n" );
          else {
               D_DEBUG_AT( Direct_Thread, "  -> still running!\n" );

               if (thread->name)
                    D_ERROR( "Direct/Thread: Canceling '%s' (%d)!\n", thread->name, thread->tid );
               else
                    D_ERROR( "Direct/Thread: Canceling %d!\n", thread->tid );

               thread->detached = true;

               pthread_detach( thread->thread );

               direct_thread_cancel( thread );

               return;
          }
     }

     D_MAGIC_CLEAR( thread );

     D_FREE( thread->name );
     D_FREE( thread );
}

/******************************************************************************/

#if DIRECT_BUILD_TEXT
const char *
direct_thread_type_name( DirectThreadType type )
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

     return "<unknown>";
}

const char *
direct_thread_scheduler_name( DirectConfigThreadScheduler scheduler )
{
     switch (scheduler) {
          case DCTS_OTHER:
               return "OTHER";

          case DCTS_FIFO:
               return "FIFO";

          case DCTS_RR:
               return "RR";
     }

     return "<unknown>";
}

const char *
direct_thread_policy_name( int policy )
{
     switch (policy) {
          case SCHED_OTHER:
               return "OTHER";

          case SCHED_FIFO:
               return "FIFO";

          case SCHED_RR:
               return "RR";
     }

     return "<unknown>";
}
#endif

/******************************************************************************/

static void
direct_thread_cleanup( void *arg )
{
     DirectThread *thread = arg;

     D_MAGIC_ASSERT( thread, DirectThread );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     if (thread->detached) {
          D_MAGIC_CLEAR( thread );

          D_FREE( thread->name );
          D_FREE( thread );
     }
}

/******************************************************************************/

static void *
direct_thread_main( void *arg )
{
     void                    *ret;
     DirectThread            *thread = arg;
     DirectThreadInitHandler *handler;
     pid_t                    tid;

     tid = direct_gettid();

     D_DEBUG_AT( Direct_ThreadInit, "%s( %p ) <- tid %d\n", __FUNCTION__, arg, tid );

     D_DEBUG_AT( Direct_ThreadInit, "  -> starting...\n" );

     D_MAGIC_ASSERT( thread, DirectThread );

     pthread_cleanup_push( direct_thread_cleanup, thread );


     pthread_setspecific( thread_key, thread );

     thread->tid = tid;


     /* Call all init handlers. */
     pthread_mutex_lock( &handler_lock );

     direct_list_foreach (handler, handlers)
          handler->func( thread, handler->arg );

     pthread_mutex_unlock( &handler_lock );


     /* Have all signals handled by the main thread. */
     if (direct_config->thread_block_signals)
          direct_signals_block_all();

     /* Lock the thread mutex. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> locking...\n" );
     pthread_mutex_lock( &thread->lock );

     /* Indicate that our initialization has completed. */
     thread->init = true;

#ifdef DIRECT_THREAD_WAIT_INIT
     D_DEBUG_AT( Direct_ThreadInit, "  -> signalling...\n" );
     pthread_cond_signal( &thread->cond );
#endif

     /* Unlock the thread mutex. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> unlocking...\n" );
     pthread_mutex_unlock( &thread->lock );

     if (thread->joining) {
          D_DEBUG_AT( Direct_Thread, "  -> Being joined before entering main routine!\n" );
          return NULL;
     }

     D_MAGIC_ASSERT( thread, DirectThread );

     /* Call main routine. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> running...\n" );
     ret = thread->main( thread, thread->arg );

     D_DEBUG_AT( Direct_Thread, "  -> Returning %p from '%s' (%s, %d)...\n",
                 ret, thread->name, direct_thread_type_name(thread->type), thread->tid );

     D_MAGIC_ASSERT( thread, DirectThread );

     pthread_cleanup_pop( 1 );

     return ret;
}

