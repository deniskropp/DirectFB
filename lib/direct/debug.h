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

#ifndef __DIRECT__DEBUG_H__
#define __DIRECT__DEBUG_H__

#include <direct/build.h>

#include <stdio.h>
#include <errno.h>

#include <direct/conf.h>
#include <direct/messages.h>
#include <direct/system.h>


void direct_debug( const char *format, ... );

void direct_break( const char *func,
                   const char *file,
                   int         line,
                   const char *format, ... );

void direct_assertion( const char *exp,
                       const char *func,
                       const char *file,
                       int         line );

void direct_assumption( const char *exp,
                        const char *func,
                        const char *file,
                        int         line );


#if DIRECT_BUILD_DEBUG || defined(DIRECT_FORCE_DEBUG)

#ifdef HEAVYDEBUG
     #define D_HEAVYDEBUG(x...)    if (!direct_config || direct_config->debug) {\
                                             fprintf( stderr, "(=) "x );  \
                                   }
#else
     #define D_HEAVYDEBUG(x...)
#endif

#define D_DEBUG(x...)    do {                                                        \
                              if (!direct_config || direct_config->debug)            \
                                   direct_debug( x );                                \
                         } while (0)

#define D_ASSERT(exp)    do {                                                        \
                              if (!(exp)) {                                          \
                                   direct_assertion( #exp,                           \
                                                     __FUNCTION__,                   \
                                                     __FILE__,                       \
                                                     __LINE__ );                     \
                              }                                                      \
                         } while (0)

#define D_ASSUME(exp)    do {                                                        \
                              if (!(exp)) {                                          \
                                   direct_assumption( #exp,                          \
                                                      __FUNCTION__,                  \
                                                      __FILE__,                      \
                                                      __LINE__ );                    \
                              }                                                      \
                         } while (0)

#define D_BREAK(x...)    do {                                                        \
                              direct_break( __FUNCTION__, __FILE__, __LINE__, x );   \
                         } while (0)

#define D_MAGIC(spell)        ( (((spell)[sizeof(spell)*8/9] << 24) | \
                                 ((spell)[sizeof(spell)*7/9] << 16) | \
                                 ((spell)[sizeof(spell)*6/9] <<  8) | \
                                 ((spell)[sizeof(spell)*5/9]      )) ^  \
                                (((spell)[sizeof(spell)*4/9] << 24) | \
                                 ((spell)[sizeof(spell)*3/9] << 16) | \
                                 ((spell)[sizeof(spell)*2/9] <<  8) | \
                                 ((spell)[sizeof(spell)*1/9]      )) )

#define D_MAGIC_SET(o,m)      do {                                         \
                                   D_ASSERT( (o) != NULL );                \
                                                                           \
                                   (o)->magic = D_MAGIC(#m);               \
                              } while (0)

#define D_MAGIC_ASSERT(o,m)   do {                                         \
                                   D_ASSERT( (o) != NULL );                \
                                   D_ASSERT( (o)->magic == D_MAGIC(#m) );  \
                              } while (0)

#define D_MAGIC_CLEAR(o)      do {                                         \
                                   D_ASSERT( (o) != NULL );                \
                                   D_ASSUME( (o)->magic != 0 );            \
                                                                           \
                                   (o)->magic = 0;                         \
                              } while (0)

#else

#define D_MAGIC_CLEAR(o)      do {} while (0)
#define D_MAGIC_SET(o,m)      do {} while (0)
#define D_MAGIC_ASSERT(o,m)   do {} while (0)

#define D_HEAVYDEBUG(x...)    do {} while (0)
#define D_DEBUG(x...)         do {} while (0)
#define D_ASSERT(exp)         do {} while (0)
#define D_ASSUME(exp)         do {} while (0)
#define D_BREAK(x...)         do {} while (0)

#endif



#endif

