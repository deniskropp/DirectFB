/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <direct/build.h>


#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/trace.h>
#include <direct/util.h>

typedef struct {
     const void        *mem;
     size_t             bytes;
     const char        *func;
     const char        *file;
     int                line;
     DirectTraceBuffer *trace;
} MemDesc;


static int              alloc_count    = 0;
static int              alloc_capacity = 0;
static MemDesc         *alloc_list     = NULL;
static pthread_mutex_t  alloc_lock     = PTHREAD_MUTEX_INITIALIZER;


void
direct_print_memleaks()
{
     unsigned int i;

     pthread_mutex_lock( &alloc_lock );

     if (alloc_count && (!direct_config || direct_config->debugmem)) {
          direct_log_printf( NULL, "Local memory allocations remaining (%d): \n", alloc_count );

          for (i=0; i<alloc_count; i++) {
               MemDesc *desc = &alloc_list[i];

               direct_log_printf( NULL, "%7d bytes at %p allocated in %s (%s: %u)\n",
                                  desc->bytes, desc->mem, desc->func, desc->file, desc->line );

               if (desc->trace)
                    direct_trace_print_stack( desc->trace );
          }
     }

     pthread_mutex_unlock( &alloc_lock );
}

static MemDesc *
allocate_mem_desc()
{
     int cap = alloc_capacity;

     if (!cap)
          cap = 64;
     else if (cap == alloc_count)
          cap <<= 1;

     if (cap != alloc_capacity) {
          alloc_capacity = cap;
          alloc_list     = realloc( alloc_list, sizeof(MemDesc) * cap );

          D_ASSERT( alloc_list != NULL );
     }

     return &alloc_list[alloc_count++];
}

static inline void
fill_mem_desc( MemDesc *desc, const void *mem, int bytes,
               const char *func, const char *file, int line, DirectTraceBuffer *trace )
{
     desc->mem   = mem;
     desc->bytes = bytes;
     desc->func  = func;
     desc->file  = file;
     desc->line  = line;
     desc->trace = trace;
}

void *
direct_malloc( const char* file, int line, const char *func, size_t bytes )
{
     void    *mem;
     MemDesc *desc;

     D_HEAVYDEBUG( "Direct/Mem: allocating %7d bytes in %s (%s: %u)\n", bytes, func, file, line );

     mem = malloc( bytes );
     if (!mem)
          return NULL;

     pthread_mutex_lock( &alloc_lock );

     desc = allocate_mem_desc();

     fill_mem_desc( desc, mem, bytes, func, file, line, direct_trace_copy_buffer(NULL) );

     pthread_mutex_unlock( &alloc_lock );

     return mem;
}


void *
direct_calloc( const char* file, int line, const char *func, size_t count, size_t bytes )
{
     void    *mem;
     MemDesc *desc;

     D_HEAVYDEBUG( "Direct/Mem: allocating %7d bytes in %s (%s: %u)\n",
                   count * bytes, func, file, line );

     mem = calloc( count, bytes );
     if (!mem)
          return NULL;

     pthread_mutex_lock( &alloc_lock );

     desc = allocate_mem_desc();

     fill_mem_desc( desc, mem, count * bytes, func, file, line, direct_trace_copy_buffer(NULL) );

     pthread_mutex_unlock( &alloc_lock );

     return mem;
}


void *
direct_realloc( const char *file, int line, const char *func, const char *what, void *mem, size_t bytes )
{
     int i;

     if (!mem)
          return direct_malloc( file, line, func, bytes );

     if (!bytes) {
          direct_free( file, line, func, what, mem );
          return NULL;
     }

     pthread_mutex_lock( &alloc_lock );

     for (i=0; i<alloc_count; i++) {
          MemDesc *desc = &alloc_list[i];

          if (desc->mem == mem) {
               void *new_mem = realloc( mem, bytes );

               D_ASSERT( new_mem != NULL );

               if (desc->trace)
                    direct_trace_free_buffer( desc->trace );

               fill_mem_desc( desc, new_mem, bytes, func, file, line, direct_trace_copy_buffer(NULL) );

               pthread_mutex_unlock( &alloc_lock );

               return new_mem;
          }
     }

     pthread_mutex_unlock( &alloc_lock );

     if (mem != NULL) {
          D_ERROR( "Direct/Mem: unknown chunk %p (%s) from [%s:%d in %s()]\n",
                   mem, what, file, line, func );
          D_BREAK( "unknown chunk" );
     }

     return direct_malloc( file, line, func, bytes );
}

void
direct_free( const char *file, int line, const char *func, const char *what, void *mem )
{
     unsigned int i;

     if (!mem)
          return;

     pthread_mutex_lock( &alloc_lock );

     for (i=0; i<alloc_count; i++) {
          MemDesc *desc = &alloc_list[i];

          if (desc->mem == mem) {
               free( mem );

               if (desc->trace)
                    direct_trace_free_buffer( desc->trace );

               alloc_count--;

               if (i < alloc_count)
                    direct_memmove( desc, desc + 1, (alloc_count - i) * sizeof(MemDesc) );

               pthread_mutex_unlock( &alloc_lock );

               return;
          }
     }

     pthread_mutex_unlock( &alloc_lock );

     D_ERROR( "Direct/Mem: unknown chunk %p (%s) from [%s:%d in %s()]\n",
              mem, what, file, line, func );
     D_BREAK( "unknown chunk" );
}

char *
direct_strdup( const char* file, int line, const char *func, const char *string )
{
     void    *mem;
     MemDesc *desc;
     int      length = strlen( string ) + 1;

     D_HEAVYDEBUG( "Direct/Mem: allocating %7d bytes in %s (%s: %u)\n",
                   length, func, file, line );

     mem = malloc( length );
     if (!mem)
          return NULL;

     direct_memcpy( mem, string, length );

     pthread_mutex_lock( &alloc_lock );

     desc = allocate_mem_desc();

     fill_mem_desc( desc, mem, length, func, file, line, direct_trace_copy_buffer(NULL) );

     pthread_mutex_unlock( &alloc_lock );

     return mem;
}

#else

void
direct_print_memleaks()
{
}

#endif

