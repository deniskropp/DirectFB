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

#include <config.h>

#include <pthread.h>

#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/signals.h>
#include <direct/util.h>

/**************************************************************************************************/

static int             refs      = 0;
static pthread_mutex_t refs_lock = DIRECT_UTIL_RECURSIVE_PTHREAD_MUTEX_INITIALIZER;

/**************************************************************************************************/

DirectResult
direct_initialize()
{
     pthread_mutex_lock( &refs_lock );

     D_DEBUG( "Direct/Initialize: direct_initialize() called...\n" );

     if (refs++) {
          D_DEBUG( "Direct/Initialize: ...%d references now.\n" );
          pthread_mutex_unlock( &refs_lock );
          return DFB_OK;
     }

     D_DEBUG( "Direct/Initialize: ...initializing now.\n" );

     direct_signals_initialize();

     pthread_mutex_unlock( &refs_lock );

     return DFB_OK;
}

DirectResult
direct_shutdown()
{
     pthread_mutex_lock( &refs_lock );

     D_DEBUG( "Direct/Shutdown: direct_shutdown() called...\n" );

     if (--refs) {
          D_DEBUG( "Direct/Shutdown: ...%d references now.\n" );
          pthread_mutex_unlock( &refs_lock );
          return DFB_OK;
     }

     D_DEBUG( "Direct/Shutdown: ...shutting down now.\n" );

     direct_signals_shutdown();

     pthread_mutex_unlock( &refs_lock );

     return DFB_OK;
}

