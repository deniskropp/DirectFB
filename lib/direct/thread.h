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

#ifndef __DIRECT__THREAD_H__
#define __DIRECT__THREAD_H__

#include <sys/types.h>

#include <direct/types.h>
#include <direct/conf.h>

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


/*
 * Add a handler being called at the beginning of new threads.
 */
DirectThreadInitHandler *direct_thread_add_init_handler   ( DirectThreadInitFunc     func,
                                                            void                    *arg );

/*
 * Remove the specified handler.
 */
void                     direct_thread_remove_init_handler( DirectThreadInitHandler *handler );

/*
 * Create a new thread and start it.
 * The thread type is relevant for the scheduling priority.
 */
DirectThread *direct_thread_create     ( DirectThreadType      thread_type,
                                         DirectThreadMainFunc  thread_main,
                                         void                 *arg,
                                         const char           *name );

/*
 * Returns the thread of the caller.
 */
DirectThread *direct_thread_self       ( void );

/*
 * Returns the name of the specified thread.
 */
const char   *direct_thread_get_name   ( DirectThread *thread );

/*
 * Returns the thread ID of the specified thread.
 */
pid_t         direct_thread_get_tid    ( DirectThread *thread );

/*
 * Returns the name of the calling thread.
 */
const char   *direct_thread_self_name  ( void );

/*
 * Changes the name of the calling thread.
 */
void          direct_thread_set_name   ( const char   *name );

/*
 * Wait on the thread object to be notified via direct_thread_notify().
 */
DirectResult  direct_thread_wait       ( DirectThread *thread,
                                         int           timeout_ms );

/*
 * Notify the thread object waking up callers of direct_thread_wait().
 */
void direct_thread_notify     ( DirectThread *thread );

void direct_thread_lock       ( DirectThread *thread );
void direct_thread_unlock     ( DirectThread *thread );

/*
 * Kindly ask the thread to terminate (for joining without thread cancellation).
 */
void direct_thread_terminate  ( DirectThread *thread );

/*
 * Cancel a running thread.
 */
void direct_thread_cancel     ( DirectThread *thread );

/*
 * Returns true if the specified thread has been canceled.
 */
bool direct_thread_is_canceled( DirectThread *thread );

/*
 * Detach a thread.
 */
void direct_thread_detach     ( DirectThread *thread );

/*
 * Returns true if the specified thread has been detached.
 */
bool direct_thread_is_detached( DirectThread *thread );

/*
 * Check if the calling thread is canceled.
 * Must not be called by other threads than 'thread'.
 * This function won't return if the thread is canceled.
 */
void direct_thread_testcancel ( DirectThread *thread );

/*
 * Wait until a running thread is terminated.
 */
void direct_thread_join       ( DirectThread *thread );

/*
 * Returns true if the specified thread has been join.
 */
bool direct_thread_is_joined  ( DirectThread *thread );

/*
 * Free resources allocated by direct_thread_create.
 * If the thread is still running it will be killed.
 */
void direct_thread_destroy    ( DirectThread *thread );

/*
 * Utilities for stringification.
 */
#if DIRECT_BUILD_TEXT
const char *direct_thread_type_name     ( DirectThreadType type );
const char *direct_thread_scheduler_name( DirectConfigThreadScheduler scheduler );
const char *direct_thread_policy_name   ( int policy );
#endif

#endif

