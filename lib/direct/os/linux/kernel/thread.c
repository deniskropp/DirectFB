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



#include <config.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <linux/kthread.h>
#include <linux/sched.h>

D_LOG_DOMAIN( Direct_Thread    , "Direct/Thread",      "Thread management" );
D_LOG_DOMAIN( Direct_ThreadInit, "Direct/Thread/Init", "Thread initialization" );

/**********************************************************************************************************************/
/*
 * Wrapper around kthread's main routine to pass additional arguments
 * and setup things like signal masks and scheduling priorities.
 */
__attribute__((no_instrument_function))
static int direct_thread_main( void *arg );

/**********************************************************************************************************************/

static DirectHash  thread_hash = DIRECT_HASH_INIT( 523, true );
static DirectMutex thread_lock = DIRECT_MUTEX_INITIALIZER(thread_lock);

/**********************************************************************************************************************/

DirectResult
direct_thread_init( DirectThread *thread )
{
     thread->handle.task = kthread_run( direct_thread_main, thread, "%s (%s)",
                                        thread->name, direct_thread_type_name(thread->type) );

     D_INFO( "Direct/Thread: Started '%s' (%d) [%s]...\n",
             thread->name, thread->tid, direct_thread_type_name(thread->type) );

     direct_hash_insert( &thread_hash, (ulong)thread->handle.task, thread );

     return DR_OK;
}

void
direct_thread_deinit( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSUME( thread->handle.task != current );
     D_ASSUME( !thread->detached );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     direct_hash_remove( &thread_hash, (ulong)thread->handle.task );

     if (thread->detached) {
          D_DEBUG_AT( Direct_Thread, "  -> DETACHED!\n" );
          return;
     }

     if (!thread->joined && thread->handle.task != current ) {
          if (thread->canceled)
               D_DEBUG_AT( Direct_Thread, "  -> canceled but not joined!\n" );
          else {
               D_DEBUG_AT( Direct_Thread, "  -> still running!\n" );

               if (thread->name)
                    D_ERROR( "Direct/Thread: Canceling '%s' (%d)!\n", thread->name, thread->tid );
               else
                    D_ERROR( "Direct/Thread: Canceling %d!\n", thread->tid );

               kthread_stop( thread->handle.task );
          }
     }
}

/**********************************************************************************************************************/

DirectThread *
direct_thread_self( void )
{
     DirectThread *thread;

     thread = direct_hash_lookup( &thread_hash, (ulong) current );
     D_MAGIC_ASSERT_IF( thread, DirectThread );

     return thread;
}

__attribute__((no_instrument_function))
const char *
direct_thread_self_name( void )
{
     DirectThread *thread;

     thread = direct_hash_lookup( &thread_hash, (ulong) current );

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

     thread = direct_hash_lookup( &thread_hash, (ulong) current );

     /* Support this function for non-direct threads. */
     if (!thread) {
          D_DEBUG_AT( Direct_Thread, "  -> attaching unknown thread %d\n", direct_gettid() );

          thread = D_CALLOC( 1, sizeof(DirectThread) );
          if (!thread) {
               D_OOM();
               return;
          }

          thread->handle.task = current;
          thread->tid         = direct_gettid();

          D_MAGIC_SET( thread, DirectThread );

          direct_hash_insert( &thread_hash, (ulong)thread->handle.task, thread );
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

void
direct_thread_cancel( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->tid != -1 );
     D_ASSUME( thread->handle.task != current );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     thread->canceled = true;

//     pthread_cancel( thread->thread );
     kthread_stop( thread->handle.task );    // FIXME: kthread_stop() waits...
}

void
direct_thread_detach( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->tid != -1 );
     D_ASSUME( thread->handle.task != current );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     thread->detached = true;

//     pthread_detach( thread->thread );
     D_UNIMPLEMENTED();
}

void
direct_thread_testcancel( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->tid != -1 );
     D_ASSERT( thread->handle.task == current );

     /* Quick check before calling the function. */
     if (thread->canceled)
          kthread_should_stop();
}

void
direct_thread_join( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->tid != -1 );

     D_ASSUME( thread->handle.task != current );
     D_ASSUME( !thread->joining );
     D_ASSUME( !thread->joined );
     D_ASSUME( !thread->detached );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     if (thread->detached) {
          D_DEBUG_AT( Direct_Thread, "  -> DETACHED!\n" );
          return;
     }

     if (!thread->joining && thread->handle.task != current) {
          thread->joining = true;

          D_DEBUG_AT( Direct_Thread, "  -> joining...\n" );

//          pthread_join( thread->thread, NULL );

          // kthread_stop() should have waited already...

          thread->joined = true;

          D_DEBUG_AT( Direct_Thread, "  -> joined.\n" );
     }
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
static int
direct_thread_main( void *arg )
{
     void         *ret;
     DirectThread *thread = arg;


     D_DEBUG_AT( Direct_ThreadInit, "%s( %p )\n", __FUNCTION__, arg );

     D_DEBUG_AT( Direct_ThreadInit, "  -> starting...\n" );

     D_MAGIC_ASSERT( thread, DirectThread );



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
          return -1;
     }

     D_MAGIC_ASSERT( thread, DirectThread );

     /* Call main routine. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> running...\n" );
     ret = thread->main( thread, thread->arg );

     D_DEBUG_AT( Direct_Thread, "  -> Returning %p from '%s' (%s, %d)...\n",
                 ret, thread->name, direct_thread_type_name(thread->type), thread->tid );

     D_MAGIC_ASSERT( thread, DirectThread );

     return 0;
}

