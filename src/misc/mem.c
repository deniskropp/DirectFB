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

#include <stdio.h>

#include "core/coredefs.h"

#include "mem.h"

#ifdef DFB_DEBUG

typedef struct {
     void         *mem;
     char         *allocated_in_func;
     char         *allocated_in_file;
     unsigned int  allocated_in_line;
} MemDesc;


static int initialized = 0;
static int alloc_count = 0;
static MemDesc *alloc_list = NULL;


static void dbg_memleaks_done( int exitcode, void *dummy )
{
     unsigned int i;
     (void) dummy;

     if (exitcode == 0 && alloc_count != 0) {
          DEBUGMSG( "memory leak detected !!!\n");
          DEBUGMSG( "alloc_count == %i\n\n", alloc_count);

          for (i=0; i<alloc_count; i++) {
               MemDesc *d = &alloc_list[i];
               
               DEBUGMSG( "chunk %p allocated in %s (%s: %u) not free'd !!\n",
                         d->mem, d->allocated_in_func, d->allocated_in_file,
                         d->allocated_in_line);
          }

          free( alloc_list );
     }
}


static void dbg_memleaks_init( void )
{
     on_exit( dbg_memleaks_done, NULL );
     initialized = 1;
}


void* dbg_malloc( char* file, int line, char *func, size_t bytes )
{
     void *mem = (void*) malloc (bytes);
     MemDesc *d;

     if (!initialized)
          dbg_memleaks_init();

     alloc_count++;
     alloc_list = realloc( alloc_list, alloc_count * sizeof(MemDesc) );

     d = &alloc_list[alloc_count-1];
     d->mem = mem;
     d->allocated_in_func = func;
     d->allocated_in_file = file;
     d->allocated_in_line = line;

     return mem;
}


void* dbg_calloc( char* file, int line, char *func, size_t count, size_t bytes )
{
     void *mem = (void*) calloc (count, bytes);
     MemDesc *d;

     if (!initialized)
          dbg_memleaks_init();

     alloc_count++;
     alloc_list = realloc( alloc_list, alloc_count * sizeof(MemDesc) );

     d = &alloc_list[alloc_count-1];
     d->mem = mem;
     d->allocated_in_func = func;
     d->allocated_in_file = file;
     d->allocated_in_line = line;

     return mem;
}


void* dbg_realloc( char *file, int line, char *func, char *what,
                   void *mem, size_t bytes )
{
     unsigned int i;

     for (i=0; i<alloc_count; i++) {
          if (alloc_list[i].mem == mem) {
               alloc_list[i].mem = (void*) realloc( mem, bytes );
               return alloc_list[i].mem;
          }
     }

     if (mem != NULL) {
          DEBUGMSG ( "%s: trying to reallocate unknown chunk %p (%s)\n"
                     "          in %s (%s: %u) !!!\n",
                     __FUNCTION__, mem, what, func, file, line);
          exit (-1);
     }

     return dbg_malloc( file, line, func, bytes );
}

char* dbg_strdup( char* file, int line, char *func, const char *string )
{
     char *mem = strdup (string);
     MemDesc *d;

     if (!initialized)
          dbg_memleaks_init();

     alloc_count++;
     alloc_list = realloc( alloc_list, alloc_count * sizeof(MemDesc) );

     d = &alloc_list[alloc_count-1];
     d->mem = (void*) mem;
     d->allocated_in_func = func;
     d->allocated_in_file = file;
     d->allocated_in_line = line;

     return mem;
}

void dbg_free( char *file, int line, char *func, char *what, void *mem )
{
     unsigned int i;

     if (!initialized)
          dbg_memleaks_init();

     for (i=0; i<alloc_count; i++) {
          if (alloc_list[i].mem == mem) {
               free( mem );
               alloc_count--;
               memmove( &alloc_list[i], &alloc_list[i+1],
                        (alloc_count - i) * sizeof(MemDesc) );
               return;
          }
     }

     DEBUGMSG( "%s: trying to free unknown chunk %p (%s)\n"
               "          in %s (%s: %u) !!!\n",
               __FUNCTION__, mem, what, func, file, line);

     exit (-1);
}


#endif

