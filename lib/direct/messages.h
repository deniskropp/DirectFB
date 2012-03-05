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

#ifndef __DIRECT__MESSAGES_H__
#define __DIRECT__MESSAGES_H__

#include <direct/types.h>


#if DIRECT_BUILD_TEXT

#include <direct/compiler.h>
#include <direct/conf.h>


void DIRECT_API direct_messages_info         ( const char *format, ... )  D_FORMAT_PRINTF(1);

void DIRECT_API direct_messages_error        ( const char *format, ... )  D_FORMAT_PRINTF(1);

void DIRECT_API direct_messages_derror       ( DirectResult result,
                                               const char *format, ... )  D_FORMAT_PRINTF(2);

void DIRECT_API direct_messages_perror       ( int         erno,
                                               const char *format, ... )  D_FORMAT_PRINTF(2);

void DIRECT_API direct_messages_dlerror      ( const char *dlerr,
                                               const char *format, ... )  D_FORMAT_PRINTF(2);

void DIRECT_API direct_messages_once         ( const char *func,
                                               const char *file,
                                               int         line,
                                               const char *format, ... )  D_FORMAT_PRINTF(4);

void DIRECT_API direct_messages_unimplemented( const char *func,
                                               const char *file,
                                               int         line );

void DIRECT_API direct_messages_bug          ( const char *func,
                                               const char *file,
                                               int         line,
                                               const char *format, ... )  D_FORMAT_PRINTF(4);

void DIRECT_API direct_messages_warn         ( const char *func,
                                               const char *file,
                                               int         line,
                                               const char *format, ... )  D_FORMAT_PRINTF(4);


#define D_INFO(...)      do {                                                                  \
                              if (!(direct_config->quiet & DMT_INFO))                          \
                                   direct_messages_info( __VA_ARGS__ );                        \
                         } while (0)

#define D_ERROR(...)     do {                                                                  \
                              if (!(direct_config->quiet & DMT_ERROR))                         \
                                   direct_messages_error( __VA_ARGS__ );                       \
                         } while (0)

#define D_ERROR_AT(d,...)    do {                                                                             \
                                   if (!(direct_config->quiet & DMT_ERROR))                                   \
                                        direct_log_domain_log( &(d), DIRECT_LOG_ERROR,                        \
                                                               __PRETTY_FUNCTION__, __FILE__, __LINE__, __VA_ARGS__ );  \
                              } while (0)

#define D_DERROR(r,...) do {                                                                  \
                              if (!(direct_config->quiet & DMT_ERROR))                        \
                                   direct_messages_derror( (DirectResult) r, __VA_ARGS__ );   \
                         } while (0)

// FIXME
#define D_DERROR_AT(d,r,...) do {                                                                   \
                              if (!(direct_config->quiet & DMT_ERROR))                         \
                                   direct_log_domain_log( &(d), DIRECT_LOG_ERROR,                        \
                                                          __PRETTY_FUNCTION__, __FILE__, __LINE__, __VA_ARGS__ );  \
                         } while (0)

#define D_PERROR(...)    do {                                                                  \
                              if (!(direct_config->quiet & DMT_ERROR))                         \
                                   direct_messages_perror( errno, __VA_ARGS__ );               \
                         } while (0)

#define D_DLERROR(...)   do {                                                                  \
                              if (!(direct_config->quiet & DMT_ERROR))                         \
                                   direct_messages_dlerror( dlerror(), __VA_ARGS__ );          \
                         } while (0)


#define D_ONCE(...)      do {                                                                  \
                              if (!(direct_config->quiet & DMT_ONCE)) {                        \
                                   static bool first = true;                                   \
                                   if (first) {                                                \
                                        direct_messages_once( __FUNCTION__,                    \
                                                              __FILE__, __LINE__, __VA_ARGS__ );         \
                                        first = false;                                         \
                                   }                                                           \
                              }                                                                \
                         } while (0)

#define D_UNIMPLEMENTED() do {                                                                 \
                              if (!(direct_config->quiet & DMT_UNIMPLEMENTED)) {               \
                                   static bool first = true;                                   \
                                   if (first) {                                                \
                                        direct_messages_unimplemented( __FUNCTION__,           \
                                                                       __FILE__, __LINE__ );   \
                                        first = false;                                         \
                                   }                                                           \
                              }                                                                \
                         } while (0)

#define D_UNTESTED()     do {                                                                  \
                              if (!(direct_config->quiet & DMT_UNIMPLEMENTED)) {               \
                                   static bool first = true;                                   \
                                   if (first) {                                                \
                                        direct_messages_unimplemented( __FUNCTION__,           \
                                                                       __FILE__, __LINE__ );   \
                                        first = false;                                         \
                                   }                                                           \
                              }                                                                \
                         } while (0)

#define D_BUG(...)       do {                                                                  \
                              if (!(direct_config->quiet & DMT_ERROR))                         \
                                   direct_messages_bug( __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__ ); \
                         } while (0)

#define D_WARN(...)      do {                                                                  \
                              if (!(direct_config->quiet & DMT_WARNING))                       \
                                   direct_messages_warn( __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__ );\
                         } while (0)

#define D_OOM()          (direct_messages_warn( __FUNCTION__, __FILE__, __LINE__,              \
                                                "out of memory" ), DR_NOLOCALMEMORY)


#else
     #define D_INFO(...)           do { } while (0)
     #define D_ERROR(...)          do { } while (0)
     #define D_DERROR(...)         do { } while (0)
     #define D_PERROR(...)         do { } while (0)
     #define D_DLERROR(...)        do { } while (0)
     #define D_ONCE(...)           do { } while (0)
     #define D_UNIMPLEMENTED()     do { } while (0)
     #define D_BUG(...)            do { } while (0)
     #define D_WARN(...)           do { } while (0)
     #define D_OOM()               (printf("out of memory\n"), DR_NOLOCALMEMORY)
#endif


#endif

