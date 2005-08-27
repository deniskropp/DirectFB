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
#include <string.h>
#include <stdio.h>

#include <direct/build.h>
#include <direct/log.h>
#include <direct/messages.h>
#include <direct/trace.h>
#include <direct/util.h>


#if DIRECT_BUILD_TEXT

__attribute__((no_instrument_function))
void
direct_messages_info( const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(*) %s", buf );
}

__attribute__((no_instrument_function))
void
direct_messages_error( const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) %s", buf );

     direct_trace_print_stack( NULL );
}

__attribute__((no_instrument_function))
void
direct_messages_derror( DirectResult result, const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) %s    --> %s\n", buf, DirectResultString( result ) );

     direct_trace_print_stack( NULL );
}

__attribute__((no_instrument_function))
void
direct_messages_perror( int erno, const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) %s    --> %s\n", buf, strerror( erno ) );

     direct_trace_print_stack( NULL );
}

__attribute__((no_instrument_function))
void
direct_messages_dlerror( const char *dlerr, const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) %s    --> %s\n", buf, dlerr );

     direct_trace_print_stack( NULL );
}

__attribute__((no_instrument_function))
void
direct_messages_once( const char *func,
                      const char *file,
                      int         line,
                      const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, " (!!!)  *** ONCE [%s] *** [%s:%d in %s()]\n", buf, file, line, func );

     direct_trace_print_stack( NULL );
}

__attribute__((no_instrument_function))
void
direct_messages_unimplemented( const char *func,
                               const char *file,
                               int         line )
{
     direct_log_printf( NULL, " (!!!)  *** UNIMPLEMENTED [%s] *** [%s:%d]\n", func, file, line );

     direct_trace_print_stack( NULL );
}

__attribute__((no_instrument_function))
void
direct_messages_bug( const char *func,
                     const char *file,
                     int         line,
                     const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, " (!?!)  *** BUG [%s] *** [%s:%d in %s()]\n", buf, file, line, func );

     direct_trace_print_stack( NULL );
}

__attribute__((no_instrument_function))
void
direct_messages_warn( const char *func,
                      const char *file,
                      int         line,
                      const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, " (!!!)  *** WARNING [%s] *** [%s:%d in %s()]\n", buf, file, line, func );

     direct_trace_print_stack( NULL );
}

#endif

