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

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/list.h>
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/print.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/util.h>


/**********************************************************************************************************************/

#if DIRECT_BUILD_TEXT

#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */
  
__dfb_no_instrument_function__
void
direct_debug_log( DirectLogDomain *domain,
              unsigned int     debug_level,  /* 1-9, 0 = info */
              const char      *format, ... )
{
     va_list ap;

     debug_level += DIRECT_LOG_DEBUG_0;

     va_start( ap, format );
     direct_log_domain_vprintf( domain, debug_level > DIRECT_LOG_DEBUG_9 ? DIRECT_LOG_DEBUG_9 : debug_level, format, ap );
     va_end( ap );
}

__dfb_no_instrument_function__
void
direct_debug_at( DirectLogDomain *domain,
                 const char      *format, ... )
{
     va_list ap;

     va_start( ap, format );
     direct_log_domain_vprintf( domain, DIRECT_LOG_DEBUG, format, ap );
     va_end( ap );
}

#endif /* DIRECT_BUILD_DEBUGS */

__dfb_no_instrument_function__
void
direct_debug_at_always( DirectLogDomain *domain,
                        const char      *format, ... )
{
     if (direct_config->log_level >= DIRECT_LOG_DEBUG) {
          va_list ap;

          va_start( ap, format );
          direct_log_domain_vprintf( domain, DIRECT_LOG_NONE, format, ap );
          va_end( ap );
     }
}

#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */

__dfb_no_instrument_function__
void
direct_break( const char *func,
              const char *file,
              int         line,
              const char *format, ... )
{
     char        buf[512];
     long long   millis = direct_clock_get_millis();
     const char *name   = direct_thread_self_name();

     va_list ap;

     va_start( ap, format );

     direct_vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL,
                        "(!) [%-15s %3lld.%03lld] (%5d) *** Break [%s] *** [%s:%d in %s()]\n",
                        name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL,
                        direct_gettid(), buf, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_break)
          direct_trap( "Break", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_assertion( const char *exp,
                  const char *func,
                  const char *file,
                  int         line )
{
     long long   millis = direct_clock_get_millis();
     const char *name   = direct_thread_self_name();

     direct_log_printf( NULL,
                        "(!) [%-15s %3lld.%03lld] (%5d) *** Assertion [%s] failed *** [%s:%d in %s()]\n",
                        name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL,
                        direct_gettid(), exp, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal >= DCFL_ASSERT)
          direct_trap( "Assertion", SIGTRAP );
}

__dfb_no_instrument_function__
void
direct_assumption( const char *exp,
                   const char *func,
                   const char *file,
                   int         line )
{
     long long   millis = direct_clock_get_millis();
     const char *name   = direct_thread_self_name();

     direct_log_printf( NULL,
                        "(!) [%-15s %3lld.%03lld] (%5d) *** Assumption [%s] failed *** [%s:%d in %s()]\n",
                        name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL,
                        direct_gettid(), exp, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal >= DCFL_ASSUME)
          direct_trap( "Assumption", SIGTRAP );
}

#endif /* DIRECT_BUILD_DEBUGS */

#endif /* DIRECT_BUILD_TEXT */

