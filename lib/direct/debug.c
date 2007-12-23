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

#include <direct/build.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/list.h>
#include <direct/log.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/util.h>


#if DIRECT_BUILD_TEXT

#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */

typedef struct {
     DirectLink  link;
     char       *name;
     bool        enabled;
} DebugDomainEntry;

/**************************************************************************************************/

static pthread_mutex_t  domains_lock = PTHREAD_MUTEX_INITIALIZER;
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

     /*
      * If the domain being registered contains a slash, but didn't exactly match an entry
      * in directfbrc, check to see if the domain is descended from an entry in directfbrc
      * (e.g. 'ui/field/messages' matches 'ui' or 'ui/field')
      */
     if (sub && strchr(name, '/')) {
          int passed_name_len = strlen( name );

          direct_list_foreach (entry, domains) {
               int entry_len = strlen( entry->name );
               if ((passed_name_len > entry_len) &&
                   (name[entry_len] == '/') &&
                   (! strncasecmp( entry->name, name, entry_len))) {
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

#endif /* DIRECT_BUILD_DEBUGS */

/**************************************************************************************************/

void
direct_debug_config_domain( const char *name, bool enable )
{
#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */
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

     if (! ++domains_age)
          domains_age++;

     pthread_mutex_unlock( &domains_lock );
#endif /* DIRECT_BUILD_DEBUGS */
}

/**************************************************************************************************/

__attribute__((no_instrument_function))
static inline void
debug_domain_vprintf( const char        *name,
                      int                name_len,
                      const char        *format,
                      va_list            ap )
{
     char        buf[512];
     long long   millis = direct_clock_get_millis();
     const char *thread = direct_thread_self_name();
     int         indent = direct_trace_debug_indent() * 4;

     /* Prepare user message. */
     vsnprintf( buf, sizeof(buf), format, ap );

     /* Fill up domain name column after the colon, prepending remaining space (excl. ': ') to indent. */
     indent += (name_len < 20 ? 20 : 36) - name_len - 2;

     /* Print full message. */
     direct_log_printf( NULL, "(-) [%-15s %3lld.%03lld] (%5d) %s: %*s%s", thread ? thread : "  NO NAME",
                        millis / 1000LL, millis % 1000LL, direct_gettid(), name, indent, "", buf );
}

#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */

__attribute__((no_instrument_function))
void
direct_debug( const char *format, ... )
{
     va_list ap;

     va_start( ap, format );

     debug_domain_vprintf( "- - ", 4, format, ap );

     va_end( ap );
}

__attribute__((no_instrument_function))
void
direct_debug_at( DirectDebugDomain *domain,
                 const char        *format, ... )
{
     bool enabled;

     pthread_mutex_lock( &domains_lock );

     enabled = check_domain( domain );

     pthread_mutex_unlock( &domains_lock );

     if (enabled) {
          va_list ap;

          va_start( ap, format );

          debug_domain_vprintf( domain->name, domain->name_len, format, ap );

          va_end( ap );
     }
}

#endif /* DIRECT_BUILD_DEBUGS */

__attribute__((no_instrument_function))
void
direct_debug_at_always( DirectDebugDomain *domain,
                        const char        *format, ... )
{
     va_list ap;

     va_start( ap, format );

     debug_domain_vprintf( domain->name, domain->name_len, format, ap );

     va_end( ap );
}

#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */

__attribute__((no_instrument_function))
void
direct_debug_enter( DirectDebugDomain *domain,
                    const char *func,
                    const char *file,
                    int         line,
                    const char *format, ... )
{
     bool enabled;

     pthread_mutex_lock( &domains_lock );

     enabled = check_domain( domain );

     pthread_mutex_unlock( &domains_lock );

     if (enabled) {
          int         len;
          char        dom[48];
          char        fmt[128];
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

          snprintf( fmt, sizeof(fmt), "(>) [%%-15s %%3lld.%%03lld] (%%5d) %%-%ds Entering %%s%%s [%%s:%%d]\n", len );

          direct_log_printf( NULL, fmt, name ? name : "  NO NAME  ",
                             millis / 1000LL, millis % 1000LL, direct_gettid(), dom,
			     func, buf, file, line );
     }
}

__attribute__((no_instrument_function))
void
direct_debug_exit( DirectDebugDomain *domain,
                   const char *func,
                   const char *file,
                   int         line,
                   const char *format, ... )
{
     bool enabled;

     pthread_mutex_lock( &domains_lock );

     enabled = check_domain( domain );

     pthread_mutex_unlock( &domains_lock );

     if (enabled) {
          int         len;
          char        dom[48];
          char        fmt[128];
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

          snprintf( fmt, sizeof(fmt), "(<) [%%-15s %%3lld.%%03lld] (%%5d) %%-%ds Returning from %%s%%s [%%s:%%d]\n", len );

          direct_log_printf( NULL, fmt, name ? name : "  NO NAME  ",
                             millis / 1000LL, millis % 1000LL, direct_gettid(), dom,
			     func, buf, file, line );
     }
}

__attribute__((no_instrument_function))
static void
trap( const char *domain )
{
     D_DEBUG( "Direct/%s: Raising SIGTRAP...\n", domain );

     raise( SIGTRAP );

     D_DEBUG( "Direct/%s: ...didn't catch signal on my own, calling killpg().\n", domain );

     killpg( 0, SIGTRAP );

     D_DEBUG( "Direct/%s: ...still running, calling pthread_exit().\n", domain );

     pthread_exit( NULL );
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

     direct_log_printf( NULL,
                        "(!) [%-15s %3lld.%03lld] (%5d) *** Break [%s] *** [%s:%d in %s()]\n",
                        name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL,
                        direct_gettid(), buf, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_break)
          trap( "Break" );
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

     direct_log_printf( NULL,
                        "(!) [%-15s %3lld.%03lld] (%5d) *** Assertion [%s] failed *** [%s:%d in %s()]\n",
                        name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL,
                        direct_gettid(), exp, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal >= DCFL_ASSERT)
          trap( "Assertion" );
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

     direct_log_printf( NULL,
                        "(!) [%-15s %3lld.%03lld] (%5d) *** Assumption [%s] failed *** [%s:%d in %s()]\n",
                        name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL,
                        direct_gettid(), exp, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal >= DCFL_ASSUME)
          trap( "Assumption" );
}

#endif /* DIRECT_BUILD_DEBUGS */

#else

void
direct_debug_config_domain( const char *name, bool enable )
{
}

#endif /* DIRECT_BUILD_TEXT */

