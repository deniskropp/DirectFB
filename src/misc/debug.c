/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2003  convergence GmbH.
   
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

#define _GNU_SOURCE

#include <config.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

#include <core/coredefs.h>

#include "debug.h"


#ifdef DFB_DEBUG

#include <dlfcn.h>


#define MAX_LEVEL 100

static struct {
     int level;
     void *trace[MAX_LEVEL];
} threads[65536];


__attribute__((no_instrument_function))
void
dfb_debug_print_stack()
{
     Dl_info info;
     int     i, pid = getpid();
     int     level  = threads[pid].level;

     if (level > MAX_LEVEL) {
          CAUTION( "only showing 100 items" );
          return;
     }

     fprintf( stderr, "\n(-) DirectFB stack trace of pid %d\n", pid );

     for (i=0; i<level; i++) {
          int   n;
          void *fn = threads[pid].trace[i];

          fprintf( stderr, "    " );
          
          for (n=0; n<i; n++)
               fprintf( stderr, "  " );
     
          fprintf( stderr, "'-> " );
          
          if (dladdr( fn, &info )) {
               if (info.dli_sname)
                    fprintf( stderr, "%s()\n", info.dli_sname );
               else if (info.dli_fname)
                    fprintf( stderr, "%p (%p) from %s (%p)\n", fn, fn -
                             info.dli_fbase, info.dli_fname, info.dli_fbase );
               else
                    fprintf( stderr, "%p\n", fn );
          }
          else
               fprintf( stderr, "%p\n", fn );
     }
     
     fprintf( stderr, "\n" );

     fflush( stderr );
}


__attribute__((no_instrument_function))
void
__cyg_profile_func_enter (char *this_fn,
                          char *call_site)
{
     int pid   = getpid();
     int level = threads[pid].level++;

     if (level > MAX_LEVEL)
          return;

     threads[pid].trace[level] = this_fn;
}

__attribute__((no_instrument_function))
void
__cyg_profile_func_exit (char *this_fn,
                         char *call_site)
{
     int pid   = getpid();
     int level = --threads[pid].level;

     if (level > MAX_LEVEL)
          return;

     DFB_ASSERT( level >= 0 );
     DFB_ASSERT( threads[pid].trace[level] == this_fn );
}



#else

void
dfb_debug_print_stack()
{
}

#endif

