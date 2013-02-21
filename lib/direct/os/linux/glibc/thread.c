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

#include <config.h>

#include <signal.h>
#include <sched.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/prctl.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/signals.h>
#include <direct/thread.h>


D_LOG_DOMAIN( Direct_Thread    , "Direct/Thread",      "Thread management" );
D_LOG_DOMAIN( Direct_ThreadInit, "Direct/Thread/Init", "Thread initialization" );

/**********************************************************************************************************************/
/*
 * Wrapper around pthread's main routine to pass additional arguments
 * and setup things like signal masks and scheduling priorities.
 */
__attribute__((no_instrument_function))
static void *direct_thread_main( void *arg );

/**********************************************************************************************************************/

static pthread_key_t thread_key;
static DirectOnce    thread_init_once = DIRECT_ONCE_INIT;

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
static void
init_once( void )
{
     /* Create the key for the TSD (thread specific data). */
     pthread_key_create( &thread_key, NULL );
}

/**********************************************************************************************************************/

DirectResult
direct_thread_init( DirectThread *thread )
{
     pthread_attr_t     attr;
     struct sched_param param;
     int                policy;
     int                priority;

     direct_once( &thread_init_once, init_once );

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


     if (pthread_create( &thread->handle.thread, &attr, direct_thread_main, thread ))
          return errno2result( errno );

     pthread_attr_destroy( &attr );

     /* Read (back) value. */
     pthread_getattr_np( thread->handle.thread, &attr );
     pthread_attr_getstacksize( &attr, &thread->stack_size );
     pthread_attr_getschedparam( &attr, &param );
     thread->priority = param.sched_priority;
     pthread_attr_destroy( &attr );

     return DR_OK;
}

void
direct_thread_deinit( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSUME( !pthread_equal( thread->handle.thread, pthread_self() ) );
     D_ASSUME( !thread->detached );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     if (thread->detached) {
          D_DEBUG_AT( Direct_Thread, "  -> DETACHED!\n" );
          return;
     }

     if (!thread->joined && !pthread_equal( thread->handle.thread, pthread_self() )) {
          if (thread->canceled)
               D_DEBUG_AT( Direct_Thread, "  -> cancled but not joined!\n" );
          else {
               D_DEBUG_AT( Direct_Thread, "  -> still running!\n" );

               if (thread->name)
                    D_ERROR( "Direct/Thread: Canceling '%s' (%d)!\n", thread->name, thread->tid );
               else
                    D_ERROR( "Direct/Thread: Canceling %d!\n", thread->tid );

#ifndef DIRECT_BUILD_NO_PTHREAD_CANCEL
               pthread_cancel( thread->handle.thread );
#endif
          }

          pthread_join( thread->handle.thread, NULL );
     }
}

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
DirectThread *
direct_thread_self( void )
{
     DirectThread *thread;

     direct_once( &thread_init_once, init_once );

     thread = pthread_getspecific( thread_key );
//     D_MAGIC_ASSERT_IF( thread, DirectThread );

     /* Support this function for non-direct threads. */
     if (!thread) {
//          D_DEBUG_AT( Direct_Thread, "  -> attaching unknown thread %d\n", direct_gettid() );

          thread = direct_calloc( 1, sizeof(DirectThread) );
          if (!thread) {
               D_OOM();
               return NULL;
          }

          thread->handle.thread = pthread_self();
          thread->tid           = direct_gettid();

          D_MAGIC_SET( thread, DirectThread );

          pthread_setspecific( thread_key, thread );
     }

     return thread;
}

__attribute__((no_instrument_function))
const char *
direct_thread_self_name( void )
{
     DirectThread *thread = direct_thread_self();

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
     DirectThread *thread;

     D_DEBUG_AT( Direct_Thread, "%s( '%s' )\n", __FUNCTION__, name );

     thread = direct_thread_self();
     if (!thread)
          return;

     /* Duplicate string. */
     copy = D_STRDUP( name );
     if (!copy) {
          D_OOM();
          return;
     }

     /* Free old string. */
     if (thread->name)
          direct_free( thread->name );

     /* Keep the copy. */
     thread->name = copy;
}

void
direct_thread_cancel( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->handle.thread != -1 );
     D_ASSERT( !pthread_equal( thread->handle.thread, pthread_self() ) );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     thread->canceled = true;
#ifndef DIRECT_BUILD_NO_PTHREAD_CANCEL
     pthread_cancel( thread->handle.thread );
#else
     D_UNIMPLEMENTED();
#endif
}

void
direct_thread_detach( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->handle.thread != -1 );
     D_ASSERT( !pthread_equal( thread->handle.thread, pthread_self() ) );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     thread->detached = true;

     pthread_detach( thread->handle.thread );
}

void
direct_thread_testcancel( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->handle.thread != -1 );
     D_ASSERT( pthread_equal( thread->handle.thread, pthread_self() ) );

     /* Quick check before calling the pthread function. */
#ifndef DIRECT_BUILD_NO_PTHREAD_CANCEL
     if (thread->canceled)
          pthread_testcancel();
#else
     D_UNIMPLEMENTED();
#endif
}

void
direct_thread_join( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->handle.thread != -1 );

     D_ASSUME( !pthread_equal( thread->handle.thread, pthread_self() ) );
     D_ASSUME( !thread->joining );
     D_ASSUME( !thread->joined );
     D_ASSUME( !thread->detached );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     if (thread->detached) {
          D_DEBUG_AT( Direct_Thread, "  -> DETACHED!\n" );
          return;
     }

     if (!thread->joining && !pthread_equal( thread->handle.thread, pthread_self() )) {
          thread->joining = true;

          D_DEBUG_AT( Direct_Thread, "  -> joining...\n" );

          pthread_join( thread->handle.thread, NULL );

          thread->joined = true;

          D_DEBUG_AT( Direct_Thread, "  -> joined.\n" );
     }
}

void
direct_thread_sleep( long long micros )
{
     usleep( micros );
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

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

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
static void *
direct_thread_main( void *arg )
{
     void                    *ret;
     DirectThread            *thread = arg;

     prctl( PR_SET_NAME, thread->name, 0, 0, 0 );

     pthread_setspecific( thread_key, thread );


     D_DEBUG_AT( Direct_ThreadInit, "%s( %p )\n", __FUNCTION__, arg );

     D_DEBUG_AT( Direct_ThreadInit, "  -> starting...\n" );

     D_MAGIC_ASSERT( thread, DirectThread );

     pthread_cleanup_push( direct_thread_cleanup, thread );



     thread->tid = direct_gettid();

     D_DEBUG_AT( Direct_ThreadInit, " -> tid %d\n", thread->tid );


     __D_direct_thread_call_init_handlers( thread );


     /* Have all signals handled by the main thread. */
     if (direct_config->thread_block_signals)
          direct_signals_block_all();

     /* Lock the thread mutex. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> locking...\n" );
     direct_mutex_lock( &thread->lock );

     /* Indicate that our initialization has completed. */
     D_ASSERT( !thread->init );
     thread->init = true;

     D_DEBUG_AT( Direct_ThreadInit, "  -> signalling...\n" );
     direct_waitqueue_signal( &thread->cond );

     /* Unlock the thread mutex. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> unlocking...\n" );
     direct_mutex_unlock( &thread->lock );

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

