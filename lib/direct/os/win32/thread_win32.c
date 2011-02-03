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
#include <direct/mem.h>
#include <direct/signals.h>
#include <direct/thread.h>


D_LOG_DOMAIN( Direct_Thread    , "Direct/Thread",      "Thread management" );
D_LOG_DOMAIN( Direct_ThreadInit, "Direct/Thread/Init", "Thread initialization" );

/**********************************************************************************************************************/

static DWORD WINAPI
StartThread(LPVOID iValue)
{
     DWORD         ret;
     DirectThread *thread = iValue;

     D_DEBUG_AT( Direct_ThreadInit, "%s( %p )\n", __FUNCTION__, thread );

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
          return 0;
     }

     D_MAGIC_ASSERT( thread, DirectThread );

     /* Call main routine. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> running...\n" );
     ret = (DWORD) thread->main( thread, thread->arg );

//     D_DEBUG_AT( Direct_Thread, "  -> Returning %p from '%s' (%s, %d)...\n",
//                 ret, thread->name, direct_thread_type_name(thread->type), thread->tid );

     //D_MAGIC_ASSERT( thread, DirectThread );

     return ret;
}

DirectResult
direct_thread_init( DirectThread *thread )
{
     thread->handle.thread = CreateThread( NULL, 0, StartThread, thread, 0, &thread->handle.gen );

     return DR_UNIMPLEMENTED;
}

void
direct_thread_deinit( DirectThread *thread )
{
}

/**********************************************************************************************************************/

DirectThread *
direct_thread_self( void )
{
     D_UNIMPLEMENTED();

     return NULL;
}

__no_instrument_function__
const char *
direct_thread_self_name( void )
{
     D_UNIMPLEMENTED();

     return "NO NAME";
}

void
direct_thread_set_name( const char *name )
{
     D_UNIMPLEMENTED();
}

void
direct_thread_cancel( DirectThread *thread )
{
     D_UNIMPLEMENTED();
}

void
direct_thread_detach( DirectThread *thread )
{
     D_UNIMPLEMENTED();
}

void
direct_thread_testcancel( DirectThread *thread )
{
     D_UNIMPLEMENTED();
}

void
direct_thread_join( DirectThread *thread )
{
     D_UNIMPLEMENTED();
}

void
direct_thread_sleep( long long micros )
{
     Sleep( (DWORD)(micros / 1000) );
}
