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

#ifndef __DIRECT__MESSAGES_H__
#define __DIRECT__MESSAGES_H__

#include <direct/build.h>
#include <direct/types.h>


#if __GNUC__ >= 3
#define D_FORMAT_PRINTF(n)         __attribute__((__format__ (__printf__, n, n+1)))
#else
#define D_FORMAT_PRINTF(n)
#endif

typedef enum {
     DMT_NONE           = 0x00000000, /* No message type. */

     DMT_BANNER         = 0x00000001, /* Startup banner. */
     DMT_INFO           = 0x00000002, /* Info messages. */
     DMT_WARNING        = 0x00000004, /* Warnings. */
     DMT_ERROR          = 0x00000008, /* Error messages: regular, with DFBResult, bugs,
                                         system call errors, dlopen errors */
     DMT_UNIMPLEMENTED  = 0x00000010, /* Messages notifying unimplemented functionality. */
     DMT_ONCE           = 0x00000020, /* One-shot messages .*/

     DMT_ALL            = 0x0000003f  /* All types. */
} DirectMessageType;

#if DIRECT_BUILD_TEXT

#include <errno.h>

#include <direct/conf.h>


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
                              if (!(direct_config->quiet & DMT_INFO))                          \
                                   direct_messages_info( x );                                  \
                         } while (0)

#define D_ERROR(x...)    do {                                                                  \
                              if (!(direct_config->quiet & DMT_ERROR))                         \
                                   direct_messages_error( x );                                 \
                         } while (0)

#define D_DERROR(r,x...) do {                                                                  \
                              if (!(direct_config->quiet & DMT_ERROR))                         \
                                   direct_messages_derror( r, x );                             \
                         } while (0)

#define D_PERROR(x...)   do {                                                                  \
                              if (!(direct_config->quiet & DMT_ERROR))                         \
                                   direct_messages_perror( errno, x );                         \
                         } while (0)

#define D_DLERROR(x...)  do {                                                                  \
                              if (!(direct_config->quiet & DMT_ERROR))                         \
                                   direct_messages_dlerror( dlerror(), x );                    \
                         } while (0)


#define D_ONCE(x...)     do {                                                                  \
                              if (!(direct_config->quiet & DMT_ONCE)) {                        \
                                   static bool first = true;                                   \
                                   if (first) {                                                \
                                        direct_messages_once( __FUNCTION__,                    \
                                                              __FILE__, __LINE__, x );         \
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

#define D_BUG(x...)      do {                                                                  \
                              if (!(direct_config->quiet & DMT_ERROR))                         \
                                   direct_messages_bug( __FUNCTION__, __FILE__, __LINE__, x ); \
                         } while (0)

#define D_WARN(x...)     do {                                                                  \
                              if (!(direct_config->quiet & DMT_WARNING))                       \
                                   direct_messages_warn( __FUNCTION__, __FILE__, __LINE__, x );\
                         } while (0)

#define D_OOM()          (direct_messages_warn( __FUNCTION__, __FILE__, __LINE__,              \
                                                "out of memory" ), DR_NOLOCALMEMORY)


#else
     #define D_INFO(x...)          do { } while (0)
     #define D_ERROR(x...)         do { } while (0)
     #define D_DERROR(x...)        do { } while (0)
     #define D_PERROR(x...)        do { } while (0)
     #define D_DLERROR(x...)       do { } while (0)
     #define D_ONCE(x...)          do { } while (0)
     #define D_UNIMPLEMENTED()     do { } while (0)
     #define D_BUG(x...)           do { } while (0)
     #define D_WARN(x...)          do { } while (0)
     #define D_OOM()               (printf("out of memory\n"), DR_NOLOCALMEMORY)
#endif


#endif

