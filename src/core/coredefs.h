/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __COREDEFS_H__
#define __COREDEFS_H__

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include <config.h>

#include <core/fusion/fusion.h>

/* #define HEAVYDEBUG */

#include <misc/conf.h>


#ifdef PIC
#define DFB_DYNAMIC_LINKING
#endif

#define MAX_INPUTDEVICES 100
#define MAX_LAYERS       100


#if defined(DFB_NOTEXT)
     #define INITMSG(x...)    do { } while (0)
     #define ERRORMSG(x...)   do { } while (0)
     #define PERRORMSG(x...)  do { } while (0)
     #define DLERRORMSG(x...) do { } while (0)
     #define ONCE(msg)        do { } while (0)
     #define BUG(msg)         do { } while (0)
     #define CAUTION(msg)     do { } while (0)
#else
#define INITMSG(x...)    if (!dfb_config->quiet) fprintf( stderr, "(*) "x );
#define ERRORMSG(x...)   if (!dfb_config->quiet) fprintf( stderr, "(!) "x );

#define PERRORMSG(x...)  do { if (!dfb_config->quiet) {                        \
                              fprintf( stderr, "(!) "x );                      \
                              fprintf( stderr, "    --> " );                   \
                              perror("");                                      \
                         } } while (0)

#define DLERRORMSG(x...) do { if (!dfb_config->quiet) {                        \
                              fprintf( stderr, "(!) "x );                      \
                              fprintf( stderr, "    --> %s\n",                 \
                                               dlerror() );                    \
                         } } while (0)

#define ONCE(msg)   do {                                                       \
                         static int print = 1;                                 \
                         if (print) {                                          \
                              fprintf( stderr, "(!) *** [%s] *** %s (%d)\n",   \
                                       msg, __FILE__, __LINE__ );              \
                              print = 0;                                       \
                         }                                                     \
                    } while (0)

#define BUG(x)     fprintf( stderr, " (!?!)  *** BUG ALERT [%s] *** %s (%d)\n",\
                            x, __FILE__, __LINE__ )

#define CAUTION(x) fprintf( stderr, " (!!!)  *** CAUTION [%s] *** %s (%d)\n",  \
                            x, __FILE__, __LINE__ )
#endif


#define DFB_BREAK(msg)   do {                                                  \
                              int       pid    = getpid();                     \
                              long long millis = fusion_get_millis();          \
                                                                               \
                              fprintf( stderr,                                 \
                                       "(!) [%5d: %4lld.%03lld] *** "          \
                                       "Break! [%s] *** %s:%d %s()\n",         \
                                       pid, millis/1000, millis%1000,          \
                                       msg, __FILE__, __LINE__, __FUNCTION__ );\
                              fflush( stderr );                                \
                              kill( getpgrp(), SIGTRAP );                      \
                              pause();                                         \
                         } while (0)

#if (defined(DFB_DEBUG) && !defined(DFB_NOTEXT)) || defined(DFB_FORCE_DEBUG)

     #ifdef HEAVYDEBUG
          #define HEAVYDEBUGMSG(x...)   if (!dfb_config || dfb_config->debug) {\
                                                  fprintf( stderr, "(=) "x );  \
                                        }
     #else
          #define HEAVYDEBUGMSG(x...)
     #endif

     #define DEBUGMSG(x...)   do { if (!dfb_config || dfb_config->debug) {     \
                                   long long millis = fusion_get_millis();     \
                                   fprintf( stderr, "(-) [%5d: %4lld.%03lld] ",\
                                            getpid(),millis/1000,millis%1000 );\
                                   fprintf( stderr, x );                       \
                                   fflush( stderr );                           \
                              } } while (0)

     #define DFB_ASSERT(exp)  if (!(exp)) {                                    \
                                   int       pid    = getpid();                \
                                   long long millis = fusion_get_millis();     \
                                                                               \
                                   fprintf( stderr,                            \
                                            "(!) [%5d: %4lld.%03lld] *** "     \
                                            "Assertion [%s] failed! *** %s:"   \
                                            "%d %s()\n", pid, millis/1000,     \
                                            millis%1000, #exp,                 \
                                            __FILE__, __LINE__, __FUNCTION__ );\
                                   fflush( stderr );                           \
                                   kill( getpgrp(), SIGTRAP );                 \
                                   pause();                                    \
                              }

     #define DFB_ASSUME(exp)  if (!(exp)) {                                    \
                                   int       pid    = getpid();                \
                                   long long millis = fusion_get_millis();     \
                                                                               \
                                   fprintf( stderr,                            \
                                            "(?) [%5d: %4lld.%03lld] *** "     \
                                            "Assumption [%s] failed! *** %s:"  \
                                            "%d %s()\n", pid, millis/1000,     \
                                            millis%1000, #exp,                 \
                                            __FILE__, __LINE__, __FUNCTION__ );\
                                   fflush( stderr );                           \
                              }

#else
     #define HEAVYDEBUGMSG(x...)   do { } while (0)
     #define DEBUGMSG(x...)        do { } while (0)
     #define DFB_ASSERT(exp)       do { } while (0)
     #define DFB_ASSUME(exp)       do { } while (0)
#endif



#endif

