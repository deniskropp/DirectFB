/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __CORE__THREAD_H__
#define __CORE__THREAD_H__

#include <core/coretypes.h>

#include <core/fusion/fusion_types.h>


typedef enum {
     CTT_ANY,
     CTT_INPUT
} CoreThreadType;

typedef void * (*CoreThreadMain)( CoreThread *thread, void *arg );


/*
 * Create a new thread and start it. The thread type is relevant
 * for the scheduling priority.
 */
CoreThread *dfb_thread_create     ( CoreThreadType  thread_type,
                                    CoreThreadMain  thread_main,
                                    void           *arg );

/*
 * Cancel a running thread.
 */
void        dfb_thread_cancel     ( CoreThread     *thread );

/*
 * Returns true if the specified thread has been canceled.
 */
bool        dfb_thread_is_canceled( CoreThread     *thread );

/*
 * Check if the calling thread is canceled.
 * Must not be called by other threads than 'thread'.
 * This function won't return if the thread is canceled.
 */
void        dfb_thread_testcancel ( CoreThread     *thread );

/*
 * Wait until a running thread is terminated.
 */
void        dfb_thread_join       ( CoreThread     *thread );

/*
 * Returns true if the specified thread has been join.
 */
bool        dfb_thread_is_joined  ( CoreThread     *thread );

/*
 * Free resources allocated by dfb_thread_create.
 * If the thread is still running it will be killed.
 */
void        dfb_thread_destroy    ( CoreThread     *thread );

#endif
