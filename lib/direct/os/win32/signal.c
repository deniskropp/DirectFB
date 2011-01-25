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

#include <direct/clock.h>
#include <direct/conf.h>
#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/util.h>

D_LOG_DOMAIN( Direct_Signals, "Direct/Signals", "Signal handling" );


struct __D_DirectSignalHandler {
     DirectLink               link;

     int                      magic;

     int                      num;
     DirectSignalHandlerFunc  func;
     void                    *ctx;
};

/**************************************************************************************************/

DirectResult
direct_signals_initialize( void )
{
     D_DEBUG_AT( Direct_Signals, "Initializing...\n" );

     return DR_OK;
}

DirectResult
direct_signals_shutdown( void )
{
     D_DEBUG_AT( Direct_Signals, "Shutting down...\n" );

     return DR_OK;
}

void
direct_signals_block_all( void )
{
     D_UNIMPLEMENTED();
}

DirectResult
direct_signal_handler_add( int                       num,
                           DirectSignalHandlerFunc   func,
                           void                     *ctx,
                           DirectSignalHandler     **ret_handler )
{
     D_UNIMPLEMENTED();

     return DR_OK;
}

DirectResult
direct_signal_handler_remove( DirectSignalHandler *handler )
{
     D_UNIMPLEMENTED();

     return DR_OK;
}
