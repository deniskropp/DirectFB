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

/* #define HEAVYDEBUG */

#include <misc/conf.h>

#ifdef PIC
#define DFB_DYNAMIC_LINKING
#endif

#define MAX_INPUTDEVICES 100
#define MAX_LAYERS       100


#define INITMSG(x...)    if (!dfb_config->quiet) fprintf( stderr, "(*) "x );
#define ERRORMSG(x...)   if (!dfb_config->quiet) fprintf( stderr, "(!) "x );

#define PERRORMSG(x...)  if (!dfb_config->quiet) {                             \
                              fprintf( stderr, "(!) "x );                      \
                              fprintf( stderr, "    --> " );                   \
                              perror("");                                      \
                         }

#define DLERRORMSG(x...) if (!dfb_config->quiet) {                             \
                              fprintf( stderr, "(!) "x );                      \
                              fprintf( stderr, "    --> %s\n",                 \
                                               dlerror() );                    \
                         }

#ifdef DFB_DEBUG

     #include <misc/util.h>   /* for dfb_get_millis() */
     
     #ifdef HEAVYDEBUG
          #define HEAVYDEBUGMSG(x...)   if (dfb_config->debug) {               \
                                                  fprintf( stderr, "(=) "x );  \
                                        }
     #else
          #define HEAVYDEBUGMSG(x...)
     #endif

     #define DEBUGMSG(x...)   do { if (dfb_config->debug) {                    \
                                   fprintf( stderr, "(-) [%d: %5lld] ",        \
                                            getpid(), dfb_get_millis() );      \
                                   fprintf( stderr, x );                       \
                                   fflush( stderr );                           \
                              } } while (0)

     #define DFB_ASSERT(exp)  if (!(exp)) {                                    \
                                   DEBUGMSG( "*** Assertion [%s] failed! "     \
                                             "*** %s (%d)\n", #exp,            \
                                             __FILE__, __LINE__ );             \
                                   kill( 0, SIGTRAP );                         \
                              }

#else
     #define HEAVYDEBUGMSG(x...)
     #define DEBUGMSG(x...)
     #define DFB_ASSERT(exp)
#endif


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

