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


#ifdef DFB_TRACE

#include <dlfcn.h>


#define MAX_LEVEL 100

static struct {
     int level;
     void *trace[MAX_LEVEL];
} threads[65536];


__attribute__((no_instrument_function))
void
dfb_trace_print_stack( int pid )
{
     Dl_info info;
     int     i;
     int     level;

     if (!pid)
          pid = getpid();

     level = threads[pid].level;
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
                    fprintf( stderr, "%p (%x) from %s (%p)\n", fn, fn -
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
dfb_trace_print_stacks()
{
     int i, pid = getpid();

     if (threads[pid].level)
          dfb_trace_print_stack( pid );

     for (i=0; i<65536; i++) {
          if (i != pid && threads[i].level)
               dfb_trace_print_stack( i );
     }
}


__attribute__((no_instrument_function))
void
__cyg_profile_func_enter (void *this_fn,
                          void *call_site)
{
     int pid   = getpid();
     int level = threads[pid].level++;

     if (level > MAX_LEVEL)
          return;

     threads[pid].trace[level] = this_fn;
}

__attribute__((no_instrument_function))
void
__cyg_profile_func_exit (void *this_fn,
                         void *call_site)
{
     int pid   = getpid();
     int level = --threads[pid].level;

     if (level > MAX_LEVEL)
          return;

//     DFB_ASSERT( level >= 0 );
//     DFB_ASSERT( threads[pid].trace[level] == this_fn );
}



#else

void
dfb_trace_print_stack( int pid )
{
}

void
dfb_trace_print_stacks()
{
}

#endif

#ifdef DFB_DEBUG

void
dfb_assertion_fail( const char *expression,
                    const char *filename,
                    int         line,
                    const char *function )
{
     int       pid    = getpid();
     long long millis = fusion_get_millis();

     fprintf( stderr,
              "(!) [%5d: %4lld.%03lld] *** "
              "Assertion [%s] failed! *** %s:"
              "%d in %s()\n", pid, millis/1000,
              millis%1000, expression,
              filename, line, function );

     fflush( stderr );

     kill( getpgrp(), SIGTRAP );

     pause();
}

void
dfb_assumption_fail( const char *expression,
                     const char *filename,
                     int         line,
                     const char *function )
{
     int       pid    = getpid();
     long long millis = fusion_get_millis();

     fprintf( stderr,
              "(!) [%5d: %4lld.%03lld] *** "
              "Assumption [%s] failed! *** %s:"
              "%d in %s()\n", pid, millis/1000,
              millis%1000, expression,
              filename, line, function );

     fflush( stderr );
}

#endif

