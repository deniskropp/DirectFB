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

#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/signals.h>
#include <direct/thread.h>
#include <direct/util.h>

D_DEBUG_DOMAIN( Direct_Main, "Direct/Main", "Initialization and shutdown of libdirect" );

/**************************************************************************************************/

static int             refs      = 0;
static pthread_mutex_t refs_lock = PTHREAD_MUTEX_INITIALIZER;

/**************************************************************************************************/

DirectResult
direct_initialize()
{
     pthread_mutex_lock( &refs_lock );

     D_DEBUG_AT( Direct_Main, "direct_initialize() called...\n" );

     if (refs++) {
          D_DEBUG_AT( Direct_Main, "...%d references now.\n", refs );
          pthread_mutex_unlock( &refs_lock );
          return DFB_OK;
     }
     else if (!direct_thread_self_name())
          direct_thread_set_name( "Main Thread" );

     D_DEBUG_AT( Direct_Main, "...initializing now.\n" );

     direct_signals_initialize();

     pthread_mutex_unlock( &refs_lock );

     return DFB_OK;
}

DirectResult
direct_shutdown()
{
     pthread_mutex_lock( &refs_lock );

     D_DEBUG_AT( Direct_Main, "direct_shutdown() called...\n" );

     if (--refs) {
          D_DEBUG_AT( Direct_Main, "...%d references left.\n", refs );
          pthread_mutex_unlock( &refs_lock );
          return DFB_OK;
     }

     D_DEBUG_AT( Direct_Main, "...shutting down now.\n" );

     direct_signals_shutdown();

     pthread_mutex_unlock( &refs_lock );

     return DFB_OK;
}

