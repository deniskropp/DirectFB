/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

//#define HEAVYDEBUG

#include <stdio.h>
#include <misc/conf.h>

#ifdef PIC
#define DFB_DYNAMIC_LINKING
#endif

#define MAX_INPUTDEVICES 100
#define MAX_LAYERS       100


#define INITMSG(x...)    { if (!dfb_config->quiet) fprintf( stderr, "(*) "x ); }
#define ERRORMSG(x...)   { if (!dfb_config->quiet) fprintf( stderr, "(!) "x ); }

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
     #ifdef HEAVYDEBUG
          #define HEAVYDEBUGMSG(x...)   if (!dfb_config->no_debug) {           \
                                                  fprintf( stderr, "(=) "x );  \
                                        }
     #else
          #define HEAVYDEBUGMSG(x...)
     #endif
     
     #define DEBUGMSG(x...)   if (!dfb_config->no_debug) {                     \
                                   fprintf( stderr, "(-) "x );                 \
                              }

     #define DFB_ASSERT(exp)  if (!(exp)) {                                    \
                                   ERRORMSG("DirectFB/Assertion: '" #exp       \
                                            "' failed!\n");                    \
                              }

     #define ONCE(x){                                                          \
                         static int print = 1;                                 \
                         if (print)                                            \
                              DEBUGMSG( "*** [%s] *** %s (%d)\n",              \
                                        x, __FILE__, __LINE__ );               \
                         print = 0;                                            \
                    }

#else
     #define HEAVYDEBUGMSG(x...)
     #define DEBUGMSG(x...)
     #define DFB_ASSERT(exp)
     #define ONCE(x)
#endif


#define BUG(x)     fprintf( stderr, " (!?!)  *** BUG ALERT [%s] *** %s (%d)\n",\
                            x, __FILE__, __LINE__ );

#define CAUTION(x) fprintf( stderr, " (!!!)  *** CAUTION [%s] *** %s (%d)\n",  \
                            x, __FILE__, __LINE__ );

#endif

