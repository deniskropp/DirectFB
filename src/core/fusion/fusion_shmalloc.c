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

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <core/coredefs.h>

#include <misc/memcpy.h>

#include "shmalloc.h"


#ifndef FUSION_FAKE

/*************************** MULTI APPLICATION CORE ***************************/

#include "shmalloc/shmalloc_internal.h"

#ifdef DFB_DEBUG

void
fusion_dbg_print_memleaks()
{
     unsigned int i;

     DFB_ASSERT( _sheap != NULL );
     
     fusion_skirmish_prevail( &_sheap->lock );

     if (_sheap->alloc_count) {
          DEBUGMSG( "Shared memory allocations remaining (%d): \n",
                    _sheap->alloc_count);

          for (i=0; i<_sheap->alloc_count; i++) {
               SHMemDesc *d = &_sheap->alloc_list[i];

               DEBUGMSG( "%7d bytes at %p allocated in %s (%s: %u)\n",
                         d->bytes, d->mem, d->allocated_in_func,
                         d->allocated_in_file, d->allocated_in_line);
          }
     }

     fusion_skirmish_dismiss( &_sheap->lock );
}

/* Allocate SIZE bytes of memory.  */
void *
fusion_dbg_shmalloc( char *file, int line,
                     char *func, size_t __size )
{
     void *ret;

     DFB_ASSERT( __size > 0 );
     DFB_ASSERT( _sheap != NULL );

     HEAVYDEBUGMSG("DirectFB/shmem: allocating "
                   "%7d bytes in %s (%s: %u)\n", __size, func, file, line);
     
     fusion_skirmish_prevail( &_sheap->lock );
     
     ret = _fusion_shmalloc( __size );
     if (ret) {
          SHMemDesc *d;

          _sheap->alloc_count++;
          _sheap->alloc_list = _fusion_shrealloc( _sheap->alloc_list,
                                                  (sizeof(SHMemDesc) *
                                                   _sheap->alloc_count) );

          d = &_sheap->alloc_list[_sheap->alloc_count-1];

          d->mem               = ret;
          d->bytes             = __size;
          d->allocated_in_func = func;
          d->allocated_in_file = file;
          d->allocated_in_line = line;
     }
     
     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_dbg_shcalloc( char *file, int line,
                     char *func, size_t __nmemb, size_t __size)
{
     void *ret;

     DFB_ASSERT( __nmemb > 0 );
     DFB_ASSERT( __size > 0 );
     DFB_ASSERT( _sheap != NULL );

     HEAVYDEBUGMSG("DirectFB/shmem: allocating %7d bytes "
                   "in %s (%s: %u)\n", __size * __nmemb, func, file, line);
     
     fusion_skirmish_prevail( &_sheap->lock );
     
     ret = _fusion_shcalloc( __nmemb, __size );
     if (ret) {
          SHMemDesc *d;

          _sheap->alloc_count++;
          _sheap->alloc_list = _fusion_shrealloc( _sheap->alloc_list,
                                                  (sizeof(SHMemDesc) *
                                                   _sheap->alloc_count) );

          d = &_sheap->alloc_list[_sheap->alloc_count-1];

          d->mem               = ret;
          d->bytes             = __size * __nmemb;
          d->allocated_in_func = func;
          d->allocated_in_file = file;
          d->allocated_in_line = line;
     }
     
     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_dbg_shrealloc( char *file, int line,
                      char *func, char *what, void *__ptr,
                      size_t __size )
{
     unsigned int i;

     DFB_ASSERT( _sheap != NULL );
     
     if (!__ptr)
          return fusion_dbg_shmalloc( file, line, func, __size );

     if (!__size) {
          fusion_dbg_shfree( file, line, func, what, __ptr );
          return NULL;
     }

     fusion_skirmish_prevail( &_sheap->lock );

     for (i=0; i<_sheap->alloc_count; i++) {
          if (_sheap->alloc_list[i].mem == __ptr) {
               void *new_mem = _fusion_shrealloc( __ptr, __size );

               _sheap->alloc_list[i].mem   = new_mem;
               _sheap->alloc_list[i].bytes = new_mem ? __size : 0;

               fusion_skirmish_dismiss( &_sheap->lock );

               return new_mem;
          }
     }

     fusion_skirmish_dismiss( &_sheap->lock );

     ERRORMSG ( "%s: trying to reallocate unknown chunk %p (%s)\n"
                "          in %s (%s: %u) !!!\n",
                __FUNCTION__, __ptr, what, func, file, line);
     kill( 0, SIGTRAP );

     return NULL;
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_dbg_shfree( char *file, int line,
                   char *func, char *what, void *__ptr )
{
     unsigned int i;

     DFB_ASSERT( __ptr != NULL );
     DFB_ASSERT( _sheap != NULL );
     
     fusion_skirmish_prevail( &_sheap->lock );

     for (i=0; i<_sheap->alloc_count; i++) {
          if (_sheap->alloc_list[i].mem == __ptr) {
               _fusion_shfree( __ptr );

               _sheap->alloc_count--;

               dfb_memcpy( &_sheap->alloc_list[i], &_sheap->alloc_list[i+1],
                           (_sheap->alloc_count - i) * sizeof(SHMemDesc) );

               fusion_skirmish_dismiss( &_sheap->lock );

               return;
          }
     }

     fusion_skirmish_dismiss( &_sheap->lock );

     ERRORMSG( "%s: trying to free unknown chunk %p (%s)\n"
               "          in %s (%s: %u) !!!\n",
               __FUNCTION__, __ptr, what, func, file, line);
     
     kill( 0, SIGTRAP );
}

/* Duplicate string in shared memory. */
char *
fusion_dbg_shstrdup( char *file, int line,
                     char *func, const char *string )
{
     void *ret;
     int   len;

     DFB_ASSERT( string != NULL );
     DFB_ASSERT( _sheap != NULL );

     len = strlen( string ) + 1;

     HEAVYDEBUGMSG("DirectFB/mem: allocating %7d bytes in %s (%s: %u)\n",
                   len, func, file, line);
     
     fusion_skirmish_prevail( &_sheap->lock );

     ret = _fusion_shmalloc( len );
     if (ret) {
          SHMemDesc *d;
          
          dfb_memcpy( ret, string, len );

          _sheap->alloc_count++;
          _sheap->alloc_list = _fusion_shrealloc( _sheap->alloc_list,
                                                  (sizeof(SHMemDesc) *
                                                   _sheap->alloc_count) );

          d = &_sheap->alloc_list[_sheap->alloc_count-1];
          d->mem   = ret;
          d->bytes = len;
          d->allocated_in_func = func;
          d->allocated_in_file = file;
          d->allocated_in_line = line;
     }

     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

#else

/* Allocate SIZE bytes of memory.  */
void *
fusion_shmalloc (size_t __size)
{
     void *ret;

     DFB_ASSERT( __size > 0 );
     DFB_ASSERT( _sheap != NULL );

     fusion_skirmish_prevail( &_sheap->lock );
     
     ret = _fusion_shmalloc( __size );
     
     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_shcalloc (size_t __nmemb, size_t __size)
{
     void *ret;

     DFB_ASSERT( __nmemb > 0 );
     DFB_ASSERT( __size > 0 );
     DFB_ASSERT( _sheap != NULL );

     fusion_skirmish_prevail( &_sheap->lock );
     
     ret = _fusion_shcalloc( __nmemb, __size );
     
     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_shrealloc (void *__ptr, size_t __size)
{
     void *ret;

     DFB_ASSERT( _sheap != NULL );

     fusion_skirmish_prevail( &_sheap->lock );
     
     ret = _fusion_shrealloc( __ptr, __size );
     
     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_shfree (void *__ptr)
{
     DFB_ASSERT( __ptr != NULL );
     DFB_ASSERT( _sheap != NULL );

     fusion_skirmish_prevail( &_sheap->lock );
     
     _fusion_shfree( __ptr );
     
     fusion_skirmish_dismiss( &_sheap->lock );
}

/* Duplicate string in shared memory. */
char *
fusion_shstrdup (const char* string)
{
     void *ret;
     int   len;

     DFB_ASSERT( string != NULL );
     DFB_ASSERT( _sheap != NULL );

     len = strlen( string ) + 1;

     fusion_skirmish_prevail( &_sheap->lock );
     
     ret = _fusion_shmalloc( len );
     if (ret)
          dfb_memcpy( ret, string, len );
     
     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

#endif

#else

/************************** SINGLE APPLICATION CORE ***************************/

#ifdef DFB_DEBUG

#include <misc/mem.h>

void
fusion_dbg_print_memleaks()
{
}

/* Allocate SIZE bytes of memory.  */
void *
fusion_dbg_shmalloc( char *file, int line,
                     char *func, size_t __size )
{
     DFB_ASSERT( __size > 0 );

     return dfb_dbg_malloc( file, line, func, __size );
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_dbg_shcalloc( char *file, int line,
                     char *func, size_t __nmemb, size_t __size)
{
     DFB_ASSERT( __nmemb > 0 );
     DFB_ASSERT( __size > 0 );

     return dfb_dbg_calloc( file, line, func, __nmemb, __size );
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_dbg_shrealloc( char *file, int line,
                      char *func, char *what, void *__ptr,
                      size_t __size )
{
     return dfb_dbg_realloc( file, line, func, what, __ptr, __size );
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_dbg_shfree( char *file, int line,
                   char *func, char *what, void *__ptr )
{
     DFB_ASSERT( __ptr != NULL );

     dfb_dbg_free( file, line, func, what, __ptr );
}

/* Duplicate string in shared memory. */
char *
fusion_dbg_shstrdup( char *file, int line,
                     char *func, const char *string )
{
     DFB_ASSERT( string != NULL );

     return dfb_dbg_strdup( file, line, func, string );
}

#else

/* Allocate SIZE bytes of memory.  */
void *
fusion_shmalloc (size_t __size)
{
     DFB_ASSERT( __size > 0 );

     return malloc( __size );
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_shcalloc (size_t __nmemb, size_t __size)
{
     DFB_ASSERT( __nmemb > 0 );
     DFB_ASSERT( __size > 0 );

     return calloc( __nmemb, __size );
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_shrealloc (void *__ptr, size_t __size)
{
     return realloc( __ptr, __size );
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_shfree (void *__ptr)
{
     DFB_ASSERT( __ptr != NULL );

     free( __ptr );
}

/* Duplicate string in shared memory. */
char *
fusion_shstrdup (const char* string)
{
     DFB_ASSERT( string != NULL );

     return strdup( string );
}

#endif

bool
fusion_is_shared (const void *ptr)
{
     return true;
}

#endif

