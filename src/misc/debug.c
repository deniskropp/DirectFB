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

#include <core/fusion/list.h>

#include <misc/memcpy.h>
#include <misc/util.h>

#include "debug.h"


#ifdef DFB_TRACE

#ifdef DFB_DYNAMIC_LINKING
#include <dlfcn.h>
#endif

#define MAX_BUFFERS 200
#define MAX_LEVEL   100

#define NAME_LEN    92

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

     buffers_num--;

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
     char name[NAME_LEN];
} Symbol;

typedef struct {
     FusionLink  link;

     char       *filename;
     Symbol     *symbols;
     int         capacity;
     int         num_symbols;
} SymbolTable;

static FusionLink      *tables      = NULL;
static pthread_mutex_t  tables_lock = PTHREAD_MUTEX_INITIALIZER;

__attribute__((no_instrument_function))
static void
add_symbol( SymbolTable *table, long offset, const char *name )
{
     Symbol *symbol;

     if (table->num_symbols == table->capacity) {
          Symbol *symbols;
          int     capacity = table->capacity * 2;

          if (!capacity)
               capacity = 256;

          symbols = malloc( capacity * sizeof(Symbol) );
          if (!symbols) {
               CAUTION( "out of memory" );
               return;
          }

          dfb_memcpy( symbols, table->symbols, table->num_symbols * sizeof(Symbol) );

          free( table->symbols );

          table->symbols  = symbols;
          table->capacity = capacity;
     }

     symbol = &table->symbols[ table->num_symbols++ ];

     symbol->offset = offset;

     strncpy( symbol->name, name, NAME_LEN - 1 );

     symbol->name[NAME_LEN - 1] = 0;
}

__attribute__((no_instrument_function))
static SymbolTable *
load_symbols( const char *filename )
{
     SymbolTable *table;
     FILE        *pipe;
     char         line[1024];
     char         command[ strlen(filename) + 8 ];

     snprintf( command, sizeof(command), "nm -n %s", filename );

     pipe = popen( command, "r" );
     if (!pipe) {
          PERRORMSG( "DirectFB/debug: popen( \"%s\", \"r\" ) failed!\n", command );
          return NULL;
     }

     table = calloc( 1, sizeof(SymbolTable) );
     if (!table) {
          CAUTION( "out of memory" );
          pclose( pipe );
          return NULL;
     }

     table->filename = strdup( filename );
     if (!table->filename) {
          CAUTION( "out of memory" );
          free( table );
          pclose( pipe );
          return NULL;
     }

     while (fgets( line, sizeof(line), pipe )) {
          int  n;
          long offset = 0;
          int  length = strlen(line);

          if (length < 13 || line[8] != ' ' || line[10] != ' ' || line[length-1] != '\n')
               continue;

          if (line[9] != 't' && line[9] != 'T')
               continue;

          for (n=0; n<8; n++) {
               char c = line[n];

               offset <<= 4;

               if (c >= '0' && c <= '9')
                    offset |= c - '0';
               else
                    offset |= c - 'a' + 10;
          }

          line[length-1] = 0;

          add_symbol( table, offset, line + 11 );
     }

     pclose( pipe );

     return table;
}

__attribute__((no_instrument_function))
static int
compare_symbols(const void *x, const void *y)
{
     return  *((long*) x)  -  *((long*) y);
}

__attribute__((no_instrument_function))
static SymbolTable *
find_table( const char *filename )
{
     FusionLink *l;

     fusion_list_foreach (l, tables) {
          SymbolTable *table = (SymbolTable*) l;

          if (!strcmp( filename, table->filename ))
               return table;
     }

     return NULL;
}

__attribute__((no_instrument_function))
static const char *
lookup_symbol( const char *filename, long offset )
{
     Symbol      *symbol;
     SymbolTable *table;

     pthread_mutex_lock( &tables_lock );

     table = find_table( filename );
     if (!table) {
          table = load_symbols( filename );
          if (!table) {
               pthread_mutex_unlock( &tables_lock );
               return false;
          }

          fusion_list_prepend( &tables, &table->link );
     }

     pthread_mutex_unlock( &tables_lock );


     symbol = bsearch( &offset, table->symbols, table->num_symbols,
                       sizeof(Symbol), compare_symbols );

     return symbol ? symbol->name : NULL;
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

     for (i=level-1; i>=0; i--) {
          void *fn = buffer->trace[i];

          fprintf( stderr, "  #%-2d %p in ", level - i - 1, fn );

#ifdef DFB_DYNAMIC_LINKING
          if (dladdr( fn, &info )) {
               if (info.dli_fname) {
                    const char *fname;
                    const char *symbol = lookup_symbol(info.dli_fname, (long)(fn - info.dli_fbase));

                    if (!symbol) {
                         if (info.dli_sname)
                              symbol = info.dli_sname;
                         else
                              symbol = "??";
                    }

                    fname = strrchr( info.dli_fname, '/' );
                    if (fname)
                         fname++;
                    else
                         fname = info.dli_fname;

                    fprintf( stderr, "%s () from %s\n", symbol, fname );
               }
               else if (info.dli_sname) {
                    fprintf( stderr, "%s ()\n", info.dli_sname );
               }
               else
                    fprintf( stderr, "?? ()\n" );
          }
          else
#endif
               fprintf( stderr, "?? ()\n" );
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

