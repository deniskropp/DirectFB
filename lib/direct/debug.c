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

#include <stdarg.h>
#include <unistd.h>
#include <signal.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/trace.h>


#ifdef DFB_DEBUG

void
direct_debug( const char *format, ... )
{
     char      buf[512];
     long long millis = direct_clock_get_millis();

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     fprintf( stderr, "(-) [%5d: %4lld.%03lld] %s",
              direct_gettid(), millis / 1000LL, millis % 1000LL, buf );

     fflush( stderr );
}

void
direct_break( const char *func,
              const char *file,
              int         line,
              const char *format, ... )
{
     char      buf[512];
     long long millis = direct_clock_get_millis();

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     fprintf( stderr,
              "(!) [%5d: %4lld.%03lld] *** Break! *** [%s:%d in %s()] %s",
              direct_gettid(), millis / 1000LL, millis % 1000LL, file, line, func, buf );

     fflush( stderr );

     direct_trace_print_stack( NULL );

     D_DEBUG( "Direct/Break: "
              "Sending SIGTRAP to process group %d...\n", getpgrp() );

     killpg( getpgrp(), SIGTRAP );

     D_DEBUG( "Direct/Break: "
              "...didn't catch signal on my own, calling _exit(-1).\n" );

     _exit( -1 );
}

void
direct_assertion( const char *exp,
                  const char *func,
                  const char *file,
                  int         line )
{
     long long millis = direct_clock_get_millis();

     fprintf( stderr,
              "(!) [%5d: %4lld.%03lld] *** Assertion '%s' failed! *** [%s:%d in %s()]\n",
              direct_gettid(), millis / 1000LL, millis % 1000LL, exp, file, line, func );

     fflush( stderr );

     direct_trace_print_stack( NULL );

     D_DEBUG( "Direct/Assertion: "
              "Sending SIGTRAP to process group %d...\n", getpgrp() );

     killpg( getpgrp(), SIGTRAP );

     D_DEBUG( "Direct/Assertion: "
              "...didn't catch signal on my own, calling _exit(-1).\n" );

     _exit( -1 );
}

void
direct_assumption( const char *exp,
                   const char *func,
                   const char *file,
                   int         line )
{
     long long millis = direct_clock_get_millis();

     fprintf( stderr,
              "(!) [%5d: %4lld.%03lld] *** Assumption '%s' failed *** [%s:%d in %s()]\n",
              direct_gettid(), millis / 1000LL, millis % 1000LL, exp, file, line, func );

     fflush( stderr );

     direct_trace_print_stack( NULL );
}

#endif

