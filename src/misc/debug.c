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

#define _GNU_SOURCE

#include <config.h>

#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include <core/coredefs.h>

#include <misc/util.h>

#include "debug.h"


#ifdef DFB_TRACE

#ifdef DFB_DYNAMIC_LINKING
#include <dlfcn.h>
#endif

#define MAX_BUFFERS 200
#define MAX_LEVEL   100
#define MAX_NAME    92

struct __DFB_TraceBuffer {
     pid_t tid;
     int   level;
     void *trace[MAX_LEVEL];
};

static TraceBuffer     *buffers[MAX_BUFFERS];
static int              buffers_num  = 0;
static pthread_mutex_t  buffers_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t    trace_key    = -1;

__attribute__((no_instrument_function))
static void
buffer_destroy( void *buffer )
{
     int i;

     pthread_mutex_lock( &buffers_lock );

     for (i=0; i<buffers_num; i++) {
          if (buffers[i] == buffer)
               break;
     }

     for (; i<buffers_num-1; i++)
          buffers[i] = buffers[i+1];

     free( buffer );

     pthread_mutex_unlock( &buffers_lock );
}

__attribute__((no_instrument_function))
static inline TraceBuffer *
get_trace_buffer()
{
     TraceBuffer *buffer;

     buffer = pthread_getspecific( trace_key );
     if (!buffer) {
          pthread_mutex_lock( &buffers_lock );

          if (!buffers_num)
               pthread_key_create( &trace_key, buffer_destroy );
          else if (buffers_num == MAX_BUFFERS) {
               ERRORMSG( "DirectFB/debug/trace: "
                         "Maximum number of threads reached!\n" );
               pthread_mutex_unlock( &buffers_lock );
               return NULL;
          }

          pthread_setspecific( trace_key,
                               buffer = calloc( 1, sizeof(TraceBuffer) ) );

          buffer->tid = gettid();

          buffers[buffers_num++] = buffer;

          pthread_mutex_unlock( &buffers_lock );
     }

     return buffer;
}


#ifdef DFB_DYNAMIC_LINKING

typedef struct {
     long offset;
     char name[MAX_NAME];
} Symbol;

static Symbol *symbols     = NULL;
static int     num_symbols = 0;

__attribute__((constructor)) void _directfb_load_symbols();

__attribute__((no_instrument_function))
void
_directfb_load_symbols()
{
     int          i, j;
     int          num = 0;
     int          fd;
     struct stat  stat;
     char        *map;

     if (symbols) {
          free( symbols );
          symbols = NULL;
     }

     fd = open( MODULEDIR"/symbols.dynamic", O_RDONLY );
     if (fd < 0) {
          perror( "open "MODULEDIR"/symbols.dynamic" );
          return;
     }

     if (fstat( fd, &stat )) {
          perror( "stat "MODULEDIR"/symbols.dynamic" );
          close( fd );
          return;
     }

     map = mmap( NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0 );
     if (map == MAP_FAILED) {
          perror( "mmap "MODULEDIR"/symbols.dynamic" );
          close( fd );
          return;
     }

     for (i=0; i<stat.st_size; i++) {
          if (map[i] == '\n')
               num++;
     }

     symbols = malloc( num * sizeof(Symbol) );
     if (!symbols) {
          fprintf( stderr, "%s: Out of system memory!\n", __FUNCTION__ );
          close( fd );
          return;
     }

     for (i=0,j=0; i<num && j<stat.st_size-11; i++) {
          int  n;
          long offset = 0;

          for (n=0; n<8; n++) {
               char c = map[j++];

               offset <<= 4;

               if (c >= '0' && c <= '9')
                    offset |= c - '0';
               else
                    offset |= c - 'a' + 10;
          }

          symbols[i].offset = offset;

          j += 3;

          for (n=0; map[j] != '\n'; n++, j++) {
               if (n < MAX_NAME - 1) {
                    symbols[i].name[n] = map[j];
               }
          }

          symbols[i].name[MIN(n, MAX_NAME - 1)] = 0;

          j++;
     }

     num_symbols = num;

     munmap( map, stat.st_size );
     close( fd );
}

__attribute__((no_instrument_function))
static int
compare_symbols(const void *x, const void *y)
{
     return  *((long*) x)  -  *((long*) y);
}

__attribute__((no_instrument_function))
static bool
lookup_symbol( long offset )
{
     Symbol *symbol;

     if (!symbols)
          return false;

     symbol = bsearch( &offset, symbols, num_symbols,
                       sizeof(Symbol), compare_symbols );
     if (!symbol)
          return false;

     fprintf( stderr, "%s()\n", symbol->name );

     return true;
}

#endif


__attribute__((no_instrument_function))
void
dfb_trace_print_stack( TraceBuffer *buffer )
{
#ifdef DFB_DYNAMIC_LINKING
     Dl_info info;
#endif
     int     i;
     int     level;

     if (!buffer)
          buffer = get_trace_buffer();

     level = buffer->level;
     if (level >= MAX_LEVEL) {
          CAUTION( "only showing 100 items" );
          level = MAX_LEVEL - 1;
     }

     fprintf( stderr, "\n(-) DirectFB stack trace of %d\n", buffer->tid );

     for (i=0; i<level; i++) {
          int   n;
          void *fn = buffer->trace[i];

          fprintf( stderr, "    " );

          for (n=0; n<i; n++)
               fprintf( stderr, "  " );

          fprintf( stderr, "'-> " );

#ifdef DFB_DYNAMIC_LINKING
          if (dladdr( fn, &info )) {
               if (info.dli_sname)
                    fprintf( stderr, "%s()\n", info.dli_sname );
               else if (info.dli_fname) {
                    if (!strstr(SOPATH, info.dli_fname) ||
                        !lookup_symbol((long)(fn - info.dli_fbase)))
                    {
                         fprintf( stderr, "%p (%x) from %s (%p)\n",
                                  fn, fn - info.dli_fbase, info.dli_fname,
                                  info.dli_fbase );
                    }
               }
               else
                    fprintf( stderr, "%p\n", fn );
          }
          else
#endif
               fprintf( stderr, "%p\n", fn );
     }

     fprintf( stderr, "\n" );

     fflush( stderr );
}

__attribute__((no_instrument_function))
void
dfb_trace_print_stacks()
{
     int          i;
     TraceBuffer *buffer = get_trace_buffer();

     if (buffer->level)
          dfb_trace_print_stack( buffer );

     pthread_mutex_lock( &buffers_lock );

     for (i=0; i<buffers_num; i++) {
          if (buffers[i] != buffer && buffers[i]->level)
               dfb_trace_print_stack( buffers[i] );
     }

     pthread_mutex_unlock( &buffers_lock );
}


__attribute__((no_instrument_function))
void
__cyg_profile_func_enter (void *this_fn,
                          void *call_site)
{
     TraceBuffer *buffer = get_trace_buffer();
     int          level  = buffer->level++;

     if (level < MAX_LEVEL)
          buffer->trace[level] = this_fn;
}

__attribute__((no_instrument_function))
void
__cyg_profile_func_exit (void *this_fn,
                         void *call_site)
{
     TraceBuffer *buffer = get_trace_buffer();

     if (buffer->level > 0)
          buffer->level--;
}



#else

void
dfb_trace_print_stack( TraceBuffer *buffer )
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
     int       tid    = gettid();
     int       pgrp   = getpgrp();
     long long millis = fusion_get_millis();

     fprintf( stderr,
              "(!) [%5d: %4lld.%03lld] *** "
              "Assertion [%s] failed! *** %s:"
              "%d in %s()\n", tid, millis/1000,
              millis%1000, expression,
              filename, line, function );

     fflush( stderr );

     dfb_trace_print_stack( NULL );

     DEBUGMSG( "DirectFB/Assertion: "
               "Sending SIGTRAP to process group %d...\n", pgrp );

     killpg( pgrp, SIGTRAP );

     DEBUGMSG( "DirectFB/Assertion: "
               "...didn't catch signal on my own, calling _exit(-1).\n" );

     _exit( -1 );
}

void
dfb_assumption_fail( const char *expression,
                     const char *filename,
                     int         line,
                     const char *function )
{
     int       tid    = gettid();
     long long millis = fusion_get_millis();

     fprintf( stderr,
              "(!) [%5d: %4lld.%03lld] *** "
              "Assumption [%s] failed! *** %s:"
              "%d in %s()\n", tid, millis/1000,
              millis%1000, expression,
              filename, line, function );

     fflush( stderr );

     dfb_trace_print_stack( NULL );
}

#endif

