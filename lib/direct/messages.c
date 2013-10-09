/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <direct/build.h>
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/print.h>
#include <direct/result.h>
#include <direct/system.h>
#include <direct/trace.h>
#include <direct/util.h>

/**********************************************************************************************************************/

#if DIRECT_BUILD_TEXT

__dfb_no_instrument_function__
void
direct_messages_info( const char *format, ... )
{
     char  buf[512];
     char *ptr = buf;
     int   len;

     va_list ap;

     va_start( ap, format );
     len = direct_vsnprintf( buf, sizeof(buf), format, ap );
     va_end( ap );

     if (len < 0)
          return;

     if (len >= sizeof(buf)) {
          ptr = direct_malloc( len+1 );
          if (!ptr)
               return;

          va_start( ap, format );
          len = direct_vsnprintf( ptr, len+1, format, ap );
          va_end( ap );
          if (len < 0) {
               direct_free( ptr );
               return;
          }
     }

     direct_log_printf( NULL, "(*) %s", ptr );

     if (direct_config->fatal_messages & DMT_INFO)
          direct_trap( "Info", SIGABRT );

     if (ptr != buf)
          direct_free( ptr );
}

__dfb_no_instrument_function__
void
direct_messages_error( const char *format, ... )
{
     char  buf[512];
     char *ptr = buf;
     int   len;

     va_list ap;

     va_start( ap, format );
     len = direct_vsnprintf( buf, sizeof(buf), format, ap );
     va_end( ap );

     if (len < 0)
          return;

     if (len >= sizeof(buf)) {
          ptr = direct_malloc( len+1 );
          if (!ptr)
               return;

          va_start( ap, format );
          len = direct_vsnprintf( ptr, len+1, format, ap );
          va_end( ap );
          if (len < 0) {
               direct_free( ptr );
               return;
          }
     }

     direct_log_printf( NULL, "(!) %s", ptr );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_ERROR)
          direct_trap( "Error", SIGABRT );

     if (ptr != buf)
          direct_free( ptr );
}

__dfb_no_instrument_function__
void
direct_messages_derror( DirectResult result, const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     direct_vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) %s    --> %s\n", buf, DirectResultString( result ) );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_ERROR)
          direct_trap( "DError", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_perror( int erno, const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     direct_vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) %s    --> %s\n", buf, direct_strerror( erno ) );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_ERROR)
          direct_trap( "PError", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_dlerror( const char *dlerr, const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     direct_vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) %s    --> %s\n", buf, dlerr );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_ERROR)
          direct_trap( "DlError", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_once( const char *func,
                      const char *file,
                      int         line,
                      const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     direct_vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, " (!!!)  *** ONCE [%s] *** [%s:%d in %s()]\n", buf, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_ONCE)
          direct_trap( "Once", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_unimplemented( const char *func,
                               const char *file,
                               int         line )
{
     direct_log_printf( NULL, " (!!!)  *** UNIMPLEMENTED [%s] *** [%s:%d]\n", func, file, line );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_UNIMPLEMENTED)
          direct_trap( "Unimplemented", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_bug( const char *func,
                     const char *file,
                     int         line,
                     const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     direct_vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, " (!?!)  *** BUG [%s] *** [%s:%d in %s()]\n", buf, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_BUG)
          direct_trap( "Bug", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_warn( const char *func,
                      const char *file,
                      int         line,
                      const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     direct_vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, " (!!!)  *** WARNING [%s] *** [%s:%d in %s()]\n", buf, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_WARNING)
          direct_trap( "Warning", SIGABRT );
}

#endif

