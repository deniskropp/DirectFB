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

#include <direct/build.h>


#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/list.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/util.h>


#if DIRECT_BUILD_TEXT

typedef struct {
     DirectLink  link;
     char       *name;
     bool        enabled;
} DebugDomainEntry;

/**************************************************************************************************/

static pthread_mutex_t  domains_lock = DIRECT_UTIL_RECURSIVE_PTHREAD_MUTEX_INITIALIZER;
static unsigned int     domains_age  = 1;
static DirectLink      *domains      = NULL;

/**************************************************************************************************/

__attribute__((no_instrument_function))
static inline DebugDomainEntry *
lookup_domain( const char *name, bool sub )
{
     DebugDomainEntry *entry;

     direct_list_foreach (entry, domains) {
          if (! strcasecmp( entry->name, name ))
               return entry;
     }

     if (sub) {
          char *tmp = strchr( name, '/' );

          if (tmp) {
               direct_list_foreach (entry, domains) {
                    if (! strchr( entry->name, '/' ) &&
                        ! strncasecmp( entry->name, name, tmp - name ))
                         return entry;
               }
          }
     }

     return NULL;
}

__attribute__((no_instrument_function))
static inline bool
check_domain( DirectDebugDomain *domain )
{
     if (domain->age != domains_age) {
          DebugDomainEntry *entry = lookup_domain( domain->name, true );

          domain->age = domains_age;

          if (entry) {
               domain->registered = true;
               domain->enabled    = entry->enabled;
          }
     }

     return domain->registered ? domain->enabled : direct_config->debug;
}

/**************************************************************************************************/

void
direct_debug_config_domain( const char *name, bool enable )
{
     DebugDomainEntry *entry;

     pthread_mutex_lock( &domains_lock );

     entry = lookup_domain( name, false );
     if (!entry) {
          entry = calloc( 1, sizeof(DebugDomainEntry) );
          if (!entry) {
               D_WARN( "out of memory" );
               pthread_mutex_unlock( &domains_lock );
               return;
          }

          entry->name = strdup( name );

          direct_list_prepend( &domains, &entry->link );
     }

     entry->enabled = enable;

     ++domains_age || domains_age++;

     pthread_mutex_unlock( &domains_lock );
}

/**************************************************************************************************/

__attribute__((no_instrument_function))
void
direct_debug( const char *format, ... )
{
     char        buf[512];
     long long   millis = direct_clock_get_millis();
     const char *name   = direct_thread_self_name();

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     fprintf( stderr, "(-) [%-15s %3lld.%03lld] (%5d) %s",
              name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL, direct_gettid(), buf );

     fflush( stderr );
}

__attribute__((no_instrument_function))
void
direct_debug_at( DirectDebugDomain *domain,
                 const char        *format, ... )
{
     pthread_mutex_lock( &domains_lock );

     if (check_domain( domain )) {
          int         len;
          char        dom[48];
          char        fmt[64];
          char        buf[512];
          long long   millis = direct_clock_get_millis();
          const char *name   = direct_thread_self_name();
          va_list     ap;

          va_start( ap, format );

          vsnprintf( buf, sizeof(buf), format, ap );

          va_end( ap );


          len = snprintf( dom, sizeof(dom), "%s:", domain->name );

          if (len < 18)
               len = 18;
          else
               len = 28;

          len += direct_trace_debug_indent() * 4;

          snprintf( fmt, sizeof(fmt), "(-) [%%-15s %%3lld.%%03lld] (%%5d) %%-%ds %%s", len );

          fprintf( stderr, fmt, name ? name : "  NO NAME  ",
                   millis / 1000LL, millis % 1000LL, direct_gettid(), dom, buf );

          fflush( stderr );
     }

     pthread_mutex_unlock( &domains_lock );
}

__attribute__((no_instrument_function))
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

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     fprintf( stderr,
              "(!) [%-15s %3lld.%03lld] (%5d) *** Break [%s] *** [%s:%d in %s()]\n",
              name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL,
              direct_gettid(), buf, file, line, func );

     fflush( stderr );

     direct_trace_print_stack( NULL );

     D_DEBUG( "Direct/Break: "
              "Sending SIGTRAP to process group %d...\n", getpgrp() );
#ifndef USE_KOS
     killpg( getpgrp(), SIGTRAP );
#endif
     D_DEBUG( "Direct/Break: "
              "...didn't catch signal on my own, calling _exit(-1).\n" );

     _exit( -1 );
}

__attribute__((no_instrument_function))
void
direct_assertion( const char *exp,
                  const char *func,
                  const char *file,
                  int         line )
{
     long long   millis = direct_clock_get_millis();
     const char *name   = direct_thread_self_name();

     fprintf( stderr,
              "(!) [%-15s %3lld.%03lld] (%5d) *** Assertion [%s] failed *** [%s:%d in %s()]\n",
              name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL,
              direct_gettid(), exp, file, line, func );

     fflush( stderr );

     direct_trace_print_stack( NULL );

     D_DEBUG( "Direct/Assertion: "
              "Sending SIGTRAP to process group %d...\n", getpgrp() );
#ifndef USE_KOS
     killpg( getpgrp(), SIGTRAP );
#endif
     D_DEBUG( "Direct/Assertion: "
              "...didn't catch signal on my own, calling _exit(-1).\n" );

     _exit( -1 );
}

__attribute__((no_instrument_function))
void
direct_assumption( const char *exp,
                   const char *func,
                   const char *file,
                   int         line )
{
     long long   millis = direct_clock_get_millis();
     const char *name   = direct_thread_self_name();

     fprintf( stderr,
              "(!) [%-15s %3lld.%03lld] (%5d) *** Assumption [%s] failed *** [%s:%d in %s()]\n",
              name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL,
              direct_gettid(), exp, file, line, func );

     fflush( stderr );

     direct_trace_print_stack( NULL );
}

#else

void
direct_debug_config_domain( const char *name, bool enable )
{
}

#endif    /* DIRECT_BUILD_TEXT */

