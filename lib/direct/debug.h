/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECT__DEBUG_H__
#define __DIRECT__DEBUG_H__

#include <direct/build.h>

#include <stdio.h>
#include <errno.h>

#include <direct/clock.h>
#include <direct/conf.h>
#include <direct/log.h>
#include <direct/messages.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/types.h>


typedef struct {
     unsigned int   age;
     bool           enabled;
     bool           registered;

     const char    *name;
     const char    *description;

     int            name_len;
} DirectDebugDomain;

void direct_debug_config_domain( const char *name, bool enable );


#if DIRECT_BUILD_TEXT

#define D_DEBUG_DOMAIN(identifier,name,description)                                  \
     static DirectDebugDomain identifier __attribute__((unused))                     \
            = { 0, false, false, name, description, sizeof(name) - 1 }

void direct_debug_at_always( DirectDebugDomain *domain,
                             const char        *format, ... )  D_FORMAT_PRINTF(2);

#define d_debug_at( domain, x... )      direct_debug_at_always( &domain, x )


#if DIRECT_BUILD_DEBUGS

void direct_debug( const char *format, ... )  D_FORMAT_PRINTF(1);

void direct_debug_at( DirectDebugDomain *domain,
                      const char        *format, ... )  D_FORMAT_PRINTF(2);

void direct_debug_enter( DirectDebugDomain *domain,
	      	         const char *func,
                         const char *file,
                         int         line,
                         const char *format, ... )  D_FORMAT_PRINTF(5);

void direct_debug_exit( DirectDebugDomain *domain,
	      	        const char *func,
                        const char *file,
                        int         line,
                        const char *format, ... )  D_FORMAT_PRINTF(5);

void direct_break( const char *func,
                   const char *file,
                   int         line,
                   const char *format, ... )  D_FORMAT_PRINTF(4);

void direct_assertion( const char *exp,
                       const char *func,
                       const char *file,
                       int         line );

void direct_assumption( const char *exp,
                        const char *func,
                        const char *file,
                        int         line );
#endif

#if DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG) || defined(DIRECT_FORCE_DEBUG)

#define D_DEBUG_ENABLED  (1)

#define D_DEBUG(x...)                                                                \
     do {                                                                            \
          if (!direct_config || direct_config->debug)                                \
               direct_debug( x );                                                    \
     } while (0)


#define D_DEBUG_AT(d,x...)                                                           \
     do {                                                                            \
          direct_debug_at( &d, x );                                                  \
     } while (0)

#define D_DEBUG_ENTER(d,x...)                                                        \
     do {                                                                            \
          direct_debug_enter( &d, __FUNCTION__, __FILE__, __LINE__, x );             \
     } while (0)

#define D_DEBUG_EXIT(d,x...)                                                         \
     do {                                                                            \
          direct_debug_exit( &d, __FUNCTION__, __FILE__, __LINE__, x );              \
     } while (0)

#define D_ASSERT(exp)                                                                \
     do {                                                                            \
          if (!(exp))                                                                \
               direct_assertion( #exp, __FUNCTION__, __FILE__, __LINE__ );           \
     } while (0)


#define D_ASSUME(exp)                                                                \
     do {                                                                            \
          if (!(exp))                                                                \
               direct_assumption( #exp, __FUNCTION__, __FILE__, __LINE__ );          \
     } while (0)


#define D_BREAK(x...)                                                                \
     do {                                                                            \
          direct_break( __FUNCTION__, __FILE__, __LINE__, x );                       \
     } while (0)

#elif defined(DIRECT_MINI_DEBUG)

/*
 * Mini debug mode, only D_DEBUG_AT, no domain filters, simple assertion
 */

#define D_DEBUG_ENABLED  (2)

#define D_DEBUG_AT(d,x...)                                                           \
     do {                                                                            \
          if (direct_config->debug) direct_debug_at_always( &d, x );                 \
     } while (0)

#define D_CHECK(exp, aa)                                                             \
     do {                                                                            \
          if (!(exp)) {                                                              \
               long long   millis = direct_clock_get_millis();                       \
               const char *name   = direct_thread_self_name();                       \
                                                                                     \
               direct_log_printf( NULL,                                              \
                                  "(!) [%-15s %3lld.%03lld] (%5d) *** " #aa " [%s] failed *** [%s:%d in %s()]\n",  \
                                  name ? name : "  NO NAME  ", millis / 1000LL, millis % 1000LL,                   \
                                  direct_gettid(), #exp, __FILE__, __LINE__, __FUNCTION__ );                       \
                                                                                     \
               direct_trace_print_stack( NULL );                                     \
          }                                                                          \
     } while (0)

#define D_ASSERT(exp)    D_CHECK(exp, Assertion)
#define D_ASSUME(exp)    D_CHECK(exp, Assumption)

#endif    /* MINI_DEBUG  / DIRECT_BUILD_DEBUG || DIRECT_ENABLE_DEBUG || DIRECT_FORCE_DEBUG */

#endif    /* DIRECT_BUILD_TEXT */


/*
 * Fallback definitions, e.g. without DIRECT_BUILD_TEXT or DIRECT_ENABLE_DEBUG
 */

#ifndef D_DEBUG_ENABLED
#define D_DEBUG_ENABLED  (0)
#endif

#ifndef D_DEBUG
#define D_DEBUG(x...)              do {} while (0)
#endif

#ifndef D_DEBUG_AT
#define D_DEBUG_AT(d,x...)         do {} while (0)
#endif

#ifndef D_DEBUG_ENTER
#define D_DEBUG_ENTER(d,x...)      do {} while (0)
#endif

#ifndef D_DEBUG_EXIT
#define D_DEBUG_EXIT(d,x...)       do {} while (0)
#endif

#ifndef D_ASSERT
#define D_ASSERT(exp)              do {} while (0)
#endif

#ifndef D_ASSUME
#define D_ASSUME(exp)              do {} while (0)
#endif

#ifndef D_BREAK
#define D_BREAK(x...)              do {} while (0)
#endif

#ifndef d_debug_at
#define d_debug_at( domain, x... ) do {} while (0)
#endif

#ifndef D_DEBUG_DOMAIN
#define D_DEBUG_DOMAIN(i,n,d)
#endif



/*
 * Magic Assertions & Utilities
 */

#define D_MAGIC(spell)             ( (((spell)[sizeof(spell)*8/9] << 24) | \
                                      ((spell)[sizeof(spell)*7/9] << 16) | \
                                      ((spell)[sizeof(spell)*6/9] <<  8) | \
                                      ((spell)[sizeof(spell)*5/9]      )) ^  \
                                     (((spell)[sizeof(spell)*4/9] << 24) | \
                                      ((spell)[sizeof(spell)*3/9] << 16) | \
                                      ((spell)[sizeof(spell)*2/9] <<  8) | \
                                      ((spell)[sizeof(spell)*1/9]      )) )


#define D_MAGIC_SET(o,m)           do {                                              \
                                        D_ASSERT( (o) != NULL );                     \
                                        D_ASSUME( (o)->magic != D_MAGIC(#m) );       \
                                                                                     \
                                        (o)->magic = D_MAGIC(#m);                    \
                                   } while (0)

#define D_MAGIC_SET_ONLY(o,m)      do {                                              \
                                        D_ASSERT( (o) != NULL );                     \
                                                                                     \
                                        (o)->magic = D_MAGIC(#m);                    \
                                   } while (0)

#define D_MAGIC_ASSERT(o,m)        do {                                              \
                                        D_ASSERT( (o) != NULL );                     \
                                        D_ASSERT( (o)->magic == D_MAGIC(#m) );       \
                                   } while (0)

#define D_MAGIC_ASSUME(o,m)        do {                                              \
                                        D_ASSUME( (o) != NULL );                     \
                                        if (o)                                       \
                                             D_ASSUME( (o)->magic == D_MAGIC(#m) );  \
                                   } while (0)

#define D_MAGIC_ASSERT_IF(o,m)     do {                                              \
                                        if (o)                                       \
                                             D_ASSERT( (o)->magic == D_MAGIC(#m) );  \
                                   } while (0)

#define D_MAGIC_CLEAR(o)           do {                                              \
                                        D_ASSERT( (o) != NULL );                     \
                                        D_ASSUME( (o)->magic != 0 );                 \
                                                                                     \
                                        (o)->magic = 0;                              \
                                   } while (0)


#endif

