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

#ifndef __DIRECT__THREAD_H__
#define __DIRECT__THREAD_H__

#include <direct/types.h>


typedef enum {
     DTT_DEFAULT    =   0,
     DTT_CLEANUP    =  -5,
     DTT_INPUT      = -10,
     DTT_OUTPUT     = -12,
     DTT_MESSAGING  = -15,
     DTT_CRITICAL   = -20
} DirectThreadType;

typedef void * (*DirectThreadMain)( DirectThread *thread, void *arg );


/*
 * Create a new thread and start it. The thread type is relevant
 * for the scheduling priority.
 */
DirectThread *direct_thread_create     ( DirectThreadType  thread_type,
                                         DirectThreadMain  thread_main,
                                         void             *arg,
                                         const char       *name );

/*
 * Returns the thread of the caller.
 */
DirectThread *direct_thread_self       ();

/*
 * Returns the name of the specified thread.
 */
const char   *direct_thread_get_name   ( DirectThread     *thread );

/*
 * Returns the name of the calling thread.
 */
const char   *direct_thread_self_name  ();

/*
 * Cancel a running thread.
 */
void          direct_thread_cancel     ( DirectThread     *thread );

/*
 * Returns true if the specified thread has been canceled.
 */
bool          direct_thread_is_canceled( DirectThread     *thread );

/*
 * Check if the calling thread is canceled.
 * Must not be called by other threads than 'thread'.
 * This function won't return if the thread is canceled.
 */
void          direct_thread_testcancel ( DirectThread     *thread );

/*
 * Wait until a running thread is terminated.
 */
void          direct_thread_join       ( DirectThread     *thread );

/*
 * Returns true if the specified thread has been join.
 */
bool          direct_thread_is_joined  ( DirectThread     *thread );

/*
 * Free resources allocated by dfb_thread_create.
 * If the thread is still running it will be killed.
 */
void          direct_thread_destroy    ( DirectThread     *thread );

#endif

