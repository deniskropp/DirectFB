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

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#include "core/coredefs.h"

#include "mem.h"
#include "memcpy.h"

#ifdef DFB_DEBUG

typedef struct {
     void         *mem;
     size_t        bytes;
     char         *allocated_in_func;
     char         *allocated_in_file;
     unsigned int  allocated_in_line;
} MemDesc;


static int              alloc_count = 0;
static MemDesc         *alloc_list = NULL;
static pthread_mutex_t  alloc_lock = PTHREAD_MUTEX_INITIALIZER;


void
dfb_dbg_print_memleaks()
{
     unsigned int i;

     pthread_mutex_lock( &alloc_lock );

     if (alloc_count) {
          DEBUGMSG( "Local memory allocations remaining (%d): \n", alloc_count);

          for (i=0; i<alloc_count; i++) {
               MemDesc *d = &alloc_list[i];

               DEBUGMSG( "%7d bytes at %p allocated in %s (%s: %u)\n",
                         d->bytes, d->mem, d->allocated_in_func,
                         d->allocated_in_file, d->allocated_in_line);
          }
     }

     pthread_mutex_unlock( &alloc_lock );
}


void *
dfb_dbg_malloc( char* file, int line, char *func, size_t bytes )
{
     void *mem = (void*) malloc (bytes);
     MemDesc *d;

     pthread_mutex_lock( &alloc_lock );

     HEAVYDEBUGMSG("DirectFB/mem: "
                   "allocating %7d bytes in %s (%s: %u)\n", bytes, func, file, line);

     alloc_count++;
     alloc_list = realloc( alloc_list, alloc_count * sizeof(MemDesc) );

     d = &alloc_list[alloc_count-1];
     d->mem   = mem;
     d->bytes = bytes;
     d->allocated_in_func = func;
     d->allocated_in_file = file;
     d->allocated_in_line = line;

     pthread_mutex_unlock( &alloc_lock );

     return mem;
}


void *
dfb_dbg_calloc( char* file, int line, char *func, size_t count, size_t bytes )
{
     void *mem = (void*) calloc (count, bytes);
     MemDesc *d;

     pthread_mutex_lock( &alloc_lock );

     HEAVYDEBUGMSG("DirectFB/mem: allocating %7d bytes in %s (%s: %u)\n", 
                   count * bytes, func, file, line);

     alloc_count++;
     alloc_list = realloc( alloc_list, alloc_count * sizeof(MemDesc) );

     d = &alloc_list[alloc_count-1];
     d->mem   = mem;
     d->bytes = count * bytes;
     d->allocated_in_func = func;
     d->allocated_in_file = file;
     d->allocated_in_line = line;

     pthread_mutex_unlock( &alloc_lock );

     return mem;
}


void *
dfb_dbg_realloc( char *file, int line, char *func, char *what,
                 void *mem, size_t bytes )
{
     unsigned int i;

     if (!mem)
          return dfb_dbg_malloc( file, line, func, bytes );

     if (!bytes) {
          dfb_dbg_free( file, line, func, what, mem );
          return NULL;
     }

     pthread_mutex_lock( &alloc_lock );

     for (i=0; i<alloc_count; i++) {
          if (alloc_list[i].mem == mem) {
               char *new_mem = (void*) realloc( mem, bytes );
               alloc_list[i].mem   = new_mem;
               alloc_list[i].bytes = bytes;
               pthread_mutex_unlock( &alloc_lock );
               return new_mem;
          }
     }

     pthread_mutex_unlock( &alloc_lock );

     if (mem != NULL) {
          ERRORMSG ( "%s: trying to reallocate unknown chunk %p (%s)\n"
                     "          in %s (%s: %u) !!!\n",
                     __FUNCTION__, mem, what, func, file, line);
          kill( 0, SIGTRAP );
     }

     return dfb_dbg_malloc( file, line, func, bytes );
}

void
dfb_dbg_free( char *file, int line, char *func, char *what, void *mem )
{
     unsigned int i;

     pthread_mutex_lock( &alloc_lock );

     for (i=0; i<alloc_count; i++) {
          if (alloc_list[i].mem == mem) {
               free( mem );
               alloc_count--;
               dfb_memcpy( &alloc_list[i], &alloc_list[i+1],
                           (alloc_count - i) * sizeof(MemDesc) );
               pthread_mutex_unlock( &alloc_lock );
               return;
          }
     }

     pthread_mutex_unlock( &alloc_lock );

     ERRORMSG( "%s: trying to free unknown chunk %p (%s)\n"
               "          in %s (%s: %u) !!!\n",
               __FUNCTION__, mem, what, func, file, line);

     kill( 0, SIGTRAP );
}

char *
dfb_dbg_strdup( char* file, int line, char *func, const char *string )
{
     MemDesc *d;
     void    *mem;
     int      len = strlen( string ) + 1;

     mem = malloc( len );
     if (!mem)
          return NULL;

     dfb_memcpy( mem, string, len );

     pthread_mutex_lock( &alloc_lock );

     HEAVYDEBUGMSG("DirectFB/mem: allocating %7d bytes in %s (%s: %u)\n",
                   len, func, file, line);

     alloc_count++;
     alloc_list = realloc( alloc_list, alloc_count * sizeof(MemDesc) );

     d = &alloc_list[alloc_count-1];
     d->mem   = mem;
     d->bytes = len;
     d->allocated_in_func = func;
     d->allocated_in_file = file;
     d->allocated_in_line = line;

     pthread_mutex_unlock( &alloc_lock );

     return mem;
}

#endif

