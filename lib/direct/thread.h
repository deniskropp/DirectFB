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

#ifndef __DIRECT__THREAD_H__
#define __DIRECT__THREAD_H__

#include <direct/os/mutex.h>
#include <direct/os/thread.h>
#include <direct/os/waitqueue.h>

/**********************************************************************************************************************/

typedef enum {
     DTT_DEFAULT    =   0,
     DTT_CLEANUP    =  -5,
     DTT_INPUT      = -10,
     DTT_OUTPUT     = -12,
     DTT_MESSAGING  = -15,
     DTT_CRITICAL   = -20
} DirectThreadType;

typedef void * (*DirectThreadMainFunc)( DirectThread *thread, void *arg );

typedef void   (*DirectThreadInitFunc)( DirectThread *thread, void *arg );

/**********************************************************************************************************************/

struct __D_DirectThread {
     int                   magic;

     char                 *name;

     DirectThreadType      type;        /* The thread's type, e.g. input thread. */
     DirectThreadMainFunc  main;        /* The thread's main routine (or entry point). */
     void                 *arg;         /* Custom argument passed to the main routine. */


     DirectThreadHandle    handle;      /* The OS thread handle... */
     pid_t                 tid;         /* A simplified version using an integer */


     bool                  canceled;    /* Set when direct_thread_cancel() is called. */
     bool                  joining;     /* Set when direct_thread_join() is called. */
     bool                  joined;      /* Set when direct_thread_join() has finished. */
     bool                  detached;    /* Set when direct_thread_detach() is called. */
     bool                  terminated;  /* Set when direct_thread_terminate() is called. */

     bool                  init;        /* Set to true before the main routine is called. */

     DirectMutex           lock;
     DirectWaitQueue       cond;

     unsigned int          counter;
};

/**********************************************************************************************************************/

#define DIRECT_THREAD_ASSERT( _thread )                                                                  \
     do {                                                                                                \
          D_MAGIC_ASSERT( _thread, DirectThread );                                                       \
          D_ASSERT( (_thread)->tid != (pid_t) -1 );                                                      \
     } while (0)

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/*
 * Add a handler being called at the beginning of new threads.
 */
DirectThreadInitHandler DIRECT_API *direct_thread_add_init_handler   ( DirectThreadInitFunc     func,
                                                            void                    *arg );

/*
 * Remove the specified handler.
 */
void                    DIRECT_API  direct_thread_remove_init_handler( DirectThreadInitHandler *handler );

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/*
 * Create a new thread and start it.
 * The thread type is relevant for the scheduling priority.
 */
DirectThread DIRECT_API *direct_thread_create     ( DirectThreadType      thread_type,
                                                    DirectThreadMainFunc  thread_main,
                                                    void                 *arg,
                                                    const char           *name );

/*
 * Wait on the thread object to be notified via direct_thread_notify().
 */
DirectResult DIRECT_API  direct_thread_wait       ( DirectThread *thread,
                                                    int           timeout_ms );

/*
 * Notify the thread object waking up callers of direct_thread_wait().
 */
DirectResult DIRECT_API direct_thread_notify     ( DirectThread *thread );

DirectResult DIRECT_API direct_thread_lock       ( DirectThread *thread );
DirectResult DIRECT_API direct_thread_unlock     ( DirectThread *thread );

/*
 * Kindly ask the thread to terminate (for joining without thread cancellation).
 */
DirectResult DIRECT_API direct_thread_terminate  ( DirectThread *thread );


/*
 * Free resources allocated by direct_thread_create.
 * If the thread is still running it will be killed.
 */
void  DIRECT_API direct_thread_destroy    ( DirectThread       *thread );


pid_t DIRECT_API direct_thread_get_tid    ( const DirectThread *thread );
bool  DIRECT_API direct_thread_is_canceled( const DirectThread *thread );
bool  DIRECT_API direct_thread_is_joined  ( const DirectThread *thread );
/*
 * Returns the name of the specified thread.
 */
const char   *direct_thread_get_name   ( DirectThread     *thread );

/*
 * Utilities for stringification.
 */
#if DIRECT_BUILD_TEXT
const char DIRECT_API *direct_thread_type_name     ( DirectThreadType type );
const char DIRECT_API *direct_thread_scheduler_name( DirectConfigThreadScheduler scheduler );
const char DIRECT_API *direct_thread_policy_name   ( int policy );
#endif



void DIRECT_API __D_direct_thread_call_init_handlers( DirectThread *thread );


void __D_thread_init( void );
void __D_thread_deinit( void );

#endif

