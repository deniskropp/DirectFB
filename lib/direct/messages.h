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

#ifndef __DIRECT__MESSAGES_H__
#define __DIRECT__MESSAGES_H__

#include <direct/build.h>

#if DIRECT_BUILD_NOTEXT
     #define D_INFO(x...)          do { } while (0)
     #define D_ERROR(x...)         do { } while (0)
     #define D_DERROR(x...)        do { } while (0)
     #define D_PERROR(x...)        do { } while (0)
     #define D_DLERROR(x...)       do { } while (0)
     #define D_ONCE(x...)          do { } while (0)
     #define D_UNIMPLEMENTED(x...) do { } while (0)
     #define D_BUG(x...)           do { } while (0)
     #define D_WARN(x...)          do { } while (0)
#else

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <direct/conf.h>


#define D_FORMAT_PRINTF(n)    __attribute__((__format__ (__printf__, n, n+1)))


void direct_messages_info         ( const char *format, ... )  D_FORMAT_PRINTF(1);

void direct_messages_error        ( const char *format, ... )  D_FORMAT_PRINTF(1);

void direct_messages_derror       ( DirectResult result,
                                    const char *format, ... )  D_FORMAT_PRINTF(2);

void direct_messages_perror       ( int         erno,
                                    const char *format, ... )  D_FORMAT_PRINTF(2);

void direct_messages_dlerror      ( const char *dlerr,
                                    const char *format, ... )  D_FORMAT_PRINTF(2);

void direct_messages_once         ( const char *func,
                                    const char *file,
                                    int         line,
                                    const char *format, ... )  D_FORMAT_PRINTF(4);

void direct_messages_unimplemented( const char *func,
                                    const char *file,
                                    int         line );

void direct_messages_bug          ( const char *func,
                                    const char *file,
                                    int         line,
                                    const char *format, ... )  D_FORMAT_PRINTF(4);

void direct_messages_warn         ( const char *func,
                                    const char *file,
                                    int         line,
                                    const char *format, ... )  D_FORMAT_PRINTF(4);


#define D_INFO(x...)     do {                                                                  \
                              if (!direct_config->quiet)                                       \
                                   direct_messages_info( x );                                  \
                         } while (0)

#define D_ERROR(x...)    do {                                                                  \
                              if (!direct_config->quiet)                                       \
                                   direct_messages_error( x );                                 \
                         } while (0)

#define D_DERROR(r,x...) do {                                                                  \
                              if (!direct_config->quiet)                                       \
                                   direct_messages_derror( r, x );                             \
                         } while (0)

#define D_PERROR(x...)   do {                                                                  \
                              if (!direct_config->quiet)                                       \
                                   direct_messages_perror( errno, x );                         \
                         } while (0)

#define D_DLERROR(x...)  do {                                                                  \
                              if (!direct_config->quiet)                                       \
                                   direct_messages_dlerror( dlerror(), x );                    \
                         } while (0)


#define D_ONCE(x...)     do {                                                                  \
                              if (!direct_config->quiet) {                                     \
                                   static bool first = true;                                   \
                                   if (first) {                                                \
                                        direct_messages_once( __FUNCTION__,                    \
                                                              __FILE__, __LINE__, x );         \
                                        first = false;                                         \
                                   }                                                           \
                              }                                                                \
                         } while (0)

#define D_UNIMPLEMENTED() do {                                                                 \
                              if (!direct_config->quiet) {                                     \
                                   static bool first = true;                                   \
                                   if (first) {                                                \
                                        direct_messages_unimplemented( __FUNCTION__,           \
                                                                       __FILE__, __LINE__ );   \
                                        first = false;                                         \
                                   }                                                           \
                              }                                                                \
                         } while (0)

#define D_BUG(x...)      do {                                                                  \
                              if (!direct_config->quiet)                                       \
                                   direct_messages_bug( __FUNCTION__, __FILE__, __LINE__, x ); \
                         } while (0)

#define D_WARN(x...)     do {                                                                  \
                              if (!direct_config->quiet)                                       \
                                   direct_messages_warn( __FUNCTION__, __FILE__, __LINE__, x );\
                         } while (0)
#endif


#endif

