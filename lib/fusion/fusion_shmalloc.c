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

#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <direct/build.h>
#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <fusion/build.h>
#include <fusion/shmalloc.h>


#if FUSION_BUILD_MULTI

/*************************** MULTI APPLICATION CORE ***************************/

#include "shmalloc/shmalloc_internal.h"

#if DIRECT_BUILD_DEBUG

void
fusion_dbg_print_memleaks()
{
     int i;

     D_ASSERT( _sheap != NULL );

     fusion_skirmish_prevail( &_sheap->lock );

     if (_sheap->alloc_count) {
          D_DEBUG( "Shared memory allocations remaining (%d): \n", _sheap->alloc_count);

          for (i=0; i<_sheap->alloc_count; i++) {
               SHMemDesc *d = &_sheap->alloc_list[i];

               D_DEBUG( "%7d bytes at %p allocated in %s (%s: %u)\n",
                        d->bytes, d->mem, d->func, d->file, d->line );
          }
     }

     fusion_skirmish_dismiss( &_sheap->lock );
}

static SHMemDesc *
allocate_shmem_desc()
{
     int cap = _sheap->alloc_capacity;

     if (!cap)
          cap = 64;
     else if (cap == _sheap->alloc_count)
          cap <<= 1;

     if (cap != _sheap->alloc_capacity) {
          _sheap->alloc_capacity = cap;
          _sheap->alloc_list     = _fusion_shrealloc( _sheap->alloc_list, sizeof(SHMemDesc) * cap );

          D_ASSERT( _sheap->alloc_list != NULL );
     }

     return &_sheap->alloc_list[_sheap->alloc_count++];
}

static void
fill_shmem_desc( SHMemDesc  *desc, const void *mem,  int bytes,
                 const char *func, const char *file, int line )
{
     desc->mem   = mem;
     desc->bytes = bytes;

     snprintf( desc->func, SHMEMDESC_FUNC_NAME_LENGTH, func );
     snprintf( desc->file, SHMEMDESC_FILE_NAME_LENGTH, file );

     desc->line = line;
}

/* Allocate SIZE bytes of memory.  */
void *
fusion_dbg_shmalloc( char *file, int line,
                     char *func, size_t __size )
{
     void *ret;

     D_ASSERT( __size > 0 );
     D_ASSERT( _sheap != NULL );

     D_HEAVYDEBUG("Fusion/SHM: allocating "
                  "%7d bytes in %s (%s: %u)\n", __size, func, file, line);

     fusion_skirmish_prevail( &_sheap->lock );

     ret = _fusion_shmalloc( __size );
     if (ret) {
          SHMemDesc *desc = allocate_shmem_desc();

          fill_shmem_desc( desc, ret, __size, func, file, line );
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

     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );
     D_ASSERT( _sheap != NULL );

     D_HEAVYDEBUG( "Fusion/SHM: allocating %7d bytes "
                   "in %s (%s: %u)\n", __size * __nmemb, func, file, line );

     fusion_skirmish_prevail( &_sheap->lock );

     ret = _fusion_shcalloc( __nmemb, __size );
     if (ret) {
          SHMemDesc *desc = allocate_shmem_desc();

          fill_shmem_desc( desc, ret, __size * __nmemb, func, file, line );
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

     D_ASSERT( _sheap != NULL );

     if (!__ptr)
          return fusion_dbg_shmalloc( file, line, func, __size );

     if (!__size) {
          fusion_dbg_shfree( file, line, func, what, __ptr );
          return NULL;
     }

     fusion_skirmish_prevail( &_sheap->lock );

     for (i=0; i<_sheap->alloc_count; i++) {
          SHMemDesc *desc = &_sheap->alloc_list[i];

          if (desc->mem == __ptr) {
               void *new_mem = _fusion_shrealloc( __ptr, __size );

               fill_shmem_desc( desc, new_mem, __size, func, file, line );

               fusion_skirmish_dismiss( &_sheap->lock );

               return new_mem;
          }
     }

     fusion_skirmish_dismiss( &_sheap->lock );

     D_ERROR( "Fusion/SHM: unknown chunk %p (%s) from [%s:%d in %s()]\n",
              __ptr, what, file, line, func );
     D_BREAK( "unknown chunk" );

     return NULL;
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_dbg_shfree( char *file, int line,
                   char *func, char *what, void *__ptr )
{
     unsigned int i;

     D_ASSERT( __ptr != NULL );
     D_ASSERT( _sheap != NULL );

     fusion_skirmish_prevail( &_sheap->lock );

     for (i=0; i<_sheap->alloc_count; i++) {
          SHMemDesc *desc = &_sheap->alloc_list[i];

          if (desc->mem == __ptr) {
               _fusion_shfree( __ptr );

               _sheap->alloc_count--;

               if (i < _sheap->alloc_count)
                    direct_memcpy( desc, desc + 1, (_sheap->alloc_count - i) * sizeof(SHMemDesc) );

               fusion_skirmish_dismiss( &_sheap->lock );

               return;
          }
     }

     fusion_skirmish_dismiss( &_sheap->lock );

     D_ERROR( "Fusion/SHM: unknown chunk %p (%s) from [%s:%d in %s()]\n",
              __ptr, what, file, line, func );
     D_BREAK( "unknown chunk" );
}

/* Duplicate string in shared memory. */
char *
fusion_dbg_shstrdup( char *file, int line,
                     char *func, const char *string )
{
     void *ret;
     int   len;

     D_ASSERT( string != NULL );
     D_ASSERT( _sheap != NULL );

     len = strlen( string ) + 1;

     D_HEAVYDEBUG( "Fusion/SHM: allocating %7d bytes in %s (%s: %u)\n",
                   len, func, file, line );

     fusion_skirmish_prevail( &_sheap->lock );

     ret = _fusion_shmalloc( len );
     if (ret) {
          SHMemDesc *desc = allocate_shmem_desc();

          fill_shmem_desc( desc, ret, len, func, file, line );

          direct_memcpy( ret, string, len );
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

     D_ASSERT( __size > 0 );
     D_ASSERT( _sheap != NULL );

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

     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );
     D_ASSERT( _sheap != NULL );

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

     D_ASSERT( _sheap != NULL );

     fusion_skirmish_prevail( &_sheap->lock );

     ret = _fusion_shrealloc( __ptr, __size );

     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_shfree (void *__ptr)
{
     D_ASSERT( __ptr != NULL );
     D_ASSERT( _sheap != NULL );

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

     D_ASSERT( string != NULL );
     D_ASSERT( _sheap != NULL );

     len = strlen( string ) + 1;

     fusion_skirmish_prevail( &_sheap->lock );

     ret = _fusion_shmalloc( len );
     if (ret)
          direct_memcpy( ret, string, len );

     fusion_skirmish_dismiss( &_sheap->lock );

     return ret;
}

#endif

#else

/************************** SINGLE APPLICATION CORE ***************************/

#if DIRECT_BUILD_DEBUG

#include <direct/mem.h>

void
fusion_dbg_print_memleaks()
{
}

/* Allocate SIZE bytes of memory.  */
void *
fusion_dbg_shmalloc( char *file, int line,
                     char *func, size_t __size )
{
     D_ASSERT( __size > 0 );

     return direct_malloc( file, line, func, __size );
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_dbg_shcalloc( char *file, int line,
                     char *func, size_t __nmemb, size_t __size)
{
     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );

     return direct_calloc( file, line, func, __nmemb, __size );
}

/* Re-allocate the previously allocated block
   in __ptr, making the new block SIZE bytes long.  */
void *
fusion_dbg_shrealloc( char *file, int line,
                      char *func, char *what, void *__ptr,
                      size_t __size )
{
     return direct_realloc( file, line, func, what, __ptr, __size );
}

/* Free a block allocated by `shmalloc', `shrealloc' or `shcalloc'.  */
void
fusion_dbg_shfree( char *file, int line,
                   char *func, char *what, void *__ptr )
{
     D_ASSERT( __ptr != NULL );

     direct_free( file, line, func, what, __ptr );
}

/* Duplicate string in shared memory. */
char *
fusion_dbg_shstrdup( char *file, int line,
                     char *func, const char *string )
{
     D_ASSERT( string != NULL );

     return direct_strdup( file, line, func, string );
}

#else

/* Allocate SIZE bytes of memory.  */
void *
fusion_shmalloc (size_t __size)
{
     D_ASSERT( __size > 0 );

     return malloc( __size );
}

/* Allocate NMEMB elements of SIZE bytes each, all initialized to 0.  */
void *
fusion_shcalloc (size_t __nmemb, size_t __size)
{
     D_ASSERT( __nmemb > 0 );
     D_ASSERT( __size > 0 );

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
     D_ASSERT( __ptr != NULL );

     free( __ptr );
}

/* Duplicate string in shared memory. */
char *
fusion_shstrdup (const char* string)
{
     D_ASSERT( string != NULL );

     return strdup( string );
}

#endif

bool
fusion_is_shared (const void *ptr)
{
     return true;
}

bool
fusion_shmalloc_cure (const void *ptr)
{
     return false;
}

#endif

