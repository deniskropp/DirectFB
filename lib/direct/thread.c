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

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/util.h>


D_LOG_DOMAIN( Direct_Thread    , "Direct/Thread",      "Thread management" );
D_LOG_DOMAIN( Direct_ThreadInit, "Direct/Thread/Init", "Thread initialization" );

/**********************************************************************************************************************/

struct __D_DirectThreadInitHandler {
     DirectLink            link;

     int                   magic;

     DirectThreadInitFunc  func;
     void                 *arg;
};

/**********************************************************************************************************************/

static DirectMutex  handler_lock;
static DirectLink  *handlers;

void
__D_thread_init()
{
     direct_mutex_init( &handler_lock );
}

void
__D_thread_deinit()
{
     direct_mutex_deinit( &handler_lock );
}

/**********************************************************************************************************************/

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

     direct_mutex_lock( &handler_lock );

     direct_list_append( &handlers, &handler->link );

     direct_mutex_unlock( &handler_lock );

     return handler;
}

void
direct_thread_remove_init_handler( DirectThreadInitHandler *handler )
{
     D_MAGIC_ASSERT( handler, DirectThreadInitHandler );

     direct_mutex_lock( &handler_lock );

     direct_list_remove( &handlers, &handler->link );

     direct_mutex_unlock( &handler_lock );

     D_MAGIC_CLEAR( handler );

     D_FREE( handler );
}

void
__D_direct_thread_call_init_handlers( DirectThread *thread )
{
     DirectThreadInitHandler *handler;

     /* Call all init handlers. */
     direct_mutex_lock( &handler_lock );

     direct_list_foreach (handler, handlers)
          handler->func( thread, handler->arg );

     direct_mutex_unlock( &handler_lock );
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

DirectThread *
direct_thread_create( DirectThreadType      thread_type,
                      DirectThreadMainFunc  thread_main,
                      void                 *arg,
                      const char           *name )
{
     DirectThread *thread;

     D_ASSERT( thread_main != NULL );
     D_ASSERT( name != NULL );

     D_DEBUG_AT( Direct_Thread, "%s( %s, %p(%p), '%s' )\n", __FUNCTION__,
                 direct_thread_type_name(thread_type), thread_main, arg, name );


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
     thread->tid  = (pid_t) -1;

     /* Initialize mutex and condition. */
     direct_recursive_mutex_init( &thread->lock );
     direct_waitqueue_init( &thread->cond );

     D_MAGIC_SET( thread, DirectThread );



     /* Lock the thread mutex. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> locking...\n" );
     direct_mutex_lock( &thread->lock );


     /* Create and run the thread. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> creating handle...\n" );
     direct_thread_init( thread );


     /* Wait for completion of the thread's initialization. */
     while (!thread->init) {
          D_DEBUG_AT( Direct_ThreadInit, "  -> waiting...\n" );
          direct_waitqueue_wait( &thread->cond, &thread->lock );
     }

     D_DEBUG_AT( Direct_ThreadInit, "  -> ...thread is running.\n" );

     /* Unlock the thread mutex. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> unlocking...\n" );
     direct_mutex_unlock( &thread->lock );

     D_DEBUG_AT( Direct_ThreadInit, "  -> returning %p\n", thread );

     return thread;
}

/**********************************************************************************************************************/

DirectResult
direct_thread_wait( DirectThread *thread, int timeout_ms )
{
     DirectResult ret;
     unsigned int old_counter = thread->counter;

     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->tid != -1 );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d, %dms )\n", __FUNCTION__,
                 thread->main, thread->name, thread->tid, timeout_ms );

     while (old_counter == thread->counter && !thread->terminated) {
          if (timeout_ms <= 0)
               ret = direct_waitqueue_wait( &thread->cond, &thread->lock );
          else
               ret = direct_waitqueue_wait_timeout( &thread->cond, &thread->lock, timeout_ms * 1000 );

          if (ret)
               return ret;
     }

     if (thread->terminated)
          return DR_DEAD;

     return DR_OK;
}

DirectResult
direct_thread_notify( DirectThread *thread )
{
     DirectResult ret;

     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->tid != -1 );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     ret = direct_mutex_lock( &thread->lock );
     if (ret)
          return ret;

     thread->counter++;

     direct_mutex_unlock( &thread->lock );

     return direct_waitqueue_broadcast( &thread->cond );
}

DirectResult
direct_thread_lock( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->tid != -1 );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     return direct_mutex_lock( &thread->lock );
}

DirectResult
direct_thread_unlock( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->tid != -1 );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     return direct_mutex_unlock( &thread->lock );
}

DirectResult
direct_thread_terminate( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->tid != -1 );
     D_ASSUME( thread->tid != direct_gettid() );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     thread->terminated = true;

     return direct_thread_notify( thread );
}

/**********************************************************************************************************************/

void
direct_thread_destroy( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSUME( thread->tid != direct_gettid() );
     D_ASSUME( !thread->detached );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     if (thread->detached) {
          D_DEBUG_AT( Direct_Thread, "  -> DETACHED!\n" );
          return;
     }

     direct_thread_deinit( thread );

     D_MAGIC_CLEAR( thread );

     D_FREE( thread->name );
     D_FREE( thread );
}

/******************************************************************************/

pid_t
direct_thread_get_tid( const DirectThread *thread )
{
     return thread->tid;
}

bool
direct_thread_is_canceled( const DirectThread *thread )
{
     return thread->canceled;
}

bool
direct_thread_is_joined( const DirectThread *thread )
{
     return thread->joined;
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

